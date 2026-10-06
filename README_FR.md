<p align="center"><img src="https://raw.githubusercontent.com/melonDS-emu/melonDS/master/res/icon/melon_128x128.png"></p>
<h2 align="center"><b>melonDS - Édition Soul Link & Project PM</b></h2>
<p align="center">
<a href="http://melonds.kuribo64.net/" alt="Site web melonDS"><img src="https://img.shields.io/badge/site_web-melonds.kuribo64.net-%2331352e.svg"></a>
<a href="https://www.gnu.org/licenses/gpl-3.0" alt="Licence : GPLv3"><img src="https://img.shields.io/badge/Licence-GPL%20v3-%23ff554d.svg"></a>
<a href="https://discord.gg/pAMAtExcqV" alt="Discord"><img src="https://img.shields.io/badge/Discord-Kuribo64-7289da?logo=discord&logoColor=white"></a>
<br>
<a href="https://github.com/Ekiiii/melonDS-Project-PM-Soul-Link/actions/workflows/build-windows.yml?query=event%3Apush"><img src="https://github.com/Ekiiii/melonDS-Project-PM-Soul-Link/actions/workflows/build-windows.yml/badge.svg" /></a>
</p>

<p align="center">
  <b>🇬🇧 <a href="README.md">Click here to read the documentation in English</a></b>
</p>

---

## Édition Soul Link ([melonDS-Project-PM-Soul-Link](https://github.com/Ekiiii/melonDS-Project-PM-Soul-Link))

Ce fork développé par **Ekiiii** enrichit melonDS et Project PM spécialement pour les aventures coopératives en **Soul Link / Nuzlocke**. Il intègre une connexion Peer-to-Peer directe, un hébergement en un clic avec UPnP, une synchronisation automatique des morts et des paires, ainsi qu'un overlay de streaming en direct prêt pour OBS et Twitch.

### Fonctionnalités Clés

1. **Connexion P2P Directe & Redirection de Port UPnP Automatique** :
   - **Sans serveur relais** : Connexion directe de joueur à joueur via des sockets TCP en streaming haute performance, sans dépendre de serveurs relais publics ni de la détection LAN.
   - **UPnP Automatique** : Demande automatiquement l'ouverture et la redirection du port sur votre box Internet/routeur (`TCP 7820`) en un clic—aucune configuration manuelle de routeur n'est requise pour la plupart des box domestiques (Freebox, Livebox, etc.).
   - **Assistant Pare-feu Windows Intégré** : Configure les règles de trafic entrant automatiquement avec une seule invite administrateur.
   - **Bouton Test Local** : Possibilité de tester deux instances sur la même machine (`127.0.0.1:7820`) en un clic.

2. **Codes de Salon Courts et Partageables (`SL-XXXXX-XXXXX`)** :
   - Plus besoin de chercher ni de divulguer votre adresse IP publique brute : l'émulateur compresse l'adresse IP et le port dans un code de salon en Base32 propre de 10 caractères (ex. `SL-4LADD-A69NE`).
   - Copie en un clic pour l'hôte, décodage instantané pour le joueur qui rejoint.
   - Accepte également directement les adresses IPv4 brutes et les VPN virtuels (Radmin VPN, Tailscale, ZeroTier).

3. **Interface Graphique Dédiée Direct P2P** :
   - Menu accessible via `Système -> Multijoueur -> Direct P2P Soul Link...` :
     - **Onglet Héberger** : Création de salon en un clic, affichage du code de salon en grand format, bouton de copie, indicateur de statut UPnP, liste des joueurs connectés avec ping/latence, et contrôles de session.
     - **Onglet Rejoindre** : Saisie du code de salon ou de l'IP, décodage immédiat, état de la connexion et roster des participants.

4. **Overlay HTML Twitch & OBS en Direct (1 à 8 Joueurs)** :
   - Serveur HTTP léger intégré disponible sur `http://localhost:8080/overlay`.
   - **Déchiffrement Mémoire Temps Réel** : Déchiffre les structures `BoxMon` et `PartyPokemon` directement depuis la mémoire vive ARM9 de la Nintendo DS (algorithme LCRNG officiel 4G avec seed PID) pour restituer les vrais PV, niveaux, espèces et états K.O. avec une latence sub-frame.
   - **Animation Fluide des GIFs** : Maintien dynamique des éléments DOM pour éviter que les GIFs animés des Pokémon ne redémarrent en boucle à chaque rafraîchissement.
   - **Design de Slots Agrandis** : Cadres de sprites spacieux de 70×70px et Pokémon agrandis de +50% pour un rendu pixel art net et lisible sur les streams.
   - **Détection Automatique des Paires Soul Link** : Regroupe automatiquement les Pokémon capturés dans la même zone (`met_location`) entre tous les joueurs.
   - **Badges de Statut d'Âme** :
     - `🔗 LIÉ [Zone X]` (Lien d'âme actif)
     - `🔗 ÂME BRISÉE` (Alerte de mort / Pokémon K.O.)
   - **Mode Streamer Solo** : `http://localhost:8080/overlay?player=me` affiche une disposition compacte et soignée dédiée aux streamers solo sur OBS sans espace perdu.
   - **Vue Équipe Complète** : `http://localhost:8080/overlay?player=all` affiche les équipes de tous les participants (jusqu'à 8 joueurs) avec options de mise en page (`?layout=horizontal` ou `?layout=vertical`).

5. **Synchronisation Automatique des Morts & Paires (Soul Link)** :
   - Dès qu'un Pokémon meurt en combat ou est envoyé au cimetière (Boîte PC 18 "CIMETIERE"), la zone est immédiatement marquée comme décédée et transmise à tous les pairs connectés en P2P.
   - Lorsque le partenaire est dans l'overworld (hors combat, `!inBattle`), son Pokémon lié **disparaît automatiquement de son équipe** et les slots restants sont réorganisés sans corrompre la mémoire.
   - Si le joueur est en plein combat, le retrait s'applique automatiquement à la fin de celui-ci.
   - Synchronisation périodique continue pour récupérer l'état des âmes même en cas de pause ou de fluctuation réseau.

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

Consultez le fichier [BUILD.md](./BUILD.md) pour les instructions détaillées de compilation multiplateforme (Windows, Linux, macOS).

---

## Crédits

* **Martin** pour GBAtek, documentation de référence indispensable.
* **Cydrak** pour les recherches sur le GPU 3D de la Nintendo DS.
* **limittox** pour l'icône de l'application.
* **L'équipe melonDS** et tous les contributeurs de la communauté.
* **Ekiiii** pour l'implémentation Soul Link, le P2P Direct UPnP, la synchronisation des morts et l'overlay streaming OBS.

---

## Licence

[![Image GNU GPLv3](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.fr.html)

melonDS est un logiciel libre : vous pouvez le redistribuer et/ou le modifier selon les termes de la Licence Publique Générale GNU (GPL) telle que publiée par la Free Software Foundation, soit la version 3 de la licence, soit (à votre convenance) toute version ultérieure.
