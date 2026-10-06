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
        socket->disconnectFromHost();
    }
    else if (path == "/overlay" || path == "/" || path == "/index.html") {
        QByteArray body = cachedHtml;
        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html; charset=utf-8\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + body;
        socket->write(response);
        socket->disconnectFromHost();
    }
    else {
        QByteArray notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        socket->write(notFound);
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
    quint16 metLoc = *(const quint16*)(decBytes + blockDPos + 0x16);

    // Unencrypted party stats
    quint32 status = *(const quint32*)(data + 0x88);
    quint8 level   = *(const quint8*)(data + 0x8C);
    quint16 curHp  = *(const quint16*)(data + 0x8E);
    quint16 maxHp  = *(const quint16*)(data + 0x90);

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
    if (!partyExp || partySize < 8) return;

    QJsonObject root;
    root["active"] = true;

    QJsonArray p1Array;
    quint32 p1Count = *(const quint32*)(partyExp + 4);
    if (p1Count > 6) p1Count = 6;
    for (quint32 i = 0; i < p1Count; i++) {
        const melonDS::u8* monPtr = partyExp + 8 + i * 236;
        QJsonObject mon = parsePartyPokemon(monPtr, i);
        if (!mon.isEmpty()) {
            p1Array.append(mon);
        }
    }
    root["player1"] = p1Array;

    QJsonArray p2Array;
    if (partyImp) {
        quint32 p2Count = *(const quint32*)(partyImp + 4);
        if (p2Count > 6) p2Count = 6;
        for (quint32 i = 0; i < p2Count; i++) {
            const melonDS::u8* monPtr = partyImp + 8 + i * 236;
            QJsonObject mon = parsePartyPokemon(monPtr, i);
            if (!mon.isEmpty()) {
                p2Array.append(mon);
            }
        }
    }
    root["player2"] = p2Array;

    // Detect Soul Link pairs (matching met_location)
    QJsonArray pairsArray;
    for (int i = 0; i < p1Array.size(); i++) {
        QJsonObject m1 = p1Array[i].toObject();
        int loc1 = m1["met_location"].toInt();
        if (loc1 <= 0) continue;

        for (int j = 0; j < p2Array.size(); j++) {
            QJsonObject m2 = p2Array[j].toObject();
            int loc2 = m2["met_location"].toInt();
            if (loc1 == loc2) {
                QJsonObject pair;
                pair["p1_slot"] = m1["slot"].toInt();
                pair["p2_slot"] = m2["slot"].toInt();
                pair["location_id"] = loc1;
                bool dead = m1["is_fainted"].toBool() || m2["is_fainted"].toBool();
                pair["status"] = dead ? "DEAD" : "ALIVE";
                pairsArray.append(pair);
                break;
            }
        }
    }
    root["pairs"] = pairsArray;

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
    <title>Twitch Soullocke Overlay - Project PM</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            background: transparent;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            color: #ffffff;
            overflow: hidden;
            user-select: none;
        }

        .container {
            padding: 10px;
            display: flex;
            gap: 16px;
        }

        /* Layout modes */
        .layout-horizontal {
            flex-direction: row;
            width: 100vw;
            justify-content: space-around;
        }
        .layout-vertical {
            flex-direction: column;
            width: 380px;
        }

        .team-box {
            background: rgba(18, 22, 34, 0.85);
            border: 2px solid rgba(255, 255, 255, 0.12);
            border-radius: 12px;
            padding: 12px;
            box-shadow: 0 8px 24px rgba(0,0,0,0.5);
            backdrop-filter: blur(8px);
            flex: 1;
        }

        .team-title {
            font-size: 14px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 1.5px;
            color: #4ade80;
            margin-bottom: 10px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
        .team-title.p2 { color: #60a5fa; }

        .slots-grid {
            display: grid;
            gap: 8px;
        }
        .layout-horizontal .slots-grid {
            grid-template-columns: repeat(6, 1fr);
        }
        .layout-vertical .slots-grid {
            grid-template-columns: 1fr;
        }

        .mon-card {
            background: rgba(30, 41, 59, 0.7);
            border: 1px solid rgba(255, 255, 255, 0.08);
            border-radius: 8px;
            padding: 6px 8px;
            display: flex;
            align-items: center;
            gap: 8px;
            transition: all 0.3s ease;
            position: relative;
            overflow: hidden;
        }

        .mon-card.dead {
            filter: grayscale(1);
            opacity: 0.55;
            border-color: #ef4444;
            background: rgba(40, 10, 10, 0.6);
        }

        .mon-card.dead::after {
            content: "💀 K.O.";
            position: absolute;
            right: 6px;
            top: 6px;
            font-size: 10px;
            font-weight: bold;
            color: #ef4444;
            background: rgba(0,0,0,0.8);
            padding: 2px 4px;
            border-radius: 4px;
        }

        .sprite {
            width: 48px;
            height: 48px;
            image-rendering: pixelated;
            flex-shrink: 0;
        }

        .info {
            flex: 1;
            min-width: 0;
        }

        .name-row {
            display: flex;
            justify-content: space-between;
            align-items: baseline;
            margin-bottom: 4px;
        }

        .mon-name {
            font-size: 12px;
            font-weight: 700;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }

        .mon-level {
            font-size: 11px;
            color: #94a3b8;
            font-weight: 600;
        }

        .hp-bar-bg {
            background: rgba(0, 0, 0, 0.6);
            height: 8px;
            border-radius: 4px;
            overflow: hidden;
            position: relative;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }

        .hp-bar-fill {
            height: 100%;
            width: 100%;
            background: #22c55e;
            border-radius: 3px;
            transition: width 0.4s ease, background-color 0.4s ease;
        }

        .hp-bar-fill.warn { background: #eab308; }
        .hp-bar-fill.crit { background: #ef4444; }

        .hp-text {
            font-size: 9px;
            color: #cbd5e1;
            text-align: right;
            margin-top: 2px;
            font-variant-numeric: tabular-nums;
        }

        .link-badge {
            display: inline-block;
            font-size: 9px;
            padding: 1px 5px;
            border-radius: 3px;
            margin-top: 2px;
            background: rgba(168, 85, 247, 0.3);
            border: 1px solid rgba(168, 85, 247, 0.5);
            color: #d8b4fe;
            white-space: nowrap;
        }

        .empty-slot {
            height: 60px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 11px;
            color: #64748b;
            border: 1px dashed rgba(255,255,255,0.1);
            border-radius: 8px;
        }
    </style>
</head>
<body>
    <div id="container" class="container layout-horizontal">
        <!-- Player 1 -->
        <div class="team-box">
            <div class="team-title">
                <span>Joueur 1 (Streamer)</span>
                <span id="p1-alive" style="font-size:11px; color:#cbd5e1;">0/6</span>
            </div>
            <div id="p1-slots" class="slots-grid"></div>
        </div>

        <!-- Player 2 -->
        <div class="team-box">
            <div class="team-title p2">
                <span>Joueur 2 (Partenaire)</span>
                <span id="p2-alive" style="font-size:11px; color:#cbd5e1;">0/6</span>
            </div>
            <div id="p2-slots" class="slots-grid"></div>
        </div>
    </div>

    <script>
        const params = new URLSearchParams(window.location.search);
        const layout = params.get('layout') || 'horizontal';
        const container = document.getElementById('container');
        if (layout === 'vertical') {
            container.className = 'container layout-vertical';
        }

        function getSpriteUrl(species) {
            if (!species || species <= 0) return '';
            return `https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/${species}.png`;
        }

        function renderTeam(team, pairs, containerId, countId, isP1) {
            const slotsDiv = document.getElementById(containerId);
            const countSpan = document.getElementById(countId);
            slotsDiv.innerHTML = '';

            let aliveCount = 0;
            for (let i = 0; i < 6; i++) {
                const mon = team && team[i] ? team[i] : null;
                if (!mon || !mon.species) {
                    const empty = document.createElement('div');
                    empty.className = 'empty-slot';
                    empty.textContent = `- Slot ${i+1} -`;
                    slotsDiv.appendChild(empty);
                    continue;
                }

                if (!mon.is_fainted) aliveCount++;

                const hpPct = mon.max_hp > 0 ? Math.max(0, Math.min(100, (mon.hp / mon.max_hp) * 100)) : 0;
                let hpClass = '';
                if (hpPct <= 20) hpClass = 'crit';
                else if (hpPct <= 50) hpClass = 'warn';

                // Look for soul pair
                let linkHtml = '';
                if (pairs && pairs.length > 0) {
                    for (const p of pairs) {
                        const mySlot = isP1 ? p.p1_slot : p.p2_slot;
                        if (mySlot === (i + 1)) {
                            linkHtml = `<span class="link-badge">🔗 Zone ${p.location_id}</span>`;
                            break;
                        }
                    }
                }

                const card = document.createElement('div');
                card.className = `mon-card ${mon.is_fainted ? 'dead' : ''}`;
                card.innerHTML = `
                    <img class="sprite" src="${getSpriteUrl(mon.species)}" alt="PKM">
                    <div class="info">
                        <div class="name-row">
                            <span class="mon-name">#${mon.species}</span>
                            <span class="mon-level">Nv.${mon.level}</span>
                        </div>
                        <div class="hp-bar-bg">
                            <div class="hp-bar-fill ${hpClass}" style="width: ${hpPct}%;"></div>
                        </div>
                        <div class="hp-text">${mon.hp} / ${mon.max_hp} PV</div>
                        ${linkHtml}
                    </div>
                `;
                slotsDiv.appendChild(card);
            }

            countSpan.textContent = `${aliveCount}/6 Vivants`;
        }

        async function pollTeams() {
            try {
                const res = await fetch('/api/teams');
                if (res.ok) {
                    const data = await res.json();
                    renderTeam(data.player1, data.pairs, 'p1-slots', 'p1-alive', true);
                    renderTeam(data.player2, data.pairs, 'p2-slots', 'p2-alive', false);
                }
            } catch (err) {
                // Ignore connection blips
            }
        }

        setInterval(pollTeams, 300);
        pollTeams();
    </script>
</body>
</html>
)RAWHTML";
}
