<p align="center"><img src="https://raw.githubusercontent.com/melonDS-emu/melonDS/master/res/icon/melon_128x128.png"></p>
<h2 align="center"><b>melonDS - Édition Overlay OBS Live & Project PM</b></h2>
<p align="center">
<a href="http://melonds.kuribo64.net/" alt="Site web melonDS"><img src="https://img.shields.io/badge/site_web-melonds.kuribo64.net-%2331352e.svg"></a>
<a href="https://www.gnu.org/licenses/gpl-3.0" alt="Licence : GPLv3"><img src="https://img.shields.io/badge/Licence-GPL%20v3-%23ff554d.svg"></a>
<a href="https://discord.gg/pAMAtExcqV" alt="Discord"><img src="https://img.shields.io/badge/Discord-Kuribo64-7289da?logo=discord&logoColor=white"></a>
<br>
<a href="https://github.com/Ekiiii/melonDS-Project-PM-OBS-Overlay/actions/workflows/build-windows.yml?query=event%3Apush"><img src="https://github.com/Ekiiii/melonDS-Project-PM-OBS-Overlay/actions/workflows/build-windows.yml/badge.svg" /></a>
</p>

<p align="center">
  <b>🇬🇧 <a href="README.md">Click here to read the documentation in English</a></b>
</p>

---

## Édition Overlay OBS Live ([melonDS-Project-PM-OBS-Overlay](https://github.com/Ekiiii/melonDS-Project-PM-OBS-Overlay))

Ce fork développé par **Ekiiii** enrichit melonDS et Project PM d'un **Overlay de Streaming OBS / Twitch en direct**, d'un moteur de déchiffrement RAM temps réel à faible latence, et d'une connectivité **Direct P2P** ultra-performante avec redirection automatique de port UPnP en un clic.

Conçu pour les streamers, les créateurs de contenu et les joueurs en coop, cet outil affiche à l'écran l'équipe du joueur et celles de ses amis avec des sprites animés, des barres de statut précises et un panneau de configuration rétro hautement personnalisable.

### 🌟 Fonctionnalités Clés

#### 1. 🎥 Overlay HTML Twitch & OBS en Direct (`http://localhost:8080/overlay`)
* **Déchiffrement Mémoire Temps Réel (Sub-Frame)** : Déchiffre les structures `BoxMon` et `PartyPokemon` directement depuis la mémoire vive ARM9 de la Nintendo DS (algorithme LCRNG officiel 4G avec seed PID) pour restituer les vrais PV, niveaux, espèces, statuts et états K.O. instantanément sans temps de latence.
* **Pseudos Personnalisés de l'Émulateur** : Récupère et affiche automatiquement le vrai pseudo configuré dans melonDS (`Online.PlayerName` ou `Firmware.Username`, ex. *Ekiii*, *Ted*) sur les cartes d'équipe et les filtres, remplaçant les libellés génériques.
* **Animation Fluide des GIFs** : Maintien dynamique des nœuds DOM pour éviter que les GIFs animés des Pokémon ne redémarrent en boucle à chaque rafraîchissement.
* **Cadres Agrandis & Sprites Dynamiques** : Cases spacieuses de 172×184px, filigrane Pokéball de 130px et échelle des Pokémon dynamique (`100%`, `135%` par défaut, `165%`, `200%`) pour que même les petits Pokémon (comme Tiplouf) remplissent harmonieusement le cadre.
* **Support Multi-Joueurs (Jusqu'à 8 Joueurs)** : Affichez uniquement votre équipe (`?player=me`) ou tous les pairs connectés (`?player=all`) avec filtrage automatique des joueurs inactifs.
* **5 Dispositions de Layout au Choix** :
  * **Vertical** (`2×3` par joueur superposés) : Format classique compact pour bandeau latéral.
  * **Horizontal** (`3×2` côte à côte) : Optimisé pour les scènes larges 16:9.
  * **Grille Co-op** (`2×2`) : Conçu pour afficher 2 streams côte à côte.
  * **Bandeau Bas (Bar)** (`1×6` horizontal) : Parfait pour une bannière au bas de l'écran.
  * **Colonne (Sidebar)** (`6×1` vertical) : Barre latérale ultra-fine pour streamers.
* **Compatibilité Tous Modes de Jeu** : Fonctionne aussi bien en aventure classique, en co-op multijoueur, en défi Nuzlocke qu'en SoulLocke.
  * Badges vectoriels flat (`LIÉ`, `LIÉ (PC)`, `EN ATTENTE`, `ÂME BRISÉE`, `K.O.`) avec sélecteur dans le configurateur (`Automatique`, `Toujours Afficher`, `Toujours Masquer`).

#### 2. 🎨 Panneau de Configuration Rétro Intégré
* Bouton flottant **`[ ⚙ CONFIGURER L'OVERLAY ]`** ouvrant un panneau de réglages ergonomique de 580px de large.
* **6 Thèmes Prédéfinis** : Cyan Néon, Rouge Platine, Émeraude Nuzlocke, Améthyste Nocturne, Or Rétro, et Minimaliste.
* **Sélecteurs de Couleurs Personnalisées** : Pipettes de sélection libre pour la couleur d'accent (titres, bordures) et la couleur de fond des boîtes.
* **Opacité du Fond Réglable** : `95% (Opaque)`, `75% (Semi-transparent)`, ou `0% (Transparent pour intégration OBS)`.
* **Échelle Globale (Zoom)** : 100%, 125%, 150%, 175%, 200%.
* **Support Bilingue (Français / Anglais)** : Synchronisation automatique avec la langue de melonDS (`Options -> Langue`), avec traduction dynamique de l'ensemble des textes et des 493 noms de Pokémon (ex. *Tiplouf* $\leftrightarrow$ *Piplup*), doublée d'un sélecteur manuel de langue.
* **Export OBS Studio Simplifié** : Bouton 1-clic **`[ COPIER LE LIEN OBS STUDIO ]`** intégrant tous les réglages et masquant automatiquement le panneau de configuration dans OBS (`?obs=1`).

#### 3. 🌐 Connexion P2P Directe & Redirection de Port UPnP Automatique
* **Sans serveur relais** : Connexion directe de joueur à joueur via des sockets TCP en streaming haute performance, sans dépendre de serveurs relais publics ni de la détection LAN.
* **UPnP Automatique** : Demande automatiquement l'ouverture et la redirection du port sur votre box Internet/routeur (`TCP 7820`) en un clic—aucune configuration manuelle de routeur n'est requise pour la plupart des box domestiques (Freebox, Livebox, SFR Box, Bbox, etc.).
* **Assistant Pare-feu Windows Intégré** : Configure les règles de trafic entrant automatiquement avec une seule invite administrateur.
* **Bouton Test Local** : Possibilité de tester deux instances sur la même machine (`127.0.0.1:7820`) en un clic.

#### 4. 🔑 Codes de Salon Courts et Partageables (`SL-XXXXX-XXXXX`)
* Plus besoin de chercher ni de divulguer votre adresse IP publique brute : l'émulateur compresse l'adresse IP et le port dans un code de salon en Base32 propre de 10 caractères (ex. `SL-4LADD-A69NE`).
* Copie en un clic pour l'hôte, décodage instantané pour le joueur qui rejoint.
* Accepte également directement les adresses IPv4 brutes et les VPN virtuels (Radmin VPN, Tailscale, ZeroTier) sans manipulation complexe.

#### 5. 🖥️ Interface Graphique Dédiée Direct P2P (`Système -> Multijoueur -> Direct P2P...`)
* **Onglet Héberger** : Création de salon en un clic, affichage du code de salon en grand format, bouton de copie, indicateur de statut UPnP, liste des joueurs connectés avec ping/latence en direct, et contrôles de session.
* **Onglet Rejoindre** : Saisie du code de salon ou de l'IP, décodage immédiat, état de la connexion et liste des participants.

#### 6. ⚔️ Fonctionnalités Avancées pour Coop & Défis
* Détection automatique des K.O. en combat et au cimetière (Boîte PC 18).
* Synchronisation en direct des paires et des zones pour les modes challenge, avec compaction propre de l'équipe hors combat sans corruption mémoire.

---

## Le Fork Project PM

La branche [`platinum-mp`](https://github.com/ComicartOlie/melonDS-Project-PM/tree/platinum-mp) intègre le pont multijoueur pour **Project PM**, une romhack multijoueur coopérative de Pokémon Platine. Héberger ou rejoindre une partie synchronise également les boîtes aux lettres multijoueur de la romhack entre les joueurs. Le portage équivalent sur DeSmuME est accessible sur [DeSmuME bridge](https://github.com/ComicartOlie/Desmume-Project-PM). Tous les mérites du cœur de l'émulateur reviennent à l'équipe officielle de melonDS.

### Hébergement via Internet (Mode Classique)

Un joueur héberge (« Héberger une partie LAN » dans le menu Multijoueur de melonDS) ; les autres joueurs rejoignent avec l'adresse IP de l'hôte. Sur un même réseau local (LAN) ou un VPN (Hamachi, Radmin, ZeroTier, Tailscale), cela fonctionne sans configuration particulière. Pour héberger sur Internet sans le mode Direct P2P UPnP, trois conditions doivent être respectées côté hôte :

1. **Redirection des ports du routeur** : melonDS requiert deux ports redirigés vers le PC hôte : **UDP 7064** (session LAN melonDS) et **TCP 7820** (pont de synchronisation du mod).
2. **Pare-feu Windows** : Le pare-feu doit autoriser les connexions entrantes.
3. **Adresse IP publique réelle** : Si votre fournisseur d'accès utilise le CGNAT (IP WAN commençant par 100.64.* à 100.127.*), utilisez un VPN ou activez une IP publique dédiée.

---

## Guide d'Utilisation

Le démarrage avec firmware (contrairement au démarrage direct de ROM) nécessite un dump de BIOS/firmware issu d'une Nintendo DS ou DS Lite originale. Les firmwares issus d'une DSi ou 3DS ne contiennent que des données de configuration et ne sont utilisables qu'en démarrage direct.

### Tailles possibles de firmware

* **128 Ko** : Firmware mode DS de DSi/3DS (taille réduite sans le code de boot initial).
* **256 Ko** : Firmware Nintendo DS standard.
* **512 Ko** : Firmware iQue DS.

---

## Compilation

Consultez [BUILD.md](./BUILD.md) pour les instructions de compilation.

---

## Remerciements & Crédits

* **Martin** pour GBAtek.
* **Cydrak** pour les recherches sur le GPU 3D.
* **limittox** pour l'icône de l'application.
* **L'équipe melonDS** et tous ses contributeurs.
* **L'équipe de Project PM** (Créateurs originaux de Pokémon Platine Multijoueur) :
  * **ComicartOlie** - Développeur principal & Architecte du pont multijoueur.
  * **nUt** (nUt0225) - Développeur & Systèmes de base.
  * **MottledAbyss** - Développeur & Équilibrage du jeu.
* **Ekiiii** pour l'overlay de streaming OBS live, le P2P Direct UPnP, le déchiffrement mémoire en temps réel et les outils de runs challenge.

---

## Licence

[![GNU GPLv3](https://www.gnu.org/graphics/gplv3-127x51.png)](https://www.gnu.org/licenses/gpl-3.0.html)

melonDS est un logiciel libre : vous pouvez le redistribuer et/ou le modifier selon les termes de la GNU General Public License telle que publiée par la Free Software Foundation, soit la version 3 de la Licence, ou (à votre discrétion) toute version ultérieure.
