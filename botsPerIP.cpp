// botsPerIP.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include "plugin_utils.h"
#include <map>
#include <string>

int maxBotsPerIP = 2; // Maximum connections from the same IP address.

void kickCopies()
{
	bz_APIIntList *playerlist = bz_newIntList();
	bz_getPlayerIndexList(playerlist);

	for (int i = 0;i<playerlist->size();i++)
	{
		bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerlist->operator [](i));
		if (player)
		{
			int conflicts = 0;
			for (unsigned int j = 0;j<playerlist->size();j++)
			{
				bz_BasePlayerRecord *playertwo = bz_getPlayerByIndex(playerlist->operator [](j));
				if (playertwo)
				{
					if (playertwo->ipAddress == player->ipAddress)
					{
						if (conflicts == maxBotsPerIP)
						{
							bz_kickUser(playerlist->operator [](j), "Exceeded maximum connections per IP!", true);
						}
						else conflicts++;
					}
				}
				bz_freePlayerRecord(playertwo);
			}
		}
		bz_freePlayerRecord(player);
	}
	bz_deleteIntList(playerlist);
}

class botsPerIP : public bz_Plugin, public bz_CustomSlashCommandHandler
{
	virtual const char* Name() { return "botsPerIP"; }
	virtual void Init(const char* /*config*/);
	virtual void Event(bz_EventData *eventData);
	virtual void Cleanup(void);
	virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params);
public:
};

BZ_PLUGIN(botsPerIP)

void botsPerIP::Init(const char*config)
{

	//if (commandLine != "" && atoi(commandLine)) maxBotsPerIP = atoi(commandLine);
	//else bz_debugMessage(0, "botsPerIP, SYNTAX ERROR: botsPerIP,<maxBotsPerIP>");
	// Add any initialization stuff here.

	Register(bz_ePlayerJoinEvent);
	Register(bz_eSlashCommandEvent);

	bz_registerCustomSlashCommand("botsPerIP", this);
	bz_registerCustomSlashCommand("register", this);
	bz_registerCustomSlashCommand("identify", this);

	bz_debugMessage(4, "botsPerIP plugin loaded");
}

void botsPerIP::Event(bz_EventData *eventData)
{
	if (eventData->eventType == bz_ePlayerJoinEvent) //A player joins the game
		kickCopies();
}

bool botsPerIP::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params)
{
	if (command == "botsPerIP")
	{
		bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
		if (player) {
			if (player->admin) {
				if (message == "")
					bz_sendTextMessagef(playerID, playerID, "botsPerIP: %i", maxBotsPerIP);
				else if (atoi(message.c_str())) {
					maxBotsPerIP = atoi(message.c_str());
					kickCopies();
				}
				else bz_sendTextMessagef(playerID, playerID, "Cannot set botsPerIP to %s", message.c_str());

				return true;
			}
			else return false;
		}
		else return false;

		bz_freePlayerRecord(player);
	}
	else if (command == "register")
	{
		bz_sendTextMessage(playerID, playerID, "/register disabled on this server!");
		return true;
	}
	else if (command == "identify")
	{
		bz_sendTextMessage(playerID, playerID, "/identify disabled on this server!");
		return true;
	}
	else return false;
}


void botsPerIP::Cleanup(void)
{
	// Add any clean-up/shutdown stuff here.

	Flush();

	bz_removeCustomSlashCommand("botsPerIP");
	bz_removeCustomSlashCommand("register");
	bz_removeCustomSlashCommand("identify");

	bz_debugMessage(4, "botsPerIP plugin unloaded");
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
