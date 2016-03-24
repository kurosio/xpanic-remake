/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/mapitems.h>

#include <game/generated/protocol.h>

#include "entities/pickup.h"
#include "gamecontroller.h"
#include "gamecontext.h"

#include "entities/projectile.h"
#include "entities/zdoor.h"
#include <game/layers.h>


IGameController::IGameController(class CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pServer = m_pGameServer->Server();
	m_GameFlags = GAMEFLAG_TEAMS;
	m_pGameType = "unknown";

	DoWarmup(g_Config.m_SvWarmup);
	m_RoundStartTick = Server()->Tick();

	m_aMapWish[0] = 0;
	m_GameOverTick = -1;
	m_RoundStarted = false;
	m_GrenadeLimit = 0;

	m_SuddenDeath = 0;
	m_aTeamscore[TEAM_RED] = m_aTeamscore[TEAM_BLUE] = 0;
	m_aNumSpawnPoints[0] = m_aNumSpawnPoints[1] = m_aNumSpawnPoints[2] = 0;
	m_aaOnTeamWinEvent[TEAM_RED][0] = m_aaOnTeamWinEvent[TEAM_BLUE][0] = m_aaOnTeamWinEvent[2][0] = 0; 
	
	for(int i = 0; i < 48; i++)
	{
		if(i < 32)
			m_Door[i].m_State = DOOR_CLOSED;
		else
			m_Door[i].m_State = DOOR_OPEN;
		m_Door[i].m_Tick = 0;
		m_Door[i].m_OpenTime = 10;
		m_Door[i].m_CloseTime = 3;
		m_Door[i].m_ReopenTime = 10;
	}
}

IGameController::~IGameController()
{
}

float IGameController::EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos)
{
	float Score = 0.0f;
	CCharacter *pC = static_cast<CCharacter *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));
	for (; pC; pC = (CCharacter *)pC->TypeNext())
	{
		float Scoremod = 1.0f;
		if (pEval->m_FriendlyTeam != -1 && pC->GetPlayer()->GetTeam() == pEval->m_FriendlyTeam)
			Scoremod = 0.5f;

		float d = distance(Pos, pC->m_Pos);
		Score += Scoremod * (d == 0 ? 1000000000.0f : 1.0f / d);
	}
	return Score;
}

void IGameController::EvaluateSpawnType(CSpawnEval *pEval, int Type)
{
	for (int i = 0; i < m_aNumSpawnPoints[Type]; i++)
	{
		CCharacter *aEnts[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(m_aaSpawnPoints[Type][i], 64, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		vec2 Positions[5] = { vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f) };	// start, left, up, right, down
		int Result = -1;
		for (int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			Result = Index;
			if (!GameServer()->m_World.m_Core.m_Tuning[0].m_PlayerCollision)
				break;

			for (int c = 0; c < Num; ++c)
			{
				if (GameServer()->Collision()->CheckPoint(m_aaSpawnPoints[Type][i] + Positions[Index]) ||
					distance(aEnts[c]->m_Pos, m_aaSpawnPoints[Type][i] + Positions[Index]) <= aEnts[c]->m_ProximityRadius)
				{
					Result = -1;
					break;
				}
			}
		}
		if (Result == -1)
			continue;

		vec2 P = m_aaSpawnPoints[Type][i] + Positions[Result];
		float S = EvaluateSpawnPos(pEval, P);
		if (!pEval->m_Got || pEval->m_Score > S)
		{
			pEval->m_Got = true;
			pEval->m_Score = S;
			pEval->m_Pos = P;
		}
	}
}

bool IGameController::CanSpawn(int Team, vec2 *pOutPos)
{
	if (Team == TEAM_SPECTATORS)
		return false;

	CSpawnEval Eval;

	if (IsTeamplay())
	{
		Eval.m_FriendlyTeam = Team;
		EvaluateSpawnType(&Eval, 0 + (Team & 1));
		if (!Eval.m_Got)
		{
			EvaluateSpawnType(&Eval, 0);
			if (!Eval.m_Got)
				EvaluateSpawnType(&Eval, 0 + ((Team + 1) & 1));
		}
	}
	else
	{
		EvaluateSpawnType(&Eval, 0);
		EvaluateSpawnType(&Eval, 1);
	}

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}


bool IGameController::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	if (Index < 0)
		return false;

	int Type = -1;
	int SubType = 0;

	int x, y;
	x = (Pos.x - 16.0f) / 32.0f;
	y = (Pos.y - 16.0f) / 32.0f;
	int sides[8];
	sides[0] = GameServer()->Collision()->Entity(x, y + 1, Layer);
	sides[1] = GameServer()->Collision()->Entity(x + 1, y + 1, Layer);
	sides[2] = GameServer()->Collision()->Entity(x + 1, y, Layer);
	sides[3] = GameServer()->Collision()->Entity(x + 1, y - 1, Layer);
	sides[4] = GameServer()->Collision()->Entity(x, y - 1, Layer);
	sides[5] = GameServer()->Collision()->Entity(x - 1, y - 1, Layer);
	sides[6] = GameServer()->Collision()->Entity(x - 1, y, Layer);
	sides[7] = GameServer()->Collision()->Entity(x - 1, y + 1, Layer);


	if (Index == ENTITY_SPAWN_RED)
		m_aaSpawnPoints[0][m_aNumSpawnPoints[0]++] = Pos;
	else if (Index == ENTITY_SPAWN_BLUE)
		m_aaSpawnPoints[1][m_aNumSpawnPoints[1]++] = Pos;

	if (Index == ENTITY_ARMOR_1)
		Type = POWERUP_ARMOR;
	else if (Index == ENTITY_HEALTH_1)
		Type = POWERUP_HEALTH;
	else if (Index == ENTITY_WEAPON_SHOTGUN)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_SHOTGUN;
	}
	else if (Index == ENTITY_WEAPON_GRENADE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_GRENADE;
	}
	else if (Index == ENTITY_WEAPON_RIFLE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_RIFLE;
	}
	else if((Index >= ENTITY_DOOR_BEGIN && Index <= ENTITY_DOOR_END) || (Index >= ENTITY_ZDOOR_BEGIN && Index <= ENTITY_ZDOOR_END))
	{
		CDrSz *pDoor = new CDrSz(&GameServer()->m_World, Index-17, -1);
		pDoor->m_Pos = Pos;
		return true;
	}
	if (Type != -1)
	{
		CPickup *pPickup = new CPickup(&GameServer()->m_World, Type, SubType);
		pPickup->m_Pos = Pos;
		return true;
	}
	return false;
}

void IGameController::EndRound()
{
	if (m_Warmup || m_GameOverTick != -1)
		return;

	if (!ZombStarted())
		return;

	if (!NumHumans() || !NumZombs() || (NumHumans() && (g_Config.m_SvTimelimit > 0 &&
		(Server()->Tick() - m_RoundStartTick) >= g_Config.m_SvTimelimit*Server()->TickSpeed()) && !m_SuddenDeath))
	{
		m_aTeamscore[TEAM_RED] = m_aTeamscore[TEAM_BLUE] = 0;
		if (!NumHumans())
		{
			if (GameServer()->GetPlayerChar(m_LastZomb))
				if(GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_UserID)
				{
					GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Winner++, GameServer()->m_apPlayers[m_LastZomb]->m_pAccount->Apply();
					if (g_Config.m_SvShowStatsEndRound)
					{
						float k = GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Winner;
						float d = GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Luser;
						float kofs = (k / (k + d)) * 100;

						char aBuf[64];
						GameServer()->SendChatTarget(-1, "----------------------------------");
						str_format(aBuf, sizeof(aBuf), "Zombie: %s (WIN)", Server()->ClientName(m_LastZomb));
						GameServer()->SendChatTarget(-1, aBuf);
						str_format(aBuf, sizeof(aBuf), "Win: %d / Lose: %d (%.1f)", GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Winner, GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Luser, kofs);
						GameServer()->SendChatTarget(-1, aBuf);
						GameServer()->SendChatTarget(-1, "----------------------------------");
					}
				}
			m_aTeamscore[TEAM_RED] = 100;
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if(!GameServer()->m_World.m_Paused && GameServer()->GetPlayerChar(i))
					GameServer()->GetPlayerChar(i)->ExperienceAdd(1, i);
			}
		}
		
		if (!NumZombs() || (NumHumans() && (g_Config.m_SvTimelimit > 0 && 
			(Server()->Tick() - m_RoundStartTick) >= g_Config.m_SvTimelimit*Server()->TickSpeed()) && !m_SuddenDeath))
		{
			if (GameServer()->GetPlayerChar(m_LastZomb))
				if(GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_UserID)
				{
					GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Luser++, GameServer()->m_apPlayers[m_LastZomb]->m_pAccount->Apply();
					if (g_Config.m_SvShowStatsEndRound)
					{
						float k = GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Winner;
						float d = GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Luser;
						float kofs = (k / (k + d)) * 100;

						char aBuf[64];
						GameServer()->SendChatTarget(-1, "----------------------------------");
						str_format(aBuf, sizeof(aBuf), "Zombie: %s (LOSE)", Server()->ClientName(m_LastZomb));
						GameServer()->SendChatTarget(-1, aBuf);
						str_format(aBuf, sizeof(aBuf), "Win: %d / Lose: %d (%.1f)", GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Winner, GameServer()->m_apPlayers[m_LastZomb]->m_AccData.m_Luser, kofs);
						GameServer()->SendChatTarget(-1, aBuf);
						GameServer()->SendChatTarget(-1, "----------------------------------");
					}
				}
			m_aTeamscore[TEAM_BLUE] = 100;
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_BLUE)
					GameServer()->m_apPlayers[i]->m_Score += 10;
				
				if(!GameServer()->m_World.m_Paused && GameServer()->GetPlayerChar(i))
					GameServer()->GetPlayerChar(i)->ExperienceAdd(5, i);
			}
		}
	}

	GameServer()->m_World.m_Paused = true;
	m_GameOverTick = Server()->Tick()+Server()->TickSpeed()*5;
	m_SuddenDeath = 0;
}

void IGameController::ResetGame() {
	GameServer()->m_World.m_ResetRequested = true;
}

const char *IGameController::GetTeamName(int Team)
{
	if (IsTeamplay())
	{
		if (Team == TEAM_RED) return "zombie team";
		else if (Team == TEAM_BLUE) return "human team";
	}
	return "spectators";
}

void IGameController::StartRound()
{
	ResetGame();

	m_GrenadeLimit = 0;
	m_RoundStartTick = Server()->Tick();
	m_Warmup = m_SuddenDeath = 0;
	m_GameOverTick = -1;

	GameServer()->m_World.m_Paused = false;
	m_aTeamscore[TEAM_RED] = m_aTeamscore[TEAM_BLUE] = 0;
	Server()->DemoRecorder_HandleAutoStart();

	StartZomb(false), CheckZomb();

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "start round type='%s' teamplay='%d'", m_pGameType, m_GameFlags&GAMEFLAG_TEAMS);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
}

void IGameController::ChangeMap(const char *pToMap)
{
	str_copy(g_Config.m_SvMap, pToMap, sizeof(m_aMapWish));
	EndRound();
}

void IGameController::PostReset()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->Respawn();
			GameServer()->m_apPlayers[i]->m_ScoreStartTick = Server()->Tick();
			GameServer()->m_apPlayers[i]->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;
		}
	}
}

void IGameController::OnPlayerInfoChange(class CPlayer *pP)
{
	const int aTeamColors[2] = { 65387, 10223467 };
	if (IsTeamplay())
	{
		pP->m_TeeInfos.m_UseCustomColor = 1;
		if (pP->GetTeam() >= TEAM_RED && pP->GetTeam() <= TEAM_BLUE)
			pP->m_TeeInfos.m_ColorBody = pP->m_TeeInfos.m_ColorFeet = aTeamColors[pP->GetTeam()];
		else
			pP->m_TeeInfos.m_ColorBody = pP->m_TeeInfos.m_ColorFeet = 12895054;
	}
}

int IGameController::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if (!pKiller || Weapon == WEAPON_GAME) return 0;
	if (pKiller != pVictim->GetPlayer()) pKiller->m_Score++;
	if (Weapon == WEAPON_SELF) pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed()*3.0f;

	return 0;
}

void IGameController::OnCharacterSpawn(class CCharacter *pChr)
{
	int BigLvlID = pChr->GetPlayer()->GetCID();
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[BigLvlID])
		{
			if(GameServer()->m_apPlayers[i]->m_AccData.m_UserID && GameServer()->m_apPlayers[BigLvlID]->m_AccData.m_UserID)
				if (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_BLUE && GameServer()->m_apPlayers[i]->m_AccData.m_Level > GameServer()->m_apPlayers[BigLvlID]->m_AccData.m_Level
					&& pChr->GetPlayer()->GetCID() != BigLvlID)
					BigLvlID = i;
		}
	}

	if (GameServer()->m_apPlayers[BigLvlID] && pChr->GetPlayer()->GetCID() != BigLvlID)
	{
		if (GameServer()->m_apPlayers[BigLvlID]->m_AccData.m_Level > pChr->GetPlayer()->m_AccData.m_Level && GameServer()->m_apPlayers[BigLvlID]->m_AccData.m_Level > 20)
			pChr->IncreaseHealth(700 + pChr->GetPlayer()->m_AccData.m_Level * 10 + pChr->GetPlayer()->m_AccData.m_Health * 50 + GameServer()->m_apPlayers[BigLvlID]->m_AccData.m_Level * 5);
		else
			pChr->IncreaseHealth(200 + pChr->GetPlayer()->m_AccData.m_Level * 10 + pChr->GetPlayer()->m_AccData.m_Health * 50);
	}
	else
		pChr->IncreaseHealth(700 + pChr->GetPlayer()->m_AccData.m_Level * 10 + pChr->GetPlayer()->m_AccData.m_Health * 50);

	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_GUN, 10);
}

void IGameController::DoWarmup(int Seconds)
{
	if (Seconds < 0) m_Warmup = 0;
	else m_Warmup = Seconds*Server()->TickSpeed();
}

bool IGameController::IsFriendlyFire(int ClientID1, int ClientID2)
{
	if (ClientID1 == ClientID2)
		return false;

	if (IsTeamplay())
	{
		if (!GameServer()->m_apPlayers[ClientID1] || !GameServer()->m_apPlayers[ClientID2])
			return false;

		if (GameServer()->m_apPlayers[ClientID1]->GetTeam() == GameServer()->m_apPlayers[ClientID2]->GetTeam())
			return true;
	}
	return false;
}

void IGameController::Tick()
{
	if (m_Warmup)
	{
		m_Warmup--;
		if (!m_Warmup)
		{
			if (NumPlayers() > 1 && g_Config.m_SvZombieRatio)
			{
				int ZAtStart = 1;
				ZAtStart = (int)NumPlayers() / (int)g_Config.m_SvZombieRatio;
				if (!ZAtStart)
					ZAtStart = 1;

				for (; ZAtStart; ZAtStart--)
				{
					RandomZomb(-1);
					if(NumPlayers() >= g_Config.m_SvTwoZombie && g_Config.m_SvTwoZombie)
						RandomZomb(-1);
				}
			}
			else
				StartZomb(false);
		}
	}

	if (m_GameOverTick != -1)
	{
		if (Server()->Tick() > m_GameOverTick)
			StartRound();
	}

	if(GameServer()->m_World.m_Paused)
		++m_RoundStartTick;
	
	if (g_Config.m_SvInactiveKickTime > 0)
	{
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			#ifdef CONF_DEBUG
			if (g_Config.m_DbgDummies)
			{
				if (i >= MAX_CLIENTS - g_Config.m_DbgDummies)
					break;
			}
			#endif
			if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !Server()->IsAuthed(i))
			{
				if (Server()->Tick() > GameServer()->m_apPlayers[i]->m_LastActionTick + g_Config.m_SvInactiveKickTime*Server()->TickSpeed() * 60)
					GameServer()->m_apPlayers[i]->SetTeam(TEAM_SPECTATORS);
			}
		}
	}
	
	if (!ZombStarted() || GameServer()->m_World.m_Paused)
		return;

	DoWincheck();
	
	for(int i = 0; i < 48; i++)
	{
		if(m_Door[i].m_Tick > 0)
		{
			if(m_Door[i].m_State == DOOR_CLOSED)
			{
				if(m_Door[i].m_Tick <= Server()->Tick())
				{
					SetDoorState(i, DOOR_OPEN);
					m_Door[i].m_Tick = 0;
				}
			}
			else if(m_Door[i].m_Tick <= Server()->Tick())
			{
				if(m_Door[i].m_State == DOOR_ZCLOSING && i < 32)
				{
					SetDoorState(i, DOOR_ZCLOSED);
					m_Door[i].m_Tick = Server()->Tick() + Server()->TickSpeed()*GetDoorTime(i);
				}
				else if(m_Door[i].m_State == DOOR_ZCLOSED && i < 32)
				{
					SetDoorState(i, DOOR_REOPENED);
					m_Door[i].m_Tick = 0;
				}
				else if(m_Door[i].m_State == DOOR_ZCLOSING && i >= 32)
				{
					SetDoorState(i, DOOR_ZCLOSED);
					m_Door[i].m_Tick = Server()->Tick() + Server()->TickSpeed()*GetDoorTime(i);
				}
				else if(m_Door[i].m_State == DOOR_ZCLOSED && i >= 32)
				{
					SetDoorState(i, DOOR_REOPENED);
					m_Door[i].m_Tick = 0;
				}
			}
		}
	}
}

bool IGameController::IsTeamplay() const {
	return m_GameFlags&GAMEFLAG_TEAMS;
}

void IGameController::Snap(int SnappingClient)
{
	CNetObj_GameInfo *pGameInfoObj = (CNetObj_GameInfo *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if (!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	if (m_GameOverTick != -1)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_GAMEOVER;
	if (m_SuddenDeath)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;
	if (GameServer()->m_World.m_Paused)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_PAUSED;

	pGameInfoObj->m_RoundStartTick = m_RoundStartTick;
	pGameInfoObj->m_WarmupTimer = m_Warmup;

	pGameInfoObj->m_ScoreLimit = 0;
	pGameInfoObj->m_TimeLimit = g_Config.m_SvTimelimit;

	pGameInfoObj->m_RoundNum = (str_length(g_Config.m_SvMaprotation) && g_Config.m_SvRoundsPerMap) ? g_Config.m_SvRoundsPerMap : 0;
	pGameInfoObj->m_RoundCurrent = 0;

	CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if (!pGameDataObj)
		return;

	if(m_aTeamscore[TEAM_RED] >= 100 || m_aTeamscore[TEAM_BLUE] >= 100)
	{
		pGameDataObj->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
		pGameDataObj->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];
	} else
	{
		pGameDataObj->m_TeamscoreRed = NumZombs();
		pGameDataObj->m_TeamscoreBlue = NumHumans();
	}
}

int IGameController::GetAutoTeam(int NotThisID)
{
	if (g_Config.m_DbgStress)
		return 0;

	if (ZombStarted() && !m_Warmup) return TEAM_RED;
	else return TEAM_BLUE;
}

bool IGameController::CanJoinTeam(int Team, int NotThisID)
{
	if (Team == TEAM_SPECTATORS || (GameServer()->m_apPlayers[NotThisID] && GameServer()->m_apPlayers[NotThisID]->GetTeam() != TEAM_SPECTATORS))
		return true;

	int aNumplayers[2] = { 0,0 };
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i] && i != NotThisID)
		{
			if (GameServer()->m_apPlayers[i]->GetTeam() >= TEAM_RED && GameServer()->m_apPlayers[i]->GetTeam() <= TEAM_BLUE)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}
	return (aNumplayers[0] + aNumplayers[1]) < Server()->MaxClients() - g_Config.m_SvSpectatorSlots;
}

void IGameController::DoWincheck()
{
	if (g_Config.m_SvTimelimit > 0 && (Server()->Tick() - m_RoundStartTick) >= g_Config.m_SvTimelimit*Server()->TickSpeed() * 60)
		EndRound();
}

int IGameController::ClampTeam(int Team)
{
	if (Team < 0)
		return TEAM_SPECTATORS;
	if (IsTeamplay())
		return Team & 1;
	return 0;
}

void IGameController::ResetZ()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i])
			GameServer()->m_apPlayers[i]->ResetZomb();
	}
	GameServer()->SendBroadcast("", -1);
	ResetDoors();
}

bool IGameController::ZombStarted(){
	return m_RoundStarted;
}

void IGameController::StartZomb(bool Value)
{
	if (!Value) ResetZ();
	m_RoundStarted = Value;
}

void IGameController::CheckZomb()
{
	if (NumPlayers() < 2)
	{
		StartZomb(0);
		DoWarmup(0);
		m_SuddenDeath = 1;
		m_LastZomb = m_LastZomb2 = -1;
		return;
	}
	
	m_SuddenDeath = 0;
	if (!m_RoundStarted && !m_Warmup && g_Config.m_SvZombieRatio)
	{
		StartZomb(true);
		DoWarmup(g_Config.m_SvWarmup);
	}
	else if ((!NumHumans() || !NumZombs()) && m_RoundStarted && !m_Warmup)
		EndRound();
}

int IGameController::NumPlayers()
{
	int NumPlayers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			NumPlayers++;
	}
	return NumPlayers;
}

int IGameController::NumZombs()
{
	int NumZombs = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_RED)
			NumZombs++;
	}
	return NumZombs;
}

int IGameController::NumHumans()
{
	int NumHumans = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_BLUE)
			NumHumans++;
	}
	return NumHumans;
}

void IGameController::RandomZomb(int Mode)
{
	int ZombCID = rand() % MAX_CLIENTS, WTF = 250;
	while (!GameServer()->m_apPlayers[ZombCID] || (GameServer()->m_apPlayers[ZombCID] && GameServer()->m_apPlayers[ZombCID]->GetTeam() == TEAM_SPECTATORS) ||
		m_LastZomb == ZombCID || (NumPlayers() > 2 && m_LastZomb2 == ZombCID) || !GameServer()->m_apPlayers[ZombCID]->GetCharacter() ||
			(GameServer()->m_apPlayers[ZombCID]->GetCharacter() && !GameServer()->m_apPlayers[ZombCID]->GetCharacter()->IsAlive()))
	{
		ZombCID = rand() % MAX_CLIENTS;
		WTF--;
		if (!WTF)
		{
			StartRound();
			return;
		}
	}

	GameServer()->m_apPlayers[ZombCID]->SetZomb(Mode);
	StartZomb(true);
	m_LastZomb2 = m_LastZomb;
	m_LastZomb = ZombCID;
}

void IGameController::OnHoldpoint(int Index)
{
	if(m_Door[Index].m_Tick || !(m_Door[Index].m_State == DOOR_CLOSED) || !ZombStarted() || GetDoorTime(Index) == -1)
		return;

	m_Door[Index].m_Tick = Server()->Tick() + Server()->TickSpeed()*GetDoorTime(Index)-1; // -1: if doortime is 5 we get a doublemessage (but srsly a 5 seconds door?)
}

void IGameController::OnZHoldpoint(int Index)
{
	if(m_Door[Index].m_State > DOOR_OPEN || !ZombStarted() || GetDoorTime(Index) == -1)
		return;

	SetDoorState(Index, DOOR_ZCLOSING);
	m_Door[Index].m_Tick = Server()->Tick() + Server()->TickSpeed()*GetDoorTime(Index);
}

void IGameController::OnZStop(int Index)
{
	if(m_Door[Index].m_State > DOOR_OPEN || !ZombStarted() || GetDoorTime(Index) == -1)
		return;

	SetDoorState(Index, DOOR_ZCLOSING);
	m_Door[Index].m_Tick = Server()->Tick() + Server()->TickSpeed()*GetDoorTime(Index);
}


void IGameController::ResetDoors()
{
	for(int i = 0; i < 48; i++)
	{
		if(i < 32)	m_Door[i].m_State = DOOR_CLOSED;
		else m_Door[i].m_State = DOOR_OPEN;
		m_Door[i].m_Tick = 0;
	}
}

int IGameController::DoorState(int Index){
	return m_Door[Index].m_State;
}

void IGameController::SetDoorState(int Index, int State){
	m_Door[Index].m_State = State;
}

int IGameController::GetDoorTime(int Index)
{
	if(m_Door[Index].m_State == DOOR_CLOSED) return m_Door[Index].m_OpenTime;
	else if(m_Door[Index].m_State == DOOR_ZCLOSING || m_Door[Index].m_State == DOOR_OPEN) return m_Door[Index].m_CloseTime;
	else if(m_Door[Index].m_State == DOOR_ZCLOSED) return m_Door[Index].m_ReopenTime;
	return 0;
}

