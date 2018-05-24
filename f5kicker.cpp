// f5kicker.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include "plugin_utils.h"
#include <string>
#include <map>

class F5Kicker : public bz_Plugin
{
public:
	virtual const char* Name() { return "F5Kicker"; }
	virtual void Init(const char* /*config*/);
	virtual void Event(bz_EventData *eventData);
	virtual void Cleanup(void);

	virtual bool autoDelete(void) { return false; } // this will be used for more then one event
protected:

	typedef struct
	{
		int playerID;
		std::string callsign;
		double	startTime;
		double	lastUpdateTime;
		int		spreeTotal;
		int       alreadykicked;
	} trF5Kicker;

	std::map<int, trF5Kicker > playerList;
};

std::string lastkicked;//the IP of the person last kicked. Used to ban if they f5 even more
double lastkickedtime;//the time the person got kicked. so we dont remember their IP forever
int f5sbeforewarn = 4;//set defaults. F5s before warn, kick, ban

BZ_PLUGIN(F5Kicker)

void F5Kicker::Init(const char*config)
{
	bz_debugMessage(4, "f5kicker plugin loaded");

	Register(bz_ePlayerPausedEvent);
	Register(bz_ePlayerPartEvent);
	Register(bz_ePlayerJoinEvent);
}

void F5Kicker::Cleanup(void)
{
	Flush();

	bz_debugMessage(4, "f5kicker plugin unloaded");
}

void F5Kicker::Event(bz_EventData *eventData)
{
	switch (eventData->eventType)
	{
	default:
		// really WTF!!!!
		break;

	case bz_ePlayerPausedEvent:
	{

		//Get data

		bz_PlayerPausedEventData_V1	*deathRecord = (bz_PlayerPausedEventData_V1*)eventData;

		bz_BasePlayerRecord	*killerData;

		killerData = bz_getPlayerByIndex(deathRecord->playerID);

		// PlayerHistoryTracker code mostly. Just count up their F5s
		if (playerList.find(deathRecord->playerID) != playerList.end())
		{
			trF5Kicker	&record = playerList.find(deathRecord->playerID)->second;
			std::string message;
			record.spreeTotal++;//Count em up


								//CHeck timers
			if (record.lastUpdateTime<deathRecord->eventTime - 480)
			{
				record.spreeTotal = 2;//if they havent f5ed within the last 8 minutes, reset counter.
			}

			if (record.spreeTotal % 2 == 0 && record.lastUpdateTime<deathRecord->eventTime - 5)
			{
				record.spreeTotal -= 2;//If it took more than 5 seconds to resume, they must have paused instead of F5ing. Un-increment
			}
			//Send messages, kicks
			if (record.spreeTotal == f5sbeforewarn * 2 && record.alreadykicked == 0)
				message = std::string("*** SERVER WARNING *** Please do not use the F5 cheat."); //after 5 f5s, warn
			if (message.size())
			{
				bz_sendTextMessage(BZ_SERVER, deathRecord->playerID, message.c_str());//Send warning
			}
			if (record.spreeTotal == f5sbeforewarn * 4)
			{
				lastkicked = (killerData->ipAddress).c_str();//Record their IP address before kicking. So we can ban if they come back.
				lastkickedtime = deathRecord->eventTime;
				bz_kickUser(deathRecord->playerID, "*** AUTO KICK *** Using the F5 cheat is not allowed", 1);//After 10 f5's, kick
			}
			if (record.spreeTotal == f5sbeforewarn * 2 && record.alreadykicked == 1)
			{
				//Already been warned and kicked. ban. one day.
				bz_IPBanUser(deathRecord->playerID, killerData->ipAddress.c_str(), 1440, "*** AUTO BAN *** Using the F5 cheat is not allowed.");
				bz_kickUser(deathRecord->playerID, "*** AUTO BAN *** Using the F5 cheat is not allowed", 1);
			}

			//record.spreeTotal = 0;
			record.startTime = deathRecord->eventTime;
			record.lastUpdateTime = deathRecord->eventTime;
		}

		// chock up another win for our killer
		// if they weren't the same as the killer ( suicide ).

		bz_freePlayerRecord(killerData);

		//if (soundToPlay.size())
		//	bz_sendPlayCustomLocalSound(BZ_ALLUSERS,soundToPlay.c_str());

	}
	break;

	case  bz_ePlayerJoinEvent:
	{
		trF5Kicker playerRecord;

		playerRecord.playerID = ((bz_PlayerJoinPartEventData_V1*)eventData)->playerID;
		//playerRecord.callsign = ((bz_PlayerJoinPartEventData_V1*)eventData)->callsign.c_str();
		playerRecord.spreeTotal = 0;
		playerRecord.lastUpdateTime = ((bz_PlayerJoinPartEventData_V1*)eventData)->eventTime;
		playerRecord.startTime = playerRecord.lastUpdateTime;
		playerRecord.alreadykicked = 0;

		if ((((bz_getPlayerByIndex(((bz_PlayerJoinPartEventData_V1*)eventData)->playerID))->ipAddress).c_str() == lastkicked) && (lastkickedtime>playerRecord.lastUpdateTime - 86400))
		{
			//They rejoined after being kicked (if the person last kicked's IP is equal to this one, and also that it was the same day.) 
			playerRecord.alreadykicked = 1;//So that we know to ban them instead of kicking next time
			std::string message;
			message = std::string("*** SERVER WARNING *** If you continue to use the F5 cheat, you _will_ get banned."); //Warn em
			if (message.size())
			{
				bz_sendTextMessage(BZ_SERVER, playerRecord.playerID, message.c_str());//Send warning
			}
		}
		playerList[((bz_PlayerJoinPartEventData_V1*)eventData)->playerID] = playerRecord;
	}
	break;

	case  bz_ePlayerPartEvent:
	{
		std::map<int, trF5Kicker >::iterator	itr = playerList.find(((bz_PlayerJoinPartEventData_V1*)eventData)->playerID);
		if (itr != playerList.end())
			playerList.erase(itr);
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

