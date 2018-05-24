// duplicatemap.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include <string>

class duplicatemap : public bz_Plugin
{
public:
	virtual const char* Name() { return "duplicatemap"; }
	virtual void Init(const char* /*config*/);
	virtual void Event(bz_EventData *eventData);
	virtual void Cleanup(void);
};

int teamcount = 4;//set defaults, incase they send no parameters
bz_eTeamType team1 = eRedTeam;
bz_eTeamType team2 = eGreenTeam;
bz_eTeamType team3 = eBlueTeam;
bz_eTeamType team4 = ePurpleTeam;

bz_eTeamType chooseteamcolor(int color)
{
	//this function takes a number, 1 2 3 or 4 and turns it into a team color.
	switch (color)
	{
	case 1: return eRedTeam;
	case 2: return eGreenTeam;
	case 3: return eBlueTeam;
	case 4: return ePurpleTeam;
	default: return eRedTeam;
	}
}

BZ_PLUGIN(duplicatemap)

void duplicatemap::Init(const char* commandLine)
{
	bz_debugMessage(4, "duplicatemap plugin loaded");
	Register(bz_eGetAutoTeamEvent);
	if (atoi(commandLine) != 0)
	{//if there are any parameters....
	 //get arguments. five digits #####. First digit is number of players per team. The next two are the colors of the first game. The next two are the colors of the second game. So if you wanted Red vs Blue and Green vs Purple, 5vs5, you'd do -loadplugin /path/to/duplicatemap.so,51324. Defaults to 4vs4, Red vs Green and Blue vs Purple (or, 41234). If you do the parameter string wrong, you may have unexpected resules.
		char digit = commandLine[0];
		teamcount = atoi(&digit);//Get first digit, players per team.
		digit = commandLine[1];
		team1 = chooseteamcolor(atoi(&digit));//Now get the rest, team colors.
		digit = commandLine[2];
		team2 = chooseteamcolor(atoi(&digit));//Now get the rest, team colors.
		digit = commandLine[3];
		team3 = chooseteamcolor(atoi(&digit));//Now get the rest, team colors.
		digit = commandLine[4];
		team4 = chooseteamcolor(atoi(&digit));//Now get the rest, team colors.
	}
}

void duplicatemap::Cleanup(void)
{
	Flush();
	bz_debugMessage(4, "duplicatemap plugin unloaded");
}

void duplicatemap::Event(bz_EventData *eventData)
{
	bz_GetAutoTeamEventData_V1* data = (bz_GetAutoTeamEventData_V1*)eventData;

	int reds = bz_getTeamCount(team1);//# of ppl on each team. team colors are configurable, but we just use green vs red and blue vs purple  variable names to make it easier, even if that is not the "real" team color.
	int greens = bz_getTeamCount(team2);
	int blues = bz_getTeamCount(team3);
	int purples = bz_getTeamCount(team4);

	if (data->team == eRogueTeam || data->team == eRedTeam || data->team == eGreenTeam || data->team == ePurpleTeam || data->team == eBlueTeam)
	{
		if (greens<teamcount || reds<teamcount)
		{
			//green and red are not full, stick em in green or red
			if (greens>reds)data->team = team1;//Put them in the team that has less
			else if (greens<reds)data->team = team2;
			else { if (rand() % 2 == 1)data->team = team2; else data->team = team1; }//if equal, put them in a random one.
		}
		else if (blues<teamcount || purples<teamcount)
		{
			//If green and red full, but blue and purple are not, swap them to blue or purple
			if (blues>purples)data->team = team4;//Put them in the team that has less
			else if (blues<purples)data->team = team3;
			else { if (rand() % 2 == 1)data->team = team4; else data->team = team3; }//if equal, put them in a random one.
		}
		//Otherwise...they're observer
		else { data->team = eObservers; }
	}
	else
	{
		//if all full, join observer
		data->team = eObservers;
	}
	data->handled = 1;
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

