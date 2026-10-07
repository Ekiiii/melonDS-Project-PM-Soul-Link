#include "OverlayServer.h"
#include <QDebug>
#include <QUrlQuery>
#include <QUrl>

static const char* s_Gen4BlockOrders[24] = {
    "ABCD", "ABDC", "ACBD", "ACDB", "ADBC", "ADCB",
    "BACD", "BADC", "BCAD", "BCDA", "BDAC", "BDCA",
    "CABD", "CADB", "CBAD", "CBDA", "CDAB", "CDBA",
    "DABC", "DACB", "DBAC", "DBCA", "DCAB", "DCBA"
};

OverlayServer& OverlayServer::Instance()
{
    static OverlayServer s_instance;
    return s_instance;
}

OverlayServer::OverlayServer(QObject* parent)
    : QObject(parent), tcpServer(nullptr), serverPort(8080)
{
    buildHtml();
    QJsonObject initObj;
    initObj["active"] = false;
    initObj["player1"] = QJsonArray();
    initObj["player2"] = QJsonArray();
    initObj["pairs"] = QJsonArray();
    cachedJsonResponse = QJsonDocument(initObj).toJson(QJsonDocument::Compact);
}

OverlayServer::~OverlayServer()
{
    Stop();
}

bool OverlayServer::Start(quint16 port)
{
    if (tcpServer && tcpServer->isListening()) {
        if (serverPort == port) return true;
        Stop();
    }

    serverPort = port;
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &OverlayServer::onNewConnection);

    if (!tcpServer->listen(QHostAddress::AnyIPv4, serverPort)) {
        printf("[TwitchOverlay] Failed to start HTTP server on port %d: %s\n",
               serverPort, tcpServer->errorString().toUtf8().constData());
        delete tcpServer;
        tcpServer = nullptr;
        return false;
    }

    printf("[TwitchOverlay] Server listening on http://localhost:%d/overlay\n", serverPort);
    return true;
}

void OverlayServer::Stop()
{
    if (tcpServer) {
        tcpServer->close();
        delete tcpServer;
        tcpServer = nullptr;
        printf("[TwitchOverlay] Server stopped.\n");
    }
}

bool OverlayServer::IsRunning() const
{
    return tcpServer && tcpServer->isListening();
}

void OverlayServer::onNewConnection()
{
    while (tcpServer && tcpServer->hasPendingConnections()) {
        QTcpSocket* socket = tcpServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &OverlayServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &OverlayServer::onClientDisconnected);
    }
}

void OverlayServer::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

void OverlayServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray request = socket->readAll();
    QString reqStr = QString::fromUtf8(request);
    QStringList lines = reqStr.split("\r\n");
    if (lines.isEmpty()) return;

    QString reqLine = lines[0];
    QStringList parts = reqLine.split(" ");
    if (parts.size() < 2) return;

    QString method = parts[0];
    QString urlStr = parts[1];
    QUrl url(urlStr);
    QString path = url.path();

    if (path == "/api/teams" || path == "/api/data") {
        QByteArray body;
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            body = cachedJsonResponse;
        }

        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: application/json; charset=utf-8\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + body;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }
    else if (path == "/overlay" || path == "/" || path == "/index.html") {
        QByteArray body = cachedHtml;
        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html; charset=utf-8\r\n"
                              "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                              "Pragma: no-cache\r\n"
                              "Expires: 0\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + body;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }
    else {
        QByteArray notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        socket->write(notFound);
        socket->flush();
        socket->disconnectFromHost();
    }
}

QJsonObject OverlayServer::parsePartyPokemon(const melonDS::u8* data, int slotIndex)
{
    QJsonObject mon;
    if (!data) return mon;

    // Check PID to verify valid mon
    quint32 pid = *(const quint32*)(data + 0x00);
    quint16 checksum = *(const quint16*)(data + 0x06);
    if (pid == 0) return mon;

    // Decrypt BoxMon 128 bytes (offsets +0x08 to +0x87)
    quint16 encWords[64];
    memcpy(encWords, data + 0x08, 128);

    quint32 seed = checksum;
    quint16 decWords[64];
    for (int i = 0; i < 64; i++) {
        seed = seed * 0x41C64E6Du + 0x6073u;
        quint16 key = (quint16)(seed >> 16);
        decWords[i] = encWords[i] ^ key;
    }

    const melonDS::u8* decBytes = (const melonDS::u8*)decWords;
    quint32 orderIdx = ((pid & 0x3E000u) >> 13) % 24;
    const char* order = s_Gen4BlockOrders[orderIdx];

    int blockAPos = 0, blockDPos = 0;
    for (int b = 0; b < 4; b++) {
        if (order[b] == 'A') blockAPos = b * 32;
        if (order[b] == 'D') blockDPos = b * 32;
    }

    quint16 species = *(const quint16*)(decBytes + blockAPos + 0x00);
    quint16 metLoc = *(const quint16*)(decBytes + blockDPos + 0x18);
    if (metLoc == 0) metLoc = *(const quint16*)(decBytes + blockDPos + 0x16);

    // Party stats (100 bytes at offsets +0x88 to +0xEB)
    quint16 flags = *(const quint16*)(data + 0x04);
    quint16 partyWords[50];
    memcpy(partyWords, data + 0x88, 100);

    bool isPartyDecrypted = (flags & 1) != 0;
    if (!isPartyDecrypted) {
        quint32 pSeed = pid;
        for (int i = 0; i < 50; i++) {
            pSeed = pSeed * 0x41C64E6Du + 0x6073u;
            quint16 key = (quint16)(pSeed >> 16);
            partyWords[i] ^= key;
        }
    }

    const melonDS::u8* partyBytes = (const melonDS::u8*)partyWords;
    quint32 status = *(const quint32*)(partyBytes + 0x00);
    quint8 level   = *(const quint8*)(partyBytes + 0x04);
    quint16 curHp  = *(const quint16*)(partyBytes + 0x06);
    quint16 maxHp  = *(const quint16*)(partyBytes + 0x08);

    // Sanity check: if decrypted values look invalid (e.g. level > 100 or maxHp == 0 or maxHp > 2000),
    // test the unencrypted raw data as fallback
    if (level == 0 || level > 100 || maxHp == 0 || maxHp > 2000) {
        const melonDS::u8* rawBytes = data + 0x88;
        quint8 rawLevel  = *(const quint8*)(rawBytes + 0x04);
        quint16 rawMaxHp = *(const quint16*)(rawBytes + 0x08);
        if (rawLevel >= 1 && rawLevel <= 100 && rawMaxHp > 0 && rawMaxHp <= 2000) {
            status = *(const quint32*)(rawBytes + 0x00);
            level  = rawLevel;
            curHp  = *(const quint16*)(rawBytes + 0x06);
            maxHp  = rawMaxHp;
        }
    }

    mon["slot"] = slotIndex + 1;
    mon["pid"] = (qint64)pid;
    mon["species"] = species;
    mon["level"] = level;
    mon["hp"] = curHp;
    mon["max_hp"] = maxHp;
    mon["status"] = (qint64)status;
    mon["is_fainted"] = (curHp == 0);
    mon["met_location"] = metLoc;

    return mon;
}

void OverlayServer::UpdateTeams(const melonDS::u8* partyExp, const melonDS::u8* partyImp, melonDS::u32 partySize)
{
    UpdateTeamsMulti(partyExp, nullptr, partySize, 1, nullptr, partyImp);
}

void OverlayServer::UpdateTeamsMulti(const melonDS::u8* partyExp, const melonDS::u8* partyN, melonDS::u32 partySize,
                                     int myRole, const char roster[9][24], const melonDS::u8* partyImp,
                                     const std::vector<BoxMonSummary>& localBoxes,
                                     const std::map<int, std::vector<BoxMonSummary>>& peerBoxes)
{
    if (!partyExp || partySize < 8) return;
    if (myRole < 1 || myRole > 8) myRole = 1;

    QJsonObject root;
    root["active"] = true;
    root["my_role"] = myRole;

    QJsonArray playersArray;
    QJsonArray p1Array;
    QJsonArray p2Array;

    struct MonLocInfo {
        int role;
        int slot;
        int species;
        bool is_fainted;
        bool in_box = false;
        int box_num = 0;
    };
    QMap<int, QVector<MonLocInfo>> locClusters;

    for (int r = 1; r <= 8; r++)
    {
        const melonDS::u8* pBuf = nullptr;
        if (r == myRole) {
            pBuf = partyExp;
        } else if (partyN != nullptr) {
            pBuf = partyN + (r - 1) * partySize;
        } else if (r == 2 && partyImp != nullptr) {
            pBuf = partyImp;
        }

        if (!pBuf) continue;

        quint32 count = *(const quint32*)(pBuf + 4);
        if (count == 0 || count > 6) {
            if (r != myRole) continue;
            count = 0;
        }

        QJsonObject playerObj;
        playerObj["role"] = r;
        QString pName = (roster && roster[r][0]) ? QString::fromUtf8(roster[r]) : (r == myRole ? "Joueur 1 (Moi)" : QString("Joueur %1").arg(r));
        playerObj["name"] = pName;
        playerObj["is_me"] = (r == myRole);

        QJsonArray teamArray;
        int aliveCount = 0;

        for (quint32 s = 0; s < count; s++)
        {
            const melonDS::u8* monPtr = pBuf + 8 + s * 236;
            QJsonObject mon = parsePartyPokemon(monPtr, s);
            if (!mon.isEmpty()) {
                int loc = mon["met_location"].toInt();
                bool dead = (loc > 0) && SoulLink_IsLocationDead(loc);
                bool fainted = mon["is_fainted"].toBool() || dead;
                mon["is_fainted"] = fainted;
                teamArray.append(mon);
                if (!fainted) aliveCount++;
                if (loc > 0) {
                    MonLocInfo info;
                    info.role = r;
                    info.slot = mon["slot"].toInt();
                    info.species = mon["species"].toInt();
                    info.is_fainted = fainted;
                    info.in_box = false;
                    info.box_num = 0;
                    locClusters[loc].append(info);
                }
            }
        }

        // Add PC boxed mons for player r
        const std::vector<BoxMonSummary>* bList = nullptr;
        if (r == myRole) {
            bList = &localBoxes;
        } else {
            auto it = peerBoxes.find(r);
            if (it != peerBoxes.end()) bList = &it->second;
        }
        if (bList) {
            QJsonArray pcArray;
            for (const auto& bMon : *bList) {
                QJsonObject bObj;
                bObj["species"] = bMon.species;
                bObj["met_location"] = bMon.metLoc;
                bObj["box"] = bMon.box;
                bObj["slot"] = bMon.slot;
                bool dead = SoulLink_IsLocationDead(bMon.metLoc);
                bObj["is_fainted"] = dead;
                pcArray.append(bObj);

                MonLocInfo info;
                info.role = r;
                info.slot = bMon.slot;
                info.species = bMon.species;
                info.is_fainted = dead;
                info.in_box = true;
                info.box_num = bMon.box;
                locClusters[bMon.metLoc].append(info);
            }
            playerObj["pc_box"] = pcArray;
        }

        playerObj["alive_count"] = aliveCount;
        playerObj["team"] = teamArray;
        playersArray.append(playerObj);

        if (r == myRole) {
            p1Array = teamArray;
        } else if (p2Array.isEmpty()) {
            p2Array = teamArray;
        }
    }

    root["players"] = playersArray;
    root["player1"] = p1Array;
    root["player2"] = p2Array;

    // Detect Soul Link pairs / clusters
    QJsonArray pairsArray;
    for (auto it = locClusters.begin(); it != locClusters.end(); ++it)
    {
        int loc = it.key();
        const QVector<MonLocInfo>& list = it.value();
        bool isDeadZone = SoulLink_IsLocationDead(loc);

        if (list.size() >= 2 || isDeadZone)
        {
            QJsonObject pair;
            pair["location_id"] = loc;
            bool anyDead = isDeadZone;
            QJsonArray members;
            int p1Slot = 0;
            int p2Slot = 0;
            bool p1InBox = false;
            bool p2InBox = false;
            int p1Box = 0;
            int p2Box = 0;

            for (const MonLocInfo& m : list)
            {
                if (m.is_fainted) anyDead = true;
                QJsonObject mem;
                mem["role"] = m.role;
                mem["slot"] = m.slot;
                mem["species"] = m.species;
                mem["is_fainted"] = m.is_fainted;
                mem["in_box"] = m.in_box;
                mem["box_num"] = m.box_num;
                members.append(mem);

                if (m.role == myRole) {
                    p1Slot = m.slot;
                    p1InBox = m.in_box;
                    p1Box = m.box_num;
                } else if (p2Slot == 0) {
                    p2Slot = m.slot;
                    p2InBox = m.in_box;
                    p2Box = m.box_num;
                }
            }

            // If partner is not in the active list (e.g. peer's mon died or boxed):
            if (list.size() == 1)
            {
                int presentRole = list[0].role;
                int absentRole = (presentRole == myRole) ? (myRole == 1 ? 2 : 1) : myRole;
                QJsonObject ghostMem;
                ghostMem["role"] = absentRole;
                ghostMem["slot"] = 0;
                ghostMem["species"] = 0;
                ghostMem["is_fainted"] = isDeadZone;
                ghostMem["in_box"] = true;
                ghostMem["box_num"] = isDeadZone ? 18 : 0;
                members.append(ghostMem);

                if (absentRole == myRole) {
                    p1Slot = 0;
                    p1InBox = true;
                    p1Box = isDeadZone ? 18 : 0;
                } else {
                    p2Slot = 0;
                    p2InBox = true;
                    p2Box = isDeadZone ? 18 : 0;
                }
            }

            pair["status"] = anyDead ? "DEAD" : "ALIVE";
            pair["members"] = members;
            pair["p1_slot"] = p1Slot;
            pair["p2_slot"] = p2Slot;
            pair["p1_in_box"] = p1InBox;
            pair["p2_in_box"] = p2InBox;
            pair["p1_box"] = p1Box;
            pair["p2_box"] = p2Box;
            pairsArray.append(pair);
        }
    }
    root["pairs"] = pairsArray;

    QJsonArray deadLocsArray;
    for (int locId = 1; locId < 4000; locId++) {
        if (SoulLink_IsLocationDead(locId)) {
            deadLocsArray.append(locId);
        }
    }
    root["dead_locations"] = deadLocsArray;

    QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        cachedJsonResponse = jsonBytes;
    }
}

void OverlayServer::buildHtml()
{
    // High-performance HTML5/CSS/JS OBS Twitch Overlay
    cachedHtml = R"RAWHTML(<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <title>Overlay Twitch SoulLocke - Project PM</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Press+Start+2P&display=swap" rel="stylesheet">
    <style>
        :root {
            --slot-w: 172px;
            --slot-h: 184px;
            --scale: 1;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }

        body {
            background: transparent;
            font-family: 'Press Start 2P', monospace, sans-serif;
            color: #ffffff;
            overflow: hidden;
            user-select: none;
            image-rendering: pixelated;
        }

        .overlay-wrapper {
            padding: 12px;
            display: inline-block;
            transform-origin: top left;
            zoom: var(--scale);
        }

        .container {
            display: flex;
            gap: 16px;
            width: fit-content;
        }

        /* 1. Layout: Vertical (Teams stacked, 2 cols of 3 slots each) */
        .layout-vertical {
            flex-direction: column;
            width: fit-content;
        }
        .layout-vertical .slots-grid {
            grid-template-columns: repeat(2, var(--slot-w));
            gap: 8px;
        }

        /* 2. Layout: Horizontal (Teams side-by-side, 3 cols of 2 slots each) */
        .layout-horizontal {
            flex-direction: row;
            flex-wrap: wrap;
            width: fit-content;
        }
        .layout-horizontal .slots-grid {
            grid-template-columns: repeat(3, var(--slot-w));
            gap: 8px;
        }

        /* 3. Layout: Grid (2x2 Co-op players side-by-side, 3 cols x 2 rows each) */
        .layout-grid {
            display: grid;
            grid-template-columns: repeat(2, fit-content(100%));
            gap: 16px;
            width: fit-content;
        }
        .layout-grid .slots-grid {
            grid-template-columns: repeat(3, var(--slot-w));
            gap: 8px;
        }

        /* 4. Layout: Bar (OBS Bottom Banner: 1 horizontal row of 6 slots) */
        .layout-bar {
            flex-direction: column;
            width: fit-content;
        }
        .layout-bar .slots-grid {
            grid-template-columns: repeat(6, var(--slot-w));
            gap: 8px;
        }

        /* 5. Layout: Sidebar (OBS Column: 1 vertical column of 6 slots) */
        .layout-sidebar {
            flex-direction: row;
            width: fit-content;
        }
        .layout-sidebar .slots-grid {
            grid-template-columns: repeat(1, var(--slot-w));
            gap: 8px;
        }

        /* Pixel Art Team Box Frame */
        .team-box {
            background: rgba(14, 18, 30, 0.95);
            border: 3px solid #2e3856;
            box-shadow: inset 0 0 0 2px #3e4b73, 0 0 0 2px #090c14, 0 8px 24px rgba(0,0,0,0.7);
            border-radius: 6px;
            padding: 10px 12px;
            width: fit-content;
            box-sizing: border-box;
            transition: border-color 0.2s ease, box-shadow 0.2s ease;
        }

        .team-title {
            font-size: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: #4ade80;
            text-shadow: 2px 2px 0 #000;
            margin-bottom: 9px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            border-bottom: 2px solid #242c44;
            padding-bottom: 6px;
        }
        .team-title.p2 { color: #38bdf8; }

        .alive-badge {
            font-size: 8px;
            background: #1e293b;
            border: 1px solid #475569;
            padding: 2px 6px;
            border-radius: 2px;
            color: #cbd5e1;
            text-shadow: 1px 1px 0 #000;
        }

        .slots-grid {
            display: grid;
            width: fit-content;
        }

        /* Mon Card - Full-bleed container with superimposed HUD */
        .mon-card {
            width: var(--slot-w);
            height: var(--slot-h);
            background: radial-gradient(circle at 50% 45%, #1e263d 0%, #111625 70%, #0a0d16 100%);
            border: 2px solid #3b4566;
            box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.08), 0 0 0 1px #090c14, 0 4px 10px rgba(0,0,0,0.5);
            border-radius: 5px;
            position: relative;
            overflow: hidden;
            user-select: none;
            box-sizing: border-box;
            transition: border-color 0.2s ease, box-shadow 0.2s ease;
        }

        .mon-card.dead {
            filter: grayscale(1);
            opacity: 0.6;
            border-color: #ef4444 !important;
            background: radial-gradient(circle at 50% 45%, #2d1010 0%, #170707 70%, #0d0404 100%) !important;
        }

        /* SVG Pokéball background watermark */
        .pokeball-bg {
            position: absolute;
            width: 126px;
            height: 126px;
            top: 48%;
            left: 50%;
            transform: translate(-50%, -50%);
            z-index: 1;
            pointer-events: none;
            opacity: 0.16;
            filter: drop-shadow(0 2px 4px rgba(0,0,0,0.4));
        }
        .empty-slot .pokeball-bg {
            opacity: 0.06;
        }
        .mon-card.dead .pokeball-bg {
            filter: grayscale(1);
            opacity: 0.09;
        }

        /* Pokémon Sprite centered and filling the card */
        .sprite-container {
            position: absolute;
            inset: 0;
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 2;
            pointer-events: none;
            padding-top: 10px;
            padding-bottom: 24px;
        }

        .sprite {
            max-width: 114px;
            max-height: 114px;
            width: auto;
            height: auto;
            object-fit: contain;
            image-rendering: pixelated;
            filter: drop-shadow(0 4px 6px rgba(0,0,0,0.7));
        }

        /* Top superimposed overlay: Name & Level */
        .card-top {
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            z-index: 3;
            padding: 6px 8px 4px;
            background: linear-gradient(180deg, rgba(8, 11, 20, 0.94) 0%, rgba(8, 11, 20, 0.70) 75%, transparent 100%);
        }

        .name-row {
            display: flex;
            justify-content: space-between;
            align-items: baseline;
            width: 100%;
        }

        .mon-name {
            font-size: 9.5px;
            font-weight: bold;
            color: #ffffff;
            text-shadow: 1px 1px 0 #000, -1px -1px 0 #000, 1px -1px 0 #000, -1px 1px 0 #000;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            max-width: 110px;
            letter-spacing: 0.5px;
        }

        .mon-level {
            font-size: 8.5px;
            color: #facc15;
            text-shadow: 1px 1px 0 #000, -1px -1px 0 #000;
            flex-shrink: 0;
        }

        /* Bottom superimposed overlay: HP bar & Numbers & Link */
        .card-bottom {
            position: absolute;
            bottom: 0;
            left: 0;
            right: 0;
            z-index: 3;
            padding: 5px 7px 6px;
            background: linear-gradient(0deg, rgba(8, 11, 20, 0.96) 0%, rgba(8, 11, 20, 0.85) 75%, transparent 100%);
            display: flex;
            flex-direction: column;
            gap: 3.5px;
        }

        .hp-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 4px;
        }

        .hp-container {
            display: flex;
            align-items: center;
            gap: 3.5px;
            flex: 1;
            min-width: 0;
        }

        .hp-badge {
            background: #eab308;
            color: #000;
            font-size: 6px;
            font-weight: bold;
            padding: 1px 2.5px;
            border-radius: 1px;
            box-shadow: 1px 1px 0 #000;
            line-height: 1;
            flex-shrink: 0;
        }

        .hp-bar-track {
            background: #090d16;
            border: 1px solid #1e2538;
            box-shadow: inset 0 1px 0 rgba(0,0,0,0.9);
            border-radius: 2px;
            height: 7.5px;
            flex: 1;
            position: relative;
            overflow: hidden;
        }

        .hp-bar-fill {
            height: 100%;
            width: 100%;
            background: #22c55e;
            box-shadow: inset 0 1px 0 rgba(255,255,255,0.4);
            transition: width 0.3s steps(20);
        }
        .hp-bar-fill.warn { background: #eab308; }
        .hp-bar-fill.crit { background: #ef4444; }

        .hp-numbers {
            font-size: 7.5px;
            color: #cbd5e1;
            text-shadow: 1px 1px 0 #000;
            white-space: nowrap;
            flex-shrink: 0;
        }

        /* Dead badge */
        .dead-badge {
            position: absolute;
            top: 24px;
            right: 6px;
            background: rgba(220, 38, 38, 0.95);
            color: #ffffff;
            font-size: 7.5px;
            font-weight: bold;
            padding: 2px 5px;
            border: 1px solid #000;
            box-shadow: 1px 1px 0 #000;
            z-index: 5;
            text-shadow: 1px 1px 0 #000;
            border-radius: 2px;
        }

        /* SoulLocke Link Badges */
        .link-badge {
            font-size: 7.5px;
            background: #581c87;
            border: 1px solid #c084fc;
            box-shadow: 1px 1px 0 #000;
            color: #f5d0fe;
            text-shadow: 1px 1px 0 #000;
            padding: 3px 5px;
            border-radius: 2px;
            white-space: nowrap;
            overflow: hidden;
            display: block;
            text-align: center;
            letter-spacing: 0.2px;
            line-height: 1.2;
        }
        .link-badge.pending {
            background: #78350f;
            border-color: #f59e0b;
            color: #fef3c7;
        }
        .link-badge.dead {
            background: #450a0a;
            border-color: #ef4444;
            color: #fca5a5;
        }

        /* Empty Slot */
        .empty-slot {
            width: var(--slot-w);
            height: var(--slot-h);
            border: 2px dashed #2e3856;
            box-shadow: inset 0 0 0 1px #090c14;
            background: rgba(14, 18, 30, 0.45);
            border-radius: 5px;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            gap: 6px;
            color: #475569;
            font-size: 8px;
            text-shadow: 1px 1px 0 #000;
            position: relative;
            overflow: hidden;
            box-sizing: border-box;
        }

        .container.solo {
            width: fit-content !important;
            max-width: 100vw;
            margin: 0;
            padding: 8px;
        }

        /* Floating Retro Toolbar */
        .retro-toolbar {
            position: fixed;
            top: 10px;
            right: 10px;
            background: rgba(14, 18, 30, 0.96);
            border: 2px solid #3b4566;
            box-shadow: 0 8px 24px rgba(0,0,0,0.85);
            border-radius: 6px;
            padding: 7px 11px;
            display: flex;
            align-items: center;
            gap: 10px;
            z-index: 1000;
            font-size: 7px;
            backdrop-filter: blur(8px);
            transition: opacity 0.2s ease, transform 0.2s ease;
        }
        .retro-toolbar.hidden {
            display: none !important;
        }
        .toolbar-title {
            color: #4ade80;
            font-size: 7.5px;
            letter-spacing: 0.5px;
            border-right: 1px solid #334155;
            padding-right: 8px;
            white-space: nowrap;
        }
        .toolbar-group {
            display: flex;
            align-items: center;
            gap: 3px;
        }
        .toolbar-label {
            color: #94a3b8;
            font-size: 6.5px;
            margin-right: 2px;
        }
        .tb-btn {
            background: #1e293b;
            color: #cbd5e1;
            border: 1px solid #475569;
            border-radius: 3px;
            padding: 4px 6px;
            font-family: inherit;
            font-size: 6.5px;
            cursor: pointer;
            text-shadow: 1px 1px 0 #000;
            transition: all 0.15s ease;
        }
        .tb-btn:hover {
            background: #334155;
            color: #ffffff;
            border-color: #64748b;
        }
        .tb-btn.active {
            background: #2563eb;
            color: #ffffff;
            border-color: #60a5fa;
        }
        .tb-btn.primary {
            background: #059669;
            border-color: #34d399;
            color: #ffffff;
        }
        .tb-btn.primary:hover {
            background: #10b981;
        }
        .tb-btn.close {
            background: transparent;
            border-color: transparent;
            color: #94a3b8;
            font-size: 8px;
            padding: 2px 4px;
        }
        .tb-min-btn {
            position: fixed;
            top: 10px;
            right: 10px;
            background: rgba(14, 18, 30, 0.85);
            border: 1px solid #3b4566;
            color: #cbd5e1;
            padding: 6px 9px;
            border-radius: 4px;
            cursor: pointer;
            z-index: 999;
            font-size: 11px;
        }
        .toast {
            position: fixed;
            bottom: 20px;
            right: 20px;
            background: #059669;
            border: 2px solid #34d399;
            color: #ffffff;
            padding: 8px 14px;
            border-radius: 4px;
            font-size: 8px;
            box-shadow: 0 6px 16px rgba(0,0,0,0.6);
            z-index: 2000;
            animation: fadeToast 2.5s ease forwards;
        }
        @keyframes fadeToast {
            0% { opacity: 0; transform: translateY(10px); }
            15% { opacity: 1; transform: translateY(0); }
            80% { opacity: 1; transform: translateY(0); }
            100% { opacity: 0; transform: translateY(-10px); }
        }
    </style>
</head>
<body>
    <!-- Retro Toolbar (Interactive Layout & Size Switcher) -->
    <div id="retro-toolbar" class="retro-toolbar">
        <div class="toolbar-title">🎮 SOULLOCKE</div>
        <div class="toolbar-group">
            <span class="toolbar-label">Layout:</span>
            <button class="tb-btn" data-layout="vertical" title="Équipes empilées (2x3)">Vertical</button>
            <button class="tb-btn" data-layout="horizontal" title="Équipes côte à côte (3x2)">Horizontal</button>
            <button class="tb-btn" data-layout="grid" title="Grille co-op (2x2)">Grille</button>
            <button class="tb-btn" data-layout="bar" title="Bandeau inférieur OBS (1x6)">Bandeau (1x6)</button>
            <button class="tb-btn" data-layout="sidebar" title="Colonne latérale OBS (6x1)">Colonne (6x1)</button>
        </div>
        <div class="toolbar-group">
            <span class="toolbar-label">Taille:</span>
            <button class="tb-btn" data-scale="1">100%</button>
            <button class="tb-btn" data-scale="1.25">125%</button>
            <button class="tb-btn" data-scale="1.5">150%</button>
            <button class="tb-btn" data-scale="1.75">175%</button>
        </div>
        <div class="toolbar-group">
            <span class="toolbar-label">Afficher:</span>
            <button class="tb-btn" data-player="all">Tous</button>
            <button class="tb-btn" data-player="me">Moi seul</button>
        </div>
        <button id="btn-copy-url" class="tb-btn primary" title="Copier le lien OBS configuré">📋 Copier URL OBS</button>
        <button id="btn-toggle-tb" class="tb-btn close" title="Masquer la barre (cliquez sur ⚙️ pour réafficher)">✕</button>
    </div>
    <div id="tb-min-btn" class="tb-min-btn" title="Ouvrir les réglages d'overlay" style="display:none;">⚙️</div>

    <div class="overlay-wrapper">
        <div id="container" class="container layout-vertical"></div>
    </div>

    <script>
        const PKMN_NAMES_FR = {"1": "Bulbizarre", "2": "Herbizarre", "3": "Florizarre", "4": "Salamèche", "5": "Reptincel", "6": "Dracaufeu", "7": "Carapuce", "8": "Carabaffe", "9": "Tortank", "10": "Chenipan", "11": "Chrysacier", "12": "Papilusion", "13": "Aspicot", "14": "Coconfort", "15": "Dardargnan", "16": "Roucool", "17": "Roucoups", "18": "Roucarnage", "19": "Rattata", "20": "Rattatac", "21": "Piafabec", "22": "Rapasdepic", "23": "Abo", "24": "Arbok", "25": "Pikachu", "26": "Raichu", "27": "Sabelette", "28": "Sablaireau", "29": "Nidoran♀", "30": "Nidorina", "31": "Nidoqueen", "32": "Nidoran♂", "33": "Nidorino", "34": "Nidoking", "35": "Mélofée", "36": "Mélodelfe", "37": "Goupix", "38": "Feunard", "39": "Rondoudou", "40": "Grodoudou", "41": "Nosferapti", "42": "Nosferalto", "43": "Mystherbe", "44": "Ortide", "45": "Rafflesia", "46": "Paras", "47": "Parasect", "48": "Mimitoss", "49": "Aéromite", "50": "Taupiqueur", "51": "Triopikeur", "52": "Miaouss", "53": "Persian", "54": "Psykokwak", "55": "Akwakwak", "56": "Férosinge", "57": "Colossinge", "58": "Caninos", "59": "Arcanin", "60": "Ptitard", "61": "Têtarte", "62": "Tartard", "63": "Abra", "64": "Kadabra", "65": "Alakazam", "66": "Machoc", "67": "Machopeur", "68": "Mackogneur", "69": "Chétiflor", "70": "Boustiflor", "71": "Empiflor", "72": "Tentacool", "73": "Tentacruel", "74": "Racaillou", "75": "Gravalanch", "76": "Grolem", "77": "Ponyta", "78": "Galopa", "79": "Ramoloss", "80": "Flagadoss", "81": "Magnéti", "82": "Magnéton", "83": "Canarticho", "84": "Doduo", "85": "Dodrio", "86": "Otaria", "87": "Lamantine", "88": "Tadmorv", "89": "Grotadmorv", "90": "Kokiyas", "91": "Crustabri", "92": "Fantominus", "93": "Spectrum", "94": "Ectoplasma", "95": "Onix", "96": "Soporifik", "97": "Hypnomade", "98": "Krabby", "99": "Krabboss", "100": "Voltorbe", "101": "Électrode", "102": "Noeunoeuf", "103": "Noadkoko", "104": "Osselait", "105": "Ossatueur", "106": "Kicklee", "107": "Tygnon", "108": "Excelangue", "109": "Smogo", "110": "Smogogo", "111": "Rhinocorne", "112": "Rhinoféros", "113": "Leveinard", "114": "Saquedeneu", "115": "Kangourex", "116": "Hypotrempe", "117": "Hypocéan", "118": "Poissirène", "119": "Poissoroy", "120": "Stari", "121": "Staross", "122": "M. Mime", "123": "Insécateur", "124": "Lippoutou", "125": "Élektek", "126": "Magmar", "127": "Scarabrute", "128": "Tauros", "129": "Magicarpe", "130": "Léviator", "131": "Lokhlass", "132": "Métamorph", "133": "Évoli", "134": "Aquali", "135": "Voltali", "136": "Pyroli", "137": "Porygon", "138": "Amonita", "139": "Amonistar", "140": "Kabuto", "141": "Kabutops", "142": "Ptéra", "143": "Ronflex", "144": "Artikodin", "145": "Électhor", "146": "Sulfura", "147": "Minidraco", "148": "Draco", "149": "Dracolosse", "150": "Mewtwo", "151": "Mew", "152": "Germignon", "153": "Macronium", "154": "Méganium", "155": "Héricendre", "156": "Feurisson", "157": "Typhlosion", "158": "Kaiminus", "159": "Crocrodil", "160": "Aligatueur", "161": "Fouinette", "162": "Fouinar", "163": "Hoothoot", "164": "Noarfang", "165": "Coxy", "166": "Coxyclaque", "167": "Mimigal", "168": "Migalos", "169": "Nostenfer", "170": "Loupio", "171": "Lanturn", "172": "Pichu", "173": "Mélo", "174": "Toudoudou", "175": "Togepi", "176": "Togetic", "177": "Natu", "178": "Xatu", "179": "Wattouat", "180": "Lainergie", "181": "Pharamp", "182": "Joliflor", "183": "Marill", "184": "Azumarill", "185": "Simularbre", "186": "Tarpaud", "187": "Granivol", "188": "Floravol", "189": "Cotovol", "190": "Capumain", "191": "Tournegrin", "192": "Héliatronc", "193": "Yanma", "194": "Axoloto", "195": "Maraiste", "196": "Mentali", "197": "Noctali", "198": "Cornèbre", "199": "Roigada", "200": "Feuforêve", "201": "Zarbi", "202": "Qulbutoké", "203": "Girafarig", "204": "Pomdepik", "205": "Foretress", "206": "Insolourdo", "207": "Scorplane", "208": "Steelix", "209": "Snubbull", "210": "Granbull", "211": "Qwilfish", "212": "Cizayox", "213": "Caratroc", "214": "Scarhino", "215": "Farfuret", "216": "Teddiursa", "217": "Ursaring", "218": "Limagma", "219": "Volcaropod", "220": "Marcacrin", "221": "Cochignon", "222": "Corayon", "223": "Rémoraid", "224": "Octillery", "225": "Cadoizo", "226": "Démanta", "227": "Airmure", "228": "Malosse", "229": "Démolosse", "230": "Hyporoi", "231": "Phanpy", "232": "Donphan", "233": "Porygon2", "234": "Cerfrousse", "235": "Queulorior", "236": "Debugant", "237": "Kapoera", "238": "Lippouti", "239": "Élekid", "240": "Magby", "241": "Écrémeuh", "242": "Leuphorie", "243": "Raikou", "244": "Entei", "245": "Suicune", "246": "Embrylex", "247": "Ymphect", "248": "Tyranocif", "249": "Lugia", "250": "Ho-Oh", "251": "Celebi", "252": "Arcko", "253": "Massko", "254": "Jungko", "255": "Poussifeu", "256": "Galifeu", "257": "Braségali", "258": "Gobou", "259": "Flobio", "260": "Laggron", "261": "Medhyèna", "262": "Grahyèna", "263": "Zigzaton", "264": "Linéon", "265": "Chenipotte", "266": "Armulys", "267": "Charmillon", "268": "Blindalys", "269": "Papinox", "270": "Nénupiot", "271": "Lombre", "272": "Ludicolo", "273": "Grainipiot", "274": "Pifeuil", "275": "Tengalice", "276": "Nirondelle", "277": "Hélédelle", "278": "Goélise", "279": "Bekipan", "280": "Tarsal", "281": "Kirlia", "282": "Gardevoir", "283": "Arakdo", "284": "Maskadra", "285": "Balignon", "286": "Chapignon", "287": "Parecool", "288": "Vigoroth", "289": "Monaflèmit", "290": "Ningale", "291": "Ninjask", "292": "Munja", "293": "Chuchmur", "294": "Ramboum", "295": "Brouhabam", "296": "Makuhita", "297": "Hariyama", "298": "Azurill", "299": "Tarinor", "300": "Skitty", "301": "Delcatty", "302": "Ténéfix", "303": "Mysdibule", "304": "Galekid", "305": "Galegon", "306": "Galeking", "307": "Méditikka", "308": "Charmina", "309": "Dynavolt", "310": "Élecsprint", "311": "Posipi", "312": "Négapi", "313": "Muciole", "314": "Lumivole", "315": "Rosélia", "316": "Gloupti", "317": "Avaltout", "318": "Carvanha", "319": "Sharpedo", "320": "Wailmer", "321": "Wailord", "322": "Chamallot", "323": "Camérupt", "324": "Chartor", "325": "Spoink", "326": "Groret", "327": "Spinda", "328": "Kraknoix", "329": "Vibraninf", "330": "Libégon", "331": "Cacnea", "332": "Cacturne", "333": "Tylton", "334": "Altaria", "335": "Mangriff", "336": "Séviper", "337": "Séléroc", "338": "Solaroc", "339": "Barloche", "340": "Barbicha", "341": "Écrapince", "342": "Colhomard", "343": "Balbuto", "344": "Kaorine", "345": "Lilia", "346": "Vacilys", "347": "Anorith", "348": "Armaldo", "349": "Barpau", "350": "Milobellus", "351": "Morphéo", "352": "Kecleon", "353": "Polichombr", "354": "Branette", "355": "Skelénox", "356": "Téraclope", "357": "Tropius", "358": "Éoko", "359": "Absol", "360": "Okéoké", "361": "Stalgamin", "362": "Oniglali", "363": "Obalie", "364": "Phogleur", "365": "Kaimorse", "366": "Coquiperl", "367": "Serpang", "368": "Rosabyss", "369": "Relicanth", "370": "Lovdisc", "371": "Draby", "372": "Drackhaus", "373": "Drattak", "374": "Terhal", "375": "Métang", "376": "Métalosse", "377": "Regirock", "378": "Regice", "379": "Registeel", "380": "Latias", "381": "Latios", "382": "Kyogre", "383": "Groudon", "384": "Rayquaza", "385": "Jirachi", "386": "Deoxys", "387": "Tortipouss", "388": "Boskara", "389": "Torterra", "390": "Ouisticram", "391": "Chimpenfeu", "392": "Simiabraz", "393": "Tiplouf", "394": "Prinplouf", "395": "Pingoléon", "396": "Étourmi", "397": "Étourvol", "398": "Étouraptor", "399": "Keunotor", "400": "Castorno", "401": "Crikzik", "402": "Mélokrik", "403": "Lixy", "404": "Luxio", "405": "Luxray", "406": "Rozbouton", "407": "Roserade", "408": "Kranidos", "409": "Charkos", "410": "Dinoclier", "411": "Bastiodon", "412": "Cheniti", "413": "Cheniselle", "414": "Papilord", "415": "Apitrini", "416": "Apireine", "417": "Pachirisu", "418": "Mustébouée", "419": "Mustéflott", "420": "Ceribou", "421": "Ceriflor", "422": "Sancoki", "423": "Tritosor", "424": "Capidextre", "425": "Baudrive", "426": "Grodrive", "427": "Laporeille", "428": "Lockpin", "429": "Magirêve", "430": "Corboss", "431": "Chaglam", "432": "Chaffreux", "433": "Korillon", "434": "Moufouette", "435": "Moufflair", "436": "Archéomire", "437": "Archéodong", "438": "Manzaï", "439": "Mime Jr", "440": "Ptiravi", "441": "Pijako", "442": "Spiritomb", "443": "Griknot", "444": "Carmache", "445": "Carchacrok", "446": "Goinfrex", "447": "Riolu", "448": "Lucario", "449": "Hippopotas", "450": "Hippodocus", "451": "Rapion", "452": "Drascore", "453": "Cradopaud", "454": "Coatox", "455": "Vortente", "456": "Écayon", "457": "Luminéon", "458": "Babimanta", "459": "Blizzi", "460": "Blizzaroi", "461": "Dimoret", "462": "Magnézone", "463": "Coudlangue", "464": "Rhinastoc", "465": "Bouldeneu", "466": "Élekable", "467": "Maganon", "468": "Togekiss", "469": "Yanmega", "470": "Phyllali", "471": "Givrali", "472": "Scorvol", "473": "Mammochon", "474": "Porygon-Z", "475": "Gallame", "476": "Tarinorme", "477": "Noctunoir", "478": "Momartik", "479": "Motisma", "480": "Créhelf", "481": "Créfollet", "482": "Créfadet", "483": "Dialga", "484": "Palkia", "485": "Heatran", "486": "Regigigas", "487": "Giratina", "488": "Cresselia", "489": "Phione", "490": "Manaphy", "491": "Darkrai", "492": "Shaymin", "493": "Arceus"};

        const urlParams = new URLSearchParams(window.location.search);
        let currentLayout = urlParams.get('layout') || 'vertical';
        let currentScale = parseFloat(urlParams.get('scale') || '1');
        let targetPlayer = urlParams.get('player') || 'all';
        const isObs = urlParams.get('obs') === '1' || urlParams.get('hide_ui') === '1';

        const container = document.getElementById('container');
        const tb = document.getElementById('retro-toolbar');
        const minBtn = document.getElementById('tb-min-btn');

        function applySettings() {
            document.documentElement.style.setProperty('--scale', currentScale);
            container.className = 'container layout-' + currentLayout;
            if (targetPlayer === 'me' || targetPlayer === '1') {
                container.classList.add('solo');
            }

            document.querySelectorAll('[data-layout]').forEach(b => {
                b.classList.toggle('active', b.dataset.layout === currentLayout);
            });
            document.querySelectorAll('[data-scale]').forEach(b => {
                b.classList.toggle('active', parseFloat(b.dataset.scale) === currentScale);
            });
            document.querySelectorAll('[data-player]').forEach(b => {
                b.classList.toggle('active', b.dataset.player === targetPlayer);
            });

            const p = new URLSearchParams();
            p.set('layout', currentLayout);
            if (currentScale !== 1) p.set('scale', currentScale);
            if (targetPlayer !== 'all') p.set('player', targetPlayer);
            if (isObs) p.set('obs', '1');
            window.history.replaceState({}, '', '?' + p.toString());
        }

        // Toolbar bindings
        document.querySelectorAll('[data-layout]').forEach(b => {
            b.addEventListener('click', () => { currentLayout = b.dataset.layout; applySettings(); });
        });
        document.querySelectorAll('[data-scale]').forEach(b => {
            b.addEventListener('click', () => { currentScale = parseFloat(b.dataset.scale); applySettings(); });
        });
        document.querySelectorAll('[data-player]').forEach(b => {
            b.addEventListener('click', () => {
                targetPlayer = b.dataset.player;
                applySettings();
                lastResponseText = '';
                pollTeams();
            });
        });

        document.getElementById('btn-copy-url').addEventListener('click', () => {
            const obsUrl = `${window.location.origin}/overlay?player=${targetPlayer}&layout=${currentLayout}&scale=${currentScale}&obs=1`;
            navigator.clipboard.writeText(obsUrl);
            showToast('✓ URL OBS copiée dans le presse-papier !');
        });

        document.getElementById('btn-toggle-tb').addEventListener('click', () => {
            tb.classList.add('hidden');
            minBtn.style.display = 'block';
        });
        minBtn.addEventListener('click', () => {
            tb.classList.remove('hidden');
            minBtn.style.display = 'none';
        });

        if (isObs) {
            tb.classList.add('hidden');
            minBtn.style.display = 'none';
        }

        function showToast(msg) {
            const t = document.createElement('div');
            t.className = 'toast';
            t.textContent = msg;
            document.body.appendChild(t);
            setTimeout(() => t.remove(), 2500);
        }

        applySettings();

        const ROLE_COLORS = [
            '#4ade80', '#38bdf8', '#c084fc', '#f59e0b',
            '#f43f5e', '#818cf8', '#2dd4bf', '#fb923c'
        ];
        function getRoleColor(role) {
            return ROLE_COLORS[Math.max(0, (role - 1) % ROLE_COLORS.length)];
        }

        function getMonName(species) {
            if (!species || species <= 0) return 'Inconnu';
            return PKMN_NAMES_FR[species] || ('#' + species);
        }

        function getSpriteUrl(species) {
            if (!species || species <= 0) return '';
            return `https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/other/showdown/${species}.gif`;
        }
        function getFallbackSpriteUrl(species) {
            return `https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/versions/generation-iv/platinum/${species}.png`;
        }

        let lastResponseText = '';

        function updatePlayersDOM(players, pairs, deadLocs) {
            const currentRoles = new Set(players.map(p => p.role || 1));
            const existingBoxes = container.querySelectorAll('.team-box');
            existingBoxes.forEach(box => {
                const r = parseInt(box.dataset.role);
                if (!currentRoles.has(r)) box.remove();
            });

            for (const player of players) {
                const role = player.role || 1;
                let box = container.querySelector(`.team-box[data-role="${role}"]`);
                if (!box) {
                    box = document.createElement('div');
                    box.className = 'team-box';
                    box.dataset.role = role;
                    box.innerHTML = `
                        <div class="team-title">
                            <span class="p-name"></span>
                            <span class="alive-badge"></span>
                        </div>
                        <div class="slots-grid"></div>
                    `;
                    container.appendChild(box);
                }

                const color = getRoleColor(role);
                box.style.borderColor = color + 'aa';
                const titleEl = box.querySelector('.team-title');
                titleEl.style.color = color;
                box.querySelector('.p-name').textContent = player.name;
                box.querySelector('.alive-badge').textContent = `${player.alive_count || 0}/6 EN VIE`;

                const grid = box.querySelector('.slots-grid');
                for (let i = 0; i < 6; i++) {
                    const mon = player.team && player.team[i] ? player.team[i] : null;
                    let slotEl = grid.children[i];

                    if (!mon || !mon.species) {
                        if (!slotEl || !slotEl.classList.contains('empty-slot') || slotEl.dataset.slotIdx != i) {
                            const newEmpty = document.createElement('div');
                            newEmpty.className = 'empty-slot';
                            newEmpty.dataset.slotIdx = i;
                            newEmpty.innerHTML = `
                                <svg class="pokeball-bg" viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
                                    <circle cx="50" cy="50" r="44" fill="none" stroke="#ffffff" stroke-width="6" />
                                    <line x1="6" y1="50" x2="94" y2="50" stroke="#ffffff" stroke-width="6" />
                                    <circle cx="50" cy="50" r="14" fill="none" stroke="#ffffff" stroke-width="6" />
                                </svg>
                                <span style="z-index:2;">◓ Slot ${i+1}</span>
                                <span style="opacity:0.4; z-index:2;">Vide</span>
                            `;
                            if (slotEl) grid.replaceChild(newEmpty, slotEl);
                            else grid.appendChild(newEmpty);
                        }
                        continue;
                    }

                    const species = mon.species;
                    const hpPct = mon.max_hp > 0 ? Math.max(0, Math.min(100, (mon.hp / mon.max_hp) * 100)) : 0;
                    let hpClass = '';
                    if (hpPct <= 20) hpClass = 'crit';
                    else if (hpPct <= 50) hpClass = 'warn';

                    // Link Badge Logic (No Zone Numbers!)
                    let linkText = '';
                    let linkClass = '';
                    const loc = mon.met_location || 0;
                    const isFainted = mon.is_fainted;
                    const isDeadZone = (loc > 0) && (deadLocs && deadLocs.includes(loc));
                    const isDead = isFainted || isDeadZone;

                    if (isDead) {
                        linkText = '🔗 ÂME BRISÉE';
                        linkClass = 'dead';
                    } else if (loc > 0) {
                        let foundPair = null;
                        if (pairs && pairs.length > 0) {
                            foundPair = pairs.find(p => p.location_id === loc);
                        }

                        if (foundPair) {
                            if (foundPair.status === 'DEAD') {
                                linkText = '🔗 ÂME BRISÉE';
                                linkClass = 'dead';
                            } else {
                                const partner = foundPair.members ? foundPair.members.find(m => m.role !== role) : null;
                                if (partner && partner.in_box) {
                                    linkText = '🔗 LIÉ (PC)';
                                } else {
                                    linkText = '🔗 LIÉ';
                                }
                                linkClass = '';
                            }
                        } else {
                            // Captured, but waiting for partner in this zone!
                            linkText = '⏳ EN ATTENTE';
                            linkClass = 'pending';
                        }
                    }

                    const name = getMonName(species);

                    // Create card element if missing or previously empty slot
                    if (!slotEl || !slotEl.classList.contains('mon-card')) {
                        const newCard = document.createElement('div');
                        newCard.className = 'mon-card';
                        newCard.dataset.slotIdx = i;
                        newCard.dataset.species = species;
                        newCard.innerHTML = `
                            <svg class="pokeball-bg" viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
                                <path d="M 6 50 A 44 44 0 0 1 94 50 Z" fill="#ef4444" />
                                <path d="M 6 50 A 44 44 0 0 0 94 50 Z" fill="#ffffff" />
                                <circle cx="50" cy="50" r="44" fill="none" stroke="#111827" stroke-width="7" />
                                <line x1="6" y1="50" x2="94" y2="50" stroke="#111827" stroke-width="7" />
                                <circle cx="50" cy="50" r="16" fill="#111827" />
                                <circle cx="50" cy="50" r="10" fill="#ffffff" />
                                <circle cx="50" cy="50" r="5" fill="#cbd5e1" stroke="#94a3b8" stroke-width="1.5" />
                            </svg>
                            <div class="dead-badge" style="display:none;">💀 K.O.</div>
                            <div class="card-top">
                                <div class="name-row">
                                    <span class="mon-name" title="${name}">${name}</span>
                                    <span class="mon-level">Lv.${mon.level}</span>
                                </div>
                            </div>
                            <div class="sprite-container">
                                <img class="sprite" src="${getSpriteUrl(species)}" onerror="this.onerror=null; this.src=getFallbackSpriteUrl(${species});" alt="${name}">
                            </div>
                            <div class="card-bottom">
                                <div class="hp-row">
                                    <div class="hp-container">
                                        <div class="hp-badge">HP</div>
                                        <div class="hp-bar-track">
                                            <div class="hp-bar-fill ${hpClass}" style="width: ${hpPct}%;"></div>
                                        </div>
                                    </div>
                                    <div class="hp-numbers">${mon.hp}/${mon.max_hp}</div>
                                </div>
                                <div class="link-badge-container"></div>
                            </div>
                        `;
                        if (slotEl) grid.replaceChild(newCard, slotEl);
                        else grid.appendChild(newCard);
                        slotEl = newCard;
                    }

                    // Card already exists: ONLY update img.src if species changed to prevent resetting GIF
                    if (slotEl.dataset.species !== String(species)) {
                        slotEl.dataset.species = species;
                        const img = slotEl.querySelector('.sprite');
                        img.src = getSpriteUrl(species);
                        img.alt = name;
                        const nameEl = slotEl.querySelector('.mon-name');
                        nameEl.textContent = name;
                        nameEl.title = name;
                    }

                    // Update live mutable stats
                    if (isDead) {
                        slotEl.classList.add('dead');
                        slotEl.querySelector('.dead-badge').style.display = 'block';
                    } else {
                        slotEl.classList.remove('dead');
                        slotEl.querySelector('.dead-badge').style.display = 'none';
                    }

                    slotEl.querySelector('.mon-level').textContent = `Lv.${mon.level}`;
                    const barFill = slotEl.querySelector('.hp-bar-fill');
                    barFill.className = `hp-bar-fill ${hpClass}`;
                    barFill.style.width = `${hpPct}%`;
                    slotEl.querySelector('.hp-numbers').textContent = `${mon.hp}/${mon.max_hp}`;

                    const linkContainer = slotEl.querySelector('.link-badge-container');
                    if (linkText) {
                        linkContainer.innerHTML = `<span class="link-badge ${linkClass}" title="${linkText}">${linkText}</span>`;
                    } else {
                        linkContainer.innerHTML = '';
                    }
                }
            }
        }

        async function pollTeams() {
            try {
                const res = await fetch('/api/teams');
                if (!res.ok) return;
                const text = await res.text();
                if (text === lastResponseText) return;
                lastResponseText = text;
                const data = JSON.parse(text);

                if (!data.active) {
                    if (!document.getElementById('standby-card')) {
                        container.innerHTML = `
                            <div id="standby-card" class="team-box" style="text-align:center; padding: 35px 25px; max-width: 560px; margin: 40px auto;">
                                <div class="team-title" style="justify-content: center; font-size: 11px; border-bottom: none; margin-bottom: 0; color: #4ade80;">
                                    ⏳ En attente de synchronisation du jeu...
                                </div>
                                <div style="font-size: 8px; color: #94a3b8; margin-top: 14px; line-height: 1.8;">
                                    Lancez votre jeu Pokémon Platine Multijoueur pour afficher les équipes sur l'overlay.
                                </div>
                            </div>`;
                    }
                    return;
                }

                if (document.getElementById('standby-card')) {
                    container.innerHTML = '';
                }

                let playersToRender = [];
                if (data.players && data.players.length > 0) {
                    if (targetPlayer === 'me' || targetPlayer === '1') {
                        const me = data.players.find(p => p.is_me) || data.players[0];
                        if (me) playersToRender.push(me);
                    } else if (targetPlayer === 'all') {
                        playersToRender = data.players;
                    } else {
                        const r = parseInt(targetPlayer);
                        const p = data.players.find(x => x.role === r);
                        if (p) playersToRender.push(p);
                        else playersToRender = data.players;
                    }
                } else if (data.player1) {
                    playersToRender.push({
                        role: 1,
                        name: 'Player 1 (Me)',
                        is_me: true,
                        alive_count: data.player1.filter(m => !m.is_fainted).length,
                        team: data.player1
                    });
                    if (targetPlayer === 'all' && data.player2 && data.player2.length > 0) {
                        playersToRender.push({
                            role: 2,
                            name: 'Player 2 (Partner)',
                            is_me: false,
                            alive_count: data.player2.filter(m => !m.is_fainted).length,
                            team: data.player2
                        });
                    }
                }

                updatePlayersDOM(playersToRender, data.pairs, data.dead_locations);
            } catch (err) {}
        }

        setInterval(pollTeams, 300);
        pollTeams();
    </script>
</body>
</html>)RAWHTML";
}
