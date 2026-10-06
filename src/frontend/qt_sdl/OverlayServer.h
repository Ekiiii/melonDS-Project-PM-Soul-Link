#ifndef OVERLAY_SERVER_H
#define OVERLAY_SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QString>
#include <mutex>
#include "types.h"

class OverlayServer : public QObject
{
    Q_OBJECT
public:
    static OverlayServer& Instance();

    bool Start(quint16 port = 8080);
    void Stop();
    bool IsRunning() const;
    quint16 GetPort() const { return serverPort; }

    void UpdateTeams(const melonDS::u8* partyExp, const melonDS::u8* partyImp, melonDS::u32 partySize);
    void UpdateTeamsMulti(const melonDS::u8* partyExp, const melonDS::u8* partyN, melonDS::u32 partySize,
                          int myRole, const char roster[9][24], const melonDS::u8* partyImp);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    OverlayServer(QObject* parent = nullptr);
    ~OverlayServer();

    QTcpServer* tcpServer;
    quint16 serverPort;
    std::mutex dataMutex;
    QByteArray cachedJsonResponse;
    QByteArray cachedHtml;

    void buildHtml();
    QJsonObject parsePartyPokemon(const melonDS::u8* data, int slotIndex);
};

#endif // OVERLAY_SERVER_H
