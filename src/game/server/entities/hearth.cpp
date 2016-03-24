#include "hearth.h"
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>

#define M_PI 3.14159265358979323846


CLifeHearth::CLifeHearth(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_Lifetime = 1*Server()->TickSpeed();
	GameWorld()->InsertEntity(this);	
	Fistheart = false;
}

void CLifeHearth::Reset() 
{	
	GameServer()->m_World.DestroyEntity(this);
}

void CLifeHearth::Tick()
{
	if (!GameServer()->m_apPlayers[m_Owner] || GameServer()->m_apPlayers[m_Owner]->GetTeam() == TEAM_BLUE
			|| GameServer()->m_apPlayers[m_Owner]->GetTeam() == TEAM_SPECTATORS) 
		return Reset();

	if (!GameServer()->GetPlayerChar(m_Owner))
	{
		if (GameServer()->m_apPlayers[m_Owner]->m_LifeActives)
			return Reset();

		return;
	}

	if (!GameServer()->m_apPlayers[m_Owner]->m_LifeActives)
	{
		if (GameServer()->GetPlayerChar(m_Owner)->m_TypeHealthCh && g_Config.m_SvNewHearth)
			m_Pos = GameServer()->GetPlayerChar(m_Owner)->m_Pos;
		else
			m_Pos = GameServer()->GetPlayerChar(m_Owner)->m_Pos + normalize(GetDir(pi / 180 * ((Server()->Tick() + (360) % 360 + 1) * 6)))*(m_ProximityRadius + 36);

		return;
	}

	CCharacter *pTarget = 0;
	CCharacter *pClosest = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	while (pClosest)
	{
		if (distance(pClosest->m_Pos, m_Pos) <= 400 && pClosest->GetPlayer()->GetTeam() == TEAM_BLUE && !pClosest->IhammerTick && !pClosest->m_iVisible)
		{
			if (!GameServer()->Collision()->IntersectLine(m_Pos, pClosest->m_Pos, 0, 0, false))
				pTarget = pClosest;
		}
		pClosest = (CCharacter *)pClosest->TypeNext();
	}

	if(!g_Config.m_SvNewHearth && !Fistheart)
	{
		Fistheart = true;
		m_Pos = GameServer()->GetPlayerChar(m_Owner)->m_Pos;
	}
	
	if (GameServer()->Collision()->IsSolid(m_Pos.x, m_Pos.y))
		return Reset();
	
	if (!pTarget)
	{
		if (!StopTickLef)
		{
			Lefpos = GameServer()->GetPlayerChar(m_Owner)->m_ActivTurs;
			StopTickLef = true;
		}
		if (m_Lifetime)
		{
			m_Lifetime--;
			if (!m_Lifetime)
				return Reset();
		}
	}
	m_Pos += Lefpos * 8;

	if (!pTarget)
		return;

	Lefpos = normalize(pTarget->m_Pos - m_Pos);
	
	if (m_Lifetime != 1000)
		m_Lifetime = 1000;

	if (distance(pTarget->m_Pos, m_Pos) < pTarget->m_ProximityRadius + 2.0f && GameServer()->m_apPlayers[m_Owner]->m_LifeActives)
	{
		if (pTarget && pTarget->GetPlayer())
			pTarget->GetPlayer()->SetZomb(m_Owner);

		return Reset();
	}
}

void CLifeHearth::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;
	
	if (!GameServer()->GetPlayerChar(m_Owner))
		return;
	
	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	if (!GameServer()->m_apPlayers[m_Owner]->m_LifeActives)
	{
		if (GameServer()->GetPlayerChar(m_Owner)->m_TypeHealthCh) 
		{
			float a = GetAngle(GameServer()->GetPlayerChar(m_Owner)->m_ActivTurs);
			pP->m_X = (int)(cos(a + M_PI / 1 * 4)*(32.0) + m_Pos.x);
			pP->m_Y = (int)(sin(a + M_PI / 1 * 4)*(32.0) + m_Pos.y);
		}
		else 
		{
			pP->m_X = (int)m_Pos.x;
			pP->m_Y = (int)m_Pos.y;
		}
	}
	else{
		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y;
	}
	pP->m_Subtype = 0;
	pP->m_Type = 0;
}
