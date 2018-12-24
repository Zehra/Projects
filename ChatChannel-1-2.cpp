/* "THE BEER-WARE LICENSE" (Revision 42):
* <dennis@moellegaard.dk> wrote this file. As long as you retain this notice
* you can do whatever you want with this stuff. If we meet some day, and you
* think this stuff is worth it, you can buy me a beer in return.
* - Dennis Møllegaard Pedersen, http://dennis.moellegaard.dk
*/

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include "bzfsAPI.h"

int MAXCHANNELS = 4;
#define VERSIONSTR "ChatChannel v1.2 By Meacan (aka Dennis Moellegaard Pedersen)."

class ChannelInfo;
class PlayerInfo;
typedef std::vector<int>					ChannelMembers;
typedef std::map<std::string, ChannelInfo>	Channels;
typedef std::map<int, PlayerInfo>			PlayerMeta;	// Which channel is player using + nick

struct PlayerInfo {
  std::string		name;
  std::string		channel;
};

class ChannelInfo {
public:
  ChannelMembers	members;
  ChannelMembers	invitedPlayers;

  ChannelInfo(std::string _name = "") {
    locked = false;
    name = _name;
  }

  bool isMember(int);
  bool isInvited(int);

  bool isLocked() { return locked; }
  void lock(bool l) { locked = l; }

  void removePlayers(const char*);
  void removePlayer(int);
  void addPlayer(int);
  void invitePlayer(int);
  void broadcast(int, const char*);

protected:
  bool				locked;
  std::string			name;
};

class ChatChannel : public bz_Plugin, bz_CustomSlashCommandHandler
{
public:
  virtual const char* Name() { return VERSIONSTR; }
  virtual void Init(const char* /*config*/);
  virtual void Event(bz_EventData *eventData);
  virtual void Cleanup(void);
  virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params);
  Channels		channels;

protected:
  PlayerMeta	playerConfig;

  void eventPlayerJoin(bz_PlayerJoinPartEventData_V1*);
  void eventPlayerPart(bz_PlayerJoinPartEventData_V1*);
  void eventPlayerChat(bz_ChatEventData_V1*);

  void showChannels(int);
  int countChannels(int);

  void handleChannelCreate(int playerID, const std::string);
  void handleChannelJoin(int playerID, const std::string);
  void handleChannelPart(int playerID, const std::string);
  void handleChannelUse(int playerID, const std::string = "");
  void handleChannelLock(int playerID, const std::string, bool lock);
  void handleChannelInvite(int playerID, const std::string, const std::string);

  void setChannel(int, const std::string = "");

  static void showUsage(int);
  static const char* usageChCreate() {
    return  "/channel create <channel>";
  }
  static const char* usageChJoin() {
    return  "/channel join <channel>";
  }
  static const char* usageChPart() {
    return  "/channel part <channel>";
  }
  static const char* usageChUse() {
    return  "/channel use [channel]       (or \"/public\" as alias for \"/channel use\")";
  }
  static const char* usageChList() {
    return  "/channel list";
  }
  static const char* usageChLock() {
    return  "/channel lock <channel>";
  }
  static const char* usageChUnlock() {
    return  "/channel unlock <channel>";
  }
  static const char* usageChInvite() {
    return  "/channel invite <channel> <callsign>";
  }
  static const char* usageChVersion() {
    return  "/channel version";
  }
};

BZ_PLUGIN(ChatChannel)

void ChatChannel::Init(const char*commandLine) {
  bz_debugMessage(0, VERSIONSTR);
  bz_registerCustomSlashCommand("channel", this);
  bz_registerCustomSlashCommand("ch", this);
  bz_registerCustomSlashCommand("public", this);

  Register(bz_eRawChatMessageEvent);
  Register(bz_ePlayerJoinEvent);
  Register(bz_ePlayerPartEvent);
}

void ChatChannel::Cleanup(void) {
  bz_removeCustomSlashCommand("channel");
  bz_removeCustomSlashCommand("ch");
  bz_removeCustomSlashCommand("public");

  Flush();
}

bool ChatChannel::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params) {

  if (command == "channel" || command == "ch") {
    int argc = params->size();

    if (argc == 0) {
      showUsage(playerID);
      return true;
    }

    if (params->get(0) == "create") {
      if (argc != 2)
        bz_sendTextMessage(BZ_SERVER, playerID, usageChCreate());
      else
        handleChannelCreate(playerID, bz_toupper(params->get(1).c_str()));
    }
    else if (params->get(0) == "join") {
      if (argc != 2)
        bz_sendTextMessage(BZ_SERVER, playerID, usageChJoin());
      else
        handleChannelJoin(playerID, bz_toupper(params->get(1).c_str()));
    }
    else if (params->get(0) == "part") {
      if (argc != 2)
        bz_sendTextMessage(BZ_SERVER, playerID, usageChPart());
      else
        handleChannelPart(playerID, bz_toupper(params->get(1).c_str()));
    }
    else if (params->get(0) == "use") {
      if (argc == 1 || argc == 2)
        setChannel(playerID, argc == 1 ? "" : bz_toupper(params->get(1).c_str()));
      else
        bz_sendTextMessage(BZ_SERVER, playerID, usageChUse());
    }
    else if (params->get(0) == "list") {
      if (argc == 1)
        showChannels(playerID);
      else
        bz_sendTextMessage(BZ_SERVER, playerID, usageChList());
    }
    else if (params->get(0) == "lock") {
      if (argc == 2)
        handleChannelLock(playerID, bz_toupper(params->get(1).c_str()), true);
      else
        bz_sendTextMessage(BZ_SERVER, playerID, usageChLock());
    }
    else if (params->get(0) == "unlock") {
      if (argc == 2)
        handleChannelLock(playerID, bz_toupper(params->get(1).c_str()), false);
      else
        bz_sendTextMessage(BZ_SERVER, playerID, usageChUnlock());
    }
    else if (params->get(0) == "invite") {
      if (argc == 3)
        handleChannelInvite(playerID, bz_toupper(params->get(1).c_str()), params->get(2).c_str());
      else
        bz_sendTextMessage(BZ_SERVER, playerID, usageChInvite());
    }
    else if (params->get(0) == "version") {
      bz_sendTextMessage(BZ_SERVER, playerID, VERSIONSTR);
    }
    else {
      showUsage(playerID);
    }
  }
  else if (command == "public") {
    setChannel(playerID, "");
  }

  return true;
}

void ChatChannel::Event(bz_EventData *eventData) {
  switch (eventData->eventType) {
  case bz_ePlayerJoinEvent:
    eventPlayerJoin((bz_PlayerJoinPartEventData_V1*)eventData);
    break;
  case bz_ePlayerPartEvent:
    eventPlayerPart((bz_PlayerJoinPartEventData_V1*)eventData);
    break;
  case bz_eRawChatMessageEvent:
    eventPlayerChat((bz_ChatEventData_V1*)eventData);
    break;
  default:
    break;
  }
}

void ChannelInfo::removePlayers(const char *msg) {
  ChannelMembers::iterator m;

  for (m = members.begin(); m != members.end(); ++m)
    bz_sendTextMessage(BZ_SERVER, *m, msg);

  members.clear();
  invitedPlayers.clear();
}

void ChannelInfo::removePlayer(int playerID) {
  ChannelMembers::iterator	m;

  for (m = members.begin(); m != members.end(); m++) {
    if (*m == playerID) {
      bz_BasePlayerRecord* player = bz_getPlayerByIndex(playerID);
      broadcast(BZ_SERVER, bz_format("%s leaves", player->callsign.c_str()));
      bz_freePlayerRecord(player);
      members.erase(m);
      return;
    }
  }
}

bool ChannelInfo::isMember(int playerID) {
  ChannelMembers::iterator	m;
  for (m = members.begin(); m != members.end(); m++)
    if (*m == playerID)
      return true;

  return false;
}

bool ChannelInfo::isInvited(int playerID) {
  ChannelMembers::iterator	m;
  for (m = invitedPlayers.begin(); m != invitedPlayers.end(); m++)
    if (*m == playerID)
      return true;

  return false;
}

void ChannelInfo::addPlayer(int playerID) {
  bz_BasePlayerRecord* player = bz_getPlayerByIndex(playerID);
  members.push_back(playerID);
  broadcast(BZ_SERVER, bz_format("%s joins.", player->callsign.c_str()));
  bz_freePlayerRecord(player);

  ChannelMembers::iterator i;
  // Remove player from invited list (if exists)
  for (i = invitedPlayers.begin(); i != invitedPlayers.end(); ++i) {
    if (*i == playerID) {
      invitedPlayers.erase(i);
      return;
    }
  }
}

void ChannelInfo::invitePlayer(int invitedPlayerID) {
  bz_BasePlayerRecord* player = bz_getPlayerByIndex(invitedPlayerID);
  invitedPlayers.push_back(invitedPlayerID);
  // Inform player
  bz_sendTextMessagef(BZ_SERVER, invitedPlayerID, "You have been invited to join %s. To join, write: /channel join %s", name.c_str(), name.c_str());
  // Inform channel members
  broadcast(BZ_SERVER, bz_format("%s have been invited.", player->callsign.c_str()));
  bz_freePlayerRecord(player);
}

void ChannelInfo::broadcast(int senderPlayerID, const char* msg) {
  ChannelMembers::iterator m;
  //	printf("  ^ ChatChannel: %s\n", name.c_str());
  for (m = members.begin(); m != members.end(); ++m) {
#ifdef SENDASPRVMSG
    // This is a less hackerish method, however if you're in a group
    // with 10 people, the sending player will see 10 [->nick] CHANNEL: Message,
    // which is basicially then just noise. The currenty hack ("white-text")
    // is better, but this bypasses /ignore - as the sender is set
    // to the receiving userid.
#else
    senderPlayerID = *m;
#endif
    bz_sendTextMessage(senderPlayerID, *m, bz_format("%s: %s", name.c_str(), msg));
  }
}

void ChatChannel::eventPlayerJoin(bz_PlayerJoinPartEventData_V1* event) {
  PlayerInfo	newPlayer;
  newPlayer.channel = std::string("");		// Public
  newPlayer.name = std::string(event->record->callsign.c_str());
  //playerConfig[event->playerID] = newPlayer;		// FIXME Why dosnt this work?
  playerConfig.insert(std::make_pair(event->playerID, newPlayer));

  if (channels.size())
    showChannels(event->playerID);
}

void ChatChannel::eventPlayerPart(bz_PlayerJoinPartEventData_V1* event) {
  Channels::iterator			c;

  for (c = channels.begin(); c != channels.end(); ++c) {
    c->second.removePlayer(event->playerID);

    if (c->second.members.empty()) {
      channels.erase(c);
      c = channels.begin(); // c might be invalidated by this, start again
      if (c == channels.end())
        return;
    }
  }

  PlayerMeta::iterator	i;
  for (i = playerConfig.begin(); i != playerConfig.end(); ++i) {
    if (i->first == event->playerID) {
      playerConfig.erase(i);
      //playerConfig.erase(event->playerID);		// FIXME - why dosnt this work? 
      break;
    }
  }
}
void ChatChannel::eventPlayerChat(bz_ChatEventData_V1* event) {
  if (event->to == BZ_ALLUSERS) {

    std::string curChannel = playerConfig[event->from].channel;

    if (curChannel != "" && (*(event->message.c_str()) != '/' || strncmp((event->message.c_str()), "/me", 3) == 0)) {
      bz_BasePlayerRecord* player = bz_getPlayerByIndex(event->from);


      if ((*(event->message.c_str())) == '/') {
        // This is a /me text, need to rewrite text
        std::string action(event->message.c_str()); // skip '/me '
        action = action.substr(4);
        channels[curChannel].broadcast(event->from, bz_format("%s %s", player->callsign.c_str(), action.c_str()));
      }
      else {
        // Everything else
        channels[curChannel].broadcast(event->from, bz_format("%s: %s", player->callsign.c_str(), event->message.c_str()));

      }
      event->message = "";

      bz_freePlayerRecord(player);
    }
  }
}

void ChatChannel::showChannels(int playerID) {
  Channels::iterator 			i;
  ChannelMembers::iterator	m;
  bz_BasePlayerRecord* 			player;
  bool						first;
  std::string 				line;
  bool						isAdmin;

  if (channels.empty()) {
    bz_sendTextMessage(BZ_SERVER, playerID, "No channels created.");
    return;
  }

  player = bz_getPlayerByIndex(playerID);
  isAdmin = player->admin;

  bz_sendTextMessage(BZ_SERVER, playerID, "Channels created:");

  for (i = channels.begin(); i != channels.end(); ++i) {
    first = true;
    line = " " + i->first;
    if (i->second.isLocked())
      line += " [locked]";

    line += ": ";

    // If channel not locked, or ..
    // if locked, but im a member, or..
    // im admin .. then display memberlist
    if (!i->second.isLocked() ||
      (i->second.isLocked() && i->second.isMember(playerID)) || isAdmin) {
      // Memberlist
      for (m = i->second.members.begin(); m != i->second.members.end(); m++) {
        bz_BasePlayerRecord* player = bz_getPlayerByIndex(*m);
        if (!first)
          line += ", ";
        else
          first = false;

        line += player->callsign.c_str();
      }
    }
    else {
      line += "Information hidden.";
    }

    bz_sendTextMessage(BZ_SERVER, playerID, line.c_str());
  }
  bz_freePlayerRecord(player);
}

int ChatChannel::countChannels(int playerID) {
  Channels::iterator 			i;

  int count = 0;
  for (i = channels.begin(); i != channels.end(); ++i) {
    if (i->second.isMember(playerID))
      count++;
  }

  return count;
}

void ChatChannel::setChannel(int playerID, const std::string channel) {
  if (channel == "") {
    playerConfig[playerID].channel = std::string("");
    bz_sendTextMessage(BZ_SERVER, playerID, "Using public channel");
  }
  else {
    Channels::iterator i = channels.find(channel);

    if (i == channels.end()) {
      bz_sendTextMessagef(BZ_SERVER, playerID, "Channel '%s' does not exist!", channel.c_str());
    }
    else {
      if (i->second.isMember(playerID)) {
        playerConfig[playerID].channel = channel;
        bz_sendTextMessagef(BZ_SERVER, playerID, "Using channel: '%s'", channel.c_str());
      }
      else {
        bz_sendTextMessagef(BZ_SERVER, playerID, "You are not a member of channel '%s'", channel.c_str());
      }
    }
  }
}

void ChatChannel::showUsage(int playerID) {
  bz_sendTextMessage(BZ_SERVER, playerID, "Usage: /channel <command> ...");
  bz_sendTextMessage(BZ_SERVER, playerID, usageChCreate());
  bz_sendTextMessage(BZ_SERVER, playerID, usageChJoin());
  bz_sendTextMessage(BZ_SERVER, playerID, usageChPart());
  bz_sendTextMessage(BZ_SERVER, playerID, usageChUse());
  bz_sendTextMessage(BZ_SERVER, playerID, usageChList());
  bz_sendTextMessage(BZ_SERVER, playerID, usageChLock());
  bz_sendTextMessage(BZ_SERVER, playerID, usageChUnlock());
  bz_sendTextMessage(BZ_SERVER, playerID, usageChInvite());
  bz_sendTextMessage(BZ_SERVER, playerID, usageChVersion());
}

void ChatChannel::handleChannelCreate(int playerID, const std::string name) {
  if (channels.find(name) != channels.end()) {
    bz_sendTextMessagef(BZ_SERVER, playerID, "Channel '%s' already exists!", name.c_str());
    return handleChannelJoin(playerID, name);
  }

  if (countChannels(playerID) >= MAXCHANNELS) {
    bz_sendTextMessagef(BZ_SERVER, playerID, "You cannot join more channels. Only %d is allowed at once.", MAXCHANNELS);
    return;
  }

  ChannelInfo	newChan(name);
  newChan.addPlayer(playerID);
  channels[name] = newChan;
  setChannel(playerID, name);
}

void ChatChannel::handleChannelJoin(int playerID, const std::string name) {
  Channels::iterator i = channels.find(name);

  if (i == channels.end()) {
    bz_sendTextMessagef(BZ_SERVER, playerID, "Channel '%s' does not exist!", name.c_str());
    return;
  }

  if (countChannels(playerID) >= MAXCHANNELS) {
    bz_sendTextMessagef(BZ_SERVER, playerID, "You cannot join more channels. Only %d is allowed at once.", MAXCHANNELS);
    return;
  }

  if (i->second.isLocked() && !i->second.isInvited(playerID)) {
    bz_BasePlayerRecord* player = bz_getPlayerByIndex(playerID);

    // Admins dont accept no as an answer
    if (!player->admin) {
      bz_sendTextMessagef(BZ_SERVER, playerID, "Channel '%s' is locked - no access!", name.c_str());
      i->second.broadcast(BZ_SERVER, bz_format("%s was rejected access.", player->callsign.c_str()));
      bz_freePlayerRecord(player);
      return;
    }
    bz_freePlayerRecord(player);
  }

  if (!i->second.isMember(playerID)) {
    i->second.addPlayer(playerID);
    setChannel(playerID, name);
  }
}

void ChatChannel::handleChannelPart(int playerID, const std::string name) {
  Channels::iterator i = channels.find(name);

  if (i == channels.end()) {
    bz_sendTextMessagef(BZ_SERVER, playerID, "Channel '%s' does not exist!", name.c_str());
    return;
  }
  i->second.removePlayer(playerID);
  setChannel(playerID);
}

void ChatChannel::handleChannelLock(int playerID, const std::string name, bool lock) {
  Channels::iterator i = channels.find(name);

  if (i == channels.end()) {
    bz_sendTextMessagef(BZ_SERVER, playerID, "Channel '%s' does not exist!", name.c_str());
    return;
  }

  if (i->second.isMember(playerID)) {
    i->second.lock(lock);

    i->second.broadcast(BZ_SERVER, bz_format("Channel is %s!", lock ? "locked" : "unlocked"));
  }
  else {
    bz_sendTextMessagef(BZ_SERVER, playerID, "You are not a member of channel '%s'", name.c_str());
  }
}

void ChatChannel::handleChannelInvite(int playerID, const std::string channel,
  const std::string name) {
  Channels::iterator i = channels.find(channel);

  if (i == channels.end()) {
    bz_sendTextMessagef(BZ_SERVER, playerID,"Channel '%s' does not exist!", channel.c_str());
    return;
  }

  if (!i->second.isMember(playerID)) {
    bz_sendTextMessagef(BZ_SERVER, playerID, "You are not a member of channel '%s'", channel.c_str());
    return;
  }

  // Find the player
  PlayerMeta::iterator	p;
  int invitePlayerID = -1;
  for (p = playerConfig.begin(); p != playerConfig.end(); p++) {
    if (p->second.name == name)
      invitePlayerID = p->first;
  }

  if (invitePlayerID == -1) {
    bz_sendTextMessagef(BZ_SERVER, playerID, "Player '%s' not found", name.c_str());
    return;
  }

  if (invitePlayerID == playerID) {
    bz_sendTextMessagef(BZ_SERVER, playerID, "Inviting yourself? I think you need more friends %s.", name.c_str());
    return;
  }

  // if invited player already is invite or already is member, silently ignore
  // the request
  if (i->second.isMember(invitePlayerID) || i->second.isInvited(invitePlayerID))
    return;

  i->second.invitePlayer(invitePlayerID);
}
