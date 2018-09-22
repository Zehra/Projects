// Wolverine.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include "plugin_utils.h"

bool invinciblePlayers[256] = {0}; //Array for storing invincibility* data (*a lot of "i"s in that word)

class Wolverine : public bz_Plugin, bz_CustomSlashCommandHandler
{
public:
	virtual const char* Name(){return "Wolverine";}
	virtual void Init ( const char* /*config*/ );
	virtual void Event(bz_EventData *eventData );
	virtual void Cleanup ( void );
	virtual bool SlashCommand ( int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params );
};

BZ_PLUGIN(Wolverine)

//Startup Functions
void Wolverine::Init (const char* /*commandLine*/)
{
	Register(bz_ePlayerSpawnEvent);
	Register(bz_ePlayerPartEvent);
	Register(bz_eFlagDroppedEvent);
	bz_registerCustomSlashCommand("invincible", this);
	bz_debugMessage(4,"Wolverine plugin loaded");
}

//Slash Command Handler
bool Wolverine::SlashCommand ( int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params )
{
  {
    //Check if player is an admin
    bz_BasePlayerRecord	*playerdata;
    playerdata = bz_getPlayerByIndex(playerID);
    if(!playerdata||!playerdata->admin){bz_freePlayerRecord(playerdata); return 0;}
    bz_freePlayerRecord(playerdata);
    
    if(command=="invincible") //If /invincible is executed
    {
      if(params->get(0)=="off" || params->get(0)=="0" || params->get(0)=="no") { //If the function is being deactivated
		  invinciblePlayers[playerID] = 0; //Erase invincibilty
		  bz_removePlayerFlag(playerID); //Remove shield flag
		  bz_sendTextMessage(BZ_SERVER, playerID, "You are no longer invincible"); //Notify
	  } else if(params->get(0)=="on" || params->get(0)=="1" || params->get(0)=="yes") { //If the function is being activated
		  invinciblePlayers[playerID] = 1; //Make invinsible
		  bz_givePlayerFlag(playerID, "SH", 1); //Give him a shield
		  bz_sendTextMessage(BZ_SERVER, playerID, "You are now invincible"); //Notify
	  } else { //If wrong syntax
		bz_sendTextMessage(BZ_SERVER,playerID,"Usage: /invincible <on|off>"); //Explain how to use
	  }
      return 1;
    }
    return 0;
  }
}

//Event handlers
void Wolverine::Event(bz_EventData *eventData )
{
	switch (eventData->eventType)
	{
		case bz_ePlayerSpawnEvent:  //Player Spawns
		{
			bz_PlayerSpawnEventData_V1* spawndata = (bz_PlayerSpawnEventData_V`*)eventData;
			if (invinciblePlayers[spawndata->playerID]==1) { //If player is invincible
				bz_givePlayerFlag(spawndata->playerID, "SH", 1); //Give player a shield flag
			}
		}
		break;

		case bz_ePlayerPartEvent: //Player leaves
		{
			bz_PlayerJoinPartEventData_V1* partdata = (bz_PlayerJoinPartEventData_V`*)eventData;
			invinciblePlayers[partdata->playerID] = 0; //Erase invincibility data 


		}
		break;

		case bz_eFlagDroppedEvent: //Flag is dropped
		{
			bz_FlagDroppedEventData_V1* flagdropdata = (bz_FlagDroppedEventData_V1*)eventData;
			
			if (invinciblePlayers[flagdropdata->playerID]==1) { //If the player is invincible
				bz_givePlayerFlag(flagdropdata->playerID, "SH", 1);
			}
		}
		break;

	}
}

//Shutdown functions
void Wolverine::Cleanup (void)
{
	bz_removeCustomSlashCommand("invincible");
	bz_debugMessage(4,"Wolverine plugin unloaded");
	Flush();
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
