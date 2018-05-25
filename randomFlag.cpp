// randomflag.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include "plugin_utils.h"
#include <string>

class randomFlag : public bz_Plugin
{
public:
  virtual const char* Name() { return "RandomFlag"; }
  virtual void Init(const char* /*config*/);
  virtual void Event(bz_EventData *eventData);
  virtual void Cleanup(void);
};

BZ_PLUGIN(randomFlag)

void randomFlag::Init(const char*config)
{
  bz_debugMessage(4, "randomflag plugin loaded");
  Register(bz_ePlayerDieEvent);
}

void randomFlag::Cleanup(void)
{
  bz_debugMessage(4, "randomflag plugin unloaded");
  Flush();
}

void randomFlag::Event(bz_EventData *eventData)
{
  if (eventData->eventType != bz_ePlayerDieEvent)
    return;

  bz_PlayerDieEventData_V1 *dieData = (bz_PlayerDieEventData_V1*)eventData;

  if (dieData->killerID == BZ_SERVER) return; //quit if the server killed em... we cant give a flag to the server

  int x = rand() % 38;
  switch (x)
  {
    //pick a random flag and give it to the killer
  case 0: bz_givePlayerFlag(dieData->killerID, "A", true); break;
  case 1: bz_givePlayerFlag(dieData->killerID, "BU", true); break;
  case 2: bz_givePlayerFlag(dieData->killerID, "CL", true); break;
  case 3: bz_givePlayerFlag(dieData->killerID, "G", true); break;
  case 4: bz_givePlayerFlag(dieData->killerID, "GM", true); break;
  case 5: bz_givePlayerFlag(dieData->killerID, "IB", true); break;
  case 6: bz_givePlayerFlag(dieData->killerID, "L", true); break;
  case 7: bz_givePlayerFlag(dieData->killerID, "MG", true); break;
  case 8: bz_givePlayerFlag(dieData->killerID, "MQ", true); break;
  case 9: bz_givePlayerFlag(dieData->killerID, "N", true); break;
  case 10: bz_givePlayerFlag(dieData->killerID, "OO", true); break;
  case 11: bz_givePlayerFlag(dieData->killerID, "PZ", true); break;
  case 12: bz_givePlayerFlag(dieData->killerID, "QT", true); break;
  case 13: bz_givePlayerFlag(dieData->killerID, "SE", true); break;
  case 14: bz_givePlayerFlag(dieData->killerID, "SH", true); break;
  case 15: bz_givePlayerFlag(dieData->killerID, "SW", true); break;
  case 16: bz_givePlayerFlag(dieData->killerID, "ST", true); break;
  case 17: bz_givePlayerFlag(dieData->killerID, "SR", true); break;
  case 18: bz_givePlayerFlag(dieData->killerID, "SB", true); break;
  case 19: bz_givePlayerFlag(dieData->killerID, "TH", true); break;
  case 20: bz_givePlayerFlag(dieData->killerID, "T", true); break;
  case 21: bz_givePlayerFlag(dieData->killerID, "US", true); break;
  case 22: bz_givePlayerFlag(dieData->killerID, "V", true); break;
  case 23: bz_givePlayerFlag(dieData->killerID, "WG", true); break;
  case 24: bz_givePlayerFlag(dieData->killerID, "B", true); break;
  case 25: bz_givePlayerFlag(dieData->killerID, "BY", true); break;
  case 26: bz_givePlayerFlag(dieData->killerID, "CB", true); break;
  case 27: bz_givePlayerFlag(dieData->killerID, "FO", true); break;
  case 28: bz_givePlayerFlag(dieData->killerID, "JM", true); break;
  case 29: bz_givePlayerFlag(dieData->killerID, "LT", true); break;
  case 30: bz_givePlayerFlag(dieData->killerID, "M", true); break;
  case 31: bz_givePlayerFlag(dieData->killerID, "NJ", true); break;
  case 32: bz_givePlayerFlag(dieData->killerID, "O", true); break;
  case 33: bz_givePlayerFlag(dieData->killerID, "RC", true); break;
  case 34: bz_givePlayerFlag(dieData->killerID, "RO", true); break;
  case 35: bz_givePlayerFlag(dieData->killerID, "RT", true); break;
  case 36: bz_givePlayerFlag(dieData->killerID, "TR", true); break;
  case 37: bz_givePlayerFlag(dieData->killerID, "WA", true); break;
  }

}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
