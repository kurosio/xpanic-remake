#include <base/math.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "wall.h"

CWall::CWall(CGameWorld *pGameWorld, vec2 From, vec2 To, int Owner, int Time)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Pos = To;
	m_From = From;
	m_Owner = Owner;
	m_OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(Time > 7)
		m_OwnerChar->m_RiflePos = vec2(0,0);
	m_Time = Time*Server()->TickSpeed();
	m_ID2 = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

void CWall::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
	Server()->SnapFreeID(m_ID2);
}

bool CWall::HitCharacter(CCharacter *Hit)
{
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = FindCharacters(m_From, m_Pos, 2.5f, apEnts, MAX_CLIENTS);
	for(int i = 0; i < Num; i++)
	{
		if(apEnts[i]->GetPlayer()->GetTeam() == TEAM_RED)
		{
			vec2 IntersectPos = closest_point_on_line(m_Pos, m_From, apEnts[i]->m_Pos);
			float Len = distance(apEnts[i]->m_Pos, IntersectPos);
			if(Len < apEnts[i]->m_ProximityRadius+2.0f)
			{
				apEnts[i]->m_Core.m_Pos = apEnts[i]->m_OldPos;
				apEnts[i]->m_Core.m_Vel = vec2(0,0);
				
				if(apEnts[i]->m_Core.m_Jumped >= 2)
					apEnts[i]->m_Core.m_Jumped = 1;
			}
		}
	}
	return true;
}

int CWall::FindCharacters(vec2 Pos0, vec2 Pos1, float Radius, CCharacter **ppChars, int Max)
{
	int Num = 0;
	CCharacter *pCh = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; pCh; pCh = (CCharacter *)pCh->TypeNext())
	{
		vec2 IntersectPos = Pos0;
		if(Pos0 != Pos1)
			IntersectPos = closest_point_on_line(Pos0, Pos1, pCh->m_Pos);
		if(distance(pCh->m_Pos, IntersectPos) < pCh->m_ProximityRadius+Radius)
		{
			if(ppChars)
				ppChars[Num] = pCh;
			Num++;
			if(Num == Max)
				break;
		}
	}
	return Num;
}

void CWall::Tick()
{
	if (!GameServer()->GetPlayerChar(m_Owner) || GameServer()->m_apPlayers[m_Owner]->GetTeam() == TEAM_RED 
			|| GameServer()->m_apPlayers[m_Owner]->GetTeam() == TEAM_SPECTATORS || !m_Time) 
		return Reset();

	m_Time--;
	if (GameServer()->Collision()->IntersectLine(m_Pos, m_From, &m_From, 0,false))
	{
		m_OwnerChar->GiveWeapon(WEAPON_RIFLE, 2);
		return Reset();
	}

	vec2 At;
	CCharacter *Hit = GameServer()->m_World.IntersectCharacter(m_Pos, m_From, 6.f, At, NULL);
	CCharacter *Hit2 = GameServer()->m_World.IntersectCharacter(m_From, m_Pos, 6.f, At, NULL);
	if(!Hit)
		return;

	HitCharacter(Hit);
	HitCharacter(Hit2);
}
void CWall::Snap(int SnappingClient)
{
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_From.x;
	pObj->m_FromY = (int)m_From.y;
	if(SnappingClient == m_Owner)
		pObj->m_StartTick = Server()->Tick()-3;
	else
		pObj->m_StartTick = Server()->Tick()-1;

	CNetObj_Laser *pObj2 = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID2, sizeof(CNetObj_Laser)));
	if (!pObj2)
		return;

	pObj2->m_FromX = (int)m_From.x;
	pObj2->m_FromY = (int)m_From.y;
	pObj2->m_X = (int)m_From.x;
	pObj2->m_Y = (int)m_From.y;
	if(SnappingClient == m_Owner)
		pObj2->m_StartTick = Server()->Tick()-3;
	else
		pObj2->m_StartTick = Server()->Tick()-1;
}
