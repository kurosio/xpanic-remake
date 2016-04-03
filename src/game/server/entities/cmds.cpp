#include <stdio.h>
#include <string.h>

#include <engine/shared/config.h>
#include <engine/server.h>
#include <game/version.h>
#include "cmds.h"
#include "account.h"
#include "hearth.h"

CCmd::CCmd(CPlayer *pPlayer, CGameContext *pGameServer)
{
	m_pPlayer = pPlayer;
	m_pGameServer = pGameServer;
}

void CCmd::ChatCmd(CNetMsg_Cl_Say *Msg)
{
	if(!strncmp(Msg->m_pMessage, "/login", 6))
	{
		LastChat();		
		if(GameServer()->m_World.m_Paused) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please wait end round!");
		
		char Username[256], Password[256];
		if(sscanf(Msg->m_pMessage, "/login %s %s", Username, Password) != 2) 
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Use: /login <username> <password>");
		
		if(str_length(Username) > 15 || str_length(Username) < 4 || str_length(Password) > 15 || str_length(Password) < 4) 
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Username / Password must contain 4-15 characters");
		
		m_pPlayer->m_pAccount->Login(Username, Password);
		return;
	}
	
	else if(!strncmp(Msg->m_pMessage, "/register", 9))
	{
		LastChat();
		if(GameServer()->m_World.m_Paused)  return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please wait end round!");
		
		char Username[256], Password[256];
		if(sscanf(Msg->m_pMessage, "/register %s %s", Username, Password) != 2) 
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Use /register <username> <password>'");
		
		if(str_length(Username) > 15 || str_length(Username) < 4 || str_length(Password) > 15 || str_length(Password) < 4)
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Username / Password must contain 4-15 characters");
		else if (!strcmp(Username, Password))
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Username and Password must be differend");

		m_pPlayer->m_pAccount->Register(Username, Password);
		return;
	}
	
	else if (!strncmp(Msg->m_pMessage, "/sd", 3) && GameServer()->Server()->IsAuthed(m_pPlayer->GetCID()))
	{
		LastChat();
		int size = 0;
		if ((sscanf(Msg->m_pMessage, "/sd %d", &size)) != 1)
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please use: /sd <idsound>");

		if (m_pPlayer->GetTeam() == TEAM_SPECTATORS || !GameServer()->GetPlayerChar(m_pPlayer->GetCID()))
			return;

		int soundid = clamp(size, 0, 40);
		GameServer()->CreateSound(GameServer()->GetPlayerChar(m_pPlayer->GetCID())->m_Pos, soundid);
		return;
	}

	else if(!strcmp(Msg->m_pMessage, "/logout"))
	{
		LastChat();
		if(!m_pPlayer->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You aren't logget in");
		if(GameServer()->m_World.m_Paused) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please wait end round!");
		if (GameServer()->m_pController->NumZombs() == 1 && m_pPlayer->GetTeam() == TEAM_RED) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You last zombie!");
		if (GameServer()->m_pController->NumPlayers() < 3 && GameServer()->m_pController->m_Warmup)	return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Wait for the beginning of the round");

		m_pPlayer->m_pAccount->Apply(), m_pPlayer->m_pAccount->Reset();

		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Successes! You logout.");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You can login again with the command:");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/login <username> <password>");

		if(GameServer()->GetPlayerChar(m_pPlayer->GetCID()) && GameServer()->GetPlayerChar(m_pPlayer->GetCID())->IsAlive())
			GameServer()->GetPlayerChar(m_pPlayer->GetCID())->Die(m_pPlayer->GetCID(), WEAPON_GAME);

		return;
	}	
	
	else if (!strncmp(Msg->m_pMessage, "/upgr", 5))
	{
		LastChat();
		if (!m_pPlayer->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You are not logged in! Type '/account' for more information!");

		char supgr[256], andg[64];
		if (sscanf(Msg->m_pMessage, "/upgr %s", supgr) != 1)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/upgr <Type>"), GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Types: dmg, hp, handle, ammoregen, ammo, stats");
			return;
		}
		if (!strcmp(supgr, "handle"))
		{
			if (m_pPlayer->m_AccData.m_Handle >= 300)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal Handle level!");
			if (m_pPlayer->m_AccData.m_Money <= 0)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not money counts!");

			m_pPlayer->m_AccData.m_Money--, m_pPlayer->m_AccData.m_Handle++;
			str_format(andg, sizeof(andg), "Upgrade Handle new is: %d, Your money: %d", m_pPlayer->m_AccData.m_Handle, m_pPlayer->m_AccData.m_Money);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}
		else if (!strcmp(supgr, "dmg"))
		{
			if (m_pPlayer->m_AccData.m_Dmg >= 21)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal Damage level!");
			if (m_pPlayer->m_AccData.m_Money <= 0)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not money counts!");

			m_pPlayer->m_AccData.m_Money--, m_pPlayer->m_AccData.m_Dmg++;
			str_format(andg, sizeof(andg), "Upgrade Damage new is: %d, Your money: %d", m_pPlayer->m_AccData.m_Dmg, m_pPlayer->m_AccData.m_Money);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}
		else if (!strcmp(supgr, "hp"))
		{
			if (m_pPlayer->m_AccData.m_Health >= 100)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal hp level!");
			if (m_pPlayer->m_AccData.m_Money <= 0)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not money counts!");

			m_pPlayer->m_AccData.m_Money--, m_pPlayer->m_AccData.m_Health++;
			str_format(andg, sizeof(andg), "Upgrade Health new is: %d, Maximum health: %d, Your money: %d", m_pPlayer->m_AccData.m_Health, 1000 + m_pPlayer->m_AccData.m_Health * 10 + m_pPlayer->m_AccData.m_Level * 20, m_pPlayer->m_AccData.m_Money);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}
		else if (!strcmp(supgr, "ammoregen"))
		{
			if (m_pPlayer->m_AccData.m_Ammoregen >= 60)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal ammoregen level!");
			if (m_pPlayer->m_AccData.m_Money <= 0)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not money counts!");

			m_pPlayer->m_AccData.m_Money--, m_pPlayer->m_AccData.m_Ammoregen++;
			str_format(andg, sizeof(andg), "Upgrade Ammoregen new is: %d, Your money: %d", m_pPlayer->m_AccData.m_Ammoregen, m_pPlayer->m_AccData.m_Money);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}
		else if (!strcmp(supgr, "ammo"))
		{
			if (m_pPlayer->m_AccData.m_Ammo >= 20)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal ammo level!");
			if (m_pPlayer->m_AccData.m_Money < 10)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not money counts. Need 10 money!");

			m_pPlayer->m_AccData.m_Money -= 10, m_pPlayer->m_AccData.m_Ammo++;
			str_format(andg, sizeof(andg), "Upgrade Ammo new is: %d, Your money: %d", GameServer()->GetPlayerChar(m_pPlayer->GetCID())->m_mAmmo + m_pPlayer->m_AccData.m_Ammo, m_pPlayer->m_AccData.m_Money);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}
		else if (!strcmp(supgr, "stats"))
		{
			char aBuf2[32];
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ ! Upgr stats details ! ~~~~~~~~");
			str_format(aBuf2, sizeof(aBuf2), "Dmg: %d", m_pPlayer->m_AccData.m_Dmg);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf2);
			str_format(aBuf2, sizeof(aBuf2), "HP: %d", m_pPlayer->m_AccData.m_Health);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf2);
			str_format(aBuf2, sizeof(aBuf2), "Handle: %d", m_pPlayer->m_AccData.m_Handle);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf2);
			str_format(aBuf2, sizeof(aBuf2), "AmmoRegen: %d", m_pPlayer->m_AccData.m_Ammoregen);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf2);
			str_format(aBuf2, sizeof(aBuf2), "Ammo: %d", m_pPlayer->m_AccData.m_Ammo);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf2);						
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You can upgrade your account with the command:");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/upgr info");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ ! Upgr stats details ! ~~~~~~~~");
			return;
		}
		else return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "This type doesn't exist.");
	}
	else if(!strncmp(Msg->m_pMessage, "/turret", 7))
	{
		LastChat();
		if(!m_pPlayer->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You are not logged in! Type '/account' for more information!");

		char supgr[256], andg[64];
		if (sscanf(Msg->m_pMessage, "/turret %s", supgr) != 1)
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Use /turret info");

		if(!strcmp(supgr, "info"))
		{		
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/turret info - show information");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/turret help - show help");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/turret stats - view stats upgrade turrets");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/turret speed - upgrade turret speed (1 turret money)");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/turret dmg - upgrade turret damage (1 turret money)");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/turret ammo - upgrade turret ammo (1 turret money)");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/turret ammoregen - upgrade turret ammo (1 turret money)");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/turret range - upgrade turret range (1 turret money)");
			return;
		}			
		else if(!strcmp(supgr, "help"))
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Turret could be placed via Ghost Emoticon");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Turrets can have 3 different weapons");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Gun Turret: Shoots once per sec at enemy");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Shotgun Turret: Shoots once per 3 sec at enemy");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Laser Turret: places a laser each 40 sec if the zombie reach it's line");
			return;
		}
		else if(!strcmp(supgr, "dmg"))
		{
			if(m_pPlayer->m_AccData.m_TurretDmg > 100)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal turret damage level!");		
			if(m_pPlayer->m_AccData.m_TurretMoney <= 0)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not turret money counts!");
			
			m_pPlayer->m_AccData.m_TurretMoney--, m_pPlayer->m_AccData.m_TurretDmg++;
			str_format(andg, sizeof(andg), "Your turret's damage is upgraded, now it is: %d", m_pPlayer->m_AccData.m_TurretDmg);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}			
		else if(!strcmp(supgr, "speed"))
		{
			if(m_pPlayer->m_AccData.m_TurretSpeed >= 500)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal turret speed level!");		
			if(m_pPlayer->m_AccData.m_TurretMoney <= 0)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not turret money counts!");
			
			m_pPlayer->m_AccData.m_TurretMoney--, m_pPlayer->m_AccData.m_TurretSpeed++;
			str_format(andg, sizeof(andg), "Your turret's speed is upgraded, now it is: %d", m_pPlayer->m_AccData.m_TurretSpeed);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}	
		else if (!strcmp(supgr, "ammo"))
		{
			if (m_pPlayer->m_AccData.m_TurretAmmo >= 100)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal turret ammo level!");
			if (m_pPlayer->m_AccData.m_TurretMoney <= 0)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not turret money counts!");

			m_pPlayer->m_AccData.m_TurretMoney--, m_pPlayer->m_AccData.m_TurretAmmo++;
			str_format(andg, sizeof(andg), "Your turret's gun is upgraded, now it is: %d", m_pPlayer->m_AccData.m_TurretAmmo);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}
		else if (!strcmp(supgr, "ammoregen"))
		{
			if (m_pPlayer->m_AccData.m_TurretShotgun >= 75)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal turret ammoregen level!");
			if (m_pPlayer->m_AccData.m_TurretMoney <= 0)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not turret money counts!");

			m_pPlayer->m_AccData.m_TurretMoney--, m_pPlayer->m_AccData.m_TurretShotgun++;
			str_format(andg, sizeof(andg), "Your turret's ammoregen is upgraded, now it is: %d", m_pPlayer->m_AccData.m_TurretShotgun);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}
		else if (!strcmp(supgr, "range"))
		{
			if (m_pPlayer->m_AccData.m_TurretRange >= 200)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Maximal turret range level!");
			if (m_pPlayer->m_AccData.m_TurretMoney <= 0)
				return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have not turret money counts!");

			m_pPlayer->m_AccData.m_TurretMoney--, m_pPlayer->m_AccData.m_TurretRange++;
			str_format(andg, sizeof(andg), "Your turret's range is upgraded, now it is: %d", m_pPlayer->m_AccData.m_TurretRange);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			m_pPlayer->m_pAccount->Apply();
			return;
		}
		else if (!strcmp(supgr, "stats"))
		{
			char aBuf[64];
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "---- Turret ----");
			str_format(aBuf, sizeof(aBuf), "Lvl: %d", m_pPlayer->m_AccData.m_TurretLevel);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			str_format(aBuf, sizeof(aBuf), "Exp: %d", m_pPlayer->m_AccData.m_TurretExp);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			str_format(aBuf, sizeof(aBuf), "Money: %d", m_pPlayer->m_AccData.m_TurretMoney);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			str_format(aBuf, sizeof(aBuf), "Speed: %d", m_pPlayer->m_AccData.m_TurretSpeed);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			str_format(aBuf, sizeof(aBuf), "Damage: %d", m_pPlayer->m_AccData.m_TurretDmg);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			str_format(aBuf, sizeof(aBuf), "Ammo: %d", m_pPlayer->m_AccData.m_TurretAmmo);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			str_format(aBuf, sizeof(aBuf), "AmmoRegen: %d", m_pPlayer->m_AccData.m_TurretShotgun);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			str_format(aBuf, sizeof(aBuf), "Range: %d", m_pPlayer->m_AccData.m_TurretRange);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "---- Turret ----");
			return;
		}
		else
		{
			str_format(andg, sizeof(andg), "No such turret upgrade: '%s'!", supgr);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), andg);
			return;
		}
	}
	else if (!str_comp_nocase(Msg->m_pMessage, "/me") || !str_comp_nocase(Msg->m_pMessage, "/status") || !str_comp_nocase(Msg->m_pMessage, "/stats"))
	{
		LastChat();
		if (!m_pPlayer->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You are not logged in! Type '/account' for more information!");

		char aBuf[64];
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ ! Your account details ! ~~~~~~~~");
		str_format(aBuf, sizeof(aBuf), "Login: %s", m_pPlayer->m_AccData.m_Username);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		str_format(aBuf, sizeof(aBuf), "Password: %s", m_pPlayer->m_AccData.m_Password);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Mailbox: <no mailbox> (no active)");
		str_format(aBuf, sizeof(aBuf), "Level: %d", m_pPlayer->m_AccData.m_Level);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		str_format(aBuf, sizeof(aBuf), "Exp: %d/%d", m_pPlayer->m_AccData.m_Exp, m_pPlayer->m_AccData.m_Level);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		str_format(aBuf, sizeof(aBuf), "Money: %d", m_pPlayer->m_AccData.m_Money);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		str_format(aBuf, sizeof(aBuf), "Freeze: %s", m_pPlayer->m_AccData.m_Freeze ? "yes" : "no");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You can change your password with the command:");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/password <new password>");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ ! Your account details ! ~~~~~~~~");
		return;
	}
	else if(!strncmp(Msg->m_pMessage, "/idlist", 7))
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "--- List IDs ---");
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i])
			{
				char aBuf[32];
				str_format(aBuf, sizeof(aBuf), "%s ID:%i", GameServer()->Server()->ClientName(i), i);
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			}	
		}
		return;
	}
	else if(!strncmp(Msg->m_pMessage, "/password", 9))
	{
		LastChat();
		if (!m_pPlayer->m_AccData.m_UserID)
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You are not logged in! Type '/account' for more information!");

		char NewPassword[256];
		if(sscanf(Msg->m_pMessage, "/password %s", NewPassword) != 1)
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Use \"/password <new password>\"");

		m_pPlayer->m_pAccount->NewPassword(NewPassword);
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/vip"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "--- ViP ---");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "ViP: Get 3x Exp & 2x Money & 5 For killing spree");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Invis: +1 secounds, InvisCD: 20 secounds");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/prefix = +5 ammo + Special Dmg & 5 For killing spree");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "1 month VIP = 0euros || 3 months VIP = 0euros || LifeTime VIP = 0euros");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Payments: Name, name");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Skype: nope, Name: nope");
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/supervip"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "--- SuperViP ---");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "SuperVIP: Get 5x Exp & 3x Money & 5 Score4Life & 3 For killing spree");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Invis: +3 secounds, InvisCD: 10 secounds");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/superprefix = Special Dmg + 15 Ammo, Access to commands:");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/resetupgr - Reset your upgr & /supervipmsg - Special messge");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "1 month SuperVIP = 0euros || 3 months UltimateVIP = 0euros || LifeTime UltimateVIP = 0euros");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Payments: Name, name");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Skype: nope, Name: nope");
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/ultimatevip"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "--- UltimateViP ---");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "UltimateVIP: Get 7x Exp & 5x Money & 5 Score4Life & 2 For killing spree");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Invis: +5 secounds, InvisCD: 5 secounds");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/resetupgr - Reset your upgr & /ultimatemsg - Special messge");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "1 month UltimateVIP = 0euros || 3 months UltimateVIP = 0euros || LifeTime UltimateVIP = 0euros");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Payments: Name, name");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Skype: nope, Name: nope");
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/info"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ Welcome to info! ~~~~~~~~");
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Owner: %s / Skype: %s", g_Config.m_SvOwnerName, g_Config.m_SvOwnerSkype);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Panic mod by Kurosio, helper: nope");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ Welcome to info! ~~~~~~~~");
		return;
	}
	else if(!strcmp(Msg->m_pMessage, "/help"))
    {
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ Welcome to help! ~~~~~~~~");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/account - information & help account system");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/news - for see what news in server");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/rules - for read the rules Panic");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/pm - send personal message to player");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/cmdlist - commands server");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/vip - get info vip status");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/supervip - get info supervip status");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/ultimatevip - get info ultimatevip status");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/turret info - info about turrets");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/levels - info about level");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/shop - shop score tees");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ Welcome to help! ~~~~~~~~");
		return;
    }
	else if (!strcmp(Msg->m_pMessage, "/levels"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Since 40: your shotgun spread growing by 1 bullet per 10 levels");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Since 50: +10 ammo");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Since 100: +10 ammo");
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/account"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "--- Account ---");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/register <name> <pass> - Register new account");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/login <name> <pass> - Login to your account");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/logout - Logout from your account");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/password - Change account password");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "--- Account ---");
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/shop"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "----- Shop -----");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/range [10 score] - Buy range zombie hammer");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/heart [20 score] - Buy 1 heart if you zombie team");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/jump [3 score] - Buy 1 other jump");
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/news"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Added basic ranked system");
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/policehelp"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/kick <id> - kick player ids");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/freeze <id> - freeze/unfreeze player");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/idlist - view ids players");
		return;
	}
	else if(!strcmp(Msg->m_pMessage, "/cmdlist"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ Welcome to cmdlist! ~~~~~~~~");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/register, /login, /logout - account system");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/rules, /help, /info - information");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/stats, /upgr - upgrade system");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/shop - shop score tees");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "/idlist - List IDs players");	
		if(m_pPlayer->m_AccData.m_PlayerState == 1)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "#This command police group");
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "#/policehelp - help for police group");				
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "~~~~~~~~ Welcome to cmdlist! ~~~~~~~~");
		return;
	}	
	else if(!strcmp(Msg->m_pMessage, "/rules"))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 1 nope can sell");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 2 dont farm (freeze acc)");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 3 dont insult (mute/ban 1-7 day)");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 4 dont share acc(only when agree)(freeze acc)");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 5 dont talk about other servers(ip ban)");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 6 dont use bots(perma ban)");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 7 dont teamkill (ban 1-7 day");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 8 dont sell accounts (perma ban)");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 9 no selfkill");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "rule 10 everyone who use bugs (freeze + ban)");
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/heart"))
	{
		LastChat();
		if (!m_pPlayer->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You are not logged in! Type '/account' for more information!");
		if (!GameServer()->GetPlayerChar(m_pPlayer->GetCID())) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Use only if you alive!");
		if (GameServer()->m_World.m_Paused) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please wait end round!");
		if(m_pPlayer->GetTeam() != TEAM_RED) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Available only zombies!");
		if (m_pPlayer->m_Score < 20) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Need 20 score!");

		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Done!");
		m_pPlayer->m_Score -= 20;
		m_pPlayer->m_ActivesLife = false;
		m_pPlayer->m_LifeActives = false;
		new CLifeHearth(&GameServer()->m_World, vec2(0, 0), m_pPlayer->GetCID());
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/jump"))
	{
		LastChat();
		if (!m_pPlayer->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You are not logged in! Type '/account' for more information!");
		if (!GameServer()->GetPlayerChar(m_pPlayer->GetCID())) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Use only if you alive!");
		if (GameServer()->m_World.m_Paused) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please wait end round!");
		if (m_pPlayer->m_Score < 3) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Need 3 score!");

		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Done!");
		m_pPlayer->m_JumpsShop++;
		m_pPlayer->m_Score -= 3;
		GameServer()->GetPlayerChar(m_pPlayer->GetCID())->m_Core.m_Jumps += 1;
		return;
	}
	else if (!strcmp(Msg->m_pMessage, "/range"))
	{
		LastChat();
		if (!m_pPlayer->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You are not logged in! Type '/account' for more information!");

		if (!GameServer()->GetPlayerChar(m_pPlayer->GetCID())) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Use only if you alive!");
		if (GameServer()->m_World.m_Paused) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please wait end round!");
		if (m_pPlayer->GetTeam() != TEAM_RED) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Available only zombies!");
		if (m_pPlayer->m_Score < 10) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Need 10 score!");

		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Done!");
		m_pPlayer->m_RangeShop = true;
		m_pPlayer->m_Score -= 10;
		return;
	}
	else if (!strncmp(Msg->m_pMessage, "/prefix", 7) && m_pPlayer->m_AccData.m_UserID && 
		(GameServer()->Server()->IsAuthed(m_pPlayer->GetCID()) || m_pPlayer->m_AccData.m_PlayerState == 2 || m_pPlayer->m_AccData.m_UserID == g_Config.m_SvOwnerAccID))
	{
		LastChat();
		char aBuf[24];
		m_pPlayer->m_Prefix ^= true;
		str_format(aBuf, sizeof(aBuf), "Your Prefix %s", m_pPlayer->m_Prefix ? "enable" : "disable");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		return;
	}
	else if (!strncmp(Msg->m_pMessage, "/pm", 3))
	{
		LastChat();
		int id;
		if (sscanf(Msg->m_pMessage, "/pm %d", &id) != 1)
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Use /pm <id> <text>");
		
		int cid2 = clamp(id, 0, (int)MAX_CLIENTS - 1);
		char pLsMsg[128];
		str_copy(pLsMsg, Msg->m_pMessage + 5, 128);
		GameServer()->SendPM(m_pPlayer->GetCID(), cid2, pLsMsg);
		return;
	}
	else if (!strncmp(Msg->m_pMessage, "/setlvl", 7) && GameServer()->Server()->IsAuthed(m_pPlayer->GetCID()))
	{
		LastChat();
		int id, size;
		if ((sscanf(Msg->m_pMessage, "/setlvl %d %d", &id, &size)) != 2)
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please use: /setlvl <id> <level>");

		int cid2 = clamp(id, 0, (int)MAX_CLIENTS - 1);
		if (!GameServer()->m_apPlayers[cid2]) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "There is no such player!'");
		if (!GameServer()->m_apPlayers[cid2]->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "The player is not logged in account!");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You had to change your level!'"), GameServer()->m_apPlayers[cid2]->m_AccData.m_Level = size, GameServer()->m_apPlayers[cid2]->m_AccData.m_Exp = 0, GameServer()->m_apPlayers[cid2]->m_pAccount->Apply();
		return;
	}
	else if (!strncmp(Msg->m_pMessage, "/setmoney", 9) && GameServer()->Server()->IsAuthed(m_pPlayer->GetCID()))
	{
		LastChat();
		int id, size;
		if ((sscanf(Msg->m_pMessage, "/setmoney %d %d", &id, &size)) != 2)
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please use: /setmoney <id> <money>");

		int cid2 = clamp(id, 0, (int)MAX_CLIENTS - 1);
		if (!GameServer()->m_apPlayers[cid2]) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "There is no such player!'");
		if (!GameServer()->m_apPlayers[cid2]->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "The player is not logged in account!");
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You had to change your level!'"), GameServer()->SendChatTarget(cid2, "Your money changed!'"), GameServer()->m_apPlayers[cid2]->m_AccData.m_Money = size, GameServer()->m_apPlayers[cid2]->m_pAccount->Apply();
		return;
	}
	else if (!strncmp(Msg->m_pMessage, "/freeze", 6) && (m_pPlayer->m_AccData.m_PlayerState == 1 || GameServer()->Server()->IsAuthed(m_pPlayer->GetCID())))
	{
		LastChat();
		int id;
		if(sscanf(Msg->m_pMessage, "/freeze %d", &id) != 1) 
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please use: /freeze <id>");
		
		int cid2 = clamp(id, 0, (int)MAX_CLIENTS - 1);
		if(cid2 == m_pPlayer->GetCID()) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You can not freeze your account!");
		if (!GameServer()->m_apPlayers[cid2]) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "There is no such player!'");
		if (!GameServer()->m_apPlayers[cid2]->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "The player is not logged in account!");

		GameServer()->m_apPlayers[cid2]->m_AccData.m_Freeze ^= true;
		GameServer()->m_apPlayers[cid2]->m_pAccount->Apply();
		
		char buf[128];
		str_format(buf, sizeof(buf), "You %s account player '%s'", GameServer()->m_apPlayers[cid2]->m_AccData.m_Freeze ? "Freeze":"Unfreeze", GameServer()->Server()->ClientName(cid2));
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), buf);		
		str_format(buf, sizeof(buf), "Your account %s police '%s'", GameServer()->m_apPlayers[cid2]->m_AccData.m_Freeze ? "Freeze":"Unfreeze", GameServer()->Server()->ClientName(m_pPlayer->GetCID()));
		GameServer()->SendChatTarget(cid2, buf);	
		dbg_msg("police", "Police '%s' freeze acc player '%s' login '%s'", GameServer()->Server()->ClientName(m_pPlayer->GetCID()), GameServer()->Server()->ClientName(cid2), GameServer()->m_apPlayers[cid2]->m_AccData.m_Username);			
		return;
	}
	else if (!strncmp(Msg->m_pMessage, "/kick", 5) && m_pPlayer->m_AccData.m_PlayerState == 1)
	{
		LastChat();
		int id;
		if ((sscanf(Msg->m_pMessage, "/kick %d", &id)) != 1)
			return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please use: /kick <id>");

		int cid2 = clamp(id, 0, (int)MAX_CLIENTS - 1);
		if (!GameServer()->m_apPlayers[cid2]) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "There is no such player!'");

		char buf[64];
		str_format(buf, sizeof(buf), "%s kicked by the Police", GameServer()->Server()->ClientName(cid2));
		GameServer()->SendChatTarget(-1, buf);		
			
		GameServer()->Server()->Kick(cid2, "You was kicked by the Police!");			
		return;
	}
	else if (!strncmp(Msg->m_pMessage, "/setgroup", 9) && GameServer()->Server()->IsAuthed(m_pPlayer->GetCID()))
	{
		LastChat();
		int id, size;
		if (sscanf(Msg->m_pMessage, "/setgroup %d %d", &id, &size) != 2)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Please use: /setgroup <id> <groupid>"); 
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Group ID: 0 - Removed, 1 - Police, 2 - VIP, 3 - Helper"); return;
		}
		int cid2 = clamp(id, 0, (int)MAX_CLIENTS - 1);
		char gname[4][12] = {"", "police", "vip", "helper"}, aBuf[64];

		if (!GameServer()->m_apPlayers[cid2]) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "There is no such player!");
		if (!GameServer()->m_apPlayers[cid2]->m_AccData.m_UserID) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "The player is not logged in account!");

		switch (size)
		{
			case 0:
				if (GameServer()->m_apPlayers[cid2]->m_AccData.m_PlayerState)
				{
					str_format(aBuf, sizeof(aBuf), "Removed group %s for player '%s'", gname[GameServer()->m_apPlayers[cid2]->m_AccData.m_PlayerState], GameServer()->Server()->ClientName(cid2));
					GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
					str_format(aBuf, sizeof(aBuf), "Your group removed %s!", gname[GameServer()->m_apPlayers[cid2]->m_AccData.m_PlayerState]);
					GameServer()->SendChatTarget(cid2, aBuf);
					GameServer()->m_apPlayers[cid2]->m_AccData.m_PlayerState = 0;
				}
				else GameServer()->SendChatTarget(m_pPlayer->GetCID(), "This player already no in group!"); break;
			default:
				if(size > 3 || size < 0) return GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Group ID not found!");
				GameServer()->m_apPlayers[cid2]->m_AccData.m_PlayerState = size;
				str_format(aBuf, sizeof(aBuf), "Set group %s for player '%s'", gname[GameServer()->m_apPlayers[cid2]->m_AccData.m_PlayerState], GameServer()->Server()->ClientName(cid2));
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
				str_format(aBuf, sizeof(aBuf), "Your group set %s!", gname[GameServer()->m_apPlayers[cid2]->m_AccData.m_PlayerState]);
				GameServer()->SendChatTarget(cid2, aBuf); break;
		}
		return;
	}

	if(!strncmp(Msg->m_pMessage, "/", 1))
	{
		LastChat();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Wrong command. Use /cmdlist");
	}

}

void CCmd::LastChat()
{
	 m_pPlayer->m_LastChat = GameServer()->Server()->Tick();
}
