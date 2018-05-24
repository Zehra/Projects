#include "bzfsAPI.h"

class noFlags : public bz_Plugin
{
public:
	virtual const char* Name() { return "No Flags"; }
	virtual void Init(const char* config);
	virtual void Event(bz_EventData *eventData);
	virtual void Cleanup();
};

BZ_PLUGIN(noFlags);

int teamcount = 4;

void noFlags::Init(const char* confFile)
{
	Register(bz_eAllowFlagGrab);
	bz_debugMessage(0, "No Flags Plug-in: Loaded");
}

void noFlags::Cleanup(void)
{
	Flush();
	bz_debugMessage(0, "No Flags Plug-in: Unloaded");
}

void noFlags::Event(bz_EventData *eventData)
{
  if (eventData->eventType == bz_eAllowFlagGrab)
	{
		bz_AllowFlagGrabData_V1 *flags = (bz_AllowFlagGrabData_V1*)eventData;
		if (bz_getTeamCount(eRogueTeam) + bz_getTeamCount(eRedTeam) + bz_getTeamCount(eGreenTeam) + bz_getTeamCount(eBlueTeam) + bz_getTeamCount(ePurpleTeam)<4)
		{
			flags->allow = false;
		}
	}
	return;
}
