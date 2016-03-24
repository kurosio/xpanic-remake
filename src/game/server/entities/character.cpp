/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/mapitems.h>
#include <engine/server/server.h>
#include <game/server/gamemodes/DDRace.h>

#include "character.h"
#include "projectile.h"

#include "zdoor.h"
#include "wall.h"
#include "mine.h"
#include "turret.h"

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

CCharacter::CCharacter(CGameWorld *pWorld)
: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER)
{
	m_ProximityRadius = ms_PhysSize;
	m_Health = 0;
	m_Armor = 0;
	m_InvisID = Server()->SnapNewID();
}

void CCharacter::Reset(){
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = m_LastAction = m_LastNoAmmoSound = m_QueuedWeapon = -1;
	m_LastWeapon = WEAPON_HAMMER;

	m_pPlayer = pPlayer;
	m_Pos = Pos;	
	m_OldPos = Pos;

	m_Core.Reset();
	m_Core.m_Id = GetPlayer()->GetCID();
	m_Core.Init(&GameServer()->m_World.m_Core, GameServer()->Collision(), &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.m_Core, &((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts);
	m_Core.m_Pos = m_Pos;
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;
	
	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameServer()->m_World.InsertEntity(this);
	m_Alive = true;

	m_LastRefillJumps = m_HittingDoor = m_iVisible = m_TypeHealthCh = false;
	m_TurretActive[0] = m_TurretActive[1] = m_TurretActive[2] = m_TurretActive[3] = m_TurretActive[4] = false;
	
	m_SuperJump = false;
	m_PrevPos = m_Pos;
	m_TeleCheckpoint = 0;
	m_BurnedFrom = pPlayer->GetCID();
	m_PushDirection = vec2(0, 0);
	
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(Pos));
	m_TuneZoneOld = -1; // no zone leave msg on spawn
	m_NeededFaketuning = 0; // reset fake tunings on respawn and send the client
	SendZoneMsgs(); // we want a entermessage also on spawn
	GameServer()->SendTuningParams(pPlayer->GetCID(), m_TuneZone);
	m_HeartTick = 1*Server()->TickSpeed();
	GameServer()->m_pController->OnCharacterSpawn(this);

	if (pPlayer->GetTeam() == TEAM_RED) m_Core.m_ActiveWeapon = WEAPON_HAMMER;
	else if (pPlayer->GetTeam() == TEAM_BLUE) m_Core.m_ActiveWeapon = WEAPON_GUN;

	m_Core.m_Jumps = 2 + pPlayer->m_JumpsShop;
	if (pPlayer->m_AccData.m_Level >= 50 && pPlayer->m_AccData.m_Level <= 99) m_mAmmo = 20 + pPlayer->m_AccData.m_Ammo;
	else if (pPlayer->m_AccData.m_Level >= 100) m_mAmmo = 30 + pPlayer->m_AccData.m_Ammo;
	else m_mAmmo = 10 + pPlayer->m_AccData.m_Ammo;

	GameServer()->SendBroadcast("", pPlayer->GetCID());
	return true;
}

void CCharacter::Destroy()
{
	if(m_InvisID >= 0)
	{
		Server()->SnapFreeID(m_InvisID);
		m_InvisID = -1;
	}
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_Core.m_ActiveWeapon || m_pPlayer->GetTeam() == TEAM_RED)
		return;

	m_LastWeapon = m_Core.m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_Core.m_ActiveWeapon = W;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

	if(m_Core.m_ActiveWeapon < 0 || m_Core.m_ActiveWeapon >= NUM_WEAPONS)
		m_Core.m_ActiveWeapon = 0;
}

bool CCharacter::IsGrounded()
{
	if(GameServer()->Collision()->CheckPoint(m_Pos.x+m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;
	if(GameServer()->Collision()->CheckPoint(m_Pos.x-m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;

	int index = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y+m_ProximityRadius/2+4));
	int tile = GameServer()->Collision()->GetTileIndex(index);
	int flags = GameServer()->Collision()->GetTileFlags(index);
	if(tile == TILE_STOPA || (tile == TILE_STOP && flags == ROTATION_0) || (tile ==TILE_STOPS && (flags == ROTATION_0 || flags == ROTATION_180)))
		return true;
	tile = GameServer()->Collision()->GetFTileIndex(index);
	flags = GameServer()->Collision()->GetFTileFlags(index);
	if(tile == TILE_STOPA || (tile == TILE_STOP && flags == ROTATION_0) || (tile ==TILE_STOPS && (flags == ROTATION_0 || flags == ROTATION_180)))
		return true;

	return false;
}

void CCharacter::DoWeaponSwitch()
{
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_aWeapons[WEAPON_NINJA].m_Got)
		return;

	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_Core.m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	bool Anything = false;
	 for(int i = 0; i < NUM_WEAPONS - 1; ++i)
		 if(m_aWeapons[i].m_Got)
			 Anything = true;
	 if(!Anything)
		 return;

	if(m_pPlayer->m_AccData.m_Freeze)
		return;
	 
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			if (!m_pPlayer->m_LifeActives && m_pPlayer->GetTeam() == TEAM_RED && g_Config.m_SvNewHearth)
				m_TypeHealthCh = true;

			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			if (!m_pPlayer->m_LifeActives && m_pPlayer->GetTeam() == TEAM_RED && g_Config.m_SvNewHearth)
				m_TypeHealthCh = false;

			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_Core.m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::GrenadeFire(vec2 Pos)
{
	vec2 Direction = normalize(m_Pos - Pos);
	vec2 ProjStartPos = Pos + Direction;

	new CProjectile(GameWorld(), WEAPON_GRENADE, m_pPlayer->GetCID(), ProjStartPos, Direction, (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime), 1, true, 17, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);
	GameServer()->CreateSound(Pos, SOUND_GRENADE_FIRE);
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0 || m_pPlayer->m_AccData.m_Freeze)
		return;
	
	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if(m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_GUN ||
		m_Core.m_ActiveWeapon == WEAPON_HAMMER && m_pPlayer->m_AccData.m_Level >= 10 || m_Core.m_ActiveWeapon == WEAPON_GUN)
		FullAuto = true;

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire&1) && m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	if(!m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
	{
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);
		return;
	}

	vec2 ProjStartPos = m_Pos+Direction*m_ProximityRadius*0.75f;
	switch(m_Core.m_ActiveWeapon)
	{
		case WEAPON_HAMMER:
		{
			m_NumObjectsHit = 0;
			GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			CCharacter *apEnts[MAX_CLIENTS];
			int Hits = 0;
			if (m_pPlayer->GetTeam() == TEAM_RED)
			{
				int rands = rand() % 150;
				if (rands == 15) 
					new CMine(GameWorld(), m_Pos, m_pPlayer->GetCID());

				if (!m_pPlayer->m_LifeActives && !m_HeartTick && (m_TypeHealthCh && g_Config.m_SvNewHearth || !g_Config.m_SvNewHearth))
					m_pPlayer->m_LifeActives = true;
				
				CTurret *pClosest = (CTurret *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_TURRET);
				while (pClosest) 
				{
					if (distance(pClosest->m_Pos, m_Pos) <= 25) 
					{
						if (!GameServer()->Collision()->IntersectLine(m_Pos, pClosest->m_Pos, 0, 0, false)) 
						{
							if(GameServer()->GetPlayerChar(pClosest->m_Owner))
								GameServer()->CreateSoundGlobal(35, pClosest->m_Owner);
							
							GameServer()->CreateHammerHit(pClosest->m_Pos);
							pClosest->Reset();
							ExperienceAdd(1, m_pPlayer->GetCID());
						}
					}
					pClosest = (CTurret *)pClosest->TypeNext();
				}
				
				int Num = GameServer()->m_World.FindEntities(ProjStartPos, m_ProximityRadius*(m_pPlayer->m_RangeShop?2.00f:0.76f), (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
				for (int i = 0; i < Num; ++i)
				{
					CCharacter *pTarget = apEnts[i];

					if (pTarget == this || (pTarget->IsAlive() && GameServer()->Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL, false)) || pTarget->m_pPlayer->GetTeam() == TEAM_RED)
						continue;

					if (length(pTarget->m_Pos - ProjStartPos) > 0.0f)
						GameServer()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos)*m_ProximityRadius*0.5f, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
					else
						GameServer()->CreateHammerHit(ProjStartPos, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

					if (!pTarget->IhammerTick)
						pTarget->m_pPlayer->SetZomb(m_pPlayer->GetCID());
					
					Hits++;
				}
			}
			else
			{
				if (!IhammerRelTick)
				{
					GameServer()->CreatePlayerSpawn(m_Pos);
					IhammerTick = 5 * Server()->TickSpeed();
					IhammerRelTick = 30 * Server()->TickSpeed();
				}

				int Num = GameServer()->m_World.FindEntities(ProjStartPos, m_ProximityRadius*0.5f, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
				for (int i = 0; i < Num; ++i)
				{
					CCharacter *pTarget = apEnts[i];

					if ((pTarget == this || (pTarget->IsAlive() && !CanCollide(pTarget->GetPlayer()->GetCID())) || pTarget->m_pPlayer->GetTeam() != TEAM_RED))
						continue;

					if (length(pTarget->m_Pos - ProjStartPos) > 0.0f)
						GameServer()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos)*m_ProximityRadius*0.5f, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
					else
						GameServer()->CreateHammerHit(ProjStartPos, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

					pTarget->TakeDamage(vec2(0,0), 3+m_pPlayer->m_AccData.m_Dmg, m_pPlayer->GetCID(), m_Core.m_ActiveWeapon);
					Hits++;
				}
			}
			if(Hits)
				m_ReloadTimer = Server()->TickSpeed()/3;
		} break;

		case WEAPON_GUN:
		{
			new CProjectile(GameWorld(), WEAPON_GUN, m_pPlayer->GetCID(), ProjStartPos, Direction, (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime), 2+m_pPlayer->m_AccData.m_Dmg, m_pPlayer->m_KillingSpree >= g_Config.m_SvKillingSpree & g_Config.m_SvKillingSpree ? true : false, 22, -1, WEAPON_GUN);
			GameServer()->CreateSound(m_Pos, SOUND_GUN_FIRE);
		} break;

		case WEAPON_SHOTGUN:
		{
			int ShotSpread = 5 + m_pPlayer->m_AccData.m_Level / 10;
			if(ShotSpread > 15)
			{
				ShotSpread = 15 + m_pPlayer->m_AccData.m_Level / 70;
				if(ShotSpread > 36)
					ShotSpread = 36;
			}
			
			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(ShotSpread / 2 * 2 + 1);

			float Spreading[20 * 2 + 1];
			for (int i = 0; i < 20 * 2 + 1; i++)
				Spreading[i] = -1.2f + 0.06f * i;

			for (int i = -ShotSpread / 2; i <= ShotSpread / 2; ++i)
			{
				float a = GetAngle(Direction);
				a += Spreading[i + 20];
				float v = 1 - (absolute(i) / (float)ShotSpread) / 2;
				float Speed = m_pPlayer->m_AccData.m_Level > 19 ? 1.2f : mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.2f, v);
				new CProjectile(GameWorld(), WEAPON_SHOTGUN, m_pPlayer->GetCID(), ProjStartPos, vec2(cosf(a), sinf(a))*Speed, (int)(Server()->TickSpeed()*1.5), 1+m_pPlayer->m_AccData.m_Dmg/5, 0, 1, -1, WEAPON_SHOTGUN);
			}
			Server()->SendMsg(&Msg, 0, m_pPlayer->GetCID());
			GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
		} break;

		case WEAPON_GRENADE:
		{
			new CProjectile(GameWorld(), WEAPON_GRENADE, m_pPlayer->GetCID(), ProjStartPos, Direction, (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime), 0, true, 17, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);
			GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		} break;

		case WEAPON_RIFLE:
		{
			vec2 To;
			if (m_RiflePos != vec2(0, 0))
			{
				if (m_RiflePos == m_Pos)
				{
					m_aWeapons[WEAPON_RIFLE].m_Ammo = 2;
					GameServer()->SendChatTarget(m_pPlayer->GetCID(), "The second point can not be set here");
					m_RiflePos = vec2(0, 0);
					return;
				}
				new CWall(GameWorld(), m_RiflePos, m_Pos, m_pPlayer->GetCID(), 8);
			}
			else m_RiflePos = m_Pos;
		} break;

		case WEAPON_NINJA:
			break;

	}

	m_AttackTick = Server()->Tick();

	if(m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
		m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo--;

	if(!m_ReloadTimer)
	{
		float FireDelay;
		if (!m_TuneZone)
			GameServer()->Tuning()->Get(38 + m_Core.m_ActiveWeapon, &FireDelay);
		else
			GameServer()->TuningList()[m_TuneZone].Get(38 + m_Core.m_ActiveWeapon, &FireDelay);
		
		m_ReloadTimer = FireDelay * Server()->TickSpeed() / (1000 + m_pPlayer->m_AccData.m_Handle * 16);
	}
}

void CCharacter::HandleWeapons()
{
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}
	FireWeapon();

	if (m_pPlayer->m_AccData.m_Ammoregen >= 1 && m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo < m_mAmmo + m_pPlayer->m_AccData.m_Ammo && (m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_GRENADE))
	{
		if (m_RegenTime) m_RegenTime--;
		else
		{
			m_RegenTime = 160 - m_pPlayer->m_AccData.m_Ammoregen * 2 - 5;
			m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo += 1;
		}
	}

	int AmmoRegenTime = g_pData->m_Weapons.m_aId[m_Core.m_ActiveWeapon].m_Ammoregentime;
	if(AmmoRegenTime)
	{
		if (m_ReloadTimer <= 0)
		{
			if (m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart < 0)
				m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick();

			if ((Server()->Tick() - m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
			{
				m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo = min(m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo + 1, g_pData->m_Weapons.m_aId[m_Core.m_ActiveWeapon].m_Maxammo);
				m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
			}
		}
		else
			m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
	}
	return;
}

bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	if (Weapon == WEAPON_RIFLE && !m_aWeapons[Weapon].m_Ammo || Weapon != WEAPON_RIFLE && m_aWeapons[Weapon].m_Ammo < 5 || !m_aWeapons[Weapon].m_Got)
	{
		m_aWeapons[Weapon].m_Got = true;
		if (Weapon == WEAPON_RIFLE) m_aWeapons[Weapon].m_Ammo = min((2), Ammo);
		else m_aWeapons[Weapon].m_Ammo = min(m_mAmmo+m_pPlayer->m_AccData.m_Ammo, Ammo);

		return true;
	}
	return false;
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire&1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Tick()
{
	DDRaceTick();

	m_Core.m_Input = m_Input;
	m_Core.Tick(true, false);

	HandleWeapons();
	DDRacePostCoreTick();
	m_ActivTurs = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));
	
	if (m_HeartTick)
		m_HeartTick--;
	if (IhammerRelTick)
		IhammerRelTick--;
	if (IhammerTick)
	{
		if (ITickSecond <= 0)
			ITickSecond = 50;

		ITickSecond--;
		IhammerTick--;

		char aBuf[26];
		str_format(aBuf, sizeof(aBuf), "\n\n\n\n\n\n\n\n\n\n\n\n\nInvis: %d.%d", IhammerTick/Server()->TickSpeed() , ITickSecond*2);
		GameServer()->SendBroadcast(aBuf, m_pPlayer->GetCID());
		if (!IhammerTick)
		{
			GameServer()->SendBroadcast(" ", m_pPlayer->GetCID());
			GameServer()->CreatePlayerSpawn(m_Pos);
		}
	}

	if (m_BurnTick)
	{
		m_BurnTick--;
		if (m_pPlayer->GetTeam() == TEAM_BLUE)
		{
			m_Core.m_Vel *= 0.5;
			if (m_BurnTick % 20 == 0)
			{
				GameServer()->CreateExplosion(m_Core.m_Pos, m_pPlayer->GetCID(), WEAPON_GRENADE, true, -1, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
				TakeDamage(vec2(0, 0), 1, m_BurnedFrom, WEAPON_GRENADE);
			}
		}
	}

	if(m_HittingDoor)
	{
		m_Core.m_Vel += m_PushDirection*length(m_Core.m_Vel);
		if(m_Core.m_Jumped&3) m_Core.m_Jumped &= ~2;
	}
	else if(!m_HittingDoor) m_OldPos = m_Core.m_Pos;
	m_HittingDoor = false;
	
	m_PrevInput = m_Input;
	m_PrevPos = m_Core.m_Pos;
	return;
}

void CCharacter::TickDefered()
{
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision(), &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.m_Core, &((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts);
		m_ReckoningCore.m_Id = m_pPlayer->GetCID();
		m_ReckoningCore.Tick(false, false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));

	m_Core.m_Id = m_pPlayer->GetCID();
	m_Core.Move();
	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	int Events = m_Core.m_TriggeredEvents;
	//int Mask = CmaskAllExceptOne(m_pPlayer->GetCID());

	if(Events&COREEVENT_GROUND_JUMP) GameServer()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, Teams()->TeamMask(Team(), m_pPlayer->GetCID()));

	if(Events&COREEVENT_HOOK_ATTACH_PLAYER) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	if(Events&COREEVENT_HOOK_ATTACH_GROUND) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, Teams()->TeamMask(Team(), m_pPlayer->GetCID(), m_pPlayer->GetCID()));
	if(Events&COREEVENT_HOOK_HIT_NOHOOK) GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, Teams()->TeamMask(Team(), m_pPlayer->GetCID(), m_pPlayer->GetCID()));


	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_Core.m_pReset || m_ReckoningTick+Server()->TickSpeed()*3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
			m_Core.m_pReset = false;
		}
	}
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_DamageTakenTick;
	++m_ReckoningTick;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart > -1)
		++m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
}

void CCharacter::ExperienceAdd(int Exp, int ClientID)
{
	CPlayer* pPlayer = GameServer()->m_apPlayers[ClientID];
	if(m_pPlayer) pPlayer->m_AccData.m_Exp += m_pPlayer->m_AccData.m_Freeze?0:Exp;
	else pPlayer->m_AccData.m_Exp += Exp;
	
	if (pPlayer->m_AccData.m_Exp >= pPlayer->m_AccData.m_Level && pPlayer && GameServer()->GetPlayerChar(ClientID))
	{
		m_EmoteType = EMOTE_HAPPY, m_EmoteStop = Server()->Tick() + Server()->TickSpeed();
	}
	else
	{
		char SendExp[64];
		str_format(SendExp, sizeof(SendExp), "Exp %d/%d", pPlayer->m_AccData.m_Exp, pPlayer->m_AccData.m_Level);
		GameServer()->SendChatTarget(ClientID, SendExp);
	}
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= 12000)
		return false;
	m_Health = clamp(m_Health+Amount, 0, 12000);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= 10)
		return false;
	m_Armor = clamp(m_Armor+Amount, 0, 10);
	return true;
}

void CCharacter::Die(int Killer, int Weapon)
{
	m_pPlayer->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	int ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		Killer, Server()->ClientName(Killer),
		m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	if (m_pPlayer->GetTeam() == TEAM_BLUE)
	{
		if(GameServer()->m_pController->ZombStarted() && !GameServer()->m_pController->m_Warmup)
			m_pPlayer->SetZomb(m_pPlayer->GetCID());
	}
	else
	{
		CNetMsg_Sv_KillMsg Msg;
		Msg.m_Killer = Killer;
		Msg.m_Victim = m_pPlayer->GetCID();
		Msg.m_Weapon = Weapon;
		Msg.m_ModeSpecial = ModeSpecial;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	m_pPlayer->m_DieTick = Server()->Tick();
	
	m_Alive = false;
	GameServer()->m_World.RemoveEntity(this);
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	if(From == m_pPlayer->GetCID() && m_pPlayer->GetTeam() == TEAM_BLUE && g_Config.m_SvJumpGrenadeHuman)
		m_Core.m_Vel += Force;

	if (m_pPlayer->GetTeam() == TEAM_BLUE)
		return false;

	if (Weapon == WEAPON_SHOTGUN)
		m_Core.m_Vel += Force*3.60f;
	else if (Weapon == WEAPON_GUN)
		m_Core.m_Vel += Force*0.50f;
	else if (Weapon == WEAPON_GRENADE)
		m_Core.m_Vel += Force*2.10f;
	else 
		m_Core.m_Vel += Force;

	if(GameServer()->m_pController->IsFriendlyFire(m_pPlayer->GetCID(), From))
		return false;

	m_DamageTaken++;

	if(Server()->Tick() < m_DamageTakenTick+25)
		GameServer()->CreateDamageInd(m_Pos, m_DamageTaken*0.25f, (Dmg > 50) ? 25 : Dmg);
	else
	{
		m_DamageTaken = 0;
		GameServer()->CreateDamageInd(m_Pos, 0, (Dmg > 50) ? 25 : Dmg);
	}

	if (Dmg)
		m_Health -= Dmg;
	
	m_DamageTakenTick = Server()->Tick();

	if(From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
	{
		int64_t Mask = CmaskOne(From);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_SpectatorID == From)
				Mask |= CmaskOne(i);
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
	}

	if(m_Health <= 0)
	{
		if (From >= 0 && m_pPlayer->GetCID() != From && GameServer()->m_apPlayers[From])
		{
			ExperienceAdd(3 + m_pPlayer->m_AccData.m_Level / 40 * g_Config.m_SvExpBonus, From);
			GameServer()->m_apPlayers[From]->m_KillingSpree++;
			if(GameServer()->m_apPlayers[From]->m_KillingSpree == g_Config.m_SvKillingSpree)
			{
				char aBuf[48];
				str_format(aBuf, sizeof(aBuf), "%s is on killing spree!", Server()->ClientName(From));
				GameServer()->SendChatTarget(-1, aBuf);
			}
		}
		
		Die(From, Weapon);
		return false;
	}

	if (Dmg > 2) GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
	else GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);

	m_EmoteType = EMOTE_PAIN;
	m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;

	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nHealth: %d", m_Health);
	GameServer()->SendBroadcast(aBuf, m_pPlayer->GetCID());
	return true;
}

void CCharacter::SetZomb()
{
	m_Core.m_ActiveWeapon = m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;
	m_Armor = 0;
}

void CCharacter::Snap(int SnappingClient)
{
	int id = m_pPlayer->GetCID();

	if(SnappingClient > -1 && !Server()->Translate(id, SnappingClient))
		return;

	if(NetworkClipped(SnappingClient))
		return;
	
	if(SnappingClient > -1)
	{
		CCharacter* SnapChar = GameServer()->GetPlayerChar(SnappingClient);
		CPlayer* SnapPlayer = GameServer()->m_apPlayers[SnappingClient];

		if(SnapPlayer->GetTeam() == TEAM_SPECTATORS && SnapPlayer->m_SpectatorID != -1 && !CanCollide(SnapPlayer->m_SpectatorID) && !SnapPlayer->m_ShowOthers)
			return;

		if(SnapPlayer->GetTeam() != TEAM_SPECTATORS && SnapChar && !CanCollide(SnappingClient) && !SnapPlayer->m_ShowOthers)
			return;

		if(SnapPlayer->GetTeam() == TEAM_SPECTATORS && SnapPlayer->m_SpectatorID == -1 && !CanCollide(SnappingClient) && SnapPlayer->m_SpecTeam)
			return;
	}

	if (IhammerTick && m_pPlayer->GetTeam() == TEAM_BLUE && GetPlayer()->GetCID() != SnappingClient)
		return;

	if (m_iVisible && GetPlayer()->GetCID() != SnappingClient)
		return;

	if(IhammerTick && SnappingClient == m_pPlayer->GetCID())
	{
		CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_InvisID, sizeof(CNetObj_Pickup)));
		if(!pP)
			return;

		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y - 60.0;
		pP->m_Type = POWERUP_ARMOR;
		pP->m_Subtype = 0;
	}
	
	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, id, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;
	
	// write down the m_Core
	if(!m_ReckoningTick || GameServer()->m_World.m_Paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	// set emote
	if (m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = m_pPlayer->m_DefEmote;
		m_EmoteStop = -1;
	}
	pCharacter->m_Emote = m_EmoteType;

	if (pCharacter->m_HookedPlayer != -1)
	{
		if (!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
			pCharacter->m_HookedPlayer = -1;
	}

	pCharacter->m_AttackTick = m_AttackTick;
	pCharacter->m_Direction = m_Input.m_Direction;
	pCharacter->m_Weapon = m_Core.m_ActiveWeapon;
	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!g_Config.m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID))
	{
		pCharacter->m_Health = m_Health;
		pCharacter->m_Armor = m_Armor;
		if(m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
			pCharacter->m_AmmoCount = m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo;
	}

	if (GetPlayer()->m_Afk)
		pCharacter->m_Emote = EMOTE_BLINK;

	if(GetPlayer()->m_RangeShop)
		pCharacter->m_Emote = EMOTE_ANGRY;

	if (GetPlayer()->m_AccData.m_Freeze)
	{
		if (pCharacter->m_Emote == EMOTE_NORMAL)
			pCharacter->m_Emote = EMOTE_BLINK;
		
		pCharacter->m_Weapon = WEAPON_NINJA;
	}
	
	if(pCharacter->m_Emote == EMOTE_NORMAL)
	{
		if(250 - ((Server()->Tick() - m_LastAction)%(250)) < 5)
			pCharacter->m_Emote = EMOTE_BLINK;
	}
	pCharacter->m_PlayerFlags = GetPlayer()->m_PlayerFlags;
}

int CCharacter::NetworkClipped(int SnappingClient){
	return NetworkClipped(SnappingClient, m_Pos);
}

int CCharacter::NetworkClipped(int SnappingClient, vec2 CheckPos)
{
	if(SnappingClient == -1 || GameServer()->m_apPlayers[SnappingClient]->m_ShowAll)
		return 0;

	float dx = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.x-CheckPos.x;
	float dy = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.y-CheckPos.y;

	if(absolute(dx) > 1000.0f || absolute(dy) > 800.0f)
		return 1;

	if(distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, CheckPos) > 4000.0f)
		return 1;
	return 0;
}

bool CCharacter::CanCollide(int ClientID){
	return Teams()->m_Core.CanCollide(GetPlayer()->GetCID(), ClientID);
}
bool CCharacter::SameTeam(int ClientID){
	return Teams()->m_Core.SameTeam(GetPlayer()->GetCID(), ClientID);
}

int CCharacter::Team(){
	return Teams()->m_Core.Team(m_pPlayer->GetCID());
}

CGameTeams* CCharacter::Teams(){
	return &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams;
}

void CCharacter::HandleSkippableTiles(int Index)
{
	// handle death-tiles and leaving gamelayer
	if((GameServer()->Collision()->GetCollisionAt(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
			GameServer()->Collision()->GetCollisionAt(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y+m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
			GameServer()->Collision()->GetCollisionAt(m_Pos.x-m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
			GameServer()->Collision()->GetFCollisionAt(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
			GameServer()->Collision()->GetFCollisionAt(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y+m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
			GameServer()->Collision()->GetFCollisionAt(m_Pos.x-m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
			GameServer()->Collision()->GetCollisionAt(m_Pos.x-m_ProximityRadius/3.f, m_Pos.y+m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		return;
	}

	if (GameLayerClipped(m_Pos))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		return;
	}

	if(Index < 0)
		return;

	// handle speedup tiles
	if(GameServer()->Collision()->IsSpeedup(Index))
	{
		vec2 Direction, MaxVel, TempVel = m_Core.m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		GameServer()->Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed);
		if(Force == 255 && MaxSpeed)
		{
			m_Core.m_Vel = Direction * (MaxSpeed/5);
		}
		else
		{
			if(MaxSpeed > 0 && MaxSpeed < 5) MaxSpeed = 5;
			//dbg_msg("speedup tile start","Direction %f %f, Force %d, Max Speed %d", (Direction).x,(Direction).y, Force, MaxSpeed);
			if(MaxSpeed > 0)
			{
				if(Direction.x > 0.0000001f)
					SpeederAngle = -atan(Direction.y / Direction.x);
				else if(Direction.x < 0.0000001f)
					SpeederAngle = atan(Direction.y / Direction.x) + 2.0f * asin(1.0f);
				else if(Direction.y > 0.0000001f)
					SpeederAngle = asin(1.0f);
				else
					SpeederAngle = asin(-1.0f);

				if(SpeederAngle < 0)
					SpeederAngle = 4.0f * asin(1.0f) + SpeederAngle;

				if(TempVel.x > 0.0000001f)
					TeeAngle = -atan(TempVel.y / TempVel.x);
				else if(TempVel.x < 0.0000001f)
					TeeAngle = atan(TempVel.y / TempVel.x) + 2.0f * asin(1.0f);
				else if(TempVel.y > 0.0000001f)
					TeeAngle = asin(1.0f);
				else
					TeeAngle = asin(-1.0f);

				if(TeeAngle < 0)
					TeeAngle = 4.0f * asin(1.0f) + TeeAngle;

				TeeSpeed = sqrt(pow(TempVel.x, 2) + pow(TempVel.y, 2));

				DiffAngle = SpeederAngle - TeeAngle;
				SpeedLeft = MaxSpeed / 5.0f - cos(DiffAngle) * TeeSpeed;
				if(abs((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if(abs((int)SpeedLeft) > Force)
					TempVel += Direction * -Force;
				else
					TempVel += Direction * SpeedLeft;
			}
			else
				TempVel += Direction * Force;

			if(TempVel.x > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_270) || (m_TileIndexL == TILE_STOP && m_TileFlagsL == ROTATION_270) || (m_TileIndexL == TILE_STOPS && (m_TileFlagsL == ROTATION_90 || m_TileFlagsL ==ROTATION_270)) || (m_TileIndexL == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_270) || (m_TileFIndexL == TILE_STOP && m_TileFFlagsL == ROTATION_270) || (m_TileFIndexL == TILE_STOPS && (m_TileFFlagsL == ROTATION_90 || m_TileFFlagsL == ROTATION_270)) || (m_TileFIndexL == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_270) || (m_TileSIndexL == TILE_STOP && m_TileSFlagsL == ROTATION_270) || (m_TileSIndexL == TILE_STOPS && (m_TileSFlagsL == ROTATION_90 || m_TileSFlagsL == ROTATION_270)) || (m_TileSIndexL == TILE_STOPA)))
				TempVel.x = 0;
			if(TempVel.x < 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_90) || (m_TileIndexR == TILE_STOP && m_TileFlagsR == ROTATION_90) || (m_TileIndexR == TILE_STOPS && (m_TileFlagsR == ROTATION_90 || m_TileFlagsR == ROTATION_270)) || (m_TileIndexR == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_90) || (m_TileFIndexR == TILE_STOP && m_TileFFlagsR == ROTATION_90) || (m_TileFIndexR == TILE_STOPS && (m_TileFFlagsR == ROTATION_90 || m_TileFFlagsR == ROTATION_270)) || (m_TileFIndexR == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_90) || (m_TileSIndexR == TILE_STOP && m_TileSFlagsR == ROTATION_90) || (m_TileSIndexR == TILE_STOPS && (m_TileSFlagsR == ROTATION_90 || m_TileSFlagsR == ROTATION_270)) || (m_TileSIndexR == TILE_STOPA)))
				TempVel.x = 0;
			if(TempVel.y < 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_180) || (m_TileIndexB == TILE_STOP && m_TileFlagsB == ROTATION_180) || (m_TileIndexB == TILE_STOPS && (m_TileFlagsB == ROTATION_0 || m_TileFlagsB == ROTATION_180)) || (m_TileIndexB == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_180) || (m_TileFIndexB == TILE_STOP && m_TileFFlagsB == ROTATION_180) || (m_TileFIndexB == TILE_STOPS && (m_TileFFlagsB == ROTATION_0 || m_TileFFlagsB == ROTATION_180)) || (m_TileFIndexB == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_180) || (m_TileSIndexB == TILE_STOP && m_TileSFlagsB == ROTATION_180) || (m_TileSIndexB == TILE_STOPS && (m_TileSFlagsB == ROTATION_0 || m_TileSFlagsB == ROTATION_180)) || (m_TileSIndexB == TILE_STOPA)))
				TempVel.y = 0;
			if(TempVel.y > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_0) || (m_TileIndexT == TILE_STOP && m_TileFlagsT == ROTATION_0) || (m_TileIndexT == TILE_STOPS && (m_TileFlagsT == ROTATION_0 || m_TileFlagsT == ROTATION_180)) || (m_TileIndexT == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_0) || (m_TileFIndexT == TILE_STOP && m_TileFFlagsT == ROTATION_0) || (m_TileFIndexT == TILE_STOPS && (m_TileFFlagsT == ROTATION_0 || m_TileFFlagsT == ROTATION_180)) || (m_TileFIndexT == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_0) || (m_TileSIndexT == TILE_STOP && m_TileSFlagsT == ROTATION_0) || (m_TileSIndexT == TILE_STOPS && (m_TileSFlagsT == ROTATION_0 || m_TileSFlagsT == ROTATION_180)) || (m_TileSIndexT == TILE_STOPA)))
				TempVel.y = 0;
			m_Core.m_Vel = TempVel;
		}
	}
}

void CCharacter::HandleTiles(int Index)
{
	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	int MapIndex = Index;
	float Offset = 4.0f;
	int MapIndexL = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x + (m_ProximityRadius / 2) + Offset, m_Pos.y));
	int MapIndexR = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x - (m_ProximityRadius / 2) - Offset, m_Pos.y));
	int MapIndexT = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y + (m_ProximityRadius / 2) + Offset));
	int MapIndexB = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y - (m_ProximityRadius / 2) - Offset));
	m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	m_TileFlags = GameServer()->Collision()->GetTileFlags(MapIndex);
	m_TileIndexL = GameServer()->Collision()->GetTileIndex(MapIndexL);
	m_TileFlagsL = GameServer()->Collision()->GetTileFlags(MapIndexL);
	m_TileIndexR = GameServer()->Collision()->GetTileIndex(MapIndexR);
	m_TileFlagsR = GameServer()->Collision()->GetTileFlags(MapIndexR);
	m_TileIndexB = GameServer()->Collision()->GetTileIndex(MapIndexB);
	m_TileFlagsB = GameServer()->Collision()->GetTileFlags(MapIndexB);
	m_TileIndexT = GameServer()->Collision()->GetTileIndex(MapIndexT);
	m_TileFlagsT = GameServer()->Collision()->GetTileFlags(MapIndexT);
	m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
	m_TileFFlags = GameServer()->Collision()->GetFTileFlags(MapIndex);
	m_TileFIndexL = GameServer()->Collision()->GetFTileIndex(MapIndexL);
	m_TileFFlagsL = GameServer()->Collision()->GetFTileFlags(MapIndexL);
	m_TileFIndexR = GameServer()->Collision()->GetFTileIndex(MapIndexR);
	m_TileFFlagsR = GameServer()->Collision()->GetFTileFlags(MapIndexR);
	m_TileFIndexB = GameServer()->Collision()->GetFTileIndex(MapIndexB);
	m_TileFFlagsB = GameServer()->Collision()->GetFTileFlags(MapIndexB);
	m_TileFIndexT = GameServer()->Collision()->GetFTileIndex(MapIndexT);
	m_TileFFlagsT = GameServer()->Collision()->GetFTileFlags(MapIndexT);//
	//dbg_msg("Tiles","%d, %d, %d, %d, %d", m_TileSIndex, m_TileSIndexL, m_TileSIndexR, m_TileSIndexB, m_TileSIndexT);
	//Sensitivity
	int S1 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y - m_ProximityRadius / 3.f));
	int S2 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f));
	int S3 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x - m_ProximityRadius / 3.f, m_Pos.y - m_ProximityRadius / 3.f));
	int S4 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x - m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f));
	int Tile1 = GameServer()->Collision()->GetTileIndex(S1);
	int Tile2 = GameServer()->Collision()->GetTileIndex(S2);
	int Tile3 = GameServer()->Collision()->GetTileIndex(S3);
	int Tile4 = GameServer()->Collision()->GetTileIndex(S4);
	int FTile1 = GameServer()->Collision()->GetFTileIndex(S1);
	int FTile2 = GameServer()->Collision()->GetFTileIndex(S2);
	int FTile3 = GameServer()->Collision()->GetFTileIndex(S3);
	int FTile4 = GameServer()->Collision()->GetFTileIndex(S4);
	if(Index < 0)
	{
		m_iVisible = false;
		m_LastRefillJumps = false;
		return;
	}
	int tcp = GameServer()->Collision()->IsTCheckpoint(MapIndex);
	if(tcp)
		m_TeleCheckpoint = tcp;

	if(m_TileIndex >= TILE_HOLDPOINT_BEGIN && m_TileIndex <= TILE_HOLDPOINT_END) 
		GameServer()->m_pController->OnHoldpoint(m_TileIndex-TILE_HOLDPOINT_BEGIN);
	else if(m_TileIndex >= TILE_ZSTOP_BEGIN && m_TileIndex <= TILE_ZSTOP_END && m_pPlayer->GetTeam() == TEAM_BLUE) 
		GameServer()->m_pController->OnZStop(m_TileIndex-TILE_ZSTOP_BEGIN);
	else if(m_TileIndex >= TILE_ZHOLDPOINT_BEGIN && m_TileIndex <= TILE_ZHOLDPOINT_END && m_pPlayer->GetTeam() == TEAM_BLUE) 
		GameServer()->m_pController->OnZHoldpoint(m_TileIndex-TILE_ZHOLDPOINT_BEGIN+32);

	// unlimited air jumps
	if(((m_TileIndex == TILE_SUPER_START) || (m_TileFIndex == TILE_SUPER_START)) && !m_SuperJump)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You have unlimited air jumps");
		m_SuperJump = true;
		if (m_Core.m_Jumps == 0)
		{
			m_NeededFaketuning &= ~FAKETUNE_NOJUMP;
			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		}
	}
	else if(((m_TileIndex == TILE_SUPER_END) || (m_TileFIndex == TILE_SUPER_END)) && m_SuperJump)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You don't have unlimited air jumps");
		m_SuperJump = false;
		if (m_Core.m_Jumps == 0)
		{
			m_NeededFaketuning |= FAKETUNE_NOJUMP;
			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		}
	}

	// walljump
	if((m_TileIndex == TILE_WALLJUMP) || (m_TileFIndex == TILE_WALLJUMP))
	{
		if(m_Core.m_Vel.y > 0 && m_Core.m_Colliding && m_Core.m_LeftWall)
		{
			m_Core.m_LeftWall = false;
			m_Core.m_JumpedTotal = m_Core.m_Jumps - 1;
			m_Core.m_Jumped = 1;
		}
	}

	// refill jumps
	if(((m_TileIndex == TILE_REFILL_JUMPS) || (m_TileFIndex == TILE_REFILL_JUMPS)) && !m_LastRefillJumps)
	{
		m_Core.m_JumpedTotal = 0;
		m_Core.m_Jumped = 0;
		m_LastRefillJumps = true;
	}
	if((m_TileIndex != TILE_REFILL_JUMPS) && (m_TileFIndex != TILE_REFILL_JUMPS))
		m_LastRefillJumps = false;

	// no visible
	if(((m_TileIndex == TILE_VISIBLE) || (m_TileFIndex == TILE_VISIBLE)) && !m_iVisible)
		m_iVisible = true;
	if((m_TileIndex != TILE_VISIBLE) && (m_TileFIndex != TILE_VISIBLE) && m_iVisible)
		m_iVisible = false;
	
	// stopper
	if(((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_270) || (m_TileIndexL == TILE_STOP && m_TileFlagsL == ROTATION_270) || (m_TileIndexL == TILE_STOPS && (m_TileFlagsL == ROTATION_90 || m_TileFlagsL ==ROTATION_270)) || (m_TileIndexL == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_270) || (m_TileFIndexL == TILE_STOP && m_TileFFlagsL == ROTATION_270) || (m_TileFIndexL == TILE_STOPS && (m_TileFFlagsL == ROTATION_90 || m_TileFFlagsL == ROTATION_270)) || (m_TileFIndexL == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_270) || (m_TileSIndexL == TILE_STOP && m_TileSFlagsL == ROTATION_270) || (m_TileSIndexL == TILE_STOPS && (m_TileSFlagsL == ROTATION_90 || m_TileSFlagsL == ROTATION_270)) || (m_TileSIndexL == TILE_STOPA)) && m_Core.m_Vel.x > 0)
	{
		if((int)GameServer()->Collision()->GetPos(MapIndexL).x)
			if((int)GameServer()->Collision()->GetPos(MapIndexL).x < (int)m_Core.m_Pos.x)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.x = 0;
	}
	if(((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_90) || (m_TileIndexR == TILE_STOP && m_TileFlagsR == ROTATION_90) || (m_TileIndexR == TILE_STOPS && (m_TileFlagsR == ROTATION_90 || m_TileFlagsR == ROTATION_270)) || (m_TileIndexR == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_90) || (m_TileFIndexR == TILE_STOP && m_TileFFlagsR == ROTATION_90) || (m_TileFIndexR == TILE_STOPS && (m_TileFFlagsR == ROTATION_90 || m_TileFFlagsR == ROTATION_270)) || (m_TileFIndexR == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_90) || (m_TileSIndexR == TILE_STOP && m_TileSFlagsR == ROTATION_90) || (m_TileSIndexR == TILE_STOPS && (m_TileSFlagsR == ROTATION_90 || m_TileSFlagsR == ROTATION_270)) || (m_TileSIndexR == TILE_STOPA)) && m_Core.m_Vel.x < 0)
	{
		if((int)GameServer()->Collision()->GetPos(MapIndexR).x)
			if((int)GameServer()->Collision()->GetPos(MapIndexR).x > (int)m_Core.m_Pos.x)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.x = 0;
	}
	if(((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_180) || (m_TileIndexB == TILE_STOP && m_TileFlagsB == ROTATION_180) || (m_TileIndexB == TILE_STOPS && (m_TileFlagsB == ROTATION_0 || m_TileFlagsB == ROTATION_180)) || (m_TileIndexB == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_180) || (m_TileFIndexB == TILE_STOP && m_TileFFlagsB == ROTATION_180) || (m_TileFIndexB == TILE_STOPS && (m_TileFFlagsB == ROTATION_0 || m_TileFFlagsB == ROTATION_180)) || (m_TileFIndexB == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_180) || (m_TileSIndexB == TILE_STOP && m_TileSFlagsB == ROTATION_180) || (m_TileSIndexB == TILE_STOPS && (m_TileSFlagsB == ROTATION_0 || m_TileSFlagsB == ROTATION_180)) || (m_TileSIndexB == TILE_STOPA)) && m_Core.m_Vel.y < 0)
	{
		if((int)GameServer()->Collision()->GetPos(MapIndexB).y)
			if((int)GameServer()->Collision()->GetPos(MapIndexB).y > (int)m_Core.m_Pos.y)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.y = 0;
	}
	if(((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_0) || (m_TileIndexT == TILE_STOP && m_TileFlagsT == ROTATION_0) || (m_TileIndexT == TILE_STOPS && (m_TileFlagsT == ROTATION_0 || m_TileFlagsT == ROTATION_180)) || (m_TileIndexT == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_0) || (m_TileFIndexT == TILE_STOP && m_TileFFlagsT == ROTATION_0) || (m_TileFIndexT == TILE_STOPS && (m_TileFFlagsT == ROTATION_0 || m_TileFFlagsT == ROTATION_180)) || (m_TileFIndexT == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_0) || (m_TileSIndexT == TILE_STOP && m_TileSFlagsT == ROTATION_0) || (m_TileSIndexT == TILE_STOPS && (m_TileSFlagsT == ROTATION_0 || m_TileSFlagsT == ROTATION_180)) || (m_TileSIndexT == TILE_STOPA)) && m_Core.m_Vel.y > 0)
	{
		if((int)GameServer()->Collision()->GetPos(MapIndexT).y)
			if((int)GameServer()->Collision()->GetPos(MapIndexT).y < (int)m_Core.m_Pos.y)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.y = 0;
		m_Core.m_Jumped = 0;
		m_Core.m_JumpedTotal = 0;
	}

	int z = GameServer()->Collision()->IsTeleport(MapIndex);
	if(!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons && z && Controller->m_TeleOuts[z-1].size())
	{
		int Num = Controller->m_TeleOuts[z-1].size();
		m_Core.m_Pos = Controller->m_TeleOuts[z-1][(!Num)?Num:rand() % Num];
		if(!g_Config.m_SvTeleportHoldHook)
		{
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
			m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
			m_Core.m_HookPos = m_Core.m_Pos;
		}
		if(g_Config.m_SvTeleportLoseWeapons)
		{
			for(int i=WEAPON_SHOTGUN;i<NUM_WEAPONS-1;i++)
				m_aWeapons[i].m_Got = false;
		}
		return;
	}
	int evilz = GameServer()->Collision()->IsEvilTeleport(MapIndex);
	if(evilz && Controller->m_TeleOuts[evilz-1].size())
	{
		int Num = Controller->m_TeleOuts[evilz-1].size();
		m_Core.m_Pos = Controller->m_TeleOuts[evilz-1][(!Num)?Num:rand() % Num];
		if (!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons)
		{
			m_Core.m_Vel = vec2(0,0);

			if(!g_Config.m_SvTeleportHoldHook)
			{
				m_Core.m_HookedPlayer = -1;
				m_Core.m_HookState = HOOK_RETRACTED;
				m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
				GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
				m_Core.m_HookPos = m_Core.m_Pos;
			}
			if(g_Config.m_SvTeleportLoseWeapons)
			{
				for(int i=WEAPON_SHOTGUN;i<NUM_WEAPONS-1;i++)
					m_aWeapons[i].m_Got = false;
			}
		}
		return;
	}
	if(GameServer()->Collision()->IsCheckEvilTeleport(MapIndex))
	{
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for(int k=m_TeleCheckpoint-1; k >= 0; k--)
		{
			if(Controller->m_TeleCheckOuts[k].size())
			{
				m_Core.m_HookedPlayer = -1;
				m_Core.m_HookState = HOOK_RETRACTED;
				m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
				int Num = Controller->m_TeleCheckOuts[k].size();
				m_Core.m_Pos = Controller->m_TeleCheckOuts[k][(!Num)?Num:rand() % Num];
				GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
				m_Core.m_Vel = vec2(0,0);
				m_Core.m_HookPos = m_Core.m_Pos;
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if(GameServer()->m_pController->CanSpawn(m_pPlayer->GetTeam(), &SpawnPos))
		{
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
			m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
			m_Core.m_Pos = SpawnPos;
			GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
			m_Core.m_Vel = vec2(0,0);
			m_Core.m_HookPos = m_Core.m_Pos;
		}
		return;
	}
	if(GameServer()->Collision()->IsCheckTeleport(MapIndex))
	{
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for(int k=m_TeleCheckpoint-1; k >= 0; k--)
		{
			if(Controller->m_TeleCheckOuts[k].size())
			{
				m_Core.m_HookedPlayer = -1;
				m_Core.m_HookState = HOOK_RETRACTED;
				m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
				int Num = Controller->m_TeleCheckOuts[k].size();
				m_Core.m_Pos = Controller->m_TeleCheckOuts[k][(!Num)?Num:rand() % Num];
				m_Core.m_HookPos = m_Core.m_Pos;
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if(GameServer()->m_pController->CanSpawn(m_pPlayer->GetTeam(), &SpawnPos))
		{
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
			m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
			m_Core.m_Pos = SpawnPos;
			m_Core.m_HookPos = m_Core.m_Pos;
		}
		return;
	}
}

void CCharacter::HandleTuneLayer()
{
	m_TuneZoneOld = m_TuneZone;
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if(m_TuneZone)
		m_Core.m_pWorld->m_Tuning[g_Config.m_ClDummy] = GameServer()->TuningList()[m_TuneZone]; // throw tunings from specific zone into gamecore
	else
		m_Core.m_pWorld->m_Tuning[g_Config.m_ClDummy] = *GameServer()->Tuning();

	if (m_TuneZone != m_TuneZoneOld) // dont send tunigs all the time
		SendZoneMsgs();
}

void CCharacter::SendZoneMsgs()
{
	// send zone leave msg
	if (m_TuneZoneOld >= 0 && GameServer()->m_ZoneLeaveMsg[m_TuneZoneOld]) // m_TuneZoneOld >= 0: avoid zone leave msgs on spawn
	{
		const char* cur = GameServer()->m_ZoneLeaveMsg[m_TuneZoneOld];
		const char* pos;
		while ((pos = str_find(cur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, cur, pos - cur + 1);
			aBuf[pos - cur + 1] = '\0';
			cur = pos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), cur);
	}
	// send zone enter msg
	if (GameServer()->m_ZoneEnterMsg[m_TuneZone])
	{
		const char* cur = GameServer()->m_ZoneEnterMsg[m_TuneZone];
		const char* pos;
		while ((pos = str_find(cur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, cur, pos - cur + 1);
			aBuf[pos - cur + 1] = '\0';
			cur = pos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), cur);
	}
}

void CCharacter::DDRaceTick()
{
	if (m_Input.m_Direction != 0 || m_Input.m_Jump != 0)
		m_LastMove = Server()->Tick();

	if (m_pPlayer->m_AccData.m_Freeze){
		m_Input.m_Direction = m_Input.m_Jump = m_Input.m_Hook = 0;
	}
	
	HandleTuneLayer();
	m_Core.m_Id = GetPlayer()->GetCID();
}

void CCharacter::DDRacePostCoreTick()
{
	if (m_pPlayer->m_DefEmoteReset >= 0 && m_pPlayer->m_DefEmoteReset <= Server()->Tick())
	{
		m_pPlayer->m_DefEmoteReset = -1;
		m_EmoteType = m_pPlayer->m_DefEmote = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}

	if (m_Core.m_Jumps == 0) m_Core.m_Jumped = 3;
	else if (m_Core.m_Jumps == 1 && m_Core.m_Jumped > 0) m_Core.m_Jumped = 3;
	else if (m_Core.m_JumpedTotal < m_Core.m_Jumps - 1 && m_Core.m_Jumped > 1)	m_Core.m_Jumped = 1;

	if (m_SuperJump && m_Core.m_Jumped > 1)
		m_Core.m_Jumped = 1;

	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	HandleSkippableTiles(CurrentIndex);

	std::list < int > Indices = GameServer()->Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if (!Indices.empty())
	{
		for (std::list < int >::iterator i = Indices.begin(); i != Indices.end(); i++)
			HandleTiles(*i);
	}
	else
		HandleTiles(CurrentIndex);
}