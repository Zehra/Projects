// steamrollerRace.cpp : Defines the entry point for the DLL application.
//
#include "bzfsAPI.h"
#include "plugin_utils.h"
#include <map>
#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>
#include <stack>
#include <fstream>

#ifndef M_PI
#  define	M_PI		3.14159265358979323846f
#endif

#define mybzfrand()	((double)rand() / ((double)RAND_MAX + 1.0))

/* START - Compile-time configuration */

// These shouldn't need to be changed. However, they are here to make it
// easier to do so should it be deemed necessary.

// This controls the name of the custom map object
#define _OBJECT_NAME "SPAWNZONE"

// This controls the name of the custom slash command
#define _COMMAND_NAME "oc"

/*  END - Compile-time configuration  */

// Individual player
typedef struct {
  bz_ApiString callsign;
  int score;
} Player;

// List of players
std::map<int, Player> players;

bool sortPlayersByScore(const Player& p1, const Player& p2)
{
  return p1.score > p2.score;
}

/** Main hander class definition **/

class steamrollerRaceHandler : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_CustomMapObjectHandler
{
private:

public:
  // Current spawn point ID
  int currentID;
  // Lowest spawn point ID
  int lowestID;
  // Highest spawn point ID
  int highestID;

  struct KillTime
  {
    int playerID;
    double time;
  };
  struct Bzvar
  {
    std::string name;
    bz_ApiString value;
    int perms;
    bool persistent;
  };

  bool isgrabed;
  bool isfrozen;
  std::stack<Bzvar> bzvars;
  Bzvar bzspeedsave;
  double grabtime;
  int countdown;
  bz_ApiString winner;

  std::queue<KillTime> killTimes;

  steamrollerRaceHandler();
  virtual const char* Name() { return "steamrollerRace"; }
  virtual void Init(const char* /*config*/);
  virtual void Cleanup(void);
  virtual void Event(bz_EventData *eventData);
  virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params);
  virtual bool MapObject(bz_ApiString object, bz_CustomMapObjectInfo *data);

  virtual bool addPlayer(int index);
  virtual bool removePlayer(int index);
};

steamrollerRaceHandler steamrollerRace;


/** Custom map object class definition **/

typedef struct SpawnZone
{
  float pos[3];
  float size[2];
  float rotation;
  int id;
  bool forceSpawnAzimuth;
  float spawnAzimuth;
  std::map <std::string, std::string> Spawnzone_bzdb;
  bool isnokillshot;

  SpawnZone()
  {
    pos[0] = pos[1] = pos[2] = size[0] = size[1] = rotation = spawnAzimuth = 0.0f;
    id = -1;
    forceSpawnAzimuth = false;
    isnokillshot = false;
  }

  void getRandomPoint(float* pt)
  {
    float x = (float)((mybzfrand() * (2.0f * size[0])) - size[0]);
    float y = (float)((mybzfrand() * (2.0f * size[1])) - size[1]);

    const float cos_val = cosf(rotation);
    const float sin_val = sinf(rotation);
    pt[0] = (x * cos_val) - (y * sin_val);
    pt[1] = (x * sin_val) + (y * cos_val);

    pt[0] += pos[0];
    pt[1] += pos[1];
    pt[2] = pos[2];
  }
} SpawnZone;

std::map <int, SpawnZone> SpawnZones;

void steamrollerRaceHandler::Init(const char* /*commandLine*/)
{
  // In the event that the plugin is loaded while the server is already
  // running with players, initialize our current player list.
  bz_APIIntList *playerList = bz_newIntList();
  bz_getPlayerIndexList(playerList);
  for (unsigned int i = 0; i < playerList->size(); i++)
    steamrollerRace.addPlayer(i);
  bz_deleteIntList(playerList);

  // Register the custom map object
  bz_registerCustomMapObject(_OBJECT_NAME, this);

  // Register the required events
  Register(bz_ePlayerJoinEvent);
  Register(bz_ePlayerPartEvent);
  Register(bz_eGetPlayerSpawnPosEvent);
  Register(bz_eFlagGrabbedEvent);
  Register(bz_eTickEvent);
  Register(bz_eShotFiredEvent);

  // Register the custom slash command
  bz_registerCustomSlashCommand(_COMMAND_NAME, this);

  MaxWaitTime = (0.1f);

  bz_debugMessage(4, "steamrollerRace plugin loaded");
}

void steamrollerRaceHandler::Cleanup(void)
{
  // Add any clean-up/shutdown stuff here.

  bz_removeCustomMapObject(_OBJECT_NAME);

  Flush();

  bz_removeCustomSlashCommand(_COMMAND_NAME);

  bz_debugMessage(4, "steamrollerRace plugin unloaded");
}

steamrollerRaceHandler::steamrollerRaceHandler()
{
  currentID = lowestID = highestID = -1;
  isgrabed = false;
  isfrozen = true;
}

void steamrollerRaceHandler::Event(bz_EventData *eventData)
{
  switch (eventData->eventType)
  {
  case bz_ePlayerJoinEvent: //A player joins the game
  {
    bz_PlayerJoinPartEventData_V1* joindata = (bz_PlayerJoinPartEventData_V1*)eventData;

    if (joindata->record->team != eObservers)
      addPlayer(joindata->playerID);

    // Members for bz_PlayerJoinPartEventData:

    // int playerID: The player ID
    // bz_eTeamType team: The player's team
    // bzApiString callsign: Their callsign
    // bzApiString email: Their email
    // bool verified: Whether or not they're verified (registered + identified)
    // bzApiString globalUser: BZID (when using global auth)
    // bzApiString ipAddress: Player's IP address
    // bzApiString reason: /part or /quit reason (for parting only)
    // double time: The game time (in seconds)

  }
  break;

  case bz_ePlayerPartEvent: //A player leaves the game
  {
    bz_PlayerJoinPartEventData_V1* partdata = (bz_PlayerJoinPartEventData_V1*)eventData;

    if (partdata->record->team != eObservers)
      removePlayer(partdata->playerID);

    // Members for bz_PlayerJoinPartEventData:

    // int playerID: The player ID
    // bz_eTeamType team: The player's team
    // bzApiString callsign: Their callsign
    // bzApiString email: Their email
    // bool verified: Whether or not they're verified (registered + identified)
    // bzApiString globalUser: BZID (when using global auth)
    // bzApiString ipAddress: Player's IP address
    // bzApiString reason: /part or /quit reason (for parting only)
    // double time: The game time (in seconds)

  }
  break;

  case bz_eGetPlayerSpawnPosEvent: //A spawn location is selected
  {
    bz_GetPlayerSpawnPosEventData_V1* spawnposdata = (bz_GetPlayerSpawnPosEventData_V1*)eventData;

    std::map<int, SpawnZone>::iterator itr = SpawnZones.find(currentID);
    if (itr != SpawnZones.end())
    {
      float pos[3];
      itr->second.getRandomPoint(pos);
      spawnposdata->pos[0] = pos[0];
      spawnposdata->pos[1] = pos[1];
      spawnposdata->pos[2] = pos[2];
      if (itr->second.forceSpawnAzimuth)
        spawnposdata->rot = itr->second.spawnAzimuth;
      else
        spawnposdata->rot = (float)(mybzfrand() * 2.0 * M_PI);
      spawnposdata->handled = true;

    }
    else {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Unable to find spawn zone #%d.", currentID);
    }

    // Members for bz_GetPlayerSpawnPosEventData:

    // int playerID: The player who's going to spawn
    // bz_eTeamType team: The player's team
    // bool handled: Whether the plug-in has altered the spawn position
    // float pos[3]: spawn position
    // float rot: spawn direction
    // double time: The game time (in seconds)

  }
  break;

  case bz_eFlagGrabbedEvent: //A flag is grabbed
  {
    bz_FlagGrabbedEventData_V1* flaggrabdata = (bz_FlagGrabbedEventData_V1*)eventData;

    if (highestID == -1)
      return;

    if (bz_ApiString(flaggrabdata->flagType) != "SR")
      return;

    std::vector<Player> playersByScore;

    std::map<int, Player>::iterator itr = players.find(flaggrabdata->playerID);
    winner = itr->second.callsign;
    if (itr != players.end())
    {
      // We need to delay killing and resetting everything just a bit
      KillTime kt;
      kt.playerID = itr->first;
      // Short delay
      kt.time = bz_getCurrentTime() + 0.1f;

      killTimes.push(kt);
    }

    do {
      currentID++;
    } while (currentID <= highestID && SpawnZones.find(currentID) == SpawnZones.end());

    if (currentID > highestID)
      currentID = lowestID;

    isgrabed = true;
    isfrozen = false;

    // Members for bz_FlagGrabbedEventData:

    // int playerID: 
    // int flagID: 
    // char *flagType: The flag type
    // float pos[3]: The position of the grab

  }
  break;

  case bz_eTickEvent: // Tick event
  {

    if (isgrabed && !isfrozen) {
      isfrozen = true;
      grabtime = bz_getCurrentTime();
      countdown = 10;
      bzspeedsave.name = std::string("_tankSpeed");
      bzspeedsave.value = bz_getBZDBString("_tankSpeed");
      bzspeedsave.perms = bz_getBZDBItemPerms("_tankSpeed");
      bzspeedsave.persistent = bz_getBZDBItemPesistent("_tankSpeed");

    }
    if (isgrabed && isfrozen) {

      bz_setBZDBString(std::string("_tankSpeed").c_str(), std::string("0.0001").c_str(), bz_getBZDBItemPerms("_tankSpeed"), bz_getBZDBItemPesistent("_tankSpeed"));
      //          bz_debugMessagef(4, "DEBUG::freezeing => _tankSpeed = %s", bz_getBZDBString("_tankSpeed").c_str());
      double freezetime = bz_getCurrentTime() - grabtime;
      if (countdown >= 10 - freezetime) {
        if (countdown >0 && countdown <= 5) {
          bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Countdown %d", countdown);
        }
        countdown--;
      }
      else if (countdown <0) {
        isgrabed = false;
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, (std::string("RUN !!  RUN !!").c_str()));

      }
    }
    if (!isgrabed &&  isfrozen) {
      isfrozen = false;
      std::map<int, SpawnZone>::iterator itr = SpawnZones.find(currentID);
      std::map <std::string, std::string> bzdb = itr->second.Spawnzone_bzdb;
      std::map<std::string, std::string>::iterator itr2 = bzdb.begin();
      bz_setBZDBString(bzspeedsave.name.c_str(), bzspeedsave.value.c_str(), bzspeedsave.perms, bzspeedsave.persistent);
      while (!bzvars.empty())
      {
        Bzvar bv;
        bv = bzvars.top();
        bz_setBZDBString(bv.name.c_str(), bv.value.c_str(), bv.perms, bv.persistent);
        //               bz_debugMessagef(1, "DEBUG:: Reset default variable %s  = %s (should have been %s)", bv.name.c_str(), bz_getBZDBString( bv.name.c_str()).c_str(),bv.value.c_str());

        bzvars.pop();
      }
      while (itr2 != bzdb.end())
      {
        Bzvar bv;
        bv.name = itr2->first;
        bv.value = bz_getBZDBString(itr2->first.c_str());
        bv.perms = bz_getBZDBItemPerms(itr2->first.c_str());
        bv.persistent = bz_getBZDBItemPesistent(itr2->first.c_str());
        //               bz_debugMessagef(1, "DEBUG:: ID=%d Record variable %s = %s (should have been %s)",currentID,  bv.name.c_str(),bz_getBZDBString( bv.name.c_str()).c_str(),bv.value.c_str());
        bzvars.push(bv);
        bz_setBZDBString(itr2->first.c_str(), itr2->second.c_str(), bz_getBZDBItemPerms(itr2->first.c_str()), bz_getBZDBItemPesistent(itr2->first.c_str()));
        //               bz_debugMessagef(1, "DEBUG:: ID=%d Set variable %s  = %s (should have been set to %s)",currentID,itr2->first.c_str(), bz_getBZDBString(itr2->first.c_str()).c_str(),itr2->second.c_str());
        itr2++;
      }
    }
    if (!killTimes.empty() && killTimes.front().time < bz_getCurrentTime())
    {
      KillTime kt = killTimes.front();

      std::vector<Player> playersByScore;

      std::map<int, Player>::iterator itr = players.begin();
      while (itr != players.end())
      {
        if (itr->first == kt.playerID)
        {
          itr->second.score++;
          bz_killPlayer(kt.playerID, false);
          killTimes.pop();
        }

        // We only want to show people with positive scores, because people with 0 are looooosers
        if (itr->second.score != 0)
          playersByScore.push_back(itr->second);
        itr++;
      }

      bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "-----------Current scores-------------");

      // Sort the vector by score
      std::sort(playersByScore.begin(), playersByScore.end(), sortPlayersByScore);

      // Display everyone's score
      std::vector<Player>::iterator itr2 = playersByScore.begin();
      while (itr2 != playersByScore.end()) {
        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%4d : %s", itr2->score, itr2->callsign.c_str());
        itr2++;
      }
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "WINNER : %s", winner.c_str());
    }
  }
  break;
  case bz_eShotFiredEvent: // shot fired event
  {
    bz_ShotFiredEventData_V1 *ShotFiredEventData = (bz_ShotFiredEventData_V1 *)eventData;
    std::map<int, SpawnZone>::iterator itr = SpawnZones.find(currentID);
    //           bz_debugMessagef(1, "DEBUG:: ID=%d NOKILL = %s",currentID,(itr->second.isnokillshot) ? "true":"false");
    if (itr->second.isnokillshot) {
      ShotFiredEventData->type = bz_ApiString("PZ");
      ShotFiredEventData->changed = true;
    }
  }
  break;
  default:
    break;

  }
}
bool steamrollerRaceHandler::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params)
{
  // Handle the command "oc" here.

  return false;
}

bool steamrollerRaceHandler::MapObject(bz_ApiString object, bz_CustomMapObjectInfo *data)
{
  if (object != _OBJECT_NAME || !data)
    return false;

  SpawnZone sp;

  // parse all the chunks
  for (unsigned int i = 0; i < data->data.size(); i++)
  {
    std::string line = data->data.get(i).c_str();

    bz_APIStringList *parts = bz_newStringList();
    parts->tokenize(line.c_str(), " ", 0, true);

    if (parts->size() > 0)
    {
      std::string key = bz_toupper(parts->get(0).c_str());

      if (key == "ID" && parts->size() > 1)
      {
        sp.id = (int)atoi(parts->get(1).c_str());
      }
      else if (key == "POSITION" && parts->size() > 3)
      {
        sp.pos[0] = (float)atof(parts->get(1).c_str());
        sp.pos[1] = (float)atof(parts->get(2).c_str());
        sp.pos[2] = (float)atof(parts->get(3).c_str());
      }
      else if (key == "SIZE" && parts->size() > 2)
      {
        sp.size[0] = (float)atof(parts->get(1).c_str());
        sp.size[1] = (float)atof(parts->get(2).c_str());
      }
      else if (key == "ROTATION" && parts->size() > 1)
      {
        sp.rotation = (float)atof(parts->get(1).c_str());
      }
      else if (key == "SPAWNAZIMUTH" && parts->size() > 1)
      {
        sp.spawnAzimuth = M_PI / 2.f - (float)atof(parts->get(1).c_str()) * M_PI / 180.0f;
        //        bz_debugMessagef(4, "DEBUG::SPAWNAZIMUTH =>  %s => %f", parts->get(1).c_str(), sp.spawnAzimuth*180.0f/M_PI);
        sp.forceSpawnAzimuth = true;
      }
      else if (key == "SET" && parts->size() > 2)
      {
        if (bz_BZDBItemExists(parts->get(1).c_str()))
        {
          std::string str1 = std::string(parts->get(1).c_str());
          std::string str2 = std::string(parts->get(2).c_str());
          sp.Spawnzone_bzdb[str1] = str2;
        }
      }
      else if (key == "NOKILL")
      {
        sp.isnokillshot = true;
      }
    }
    bz_deleteStringList(parts);
  }

  if (sp.id >= 0) {
    // Check if this is the lowest spawn id (also set the current ID)
    if (steamrollerRace.currentID == -1 || steamrollerRace.currentID > sp.id)
      steamrollerRace.currentID = steamrollerRace.lowestID = sp.id;
    // Check if this is the highest spawn id
    if (steamrollerRace.highestID == -1 || steamrollerRace.highestID < sp.id)
      steamrollerRace.highestID = sp.id;

    // Add our spawn point
    SpawnZones[sp.id] = sp;
  }

  return true;
}

bool steamrollerRaceHandler::addPlayer(int index)
{
  bz_BasePlayerRecord *playerRecord;
  if ((playerRecord = bz_getPlayerByIndex(index)) != NULL) {
    Player player;
    player.callsign = playerRecord->callsign;
    player.score = 0;

    players[index] = player;

    // BZF_API bool bz_sendTextMessagef(int from, int to, const char* fmt, ...);
    bz_sendTextMessage(BZ_SERVER, index, "Welcome to the the steamroller race.");

    bz_freePlayerRecord(playerRecord);

    return true;
  }

  return false;
}

bool steamrollerRaceHandler::removePlayer(int index)
{
  std::map<int, Player>::iterator itr = players.find(index);
  if (itr != players.end())
  {
    players.erase(itr);
    return true;
  }

  return false;
}

BZ_PLUGIN(steamrollerRaceHandler)

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
