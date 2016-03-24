/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_H
#define GAME_SERVER_ENTITIES_CHARACTER_H

#include <game/server/entity.h>
#include <game/generated/server_data.h>
#include <game/generated/protocol.h>

#include <game/gamecore.h>

class CGameTeams;

enum
{
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

enum
{
	FAKETUNE_FREEZE = 1,
	FAKETUNE_NOJUMP = 4,
	FAKETUNE_NOCOLL = 8,
	FAKETUNE_NOHOOK = 16,
	FAKETUNE_JETPACK = 32,
};

class CCharacter : public CEntity
{
	MACRO_ALLOC_POOL_ID()

	friend class CSaveTee; // need to use core

public:
	//character's size
	static const int ms_PhysSize = 28;

	CCharacter(CGameWorld *pWorld);

	virtual void Reset();
	virtual void Destroy();
	virtual void Tick();
	virtual void TickDefered();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);
	virtual int NetworkClipped(int SnappingClient);
	virtual int NetworkClipped(int SnappingClient, vec2 CheckPos);

	bool IsGrounded();
	void SetWeapon(int W);
	void HandleWeaponSwitch();
	void DoWeaponSwitch();
	void HandleWeapons();

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void ResetInput();
	void FireWeapon();
	void GrenadeFire(vec2 Pos);
	
	void Die(int Killer, int Weapon);
	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon);

	bool Spawn(class CPlayer *pPlayer, vec2 Pos);
	bool Remove();

	void ExperienceAdd(int Exp, int ClientID);
	bool IncreaseHealth(int Amount);
	bool IncreaseArmor(int Amount);

	bool GiveWeapon(int Weapon, int Ammo);

	void SetEmote(int Emote, int Tick);

	// the player core for the physics
	CCharacterCore m_Core;

	int NeededFaketuning() {return m_NeededFaketuning;}
	bool IsAlive() const { return m_Alive; }
	class CPlayer *GetPlayer() { return m_pPlayer; }

private:
	// player controlling this character
	class CPlayer *m_pPlayer;

	bool m_Alive;
	int m_NeededFaketuning;

	// weapon info
	CEntity *m_apHitObjects[10];
	int m_NumObjectsHit;

	struct WeaponStat
	{
		int m_AmmoRegenStart;
		int m_Ammo;
		int m_Ammocost;
		bool m_Got;

	} m_aWeapons[NUM_WEAPONS];

	int m_LastWeapon;
	int m_QueuedWeapon;

	int m_ReloadTimer;
	int m_AttackTick;
	int IhammerRelTick;
	int m_DamageTaken;
	int m_HeartTick;
	
	int m_EmoteType;
	int m_EmoteStop;

	// last tick that the player took any action ie some input
	int m_LastAction;
	int m_LastNoAmmoSound;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_PrevInput;
	CNetObj_PlayerInput m_Input;
	int m_NumInputs;
	int m_Jumped;

	int m_DamageTakenTick;

	int m_Health;
	int m_Armor;

	// info for dead reckoning
	int m_ReckoningTick; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core

	// DDRace


	void HandleTiles(int Index);
	void HandleSkippableTiles(int Index);
	void DDRaceTick();
	void DDRacePostCoreTick();
	void HandleTuneLayer();
	void SendZoneMsgs();
	
	int m_InvisID;

public:
	CGameTeams* Teams();
	void DestroyChildEntities();
	int Team();
	bool CanCollide(int ClientID);
	bool SameTeam(int ClientID);
	bool m_HittingDoor;
	int m_BurnTick;
	int m_BurnedFrom;
	bool m_TypeHealthCh;
	vec2 m_PushDirection;
	vec2 m_RiflePos;
	vec2 m_TurGrenade;
	vec2 m_TurRifle;
	vec2 m_OldPos;
	vec2 m_ActivTurs;
	int m_JumpShop;
	bool m_TurretActive[5];
	int m_RegenTime;
	int IhammerTick;
	int ITickSecond;
	int m_mAmmo;
	bool m_SuperJump;
	int m_TuneZone;
	int m_TuneZoneOld;
	int m_LastMove;
	vec2 m_PrevPos;
	int m_TeleCheckpoint;
	int m_TileIndex;
	int m_TileFlags;
	int m_TileFIndex;
	int m_TileFFlags;
	int m_TileSIndex;
	int m_TileSFlags;
	int m_TileIndexL;
	int m_TileFlagsL;
	int m_TileFIndexL;
	int m_TileFFlagsL;
	int m_TileSIndexL;
	int m_TileSFlagsL;
	int m_TileIndexR;
	int m_TileFlagsR;
	int m_TileFIndexR;
	int m_TileFFlagsR;
	int m_TileSIndexR;
	int m_TileSFlagsR;
	int m_TileIndexT;
	int m_TileFlagsT;
	int m_TileFIndexT;
	int m_TileFFlagsT;
	int m_TileSIndexT;
	int m_TileSFlagsT;
	int m_TileIndexB;
	int m_TileFlagsB;
	int m_TileFIndexB;
	int m_TileFFlagsB;
	int m_TileSIndexB;
	int m_TileSFlagsB;
	vec2 m_Intersection;
	int64 m_LastStartWarning;
	bool m_LastRefillJumps;
	bool m_iVisible;
	void SetZomb();

	int GetLastWeapon() { return m_LastWeapon; };
	void SetLastWeapon(int LastWeap) {m_LastWeapon = LastWeap; };
	int GetActiveWeapon() { return m_Core.m_ActiveWeapon; };
	void SetActiveWeapon(int ActiveWeap) {m_Core.m_ActiveWeapon = ActiveWeap; };
	void SetLastAction(int LastAction) {m_LastAction = LastAction; };
	int GetArmor() { return m_Armor; };
	void SetArmor(int Armor) {m_Armor = Armor; };
	CCharacterCore GetCore() { return m_Core; };
	void SetCore(CCharacterCore Core) {m_Core = Core; };
	CCharacterCore* Core() { return &m_Core; };
	bool GetWeaponGot(int Type) { return m_aWeapons[Type].m_Got; };
	void SetWeaponGot(int Type, bool Value) { m_aWeapons[Type].m_Got = Value; };
	int GetWeaponAmmo(int Type) { return m_aWeapons[Type].m_Ammo; };
	void SetWeaponAmmo(int Type, int Value) { m_aWeapons[Type].m_Ammo = Value; };
	bool IsAlive() { return m_Alive; };
	void SetEmoteType(int EmoteType) { m_EmoteType = EmoteType; };
	void SetEmoteStop(int EmoteStop) { m_EmoteStop = EmoteStop; };
};

#endif
