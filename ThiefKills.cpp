// ThiefKills.cpp : 

#include "bzfsAPI.h"

class ThiefKills : public bz_Plugin
{
  virtual const char* Name() { return "ThiefKills"; }
  virtual void Init(const char* /*config*/);
  virtual void Event(bz_EventData *eventData);
  virtual void Cleanup(void);
public:
  int kilid;
};

BZ_PLUGIN(ThiefKills)

void ThiefKills::Init(const char*config)
{
  Register(bz_ePlayerDieEvent);
  Register(bz_eFlagTransferredEvent);

  bz_debugMessage(4, "THKills plugin loaded");
}

void ThiefKills::Cleanup(void)
{
  bz_debugMessage(4, "THKills plugin unloaded");
  Flush();
}

void ThiefKills::Event(bz_EventData *eventData)
{
  switch (eventData->eventType)
  {
  case bz_ePlayerDieEvent: //A player dies
  {
    bz_PlayerDieEventData_V1* diedata = (bz_PlayerDieEventData_V1*)eventData;
    if (diedata->flagKilledWith != "TH") break;
    float vector[3] = { 0, 0, 0 };
    bz_fireServerShot("SW", diedata->state.pos, vector);
  }
  break;

  case bz_eFlagTransferredEvent: //A flag is transferred (with the thief flag)
  {
    bz_FlagTransferredEventData_V1* flagtransferdata = (bz_FlagTransferredEventData_V1*)eventData;
    bz_killPlayer(flagtransferdata->fromPlayerID, true, flagtransferdata->toPlayerID, "TH");
    kilid = flagtransferdata->toPlayerID;
    bz_sendTextMessage(BZ_SERVER, flagtransferdata->fromPlayerID, "Thief KILLS!");
  }
  break;
  }
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
