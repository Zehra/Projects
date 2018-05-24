// flagOfTheDay

#include "bzfsAPI.h"
#include "plugin_utils.h"
#include <math.h>
#include <time.h>
#include <fstream>
#include <string>
#include <list>

class flagOfTheDay : public bz_Plugin, bz_CustomSlashCommandHandler
{
	virtual const char* Name() { return "Plugin flagOfTheDay"; }
	virtual void Init(const char* /*config*/);
	virtual void Event(bz_EventData *eventData);
	virtual void Cleanup(void);
	virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params);
public:

};

BZ_PLUGIN(flagOfTheDay)

void flagOfTheDay::Init(const char*config)
{
	bz_debugMessage(4, "flagOfTheDay plugin loaded");
	bz_registerCustomSlashCommand ( "flagoftheday", this );
	bz_registerCustomSlashCommand ( "changeflagoftheday", this );
}

void flagOfTheDay::Cleanup(void)
{
	bz_removeCustomSlashCommand ( "flagoftheday" );
	bz_removeCustomSlashCommand ( "changeflagoftheday" );
	bz_debugMessage(4, "flagOfTheDay plugin unloaded");
	Flush();
}

bool flagOfTheDay::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params)
{
	std::ifstream confStream("flagOfDay.conf");
	if (!confStream.fail())
	{
	std::string line;

	std::getline(confStream, line);

	if (command == "flagoftheday")
	{
		bz_givePlayerFlag(playerID, line.c_str(), true);
		std::string msg("");
		msg += " You were given the flag of the day, which is ";
		msg += line;
		msg += ".";

		bz_sendTextMessage(BZ_SERVER, playerID, msg.c_str());
	}
}
 if (command == "changeflagoftheday")
{
	if (!bz_hasPerm(playerID, "CHFLAG"))
		{
			bz_sendTextMessage(BZ_SERVER, playerID, "You don't have permission to change the flag of the day.");
		}
        else if (params->size() == 1)
		{
			std::string msg("Flag of the Day has been changed to ");
			msg += params->get(0).c_str();
			msg += ".";
			bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, msg.c_str());
			std::ofstream oputfStream("flagOfDay.conf");
			oputfStream << params->get(0).c_str();
			oputfStream.close();
		}
		else
		{
			bz_sendTextMessage(BZ_SERVER, playerID, "Usage:  /changeflagoftheday <flagType>");
		}
		return true;
	}
	return false;
}

void flagOfTheDay::Event(bz_EventData *eventData)
{
	if (eventData->eventType == bz_eSlashCommandEvent) // This event is called each time a player sends a slash command
	{
		bz_SlashCommandEventData_V1* slashCommandData = (bz_SlashCommandEventData_V1*)eventData;
	}
}
