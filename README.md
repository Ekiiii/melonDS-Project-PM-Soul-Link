<p align="center"><img src="https://raw.githubusercontent.com/melonDS-emu/melonDS/master/res/icon/melon_128x128.png"></p>
<h2 align="center"><b>melonDS - SoulLocke & Project PM Edition</b></h2>
<p align="center">
<a href="http://melonds.kuribo64.net/" alt="melonDS website"><img src="https://img.shields.io/badge/website-melonds.kuribo64.net-%2331352e.svg"></a>
<a href="https://www.gnu.org/licenses/gpl-3.0" alt="License: GPLv3"><img src="https://img.shields.io/badge/License-GPL%20v3-%23ff554d.svg"></a>
<a href="https://discord.gg/pAMAtExcqV" alt="Discord"><img src="https://img.shields.io/badge/Discord-Kuribo64-7289da?logo=discord&logoColor=white"></a>
<br>
<a href="https://github.com/Ekiiii/melonDS-Project-PM-Soul-Link/actions/workflows/build-windows.yml?query=event%3Apush"><img src="https://github.com/Ekiiii/melonDS-Project-PM-Soul-Link/actions/workflows/build-windows.yml/badge.svg" /></a>
</p>

<p align="center">
  <b>🇫🇷 <a href="README_FR.md">Cliquez ici pour lire la documentation en Français</a></b>
</p>

---

## SoulLocke Edition ([melonDS-Project-PM-Soul-Link](https://github.com/Ekiiii/melonDS-Project-PM-Soul-Link))

This fork by **Ekiiii** extends melonDS and Project PM specifically for **SoulLocke** co-op adventures, introducing direct P2P connectivity, one-click UPnP port forwarding, automated death & pair synchronization, and a live broadcast-ready streaming overlay with a rich retro customization dashboard.

### Key Additions & Features

1. **Direct P2P & Automatic UPnP Port Forwarding**:
   - **Zero Relay Dependency**: Connect directly peer-to-peer using high-performance streaming TCP sockets without relying on public relay servers or local LAN discovery.
   - **Automatic UPnP**: Automatically requests port mapping on your router/gateway (`TCP 7820`) in one click—no manual port forwarding or router configuration needed for most home setups.
   - **Integrated Windows Firewall Helper**: Ensures inbound rules are present with a single prompt.
   - **1-Click Local Testing**: Instantly test two emulator instances on the same machine via `127.0.0.1:7820`.

2. **Short, Shareable Room Codes (`SL-XXXXX-XXXXX`)**:
   - Hosts don't need to look up or share raw IP addresses. The emulator encodes the public IP and port into a clean, 10-character Base32 room code (e.g. `SL-4LADD-A69NE`).
   - One-click copy for the host, instant decoding for joining players.
   - Also accepts raw IPv4 addresses or virtual LAN IPs (Radmin VPN, Tailscale, ZeroTier) seamlessly.

3. **Dedicated Direct P2P Window**:
   - Custom graphical interface (`System -> Multiplayer -> Direct P2P SoulLocke...`) featuring:
     - **Host Tab**: One-click session creation, large room code display, copy button, UPnP diagnostic indicator, live roster with latency/ping times, and session controls.
     - **Join Tab**: Room code / IP input, instant decoding, connection status, and connected roster.

4. **Multi-Player Live OBS / Twitch Streaming Overlay (`http://localhost:8080/overlay`)**:
   - Built-in lightweight HTTP server serving a zero-dependency HTML5/CSS/JS overlay.
   - **Sub-frame RAM Decryption**: Automatically decrypts Gen 4 `BoxMon` and `PartyPokemon` structures directly from Nintendo DS RAM using the official Gen 4 LCRNG algorithm (with PID seed) to report accurate HP, levels, species, and fainted states.
   - **Smooth Animated GIFs**: Intelligent DOM node retention prevents animated Pokemon GIFs from resetting their animation cycle on every polling tick.
   - **Enlarged Cards & Dynamic Sprite Scaling**: Generous 172×184px cards, 130px Pokéball watermark, and adjustable Pokémon sprite scaling (`100%`, `135%` default, `165%`, `200%`) so even smaller Pokémon (like Piplup) fill the card beautifully.
   - **Automatic SoulLocke Pair Detection**: Matches caught Pokémon across players using encounter zone IDs (`met_location`).
   - **100% Flat Vector Badges (No 3D Emojis)**:
     - `LIÉ` / `LINKED`: Active soul link (partner in party)
     - `LIÉ (PC)` / `LINKED (PC)`: Active soul link (partner in PC box)
     - `EN ATTENTE` / `PENDING`: Player has caught a Pokémon in a new zone, waiting for partner's encounter
     - `ÂME BRISÉE` / `SOUL BROKEN`: Fainted Pokémon / Broken soul pair
     - `K.O.` / `FAINTED`: Fainted status indicator
   - **5 Broadcast Layout Modes**:
     - **Vertical** (`2×3` slots stacked): Classic stream sidebar.
     - **Horizontal** (`3×2` slots side-by-side): Ideal for wide 16:9 layouts.
     - **Grid** (`2×2` co-op layout): Perfect for two streams side-by-side.
     - **Bar** (`1×6` horizontal strip): Perfect for a stream bottom banner.
     - **Sidebar** (`6×1` vertical strip): Ultra-narrow side column.

5. **Integrated Retro Customization Dashboard**:
   - Floating `[ ⚙ CONFIGURE OVERLAY ]` button that opens a comprehensive retro settings panel (580px wide).
   - **6 Preset Themes**: Cyan Neon, Platinum Red, Emerald Nuzlocke, Amethyst Night, Retro Gold, and Minimalist Slate.
   - **Custom Color Pickers**: Full customization for Accent color (borders/titles) and Background color.
   - **Background Opacity**: `95% (Opaque)`, `75% (Semi-transparent)`, or `0% (Chroma/Transparent for OBS)`.
   - **Global Zoom**: 100%, 125%, 150%, 175%, 200%.
   - **Bilingual Support (EN / FR)**: Automatic synchronization with melonDS language setting (`Options -> Language`), with dynamic live translation of all texts and all 493 Pokémon names (e.g. *Tiplouf* $\leftrightarrow$ *Piplup*), plus manual language switch buttons.
   - **OBS Studio Export**: 1-click `[ COPY OBS STUDIO URL ]` button that embeds all layout, scale, and theme preferences while automatically hiding the configurator panel in OBS (`?obs=1`).

6. **Automatic Death & SoulLocke Synchronization**:
   - When a Pokémon faints in battle or is sent to PC Box 18 ("CIMETIERE"), its zone is marked dead and synced to all peers over P2P.
   - In the overworld (out of battle), the partner's linked Pokémon is automatically removed from their party and remaining slots are safely compacted without RAM corruption.
   - If the partner is currently in battle, the removal safely waits until the battle concludes to prevent mid-battle desyncs or flickering.

---

## Project PM fork

The [`platinum-mp`](https://github.com/ComicartOlie/melonDS-Project-PM/tree/platinum-mp)
branch carries the embedded multiplayer bridge for
**Project PM**, a co-op multiplayer romhack of Pokémon Platinum. Hosting or
joining a LAN game also syncs the romhack's multiplayer mailboxes with the
other players. The sibling DeSmuME port of the same bridge lives at
[DeSmuME bridge](https://github.com/ComicartOlie/Desmume-Project-PM).
All credit for the emulator itself goes to the melonDS team.

### Hosting over the internet (Legacy Mode)

One player hosts ("Host LAN game" in melonDS's Multiplayer menu); everyone
else joins with the host's IP. On the same LAN or a VPN (Hamachi, Radmin,
ZeroTier, Tailscale) this works with no setup. To host over the open
internet without Direct P2P UPnP, three things must all be true on the **host's** side:

1. **Router port forwards**: melonDS needs **two** ports forwarded to the
   host PC: **UDP 7064** (melonDS's LAN session) and **TCP 7820** (the mod's
   sync bridge).
2. **Windows Firewall**: Inbound connections must be allowed.
3. **A real public IP**: Avoid CGNAT (IP starting with 100.64.* to 100.127.*).

---

## How to use

Firmware boot (not direct boot) requires a BIOS/firmware dump from an original DS or DS Lite.
DS firmwares dumped from a DSi or 3DS aren't bootable and only contain configuration data, thus they are only suitable when booting games directly.

### Possible firmware sizes

 * 128KB: DSi/3DS DS-mode firmware (reduced size due to lacking bootcode)
 * 256KB: regular DS firmware
 * 512KB: iQue DS firmware

---

## How to build

See [BUILD.md](./BUILD.md) for build instructions.

---

## Credits

 * **Martin** for GBAtek, a good piece of documentation
 * **Cydrak** for 3D GPU research
 * **limittox** for the application icon
 * **The melonDS team** and contributors
 * **Ekiiii** for the SoulLocke implementation, Direct P2P with UPnP, automatic death sync, and OBS streaming overlay.

---

## License

[![GNU GPLv3](https://www.gnu.org/graphics/gplv3-127x51.png)](https://www.gnu.org/licenses/gpl-3.0.html)

melonDS is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option)
any later version.
