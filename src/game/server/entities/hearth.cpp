#include "hearth.h"
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>

#define M_PI 3.14159265358979323846


CLifeHearth::CLifeHearth(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_HEARTH)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_Active = false;
	m_Lifetime = 1*Server()->TickSpeed();
	GameWorld()->InsertEntity(this);	
}

void CLifeHearth::Reset() 
{	
	GameServer()->m_World.DestroyEntity(this);
}

void CLifeHearth::Tick()
{
	CPlayer *pOwner = GameServer()->m_apPlayers[m_Owner];
	if (!pOwner ||  pOwner->GetTeam() != TEAM_RED || !m_Lifetime || (GameServer()->Collision()->IsSolid(m_Pos.x, m_Pos.y) && m_Active)) 
	{
		GameServer()->m_World.DestroyEntity(this);
		return;
	}

	if(m_Active) 
	{
		bool TickGot = false;
		for(CCharacter *pChr = (CCharacter*) GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChr; pChr = (CCharacter *)pChr->TypeNext())
		{
			if(distance(pChr->m_Pos, m_Pos) <= 400 && 
				pChr->GetPlayer()->GetTeam() == TEAM_BLUE && !pChr->m_iVisible && !pChr->IhammerTick) 
			{
				if (!GameServer()->Collision()->IntersectLine(m_Pos, pChr->m_Pos, 0, 0, false))
				{
					Lefpos = normalize(pChr->m_Pos - m_Pos);		
					if (distance(pChr->m_Pos, m_Pos) < 20)
					{
						if (pChr && pChr->GetPlayer())
						{
							pChr->GetPlayer()->SetZomb(m_Owner);	
							GameServer()->m_World.DestroyEntity(this);
							return;
						}
					}
					TickGot = true;	
				}	
			}
		}
		if(!TickGot)
		{
			m_Lifetime--;
			if(!pOwner->GetCharacter())
			{
				GameServer()->m_World.DestroyEntity(this);
				return;
			}
			Lefpos = pOwner->GetCharacter()->m_ActivTurs : vec2(0.0f, 0.0f);	
		}				
		m_Pos += Lefpos * 8;
	}
	else 
		m_Pos = pOwner->GetCharacter() ? GameServer()->GetPlayerChar(m_Owner)->m_Pos + normalize(GetDir(pi / 180 * ((Server()->Tick() + (360) % 360 + 1) * 6)))*(m_ProximityRadius + 36) : vec2(0.0f, 0.0f);
}

void CLifeHearth::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient) || !GameServer()->GetPlayerChar(m_Owner))
		return;
	
	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Subtype = 0;
	pP->m_Type = 0;
}
