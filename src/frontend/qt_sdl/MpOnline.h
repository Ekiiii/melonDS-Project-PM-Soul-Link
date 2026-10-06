/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

// ---------------------------------------------------------------------------
// Project PM online relay (PMRELAY1) control surface.
//
// The relay transport itself lives with the rest of the fork bridge in
// EmuThread.cpp; this is the small thread-safe surface the Qt UI uses to start
// an online session and to display its state.  Nothing here pulls in winsock,
// so Qt sources can include it freely: the resolved relay address travels as
// opaque bytes.
//
// Threading: MpOnlineResolve() blocks (DNS) and must be called from the UI
// thread at click time, never from the emulation thread's frame loop.  The
// start/stop calls only post a request; the emulation thread picks it up on
// its next bridge pump.
// ---------------------------------------------------------------------------

#ifndef MPONLINE_H
#define MPONLINE_H

// A relay server address resolved once, ahead of any frame work.
struct MpOnlineTarget
{
    unsigned char addr[32] = {};    // sockaddr_in, opaque to the UI
    int addrLen = 0;
    char disp[96] = "";             // "host:port" for display
};

struct MpOnlineStatus
{
    int mode = 0;                   // 0 off, 1 hosting online, 2 joined online
    int peers = 0;                  // live game links
    bool pending = false;           // request posted, emulation has not run yet
    char code[8] = "";              // room code (empty until the relay assigns)
    char server[96] = "";
    char text[128] = "";            // connection state / last error, for the UI

    // Lobby roster, indexed by role 1..8 (slot 0 unused).  Names arrive over
    // the shared 0xFE frame, so they are known before anyone starts playing.
    char roster[9][24] = {};
    int rosterPing[9] = {};         // that player's ping to the host, ms
    int myRole = 0;
};

// Resolves "host" or "host:port" (blocking).  false = could not resolve.
bool MpOnlineResolve(const char* server, int defPort, MpOnlineTarget* out);

// Post a start request.  Takes precedence over the native LAN session follow.
void MpOnlineHost(const MpOnlineTarget& t, const char* name);
void MpOnlineJoin(const MpOnlineTarget& t, const char* code, const char* name);
void MpOnlineStop();

void MpOnlineGetStatus(MpOnlineStatus* out);

// The bridge wire protocol version, as advertised by the LAN beacon.
int MpOnlineWireVersion();

// Default relay port when the player types a bare host name.
const int MpOnlineDefaultPort = 7833;

// The community relay, pre-filled into the online dialogs so players never
// have to type an address.  A relay saved in the config always wins over this,
// and the env-var harness is unaffected: it still requires MELONDS_AP_RELAY.
const char* const MpOnlineDefaultRelay = "193.122.236.144:7833";

#endif // MPONLINE_H
