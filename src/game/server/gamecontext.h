/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/server.h>
#include <engine/console.h>
#include <engine/shared/memheap.h>

#include <game/layers.h>
#include <game/voting.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "player.h"

#ifdef _MSC_VER
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

enum
{
	NUM_TUNINGZONES = 256
};

class CGameContext : public IGameServer
{
	IServer *m_pServer;
	class IConsole *m_pConsole;
	CLayers m_Layers;
	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;
	CTuningParams m_TuningList[NUM_TUNINGZONES];

	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDump(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneZone(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDumpZone(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneResetZone(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneSetZoneMsgEnter(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneSetZoneMsgLeave(IConsole::IResult *pResult, void *pUserData);
	static void ConPause(IConsole::IResult *pResult, void *pUserData);
	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData);
	static void ConForceVote(IConsole::IResult *pResult, void *pUserData);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	CGameContext(int Resetting);
	void Construct(int Resetting);

	bool m_Resetting;
public:
	IServer *Server() const { return m_pServer; }
	class IConsole *Console() { return m_pConsole; }
	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }
	CTuningParams *TuningList() { return &m_TuningList[0]; }

	CGameContext();
	~CGameContext();

	void Clear();

	CEventHandler m_Events;
	CPlayer *m_apPlayers[MAX_CLIENTS];

	IGameController *m_pController;
	CGameWorld m_World;

	// helper functions
	class CCharacter *GetPlayerChar(int ClientID);

	//int m_LockTeams;

	// voting
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason);
	void EndVote();
	void SendVoteSet(int ClientID);
	void SendVoteStatus(int ClientID, int Total, int Yes, int No);
	void AbortVoteKickOnDisconnect(int ClientID);
	void SendPM(int ClientID, int FromID, char *string);

	int m_VoteCreator;
	int64 m_VoteCloseTime;
	bool m_VoteUpdate;
	int m_VotePos;
	char m_aVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_NumVoteOptions;
	int m_VoteEnforce;
	char m_ZoneEnterMsg[NUM_TUNINGZONES][256]; // 0 is used for switching from or to area without tunings
	char m_ZoneLeaveMsg[NUM_TUNINGZONES][256];

	char m_aDeleteTempfile[128];
	void DeleteTempfile();

	enum
	{
		VOTE_ENFORCE_UNKNOWN=0,
		VOTE_ENFORCE_NO,
		VOTE_ENFORCE_YES,
	};
	CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;

	// helper functions
	void CreateDamageInd(vec2 Pos, float AngleMod, int Amount, int64_t Mask=-1);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int64_t Mask);
	void CreateHammerHit(vec2 Pos, int64_t Mask=-1);
	void CreatePlayerSpawn(vec2 Pos, int64_t Mask=-1);
	void CreateDeath(vec2 Pos, int Who, int64_t Mask=-1);
	void CreateSound(vec2 Pos, int Sound, int64_t Mask=-1);
	void CreateSoundGlobal(int Sound, int Target=-1);


	enum
	{
		CHAT_ALL=-2,
		CHAT_SPEC=-1,
		CHAT_RED=0,
		CHAT_BLUE=1,
		CHAT_WHISPER_SEND=2,
		CHAT_WHISPER_RECV=3
	};

	// network
	void CallVote(int ClientID, const char *aDesc, const char *aCmd, const char *pReason, const char *aChatmsg);
	void SendChatTarget(int To, const char *pText);
	void SendChatTeam(int Team, const char *pText);
	void SendChat(int ClientID, int Team, const char *pText, int SpamProtectionClientID = -1);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendBroadcast(const char *pText, int ClientID);
	void SendTuningParams(int ClientID, int Zone = 0);

	struct CVoteOptionServer *GetVoteOption(int Index);
	void ProgressVoteOptions(int ClientID);

	//
	//void SwapTeams();

	void LoadMapSettings();

	// engine events
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnMapChange(char *pNewMapName, int MapNameSize);
	virtual void OnShutdown();

	virtual void OnTick();
	virtual void OnPreSnap();
	virtual void OnSnap(int ClientID);
	virtual void OnPostSnap();

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID);

	virtual void OnClientConnected(int ClientID);
	virtual void OnClientEnter(int ClientID);
	virtual void OnClientDrop(int ClientID, const char *pReason);
	virtual void OnClientDirectInput(int ClientID, void *pInput);
	virtual void OnClientPredictedInput(int ClientID, void *pInput);

	virtual bool IsClientReady(int ClientID);
	virtual bool IsClientPlayer(int ClientID);

	virtual const char *GameType();
	virtual const char *Version();
	virtual const char *NetVersion();

	// DDRace

	int ProcessSpamProtection(int ClientID);
	int GetDDRaceTeam(int ClientID);
	int64 m_LastMapVote;
	int m_aaExtIDs[MAX_CLIENTS];

private:

	bool m_VoteWillPass;
	//DDRace Console Commands

	//static void ConMute(IConsole::IResult *pResult, void *pUserData);
	//static void ConUnmute(IConsole::IResult *pResult, void *pUserData);
	static void ConKillPlayer(IConsole::IResult *pResult, void *pUserData);

	static void ConShotgun(IConsole::IResult *pResult, void *pUserData);
	static void ConGrenade(IConsole::IResult *pResult, void *pUserData);
	static void ConRifle(IConsole::IResult *pResult, void *pUserData);
	static void ConWeapons(IConsole::IResult *pResult, void *pUserData);
	static void ConUnShotgun(IConsole::IResult *pResult, void *pUserData);
	static void ConUnGrenade(IConsole::IResult *pResult, void *pUserData);
	static void ConUnRifle(IConsole::IResult *pResult, void *pUserData);
	static void ConUnWeapons(IConsole::IResult *pResult, void *pUserData);
	static void ConAddWeapon(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveWeapon(IConsole::IResult *pResult, void *pUserData);

	void ModifyWeapons(IConsole::IResult *pResult, void *pUserData, int Weapon, bool Remove);
	void MoveCharacter(int ClientID, int X, int Y, bool Raw = false);
	static void ConGoLeft(IConsole::IResult *pResult, void *pUserData);
	static void ConGoRight(IConsole::IResult *pResult, void *pUserData);
	static void ConGoUp(IConsole::IResult *pResult, void *pUserData);
	static void ConGoDown(IConsole::IResult *pResult, void *pUserData);
	static void ConMove(IConsole::IResult *pResult, void *pUserData);
	static void ConMoveRaw(IConsole::IResult *pResult, void *pUserData);

	static void ConToTeleporter(IConsole::IResult *pResult, void *pUserData);
	static void ConToCheckTeleporter(IConsole::IResult *pResult, void *pUserData);
	static void ConTeleport(IConsole::IResult *pResult, void *pUserData);

	static void ConInfo(IConsole::IResult *pResult, void *pUserData);
	static void ConHelp(IConsole::IResult *pResult, void *pUserData);
	static void ConRules(IConsole::IResult *pResult, void *pUserData);
	static void ConKill(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleSpec(IConsole::IResult *pResult, void *pUserData);

	static void ConMute(IConsole::IResult *pResult, void *pUserData);
	static void ConMuteID(IConsole::IResult *pResult, void *pUserData);
	static void ConMuteIP(IConsole::IResult *pResult, void *pUserData);
	static void ConUnmute(IConsole::IResult *pResult, void *pUserData);
	static void ConMutes(IConsole::IResult *pResult, void *pUserData);

	static void ConList(IConsole::IResult *pResult, void *pUserData);

	enum
	{
		MAX_MUTES=32,
	};
	struct CMute
	{
		NETADDR m_Addr;
		int m_Expire;
	};

	CMute m_aMutes[MAX_MUTES];
	int m_NumMutes;
	void Mute(IConsole::IResult *pResult, NETADDR *Addr, int Secs, const char *pDisplayName);

public:
	CLayers *Layers() { return &m_Layers; }
	bool m_VoteKick;
	bool m_VoteSpec;
	enum
	{
		VOTE_ENFORCE_NO_ADMIN = VOTE_ENFORCE_YES + 1,
		VOTE_ENFORCE_YES_ADMIN
	};
	int m_VoteEnforcer;
	void SendRecord(int ClientID);
	static void SendChatResponse(const char *pLine, void *pUser, bool Highlighted = false);
	static void SendChatResponseAll(const char *pLine, void *pUser);
	virtual void OnSetAuthed(int ClientID,int Level);
	virtual bool PlayerCollision();
	virtual bool PlayerHooking();
	virtual float PlayerJetpack();

	void ResetTuning();

	int m_ChatResponseTargetID;
	int m_ChatPrintCBIndex;
};

inline int64_t CmaskAll() { return -1LL; }
inline int64_t CmaskOne(int ClientID) { return 1LL<<ClientID; }
inline int64_t CmaskAllExceptOne(int ClientID) { return CmaskAll()^CmaskOne(ClientID); }
inline bool CmaskIsSet(int64_t Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }
#endif
