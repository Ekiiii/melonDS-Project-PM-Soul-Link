<p align="center"><img src="https://raw.githubusercontent.com/melonDS-emu/melonDS/master/res/icon/melon_128x128.png"></p>
<h2 align="center"><b>melonDS - Live OBS Overlay & Project PM Edition</b></h2>
<p align="center">
<a href="http://melonds.kuribo64.net/" alt="melonDS website"><img src="https://img.shields.io/badge/website-melonds.kuribo64.net-%2331352e.svg"></a>
<a href="https://www.gnu.org/licenses/gpl-3.0" alt="License: GPLv3"><img src="https://img.shields.io/badge/License-GPL%20v3-%23ff554d.svg"></a>
<a href="https://discord.gg/pAMAtExcqV" alt="Discord"><img src="https://img.shields.io/badge/Discord-Kuribo64-7289da?logo=discord&logoColor=white"></a>
<br>
<a href="https://github.com/Ekiiii/melonDS-Project-PM-OBS-Overlay/actions/workflows/build-windows.yml?query=event%3Apush"><img src="https://github.com/Ekiiii/melonDS-Project-PM-OBS-Overlay/actions/workflows/build-windows.yml/badge.svg" /></a>
</p>

<p align="center">
  <b>🇫🇷 <a href="README_FR.md">Cliquez ici pour lire la documentation en Français</a></b>
</p>

---

## Live OBS Overlay Edition ([melonDS-Project-PM-OBS-Overlay](https://github.com/Ekiiii/melonDS-Project-PM-OBS-Overlay))

This fork by **Ekiiii** extends melonDS and Project PM with a broadcast-ready **Live OBS / Twitch Streaming Overlay**, real-time sub-frame RAM decryption, and high-performance **Direct P2P connectivity** with 1-click UPnP automatic port forwarding.

It is designed to give streamers, content creators, and co-op players a seamless, zero-config on-screen overlay of their team and their friends' teams with animated sprites, live stats, and customizable retro styling.

### 🌟 Key Additions & Features

#### 1. 🎥 Multi-Player Live OBS / Twitch Streaming Overlay (`http://localhost:8080/overlay`)
* **Sub-frame RAM Decryption**: Directly inspects Gen 4 RAM structures (`BoxMon` & `PartyPokemon`) using official Gen 4 LCRNG decryption (with PID seed) to report accurate HP, levels, real nicknames, fainted states, and status effects with zero latency.
* **Automatic Player Nicknames**: Displays each participant's configured nickname from melonDS (`Online.PlayerName` or `Firmware.Username`, e.g. *Ekiii*, *Ted*) directly on their team card and filter controls instead of generic role numbers.
* **Smooth Animated GIFs**: Intelligent DOM node retention ensures animated Pokémon sprites keep playing continuously without reset-flickering on polling updates.
* **Enlarged Cards & Dynamic Sprite Scaling**: Generous 172×184px cards, 130px Pokéball watermark, and adjustable Pokémon sprite scaling (`100%`, `135%` default, `165%`, `200%`) so even smaller Pokémon (like Piplup) fill the card beautifully.
* **Multi-Player Support (Up to 8 Players)**: Display only your team (`?player=me`) or all connected peers (`?player=all`) with automatic active-player filtering.
* **5 Broadcast Layout Modes**:
  * **Vertical** (`2×3` slots stacked): Classic stream sidebar.
  * **Horizontal** (`3×2` slots side-by-side): Ideal for wide 16:9 bottom layouts.
  * **Grid** (`2×2` co-op layout): Perfect for two streams side-by-side.
  * **Bar** (`1×6` horizontal strip): Ultra-compact bottom banner.
  * **Sidebar** (`6×1` vertical strip): Ultra-narrow side column.
* **Universal Mode Support**: Compatible with vanilla adventure play, co-op multiplayer, Nuzlocke challenges, and SoulLocke runs.
  * Includes flat vector badges (`LIÉ`, `LIÉ (PC)`, `EN ATTENTE`, `ÂME BRISÉE`, `K.O.`) with a configurator toggle (`Auto`, `Always Show`, `Always Hide`).

#### 2. 🎨 Integrated Retro Customization Dashboard
* Floating **`[ ⚙ CONFIGURE OVERLAY ]`** button that opens a comprehensive retro settings panel (580px wide).
* **6 Preset Themes**: Cyan Neon, Platinum Red, Emerald Nuzlocke, Amethyst Night, Retro Gold, and Minimalist Slate.
* **Custom Color Pickers**: Full color customization for Accent color (borders/titles) and Background color.
* **Background Opacity**: `95% (Opaque)`, `75% (Semi-transparent)`, or `0% (Chroma/Transparent for OBS)`.
* **Global Zoom**: 100%, 125%, 150%, 175%, 200%.
* **Bilingual Support (EN / FR)**: Automatic synchronization with melonDS language setting (`Options -> Language`), with dynamic live translation of all texts and all 493 Pokémon names (e.g. *Tiplouf* $\leftrightarrow$ *Piplup*), plus manual language switch buttons.
* **1-Click OBS Studio Export**: **`[ COPY OBS STUDIO URL ]`** button that embeds all layout, scale, and theme preferences while automatically hiding the configurator panel in OBS (`?obs=1`).

#### 3. 🌐 Direct P2P & Automatic UPnP Port Forwarding
* **Zero Relay Dependency**: Connect directly peer-to-peer using high-performance streaming TCP sockets without relying on public relay servers or local LAN discovery.
* **Automatic UPnP**: Automatically requests port mapping on your router/gateway (`TCP 7820`) in one click—no manual port forwarding or router configuration needed for most home setups.
* **Integrated Windows Firewall Helper**: Ensures inbound rules are present with a single prompt.
* **1-Click Local Testing**: Instantly test two emulator instances on the same machine via `127.0.0.1:7820`.

#### 4. 🔑 Short, Shareable Room Codes (`SL-XXXXX-XXXXX`)
* Hosts don't need to look up or share raw IP addresses. The emulator encodes the public IP and port into a clean, 10-character Base32 room code (e.g. `SL-4LADD-A69NE`).
* One-click copy for the host, instant decoding for joining players.
* Also accepts raw IPv4 addresses or virtual LAN IPs (Radmin VPN, Tailscale, ZeroTier) seamlessly.

#### 5. 🖥️ Dedicated Direct P2P Window (`System -> Multiplayer -> Direct P2P...`)
* **Host Tab**: One-click session creation, large room code display, copy button, UPnP diagnostic indicator, live roster with latency/ping times, and session controls.
* **Join Tab**: Room code / IP input, instant decoding, connection status, and connected roster.

#### 6. ⚔️ Advanced Co-op & Challenge Run Features
* Automatic death detection via PC Box 18 ("CIMETIERE") or in-battle fainting.
* Real-time pair & zone synchronization for co-op challenges with safe out-of-battle party compaction.

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
 * **The Project PM Team** (Original creators of Pokémon Platinum Multiplayer):
   * **ComicartOlie** - Lead Developer & Multiplayer Bridge Architecture
   * **nUt** (nUt0225) - Developer & Core Systems
   * **MottledAbyss** - Developer & Game Balancer
 * **Ekiiii** for the Live OBS streaming overlay, Direct P2P with UPnP, real-time RAM decryption, and challenge run features.

---

## License

[![GNU GPLv3](https://www.gnu.org/graphics/gplv3-127x51.png)](https://www.gnu.org/licenses/gpl-3.0.html)

melonDS is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option)
any later version.
