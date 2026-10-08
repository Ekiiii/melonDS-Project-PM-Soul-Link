#include "OverlayServer.h"
#include "Config.h"
#include "Translation.h"
#include <QDebug>
#include <QUrlQuery>
#include <QUrl>

static const char* s_Gen4BlockOrders[24] = {
    "ABCD", "ABDC", "ACBD", "ACDB", "ADBC", "ADCB",
    "BACD", "BADC", "BCAD", "BCDA", "BDAC", "BDCA",
    "CABD", "CADB", "CBAD", "CBDA", "CDAB", "CDBA",
    "DABC", "DACB", "DBAC", "DBCA", "DCAB", "DCBA"
};

static QString GetEmulatorPlayerName(int role = 1, bool isMe = true)
{
    if (isMe) {
        try {
            QString name = Config::GetGlobalTable().GetQString("Online.PlayerName").trimmed();
            if (!name.isEmpty()) return name;
        } catch (...) {}

        try {
            QString name = Config::GetGlobalTable().GetQString("LAN.PlayerName").trimmed();
            if (!name.isEmpty()) return name;
        } catch (...) {}

        try {
            QString name = QString::fromStdString(Config::GetLocalTable(0).GetString("Firmware.Username")).trimmed();
            if (!name.isEmpty() && name.toLower() != "melonds") return name;
        } catch (...) {}
    }
    return QString();
}


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
    initObj["my_role"] = 1;
    initObj["build_id"] = "V0.4.5-SL-20261008-07";
    initObj["is_soullink"] = false;
    initObj["lang"] = MelonTranslator::GetLanguage().isEmpty() ? "fr" : MelonTranslator::GetLanguage();
    initObj["player1"] = QJsonArray();
    initObj["player2"] = QJsonArray();
    initObj["pairs"] = QJsonArray();

    QString defName = GetEmulatorPlayerName(1, true);
    if (defName.isEmpty()) defName = "Joueur 1";

    QJsonObject p1Obj;
    p1Obj["role"] = 1;
    p1Obj["name"] = defName;
    p1Obj["is_me"] = true;
    p1Obj["team"] = QJsonArray();

    QJsonArray pArray;
    pArray.append(p1Obj);
    initObj["players"] = pArray;

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

    if (path == "/api/set_lang") {
        QUrlQuery query(url);
        QString lang = query.queryItemValue("lang");
        if (lang == "fr" || lang == "en") {
            Config::GetGlobalTable().SetQString("UI.Language", lang);
            MelonTranslator::SetLanguage(lang);
        }
        QByteArray respBody = "{\"status\":\"ok\",\"lang\":\"" + lang.toUtf8() + "\"}";
        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: application/json; charset=utf-8\r\n"
                              "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                              "Pragma: no-cache\r\n"
                              "Expires: 0\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Content-Length: " + QByteArray::number(respBody.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + respBody;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }
    else if (path == "/api/teams" || path == "/api/data") {
        QByteArray body;
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            body = cachedJsonResponse;
        }

        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: application/json; charset=utf-8\r\n"
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
    UpdateTeamsMulti(partyExp, nullptr, partySize, 1, nullptr, partyImp, {}, {}, false, 0x01);
}

void OverlayServer::UpdateTeamsMulti(const melonDS::u8* partyExp, const melonDS::u8* partyN, melonDS::u32 partySize,
                                     int myRole, const char roster[9][24], const melonDS::u8* partyImp,
                                     const std::vector<BoxMonSummary>& localBoxes,
                                     const std::map<int, std::vector<BoxMonSummary>>& peerBoxes,
                                     bool isSoulLink,
                                     melonDS::u8 activeRolesMask)
{
    if (myRole < 1 || myRole > 8) myRole = 1;

    QJsonObject root;
    root["active"] = (partyExp != nullptr && partySize >= 8);
    root["my_role"] = myRole;
    root["build_id"] = "V0.4.5-SL-20261008-07";
    root["is_soullink"] = isSoulLink;

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
        bool isMe = (r == myRole);
        bool isActive = (activeRolesMask & (1 << (r - 1))) != 0;

        // Only include player if active in mask or is local player
        if (!isMe && !isActive) continue;

        const melonDS::u8* pBuf = nullptr;
        if (partyExp && partySize >= 8) {
            if (r == myRole) {
                pBuf = partyExp;
            } else if (partyN != nullptr) {
                pBuf = partyN + (r - 1) * partySize;
            } else if (r == 2 && partyImp != nullptr) {
                pBuf = partyImp;
            }
        }

        quint32 count = 0;
        if (pBuf) {
            count = *(const quint32*)(pBuf + 4);
            if (count > 6) count = 0;
        }

        QJsonObject playerObj;
        playerObj["role"] = r;
        playerObj["is_me"] = isMe;

        QString pName;
        if (roster && roster[r][0] != '\0') {
            pName = QString::fromUtf8(roster[r]).trimmed();
        }
        if (pName.isEmpty() && isMe) {
            pName = GetEmulatorPlayerName(r, isMe);
        }
        if (pName.isEmpty()) {
            pName = QString("Joueur %1").arg(r);
        }
        playerObj["name"] = pName;

        QJsonArray teamArray;
        int aliveCount = 0;

        for (quint32 s = 0; s < count; s++)
        {
            const melonDS::u8* monPtr = pBuf + 8 + s * 236;
            QJsonObject mon = parsePartyPokemon(monPtr, s);
            if (!mon.isEmpty()) {
                int loc = mon["met_location"].toInt();
                bool dead = isSoulLink && (loc > 0) && SoulLink_IsLocationDead(loc);
                bool fainted = mon["is_fainted"].toBool() || dead;
                mon["is_fainted"] = fainted;
                teamArray.append(mon);
                if (!fainted) aliveCount++;
                if (isSoulLink && loc > 0) {
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
                bool dead = isSoulLink && SoulLink_IsLocationDead(bMon.metLoc);
                bObj["is_fainted"] = dead;
                pcArray.append(bObj);

                if (isSoulLink) {
                    MonLocInfo info;
                    info.role = r;
                    info.slot = bMon.slot;
                    info.species = bMon.species;
                    info.is_fainted = dead;
                    info.in_box = true;
                    info.box_num = bMon.box;
                    locClusters[bMon.metLoc].append(info);
                }
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
    if (isSoulLink)
    {
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
                    if ((activeRolesMask & (1 << (absentRole - 1))) != 0)
                    {
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
    }
    root["pairs"] = pairsArray;

    QJsonArray deadLocsArray;
    if (isSoulLink)
    {
        for (int locId = 1; locId < 4000; locId++) {
            if (SoulLink_IsLocationDead(locId)) {
                deadLocsArray.append(locId);
            }
        }
    }
    root["dead_locations"] = deadLocsArray;

    QString emuLang = MelonTranslator::GetLanguage();
    if (emuLang.isEmpty()) emuLang = Config::GetGlobalTable().GetQString("UI.Language");
    if (emuLang.isEmpty()) emuLang = "fr";
    root["lang"] = emuLang;

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
    <title>Overlay SoulLocke - Project PM</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Press+Start+2P&family=Rajdhani:wght@600;700;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --c-accent: #38bdf8;
            --c-accent-glow: rgba(56, 189, 248, 0.45);
            --c-bg-box: rgba(9, 13, 22, 0.94);
            --c-bg-card: rgba(15, 23, 42, 0.92);
            --c-border: #1e293b;
            --c-text-main: #f8fafc;
            --c-text-muted: #94a3b8;
            --slot-w: 172px;
            --slot-h: 192px;
            --scale: 1;
            --sprite-scale: 1.35;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }

        body {
            background: transparent;
            font-family: 'Press Start 2P', monospace, sans-serif;
            color: var(--c-text-main);
            overflow: hidden;
            user-select: none;
            image-rendering: pixelated;
        }

        /* Clean Flat SVG Icons */
        .flat-icon {
            display: inline-block;
            vertical-align: middle;
            flex-shrink: 0;
        }

        /* Overlay Wrapper & Zoom */
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

        /* 1. Layout: Vertical (2 cols x 3 rows stacked) */
        .layout-vertical {
            flex-direction: column;
            width: fit-content;
        }
        .layout-vertical .slots-grid {
            grid-template-columns: repeat(2, var(--slot-w));
            gap: 8px;
        }

        /* 2. Layout: Horizontal (3 cols x 2 rows side-by-side) */
        .layout-horizontal {
            flex-direction: row;
            flex-wrap: wrap;
            width: fit-content;
        }
        .layout-horizontal .slots-grid {
            grid-template-columns: repeat(3, var(--slot-w));
            gap: 8px;
        }

        /* 3. Layout: Grid (2x2 Co-op) */
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

        /* 4. Layout: Bar (OBS Bottom Banner: 1x6) */
        .layout-bar {
            flex-direction: column;
            width: fit-content;
        }
        .layout-bar .slots-grid {
            grid-template-columns: repeat(6, var(--slot-w));
            gap: 8px;
        }

        /* 5. Layout: Sidebar (OBS Vertical Column: 6x1) */
        .layout-sidebar {
            flex-direction: row;
            width: fit-content;
        }
        .layout-sidebar .team-box {
            width: calc(var(--slot-w) + 24px);
            max-width: calc(var(--slot-w) + 24px);
            box-sizing: border-box;
        }
        .layout-sidebar .team-header {
            flex-direction: column;
            align-items: center;
            text-align: center;
            gap: 4px;
            padding-bottom: 8px;
        }
        .layout-sidebar .player-name {
            width: 100%;
            text-align: center;
            font-size: 13px;
            letter-spacing: 1px;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .layout-sidebar .team-status {
            width: 100%;
            text-align: center;
            font-size: 9px;
            letter-spacing: 0.5px;
        }
        .layout-sidebar .slots-grid {
            grid-template-columns: 1fr;
            width: 100%;
            gap: 8px;
        }
        .layout-sidebar .mon-card {
            width: 100%;
        }

        /* Team Box Container */
        .team-box {
            background: var(--c-bg-box);
            border: 3px solid var(--c-border);
            border-radius: 12px;
            padding: 10px 12px;
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.7), inset 0 0 16px rgba(0, 0, 0, 0.5);
            width: fit-content;
            transition: border-color 0.3s ease, box-shadow 0.3s ease;
        }

        .team-box.active-player {
            border-color: var(--c-accent);
            box-shadow: 0 0 20px var(--c-accent-glow), inset 0 0 14px rgba(0,0,0,0.6);
        }

        .team-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
            padding-bottom: 6px;
            border-bottom: 2px solid var(--c-border);
        }

        .player-name {
            font-size: 13px;
            letter-spacing: 1px;
            text-transform: uppercase;
        }

        .team-status {
            font-size: 9.5px;
            color: var(--c-text-muted);
            letter-spacing: 0.5px;
        }

        .slots-grid {
            display: grid;
            width: fit-content;
        }

        /* Pokémon Slot Card */
        .mon-card {
            width: var(--slot-w);
            height: var(--slot-h);
            background: var(--c-bg-card);
            border: 2px solid var(--c-border);
            border-radius: 8px;
            padding: 6px 8px;
            display: flex;
            flex-direction: column;
            justify-content: space-between;
            position: relative;
            box-shadow: inset 0 0 10px rgba(0, 0, 0, 0.6);
            transition: all 0.25s ease;
        }

        .mon-card:hover {
            border-color: var(--c-accent);
        }

        .mon-card.empty {
            background: rgba(15, 23, 42, 0.4);
            border: 2px dashed rgba(51, 65, 85, 0.7);
            align-items: center;
            justify-content: center;
        }

        .empty-slot-text {
            font-size: 8px;
            color: #475569;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            z-index: 2;
        }

        .mon-card.fainted {
            filter: grayscale(90%) brightness(0.65);
            border-color: #ef4444;
        }

        .card-top {
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 9px;
            z-index: 3;
        }

        .mon-name {
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            flex: 1;
            min-width: 0;
            margin-right: 6px;
            letter-spacing: 0.5px;
        }

        .mon-lvl {
            color: #ffffff;
            font-size: 12.5px;
            font-family: 'Rajdhani', sans-serif;
            font-weight: 800;
            background: rgba(0, 0, 0, 0.55);
            padding: 1px 6px;
            border-radius: 4px;
            border: 1px solid rgba(255, 255, 255, 0.2);
            letter-spacing: 0.5px;
            text-shadow: 0 1px 2px rgba(0, 0, 0, 0.8);
            line-height: 1.2;
            flex-shrink: 0;
        }

        /* Enlarged Sprite Area */
        .sprite-container {
            position: relative;
            height: 114px;
            display: flex;
            align-items: center;
            justify-content: center;
            overflow: visible;
            margin: 1px 0 2px 0;
        }

        .pkball-bg {
            position: absolute;
            width: 130px;
            height: 130px;
            opacity: 0.16;
            pointer-events: none;
            z-index: 1;
            transition: all 0.3s;
        }

        .pkmn-sprite {
            position: relative;
            z-index: 2;
            max-height: 106px;
            max-width: 106px;
            transform: scale(var(--sprite-scale, 1.35));
            transform-origin: center center;
            object-fit: contain;
            image-rendering: pixelated;
            filter: drop-shadow(0 4px 8px rgba(0, 0, 0, 0.7));
            transition: transform 0.2s cubic-bezier(0.34, 1.56, 0.64, 1);
        }

        .card-bottom {
            display: flex;
            flex-direction: column;
            gap: 4px;
            z-index: 3;
        }

        .hp-bar-wrap {
            width: 100%;
            height: 8px;
            background: #090d16;
            border-radius: 4px;
            border: 1px solid rgba(255, 255, 255, 0.25);
            box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.8);
            overflow: hidden;
            position: relative;
        }

        .hp-bar-fill {
            height: 100%;
            width: 100%;
            border-radius: 3px;
            transition: width 0.4s ease, background 0.4s ease;
        }

        .hp-text {
            font-size: 13.5px;
            text-align: right;
            color: #ffffff;
            font-family: 'Rajdhani', sans-serif;
            font-weight: 800;
            letter-spacing: 0.8px;
            line-height: 1.1;
            text-shadow: 0 1px 3px rgba(0, 0, 0, 0.95);
        }

        /* Badges & Container */
        .badge-wrap {
            margin-top: 2px;
        }
        .badge-wrap:empty {
            display: none !important;
            margin: 0 !important;
            padding: 0 !important;
        }

        /* Flat Badges (Zero 3D Emojis) */
        .badge-slot {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 5px;
            font-size: 9.5px;
            padding: 3px 8px;
            border-radius: 4px;
            font-weight: 800;
            letter-spacing: 0.5px;
            text-transform: uppercase;
            width: fit-content;
            margin: 0 auto;
            border: 1px solid transparent;
        }

        .badge-linked {
            background: rgba(88, 28, 135, 0.85);
            color: #e9d5ff;
            border-color: #a855f7;
        }

        .badge-pending {
            background: rgba(120, 53, 15, 0.85);
            color: #fde68a;
            border-color: #f59e0b;
        }

        .badge-broken {
            background: rgba(69, 10, 10, 0.85);
            color: #fecaca;
            border-color: #ef4444;
        }

        .badge-fainted {
            background: rgba(30, 41, 59, 0.9);
            color: #94a3b8;
            border-color: #64748b;
        }

        /* ========================================================
           SPACIOUS & ELEGANT RETRO CONFIGURATOR PANEL
           ======================================================== */
        .cfg-toggle-btn {
            position: fixed;
            top: 14px;
            right: 14px;
            z-index: 9999;
            background: #090d16;
            color: var(--c-accent);
            border: 2px solid var(--c-accent);
            border-radius: 8px;
            padding: 9px 16px;
            font-family: 'Press Start 2P', monospace;
            font-size: 10px;
            cursor: pointer;
            display: flex;
            align-items: center;
            gap: 8px;
            box-shadow: 0 4px 16px rgba(0, 0, 0, 0.8), 0 0 12px var(--c-accent-glow);
            transition: all 0.2s ease;
        }

        .cfg-toggle-btn:hover {
            background: var(--c-accent);
            color: #090d16;
            transform: scale(1.04);
            box-shadow: 0 6px 20px var(--c-accent-glow);
        }

        .cfg-panel {
            position: fixed;
            top: 14px;
            right: 14px;
            z-index: 10000;
            width: 580px;
            max-width: calc(100vw - 28px);
            max-height: calc(100vh - 28px);
            background: rgba(9, 13, 22, 0.97);
            border: 2px solid var(--c-accent);
            border-radius: 12px;
            padding: 16px 20px;
            box-shadow: 0 12px 40px rgba(0, 0, 0, 0.9), 0 0 24px var(--c-accent-glow);
            overflow-y: auto;
            display: flex;
            flex-direction: column;
            gap: 14px;
            backdrop-filter: blur(14px);
            font-family: 'Rajdhani', sans-serif;
            font-weight: 600;
            transition: opacity 0.25s ease, transform 0.25s ease;
        }

        .cfg-panel.hidden {
            display: none !important;
        }

        .cfg-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid rgba(255, 255, 255, 0.12);
            padding-bottom: 10px;
        }

        .cfg-title-wrap {
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .cfg-title {
            font-family: 'Press Start 2P', monospace;
            font-size: 12px;
            color: var(--c-accent);
            letter-spacing: 0.5px;
        }

        .cfg-sub {
            font-size: 13px;
            color: var(--c-text-muted);
            margin-top: 2px;
        }

        .cfg-close-btn {
            background: rgba(255, 255, 255, 0.08);
            border: 1px solid rgba(255, 255, 255, 0.2);
            border-radius: 6px;
            color: #ffffff;
            cursor: pointer;
            padding: 6px 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: all 0.2s;
        }

        .cfg-close-btn:hover {
            background: #ef4444;
            border-color: #ef4444;
        }

        .cfg-section {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .cfg-section-title {
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: var(--c-text-muted);
            font-family: 'Press Start 2P', monospace;
            font-size: 9px;
        }

        .cfg-row-2col {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
        }

        .cfg-btn-grid {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
        }

        .cfg-btn {
            background: rgba(255, 255, 255, 0.06);
            border: 1px solid rgba(255, 255, 255, 0.15);
            border-radius: 6px;
            color: #e2e8f0;
            padding: 7px 11px;
            font-size: 13px;
            font-family: 'Rajdhani', sans-serif;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.15s ease;
            display: flex;
            align-items: center;
            gap: 6px;
        }

        .cfg-btn:hover {
            background: rgba(255, 255, 255, 0.14);
            border-color: var(--c-accent);
            color: #ffffff;
        }

        .cfg-btn.active {
            background: var(--c-accent);
            color: #090d16;
            border-color: var(--c-accent);
            font-weight: 800;
            box-shadow: 0 0 10px var(--c-accent-glow);
        }

        /* Themes */
        .cfg-themes-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 8px;
        }

        .cfg-theme-btn {
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid rgba(255, 255, 255, 0.15);
            border-radius: 6px;
            padding: 8px 10px;
            display: flex;
            align-items: center;
            gap: 8px;
            color: #ffffff;
            font-family: 'Rajdhani', sans-serif;
            font-size: 14px;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.2s;
        }

        .cfg-theme-btn:hover {
            background: rgba(255, 255, 255, 0.12);
            border-color: rgba(255, 255, 255, 0.4);
        }

        .cfg-theme-btn.active {
            border-color: var(--c-accent);
            background: rgba(255, 255, 255, 0.14);
            box-shadow: inset 0 0 8px var(--c-accent-glow);
        }

        .theme-dot {
            width: 14px;
            height: 14px;
            border-radius: 50%;
            display: inline-block;
            flex-shrink: 0;
        }

        /* Custom Pickers */
        .cfg-custom-colors {
            display: flex;
            align-items: center;
            gap: 16px;
            background: rgba(0, 0, 0, 0.35);
            padding: 10px 14px;
            border-radius: 8px;
            border: 1px solid rgba(255, 255, 255, 0.08);
            margin-top: 4px;
        }

        .color-item {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 13px;
        }

        .color-item input[type="color"] {
            appearance: none;
            -webkit-appearance: none;
            border: 1px solid rgba(255, 255, 255, 0.3);
            border-radius: 4px;
            width: 28px;
            height: 28px;
            cursor: pointer;
            background: transparent;
        }

        .color-item input[type="color"]::-webkit-color-swatch {
            border: none;
            border-radius: 3px;
        }

        /* OBS Export Box */
        .cfg-obs-box {
            background: rgba(15, 23, 42, 0.8);
            border: 1px solid rgba(56, 189, 248, 0.3);
            border-radius: 8px;
            padding: 12px 14px;
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .cfg-obs-btn {
            background: var(--c-accent);
            color: #090d16;
            border: none;
            border-radius: 6px;
            padding: 10px 16px;
            font-family: 'Press Start 2P', monospace;
            font-size: 10px;
            font-weight: bold;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            box-shadow: 0 0 14px var(--c-accent-glow);
            transition: all 0.2s;
        }

        .cfg-obs-btn:hover {
            transform: scale(1.02);
            filter: brightness(1.15);
        }

        .cfg-obs-hint {
            font-size: 12px;
            color: var(--c-text-muted);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .cfg-obs-res {
            color: var(--c-accent);
            font-weight: 700;
        }

        /* Toast notification */
        .toast {
            position: fixed;
            bottom: 24px;
            right: 24px;
            background: #10b981;
            color: #090d16;
            font-family: 'Rajdhani', sans-serif;
            font-weight: 700;
            font-size: 15px;
            padding: 10px 18px;
            border-radius: 8px;
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.7);
            z-index: 20000;
            display: flex;
            align-items: center;
            gap: 8px;
            animation: fadeIn 0.2s ease;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(10px); }
            to { opacity: 1; transform: translateY(0); }
        }
    </style>
</head>
<body>
    <!-- Floating Configurator Button (hidden in OBS) -->
    <button id="cfg-toggle-btn" class="cfg-toggle-btn" title="Ouvrir le configurateur d'overlay">
        <svg class="flat-icon" viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="3"></circle><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path></svg>
        <span id="txt-open-cfg">CONFIGURER L'OVERLAY</span>
    </button>

    <!-- Comprehensive Configurator Panel -->
    <div id="cfg-panel" class="cfg-panel hidden">
        <div class="cfg-header">
            <div class="cfg-title-wrap">
                <svg class="flat-icon" viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="3"></circle><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path></svg>
                <div>
                    <div id="txt-cfg-title" class="cfg-title">CONFIGURATEUR D'OVERLAY</div>
                    <div id="txt-cfg-sub" class="cfg-sub">Personnalisation en direct pour Stream & OBS</div>
                </div>
            </div>
            <button id="cfg-close-btn" class="cfg-close-btn" title="Fermer">
                <svg class="flat-icon" viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><line x1="18" y1="6" x2="6" y2="18"></line><line x1="6" y1="6" x2="18" y2="18"></line></svg>
            </button>
        </div>

        <!-- Section: Layout -->
        <div class="cfg-section">
            <div id="lbl-layout" class="cfg-section-title">Disposition (Layout)</div>
            <div class="cfg-btn-grid">
                <button class="cfg-btn" data-layout="vertical"><span id="txt-layout-vertical">Vertical (2x3)</span></button>
                <button class="cfg-btn" data-layout="horizontal"><span id="txt-layout-horizontal">Horizontal (3x2)</span></button>
                <button class="cfg-btn" data-layout="grid"><span id="txt-layout-grid">Grille Co-op</span></button>
                <button class="cfg-btn" data-layout="bar"><span id="txt-layout-bar">Bandeau Bas (1x6)</span></button>
                <button class="cfg-btn" data-layout="sidebar"><span id="txt-layout-sidebar">Colonne (6x1)</span></button>
            </div>
        </div>

        <!-- Section: Scale & Sprites -->
        <div class="cfg-row-2col">
            <div class="cfg-section">
                <div id="lbl-scale" class="cfg-section-title">Échelle Globale</div>
                <div class="cfg-btn-grid">
                    <button class="cfg-btn" data-scale="1">100%</button>
                    <button class="cfg-btn" data-scale="1.25">125%</button>
                    <button class="cfg-btn" data-scale="1.5">150%</button>
                    <button class="cfg-btn" data-scale="1.75">175%</button>
                    <button class="cfg-btn" data-scale="2">200%</button>
                </div>
            </div>

            <div class="cfg-section">
                <div id="lbl-sprite-size" class="cfg-section-title">Taille Pokémon</div>
                <div class="cfg-btn-grid">
                    <button class="cfg-btn" data-sprite-scale="1"><span id="txt-sprite-normal">Normal</span></button>
                    <button class="cfg-btn" data-sprite-scale="1.35"><span id="txt-sprite-large">Grand</span></button>
                    <button class="cfg-btn" data-sprite-scale="1.65"><span id="txt-sprite-extra">Maxi</span></button>
                    <button class="cfg-btn" data-sprite-scale="2"><span id="txt-sprite-ultra">Ultra</span></button>
                </div>
            </div>
        </div>

        <!-- Section: Themes & Couleurs -->
        <div class="cfg-section">
            <div id="lbl-themes" class="cfg-section-title">Thèmes & Couleurs</div>
            <div class="cfg-themes-grid">
                <button class="cfg-theme-btn" data-theme="cyan">
                    <span class="theme-dot" style="background:#38bdf8;box-shadow:0 0 8px #38bdf8;"></span>
                    <span id="txt-theme-cyan">Cyan Néon</span>
                </button>
                <button class="cfg-theme-btn" data-theme="red">
                    <span class="theme-dot" style="background:#f43f5e;box-shadow:0 0 8px #f43f5e;"></span>
                    <span id="txt-theme-red">Rouge Platine</span>
                </button>
                <button class="cfg-theme-btn" data-theme="emerald">
                    <span class="theme-dot" style="background:#10b981;box-shadow:0 0 8px #10b981;"></span>
                    <span id="txt-theme-emerald">Émeraude</span>
                </button>
                <button class="cfg-theme-btn" data-theme="purple">
                    <span class="theme-dot" style="background:#c084fc;box-shadow:0 0 8px #c084fc;"></span>
                    <span id="txt-theme-purple">Améthyste</span>
                </button>
                <button class="cfg-theme-btn" data-theme="amber">
                    <span class="theme-dot" style="background:#fbbf24;box-shadow:0 0 8px #fbbf24;"></span>
                    <span id="txt-theme-amber">Or Rétro</span>
                </button>
                <button class="cfg-theme-btn" data-theme="slate">
                    <span class="theme-dot" style="background:#94a3b8;box-shadow:0 0 8px #94a3b8;"></span>
                    <span id="txt-theme-slate">Minimaliste</span>
                </button>
            </div>

            <div class="cfg-custom-colors">
                <div class="color-item">
                    <span id="lbl-accent">Accent :</span>
                    <input type="color" id="picker-accent" value="#38bdf8">
                </div>
                <div class="color-item">
                    <span id="lbl-bg">Fond :</span>
                    <input type="color" id="picker-bg" value="#090d16">
                </div>
                <div class="color-item">
                    <span id="lbl-opacity">Opacité :</span>
                    <div class="cfg-btn-grid">
                        <button class="cfg-btn" data-opacity="0.94">95%</button>
                        <button class="cfg-btn" data-opacity="0.75">75%</button>
                        <button class="cfg-btn" data-opacity="0">0%</button>
                    </div>
                </div>
            </div>
        </div>

        <!-- Section: Joueurs & Langue -->
        <div class="cfg-row-2col">
            <div class="cfg-section">
                <div id="lbl-players" class="cfg-section-title">Joueurs affichés</div>
                <div id="player-selector-btns" class="cfg-btn-grid">
                    <button class="cfg-btn active" data-player="all">Tous</button>
                </div>
            </div>

            <div class="cfg-section">
                <div id="lbl-lang" class="cfg-section-title">Langue (Language)</div>
                <div class="cfg-btn-grid">
                    <button class="cfg-btn" data-lang-choice="auto">Auto</button>
                    <button class="cfg-btn" data-lang-choice="fr">Français</button>
                    <button class="cfg-btn" data-lang-choice="en">English</button>
                </div>
            </div>
        </div>

        <!-- Section: Badges Soul Link -->
        <div class="cfg-section">
            <div id="lbl-badges" class="cfg-section-title">Badges Soul Link</div>
            <div class="cfg-btn-grid">
                <button class="cfg-btn" data-badge-mode="auto"><span id="txt-badge-auto">Auto (ROM)</span></button>
                <button class="cfg-btn" data-badge-mode="show"><span id="txt-badge-show">Toujours affichés</span></button>
                <button class="cfg-btn" data-badge-mode="hide"><span id="txt-badge-hide">Toujours masqués</span></button>
            </div>
        </div>

        <!-- Section: OBS Export -->
        <div class="cfg-obs-box">
            <button id="btn-copy-obs" class="cfg-obs-btn">
                <svg class="flat-icon" viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path></svg>
                <span id="txt-copy-obs">COPIER LE LIEN OBS STUDIO</span>
            </button>
            <div class="cfg-obs-hint">
                <span id="txt-obs-hint">Résolution conseillée dans OBS :</span>
                <span id="obs-res-hint" class="cfg-obs-res">380 × 660 px</span>
            </div>
        </div>
    </div>

    <!-- Main Overlay Container -->
    <div class="overlay-wrapper">
        <div id="container" class="container layout-vertical"></div>
    </div>

    <script>
        // 1. ALL CONSTANTS DECLARED AT THE VERY TOP
        const PKMN_NAMES_FR = {"1": "Bulbizarre", "2": "Herbizarre", "3": "Florizarre", "4": "Salamèche", "5": "Reptincel", "6": "Dracaufeu", "7": "Carapuce", "8": "Carabaffe", "9": "Tortank", "10": "Chenipan", "11": "Chrysacier", "12": "Papilusion", "13": "Aspicot", "14": "Coconfort", "15": "Dardargnan", "16": "Roucool", "17": "Roucoups", "18": "Roucarnage", "19": "Rattata", "20": "Rattatac", "21": "Piafabec", "22": "Rapasdepic", "23": "Abo", "24": "Arbok", "25": "Pikachu", "26": "Raichu", "27": "Sabelette", "28": "Sablaireau", "29": "Nidoran♀", "30": "Nidorina", "31": "Nidoqueen", "32": "Nidoran♂", "33": "Nidorino", "34": "Nidoking", "35": "Mélofée", "36": "Mélodelfe", "37": "Goupix", "38": "Feunard", "39": "Rondoudou", "40": "Grodoudou", "41": "Nosferapti", "42": "Nosferalto", "43": "Mystherbe", "44": "Ortide", "45": "Rafflesia", "46": "Paras", "47": "Parasect", "48": "Mimitoss", "49": "Aéromite", "50": "Taupiqueur", "51": "Triopikeur", "52": "Miaouss", "53": "Persian", "54": "Psykokwak", "55": "Akwakwak", "56": "Férosinge", "57": "Colossinge", "58": "Caninos", "59": "Arcanin", "60": "Ptitard", "61": "Têtarte", "62": "Tartard", "63": "Abra", "64": "Kadabra", "65": "Alakazam", "66": "Machoc", "67": "Machopeur", "68": "Mackogneur", "69": "Chétiflor", "70": "Boustiflor", "71": "Empiflor", "72": "Tentacool", "73": "Tentacruel", "74": "Racaillou", "75": "Gravalanch", "76": "Grolem", "77": "Ponyta", "78": "Galopa", "79": "Ramoloss", "80": "Flagadoss", "81": "Magnéti", "82": "Magnéton", "83": "Canarticho", "84": "Doduo", "85": "Dodrio", "86": "Otaria", "87": "Lamantine", "88": "Tadmorv", "89": "Grotadmorv", "90": "Kokiyas", "91": "Crustabri", "92": "Fantominus", "93": "Spectrum", "94": "Ectoplasma", "95": "Onix", "96": "Soporifik", "97": "Hypnomade", "98": "Krabby", "99": "Krabboss", "100": "Voltorbe", "101": "Électrode", "102": "Noeunoeuf", "103": "Noadkoko", "104": "Osselait", "105": "Ossatueur", "106": "Kicklee", "107": "Tygnon", "108": "Excelangue", "109": "Smogo", "110": "Smogogo", "111": "Rhinocorne", "112": "Rhinoféros", "113": "Leveinard", "114": "Saquedeneu", "115": "Kangourex", "116": "Hypotrempe", "117": "Hypocéan", "118": "Poissirène", "119": "Poissoroy", "120": "Stari", "121": "Staross", "122": "M. Mime", "123": "Insécateur", "124": "Lippoutou", "125": "Élektek", "126": "Magmar", "127": "Scarabrute", "128": "Tauros", "129": "Magicarpe", "130": "Léviator", "131": "Lokhlass", "132": "Métamorph", "133": "Évoli", "134": "Aquali", "135": "Voltali", "136": "Pyroli", "137": "Porygon", "138": "Amonita", "139": "Amonistar", "140": "Kabuto", "141": "Kabutops", "142": "Ptéra", "143": "Ronflex", "144": "Artikodin", "145": "Électhor", "146": "Sulfura", "147": "Minidraco", "148": "Draco", "149": "Dracolosse", "150": "Mewtwo", "151": "Mew", "152": "Germignon", "153": "Macronium", "154": "Méganium", "155": "Héricendre", "156": "Feurisson", "157": "Typhlosion", "158": "Kaiminus", "159": "Crocrodil", "160": "Aligatueur", "161": "Fouinette", "162": "Fouinar", "163": "Hoothoot", "164": "Noarfang", "165": "Coxy", "166": "Coxyclaque", "167": "Mimigal", "168": "Migalos", "169": "Nostenfer", "170": "Loupio", "171": "Lanturn", "172": "Pichu", "173": "Mélo", "174": "Toudoudou", "175": "Togepi", "176": "Togetic", "177": "Natu", "178": "Xatu", "179": "Wattouat", "180": "Lainergie", "181": "Pharamp", "182": "Joliflor", "183": "Marill", "184": "Azumarill", "185": "Simularbre", "186": "Tarpaud", "187": "Granivol", "188": "Floravol", "189": "Cotovol", "190": "Capumain", "191": "Tournegrin", "192": "Héliatronc", "193": "Yanma", "194": "Axoloto", "195": "Maraiste", "196": "Mentali", "197": "Noctali", "198": "Cornèbre", "199": "Roigada", "200": "Feuforêve", "201": "Zarbi", "202": "Qulbutoké", "203": "Girafarig", "204": "Pomdepik", "205": "Foretress", "206": "Insolourdo", "207": "Scorplane", "208": "Steelix", "209": "Snubbull", "210": "Granbull", "211": "Qwilfish", "212": "Cizayox", "213": "Caratroc", "214": "Scarhino", "215": "Farfuret", "216": "Teddiursa", "217": "Ursaring", "218": "Limagma", "219": "Volcaropod", "220": "Marcacrin", "221": "Cochignon", "222": "Corayon", "223": "Rémoraid", "224": "Octillery", "225": "Cadoizo", "226": "Démanta", "227": "Airmure", "228": "Malosse", "229": "Démolosse", "230": "Hyporoi", "231": "Phanpy", "232": "Donphan", "233": "Porygon2", "234": "Cerfrousse", "235": "Queulorior", "236": "Debugant", "237": "Kapoera", "238": "Lippouti", "239": "Élekid", "240": "Magby", "241": "Écrémeuh", "242": "Leuphorie", "243": "Raikou", "244": "Entei", "245": "Suicune", "246": "Embrylex", "247": "Ymphect", "248": "Tyranocif", "249": "Lugia", "250": "Ho-Oh", "251": "Celebi", "252": "Arcko", "253": "Massko", "254": "Jungko", "255": "Poussifeu", "256": "Galifeu", "257": "Braségali", "258": "Gobou", "259": "Flobio", "260": "Laggron", "261": "Medhyèna", "262": "Grahyèna", "263": "Zigzaton", "264": "Linéon", "265": "Chenipotte", "266": "Armulys", "267": "Charmillon", "268": "Blindalys", "269": "Papinox", "270": "Nénupiot", "271": "Lombre", "272": "Ludicolo", "273": "Grainipiot", "274": "Pifeuil", "275": "Tengalice", "276": "Nirondelle", "277": "Hélédelle", "278": "Goélise", "279": "Bekipan", "280": "Tarsal", "281": "Kirlia", "282": "Gardevoir", "283": "Arakdo", "284": "Maskadra", "285": "Balignon", "286": "Chapignon", "287": "Parecool", "288": "Vigoroth", "289": "Monaflèmit", "290": "Ningale", "291": "Ninjask", "292": "Munja", "293": "Chuchmur", "294": "Ramboum", "295": "Brouhabam", "296": "Makuhita", "297": "Hariyama", "298": "Azurill", "299": "Tarinor", "300": "Skitty", "301": "Delcatty", "302": "Ténéfix", "303": "Mysdibule", "304": "Galekid", "305": "Galegon", "306": "Galeking", "307": "Méditikka", "308": "Charmina", "309": "Dynavolt", "310": "Élecsprint", "311": "Posipi", "312": "Négapi", "313": "Muciole", "314": "Lumivole", "315": "Rosélia", "316": "Gloupti", "317": "Avaltout", "318": "Carvanha", "319": "Sharpedo", "320": "Wailmer", "321": "Wailord", "322": "Chamallot", "323": "Camérupt", "324": "Chartor", "325": "Spoink", "326": "Groret", "327": "Spinda", "328": "Kraknoix", "329": "Vibraninf", "330": "Libégon", "331": "Cacnea", "332": "Cacturne", "333": "Tylton", "334": "Altaria", "335": "Mangriff", "336": "Séviper", "337": "Séléroc", "338": "Solaroc", "339": "Barloche", "340": "Barbicha", "341": "Écrapince", "342": "Colhomard", "343": "Balbuto", "344": "Kaorine", "345": "Lilia", "346": "Vacilys", "347": "Anorith", "348": "Armaldo", "349": "Barpau", "350": "Milobellus", "351": "Morphéo", "352": "Kecleon", "353": "Polichombr", "354": "Branette", "355": "Skelénox", "356": "Téraclope", "357": "Tropius", "358": "Éoko", "359": "Absol", "360": "Okéoké", "361": "Stalgamin", "362": "Oniglali", "363": "Obalie", "364": "Phogleur", "365": "Kaimorse", "366": "Coquiperl", "367": "Serpang", "368": "Rosabyss", "369": "Relicanth", "370": "Lovdisc", "371": "Draby", "372": "Drackhaus", "373": "Drattak", "374": "Terhal", "375": "Métang", "376": "Métalosse", "377": "Regirock", "378": "Regice", "379": "Registeel", "380": "Latias", "381": "Latios", "382": "Kyogre", "383": "Groudon", "384": "Rayquaza", "385": "Jirachi", "386": "Deoxys", "387": "Tortipouss", "388": "Boskara", "389": "Torterra", "390": "Ouisticram", "391": "Chimpenfeu", "392": "Simiabraz", "393": "Tiplouf", "394": "Prinplouf", "395": "Pingoléon", "396": "Étourmi", "397": "Étourvol", "398": "Étouraptor", "399": "Keunotor", "400": "Castorno", "401": "Crikzik", "402": "Mélokrik", "403": "Lixy", "404": "Luxio", "405": "Luxray", "406": "Rozbouton", "407": "Roserade", "408": "Kranidos", "409": "Charkos", "410": "Dinoclier", "411": "Bastiodon", "412": "Cheniti", "413": "Cheniselle", "414": "Papilord", "415": "Apitrini", "416": "Apireine", "417": "Pachirisu", "418": "Mustébouée", "419": "Mustéflott", "420": "Ceribou", "421": "Ceriflor", "422": "Sancoki", "423": "Tritosor", "424": "Capidextre", "425": "Baudrive", "426": "Grodrive", "427": "Laporeille", "428": "Lockpin", "429": "Magirêve", "430": "Corboss", "431": "Chaglam", "432": "Chaffreux", "433": "Korillon", "434": "Moufouette", "435": "Moufflair", "436": "Archéomire", "437": "Archéodong", "438": "Manzaï", "439": "Mime Jr", "440": "Ptiravi", "441": "Pijako", "442": "Spiritomb", "443": "Griknot", "444": "Carmache", "445": "Carchacrok", "446": "Goinfrex", "447": "Riolu", "448": "Lucario", "449": "Hippopotas", "450": "Hippodocus", "451": "Rapion", "452": "Drascore", "453": "Cradopaud", "454": "Coatox", "455": "Vortente", "456": "Écayon", "457": "Luminéon", "458": "Babimanta", "459": "Blizzi", "460": "Blizzaroi", "461": "Dimoret", "462": "Magnézone", "463": "Coudlangue", "464": "Rhinastoc", "465": "Bouldeneu", "466": "Élekable", "467": "Maganon", "468": "Togekiss", "469": "Yanmega", "470": "Phyllali", "471": "Givrali", "472": "Scorvol", "473": "Mammochon", "474": "Porygon-Z", "475": "Gallame", "476": "Tarinorme", "477": "Noctunoir", "478": "Momartik", "479": "Motisma", "480": "Créhelf", "481": "Créfollet", "482": "Créfadet", "483": "Dialga", "484": "Palkia", "485": "Heatran", "486": "Regigigas", "487": "Giratina", "488": "Cresselia", "489": "Phione", "490": "Manaphy", "491": "Darkrai", "492": "Shaymin", "493": "Arceus"};
        const PKMN_NAMES_EN = {"1": "Bulbasaur", "2": "Ivysaur", "3": "Venusaur", "4": "Charmander", "5": "Charmeleon", "6": "Charizard", "7": "Squirtle", "8": "Wartortle", "9": "Blastoise", "10": "Caterpie", "11": "Metapod", "12": "Butterfree", "13": "Weedle", "14": "Kakuna", "15": "Beedrill", "16": "Pidgey", "17": "Pidgeotto", "18": "Pidgeot", "19": "Rattata", "20": "Raticate", "21": "Spearow", "22": "Fearow", "23": "Ekans", "24": "Arbok", "25": "Pikachu", "26": "Raichu", "27": "Sandshrew", "28": "Sandslash", "29": "Nidoran♀", "30": "Nidorina", "31": "Nidoqueen", "32": "Nidoran♂", "33": "Nidorino", "34": "Nidoking", "35": "Clefairy", "36": "Clefable", "37": "Vulpix", "38": "Ninetales", "39": "Jigglypuff", "40": "Wigglytuff", "41": "Zubat", "42": "Golbat", "43": "Oddish", "44": "Gloom", "45": "Vileplume", "46": "Paras", "47": "Parasect", "48": "Venonat", "49": "Venomoth", "50": "Diglett", "51": "Dugtrio", "52": "Meowth", "53": "Persian", "54": "Psyduck", "55": "Golduck", "56": "Mankey", "57": "Primeape", "58": "Growlithe", "59": "Arcanine", "60": "Poliwag", "61": "Poliwhirl", "62": "Poliwrath", "63": "Abra", "64": "Kadabra", "65": "Alakazam", "66": "Machop", "67": "Machoke", "68": "Machamp", "69": "Bellsprout", "70": "Weepinbell", "71": "Victreebel", "72": "Tentacool", "73": "Tentacruel", "74": "Geodude", "75": "Graveler", "76": "Golem", "77": "Ponyta", "78": "Rapidash", "79": "Slowpoke", "80": "Slowbro", "81": "Magnemite", "82": "Magneton", "83": "Farfetch’d", "84": "Doduo", "85": "Dodrio", "86": "Seel", "87": "Dewgong", "88": "Grimer", "89": "Muk", "90": "Shellder", "91": "Cloyster", "92": "Gastly", "93": "Haunter", "94": "Gengar", "95": "Onix", "96": "Drowzee", "97": "Hypno", "98": "Krabby", "99": "Kingler", "100": "Voltorb", "101": "Electrode", "102": "Exeggcute", "103": "Exeggutor", "104": "Cubone", "105": "Marowak", "106": "Hitmonlee", "107": "Hitmonchan", "108": "Lickitung", "109": "Koffing", "110": "Weezing", "111": "Rhyhorn", "112": "Rhydon", "113": "Chansey", "114": "Tangela", "115": "Kangaskhan", "116": "Horsea", "117": "Seadra", "118": "Goldeen", "119": "Seaking", "120": "Staryu", "121": "Starmie", "122": "Mr. Mime", "123": "Scyther", "124": "Jynx", "125": "Electabuzz", "126": "Magmar", "127": "Pinsir", "128": "Tauros", "129": "Magikarp", "130": "Gyarados", "131": "Lapras", "132": "Ditto", "133": "Eevee", "134": "Vaporeon", "135": "Jolteon", "136": "Flareon", "137": "Porygon", "138": "Omanyte", "139": "Omastar", "140": "Kabuto", "141": "Kabutops", "142": "Aerodactyl", "143": "Snorlax", "144": "Articuno", "145": "Zapdos", "146": "Moltres", "147": "Dratini", "148": "Dragonair", "149": "Dragonite", "150": "Mewtwo", "151": "Mew", "152": "Chikorita", "153": "Bayleef", "154": "Meganium", "155": "Cyndaquil", "156": "Quilava", "157": "Typhlosion", "158": "Totodile", "159": "Croconaw", "160": "Feraligatr", "161": "Sentret", "162": "Furret", "163": "Hoothoot", "164": "Noctowl", "165": "Ledyba", "166": "Ledian", "167": "Spinarak", "168": "Ariados", "169": "Crobat", "170": "Chinchou", "171": "Lanturn", "172": "Pichu", "173": "Cleffa", "174": "Igglybuff", "175": "Togepi", "176": "Togetic", "177": "Natu", "178": "Xatu", "179": "Mareep", "180": "Flaaffy", "181": "Ampharos", "182": "Bellossom", "183": "Marill", "184": "Azumarill", "185": "Sudowoodo", "186": "Politoed", "187": "Hoppip", "188": "Skiploom", "189": "Jumpluff", "190": "Aipom", "191": "Sunkern", "192": "Sunflora", "193": "Yanma", "194": "Wooper", "195": "Quagsire", "196": "Espeon", "197": "Umbreon", "198": "Murkrow", "199": "Slowking", "200": "Misdreavus", "201": "Unown", "202": "Wobbuffet", "203": "Girafarig", "204": "Pineco", "205": "Forretress", "206": "Dunsparce", "207": "Gligar", "208": "Steelix", "209": "Snubbull", "210": "Granbull", "211": "Qwilfish", "212": "Scizor", "213": "Shuckle", "214": "Heracross", "215": "Sneasel", "216": "Teddiursa", "217": "Ursaring", "218": "Slugma", "219": "Magcargo", "220": "Swinub", "221": "Piloswine", "222": "Corsola", "223": "Remoraid", "224": "Octillery", "225": "Delibird", "226": "Mantine", "227": "Skarmory", "228": "Houndour", "229": "Houndoom", "230": "Kingdra", "231": "Phanpy", "232": "Donphan", "233": "Porygon2", "234": "Stantler", "235": "Smeargle", "236": "Tyrogue", "237": "Hitmontop", "238": "Smoochum", "239": "Elekid", "240": "Magby", "241": "Miltank", "242": "Blissey", "243": "Raikou", "244": "Entei", "245": "Suicune", "246": "Larvitar", "247": "Pupitar", "248": "Tyranitar", "249": "Lugia", "250": "Ho-Oh", "251": "Celebi", "252": "Treecko", "253": "Grovyle", "254": "Sceptile", "255": "Torchic", "256": "Combusken", "257": "Blaziken", "258": "Mudkip", "259": "Marshtomp", "260": "Swampert", "261": "Poochyena", "262": "Mightyena", "263": "Zigzagoon", "264": "Linoone", "265": "Wurmple", "266": "Silcoon", "267": "Beautifly", "268": "Cascoon", "269": "Dustox", "270": "Lotad", "271": "Lombre", "272": "Ludicolo", "273": "Seedot", "274": "Nuzleaf", "275": "Shiftry", "276": "Taillow", "277": "Swellow", "278": "Wingull", "279": "Pelipper", "280": "Ralts", "281": "Kirlia", "282": "Gardevoir", "283": "Surskit", "284": "Masquerain", "285": "Shroomish", "286": "Breloom", "287": "Slakoth", "288": "Vigoroth", "289": "Slaking", "290": "Nincada", "291": "Ninjask", "292": "Shedinja", "293": "Whismur", "294": "Loudred", "295": "Exploud", "296": "Makuhita", "297": "Hariyama", "298": "Azurill", "299": "Nosepass", "300": "Skitty", "301": "Delcatty", "302": "Sableye", "303": "Mawile", "304": "Aron", "305": "Lairon", "306": "Aggron", "307": "Meditite", "308": "Medicham", "309": "Electrike", "310": "Manectric", "311": "Plusle", "312": "Minun", "313": "Volbeat", "314": "Illumise", "315": "Roselia", "316": "Gulpin", "317": "Swalot", "318": "Carvanha", "319": "Sharpedo", "320": "Wailmer", "321": "Wailord", "322": "Numel", "323": "Camerupt", "324": "Torkoal", "325": "Spoink", "326": "Grumpig", "327": "Spinda", "328": "Trapinch", "329": "Vibrava", "330": "Flygon", "331": "Cacnea", "332": "Cacturne", "333": "Swablu", "334": "Altaria", "335": "Zangoose", "336": "Seviper", "337": "Lunatone", "338": "Solrock", "339": "Barboach", "340": "Whiscash", "341": "Corphish", "342": "Crawdaunt", "343": "Baltoy", "344": "Claydol", "345": "Lileep", "346": "Cradily", "347": "Anorith", "348": "Armaldo", "349": "Feebas", "350": "Milotic", "351": "Castform", "352": "Kecleon", "353": "Shuppet", "354": "Banette", "355": "Duskull", "356": "Dusclops", "357": "Tropius", "358": "Chimecho", "359": "Absol", "360": "Wynaut", "361": "Snorunt", "362": "Glalie", "363": "Spheal", "364": "Sealeo", "365": "Walrein", "366": "Clamperl", "367": "Huntail", "368": "Gorebyss", "369": "Relicanth", "370": "Luvdisc", "371": "Bagon", "372": "Shelgon", "373": "Salamence", "374": "Beldum", "375": "Metang", "376": "Metagross", "377": "Regirock", "378": "Regice", "379": "Registeel", "380": "Latias", "381": "Latios", "382": "Kyogre", "383": "Groudon", "384": "Rayquaza", "385": "Jirachi", "386": "Deoxys", "387": "Turtwig", "388": "Grotle", "389": "Torterra", "390": "Chimchar", "391": "Monferno", "392": "Infernape", "393": "Piplup", "394": "Prinplup", "395": "Empoleon", "396": "Starly", "397": "Staravia", "398": "Staraptor", "399": "Bidoof", "400": "Bibarel", "401": "Kricketot", "402": "Kricketune", "403": "Shinx", "404": "Luxio", "405": "Luxray", "406": "Budew", "407": "Roserade", "408": "Cranidos", "409": "Rampardos", "410": "Shieldon", "411": "Bastiodon", "412": "Burmy", "413": "Wormadam", "414": "Mothim", "415": "Combee", "416": "Vespiquen", "417": "Pachirisu", "418": "Buizel", "419": "Floatzel", "420": "Cherubi", "421": "Cherrim", "422": "Shellos", "423": "Gastrodon", "424": "Ambipom", "425": "Drifloon", "426": "Drifblim", "427": "Buneary", "428": "Lopunny", "429": "Mismagius", "430": "Honchkrow", "431": "Glameow", "432": "Purugly", "433": "Chingling", "434": "Stunky", "435": "Skuntank", "436": "Bronzor", "437": "Bronzong", "438": "Bonsly", "439": "Mime Jr.", "440": "Happiny", "441": "Chatot", "442": "Spiritomb", "443": "Gible", "444": "Gabite", "445": "Garchomp", "446": "Munchlax", "447": "Riolu", "448": "Lucario", "449": "Hippopotas", "450": "Hippowdon", "451": "Skorupi", "452": "Drapion", "453": "Croagunk", "454": "Toxicroak", "455": "Carnivine", "456": "Finneon", "457": "Lumineon", "458": "Mantyke", "459": "Snover", "460": "Abomasnow", "461": "Weavile", "462": "Magnezone", "463": "Lickilicky", "464": "Rhyperior", "465": "Tangrowth", "466": "Electivire", "467": "Magmortar", "468": "Togekiss", "469": "Yanmega", "470": "Leafeon", "471": "Glaceon", "472": "Gliscor", "473": "Mamoswine", "474": "Porygon-Z", "475": "Gallade", "476": "Probopass", "477": "Dusknoir", "478": "Froslass", "479": "Rotom", "480": "Uxie", "481": "Mesprit", "482": "Azelf", "483": "Dialga", "484": "Palkia", "485": "Heatran", "486": "Regigigas", "487": "Giratina", "488": "Cresselia", "489": "Phione", "490": "Manaphy", "491": "Darkrai", "492": "Shaymin", "493": "Arceus"};

        const I18N = {
            fr: {
                openCfg: "CONFIGURER L'OVERLAY",
                cfgTitle: "CONFIGURATEUR D'OVERLAY",
                cfgSub: "Personnalisation en direct pour Stream & OBS",
                closeBtn: "Fermer",
                layout: "Disposition (Layout)",
                layoutVert: "Vertical (2x3)",
                layoutHori: "Horizontal (3x2)",
                layoutGrid: "Grille Co-op",
                layoutBar: "Bandeau Bas (1x6)",
                layoutSidebar: "Colonne (6x1)",
                scale: "Échelle Globale",
                spriteSize: "Taille Pokémon",
                spriteNormal: "Normal",
                spriteLarge: "Grand",
                spriteExtra: "Maxi",
                spriteUltra: "Ultra",
                themes: "Thèmes & Couleurs",
                themeCyan: "Cyan Néon",
                themeRed: "Rouge Platine",
                themeEmerald: "Émeraude",
                themePurple: "Améthyste",
                themeAmber: "Or Rétro",
                themeSlate: "Minimaliste",
                accent: "Accent :",
                bg: "Fond :",
                opacity: "Opacité :",
                players: "Joueurs affichés",
                allPlayers: "Tous",
                p1Only: "Joueur 1",
                p2Only: "Joueur 2",
                language: "Langue (Language)",
                badges: "Badges Soul Link",
                badgeAuto: "Auto (ROM)",
                badgeShow: "Toujours affichés",
                badgeHide: "Toujours masqués",
                copyObs: "COPIER LE LIEN OBS STUDIO",
                obsHint: "Résolution conseillée dans OBS :",
                copiedToast: "URL OBS copiée dans le presse-papiers !",
                alive: "EN VIE",
                linked: "LIÉ",
                linkedPc: "LIÉ (PC)",
                pending: "EN ATTENTE",
                broken: "ÂME BRISÉE",
                fainted: "K.O.",
                level: "Niv.",
                slot: "Emplacement",
                unknown: "Inconnu"
            },
            en: {
                openCfg: "CONFIGURE OVERLAY",
                cfgTitle: "OVERLAY CONFIGURATOR",
                cfgSub: "Live customization for Stream & OBS",
                closeBtn: "Close",
                layout: "Layout",
                layoutVert: "Vertical (2x3)",
                layoutHori: "Horizontal (3x2)",
                layoutGrid: "Co-op Grid",
                layoutBar: "Bottom Bar (1x6)",
                layoutSidebar: "Sidebar (6x1)",
                scale: "Global Scale",
                spriteSize: "Pokemon Size",
                spriteNormal: "Normal",
                spriteLarge: "Large",
                spriteExtra: "Extra Large",
                spriteUltra: "Ultra",
                themes: "Themes & Colors",
                themeCyan: "Neon Cyan",
                themeRed: "Platinum Red",
                themeEmerald: "Emerald",
                themePurple: "Amethyst",
                themeAmber: "Retro Gold",
                themeSlate: "Minimalist",
                accent: "Accent:",
                bg: "Background:",
                opacity: "Opacity:",
                players: "Visible Players",
                allPlayers: "All",
                p1Only: "Player 1",
                p2Only: "Player 2",
                language: "Language",
                badges: "Soul Link Badges",
                badgeAuto: "Auto (ROM)",
                badgeShow: "Always shown",
                badgeHide: "Always hidden",
                copyObs: "COPY OBS STUDIO URL",
                obsHint: "Recommended size in OBS:",
                copiedToast: "OBS URL copied to clipboard!",
                alive: "ALIVE",
                linked: "LINKED",
                linkedPc: "LINKED (PC)",
                pending: "PENDING",
                broken: "SOUL BROKEN",
                fainted: "FAINTED",
                level: "Lv.",
                slot: "Slot",
                unknown: "Unknown"
            }
        };

        const THEMES = {
            cyan: { accent: '#38bdf8', glow: 'rgba(56, 189, 248, 0.45)', bgBox: '#090d16', bgCard: '#0f172a' },
            red: { accent: '#f43f5e', glow: 'rgba(244, 63, 94, 0.45)', bgBox: '#18080c', bgCard: '#260c12' },
            emerald: { accent: '#10b981', glow: 'rgba(16, 185, 129, 0.45)', bgBox: '#06150e', bgCard: '#0a1f16' },
            purple: { accent: '#c084fc', glow: 'rgba(192, 132, 252, 0.45)', bgBox: '#14091f', bgCard: '#200e32' },
            amber: { accent: '#fbbf24', glow: 'rgba(251, 191, 36, 0.45)', bgBox: '#181205', bgCard: '#281e08' },
            slate: { accent: '#94a3b8', glow: 'rgba(148, 163, 184, 0.3)', bgBox: '#0f172a', bgCard: '#1e293b' }
        };

        const ROLE_COLORS = [
            '#4ade80', '#38bdf8', '#c084fc', '#f59e0b',
            '#f43f5e', '#818cf8', '#2dd4bf', '#fb923c'
        ];

        const RESOLUTION_HINTS = {
            vertical: '380 × 660 px',
            horizontal: '660 × 430 px',
            grid: '660 × 430 px',
            bar: '1120 × 230 px',
            sidebar: '200 × 1180 px'
        };

        const POKEBALL_SVG = `<svg class="pkball-bg" viewBox="0 0 100 100">
            <circle cx="50" cy="50" r="46" fill="none" stroke="#ffffff" stroke-width="6"/>
            <line x1="4" y1="50" x2="96" y2="50" stroke="#ffffff" stroke-width="6"/>
            <circle cx="50" cy="50" r="14" fill="#090d16" stroke="#ffffff" stroke-width="6"/>
            <circle cx="50" cy="50" r="6" fill="#ffffff"/>
        </svg>`;

        const SVG_CHAIN = `<svg class="flat-icon" viewBox="0 0 24 24" width="11" height="11" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"></path><path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"></path></svg>`;
        const SVG_CLOCK = `<svg class="flat-icon" viewBox="0 0 24 24" width="10" height="10" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><polyline points="12 6 12 12 16 14"></polyline></svg>`;
        const SVG_BROKEN = `<svg class="flat-icon" viewBox="0 0 24 24" width="11" height="11" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><line x1="18" y1="6" x2="6" y2="18"></line><line x1="6" y1="6" x2="18" y2="18"></line></svg>`;

        // 2. ALL STATE VARIABLES DECLARED
        const urlParams = new URLSearchParams(window.location.search);
        let currentLayout = urlParams.get('layout') || localStorage.getItem('ov_layout') || 'vertical';
        let currentScale = parseFloat(urlParams.get('scale') || localStorage.getItem('ov_scale') || '1');
        let currentSpriteScale = parseFloat(urlParams.get('spriteScale') || localStorage.getItem('ov_sprite_scale') || '1.35');
        let targetPlayer = urlParams.get('player') || localStorage.getItem('ov_player') || 'all';
        if (targetPlayer === 'me') targetPlayer = '1';
        let currentTheme = urlParams.get('theme') || localStorage.getItem('ov_theme') || 'cyan';
        let customAccent = urlParams.get('accent') || localStorage.getItem('ov_accent') || null;
        let customBg = urlParams.get('bg') || localStorage.getItem('ov_bg') || null;
        let currentOpacity = parseFloat(urlParams.get('opacity') || localStorage.getItem('ov_opacity') || '0.94');
        let langChoice = urlParams.get('langChoice') || localStorage.getItem('ov_lang_choice') || 'auto';
        let badgeMode = urlParams.get('badgeMode') || localStorage.getItem('ov_badge_mode') || 'auto';
        let activeLang = 'fr';
        let lastJsonData = null;
        const isObs = urlParams.get('obs') === '1' || urlParams.get('hide_ui') === '1';

        // 3. ALL HELPER FUNCTIONS
        function getRoleColor(role) {
            return ROLE_COLORS[Math.max(0, (role - 1) % ROLE_COLORS.length)];
        }

        function getMonName(species) {
            if (!species || species <= 0) return I18N[activeLang].unknown;
            if (activeLang === 'en') {
                return PKMN_NAMES_EN[species] || ('#' + species);
            }
            return PKMN_NAMES_FR[species] || ('#' + species);
        }

        function getSpriteUrl(species) {
            if (!species || species <= 0) return '';
            return `https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/other/showdown/${species}.gif`;
        }

        function getFallbackSpriteUrl(species) {
            return `https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/versions/generation-iv/platinum/${species}.png`;
        }

        function hexToRgba(hex, alpha) {
            let c = hex.replace('#', '');
            if (c.length === 3) c = c.split('').map(x => x + x).join('');
            const num = parseInt(c, 16);
            return `rgba(${(num >> 16) & 255}, ${(num >> 8) & 255}, ${num & 255}, ${alpha})`;
        }

        function showToast(msg) {
            const t = document.createElement('div');
            t.className = 'toast';
            t.innerHTML = `<svg class="flat-icon" viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg> <span>${msg}</span>`;
            document.body.appendChild(t);
            setTimeout(() => t.remove(), 2500);
        }

        function selectPlayer(player) {
            targetPlayer = String(player);
            applySettings();
            if (lastJsonData) render(lastJsonData);
        }

        // 4. RENDER FUNCTION
        function render(data) {
            const container = document.getElementById('container');
            if (!container) return;

            const myRole = data.my_role || 1;
            const pairs = data.pairs || [];
            const deadLocs = data.dead_locations || [];

            let playersToRender = [];
            if (data.players && data.players.length > 0) {
                playersToRender = data.players.slice();
            } else if (data.player1 && data.player1.length > 0) {
                playersToRender.push({ role: 1, name: "Joueur 1", team: data.player1 });
            }

            if (targetPlayer !== 'all') {
                const tr = parseInt(targetPlayer);
                if (!isNaN(tr)) {
                    playersToRender = playersToRender.filter(p => p.role === tr);
                }
            }

            // If selected player not found or empty, fallback
            if (playersToRender.length === 0) {
                if (data.players && data.players.length > 0) {
                    targetPlayer = 'all';
                    playersToRender = data.players.slice();
                } else {
                    const r = (targetPlayer !== 'all') ? parseInt(targetPlayer) : myRole;
                    playersToRender.push({ role: r, name: (r === 1 ? "Joueur 1" : "Joueur " + r), is_me: (r === myRole), team: [] });
                }
            }

            const currentBoxIds = new Set(playersToRender.map(p => `player-box-${p.role}`));
            Array.from(container.children).forEach(child => {
                if (!currentBoxIds.has(child.id)) child.remove();
            });

            const t = I18N[activeLang];

            playersToRender.forEach(p => {
                const boxId = `player-box-${p.role}`;
                let box = document.getElementById(boxId);
                const isMe = (p.role === myRole);

                if (!box) {
                    box = document.createElement('div');
                    box.id = boxId;
                    box.className = `team-box ${isMe ? 'active-player' : ''}`;
                    box.innerHTML = `
                        <div class="team-header">
                            <div class="player-name" style="color: ${getRoleColor(p.role)}">${p.name || 'Joueur ' + p.role}</div>
                            <div class="team-status">0/6 ${t.alive}</div>
                        </div>
                        <div class="slots-grid"></div>
                    `;
                    container.appendChild(box);
                } else {
                    const nameEl = box.querySelector('.player-name');
                    if (nameEl && p.name && nameEl.textContent !== p.name) {
                        nameEl.textContent = p.name;
                    }
                    if (nameEl) {
                        nameEl.style.color = getRoleColor(p.role);
                    }
                    box.classList.toggle('active-player', isMe);
                }

                const grid = box.querySelector('.slots-grid');
                if (!grid) return;

                // Ensure grid always contains exactly 6 slot cards in pristine order [0..5]
                let needsReset = (grid.children.length !== 6);
                if (!needsReset) {
                    for (let s = 0; s < 6; s++) {
                        if (grid.children[s].id !== `slot-${p.role}-${s}`) {
                            needsReset = true;
                            break;
                        }
                    }
                }
                if (needsReset) {
                    grid.innerHTML = '';
                    for (let s = 0; s < 6; s++) {
                        const sEl = document.createElement('div');
                        sEl.id = `slot-${p.role}-${s}`;
                        sEl.className = 'mon-card empty';
                        sEl.innerHTML = `${POKEBALL_SVG}<div class="empty-slot-text">- ${t.slot} ${s + 1} -</div>`;
                        grid.appendChild(sEl);
                    }
                }

                const party = p.team || p.party || [];
                const aliveCount = party.filter(m => m && !m.is_fainted && m.species > 0).length;
                const statusEl = box.querySelector('.team-status');
                if (statusEl) {
                    statusEl.textContent = `${aliveCount}/6 ${t.alive}`;
                }

                for (let i = 0; i < 6; i++) {
                    const slotEl = grid.children[i];
                    const mon = party[i];

                    if (!mon || !mon.species) {
                        if (!slotEl.classList.contains('empty')) {
                            slotEl.className = 'mon-card empty';
                            slotEl.removeAttribute('data-species');
                            slotEl.innerHTML = `${POKEBALL_SVG}<div class="empty-slot-text">- ${t.slot} ${i + 1} -</div>`;
                        } else {
                            const txtEl = slotEl.querySelector('.empty-slot-text');
                            if (txtEl) txtEl.textContent = `- ${t.slot} ${i + 1} -`;
                        }
                        continue;
                    }

                    const species = mon.species;
                    const hpPercent = mon.max_hp > 0 ? Math.min(100, Math.max(0, (mon.hp / mon.max_hp) * 100)) : 0;
                    let hpColor = '#22c55e';
                    if (hpPercent <= 20) hpColor = '#ef4444';
                    else if (hpPercent <= 50) hpColor = '#eab308';

                    const isFainted = mon.is_fainted || mon.hp === 0;

                    let badgeHtml = '';
                    const isSoulLinkRom = (data.is_soullink === true);
                    const showBadges = (badgeMode === 'show') || (badgeMode === 'auto' && isSoulLinkRom);

                    if (showBadges) {
                        const loc = mon.met_location;
                        const pair = pairs.find(pr => pr.location_id === loc);
                        const isDeadPair = deadLocs.includes(loc) || (pair && pair.status === 'DEAD');

                        if (isDeadPair || isFainted) {
                            badgeHtml = `<div class="badge-slot badge-broken">${SVG_BROKEN} <span>${t.broken}</span></div>`;
                        } else if (pair) {
                            const partner = pair.members && pair.members.find(m => m.role !== p.role);
                            if (partner) {
                                badgeHtml = partner.in_box 
                                    ? `<div class="badge-slot badge-linked">${SVG_CHAIN} <span>${t.linkedPc}</span></div>`
                                    : `<div class="badge-slot badge-linked">${SVG_CHAIN} <span>${t.linked}</span></div>`;
                            } else {
                                badgeHtml = `<div class="badge-slot badge-pending">${SVG_CLOCK} <span>${t.pending}</span></div>`;
                            }
                        } else if (loc > 0) {
                            badgeHtml = `<div class="badge-slot badge-pending">${SVG_CLOCK} <span>${t.pending}</span></div>`;
                        }
                    }

                    const name = getMonName(species);

                    if (slotEl.classList.contains('empty')) {
                        // Slot transitions from empty to having a Pokemon
                        slotEl.className = `mon-card ${isFainted ? 'fainted' : ''}`;
                        slotEl.dataset.species = species;
                        slotEl.innerHTML = `
                            <div class="card-top">
                                <span class="mon-name">${name}</span>
                                <span class="mon-lvl">${t.level}${mon.level || '?'}</span>
                            </div>
                            <div class="sprite-container">
                                ${POKEBALL_SVG}
                                <img class="pkmn-sprite" src="${getSpriteUrl(species)}" onerror="this.onerror=null; this.src=getFallbackSpriteUrl(${species});" alt="${name}">
                            </div>
                            <div class="card-bottom">
                                <div class="hp-bar-wrap">
                                    <div class="hp-bar-fill" style="width: ${hpPercent}%; background: ${hpColor};"></div>
                                </div>
                                <div class="hp-text">${mon.hp}/${mon.max_hp}</div>
                                <div class="badge-wrap">${badgeHtml}</div>
                            </div>
                        `;
                    } else {
                        // In-place update to preserve DOM order and animated GIF stability
                        slotEl.className = `mon-card ${isFainted ? 'fainted' : ''}`;
                        const nameEl = slotEl.querySelector('.mon-name');
                        if (nameEl && nameEl.textContent !== name) nameEl.textContent = name;
                        const lvlEl = slotEl.querySelector('.mon-lvl');
                        if (lvlEl) {
                            const lvlText = `${t.level}${mon.level || '?'}`;
                            if (lvlEl.textContent !== lvlText) lvlEl.textContent = lvlText;
                        }

                        const img = slotEl.querySelector('.pkmn-sprite');
                        if (img && slotEl.dataset.species !== String(species)) {
                            slotEl.dataset.species = species;
                            img.src = getSpriteUrl(species);
                            img.alt = name;
                        }

                        const fill = slotEl.querySelector('.hp-bar-fill');
                        if (fill) {
                            fill.style.width = `${hpPercent}%`;
                            fill.style.background = hpColor;
                        }
                        const hpTxt = slotEl.querySelector('.hp-text');
                        if (hpTxt) {
                            const hpVal = `${mon.hp}/${mon.max_hp}`;
                            if (hpTxt.textContent !== hpVal) hpTxt.textContent = hpVal;
                        }
                        const bWrap = slotEl.querySelector('.badge-wrap');
                        if (bWrap && bWrap.innerHTML !== badgeHtml) bWrap.innerHTML = badgeHtml;
                    }
                }
            });

            updatePlayerSelectorButtons(data);
        }

        function updatePlayerSelectorButtons(data) {
            const d = data || lastJsonData;
            const players = (d && d.players && d.players.length > 0) ? d.players : [];
            const t = I18N[activeLang] || I18N.fr;
            const container = document.getElementById('player-selector-btns');
            if (!container) return;

            // Ensure targetPlayer is still valid if players list changed
            if (targetPlayer !== 'all' && !players.some(p => String(p.role) === targetPlayer)) {
                targetPlayer = 'all';
                localStorage.setItem('ov_player', 'all');
            }

            const expectedRoles = ['all'];
            players.forEach(p => {
                if (p && p.role) expectedRoles.push(String(p.role));
            });

            const currentButtons = Array.from(container.querySelectorAll('button[data-player]'));
            const currentRoles = currentButtons.map(b => b.dataset.player);
            const needsRebuild = (expectedRoles.length !== currentRoles.length) ||
                                 expectedRoles.some((r, i) => r !== currentRoles[i]);

            if (needsRebuild) {
                container.innerHTML = '';
                const allBtn = document.createElement('button');
                allBtn.className = `cfg-btn ${targetPlayer === 'all' ? 'active' : ''}`;
                allBtn.dataset.player = 'all';
                allBtn.textContent = t.allPlayers;
                allBtn.onclick = () => selectPlayer('all');
                container.appendChild(allBtn);

                players.forEach(p => {
                    const btn = document.createElement('button');
                    const rStr = String(p.role);
                    btn.className = `cfg-btn ${targetPlayer === rStr ? 'active' : ''}`;
                    btn.dataset.player = rStr;
                    btn.textContent = p.name || `${t.p1Only.replace(' 1', '')} ${p.role}`;
                    btn.onclick = () => selectPlayer(rStr);
                    container.appendChild(btn);
                });
            } else {
                currentButtons.forEach(btn => {
                    if (btn.dataset.player === 'all') {
                        btn.textContent = t.allPlayers;
                    } else {
                        const r = parseInt(btn.dataset.player);
                        const p = players.find(x => x.role === r);
                        if (p && p.name && btn.textContent !== p.name) {
                            btn.textContent = p.name;
                        }
                    }
                    btn.classList.toggle('active', btn.dataset.player === targetPlayer);
                });
            }
        }

        // 5. THEMES & UI SETTINGS
        function applyTheme() {
            const t = THEMES[currentTheme] || THEMES.cyan;
            const accent = customAccent || t.accent;
            const bgBase = customBg || t.bgBox;
            const bgCardBase = customBg ? customBg : t.bgCard;

            document.documentElement.style.setProperty('--c-accent', accent);
            document.documentElement.style.setProperty('--c-accent-glow', hexToRgba(accent, 0.45));
            document.documentElement.style.setProperty('--c-bg-box', hexToRgba(bgBase, currentOpacity));
            document.documentElement.style.setProperty('--c-bg-card', hexToRgba(bgCardBase, Math.min(1, currentOpacity + 0.05)));

            const pAcc = document.getElementById('picker-accent');
            const pBg = document.getElementById('picker-bg');
            if (pAcc) pAcc.value = accent;
            if (pBg) pBg.value = bgBase;

            document.querySelectorAll('.cfg-theme-btn').forEach(btn => {
                btn.classList.toggle('active', btn.dataset.theme === currentTheme && !customAccent);
            });

            document.querySelectorAll('[data-opacity]').forEach(btn => {
                btn.classList.toggle('active', parseFloat(btn.dataset.opacity) === currentOpacity);
            });
        }

        function updateTexts() {
            const t = I18N[activeLang] || I18N.fr;
            const setTxt = (id, txt) => { const el = document.getElementById(id); if (el) el.textContent = txt; };

            setTxt('txt-open-cfg', t.openCfg);
            setTxt('txt-cfg-title', t.cfgTitle);
            setTxt('txt-cfg-sub', t.cfgSub);
            const closeBtn = document.getElementById('cfg-close-btn');
            if (closeBtn) closeBtn.title = t.closeBtn;

            setTxt('lbl-layout', t.layout);
            setTxt('txt-layout-vertical', t.layoutVert);
            setTxt('txt-layout-horizontal', t.layoutHori);
            setTxt('txt-layout-grid', t.layoutGrid);
            setTxt('txt-layout-bar', t.layoutBar);
            setTxt('txt-layout-sidebar', t.layoutSidebar);

            setTxt('lbl-scale', t.scale);
            setTxt('lbl-sprite-size', t.spriteSize);
            setTxt('txt-sprite-normal', t.spriteNormal);
            setTxt('txt-sprite-large', t.spriteLarge);
            setTxt('txt-sprite-extra', t.spriteExtra);
            setTxt('txt-sprite-ultra', t.spriteUltra);

            setTxt('lbl-themes', t.themes);
            setTxt('txt-theme-cyan', t.themeCyan);
            setTxt('txt-theme-red', t.themeRed);
            setTxt('txt-theme-emerald', t.themeEmerald);
            setTxt('txt-theme-purple', t.themePurple);
            setTxt('txt-theme-amber', t.themeAmber);
            setTxt('txt-theme-slate', t.themeSlate);

            setTxt('lbl-accent', t.accent);
            setTxt('lbl-bg', t.bg);
            setTxt('lbl-opacity', t.opacity);

            setTxt('lbl-players', t.players);
            setTxt('lbl-lang', t.language);
            setTxt('lbl-badges', t.badges);
            setTxt('txt-badge-auto', t.badgeAuto);
            setTxt('txt-badge-show', t.badgeShow);
            setTxt('txt-badge-hide', t.badgeHide);

            setTxt('txt-copy-obs', t.copyObs);
            setTxt('txt-obs-hint', t.obsHint);

            updatePlayerSelectorButtons(lastJsonData);

            if (lastJsonData) render(lastJsonData);
        }

        function applySettings() {
            document.documentElement.style.setProperty('--scale', currentScale);
            document.documentElement.style.setProperty('--sprite-scale', currentSpriteScale);

            const container = document.getElementById('container');
            if (container) {
                container.className = `container layout-${currentLayout}`;
            }

            const resHint = document.getElementById('obs-res-hint');
            if (resHint) {
                resHint.textContent = RESOLUTION_HINTS[currentLayout] || '600 × 400 px';
            }

            document.querySelectorAll('[data-layout]').forEach(b => b.classList.toggle('active', b.dataset.layout === currentLayout));
            document.querySelectorAll('[data-scale]').forEach(b => b.classList.toggle('active', parseFloat(b.dataset.scale) === currentScale));
            document.querySelectorAll('[data-sprite-scale]').forEach(b => b.classList.toggle('active', parseFloat(b.dataset.spriteScale) === currentSpriteScale));
            document.querySelectorAll('[data-player]').forEach(b => b.classList.toggle('active', b.dataset.player === targetPlayer));
            document.querySelectorAll('[data-lang-choice]').forEach(b => b.classList.toggle('active', b.dataset.langChoice === langChoice));
            document.querySelectorAll('[data-badge-mode]').forEach(b => b.classList.toggle('active', b.dataset.badgeMode === badgeMode));

            applyTheme();
            updateTexts();

            localStorage.setItem('ov_layout', currentLayout);
            localStorage.setItem('ov_scale', currentScale);
            localStorage.setItem('ov_sprite_scale', currentSpriteScale);
            localStorage.setItem('ov_player', targetPlayer);
            localStorage.setItem('ov_theme', currentTheme);
            localStorage.setItem('ov_opacity', currentOpacity);
            localStorage.setItem('ov_lang_choice', langChoice);
            localStorage.setItem('ov_badge_mode', badgeMode);
            if (customAccent) localStorage.setItem('ov_accent', customAccent);
            if (customBg) localStorage.setItem('ov_bg', customBg);
        }

        let currentBuildId = null;
        let lastEmuLang = null;

        // 6. FETCH POLLING
        async function fetchTeams() {
            try {
                const res = await fetch('/api/teams?t=' + Date.now(), { cache: 'no-store' });
                if (!res.ok) return;
                const data = await res.json();

                // If melonDS was updated with a new build while overlay was open (e.g. in OBS), reload automatically!
                if (data.build_id) {
                    if (currentBuildId === null) {
                        currentBuildId = data.build_id;
                    } else if (currentBuildId !== data.build_id) {
                        console.log('New melonDS build detected (' + data.build_id + '), reloading overlay...');
                        window.location.reload();
                        return;
                    }
                }

                lastJsonData = data;

                if (data.lang) {
                    const emuLang = data.lang.toLowerCase() === 'en' ? 'en' : 'fr';
                    if (lastEmuLang === null) {
                        lastEmuLang = emuLang;
                        if (langChoice === 'auto') {
                            activeLang = emuLang;
                            updateTexts();
                        }
                    } else if (lastEmuLang !== emuLang) {
                        // User changed emulator language live in melonDS!
                        lastEmuLang = emuLang;
                        langChoice = 'auto';
                        localStorage.setItem('ov_lang_choice', 'auto');
                        activeLang = emuLang;
                        applySettings();
                        updateTexts();
                    } else if (langChoice === 'auto' && activeLang !== emuLang) {
                        activeLang = emuLang;
                        updateTexts();
                    }
                }

                render(data);
            } catch (err) {
                // Ignore fetch errors during emulator boot
            }
        }

        // 7. EVENT LISTENERS
        const cfgToggleBtn = document.getElementById('cfg-toggle-btn');
        const cfgPanel = document.getElementById('cfg-panel');
        const cfgCloseBtn = document.getElementById('cfg-close-btn');

        if (cfgToggleBtn && cfgPanel) {
            cfgToggleBtn.addEventListener('click', () => {
                cfgPanel.classList.toggle('hidden');
            });
        }

        if (cfgCloseBtn && cfgPanel) {
            cfgCloseBtn.addEventListener('click', () => {
                cfgPanel.classList.add('hidden');
            });
        }

        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' || e.key === 'h' || e.key === 'H') {
                if (cfgPanel) cfgPanel.classList.add('hidden');
            }
        });

        if (isObs) {
            if (cfgToggleBtn) cfgToggleBtn.style.display = 'none';
            if (cfgPanel) cfgPanel.style.display = 'none';
        }

        document.querySelectorAll('[data-layout]').forEach(btn => {
            btn.addEventListener('click', () => {
                currentLayout = btn.dataset.layout;
                applySettings();
            });
        });

        document.querySelectorAll('[data-scale]').forEach(btn => {
            btn.addEventListener('click', () => {
                currentScale = parseFloat(btn.dataset.scale);
                applySettings();
            });
        });

        document.querySelectorAll('[data-sprite-scale]').forEach(btn => {
            btn.addEventListener('click', () => {
                currentSpriteScale = parseFloat(btn.dataset.spriteScale);
                applySettings();
            });
        });

        document.addEventListener('click', (e) => {
            const btn = e.target.closest('[data-player]');
            if (btn) {
                selectPlayer(btn.dataset.player);
            }
        });

        document.querySelectorAll('.cfg-theme-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                currentTheme = btn.dataset.theme;
                customAccent = null;
                customBg = null;
                localStorage.removeItem('ov_accent');
                localStorage.removeItem('ov_bg');
                applySettings();
            });
        });

        const pickerAcc = document.getElementById('picker-accent');
        if (pickerAcc) {
            pickerAcc.addEventListener('input', (e) => {
                customAccent = e.target.value;
                applyTheme();
            });
        }

        const pickerBg = document.getElementById('picker-bg');
        if (pickerBg) {
            pickerBg.addEventListener('input', (e) => {
                customBg = e.target.value;
                applyTheme();
            });
        }

        document.querySelectorAll('[data-opacity]').forEach(btn => {
            btn.addEventListener('click', () => {
                currentOpacity = parseFloat(btn.dataset.opacity);
                applySettings();
            });
        });

        document.querySelectorAll('[data-lang-choice]').forEach(btn => {
            btn.addEventListener('click', () => {
                langChoice = btn.dataset.langChoice;
                if (langChoice !== 'auto') {
                    activeLang = langChoice;
                    fetch(`/api/set_lang?lang=${activeLang}`).catch(() => {});
                } else {
                    if (lastEmuLang) activeLang = lastEmuLang;
                }
                applySettings();
                updateTexts();
                if (lastJsonData) render(lastJsonData);
            });
        });

        document.querySelectorAll('[data-badge-mode]').forEach(btn => {
            btn.addEventListener('click', () => {
                badgeMode = btn.dataset.badgeMode;
                applySettings();
                if (lastJsonData) render(lastJsonData);
            });
        });

        const btnCopyObs = document.getElementById('btn-copy-obs');
        if (btnCopyObs) {
            btnCopyObs.addEventListener('click', () => {
                let obsUrl = `${window.location.origin}/overlay?obs=1&layout=${currentLayout}&scale=${currentScale}&spriteScale=${currentSpriteScale}&player=${targetPlayer}&theme=${currentTheme}&opacity=${currentOpacity}`;
                if (customAccent) obsUrl += `&accent=${encodeURIComponent(customAccent)}`;
                if (customBg) obsUrl += `&bg=${encodeURIComponent(customBg)}`;
                if (langChoice !== 'auto') obsUrl += `&langChoice=${langChoice}`;
                if (badgeMode !== 'auto') obsUrl += `&badgeMode=${badgeMode}`;

                navigator.clipboard.writeText(obsUrl);
                showToast(I18N[activeLang].copiedToast);
            });
        }

        // 8. INITIAL STARTUP
        applySettings();
        render({ my_role: 1, players: [{ role: 1, name: "Joueur 1", is_me: true, team: [] }] });
        setInterval(fetchTeams, 1000);
        fetchTeams();
    </script>
</body>
</html>)RAWHTML";
}
