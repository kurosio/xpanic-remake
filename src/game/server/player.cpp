/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include "player.h"

#include <engine/server.h>
#include <engine/server/server.h>
#include "gamecontext.h"
#include <game/gamecore.h>
#include <game/version.h>
#include <game/server/teams.h>
#include "gamemodes/DDRace.h"
#include "entities/hearth.h"
#include "entities/cmds.h"
#include <stdio.h>

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, int Team)
{
	m_pGameServer = pGameServer;
	m_ClientID = ClientID;
	m_Team = GameServer()->m_pController->ClampTeam(Team);
	m_pCharacter = 0;
	m_NumInputs = 0;
	m_KillMe = 0;
	
	m_pAccount = new CAccount(this, m_pGameServer);
	m_pChatCmd = new CCmd(this, m_pGameServer);	
	
	if(m_AccData.m_UserID)
		m_pAccount->Apply();
	
	Reset();
}

CPlayer::~CPlayer()
{
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Reset()
{
	m_RespawnTick = m_ScoreStartTick = m_DieTick = Server()->Tick();
	if (m_pCharacter)
		delete m_pCharacter;
	m_pCharacter = 0;
	m_KillMe = 0;
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();

	int* idMap = Server()->GetIdMap(m_ClientID);
	for (int i = 1;i < VANILLA_MAX_CLIENTS;i++)
	{
		idMap[i] = -1;
	}
	idMap[0] = m_ClientID;

	m_LastCommandPos = m_Sent1stAfkWarning = m_Sent2ndAfkWarning = m_ChatScore = m_LastSetSpectatorMode = 0;
	m_LastPlaytime = time_get();
	m_EyeEmote = true;
	m_DefEmote = EMOTE_NORMAL;
	m_Afk = false;
	m_LifeActives = false;
	m_LastWhisperTo = -1;
	m_TimeoutCode[0] = '\0';
	m_TuneZone = 0;
	m_TuneZoneOld = m_TuneZone;
	m_FirstPacket = true;

	m_SendVoteIndex = -1;
	m_DefEmoteReset = -1;

	m_ClientVersion = VERSION_VANILLA;
	m_ShowOthers = g_Config.m_SvShowOthersDefault;
	m_ShowAll = g_Config.m_SvShowAllDefault;
	m_SpecTeam = m_NextPauseTick = 0;
}

void CPlayer::Tick()
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	if (!m_AccData.m_UserID)
		m_Team = -1;

	if(m_KillMe != 0)
	{
		KillCharacter(m_KillMe);
		m_KillMe = 0;
		return;
	}

	if (m_ChatScore > 0)
		m_ChatScore--;

	if (m_AccData.m_UserID && m_AccData.m_Exp >= m_AccData.m_Level)
	{
		m_AccData.m_Money++;
		m_AccData.m_Exp -= m_AccData.m_Level;
		m_AccData.m_Level++;
		if (m_AccData.m_Exp < m_AccData.m_Level)
		{
			if (m_AccData.m_UserID)
				m_pAccount->Apply();

			char SendLVL[64];
			str_format(SendLVL, sizeof(SendLVL), "Successfully! Your new level %d\n/ Upgrade counts %d", m_AccData.m_Level, m_AccData.m_Money);
			GameServer()->SendChatTarget(m_ClientID, SendLVL);
		}
	}
	
	Server()->SetClientScore(m_ClientID, m_Score);

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick()%Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum/Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(((CServer *)Server())->m_NetServer.ErrorString(m_ClientID)[0])
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' would have timed out, but can use timeout protection now", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		((CServer *)(Server()))->m_NetServer.ResetErrorString(m_ClientID);
	}

	if(!GameServer()->m_World.m_Paused)
	{
		if(!m_pCharacter && m_Team == TEAM_SPECTATORS && m_SpectatorID == SPEC_FREEVIEW)
			m_ViewPos -= vec2(clamp(m_ViewPos.x-m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y-m_LatestActivity.m_TargetY, -400.0f, 400.0f));

		if(!m_pCharacter && m_DieTick+Server()->TickSpeed()*3 <= Server()->Tick())
			m_Spawning = true;

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
				m_ViewPos = m_pCharacter->m_Pos;
			else
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else if(m_Spawning && m_RespawnTick <= Server()->Tick())
			TryRespawn();
	}
	else
	{
		++m_RespawnTick;
		++m_DieTick;
		++m_ScoreStartTick;
		++m_LastActionTick;
	}

	m_TuneZoneOld = m_TuneZone; // determine needed tunings with viewpos
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_ViewPos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if (m_TuneZone != m_TuneZoneOld) // dont send tunigs all the time
	{
		GameServer()->SendTuningParams(m_ClientID, m_TuneZone);
	}
}

void CPlayer::PostTick()
{
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}
	if(m_Team == TEAM_SPECTATORS && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID] && GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter())
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter()->m_Pos;
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	int id = m_ClientID;
	if (SnappingClient > -1 && !Server()->Translate(id, SnappingClient)) 
		return;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, id, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	char pSendName[22];
	if (m_AccData.m_UserID)
	{
		if (m_AccData.m_UserID == g_Config.m_SvOwnerAccID && m_Prefix) str_format(pSendName, sizeof(pSendName), "[O]%s", Server()->ClientName(m_ClientID));
		else if(Server()->IsAuthed(GetCID()) && m_Prefix) str_format(pSendName, sizeof(pSendName), "[A]%s", Server()->ClientName(m_ClientID));
		else if(m_AccData.m_PlayerState == 1) str_format(pSendName, sizeof(pSendName), "[P-%d]%s", m_AccData.m_Level, Server()->ClientName(m_ClientID));
		else if(m_AccData.m_PlayerState == 2 && m_Prefix) str_format(pSendName, sizeof(pSendName), "[VIP]%s", Server()->ClientName(m_ClientID));
		else if(m_AccData.m_PlayerState == 3) str_format(pSendName, sizeof(pSendName), "[H-%d]%s", m_AccData.m_Level, Server()->ClientName(m_ClientID));
		else str_format(pSendName, sizeof(pSendName), "[%d]%s", m_AccData.m_Level, Server()->ClientName(m_ClientID));
		
		if(m_AccData.m_Freeze)
			str_format(pSendName, sizeof(pSendName), "[F-%d]%s", m_AccData.m_Level, Server()->ClientName(m_ClientID));
	}
	StrToInts(&pClientInfo->m_Name0, 4, m_AccData.m_UserID ? pSendName : Server()->ClientName(m_ClientID));
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	if (m_StolenSkin && SnappingClient != m_ClientID && g_Config.m_SvSkinStealAction == 1)
	{
		StrToInts(&pClientInfo->m_Skin0, 6, "pinky");
		pClientInfo->m_UseCustomColor = 0;
		pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
		pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;
	} else
	{
		StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
		pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
		pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
		pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;
	}

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	pPlayerInfo->m_Local = 0;
	pPlayerInfo->m_ClientID = id;
	pPlayerInfo->m_Score = m_Score;
	pPlayerInfo->m_Team = m_Team;

	if(m_ClientID == SnappingClient)
		pPlayerInfo->m_Local = 1;

	if(m_ClientID == SnappingClient && m_Team == TEAM_SPECTATORS)
	{
		CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
		if(!pSpectatorInfo)
			return;

		pSpectatorInfo->m_SpectatorID = m_SpectatorID;
		pSpectatorInfo->m_X = m_ViewPos.x;
		pSpectatorInfo->m_Y = m_ViewPos.y;
	}
}

void CPlayer::FakeSnap(int SnappingClient)
{
	IServer::CClientInfo info;
	Server()->GetClientInfo(SnappingClient, &info);
	CGameContext *GameContext = (CGameContext *) GameServer();
	if (SnappingClient > -1 && GameContext->m_apPlayers[SnappingClient] && GameContext->m_apPlayers[SnappingClient]->m_ClientVersion >= VERSION_DDNET_OLD)
		return;

	int id = VANILLA_MAX_CLIENTS - 1;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, id, sizeof(CNetObj_ClientInfo)));

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, " ");
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
}

void CPlayer::OnDisconnect(const char *pReason)
{
	if(m_AccData.m_UserID)
		m_pAccount->Reset();
	
	KillCharacter();

	if(GameServer()->m_pController->NumZombs() == 1 && GetTeam() == TEAM_RED
		&& GameServer()->m_pController->ZombStarted() && !GameServer()->m_pController->m_Warmup)
	{
		char aBuf[128], aAddrStr[NETADDR_MAXSTRSIZE] = {0};
		str_format(aBuf, sizeof(aBuf), "'%s' has left the game (banned 5 minutes last zombie)", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		
		Server()->GetClientAddr(m_ClientID, aAddrStr, sizeof(aAddrStr));
		str_format(aBuf, sizeof(aBuf), "ban %s 5 You were last zombie", aAddrStr);
		GameServer()->Console()->ExecuteLine(aBuf);
	}
	else if(Server()->ClientIngame(m_ClientID))
	{
		char aBuf[96];
		if(pReason && *pReason)
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(m_ClientID), pReason);
		else
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", m_ClientID, Server()->ClientName(m_ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	AfkVoteTimer(NewInput);
	m_NumInputs++;

	if(m_pCharacter)
		m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if (AfkTimer(NewInput->m_TargetX, NewInput->m_TargetY))
		return; // we must return if kicked, as player struct is already deleted
	AfkVoteTimer(NewInput);

	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
	// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
		return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter)
		m_pCharacter->OnDirectInput(NewInput);

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		m_Spawning = true;

	if(!m_pCharacter && m_Team == TEAM_SPECTATORS && m_SpectatorID == SPEC_FREEVIEW)
		m_ViewPos = vec2(NewInput->m_TargetX, NewInput->m_TargetY);

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
	}
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::KillCharacter(int Weapon)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);
		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn()
{
	if(m_Team != TEAM_SPECTATORS)
		m_Spawning = true;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	if (!m_AccData.m_UserID)
		return GameServer()->SendBroadcast("To start the game read /help.", m_ClientID);

	if (m_Team == TEAM_RED && Team != TEAM_SPECTATORS) 
		return GameServer()->SendBroadcast("Zombies can't change team.", m_ClientID);

	if (GameServer()->m_pController->ZombStarted() && !GameServer()->m_pController->m_Warmup && Team == TEAM_BLUE)
		return GameServer()->SendBroadcast("You only can join the human team when round hasn't started.", m_ClientID);
	
	if (Team == TEAM_RED && ((GameServer()->m_pController->ZombStarted() && GameServer()->m_pController->m_Warmup) || !GameServer()->m_pController->ZombStarted()))
		return GameServer()->SendBroadcast("Zombie will be chosen randomly.", m_ClientID);

	if (m_Team == TEAM_BLUE && GameServer()->m_pController->ZombStarted())
		return GameServer()->SendBroadcast("You can't join the zombie team.", m_ClientID);
	
	if (m_Team == TEAM_RED && GameServer()->m_pController->NumZombs() < 2 && GameServer()->m_pController->ZombStarted())
		return GameServer()->SendBroadcast("You are the only zombie.", m_ClientID);

	char aBuf[64];
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(m_ClientID), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	KillCharacter();

	m_Team = Team;
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;

	m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);
	GameServer()->m_pController->CheckZomb();

	if(Team == TEAM_SPECTATORS)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
				GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
		}
	}
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;

	if(!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos))
		return;

	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos);
}

bool CPlayer::AfkTimer(int NewTargetX, int NewTargetY)
{
	/*
		afk timer (x, y = mouse coordinates)
		Since a player has to move the mouse to play, this is a better method than checking
		the player's position in the game world, because it can easily be bypassed by just locking a key.
		Frozen players could be kicked as well, because they can't move.
		It also works for spectators.
		returns true if kicked
	*/

	if(m_Authed)
		return false; // don't kick admins
	if(g_Config.m_SvMaxAfkTime == 0)
		return false; // 0 = disabled

	if(NewTargetX != m_LastTarget_x || NewTargetY != m_LastTarget_y)
	{
		m_LastPlaytime = time_get();
		m_LastTarget_x = NewTargetX;
		m_LastTarget_y = NewTargetY;
		m_Sent1stAfkWarning = 0; // afk timer's 1st warning after 50% of sv_max_afk_time
		m_Sent2ndAfkWarning = 0;

	}
	else
	{
		// not playing, check how long
		if(m_Sent1stAfkWarning == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.5))
		{
			sprintf(
				m_pAfkMsg,
				"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
				(int)(g_Config.m_SvMaxAfkTime*0.5),
				g_Config.m_SvMaxAfkTime
			);
			m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
			m_Sent1stAfkWarning = 1;
		}
		else if(m_Sent2ndAfkWarning == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.9))
		{
			sprintf(
				m_pAfkMsg,
				"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
				(int)(g_Config.m_SvMaxAfkTime*0.9),
				g_Config.m_SvMaxAfkTime
			);
			m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
			m_Sent2ndAfkWarning = 1;
		}
		else if(m_LastPlaytime < time_get()-time_freq()*g_Config.m_SvMaxAfkTime)
		{
			CServer* serv =	(CServer*)m_pGameServer->Server();
			serv->Kick(m_ClientID,"Away from keyboard");
			return true;
		}
	}
	return false;
}

void CPlayer::AfkVoteTimer(CNetObj_PlayerInput *NewTarget)
{
	if(g_Config.m_SvMaxAfkVoteTime == 0)
		return;

	if(mem_comp(NewTarget, &m_LastTarget, sizeof(CNetObj_PlayerInput)) != 0)
	{
		m_LastPlaytime = time_get();
		mem_copy(&m_LastTarget, NewTarget, sizeof(CNetObj_PlayerInput));
	}
	else if(m_LastPlaytime < time_get()-time_freq()*g_Config.m_SvMaxAfkVoteTime)
	{
		m_Afk = true;
		return;
	}

	m_Afk = false;
}

bool CPlayer::IsPlaying()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return true;
	return false;
}

void CPlayer::SetZomb(int From)
{
	CNetMsg_Sv_KillMsg Msg;
	if (From > -1)
	{
		Msg.m_Killer = From;
		Msg.m_Victim = m_ClientID;
		Msg.m_Weapon = WEAPON_HAMMER;
		GameServer()->m_apPlayers[From]->m_Score++;
		if(GameServer()->GetPlayerChar(From) && From != m_ClientID)
			GameServer()->GetPlayerChar(From)->ExperienceAdd(3 + m_AccData.m_Level / 50 * g_Config.m_SvExpBonus, From);
	}
	Msg.m_ModeSpecial = 0;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	m_Team = TEAM_RED;
	if (From == -1 || From == -3)
	{
		if (From == -1)
		{
			char aBuf[52];			
			str_format(aBuf, sizeof(aBuf), "'%s' wants your brain! Run away.", Server()->ClientName(m_ClientID));
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		}
		m_LifeActives = false;
		new CLifeHearth(&GameServer()->m_World, m_pCharacter->m_Pos, m_ClientID);
		m_pCharacter->Die(m_ClientID, WEAPON_WORLD);
	}
	m_pCharacter->SetZomb();
	GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);
	GameServer()->m_pController->CheckZomb();
	GameServer()->SendChatTarget(m_ClientID, "You are now a zombie! Eat some brains.");
}

void CPlayer::ResetZomb()
{
	if (m_Team == TEAM_SPECTATORS)
		return;

	m_Team = TEAM_BLUE;
	m_KillingSpree = m_JumpsShop = m_RangeShop = 0;
	GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);
}
