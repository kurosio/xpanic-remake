/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_DOORZ_H
#define GAME_SERVER_ENTITIES_DOORZ_H

#include <game/server/entity.h>

enum
{
	DOOR_OPEN = 0,
	DOOR_CLOSED,
	DOOR_ZCLOSING,
	DOOR_ZCLOSED,
	DOOR_REOPENED
};

class CDrSz : public CEntity
{
public:
	CDrSz(CGameWorld *pGameWorld, int Index, int Time);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);

private:
	short int m_Time;
	short int m_Index;
	short int m_State;
};

#endif
