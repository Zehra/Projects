//============================================================================//
//
//  GEOIP.CPP
//
//  author:  trepan
//  brief:   bzfs plugin to display player countries
//  version: 1.0
//  date:    Sep 19, 2010
//  license: LGPL v2
//  note:    database files can be found at:  http://software77.net/geo-ip/
//  example: bzfs -loadplugin geoip,/usr/share/data/my-geoip.csv
//
//============================================================================//

#include "bzfsAPI.h"
#include "plugin_utils.h"

#include <stdio.h>
#include <stdint.h>

#ifndef _WIN32 // use inet_aton()
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#else // use inet_addr()
#  include <windows.h>
#  include <winsock2.h>
#endif

#include <string>
#include <vector>
#include <map>
using std::string;
using std::vector;
using std::map;

#include <algorithm>

BZ_PLUGIN(geoIP)

//============================================================================//

class geoIP : public bz_Plugin, bz_CustomSlashCommandHandler
{
  virtual const char* Name() { return "geoIP"; }
  virtual void Init(const char* /*config*/);
  virtual void Event(bz_EventData *eventData);
  virtual void Cleanup(void);
  virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params);
public:
  void SendPlayerCountryList(int playerID);
};

//============================================================================//

static const char* cmdName = "geoip";

static const char* defFile = "geoip.csv";

static std::string csvFile = defFile;


//============================================================================//

typedef uint32_t u32;


struct Location {
  string ctry;
  string cntry;
  string country;
};

typedef map<string, Location> LocationMap; // keyed to ctry

static LocationMap locations;


struct Range {
  u32 startIP;
  u32 endIP;
  string locID;
};

typedef map<u32, Range> RangeMap;

static RangeMap ranges;


//============================================================================//

static bool LoadDatabase(const char* filename) {
  FILE* f = fopen(filename, "r");
  if (f == NULL) {
    return false;
  }

  ranges.clear();
  locations.clear();

  // "0","16777215","iana","410227200","ZZ","ZZZ","Reserved"
  u32 startIP;
  u32 endIP;
  u32 assigned;
  char ctry[16];
  char cntry[16];
  char country[256];
  char registry[16];

#define _    ","
#define U    "\"%u\""
#define S16  "\"%16[^\"]\""
#define S256 "\"%256[^\"]\""
  const char* FMT = U _ U _ S16 _ U _ S16 _ S16 _ S256;
#undef  S256
#undef  S16
#undef  U
#undef  _

  char buf[1024];
  const size_t bufSize = sizeof(buf);
  while (fgets(buf, bufSize, f) != NULL) {
    if (sscanf(buf, FMT, &startIP, &endIP, registry,
      &assigned, ctry, cntry, country) == 7) {
      Location loc;
      loc.ctry = ctry;
      loc.cntry = cntry;
      loc.country = country;
      locations[ctry] = loc;

      Range range;
      range.startIP = startIP;
      range.endIP = endIP;
      range.locID = ctry;
      ranges[startIP] = range;
    }
  }

  fclose(f);

  return true;
}

//============================================================================//

static const Location* FindLocation(u32 ip) {
  RangeMap::const_iterator it = ranges.upper_bound(ip);
  if (it == ranges.begin()) {
    return NULL;
  }
  it--;

  const Range& range = it->second;
  if ((ip < range.startIP) || (ip > range.endIP)) {
    return NULL;
  }

  LocationMap::const_iterator locIt = locations.find(range.locID);
  if (locIt == locations.end()) {
    return NULL;
  }
  return &(locIt->second);
};


#ifndef _WIN32
static const Location* FindLocation(const char* dotAddr) {
  struct in_addr addr;
  if (!inet_aton(dotAddr, &addr)) {
    return NULL;
  }
  return FindLocation(ntohl(addr.s_addr));
};
#else
static const Location* FindLocation(const char* dotAddr) {
  const unsigned long addr = inet_addr(dotAddr);
  if (addr == INADDR_NONE) {
    return NULL;
  }
  return FindLocation(ntohl(addr));
};
#endif // _WIN32


//============================================================================//

static const Location* GetPlayerLocation(int playerID, string& callsign) {
  bz_BasePlayerRecord* pRec = bz_getPlayerByIndex(playerID);
  if (pRec == NULL) {
    return NULL;
  }

  callsign = pRec->callsign.c_str();

  const Location* loc = FindLocation(pRec->ipAddress.c_str());

  bz_freePlayerRecord(pRec);
  return loc;
}


//============================================================================//

bool geoIP::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params)
{
  if (command != cmdName) {
    return false;
  }

  if (params->size() >= 1) {
    const string arg = params->get(0).c_str();
    if (arg == "reload") {
      if (!bz_hasPerm(playerID, "setAll")) {
        bz_sendTextMessage(BZ_SERVER, playerID, "no permission");
        return true;
      }
      if (LoadDatabase(csvFile.c_str())) {
        bz_sendTextMessage(BZ_SERVER, playerID, "geoip reload succeeded");
      }
      else {
        bz_sendTextMessage(BZ_SERVER, playerID, "geoip reload failed");
      }
      return true;
    }

    const Location* loc = FindLocation(arg.c_str());
    if (!loc) {
      bz_sendTextMessage(BZ_SERVER, playerID, "geoip: unknown");
    }
    else {
      bz_sendTextMessagef(BZ_SERVER, playerID, "geoip: <%s> <%s> %s",
        loc->ctry.c_str(),
        loc->cntry.c_str(),
        loc->country.c_str());
    }
  }
  else { // list all player locations
    SendPlayerCountryList(playerID);
  }

  return true;
}

//============================================================================//

struct LocationEntry {
  LocationEntry() {}
  LocationEntry(const std::string& cs, const string& ctry)
    : callsign(cs)
    , country(ctry)
  {}
  bool operator<(const LocationEntry& e) const {
    if (country  < e.country) { return true; }
    if (e.country  <   country) { return false; }
    if (callsign < e.callsign) { return true; }
    if (e.callsign <   callsign) { return false; }
    return false;
  }
  string callsign;
  string country;
};


void geoIP::SendPlayerCountryList(int playerID) {
  bz_APIIntList* idList = bz_newIntList();
  if (!bz_getPlayerIndexList(idList)) {
    bz_deleteIntList(idList);
    return;
  }

  vector<LocationEntry> entries;

  size_t maxLen = 0;

  for (unsigned int i = 0; i < idList->size(); i++) {
    LocationEntry entry;
    const Location* loc = GetPlayerLocation(idList->get(i), entry.callsign);
    entry.country = loc ? loc->country : "-Unknown-";
    entries.push_back(entry);
    if (maxLen < entry.callsign.size()) {
      maxLen = entry.callsign.size();
    }
  }

  bz_deleteIntList(idList);

  std::sort(entries.begin(), entries.end());

  bz_sendTextMessage(BZ_SERVER, playerID, "Player Locations");
  for (size_t e = 0; e < entries.size(); e++) {
    const LocationEntry& entry = entries[e];
    bz_sendTextMessagef(BZ_SERVER, playerID, "%-*s  : %s", maxLen,
      entry.callsign.c_str(), entry.country.c_str());
  }
}


//============================================================================//

void geoIP::Event(bz_EventData *eventData) {
  bz_PlayerJoinPartEventData_V1* e = (bz_PlayerJoinPartEventData_V1*)eventData;
  const Location* loc = FindLocation(e->record->ipAddress.c_str());
  bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has joined from %s", e->record->callsign.c_str(), loc ? loc->country.c_str() : "Unknown");
}

//============================================================================//

void geoIP::Init(const char*opts)
{
  if (strlen(opts) > 0) {
    csvFile = opts;
  }
  if (!LoadDatabase(csvFile.c_str())) {
    bz_debugMessagef(0, "geoip database failed to load: %s", csvFile.c_str());
  }
  bz_debugMessage(4, "geoip plugin loaded");
  bz_registerCustomSlashCommand(cmdName, this);
  Register(bz_ePlayerJoinEvent);
}

void geoIP::Cleanup(void)
{
  bz_debugMessage(4, "geoip plugin unloaded");
  bz_removeCustomSlashCommand(cmdName);
  Flush();
}

//============================================================================//

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
