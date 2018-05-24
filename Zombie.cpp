/*
The "zombie" plugin is made by mrapple

This plugin is distributted under the GNU LGPL v. 3 (see below or lgpl.txt)

Mail tony (at) bzextreme (dot) com for questions

If you want to try it out, head over to bzextreme.com:5156

Please note that versions of the zombie plugin are periodically pushed out
The version you see may not be the actual one running on the bzextreme server

Special thanks go to the following:

trepan - individual bzdb vars, bonus points, misc help
murielle - team switching
ukeleleguy - original concept
BulletCatcher - misc help
Anxuiz - misc help
http:patorjk.com - ascii art

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this program.  If not, see
<http://www.gnu.org/licenses/>.
*/

#include "bzfsAPI.h"
#include "plugin_utils.h"
#include "StateDatabase.h"
#include "../src/bzfs/bzfs.h"
#include "../src/bzfs/CmdLineOptions.h"
#include "../src/bzfs/GameKeeper.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <ctype.h>

bool gameInProgress = false;
bool teamDecider = false;
bool updateNow = false;
bool dontUpdateNow = false;

int timer;
int num;

double lastsecond;
double lastsec;

std::map<int, bz_eTeamType> teamtracker;
std::map<int, bz_eTeamType> hometeam;
std::map<int, int> scoretracker;

bz_eTeamType winningTeam = eNoTeam;

extern CmdLineOptions *clOptions;
extern uint16_t curMaxPlayers;
extern TeamInfo team[NumTeams];

class Zombie : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
  virtual const char* Name() { return "Zombie Plugin"; }
  virtual void Init(const char* /*config*/);
  virtual void Event(bz_EventData *eventData);
  virtual void Cleanup(void);
  virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params);
};

BZ_PLUGIN(Zombie)

void Zombie::Init(const char* /*commandLine*/)
{
  Register(bz_ePlayerDieEvent);
  Register(bz_ePlayerSpawnEvent);
  Register(bz_ePlayerJoinEvent);
  Register(bz_eGetPlayerSpawnPosEvent);
  Register(bz_eFlagDroppedEvent);
  Register(bz_eTickEvent);
  Register(bz_eGetAutoTeamEvent);
  Register(bz_ePlayerPartEvent);

  bz_registerCustomSlashCommand("start", this);
  bz_registerCustomSlashCommand("stop", this);
  bz_registerCustomSlashCommand("switch", this);
  bz_registerCustomSlashCommand("restart", this);
  bz_registerCustomSlashCommand("join", this);
  bz_registerCustomSlashCommand("debug", this);
  bz_registerCustomSlashCommand("givepoints", this);
  bz_debugMessage(4, "zombie plugin loaded");
}

bool isAdmin(int playerID)
{
  bz_BasePlayerRecord* player = bz_getPlayerByIndex(playerID);
  bool admin = (player->hasPerm("ban") || player->hasPerm("shortBan"));
  bz_freePlayerRecord(player);
  return admin;
}

int getPlayerByCallsign(bz_ApiString callsign)
{
  bz_APIIntList* players = bz_newIntList();
  bz_getPlayerIndexList(players);
  int id = -1;
  bool found = false;
  for (unsigned int i = 0; i < players->size(); i++)
  {
    bz_BasePlayerRecord* player = bz_getPlayerByIndex(players->get(i));
    if (player->callsign == callsign)
    {
      id = (int)i;
      found = true;
    }
    bz_freePlayerRecord(player);
    if (found)
      break;
  }
  bz_deleteIntList(players);
  return id;
}

void fixTeamCount()
{
  int playerIndex, teamNum;
  for (teamNum = RogueTeam; teamNum < RabbitTeam; teamNum++)
    team[teamNum].team.size = 0;
  for (playerIndex = 0; playerIndex < curMaxPlayers; playerIndex++)
  {
    GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (p && p->player.isPlaying())
    {
      teamNum = p->player.getTeam();
      if (teamNum == RabbitTeam)
        teamNum = RogueTeam;
      team[teamNum].team.size++;
    }
  }
}

void addPlayer(GameKeeper::Player *playerData, int index)
{
  void *bufStart = getDirectMessageBuffer();
  void *buf = playerData->packPlayerUpdate(bufStart);
  if (playerData->getIndex() == index)
  {
    broadcastMessage(MsgAddPlayer, (char*)buf - (char*)bufStart, bufStart);
  }
  else
  {
    directMessage(index, MsgAddPlayer, (char*)buf - (char*)bufStart, bufStart);
  }
  int teamNum = int(playerData->player.getTeam());
  team[teamNum].team.size++;
  sendTeamUpdate(-1, teamNum);
  fixTeamCount();
}

void removePlayer(int playerIndex)
{
  GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(playerIndex);
  if (!playerData)
    return;
  void *buf, *bufStart = getDirectMessageBuffer();
  buf = nboPackUByte(bufStart, playerIndex);
  broadcastMessage(MsgRemovePlayer, (char*)buf - (char*)bufStart, bufStart);
  int teamNum = int(playerData->player.getTeam());
  --team[teamNum].team.size;
  sendTeamUpdate(-1, teamNum);
  fixTeamCount();
}

bool switchPlayer(int playerID, bz_eTeamType team)
{
  GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(playerID);
  if (!playerData)
    return true;
  removePlayer(playerID);
  playerData->player.setTeam((TeamColor)team);
  addPlayer(playerData, playerID);
  return true;
}

void setVar(int playerID, const std::string& key, const std::string& value)
{
  void *bufStart = getDirectMessageBuffer();
  void *buf = nboPackUShort(bufStart, 1);
  buf = nboPackUByte(buf, key.length());
  buf = nboPackString(buf, key.c_str(), key.length());
  buf = nboPackUByte(buf, value.length());
  buf = nboPackString(buf, value.c_str(), value.length());
  const int len = (char*)buf - (char*)bufStart;
  directMessage(playerID, MsgSetVar, len, bufStart);
}

void setZombie(int playerID)
{
  setVar(playerID, "_tankSpeed", "35");
  setVar(playerID, "_reloadTime", "0.0000001");
  setVar(playerID, "_radarLimit", "400");
  setVar(playerID, "_tankAngVel", "0.785398");
  bz_givePlayerFlag(playerID, "SR", false);
  switchPlayer(playerID, eGreenTeam);
}

void setHuman(int playerID)
{
  setVar(playerID, "_tankSpeed", "25");
  setVar(playerID, "_reloadTime", "3");
  setVar(playerID, "_radarLimit", "100");
  setVar(playerID, "_tankAngVel", "0.9");
  bz_removePlayerFlag(playerID);
  switchPlayer(playerID, eRedTeam);
}

void setRogue(int playerID)
{
  setVar(playerID, "_tankSpeed", "0.00000001");
  setVar(playerID, "_reloadTime", "0.00000001");
  setVar(playerID, "_radarLimit", "0");
  setVar(playerID, "_tankAngVel", "0.00000001");
  bz_removePlayerFlag(playerID);
  switchPlayer(playerID, eRogueTeam);
}

void setObserver(int playerID)
{
  setVar(playerID, "_tankSpeed", "25");
  setVar(playerID, "_reloadTime", "3.5");
  setVar(playerID, "_radarLimit", "800");
  setVar(playerID, "_tankAngVel", "0.785398");
  bz_removePlayerFlag(playerID);
  switchPlayer(playerID, eObservers);
}

void dontMove(int playerID)
{
  setVar(playerID, "_tankSpeed", "0.00000001");
  setVar(playerID, "_reloadTime", "0.00000001");
  setVar(playerID, "_radarLimit", "0");
  setVar(playerID, "_tankAngVel", "0.00000001");
  bz_removePlayerFlag(playerID);
}

void Zombie::Event(bz_EventData *eventData)
{
  switch (eventData->eventType)
  {
  default:
    break;
  case bz_ePlayerDieEvent:
  {
    bz_PlayerDieEventData_V1* diedata = (bz_PlayerDieEventData_V1*)eventData;
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(diedata->playerID);
    bz_BasePlayerRecord *killer = bz_getPlayerByIndex(diedata->killerID);

    // Someone who is actually playing
    if (diedata->killerID > 200)
      break;

    if (diedata->team == eRedTeam && diedata->killerTeam == eGreenTeam && gameInProgress)
    {
      bz_sendTextMessage(BZ_SERVER, diedata->playerID, "You are now a zombie -- Go kill those humans!");
      teamtracker[diedata->playerID] = eGreenTeam;
      bz_debugMessagef(1, "ZOMBIEDEBUG: Player: %s || Killer: %s || Player team: red || Killer team: green", player->callsign.c_str(), killer->callsign.c_str());
    }
    if (diedata->team == eGreenTeam && diedata->killerTeam == eRedTeam && gameInProgress)
    {
      bz_sendTextMessage(BZ_SERVER, diedata->playerID, "You are now a human -- Go kill those zombies!");
      teamtracker[diedata->playerID] = eRedTeam;
      bz_debugMessagef(1, "ZOMBIEDEBUG: Player: %s || Killer: %s || Player team: green || Killer team: red", player->callsign.c_str(), killer->callsign.c_str());
    }
    if (bz_getTeamCount(eGreenTeam) == 1 || bz_getTeamCount(eRedTeam) == 1)
    {
      if (bz_getTeamCount(eGreenTeam) == 1 && diedata->killerTeam == eRedTeam && diedata->team == eGreenTeam)
        winningTeam = eRedTeam;
      if (bz_getTeamCount(eRedTeam) == 1 && diedata->killerTeam == eGreenTeam && diedata->team == eRedTeam)
        winningTeam = eGreenTeam;
      // Lol wut?
      if (bz_getTeamCount(eRedTeam) == 1 && bz_getTeamCount(eGreenTeam) == 1)
        winningTeam = eNoTeam;
    }
    scoretracker[diedata->killerID]++;
    // Is our killer on a spree?
    if (scoretracker[diedata->killerID] == 3)
    {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s is on a rampage and was awarded 3 bonus points!", killer->callsign.c_str());
      //givePoints(diedata->killerID, 3);
      bz_incrementPlayerWins(diedata->killerID, 3);
    }
    // Is our killer on a spree?
    if (scoretracker[diedata->killerID] == 5)
    {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s is on a killing spree and was awarded 5 bonus points!", killer->callsign.c_str());
      //givePoints(diedata->killerID, 5);
      bz_incrementPlayerWins(diedata->killerID, 5);
    }
    // Is our killer on a spree?
    if (scoretracker[diedata->killerID] == 10)
    {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s is continuing the domination and was awarded 10 bonus points!", killer->callsign.c_str());
      //givePoints(diedata->killerID, 10);
      bz_incrementPlayerWins(diedata->killerID, 10);
    }
    // If the person who was killed had points, give the killer a few bonus points.
    if (scoretracker[diedata->playerID] > 2)
    {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s ended %s's reign and was awarded 5 bonus points!", killer->callsign.c_str(), player->callsign.c_str());
      //givePoints(diedata->killerID, 5);
      bz_incrementPlayerWins(diedata->killerID, 5);
    }
    scoretracker[diedata->playerID] = 0;
    bz_freePlayerRecord(player);
    bz_freePlayerRecord(killer);
    updateNow = true;
  }
  break;
  case bz_ePlayerJoinEvent:
  {
    bz_PlayerJoinPartEventData_V1* joindata = (bz_PlayerJoinPartEventData_V1*)eventData;
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(joindata->playerID);
    bz_debugMessagef(1, "ZOMBIEDEBUG: %s joining from %f", player->callsign.c_str(), player->ipAddress.c_str());
    scoretracker[joindata->playerID] = 0;
    if (gameInProgress || player->team == eObservers)
    {
      teamtracker[joindata->playerID] = eObservers;
      hometeam[joindata->playerID] = eObservers;
      bz_debugMessagef(1, "ZOMBIEDEBUG: %s joined to the observer team", player->callsign.c_str());
      if (gameInProgress)
      {
        bz_sendTextMessage(BZ_SERVER, joindata->playerID, "There is currently a game in progress. Wait untill the game is done, then type /join");
      }
      else
      {
        bz_sendTextMessage(BZ_SERVER, joindata->playerID, "The game has not started. If you would like to join, type /join.");
      }
    }
    else
    {
      teamtracker[joindata->playerID] = eRogueTeam;
      hometeam[joindata->playerID] = eRogueTeam;
      bz_debugMessagef(1, "ZOMBIEDEBUG: %s joined to the rogue team", player->callsign.c_str());
      dontMove(joindata->playerID);
      bz_sendTextMessage(BZ_SERVER, joindata->playerID, "To start the game, wait untill there are at least two players, then type /start");
    }
    bz_freePlayerRecord(player);
    // MAKE SURE WE DONT UPDATE ANYTHING!
    updateNow = false;
    dontUpdateNow = true;
  }
  break;
  case bz_ePlayerSpawnEvent:
  {
    bz_PlayerSpawnEventData_V1* spawndata = (bz_PlayerSpawnEventData_V1*)eventData;
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(spawndata->playerID);
    if (spawndata->team == eGreenTeam && gameInProgress)
    {
      bz_givePlayerFlag(spawndata->playerID, "SR", true);
      bz_debugMessagef(1, "ZOMBIEDEBUG: %s spawned and was given a SR flag", player->callsign.c_str());
    }
    bz_freePlayerRecord(player);
  }
  break;
  case bz_eFlagDroppedEvent:
  {
    bz_FlagDroppedEventData_V1* flagdropdata = (bz_FlagDroppedEventData_V1*)eventData;
    bz_givePlayerFlag(flagdropdata->playerID, flagdropdata->flagType, true);
  }
  break;
  case bz_eGetAutoTeamEvent:
  {
    bz_GetAutoTeamEventData_V1 *autoteamdata = (bz_GetAutoTeamEventData_V1*)eventData;
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(autoteamdata->playerID);
    autoteamdata->handled = true;
    if (gameInProgress || autoteamdata->team == eObservers || timer == 1)
    {
      autoteamdata->team = eObservers;
      teamtracker[autoteamdata->playerID] = eObservers;
      hometeam[autoteamdata->playerID] = eObservers;
      bz_debugMessagef(1, "ZOMBIEDEBUG: %s was set as an observer via autoteam", player->callsign.c_str());
    }
    else
    {
      teamtracker[autoteamdata->playerID] = eRogueTeam;
      hometeam[autoteamdata->playerID] = eRogueTeam;
      autoteamdata->team = eRogueTeam;
      bz_debugMessagef(1, "ZOMBIEDEBUG: %s was set as a rogue via autoteam", player->callsign.c_str());
    }
    bz_freePlayerRecord(player);
    // MAKE SURE WE DONT UPDATE ANYTHING!
    updateNow = false;
    dontUpdateNow = true;
  }
  break;
  case bz_ePlayerPartEvent:
  {
    bz_PlayerJoinPartEventData_V1* partdata = (bz_PlayerJoinPartEventData_V1*)eventData;

    teamtracker[partdata->playerID] = eNoTeam;
    hometeam[partdata->playerID] = eNoTeam;
    scoretracker[partdata->playerID] = 0;
  }
  break;
  case bz_eTickEvent:
  {
    bz_TickEventData_V1* tickdata = (bz_TickEventData_V1*)eventData;
    if (timer != 0)
    {
      if (tickdata->eventTime - 1>lastsecond)
      {
        lastsecond = tickdata->eventTime;
        timer -= 1;
        std::string message;
        switch (timer)
        {
        case 0:
          message = std::string("Go!");
          break;
        case 1:
          message = std::string("1");
          break;
        case 2:
          message = std::string("2");
          break;
        case 3:
          message = std::string("3");
          break;
        case 4:
          message = std::string("4");
          break;
        case 5:
          message = std::string("5");
          break;
        case 6:
          message = std::string("6");
          break;
        case 7:
          message = std::string("7");
          break;
        case 8:
          message = std::string("8");
          break;
        case 9:
          message = std::string("9");
          break;
        case 10:
          message = std::string("10");
          break;
        case 11:
          message = std::string("11");
          break;
        case 12:
          message = std::string("12");
          break;
        case 13:
          message = std::string("13 seconds untill game start");
          break;
        case 14:
          message = std::string("Last chance to join! If you are an observer, type /join to join now!");
          break;
        }
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, message.c_str());
      }

      if (timer < 1)
      {
        bz_debugMessage(1, "ZOMBIEDEBUG: Timer hit zero. Starting team decision making.");
        timer = 0;
        bz_APIIntList* playerlist = bz_newIntList();
        bz_getPlayerIndexList(playerlist);
        if (teamDecider)
        {
          num = 0;
          teamDecider = false;
        }
        else
        {
          num = 1;
          teamDecider = true;
        }
        for (unsigned int i = 0; i < playerlist->size(); i++)
        {
          bz_BasePlayerRecord *updateData;
          updateData = bz_getPlayerByIndex(playerlist->get(i));
          if (updateData->team == eRogueTeam)
          {
            // Check if the number is odd or even
            if (num % 2 == 0)
            {
              teamtracker[updateData->playerID] = eGreenTeam;
              setZombie(updateData->playerID);
              hometeam[updateData->playerID] = eGreenTeam;
            }
            else
            {
              teamtracker[updateData->playerID] = eRedTeam;
              setHuman(updateData->playerID);
              hometeam[updateData->playerID] = eRedTeam;
            }
            bz_debugMessagef(1, "ZOMBIEDEBUG: %s confirmed as a player. Decided team: ", updateData->callsign.c_str(), num % 2 == 0 ? "green" : "red");
            num++;
          }
          bz_killPlayer(updateData->playerID, false, -1);
          bz_freePlayerRecord(updateData);
        }
        bz_deleteIntList(playerlist);
        winningTeam = eNoTeam;
        gameInProgress = true;
        bz_debugMessage(1, "ZOMBIEDEBUG: Loop done. Game in progress.");
      }
    }

    if ((bz_getTeamCount(eRedTeam) == 0 || bz_getTeamCount(eGreenTeam) == 0) && gameInProgress)
    {
      gameInProgress = false;
      bz_debugMessage(1, "ZOMBIEDEBUG: Team won, ending game...");
      bz_resetTeamScores();
      bz_APIIntList* playerlist = bz_newIntList();
      bz_getPlayerIndexList(playerlist);
      for (unsigned int i = 0; i < playerlist->size(); i++)
      {
        bz_BasePlayerRecord *updateData;
        updateData = bz_getPlayerByIndex(playerlist->get(i));

        if (updateData->team != eObservers)
        {
          dontMove(updateData->playerID);
          setRogue(updateData->playerID);
          teamtracker[updateData->playerID] = eRogueTeam;
          bz_killPlayer(updateData->playerID, false, -1);
          bz_debugMessagef(1, "ZOMBIEDEBUG: %s was in the game. Killed and set as a rogue.", updateData->callsign.c_str());
        }

        bz_freePlayerRecord(updateData);
      }

      bz_deleteIntList(playerlist);

      // It might look wierd here because there are double "\"
      if (winningTeam == eGreenTeam)
      {
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "----------------------------------------------------------------------------------------------------------");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, " /$$$$$$$$                         /$$       /$$                           /$$      /$$ /$$            /$$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "|_____ $$                         | $$      |__/                          | $$  /$ | $$|__/           | $$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "     /$$/   /$$$$$$  /$$$$$$/$$$$ | $$$$$$$  /$$  /$$$$$$   /$$$$$$$      | $$ /$$$| $$ /$$ /$$$$$$$  | $$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "    /$$/   /$$__  $$| $$_  $$_  $$| $$__  $$| $$ /$$__  $$ /$$_____/      | $$/$$ $$ $$| $$| $$__  $$ | $$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "   /$$/   | $$  \\ $$| $$ \\ $$ \\ $$| $$  \\ $$| $$| $$$$$$$$|  $$$$$$       | $$$$_  $$$$| $$| $$  \\ $$ |__/");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "  /$$/    | $$  | $$| $$ | $$ | $$| $$  | $$| $$| $$_____/ \\____  $$      | $$$/ \\  $$$| $$| $$  | $$     ");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, " /$$$$$$$$|  $$$$$$/| $$ | $$ | $$| $$$$$$$/| $$|  $$$$$$$ /$$$$$$$/      | $$/   \\  $$| $$| $$  | $$  /$$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "|________/ \\______/ |__/ |__/ |__/|_______/ |__/ \\_______/|_______/       |__/     \\__/|__/|__/  |__/ |__/");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "----------------------------------------------------------------------------------------------------------");
        bz_debugMessage(1, "ZOMBIEDEBUG: Zombies won the game");
      }
      if (winningTeam == eRedTeam)
      {
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "------------------------------------------------------------------------------------------------------");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, " /$$   /$$                                                             /$$      /$$ /$$            /$$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$  | $$                                                            | $$  /$ | $$|__/           | $$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$  | $$ /$$   /$$ /$$$$$$/$$$$   /$$$$$$  /$$$$$$$   /$$$$$$$      | $$ /$$$| $$ /$$ /$$$$$$$  | $$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$$$$$$$| $$  | $$| $$_  $$_  $$ |____  $$| $$__  $$ /$$_____/      | $$/$$ $$ $$| $$| $$__  $$ | $$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$__  $$| $$  | $$| $$ \\ $$ \\ $$  /$$$$$$$| $$  \\ $$|  $$$$$$       | $$$$_  $$$$| $$| $$  \\ $$ |__/");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$  | $$| $$  | $$| $$ | $$ | $$ /$$__  $$| $$  | $$ \\____  $$      | $$$/ \\  $$$| $$| $$  | $$     ");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$  | $$|  $$$$$$/| $$ | $$ | $$|  $$$$$$$| $$  | $$ /$$$$$$$/      | $$/   \\  $$| $$| $$  | $$  /$$");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "|__/  |__/ \\______/ |__/ |__/ |__/ \\_______/|__/  |__/|_______/       |__/     \\__/|__/|__/  |__/ |__/");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "------------------------------------------------------------------------------------------------------");								bz_debugMessage(1, "ZOMBIEDEBUG: Humans won the game");
      }
      if (bz_getTeamCount(eRogueTeam) > 1 || bz_getTeamCount(eGreenTeam) > 1 || bz_getTeamCount(eRedTeam) > 1)
      {
        bz_debugMessage(1, "ZOMBIEDEBUG: It seems we have enough players, starting another game.");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "The server has automatically started a new game");
        timer = 15;
      }
      winningTeam = eNoTeam;
    }

    // Make sure we're only checking every two seconds or when we need to update
    // Make sure a game is running
    if ((tickdata->eventTime - 2>lastsec || updateNow) && gameInProgress)
    {
      // If contradicted, then we're going to update next time
      if (dontUpdateNow)
      {
        dontUpdateNow = false;
      }
      else
      {
        lastsec = tickdata->eventTime;
        bz_APIIntList* playerlist = bz_newIntList();
        bz_getPlayerIndexList(playerlist);
        for (unsigned int i = 0; i < playerlist->size(); i++)
        {
          bz_BasePlayerRecord *updateData = bz_getPlayerByIndex(playerlist->get(i));

          if (updateData->team != teamtracker[updateData->playerID])
          {
            bz_debugMessagef(1, "ZOMBIEDEBUG: %s's team was not in sync with the team tracker! Updating now...", updateData->callsign.c_str());
            switch (teamtracker[updateData->playerID])
            {
            default:
              break;
            case eGreenTeam:
              setZombie(updateData->playerID);
              break;
            case eRedTeam:
              setHuman(updateData->playerID);
              break;
            case eRogueTeam:
              setRogue(updateData->playerID);
              break;
            case eObservers:
            {
              bz_killPlayer(updateData->playerID, false, -1);
              setObserver(updateData->playerID);
            }
            break;
            }
            bz_debugMessagef(1, "ZOMBIEDEBUG: Team updated for %s", updateData->callsign.c_str());
          }
          bz_freePlayerRecord(updateData);
        }
        bz_deleteIntList(playerlist);
        updateNow = false;
      }
    }
  }
  break;
  }
}

bool Zombie::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params)
{
  if (command == "start")
  {
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
    if (!gameInProgress)
    {
      if (bz_getTeamCount(eRogueTeam) > 1)
      {
        if (timer == 0)
        {
          bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has started a new game of zombie", player->callsign.c_str());
          timer = 15;
          bz_debugMessagef(1, "%s has started a new game of zombie", player->callsign.c_str());
        }
        else
        {
          bz_sendTextMessagef(BZ_SERVER, playerID, "%s, you can not start a countdown when a countdown has been started", player->callsign.c_str());
          bz_debugMessagef(1, "%s has attempted to start a countdown when a countdown was already running", player->callsign.c_str());
        }
      }
      else
      {
        bz_sendTextMessagef(BZ_SERVER, playerID, "%s, you can not start a countdown with less then two players", player->callsign.c_str());
        bz_debugMessagef(1, "%s has attempted to start a countdown with less then two players", player->callsign.c_str());
      }
    }
    else
    {
      bz_sendTextMessagef(BZ_SERVER, playerID, "%s, you can not start a countdown when there is already a game in progress", player->callsign.c_str());
      bz_debugMessagef(1, "%s has attempted to start a countdown when a game is already in progress", player->callsign.c_str());
    }
    bz_freePlayerRecord(player);
  }
  else if (command == "stop")
  {
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
    if (isAdmin(playerID))
    {
      if (gameInProgress || timer > 0)
      {
        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has ended the current game of zombie", player->callsign.c_str());
        bz_debugMessagef(1, "%s ended the current game of zombie", player->callsign.c_str());

        gameInProgress = false;
        timer = 0;

        // Kill all players and make them not move
        bz_APIIntList* playerlist = bz_newIntList();
        bz_getPlayerIndexList(playerlist);

        for (unsigned int i = 0; i < playerlist->size(); i++)
        {
          bz_BasePlayerRecord *updateData;
          updateData = bz_getPlayerByIndex(playerlist->get(i));

          if (updateData->team != eObservers)
          {
            dontMove(updateData->playerID);
            setRogue(updateData->playerID);
            bz_killPlayer(updateData->playerID, false, -1);
          }

          bz_freePlayerRecord(updateData);
        }
        bz_deleteIntList(playerlist);
      }
      else
      {
        bz_sendTextMessagef(BZ_SERVER, playerID, "%s, there is no game in progress", player->callsign.c_str());
      }
    }
    else
      return false;
    bz_freePlayerRecord(player);
  }
  else if (command == "switch")
  {
    bz_APIStringList params;
    params.tokenize(message.c_str(), " ", 0, true);

    if (isAdmin(playerID))
    {
      if (params.size() >= 2)
      {
        int otherPlayerID = getPlayerByCallsign(params.get(0));
        if (otherPlayerID != -1)
        {
          bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
          if (params.get(1) == "red" || params.get(1) == "green" || params.get(1) == "rogue" || params.get(1) == "observer")
          {
            bz_killPlayer(otherPlayerID, false, -1);

            if (params.get(1) == "red")
              teamtracker[otherPlayerID] = eRedTeam;

            if (params.get(1) == "green")
              teamtracker[otherPlayerID] = eGreenTeam;

            if (params.get(1) == "rogue")
              teamtracker[otherPlayerID] = eRogueTeam;

            if (params.get(1) == "observer")
              teamtracker[otherPlayerID] = eObservers;

            updateNow = true;
            bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s switched %s to the %s team", player->callsign.c_str(), params.get(0).c_str(), params.get(1).c_str());
            bz_freePlayerRecord(player);
          }
          else
          {
            bz_sendTextMessagef(BZ_SERVER, playerID, "Not a valid team: %s", params.get(1).c_str());
          }
        }
        else
        {
          bz_sendTextMessagef(BZ_SERVER, playerID, "No Such Player: %s", params.get(0).c_str());
        }
      }
      else
      {
        bz_sendTextMessage(BZ_SERVER, playerID, "Usage: /switch <player> <team>");
      }
    }
    else
    {
      bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to switch teams");
    }

    return true;
  }
  else if (command == "restart")
  {
    // TODO: Add a restart command
    // The stop and start commands should be made into functions
    // so that they can easily be called here and above.
    // (No duplication of code)
  }
  else if (command == "join")
  {
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
    if (player->team == eObservers)
    {
      if (!gameInProgress)
      {
        teamtracker[playerID] = eRogueTeam;
        dontMove(playerID);
        setRogue(playerID);
        bz_sendTextMessagef(BZ_SERVER, playerID, "The game will start shortly. Good luck %s!", player->callsign.c_str());
      }
      else
      {
        bz_sendTextMessagef(BZ_SERVER, playerID, "%s, a game is in progress. Please wait until it finishes before joining.", player->callsign.c_str());
      }
    }
    else
    {
      bz_sendTextMessagef(BZ_SERVER, playerID, "%s, you must be an observer to use this command.", player->callsign.c_str());
    }
    bz_freePlayerRecord(player);
  }
  else if (command == "debug")
  {
    if (isAdmin(playerID))
    {
      // Server Statistics
      bz_sendTextMessage(BZ_SERVER, playerID, "--------( Server Stats )--------");
      bz_sendTextMessagef(BZ_SERVER, playerID, "Game running: %s", gameInProgress ? "Yes" : "No");
      bz_sendTextMessagef(BZ_SERVER, playerID, "Countdown in progress: %s", timer>0 ? "Yes" : "No");
      bz_sendTextMessagef(BZ_SERVER, playerID, "Countdown time: %d", timer);
      bz_sendTextMessagef(BZ_SERVER, playerID, "Countdown last second: %d", lastsecond);

      // Player Statistics
      bz_sendTextMessage(BZ_SERVER, playerID, "--------( Player Stats )--------");
      bz_sendTextMessagef(BZ_SERVER, playerID, "Rogue players: %d", bz_getTeamCount(eRogueTeam));
      bz_sendTextMessagef(BZ_SERVER, playerID, "Green players: %d", bz_getTeamCount(eGreenTeam));
      bz_sendTextMessagef(BZ_SERVER, playerID, "Red players: %d", bz_getTeamCount(eRedTeam));
      bz_sendTextMessagef(BZ_SERVER, playerID, "Observers: %d", bz_getTeamCount(eObservers));

      // Team Tracker Statistics
      bz_sendTextMessage(BZ_SERVER, playerID, "--------( TeamTracker Stats )--------");
      bz_APIIntList* playerlist = bz_newIntList();
      bz_getPlayerIndexList(playerlist);
      for (unsigned int i = 0; i < playerlist->size(); i++)
      {
        bz_BasePlayerRecord *updateData = bz_getPlayerByIndex(playerlist->get(i));

        switch (teamtracker[updateData->playerID])
        {
        default:
          break;
        case eGreenTeam:
          bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - green", updateData->playerID, updateData->callsign.c_str());
          break;
        case eRedTeam:
          bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - red", updateData->playerID, updateData->callsign.c_str());
          break;
        case eRogueTeam:
          bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - rogue", updateData->playerID, updateData->callsign.c_str());
          break;
        case eObservers:
          bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - observer", updateData->playerID, updateData->callsign.c_str());
          break;
        }
        bz_freePlayerRecord(updateData);
      }
      bz_deleteIntList(playerlist);

      // Score Tracker Statistics
      bz_sendTextMessage(BZ_SERVER, playerID, "--------( ScoreTracker Stats )--------");
      bz_APIIntList* scoretrack = bz_newIntList();
      bz_getPlayerIndexList(scoretrack);
      for (unsigned int i = 0; i < playerlist->size(); i++)
      {
        bz_BasePlayerRecord *updateData = bz_getPlayerByIndex(playerlist->get(i));
        bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - %d", updateData->playerID, updateData->callsign.c_str(), scoretracker[updateData->playerID]);
        bz_freePlayerRecord(updateData);
      }
      bz_deleteIntList(scoretrack);
    }
    else
      return false;
  }
  else if (command == "givepoints")
  {
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
    bz_APIStringList params;
    params.tokenize(message.c_str(), " ", 0, true);
    if (isAdmin(playerID))
    {
      if (params.size() == 2)
      {
        int otherPlayerID = getPlayerByCallsign(params.get(0));
        int pointsToGive = atoi(params.get(1).c_str());
        if (otherPlayerID != -1)
        {
          //givePoints(otherPlayerID, pointsToGive);
          bz_incrementPlayerWins(otherPlayerID, pointsToGive);
          bz_sendTextMessagef(BZ_SERVER, playerID, "You gave %s %d point%s", params.get(0).c_str(), pointsToGive, num == 1 ? "" : "s");
        }
        else
        {
          bz_sendTextMessagef(BZ_SERVER, playerID, "%s, \"%s\" is not a valid player", player->callsign.c_str(), params.get(0).c_str());
        }
      }
      else
      {
        bz_sendTextMessage(BZ_SERVER, playerID, "Usage: /givepoints <player> <points>");
      }
    }
    else
      return false;
  }
  return true;
}

void Zombie::Cleanup(void)
{
  Flush();

  bz_removeCustomSlashCommand("start");
  bz_removeCustomSlashCommand("stop");
  bz_removeCustomSlashCommand("switch");
  bz_removeCustomSlashCommand("restart");
  bz_removeCustomSlashCommand("join");
  bz_removeCustomSlashCommand("debug");
  bz_removeCustomSlashCommand("givepoints");

  bz_debugMessage(4, "zombie plugin unloaded");
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
