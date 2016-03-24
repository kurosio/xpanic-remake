/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

// this include should perhaps be removed
#include "entities/character.h"
#include "gamecontext.h"
#include "entities/cmds.h"
#include "entities/account.h"

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

	friend class CSaveTee;

public:
	CPlayer(CGameContext *pGameServer, int ClientID, int Team);
	~CPlayer();

	void Reset();
	void TryRespawn();
	void Respawn();
	CCharacter* ForceSpawn(vec2 Pos); // required for loading savegames
	void SetTeam(int Team, bool DoChatMsg=true);
	void SetZomb(int From);
	void ResetZomb();
	int GetTeam() const { return m_Team; };
	int GetCID() const { return m_ClientID; };

	void Tick();
	void PostTick();
	void Snap(int SnappingClient);
	void FakeSnap(int SnappingClient);

	void OnDirectInput(CNetObj_PlayerInput *NewInput);
	void OnPredictedInput(CNetObj_PlayerInput *NewInput);
	void OnDisconnect(const char *pReason);
	void KillCharacter(int Weapon = WEAPON_GAME);
	CCharacter *GetCharacter();
	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;
	int m_TuneZone;
	int m_TuneZoneOld;
	int m_PlayerFlags;
	int m_aActLatency[MAX_CLIENTS];
	int m_SpectatorID;
	bool m_IsReady;
	bool m_LifeActives;

	struct
	{
		int m_UserID;
		char m_Username[32];
		char m_Password[32];
		int m_PlayerState;

		int m_Level;
		int m_Exp;
		unsigned int m_Money;

		int m_Dmg;
		int m_Health;
		int m_Ammoregen;
		int m_Handle;
		int m_Ammo;

		unsigned int m_TurretMoney;
		int m_TurretLevel;
		int m_TurretExp;
		int m_TurretDmg;
		int m_TurretSpeed;
		int m_TurretAmmo;
		int m_TurretShotgun;
		int m_TurretRange;
		
		int m_Freeze;
		int m_Winner;
		int m_Luser;
	} m_AccData;

	class CCmd *m_pChatCmd;
	class CAccount *m_pAccount;

	//
	bool m_ActivesLife;
	bool m_RangeShop;
	unsigned short m_JumpsShop;
	unsigned short m_KillingSpree;
	bool m_Prefix;
	int m_Vote;
	int m_VotePos;
	//
	int m_LastVoteCall;
	int m_LastVoteTry;
	int m_LastChat;
	int m_LastSetSpectatorMode;
	int m_LastChangeInfo;
	int m_LastEmote;
	int m_LastKill;
	int m_LastCommands[4];
	int m_LastCommandPos;
	int m_LastWhisperTo;

	int m_SendVoteIndex;

	// TODO: clean this up
	struct
	{
		char m_SkinName[64];
		int m_UseCustomColor;
		int m_ColorBody;
		int m_ColorFeet;
	} m_TeeInfos;

	int m_RespawnTick;
	int m_DieTick;
	int m_Score;
	int m_ScoreStartTick;
	int m_LastActionTick;
	bool m_StolenSkin;
	struct
	{
		int m_TargetX;
		int m_TargetY;
	} m_LatestActivity;

	// network latency calculations
	struct
	{
		int m_Accum;
		int m_AccumMin;
		int m_AccumMax;
		int m_Avg;
		int m_Min;
		int m_Max;
	} m_Latency;

private:
	CCharacter *m_pCharacter;
	int m_NumInputs;
	CGameContext *m_pGameServer;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	//
	bool m_Spawning;
	int m_ClientID;
	int m_Team;


	// DDRace

public:
	int64 m_NextPauseTick;
	char m_TimeoutCode[64];

	void ProcessPause();
	bool IsPlaying();
	int64 m_Last_KickVote;
	int m_Authed;
	int m_ClientVersion;
	bool m_ShowOthers;
	bool m_ShowAll;
	bool m_FlagTrue;
	bool m_SpecTeam;
	bool m_Afk;
	int m_KillMe;

	int m_ChatScore;

	bool AfkTimer(int new_target_x, int new_target_y); //returns true if kicked
	void AfkVoteTimer(CNetObj_PlayerInput *NewTarget);
	int64 m_LastPlaytime;
	int64 m_LastEyeEmote;
	int m_LastTarget_x;
	int m_LastTarget_y;
	CNetObj_PlayerInput m_LastTarget;
	int m_Sent1stAfkWarning; // afk timer's 1st warning after 50% of sv_max_afk_time
	int m_Sent2ndAfkWarning; // afk timer's 2nd warning after 90% of sv_max_afk_time
	char m_pAfkMsg[160];
	bool m_EyeEmote;
	int m_DefEmote;
	int m_DefEmoteReset;
	bool m_FirstPacket;
};

#endif
