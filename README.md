<p align="center"><img src="https://raw.githubusercontent.com/melonDS-emu/melonDS/master/res/icon/melon_128x128.png"></p>
<h2 align="center"><b>melonDS</b></h2>
<p align="center">
<a href="http://melonds.kuribo64.net/" alt="melonDS website"><img src="https://img.shields.io/badge/website-melonds.kuribo64.net-%2331352e.svg"></a>
<a href="http://melonds.kuribo64.net/downloads.php" alt="Release: 1.1"><img src="https://img.shields.io/badge/release-1.1-%235c913b.svg"></a>
<a href="https://www.gnu.org/licenses/gpl-3.0" alt="License: GPLv3"><img src="https://img.shields.io/badge/License-GPL%20v3-%23ff554d.svg"></a>
<a href="https://kiwiirc.com/client/irc.badnik.net/?nick=IRC-Source_?#melonds" alt="IRC channel: #melonds"><img src="https://img.shields.io/badge/IRC%20chat-%23melonds-%23dd2e44.svg"></a>
<a href="https://discord.gg/pAMAtExcqV" alt="Discord"><img src="https://img.shields.io/badge/Discord-Kuribo64-7289da?logo=discord&logoColor=white"></a>
<br>
<a href="https://github.com/Ekiiii/melonDS-Project-PM-Soul-Link/actions/workflows/build-windows.yml?query=event%3Apush"><img src="https://github.com/Ekiiii/melonDS-Project-PM-Soul-Link/actions/workflows/build-windows.yml/badge.svg" /></a>
</p>

<p align="center">
  <b>🇫🇷 <a href="README_FR.md">Cliquez ici pour lire la documentation en Français</a></b>
</p>

DS emulator, sorta

The goal is to do things right and fast, akin to blargSNES (but hopefully better). But also to, you know, have a fun challenge :)
## SoulLocke Edition ([melonDS-Project-PM-Soul-Link](https://github.com/Ekiiii/melonDS-Project-PM-Soul-Link))

This fork by **Ekiiii** extends melonDS and Project PM specifically for **SoulLocke** co-op adventures, bringing direct P2P connections, one-click hosting with UPnP, and a live broadcast-ready streaming overlay.

### Key Additions & Features

1. **Direct P2P & Automatic UPnP Port Forwarding**:
   - **Zero Relay Dependency**: Connect directly peer-to-peer using high-performance TCP streaming sockets without relying on public relay servers or local LAN discovery.
   - **Automatic UPnP**: Automatically requests port mapping on your router/gateway (`TCP 7820`) in one click—no manual router configuration or port-forwarding menus needed for most home routers.
   - **Integrated Windows Firewall Helper**: Ensures inbound rules are present with a single prompt.

2. **Short, Shareable Room Codes (`SL-XXXXX-XXXXX`)**:
   - Hosts don't need to look up or share raw IP addresses. The emulator packs the public IP and port into a clean, 10-character Base32 room code (e.g. `SL-4LADD-A69NE`).
   - One-click copy for the host, instant decoding for joining players.
   - Also accepts raw IPv4 addresses or virtual LAN IPs (Radmin VPN, Tailscale, ZeroTier) seamlessly.

3. **Dedicated Direct P2P Window**:
   - Custom graphical interface (`System -> Multiplayer -> Direct P2P SoulLocke...`) featuring:
     - **Host Tab**: One-click session creation, large room code display, copy button, UPnP diagnostic indicator, live roster with latency/ping times, and session controls.
     - **Join Tab**: Room code / IP input, instant decoding, connection status, and connected roster.

4. **Multi-Player Live OBS / Twitch HTML Overlay (1 to 8 Players)**:
   - Built-in lightweight HTTP server running on `http://localhost:8080/overlay`.
   - **Real-Time Memory & Encryption Decoding**: Automatically decrypts Gen 4 BoxMon structures directly from Nintendo DS RAM to read active party HP, levels, types, and fainted states with sub-frame latency.
   - **Automatic SoulLocke Cluster Detection**: Automatically identifies linked Pokémon across all connected players by matching catch locations (`met_location`).
   - **Soul Status Badges**:
     - `🔗 LIÉ [Zone X]` (Active SoulLocke link)
     - `🔗 ÂME BRISÉE` (Broken Soul / Death alert)
   - **Streamer Solo Mode**: `http://localhost:8080/overlay?player=me` displays an ultra-compact 1:1 retro pixel-art square slots layout tailored for OBS streamers with zero wasted space.
   - **Cluster View**: `http://localhost:8080/overlay?player=all` renders all connected party cards (up to 8 players) with customizable layouts (`?layout=horizontal` or `?layout=vertical`).

<hr>

## Project PM fork

The [`platinum-mp`](https://github.com/ComicartOlie/melonDS-Project-PM/tree/platinum-mp)
branch carries the embedded multiplayer bridge for
**Project PM**, a co-op multiplayer romhack of Pokémon Platinum. Hosting or
joining a LAN game also syncs the romhack's multiplayer mailboxes with the
other players. The sibling DeSmuME port of the same bridge lives at
[DeSmuME bridge](https://github.com/ComicartOlie/Desmume-Project-PM).
All credit for the emulator itself goes to the melonDS team.

### Hosting over the internet

One player hosts ("Host LAN game" in melonDS's Multiplayer menu); everyone
else joins with the host's IP. On the same LAN or a VPN (Hamachi, Radmin,
ZeroTier, Tailscale) this works with no setup. To host over the open
internet, three things must all be true on the **host's** side. Joiners
never need any of this:

1. **Router port forwards**: melonDS needs **two** ports forwarded to the
   host PC: **UDP 7064** (melonDS's LAN session) and **TCP 7820** (the mod's
   sync bridge). Forwarding only 7820 is the most common mistake; the
   session can never form without 7064.
2. **Windows Firewall**: the router forwards the connection, but Windows
   still has to accept it. The first time you host, the emulator offers to
   add the firewall rule for you (one admin prompt, one time). Say yes.
3. **A real public IP**: if your router's WAN address (in its admin page)
   is different from what whatismyip.com shows, or starts with
   100.64-100.127, your ISP has you behind CGNAT and no amount of port
   forwarding will work. Use a VPN like Hamachi/ZeroTier, or have a friend
   with a real IP host.

## How to use

Firmware boot (not direct boot) requires a BIOS/firmware dump from an original DS or DS Lite.
DS firmwares dumped from a DSi or 3DS aren't bootable and only contain configuration data, thus they are only suitable when booting games directly.

### Possible firmware sizes

 * 128KB: DSi/3DS DS-mode firmware (reduced size due to lacking bootcode)
 * 256KB: regular DS firmware
 * 512KB: iQue DS firmware

DS BIOS dumps from a DSi or 3DS can be used with no compatibility issues. DSi BIOS dumps (in DSi mode) are not compatible. Or maybe they are. I don't know.

As for the rest, the interface should be pretty straightforward. If you have a question, don't hesitate to ask, though!

## How to build
See [BUILD.md](./BUILD.md) for build instructions.

## TODO LIST

 * better DSi emulation
 * better OpenGL rendering
 * netplay
 * the impossible quest of pixel-perfect 3D graphics
 * support for rendering screens to separate windows
 * emulating some fancy addons
 * other non-core shit (debugger, graphics viewers, etc)

### TODO LIST FOR LATER (low priority)

 * big-endian compatibility (Wii, etc)
 * LCD refresh time (used by some games for blending effects)
 * any feature you can eventually ask for that isn't outright stupid

## Credits

 * Martin for GBAtek, a good piece of documentation
 * Cydrak for the extra 3D GPU research
 * limittox for the icon
 * All of you comrades who have been testing melonDS, reporting issues, suggesting shit, etc

## Licenses

[![GNU GPLv3 Image](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

melonDS is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

### External
* Images used in the Input Config Dialog - see `src/frontend/qt_sdl/InputConfig/resources/LICENSE.md`
