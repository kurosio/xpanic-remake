/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gamemodes/DDRace.h>
#include "zdoor.h"

CDrSz::CDrSz(CGameWorld *pGameWorld, int Index, int Time)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_Index = Index;
	m_Time = -1;
	if(Time > 0)
		m_Time = Time*Server()->TickSpeed();
	m_State = DOOR_CLOSED;
	if(Index > 32)
		m_State = DOOR_OPEN;
	if(Index == -1)
		m_State = DOOR_ZCLOSED;

	GameWorld()->InsertEntity(this);
}

void CDrSz::Reset()
{
	if(m_Index == -1)
		GameServer()->m_World.DestroyEntity(this);
}

void CDrSz::Tick()
{
	if(m_Time > 0)
	{
		m_Time--;
		if(!m_Time)
			GameServer()->m_World.DestroyEntity(this);
	}
	if(m_Index > -1)
		m_State = GameServer()->m_pController->DoorState(m_Index);
	if(m_State == DOOR_OPEN || m_State == DOOR_ZCLOSING || m_State == DOOR_REOPENED || !m_Time)
		return;
	CCharacter *apCloseCCharacters[MAX_CLIENTS];
	int Num = GameServer()->m_World.FindEntities(m_Pos, 8.0f, (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		if(!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(m_Pos, apCloseCCharacters[i]->m_Pos, NULL, NULL, false) || (m_State == DOOR_ZCLOSED && apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_BLUE))
			continue;
		apCloseCCharacters[i]->m_HittingDoor = true;
		apCloseCCharacters[i]->m_PushDirection = normalize(apCloseCCharacters[i]->m_OldPos - m_Pos);
	}
}

void CDrSz::Snap(int SnappingClient)
{
	if(m_State == DOOR_OPEN || m_State == DOOR_ZCLOSING || m_State == DOOR_REOPENED || !m_Time || NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	if(m_State == DOOR_ZCLOSED)
		pP->m_Type = POWERUP_HEALTH;
	else
		pP->m_Type = POWERUP_ARMOR;
	pP->m_Subtype = 0;
}
