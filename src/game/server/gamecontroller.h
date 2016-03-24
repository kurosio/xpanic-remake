/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

#include <base/vmath.h>

class CDoor;
#ifdef _MSC_VER
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

class IGameController
{
	vec2 m_aaSpawnPoints[3][64];
	int m_aNumSpawnPoints[3];

	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

protected:
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const { return m_pServer; }

	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_FriendlyTeam = -1;
			m_Pos = vec2(100,100);
		}

		vec2 m_Pos;
		bool m_Got;
		int m_FriendlyTeam;
		float m_Score;
	};

	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos);
	void EvaluateSpawnType(CSpawnEval *pEval, int Type);
	bool EvaluateSpawn(class CPlayer *pP, vec2 *pPos);

	void ResetGame();

	char m_aMapWish[128];
	int m_RoundStartTick;
	int m_GameOverTick;
	
	int m_LastZomb, m_LastZomb2;		
	int m_GameFlags; 
	int m_SuddenDeath; 
	int m_RoundCount; 
	int m_aTeamscore[2];

public:
	const char *m_pGameType;
	char m_aaOnTeamWinEvent[3][512];

	bool IsTeamplay() const;
	int m_Warmup;
	unsigned int m_GrenadeLimit;
	bool m_RoundStarted;
	
	struct CDoor
	{
		int m_State;
		int64 m_Tick;
		int m_OpenTime;
		int m_CloseTime;
		int m_ReopenTime;
	} m_Door[48];
	
	IGameController(class CGameContext *pGameServer);
	virtual ~IGameController();

	virtual void DoWincheck();
	void DoWarmup(int Seconds);

	void StartRound();
	void EndRound();
	void ChangeMap(const char *pToMap);

	bool IsFriendlyFire(int ClientID1, int ClientID2);

	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number = 0);
	
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	virtual bool CanSpawn(int Team, vec2 *pPos);
	virtual void OnPlayerInfoChange(class CPlayer *pP);

	virtual const char *GetTeamName(int Team);
	virtual int GetAutoTeam(int NotThisID);
	virtual bool CanJoinTeam(int Team, int NotThisID);
	int ClampTeam(int Team);
	virtual void PostReset();

	virtual void ResetZ();
	virtual bool ZombStarted();
	virtual void StartZomb(bool Value), CheckZomb();
	virtual int NumPlayers(), NumZombs(), NumHumans();
	void RandomZomb(int Mode);

	virtual void OnHoldpoint(int Index);
	virtual void OnZHoldpoint(int Index);
	virtual void OnZStop(int Index);
	virtual void ResetDoors();
	virtual int DoorState(int Index);
	virtual void SetDoorState(int Index, int State);
	virtual int GetDoorTime(int Index);
};

#endif
