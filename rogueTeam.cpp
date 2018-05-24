// rogueteam.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include <string>
#include <vector>

class rogueTeam : public bz_Plugin, public bz_CustomMapObjectHandler
{
	virtual const char* Name() { return "Rogue Team"; }
	virtual void Init(const char* /*config*/);
	virtual void Event(bz_EventData *eventData);
	virtual void Cleanup(void);
	virtual bool MapObject(bz_ApiString object, bz_CustomMapObjectInfo *data);
public:
};

int whoGrabbedRogueFlag;//sending a text message in the flag grab event doesnt work, so we save their ID for another event, then send it

float basemarkers[5][5][6];//records the postition of bases. If basemarkers[x][y][z], x is the color of the base (0 is rogue, 1 is red, 4 is purple,etc). y is so that you can have multiple bases. The first red base would be basemarkers[1][0], and the third red base (if existent) would be basemarkers[1][2]. z records the size and position of each base. xmin, xmax, ymin, ymax, zmin, zmax. So, for example, basemarkers[0][0][3] would give the ymax position of the first rogue base.
int basemarkerscount[5];//records the number of bases for each color. (the y value in basemarkers). If there were 3 green bases, basemarkerscount[2] would be 3.

void teamcapped(int teamcolor, bz_eTeamType cappedby);//caps a team (kills all players on it, sends a message saying you got capped, resets flags)

int teamtoint(bz_eTeamType color)
{
	switch (color)
	{
	case eRogueTeam: return 0;
	case eRedTeam: return 1;
	case eGreenTeam: return 2;
	case eBlueTeam: return 3;
	case ePurpleTeam: return 4;
	default: return -1;
	}
}//converts a team color to an int

bool pointIn(float pos[3], int color, int count)
{//Takes a position and base. Returns true if the pos is on the base.

	if (pos[0] > basemarkers[color][count][1] || pos[0] < basemarkers[color][count][0])
		return false;

	if (pos[1] > basemarkers[color][count][3] || pos[1] < basemarkers[color][count][2])
		return false;

	if (pos[2] > basemarkers[color][count][5] || pos[2] < basemarkers[color][count][4])
		return false;
	return true;
}

void rogueTeam::Init(const char*config)
{
	bz_debugMessage(4, "rogueteam plugin loaded");
	bz_registerCustomMapObject("basemarker", this);
	Register(bz_ePlayerDieEvent);
	Register(bz_ePlayerUpdateEvent);
	Register(bz_eFlagGrabbedEvent);
	basemarkerscount[0] = 0;basemarkerscount[1] = 0;basemarkerscount[2] = 0;basemarkerscount[3] = 0;basemarkerscount[4] = 0;//init vars
	whoGrabbedRogueFlag = -1;
}

BZ_PLUGIN(rogueTeam)

void rogueTeam::Cleanup(void)
{
	Flush();
	bz_debugMessage(4, "rogueteam plugin unloaded");
	bz_removeCustomMapObject("basemarker");
}

void rogueTeam::Event(bz_EventData *eventData)
{
	if (eventData->eventType == bz_ePlayerDieEvent)
	{
		//handles rogues TKing
		bz_PlayerDieEventData_V1 *dieData = (bz_PlayerDieEventData_V1*)eventData;

		if (dieData->team == eRogueTeam&&dieData->killerTeam == eRogueTeam)
		{
			if (dieData->killerID == dieData->playerID) return;//If they killed themselves, don't send a tk message
															   //they TKed a rogue. Kill.
			bz_killPlayer(dieData->killerID, 0, BZ_SERVER);
			bz_sendTextMessage(BZ_SERVER, dieData->killerID, "Do not kill your rogue teammates!");
			dieData->killerID = BZ_SERVER;//so they dont get a point for the kill
		}
	}
	else if (eventData->eventType == bz_eFlagGrabbedEvent)
	{
		bz_FlagGrabbedEventData_V1	*info = (bz_FlagGrabbedEventData_V1*)eventData;
		std::string flag = info->flagType;//if they grab US (the rogue team flag), send a message telling them they grabbed rogue flag
		if (flag == "US") { whoGrabbedRogueFlag = info->playerID; }//sending a text message in the flag grab event doesnt work, so we save their ID for another event, then send it
	}
	else if (eventData->eventType == bz_ePlayerUpdateEvent)
	{

		//this checks if a rogue is in a base, or if a tank has a rogue flag and is on their base

		float pos[3] = { 0 };

		int playerID = -1;

		switch (eventData->eventType)
		{//get data
		/*case bz_ePlayerUpdateEvent:
			pos[0] = ((bz_PlayerUpdateEventData*)eventData)->pos[0];
			pos[1] = ((bz_PlayerUpdateEventData*)eventData)->pos[1];
			pos[2] = ((bz_PlayerUpdateEventData*)eventData)->pos[2];
			playerID = ((bz_PlayerUpdateEventData*)eventData)->playerID;
			break;*/

			case bz_ePlayerUpdateEvent: // If A player sends a position update
		{
			// Get playerID
			playerID = ((bz_PlayerUpdateEventData_V1*)eventData)->playerID;
			// Get player data
			bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
			// If the player is spawned, send the data
			// We dont want users sending data while they are dead
			if ( player->spawned ){
				pos[0] = player->lastKnownState.pos[0];
				pos[1] = player->lastKnownState.pos[1];
				pos[2] = player->lastKnownState.pos[2];
			}
			// Clear the record
			bz_freePlayerRecord(player);
		}
			break;

		default:
			return;
		}//get position and playerID

		if (whoGrabbedRogueFlag != -1)
		{
			bz_sendTextMessage(BZ_SERVER, whoGrabbedRogueFlag, "Rogue Team Flag");
			whoGrabbedRogueFlag = -1;//sending a text message in the flag grab event doesnt work, so we save their ID for another event, then send it
		}

		bz_BasePlayerRecord	*updateData;//get the player record
		updateData = bz_getPlayerByIndex(playerID);//use it to get their team color
		bz_eTeamType teamcolor = updateData->team;
		std::string flagAbrev = (updateData->currentFlag).c_str();//and also their flag
		bz_freePlayerRecord(updateData);//free the player record

		int flagcolor;
		//get the flag that they are holding

		if (flagAbrev == "USeless (+US)") flagcolor = 0;
		else if (flagAbrev == "Red team flag") flagcolor = 1;
		else if (flagAbrev == "Green team flag") flagcolor = 2;
		else if (flagAbrev == "Blue team flag") flagcolor = 3;
		else if (flagAbrev == "Purple team flag") flagcolor = 4;
		else { return; }//change flag name into int.


						//if on rogue team, check if the rogue is on its base with another team flag
						//if on rogue team, also check if rogue is on another team base with rogue flag.
						//if not on rogue team, check if they are on their base with the rogue flag


		if (teamcolor == eRogueTeam)
		{
			//rogue ...
			if (flagcolor>0)
			{
				//they have a team flag. Check if they are in their base
				for (int i = 0;i<basemarkerscount[0];i++)
				{
					if (pointIn(pos, 0, i))
					{
						//rogue capped a flag! kill the capped team.
						bz_removePlayerFlag(playerID);
						teamcapped(flagcolor, teamcolor);
					}
				}
			}
			//also check for them capping their own flag.
			else if (flagcolor == 0)
			{
				for (int i = 1; i<5; i++)
				{//loop through each color of other base.
					for (int ii = 0; ii<basemarkerscount[i]; ii++)
					{//loop through each of those bases
						if (pointIn(pos, i, ii))
						{
							//they are on a different base than their own, with their flag! they capped their own flag.
							bz_removePlayerFlag(playerID);
							teamcapped(flagcolor, teamcolor);
						}
					}
				}
			}
		}
		else if (flagcolor == 0 && (teamcolor == eRedTeam || teamcolor == eGreenTeam || teamcolor == eBlueTeam || teamcolor == ePurpleTeam))
		{
			//it is a different team than rogue, with the rogue flag. Check if they are on their own base, if so, cap rogue.
			for (int i = 0; i<basemarkerscount[teamtoint(teamcolor)]; i++)
			{
				if (pointIn(pos, teamtoint(teamcolor), i))
				{
					//they capped rogue!
					bz_removePlayerFlag(playerID);
					teamcapped(flagcolor, teamcolor);
				}
			}
		}


	}

}

bool rogueTeam::MapObject(bz_ApiString object, bz_CustomMapObjectInfo *data)
{
	if (object != "BASEMARKER" || !data)
		return false;

	//this takes a basemarker map object and stores it in basemarkers

	float x = 0, y = 0, z = 0, xsize = 0, ysize = 0, zsize = 0;
	int color = 0;//define temp variables.

				  // parse all the chunks from the map object
	for (unsigned int i = 0; i < data->data.size(); i++)
	{
		std::string line = data->data.get(i).c_str();

		bz_APIStringList *nubs = bz_newStringList();
		nubs->tokenize(line.c_str(), " ", 0, true);

		if (nubs->size() > 0)
		{
			std::string key = bz_toupper(nubs->get(0).c_str());
			if (key == "POSITION" && nubs->size() > 3)
			{
				x = (float)atof(nubs->get(1).c_str());
				y = (float)atof(nubs->get(2).c_str());
				z = (float)atof(nubs->get(3).c_str());
			}
			else if (key == "SIZE" && nubs->size() > 3)
			{
				xsize = (float)atof(nubs->get(1).c_str());
				ysize = (float)atof(nubs->get(2).c_str());
				zsize = (float)atof(nubs->get(3).c_str());
			}
			else if (key == "COLOR" && nubs->size() > 1)
			{
				color = (int)atoi(nubs->get(1).c_str());
			}

		}
		bz_deleteStringList(nubs);
	}
	//after getting all of them, convert to xmin, xmax etc and put in the correct spot in basemarkers
	basemarkers[color][basemarkerscount[color]][0] = x - xsize;
	basemarkers[color][basemarkerscount[color]][1] = x + xsize;
	basemarkers[color][basemarkerscount[color]][2] = y - ysize;
	basemarkers[color][basemarkerscount[color]][3] = y + ysize;
	basemarkers[color][basemarkerscount[color]][4] = z;
	basemarkers[color][basemarkerscount[color]][5] = z + zsize + 1;
	basemarkerscount[color] += 1;//increase the number of bases, so we know where to store it next time
	return true;
}

void teamcapped(int teamcolor, bz_eTeamType cappedby)
{
	//called when a team is capped. Kills everyone on the team and sends a message to them.
	//first make the death message
	std::string message;
	switch (cappedby)
	{
	case eRogueTeam: message = std::string("Your team flag was captured by the Rogue Team");  break;
	case eRedTeam: message = std::string("Your team flag was captured by the Red Team");  break;
	case eGreenTeam: message = std::string("Your team flag was captured by the Green Team");  break;
	case eBlueTeam: message = std::string("Your team flag was captured by the Blue Team");  break;
	case ePurpleTeam: message = std::string("Your team flag was captured by the Purple Team");  break;
	default: return;
	}

	bz_APIIntList* playerlist = bz_newIntList();
	bz_getPlayerIndexList(playerlist);//get list of all players
	for (unsigned int i = 0; i<playerlist->size(); i++)
	{//loop through them
		bz_BasePlayerRecord	*updateData;//get the player record
		updateData = bz_getPlayerByIndex(playerlist->get(i));//use it to get their team color
		if ((teamtoint(updateData->team) == teamcolor))
		{
			//they're on the kill team... Kill them
			bz_killPlayer(playerlist->get(i), 0, BZ_SERVER);
			bz_sendTextMessage(BZ_SERVER, playerlist->get(i), message.c_str());
		}
		bz_freePlayerRecord(updateData);//free the player record
	}
	bz_deleteIntList(playerlist);
	bz_resetFlags(1);//reset team flags
}


// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8*/
