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
    UpdateTeamsMulti(partyExp, nullptr, partySize, 1, nullptr, partyImp);
}

void OverlayServer::UpdateTeamsMulti(const melonDS::u8* partyExp, const melonDS::u8* partyN, melonDS::u32 partySize,
                                     int myRole, const char roster[9][24], const melonDS::u8* partyImp)
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
                teamArray.append(mon);
                if (!mon["is_fainted"].toBool()) aliveCount++;
                int loc = mon["met_location"].toInt();
                if (loc > 0) {
                    MonLocInfo info;
                    info.role = r;
                    info.slot = mon["slot"].toInt();
                    info.species = mon["species"].toInt();
                    info.is_fainted = mon["is_fainted"].toBool();
                    locClusters[loc].append(info);
                }
            }
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
        if (list.size() >= 2)
        {
            QJsonObject pair;
            pair["location_id"] = loc;
            bool anyDead = false;
            QJsonArray members;
            int p1Slot = 0;
            int p2Slot = 0;

            for (const MonLocInfo& m : list)
            {
                if (m.is_fainted) anyDead = true;
                QJsonObject mem;
                mem["role"] = m.role;
                mem["slot"] = m.slot;
                mem["species"] = m.species;
                mem["is_fainted"] = m.is_fainted;
                members.append(mem);

                if (m.role == myRole) p1Slot = m.slot;
                else if (p2Slot == 0) p2Slot = m.slot;
            }

            pair["status"] = anyDead ? "DEAD" : "ALIVE";
            pair["members"] = members;
            pair["p1_slot"] = p1Slot;
            pair["p2_slot"] = p2Slot;
            pairsArray.append(pair);
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
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Press+Start+2P&display=swap" rel="stylesheet">
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            background: transparent;
            font-family: 'Press Start 2P', monospace, sans-serif;
            color: #ffffff;
            overflow: hidden;
            user-select: none;
            image-rendering: pixelated;
        }

        .container {
            padding: 8px;
            display: flex;
            gap: 14px;
        }

        .layout-horizontal {
            flex-direction: row;
            width: 100vw;
            justify-content: space-around;
        }
        .layout-vertical {
            flex-direction: column;
            width: 320px;
        }

        /* Pixel Art Team Box Frame */
        .team-box {
            background: rgba(14, 18, 30, 0.94);
            border: 3px solid #2e3856;
            outline: 2px solid #090c14;
            box-shadow: inset 0 0 0 2px #3e4b73, 0 6px 18px rgba(0,0,0,0.65);
            border-radius: 4px;
            padding: 8px 10px;
            flex: 1;
        }

        .team-title {
            font-size: 9px;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: #4ade80;
            text-shadow: 2px 2px 0 #000;
            margin-bottom: 8px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            border-bottom: 2px solid #242c44;
            padding-bottom: 5px;
        }
        .team-title.p2 { color: #38bdf8; }

        .alive-badge {
            font-size: 7.5px;
            background: #1e293b;
            border: 1px solid #475569;
            padding: 2px 5px;
            border-radius: 2px;
            color: #cbd5e1;
            text-shadow: 1px 1px 0 #000;
        }

        /* Slots Grid - Strictly Square in BOTH layouts! */
        .slots-grid {
            display: grid;
            gap: 6px;
        }
        .layout-horizontal .slots-grid {
            grid-template-columns: repeat(6, 116px);
            justify-content: space-between;
        }
        .layout-vertical .slots-grid {
            grid-template-columns: repeat(2, 136px);
            justify-content: center;
        }

        /* Strictly Square Mon Card */
        .mon-card {
            background: rgba(24, 31, 51, 0.9);
            border: 2px solid #334155;
            outline: 1px solid #0f172a;
            box-shadow: inset 0 0 0 1px #475569;
            border-radius: 4px;
            padding: 5px 4px;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: space-between;
            position: relative;
            overflow: hidden;
            transition: all 0.2s ease;
        }

        .layout-horizontal .mon-card {
            width: 116px;
            height: 116px; /* Exactly square */
        }
        .layout-vertical .mon-card {
            width: 136px;
            height: 136px; /* Exactly square */
        }

        .mon-card.dead {
            filter: grayscale(1);
            opacity: 0.55;
            border-color: #ef4444 !important;
            box-shadow: inset 0 0 0 1px #7f1d1d !important;
            background: rgba(45, 12, 12, 0.85) !important;
        }

        .dead-badge {
            position: absolute;
            top: 4px;
            right: 4px;
            background: rgba(220, 38, 38, 0.95);
            color: #ffffff;
            font-size: 7px;
            font-weight: bold;
            padding: 2px 3px;
            border: 1px solid #000;
            box-shadow: 1px 1px 0 #000;
            z-index: 5;
            text-shadow: 1px 1px 0 #000;
        }

        /* Sprite Frame */
        .sprite-frame {
            width: 48px;
            height: 48px;
            background: radial-gradient(circle, rgba(255,255,255,0.08) 0%, rgba(0,0,0,0.5) 80%);
            border: 2px solid #1e293b;
            box-shadow: inset 0 0 0 1px #334155;
            border-radius: 3px;
            display: flex;
            align-items: center;
            justify-content: center;
            flex-shrink: 0;
            position: relative;
        }
        .layout-vertical .sprite-frame {
            width: 58px;
            height: 58px;
        }

        .sprite {
            max-width: 44px;
            max-height: 44px;
            image-rendering: pixelated;
        }
        .layout-vertical .sprite {
            max-width: 52px;
            max-height: 52px;
        }

        /* Mon Info Area inside square */
        .mon-info {
            width: 100%;
            display: flex;
            flex-direction: column;
            gap: 2px;
            min-width: 0;
        }

        .name-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            width: 100%;
        }

        .mon-name {
            font-size: 7.5px;
            color: #ffffff;
            text-shadow: 1px 1px 0 #000;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            max-width: 72px;
        }
        .layout-vertical .mon-name {
            font-size: 8px;
            max-width: 86px;
        }

        .mon-level {
            font-size: 7px;
            color: #facc15;
            text-shadow: 1px 1px 0 #78350f;
            white-space: nowrap;
            flex-shrink: 0;
        }

        /* Authentic Pokemon HP Container */
        .hp-container {
            display: flex;
            align-items: center;
            gap: 2px;
            width: 100%;
        }

        .hp-badge {
            background: #eab308;
            color: #000;
            font-size: 5.5px;
            font-weight: bold;
            padding: 1px 2px;
            border-radius: 1px;
            line-height: 1;
            box-shadow: 1px 1px 0 #000;
            flex-shrink: 0;
        }

        .hp-bar-track {
            background: #0f172a;
            border: 1px solid #000;
            box-shadow: inset 0 1px 0 rgba(0,0,0,0.8);
            border-radius: 2px;
            height: 6px;
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
            font-size: 6.5px;
            color: #94a3b8;
            text-shadow: 1px 1px 0 #000;
            text-align: right;
            line-height: 1;
        }

        /* Soul Link Badge */
        .link-badge {
            font-size: 6px;
            background: #581c87;
            border: 1px solid #c084fc;
            box-shadow: 1px 1px 0 #000;
            color: #f5d0fe;
            text-shadow: 1px 1px 0 #000;
            padding: 1px 2px;
            border-radius: 2px;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            display: block;
            text-align: center;
        }
        .link-badge.dead {
            background: #450a0a;
            border-color: #ef4444;
            color: #fca5a5;
        }

        .container.solo {
            width: fit-content !important;
            max-width: 100vw;
            margin: 0;
            padding: 8px;
        }

        /* Empty Square Slot */
        .empty-slot {
            border: 2px dashed #334155;
            background: rgba(15, 23, 42, 0.35);
            border-radius: 4px;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            gap: 4px;
            color: #475569;
            font-size: 7px;
            text-shadow: 1px 1px 0 #000;
        }
        .layout-horizontal .empty-slot {
            width: 116px;
            height: 116px;
        }
        .layout-vertical .empty-slot {
            width: 136px;
            height: 136px;
        }
    </style>
</head>
<body>
    <div id="container" class="container layout-horizontal"></div>

    <script>
        const PKMN_NAMES_FR = {"1": "Bulbizarre", "2": "Herbizarre", "3": "Florizarre", "4": "Salamèche", "5": "Reptincel", "6": "Dracaufeu", "7": "Carapuce", "8": "Carabaffe", "9": "Tortank", "10": "Chenipan", "11": "Chrysacier", "12": "Papilusion", "13": "Aspicot", "14": "Coconfort", "15": "Dardargnan", "16": "Roucool", "17": "Roucoups", "18": "Roucarnage", "19": "Rattata", "20": "Rattatac", "21": "Piafabec", "22": "Rapasdepic", "23": "Abo", "24": "Arbok", "25": "Pikachu", "26": "Raichu", "27": "Sabelette", "28": "Sablaireau", "29": "Nidoran♀", "30": "Nidorina", "31": "Nidoqueen", "32": "Nidoran♂", "33": "Nidorino", "34": "Nidoking", "35": "Mélofée", "36": "Mélodelfe", "37": "Goupix", "38": "Feunard", "39": "Rondoudou", "40": "Grodoudou", "41": "Nosferapti", "42": "Nosferalto", "43": "Mystherbe", "44": "Ortide", "45": "Rafflesia", "46": "Paras", "47": "Parasect", "48": "Mimitoss", "49": "Aéromite", "50": "Taupiqueur", "51": "Triopikeur", "52": "Miaouss", "53": "Persian", "54": "Psykokwak", "55": "Akwakwak", "56": "Férosinge", "57": "Colossinge", "58": "Caninos", "59": "Arcanin", "60": "Ptitard", "61": "Têtarte", "62": "Tartard", "63": "Abra", "64": "Kadabra", "65": "Alakazam", "66": "Machoc", "67": "Machopeur", "68": "Mackogneur", "69": "Chétiflor", "70": "Boustiflor", "71": "Empiflor", "72": "Tentacool", "73": "Tentacruel", "74": "Racaillou", "75": "Gravalanch", "76": "Grolem", "77": "Ponyta", "78": "Galopa", "79": "Ramoloss", "80": "Flagadoss", "81": "Magnéti", "82": "Magnéton", "83": "Canarticho", "84": "Doduo", "85": "Dodrio", "86": "Otaria", "87": "Lamantine", "88": "Tadmorv", "89": "Grotadmorv", "90": "Kokiyas", "91": "Crustabri", "92": "Fantominus", "93": "Spectrum", "94": "Ectoplasma", "95": "Onix", "96": "Soporifik", "97": "Hypnomade", "98": "Krabby", "99": "Krabboss", "100": "Voltorbe", "101": "Électrode", "102": "Noeunoeuf", "103": "Noadkoko", "104": "Osselait", "105": "Ossatueur", "106": "Kicklee", "107": "Tygnon", "108": "Excelangue", "109": "Smogo", "110": "Smogogo", "111": "Rhinocorne", "112": "Rhinoféros", "113": "Leveinard", "114": "Saquedeneu", "115": "Kangourex", "116": "Hypotrempe", "117": "Hypocéan", "118": "Poissirène", "119": "Poissoroy", "120": "Stari", "121": "Staross", "122": "M. Mime", "123": "Insécateur", "124": "Lippoutou", "125": "Élektek", "126": "Magmar", "127": "Scarabrute", "128": "Tauros", "129": "Magicarpe", "130": "Léviator", "131": "Lokhlass", "132": "Métamorph", "133": "Évoli", "134": "Aquali", "135": "Voltali", "136": "Pyroli", "137": "Porygon", "138": "Amonita", "139": "Amonistar", "140": "Kabuto", "141": "Kabutops", "142": "Ptéra", "143": "Ronflex", "144": "Artikodin", "145": "Électhor", "146": "Sulfura", "147": "Minidraco", "148": "Draco", "149": "Dracolosse", "150": "Mewtwo", "151": "Mew", "152": "Germignon", "153": "Macronium", "154": "Méganium", "155": "Héricendre", "156": "Feurisson", "157": "Typhlosion", "158": "Kaiminus", "159": "Crocrodil", "160": "Aligatueur", "161": "Fouinette", "162": "Fouinar", "163": "Hoothoot", "164": "Noarfang", "165": "Coxy", "166": "Coxyclaque", "167": "Mimigal", "168": "Migalos", "169": "Nostenfer", "170": "Loupio", "171": "Lanturn", "172": "Pichu", "173": "Mélo", "174": "Toudoudou", "175": "Togepi", "176": "Togetic", "177": "Natu", "178": "Xatu", "179": "Wattouat", "180": "Lainergie", "181": "Pharamp", "182": "Joliflor", "183": "Marill", "184": "Azumarill", "185": "Simularbre", "186": "Tarpaud", "187": "Granivol", "188": "Floravol", "189": "Cotovol", "190": "Capumain", "191": "Tournegrin", "192": "Héliatronc", "193": "Yanma", "194": "Axoloto", "195": "Maraiste", "196": "Mentali", "197": "Noctali", "198": "Cornèbre", "199": "Roigada", "200": "Feuforêve", "201": "Zarbi", "202": "Qulbutoké", "203": "Girafarig", "204": "Pomdepik", "205": "Foretress", "206": "Insolourdo", "207": "Scorplane", "208": "Steelix", "209": "Snubbull", "210": "Granbull", "211": "Qwilfish", "212": "Cizayox", "213": "Caratroc", "214": "Scarhino", "215": "Farfuret", "216": "Teddiursa", "217": "Ursaring", "218": "Limagma", "219": "Volcaropod", "220": "Marcacrin", "221": "Cochignon", "222": "Corayon", "223": "Rémoraid", "224": "Octillery", "225": "Cadoizo", "226": "Démanta", "227": "Airmure", "228": "Malosse", "229": "Démolosse", "230": "Hyporoi", "231": "Phanpy", "232": "Donphan", "233": "Porygon2", "234": "Cerfrousse", "235": "Queulorior", "236": "Debugant", "237": "Kapoera", "238": "Lippouti", "239": "Élekid", "240": "Magby", "241": "Écrémeuh", "242": "Leuphorie", "243": "Raikou", "244": "Entei", "245": "Suicune", "246": "Embrylex", "247": "Ymphect", "248": "Tyranocif", "249": "Lugia", "250": "Ho-Oh", "251": "Celebi", "252": "Arcko", "253": "Massko", "254": "Jungko", "255": "Poussifeu", "256": "Galifeu", "257": "Braségali", "258": "Gobou", "259": "Flobio", "260": "Laggron", "261": "Medhyèna", "262": "Grahyèna", "263": "Zigzaton", "264": "Linéon", "265": "Chenipotte", "266": "Armulys", "267": "Charmillon", "268": "Blindalys", "269": "Papinox", "270": "Nénupiot", "271": "Lombre", "272": "Ludicolo", "273": "Grainipiot", "274": "Pifeuil", "275": "Tengalice", "276": "Nirondelle", "277": "Hélédelle", "278": "Goélise", "279": "Bekipan", "280": "Tarsal", "281": "Kirlia", "282": "Gardevoir", "283": "Arakdo", "284": "Maskadra", "285": "Balignon", "286": "Chapignon", "287": "Parecool", "288": "Vigoroth", "289": "Monaflèmit", "290": "Ningale", "291": "Ninjask", "292": "Munja", "293": "Chuchmur", "294": "Ramboum", "295": "Brouhabam", "296": "Makuhita", "297": "Hariyama", "298": "Azurill", "299": "Tarinor", "300": "Skitty", "301": "Delcatty", "302": "Ténéfix", "303": "Mysdibule", "304": "Galekid", "305": "Galegon", "306": "Galeking", "307": "Méditikka", "308": "Charmina", "309": "Dynavolt", "310": "Élecsprint", "311": "Posipi", "312": "Négapi", "313": "Muciole", "314": "Lumivole", "315": "Rosélia", "316": "Gloupti", "317": "Avaltout", "318": "Carvanha", "319": "Sharpedo", "320": "Wailmer", "321": "Wailord", "322": "Chamallot", "323": "Camérupt", "324": "Chartor", "325": "Spoink", "326": "Groret", "327": "Spinda", "328": "Kraknoix", "329": "Vibraninf", "330": "Libégon", "331": "Cacnea", "332": "Cacturne", "333": "Tylton", "334": "Altaria", "335": "Mangriff", "336": "Séviper", "337": "Séléroc", "338": "Solaroc", "339": "Barloche", "340": "Barbicha", "341": "Écrapince", "342": "Colhomard", "343": "Balbuto", "344": "Kaorine", "345": "Lilia", "346": "Vacilys", "347": "Anorith", "348": "Armaldo", "349": "Barpau", "350": "Milobellus", "351": "Morphéo", "352": "Kecleon", "353": "Polichombr", "354": "Branette", "355": "Skelénox", "356": "Téraclope", "357": "Tropius", "358": "Éoko", "359": "Absol", "360": "Okéoké", "361": "Stalgamin", "362": "Oniglali", "363": "Obalie", "364": "Phogleur", "365": "Kaimorse", "366": "Coquiperl", "367": "Serpang", "368": "Rosabyss", "369": "Relicanth", "370": "Lovdisc", "371": "Draby", "372": "Drackhaus", "373": "Drattak", "374": "Terhal", "375": "Métang", "376": "Métalosse", "377": "Regirock", "378": "Regice", "379": "Registeel", "380": "Latias", "381": "Latios", "382": "Kyogre", "383": "Groudon", "384": "Rayquaza", "385": "Jirachi", "386": "Deoxys", "387": "Tortipouss", "388": "Boskara", "389": "Torterra", "390": "Ouisticram", "391": "Chimpenfeu", "392": "Simiabraz", "393": "Tiplouf", "394": "Prinplouf", "395": "Pingoléon", "396": "Étourmi", "397": "Étourvol", "398": "Étouraptor", "399": "Keunotor", "400": "Castorno", "401": "Crikzik", "402": "Mélokrik", "403": "Lixy", "404": "Luxio", "405": "Luxray", "406": "Rozbouton", "407": "Roserade", "408": "Kranidos", "409": "Charkos", "410": "Dinoclier", "411": "Bastiodon", "412": "Cheniti", "413": "Cheniselle", "414": "Papilord", "415": "Apitrini", "416": "Apireine", "417": "Pachirisu", "418": "Mustébouée", "419": "Mustéflott", "420": "Ceribou", "421": "Ceriflor", "422": "Sancoki", "423": "Tritosor", "424": "Capidextre", "425": "Baudrive", "426": "Grodrive", "427": "Laporeille", "428": "Lockpin", "429": "Magirêve", "430": "Corboss", "431": "Chaglam", "432": "Chaffreux", "433": "Korillon", "434": "Moufouette", "435": "Moufflair", "436": "Archéomire", "437": "Archéodong", "438": "Manzaï", "439": "Mime Jr", "440": "Ptiravi", "441": "Pijako", "442": "Spiritomb", "443": "Griknot", "444": "Carmache", "445": "Carchacrok", "446": "Goinfrex", "447": "Riolu", "448": "Lucario", "449": "Hippopotas", "450": "Hippodocus", "451": "Rapion", "452": "Drascore", "453": "Cradopaud", "454": "Coatox", "455": "Vortente", "456": "Écayon", "457": "Luminéon", "458": "Babimanta", "459": "Blizzi", "460": "Blizzaroi", "461": "Dimoret", "462": "Magnézone", "463": "Coudlangue", "464": "Rhinastoc", "465": "Bouldeneu", "466": "Élekable", "467": "Maganon", "468": "Togekiss", "469": "Yanmega", "470": "Phyllali", "471": "Givrali", "472": "Scorvol", "473": "Mammochon", "474": "Porygon-Z", "475": "Gallame", "476": "Tarinorme", "477": "Noctunoir", "478": "Momartik", "479": "Motisma", "480": "Créhelf", "481": "Créfollet", "482": "Créfadet", "483": "Dialga", "484": "Palkia", "485": "Heatran", "486": "Regigigas", "487": "Giratina", "488": "Cresselia", "489": "Phione", "490": "Manaphy", "491": "Darkrai", "492": "Shaymin", "493": "Arceus"};

        const params = new URLSearchParams(window.location.search);
        const layout = params.get('layout') || 'horizontal';
        const targetPlayer = params.get('player') || 'all';

        const container = document.getElementById('container');
        if (layout === 'vertical') {
            container.className = 'container layout-vertical';
        } else {
            container.className = 'container layout-horizontal';
        }
        if (targetPlayer === 'me' || targetPlayer === '1') {
            container.classList.add('solo');
        }

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

        function renderPlayerCard(player, pairs) {
            const box = document.createElement('div');
            box.className = 'team-box';
            const color = getRoleColor(player.role || 1);
            box.style.borderColor = color + 'aa';

            const title = document.createElement('div');
            title.className = 'team-title';
            title.style.color = color;
            title.innerHTML = `
                <span>${player.name}</span>
                <span class="alive-badge">${player.alive_count || 0}/6 Vivants</span>
            `;
            box.appendChild(title);

            const grid = document.createElement('div');
            grid.className = 'slots-grid';

            for (let i = 0; i < 6; i++) {
                const mon = player.team && player.team[i] ? player.team[i] : null;
                if (!mon || !mon.species) {
                    const empty = document.createElement('div');
                    empty.className = 'empty-slot';
                    empty.innerHTML = `<span>◓ Slot ${i+1}</span><span style="opacity:0.4;">Vide</span>`;
                    grid.appendChild(empty);
                    continue;
                }

                const hpPct = mon.max_hp > 0 ? Math.max(0, Math.min(100, (mon.hp / mon.max_hp) * 100)) : 0;
                let hpClass = '';
                if (hpPct <= 20) hpClass = 'crit';
                else if (hpPct <= 50) hpClass = 'warn';

                let linkHtml = '';
                if (pairs && pairs.length > 0) {
                    for (const p of pairs) {
                        const isMember = (p.members && p.members.some(m => m.role === player.role && m.slot === (i + 1)))
                                      || (player.is_me && p.p1_slot === (i + 1))
                                      || (!player.is_me && p.p2_slot === (i + 1));
                        if (isMember) {
                            if (p.status === 'DEAD') {
                                linkHtml = `<span class="link-badge dead">🔗 ÂME BRISÉE</span>`;
                            } else {
                                linkHtml = `<span class="link-badge">🔗 LIÉ [Zone ${p.location_id}]</span>`;
                            }
                            break;
                        }
                    }
                }

                const name = getMonName(mon.species);
                const card = document.createElement('div');
                card.className = `mon-card ${mon.is_fainted ? 'dead' : ''}`;
                const deadBadge = mon.is_fainted ? '<div class="dead-badge">💀 K.O.</div>' : '';

                card.innerHTML = `
                    ${deadBadge}
                    <div class="sprite-frame">
                        <img class="sprite" src="${getSpriteUrl(mon.species)}" onerror="this.onerror=null; this.src=getFallbackSpriteUrl(${mon.species});" alt="${name}">
                    </div>
                    <div class="mon-info">
                        <div class="name-row">
                            <span class="mon-name" title="${name}">${name}</span>
                            <span class="mon-level">Nv.${mon.level}</span>
                        </div>
                        <div class="hp-container">
                            <div class="hp-badge">PV</div>
                            <div class="hp-bar-track">
                                <div class="hp-bar-fill ${hpClass}" style="width: ${hpPct}%;"></div>
                            </div>
                        </div>
                        <div class="hp-numbers">${mon.hp}/${mon.max_hp}</div>
                        ${linkHtml}
                    </div>
                `;
                grid.appendChild(card);
            }

            box.appendChild(grid);
            return box;
        }

        async function pollTeams() {
            try {
                const res = await fetch('/api/teams');
                if (!res.ok) return;
                const data = await res.json();
                if (!data.active) return;

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
                } else {
                    if (data.player1) {
                        playersToRender.push({
                            role: 1,
                            name: 'Joueur 1 (Moi)',
                            is_me: true,
                            alive_count: data.player1.filter(m => !m.is_fainted).length,
                            team: data.player1
                        });
                    }
                    if (targetPlayer === 'all' && data.player2 && data.player2.length > 0) {
                        playersToRender.push({
                            role: 2,
                            name: 'Joueur 2 (Partenaire)',
                            is_me: false,
                            alive_count: data.player2.filter(m => !m.is_fainted).length,
                            team: data.player2
                        });
                    }
                }

                container.innerHTML = '';
                for (const player of playersToRender) {
                    container.appendChild(renderPlayerCard(player, data.pairs));
                }
            } catch (err) {}
        }

        setInterval(pollTeams, 300);
        pollTeams();
    </script>
</body>
</html>
)RAWHTML";
}
