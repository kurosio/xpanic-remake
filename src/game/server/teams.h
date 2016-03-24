/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <game/teamscore.h>
#include <game/server/gamecontext.h>

class CGameTeams
{
	int m_TeamState[MAX_CLIENTS];
	int m_MembersCount[MAX_CLIENTS];

	class CGameContext * m_pGameContext;

public:
	enum
	{
		TEAMSTATE_EMPTY, TEAMSTATE_OPEN, TEAMSTATE_STARTED, TEAMSTATE_FINISHED
	};

	CTeamsCore m_Core;

	CGameTeams(CGameContext *pGameContext);

	//helper methods
	CCharacter* Character(int ClientID)
	{
		return GameServer()->GetPlayerChar(ClientID);
	}
	CPlayer* GetPlayer(int ClientID)
	{
		return GameServer()->m_apPlayers[ClientID];
	}

	class CGameContext *GameServer()
	{
		return m_pGameContext;
	}
	class IServer *Server()
	{
		return m_pGameContext->Server();
	}

	void ChangeTeamState(int Team, int State);
	void onChangeTeamState(int Team, int State, int OldState);

	int64_t TeamMask(int Team, int ExceptID = -1, int Asker = -1);

	void Reset();

	int m_LastChat[MAX_CLIENTS];

	int GetTeamState(int Team)
	{
		return m_TeamState[Team];
	}
};

#endif
