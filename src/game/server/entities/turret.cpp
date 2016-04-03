#include <base/math.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#define M_PI 3.14159265358979323846

#include "turret.h"
#include "projectile.h"
#include "wall.h"
#include "hearth.h"

CTurret::CTurret(CGameWorld *pGameWorld, vec2 Pos, int Owner, int Type, vec2 PosL1, vec2 PosL2)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TURRET)
{
	m_Pos = Pos;	
	m_Pos1L = PosL1;
	m_Pos2L = PosL2;
	m_Owner = Owner;
	m_Type = Type;
	SavePosion = vec2(0, 0);
	Direction = GameServer()->GetPlayerChar(m_Owner)->m_ActivTurs;

	m_ReloadTick = 0;
	m_RegenTime = 3000 * Server()->TickSpeed() / (1000 + GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretShotgun * 27);
	m_Ammo = 1+GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretAmmo;	
   
	for (unsigned i = 0; i < sizeof(m_inIDs) / sizeof(int); i ++)
        m_inIDs[i] = Server()->SnapNewID();

	m_IDC = Server()->SnapNewID();
	m_IDS = Server()->SnapNewID();
	m_IDG = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);	
}

void CTurret::Reset() 
{	
	GameServer()->m_World.DestroyEntity(this);
	for (unsigned i = 0; i < sizeof(m_inIDs) / sizeof(int); i ++)
	{
		if(m_inIDs[i] >= 0){
			Server()->SnapFreeID(m_inIDs[i]);
			m_inIDs[i] = -1;
		}
	}
	if(m_IDS >= 0){
		Server()->SnapFreeID(m_IDS);
		m_IDS = -1;
	}
	if(m_IDC >= 0){
		Server()->SnapFreeID(m_IDC);
		m_IDC = -1;
	}
	if(m_IDG >= 0){
		Server()->SnapFreeID(m_IDG);
		m_IDG = -1;
	}

	if (GameServer()->GetPlayerChar(m_Owner))
	{
		if (m_Type == WEAPON_GUN)
			GameServer()->GetPlayerChar(m_Owner)->m_TurretActive[0] = false;
		else if (m_Type == WEAPON_SHOTGUN)
			GameServer()->GetPlayerChar(m_Owner)->m_TurretActive[1] = false;
	}
}

void CTurret::Tick() 
{
	if (!GameServer()->GetPlayerChar(m_Owner) || !GameServer()->GetPlayerChar(m_Owner)->IsAlive() || GameServer()->m_apPlayers[m_Owner]->GetTeam() == TEAM_RED
			|| GameServer()->m_apPlayers[m_Owner]->GetTeam() == TEAM_SPECTATORS) 
		return Reset();	
	
	m_RegenTime--;
	if (m_RegenTime < -1) 
	{
		m_Ammo++;
		m_RegenTime = 3000 * Server()->TickSpeed() / (1000 + GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretShotgun * 27);
	}

	if (m_Type == WEAPON_GRENADE)
	{
		if(distance(m_Pos2L, m_Pos) < 5) BackSpeed = true;
		if(BackSpeed)
		{
			SavePosion = normalize(m_Pos1L - m_Pos);
			if(distance(m_Pos1L, m_Pos) < 5) BackSpeed = false;
		}
		else SavePosion = normalize(m_Pos2L - m_Pos);
		
		m_Pos += SavePosion*2;
		if (m_ReloadTick)
		{
			m_ReloadTick--;
			return;
		}
		if (!m_Ammo)
			return;

		GameServer()->GetPlayerChar(m_Owner)->GrenadeFire(m_Pos);
		m_ReloadTick = 3000 * Server()->TickSpeed() / (1000 + GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretSpeed * 40), m_Ammo--;
		return;
	}
	Fire();
}

void CTurret::Fire() 
{       
    CCharacter *pTarget = 0;
    CCharacter *pClosest = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER);
    int D = 400.0f+GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretRange;
    while (pClosest) 
	{
        int Dis = distance(pClosest->m_Pos, m_Pos);
        if (Dis <= D && pClosest->GetPlayer()->GetTeam() == TEAM_RED) 
		{
            if (!GameServer()->Collision()->IntersectLine(m_Pos, pClosest->m_Pos, 0, 0, false)) 
			{
                pTarget = pClosest;
                D = Dis;
            }
        }
        pClosest = (CCharacter *)pClosest->TypeNext();
    }

	if (!pTarget)
	{	
		if(m_Type == WEAPON_HAMMER && SavePosion != m_Pos) 
			SavePosion = m_Pos;
		
		return;
	}

	Direction = normalize(pTarget->m_Pos - m_Pos);
	vec2 ProjStartPos = m_Pos + Direction*m_ProximityRadius*0.75f;
	
	if (m_ReloadTick)
	{
		m_ReloadTick--; 
		return;
	}

	if (!m_Ammo)
		return;

	switch(m_Type){
		case WEAPON_GUN:
		{
			ExperienceTAdd();
			new CProjectile(GameWorld(), WEAPON_GUN, m_Owner, ProjStartPos, Direction, (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime), 2+GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretDmg/5, 0, 22, -1, WEAPON_GUN);
			GameServer()->CreateSound(m_Pos, SOUND_GUN_FIRE);
			m_ReloadTick = 3600 * Server()->TickSpeed() / (1000 + GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretSpeed * 40), m_Ammo--;
		}break;
		case WEAPON_HAMMER:
		{
			if (distance(pTarget->m_Pos, m_Pos) > 30 && distance(pTarget->m_Pos, m_Pos) < 300)
			{
				SavePosion = pTarget->m_Pos;
				pTarget->m_Core.m_Vel -= Direction * 1.5;
				return;
			}
			if(distance(pTarget->m_Pos, m_Pos) < 30)
			{
				GameServer()->CreateHammerHit(pTarget->m_Pos);
				
				pTarget->TakeDamage(Direction * 2, 800 + GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretDmg*2, m_Owner, WEAPON_HAMMER);
				SavePosion = m_Pos;
				m_ReloadTick = 800;	
				return;
			}
			SavePosion = m_Pos;
		}break;
		case WEAPON_RIFLE:
		{
			vec2 Intersection;
			CCharacter *pTargetChr = GameServer()->m_World.IntersectCharacter(m_Pos1L, m_Pos2L, 1.0f, Intersection, 0x0);
			if (pTargetChr)
			{
				if (pTargetChr->GetPlayer()->GetTeam() == TEAM_RED)
				{
					new CWall(GameWorld(), m_Pos1L, m_Pos2L, m_Owner, 7);
					GameServer()->CreateSound(m_Pos, 8);
					m_ReloadTick = 1800;
				}
			}
			
			CLifeHearth *pClosest = (CLifeHearth *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_FLAG);
			for (; pClosest; pClosest = (CLifeHearth *)pClosest->TypeNext())
			{
				vec2 IntersectPoss = closest_point_on_line(m_Pos1L, m_Pos2L, pClosest->m_Pos);
				if (distance(pClosest->m_Pos, IntersectPoss) < 50)
				{
					if (GameServer()->GetPlayerChar(pClosest->m_Owner))
					{
						if(!g_Config.m_SvDestroyWall)
							continue;
					
						if(g_Config.m_SvChatDestroyWall)
						{
							char aBuf[64];
							str_format(aBuf, sizeof(aBuf), "%s destroyed turret wall hearted", Server()->ClientName(m_Owner));
							GameServer()->SendChatTarget(-1, aBuf);
						}
					
						GameServer()->CreateSound(pClosest->m_Pos, 35);
						GameServer()->CreateDeath(pClosest->m_Pos, m_Owner);
						GameServer()->CreateExplosion(pClosest->m_Pos, m_Owner, WEAPON_GRENADE, true, -1, -1);
						GameServer()->CreateSound(pClosest->m_Pos, 6);
						GameServer()->CreateExplosion(m_Pos1L, m_Owner, WEAPON_GRENADE, true, -1, -1);
						GameServer()->CreateSound(m_Pos1L, 6);
						GameServer()->CreateExplosion(m_Pos2L, m_Owner, WEAPON_GRENADE, true, -1, -1);
						GameServer()->CreateSound(m_Pos2L, 6);
						GameServer()->m_apPlayers[pClosest->m_Owner]->m_LifeActives = false;
						pClosest->Reset();
						Reset();
					}
				}
			}
		}break;
		case WEAPON_SHOTGUN:
		{
			int ShotSpread = 5 + GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretLevel / 20;

			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(ShotSpread / 2 * 2 + 1);

			float Spreading[16 * 2 + 1];
			for (int i = 0; i < 16 * 2 + 1; i++)
				Spreading[i] = -0.8f + 0.05f * i;

			for (int i = -ShotSpread / 2; i <= ShotSpread / 2; ++i)
			{
				float a = GetAngle(Direction);
				a += Spreading[i + 16];
				float v = 1 - (absolute(i) / (float)ShotSpread) / 2;
				float Speed = GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretLevel > 25 ? 1.0f : mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
				new CProjectile(GameWorld(), WEAPON_SHOTGUN, m_Owner, ProjStartPos, vec2(cosf(a), sinf(a))*Speed, (int)(Server()->TickSpeed()*1.5), 1+GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretDmg/15, 0, 1, -1, WEAPON_SHOTGUN);

			}
			ExperienceTAdd();
			Server()->SendMsg(&Msg, 0, m_Owner);
			GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
			m_ReloadTick = 3800 * Server()->TickSpeed() / (1000 + GameServer()->m_apPlayers[m_Owner]->m_AccData.m_TurretSpeed * 40), m_Ammo--;
		}break;
	}
}

void CTurret::ExperienceTAdd()
{
	CPlayer* pPlayer = GameServer()->m_apPlayers[m_Owner];
	pPlayer->m_AccData.m_TurretExp += rand()%3+1;
	if (pPlayer && pPlayer->m_AccData.m_TurretExp >= pPlayer->m_AccData.m_TurretLevel)
	{
		pPlayer->m_AccData.m_TurretLevel++;
		pPlayer->m_AccData.m_TurretExp = 0;
		pPlayer->m_AccData.m_TurretMoney++;

		if(pPlayer->m_AccData.m_UserID)
			pPlayer->m_pAccount->Apply();

		char SendExp[64];
		str_format(SendExp, sizeof(SendExp), "Turret's Level-Up! Your turret's level now is: %d", pPlayer->m_AccData.m_TurretLevel);
		GameServer()->SendChatTarget(m_Owner, SendExp);
	}
}

void CTurret::Snap(int SnappingClient)
{	
	if (NetworkClipped(SnappingClient))
		return;

    int InSize = sizeof(m_inIDs) / sizeof(int);
	for (int i = 0; i < InSize; i ++) 
	{
        CNetObj_Projectile *pEff = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_inIDs[i], sizeof(CNetObj_Projectile)));
		if (!pEff)
            continue;

		if(m_ReloadTick && m_Type == WEAPON_HAMMER){
			pEff->m_X = (int)(cos(GetAngle(Direction)+M_PI/9*i*4)*(16.0+m_ReloadTick/65)+m_Pos.x);
			pEff->m_Y = (int)(sin(GetAngle(Direction)+M_PI/9*i*4)*(16.0+m_ReloadTick/65)+m_Pos.y);
		}
		else if(m_ReloadTick && m_Type == WEAPON_RIFLE){
			pEff->m_X = (int)(cos(M_PI/9*i*4)*(16.0+m_ReloadTick/110)+m_Pos.x);
			pEff->m_Y = (int)(sin(M_PI/9*i*4)*(16.0+m_ReloadTick/110)+m_Pos.y);
		}
		else
		{
			if(m_Ammo < 5)
			{
				pEff->m_X = (int)(cos(GetAngle(Direction)+M_PI/9*i*4)*(16.0+m_ReloadTick/20)+m_Pos.x);
				pEff->m_Y = (int)(sin(GetAngle(Direction)+M_PI/9*i*4)*(16.0+m_ReloadTick/20)+m_Pos.y);
			}
			else
			{
				pEff->m_X = (int)(cos(GetAngle(Direction)+M_PI/9*i*4)*16.0+m_Pos.x);
				pEff->m_Y = (int)(sin(GetAngle(Direction)+M_PI/9*i*4)*16.0+m_Pos.y);
			}
		}
		pEff->m_StartTick = Server()->Tick()-2;
		pEff->m_Type = WEAPON_SHOTGUN;
    }
	
	//laserpos1
	CNetObj_Laser *pObj2 = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDC, sizeof(CNetObj_Laser)));
	if (!pObj2)
		return;

	if (m_Type == WEAPON_GRENADE){
		pObj2->m_X = (int)m_Pos1L.x;
		pObj2->m_Y = (int)m_Pos1L.y;
	}
	else{
		pObj2->m_X = (int)m_Pos.x;
		pObj2->m_Y = (int)m_Pos.y;
	}
	
	if (m_Type == WEAPON_HAMMER){
		pObj2->m_FromX = (int)SavePosion.x;
		pObj2->m_FromY = (int)SavePosion.y;
	}
	else if (m_Type == WEAPON_GRENADE){
		pObj2->m_FromX = (int)m_Pos2L.x;
		pObj2->m_FromY = (int)m_Pos2L.y;
	}
	else if (m_Type == WEAPON_RIFLE){
		Direction = normalize(m_Pos2L - m_Pos);
		pObj2->m_FromX = (int)m_Pos.x + Direction.x * 32;
		pObj2->m_FromY = (int)m_Pos.y + Direction.y * 32;
	}
	else{
		pObj2->m_FromX = (int)m_Pos.x + Direction.x * 32;
		pObj2->m_FromY = (int)m_Pos.y + Direction.y * 32;
	}

	if (pObj2->m_FromX < 1 && pObj2->m_FromY < 1)
	{
		if (m_Type == WEAPON_GRENADE) {
			pObj2->m_FromX = (int)m_Pos2L.x;
			pObj2->m_FromY = (int)m_Pos2L.y;
		}
		else {
			pObj2->m_FromX = (int)m_Pos.x;
			pObj2->m_FromY = (int)m_Pos.y;
		}
	}
	
	if (m_Type == WEAPON_HAMMER) pObj2->m_StartTick = Server()->Tick() - 5;
	else if (m_Type == WEAPON_GRENADE) pObj2->m_StartTick = Server()->Tick() - 2;
	else pObj2->m_StartTick = Server()->Tick();
	
	//laserpos2
	CNetObj_Laser *pObj3 = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDG, sizeof(CNetObj_Laser)));
	if (!pObj3)
		return;

	if(m_Type == WEAPON_GRENADE)
	{

		pObj3->m_X = (int)m_Pos2L.x;
		pObj3->m_Y = (int)m_Pos2L.y;
		pObj3->m_FromX = (int)m_Pos2L.x;
		pObj3->m_FromY = (int)m_Pos2L.y;
		if (pObj3->m_FromX < 1 && pObj3->m_FromY < 1){
			pObj3->m_FromX = (int)m_Pos2L.x;
			pObj3->m_FromY = (int)m_Pos2L.y;
		}
		pObj3->m_StartTick = Server()->Tick()-2;
	}
	
	//projcent
	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDS, sizeof(CNetObj_Projectile)));
	if (!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_StartTick = Server()->Tick() - 3;
	pObj->m_Type = m_Type;
}
