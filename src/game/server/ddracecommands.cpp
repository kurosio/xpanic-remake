/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/shared/config.h>
#include <engine/server/server.h>
#include <game/server/teams.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/version.h>
#include <game/generated/nethash.cpp>

bool CheckClientID(int ClientID);

void CGameContext::ConGoLeft(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, -1, 0);
}

void CGameContext::ConGoRight(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, 1, 0);
}

void CGameContext::ConGoDown(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, 0, 1);
}

void CGameContext::ConGoUp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, 0, -1);
}

void CGameContext::ConMove(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, pResult->GetInteger(0),
			pResult->GetInteger(1));
}

void CGameContext::ConMoveRaw(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, pResult->GetInteger(0),
			pResult->GetInteger(1), true);
}

void CGameContext::MoveCharacter(int ClientID, int X, int Y, bool Raw)
{
	CCharacter* pChr = GetPlayerChar(ClientID);

	if (!pChr)
		return;

	pChr->Core()->m_Pos.x += ((Raw) ? 1 : 32) * X;
	pChr->Core()->m_Pos.y += ((Raw) ? 1 : 32) * Y;
}

void CGameContext::ConKillPlayer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->KillCharacter(WEAPON_GAME);
		char aBuf[80];
		str_format(aBuf, sizeof(aBuf), "%s was killed by %s", pSelf->Server()->ClientName(Victim), pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}
}

void CGameContext::ConToTeleporter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	unsigned int TeleTo = pResult->GetInteger(0);

	if (((CGameControllerDDRace*)pSelf->m_pController)->m_TeleOuts[TeleTo-1].size())
	{
		int Num = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleOuts[TeleTo-1].size();
		vec2 TelePos = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleOuts[TeleTo-1][(!Num)?Num:rand() % Num];
		CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
		if (pChr)
		{
			pChr->Core()->m_Pos = TelePos;
			pChr->m_Pos = TelePos;
			pChr->m_PrevPos = TelePos;
		}
	}
}

void CGameContext::ConToCheckTeleporter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	unsigned int TeleTo = pResult->GetInteger(0);

	if (((CGameControllerDDRace*)pSelf->m_pController)->m_TeleCheckOuts[TeleTo-1].size())
	{
		int Num = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleCheckOuts[TeleTo-1].size();
		vec2 TelePos = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleCheckOuts[TeleTo-1][(!Num)?Num:rand() % Num];
		CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
		if (pChr)
		{
			pChr->Core()->m_Pos = TelePos;
			pChr->m_Pos = TelePos;
			pChr->m_PrevPos = TelePos;
			pChr->m_TeleCheckpoint = TeleTo;
		}
	}
}

void CGameContext::ConTeleport(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	int TeleTo = pResult->GetInteger(0);
	int Tele = pResult->m_ClientID;
	if (pResult->NumArguments() > 0)
		Tele = pResult->GetVictim();

	if (pSelf->m_apPlayers[TeleTo])
	{
		CCharacter* pChr = pSelf->GetPlayerChar(Tele);
		if (pChr && pSelf->GetPlayerChar(TeleTo))
		{
			pChr->Core()->m_Pos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
			pChr->m_Pos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
			pChr->m_PrevPos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
		}
	}
}

void CGameContext::Mute(IConsole::IResult *pResult, NETADDR *Addr, int Secs,
		const char *pDisplayName)
{
	char aBuf[128];
	int Found = 0;
	// find a matching mute for this ip, update expiration time if found
	for (int i = 0; i < m_NumMutes; i++)
	{
		if (net_addr_comp(&m_aMutes[i].m_Addr, Addr) == 0)
		{
			m_aMutes[i].m_Expire = Server()->Tick()
							+ Secs * Server()->TickSpeed();
			Found = 1;
		}
	}

	if (!Found) // nothing found so far, find a free slot..
	{
		if (m_NumMutes < MAX_MUTES)
		{
			m_aMutes[m_NumMutes].m_Addr = *Addr;
			m_aMutes[m_NumMutes].m_Expire = Server()->Tick()
							+ Secs * Server()->TickSpeed();
			m_NumMutes++;
			Found = 1;
		}
	}
	if (Found)
	{
		str_format(aBuf, sizeof aBuf, "'%s' has been muted for %d seconds.",
				pDisplayName, Secs);
		SendChat(-1, CHAT_ALL, aBuf);
	}
	else // no free slot found
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "mute array is full");
}

void CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"mutes",
			"Use either 'muteid <client_id> <seconds>' or 'muteip <ip> <seconds>'");
}

// mute through client id
void CGameContext::ConMuteID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	int Victim = pResult->GetVictim();

	NETADDR Addr;
	pSelf->Server()->GetClientAddr(Victim, &Addr);

	pSelf->Mute(pResult, &Addr, clamp(pResult->GetInteger(0), 1, 86400),
			pSelf->Server()->ClientName(Victim));
}

// mute through ip, arguments reversed to workaround parsing
void CGameContext::ConMuteIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	NETADDR Addr;
	if (net_addr_from_str(&Addr, pResult->GetString(0)))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes",
				"Invalid network address to mute");
	}
	pSelf->Mute(pResult, &Addr, clamp(pResult->GetInteger(1), 1, 86400),
			pResult->GetString(0));
}

// unmute by mute list index
void CGameContext::ConUnmute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	char aIpBuf[64];
	char aBuf[64];
	int Victim = pResult->GetVictim();

	if (Victim < 0 || Victim >= pSelf->m_NumMutes)
		return;

	pSelf->m_NumMutes--;
	pSelf->m_aMutes[Victim] = pSelf->m_aMutes[pSelf->m_NumMutes];

	net_addr_str(&pSelf->m_aMutes[Victim].m_Addr, aIpBuf, sizeof(aIpBuf), false);
	str_format(aBuf, sizeof(aBuf), "Unmuted %s", aIpBuf);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
}

// list mutes
void CGameContext::ConMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	char aIpBuf[64];
	char aBuf[128];
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes",
			"Active mutes:");
	for (int i = 0; i < pSelf->m_NumMutes; i++)
	{
		net_addr_str(&pSelf->m_aMutes[i].m_Addr, aIpBuf, sizeof(aIpBuf), false);
		str_format(
				aBuf,
				sizeof aBuf,
				"%d: \"%s\", %d seconds left",
				i,
				aIpBuf,
				(pSelf->m_aMutes[i].m_Expire - pSelf->Server()->Tick())
				/ pSelf->Server()->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
	}
}

bool CheckClientID(int ClientID)
{
	dbg_assert(ClientID >= 0 || ClientID < MAX_CLIENTS,
			"The Client ID is wrong");
	if (ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;
	return true;
}
