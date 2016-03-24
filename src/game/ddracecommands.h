/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_DDRACECOMMANDS_H
#define GAME_SERVER_DDRACECOMMANDS_H
#undef GAME_SERVER_DDRACECOMMANDS_H // this file can be included several times
#ifndef CONSOLE_COMMAND
#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help)
#endif

CONSOLE_COMMAND("kill_pl", "v[id]", CFGFLAG_SERVER, ConKillPlayer, this, "Kills player v and announces the kill")
CONSOLE_COMMAND("totele", "i[number]", CFGFLAG_SERVER|CMDFLAG_TEST, ConToTeleporter, this, "Teleports you to teleporter v")
CONSOLE_COMMAND("totelecp", "i[number]", CFGFLAG_SERVER|CMDFLAG_TEST, ConToCheckTeleporter, this, "Teleports you to checkpoint teleporter v")
CONSOLE_COMMAND("tele", "v[id] ?i[number]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleport, this, "Teleports you (or player v) to player i")
CONSOLE_COMMAND("left", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoLeft, this, "Makes you move 1 tile left")
CONSOLE_COMMAND("right", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoRight, this, "Makes you move 1 tile right")
CONSOLE_COMMAND("up", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoUp, this, "Makes you move 1 tile up")
CONSOLE_COMMAND("down", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoDown, this, "Makes you move 1 tile down")

CONSOLE_COMMAND("move", "i[x] i[y]", CFGFLAG_SERVER|CMDFLAG_TEST, ConMove, this, "Moves to the tile with x/y-number ii")
CONSOLE_COMMAND("move_raw", "i[x] i[y]", CFGFLAG_SERVER|CMDFLAG_TEST, ConMoveRaw, this, "Moves to the point with x/y-coordinates ii")

CONSOLE_COMMAND("mute", "", CFGFLAG_SERVER, ConMute, this, "");
CONSOLE_COMMAND("muteid", "v[id] i[minutes]", CFGFLAG_SERVER, ConMuteID, this, "");
CONSOLE_COMMAND("muteip", "s[ip] i[minute]", CFGFLAG_SERVER, ConMuteIP, this, "");
CONSOLE_COMMAND("unmute", "v[id]", CFGFLAG_SERVER, ConUnmute, this, "");
CONSOLE_COMMAND("mutes", "", CFGFLAG_SERVER, ConMutes, this, "");
#undef CONSOLE_COMMAND

#endif
