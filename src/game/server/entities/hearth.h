#ifndef GAME_SERVER_ENTITIES_HEARTHS_H
#define GAME_SERVER_ENTITIES_HEARTHS_H

#include <game/server/entity.h>

class CLifeHearth : public CEntity
{
public:
	CLifeHearth(CGameWorld *pGameWorld, vec2 Pos, int Owner);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	int m_Owner;
	bool m_Active;
	
private:
	vec2 Lefpos;
	int m_Lifetime;
};


#endif 
