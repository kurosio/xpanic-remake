#ifndef GAME_SERVER_ENTITIES_TURRET_H
#define GAME_SERVER_ENTITIES_TURRET_H

#include <game/server/entity.h>

class CTurret : public CEntity
{
public:
	CTurret(CGameWorld *pGameWorld, vec2 Pos, int Owner, int Type, vec2 PosL1, vec2 PosL2);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	
	void ExperienceTAdd();
	void Fire();
    
	int m_Owner;
	
private:
    int m_inIDs[9];
	int m_IDC;
	int m_IDG;
	int m_IDS;
	int m_Ammo;
	int m_Lifetime;
	vec2 Direction;
	vec2 SavePosion;
	vec2 m_Pos1L;
	vec2 m_Pos2L;
	bool BackSpeed;
	bool m_ExpTime;
	int m_Type;
    int m_ReloadTick;
	int m_RegenTime;
};


#endif // GAME_SERVER_ENTITIES_TURRET_H
