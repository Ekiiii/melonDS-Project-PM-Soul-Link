#include "DirectP2PDialog.h"
#include "DirectP2P.h"
#include "MpOnline.h"
#include "Window.h"
#include "Config.h"

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFormLayout>
#include <QFont>
#include <thread>

DirectP2PDialog::DirectP2PDialog(MainWindow* parent)
    : QDialog(parent), mainWin(parent)
{
    setWindowTitle("Direct P2P Soul Link");
    resize(500, 420);
    setWindowFlags(Qt::Window
        | Qt::CustomizeWindowHint | Qt::WindowTitleHint
        | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);

    setupUI();

    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &DirectP2PDialog::onUpdateTimer);
    pollTimer->start(500);

    updateStatus();
}

DirectP2PDialog::~DirectP2PDialog()
{
}

DirectP2PDialog* DirectP2PDialog::openDlg(MainWindow* parent)
{
    DirectP2PDialog* dlg = new DirectP2PDialog(parent);
    dlg->show();
    return dlg;
}

void DirectP2PDialog::setupUI()
{
    QVBoxLayout* mainLay = new QVBoxLayout(this);
    mainLay->setSpacing(12);

    // Header banner
    QLabel* titleLbl = new QLabel("<h2 style='margin:0; padding:0; color:#1e293b;'>Direct P2P — Soul Link</h2>", this);
    QLabel* descLbl = new QLabel("Direct peer-to-peer connection for Soul Link co-op.", this);
    descLbl->setStyleSheet("color: #64748b; font-size: 12px; margin-bottom: 4px;");
    mainLay->addWidget(titleLbl);
    mainLay->addWidget(descLbl);

    tabWidget = new QTabWidget(this);

    // -------------------------------------------------------------------------
    // TAB 1: HOST
    // -------------------------------------------------------------------------
    QWidget* tabHost = new QWidget(tabWidget);
    QVBoxLayout* layHost = new QVBoxLayout(tabHost);
    layHost->setSpacing(10);

    // Host Config Section
    hostConfigWidget = new QWidget(tabHost);
    QVBoxLayout* layHostCfg = new QVBoxLayout(hostConfigWidget);
    layHostCfg->setContentsMargins(0, 0, 0, 0);
    layHostCfg->setSpacing(10);

    QFormLayout* formHost = new QFormLayout();
    edHostName = new QLineEdit(hostConfigWidget);
    edHostName->setMaxLength(23);
    edHostName->setText(mainWin->onlineDefaultName());
    edHostName->setStyleSheet("padding: 6px; border: 1px solid #cbd5e1; border-radius: 4px;");

    chkUPnP = new QCheckBox("Enable UPnP", hostConfigWidget);
    chkUPnP->setChecked(true);

    formHost->addRow("Player Name:", edHostName);
    formHost->addRow("", chkUPnP);
    layHostCfg->addLayout(formHost);

    btnStartHost = new QPushButton("Host Room", hostConfigWidget);
    btnStartHost->setStyleSheet(
        "QPushButton { font-weight: 600; font-size: 13px; padding: 10px; background-color: #2563eb; color: white; border: none; border-radius: 6px; }"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:disabled { background-color: #94a3b8; }"
    );
    connect(btnStartHost, &QPushButton::clicked, this, &DirectP2PDialog::onStartHostClicked);
    layHostCfg->addWidget(btnStartHost);

    layHost->addWidget(hostConfigWidget);

    // Host Active Section
    hostActiveWidget = new QWidget(tabHost);
    QVBoxLayout* layHostAct = new QVBoxLayout(hostActiveWidget);
    layHostAct->setContentsMargins(0, 0, 0, 0);
    layHostAct->setSpacing(8);

    QGroupBox* codeBox = new QGroupBox("Room Codes", hostActiveWidget);
    QVBoxLayout* layCodeBox = new QVBoxLayout(codeBox);
    layCodeBox->setSpacing(6);

    QLabel* lblDescCode = new QLabel("<b>Internet Code:</b>", codeBox);
    lblHostRoomCode = new QLabel("SL-XXXXX-XXXXX", codeBox);
    QFont fCode = lblHostRoomCode->font();
    fCode.setFamily("Consolas");
    fCode.setPointSize(14);
    fCode.setBold(true);
    lblHostRoomCode->setFont(fCode);
    lblHostRoomCode->setAlignment(Qt::AlignCenter);
    lblHostRoomCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lblHostRoomCode->setStyleSheet("background: #0f172a; color: #38bdf8; padding: 8px; border: 1px solid #334155; border-radius: 6px; letter-spacing: 2px;");

    btnCopyHostCode = new QPushButton("Copy Internet Code", codeBox);
    btnCopyHostCode->setStyleSheet(
        "QPushButton { font-weight: 600; padding: 6px; background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 5px; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    connect(btnCopyHostCode, &QPushButton::clicked, this, &DirectP2PDialog::onCopyHostCodeClicked);

    QLabel* lblDescLan = new QLabel("<b>LAN / Local Code:</b>", codeBox);
    lblHostLanCode = new QLabel("SL-XXXXX-XXXXX", codeBox);
    lblHostLanCode->setFont(fCode);
    lblHostLanCode->setAlignment(Qt::AlignCenter);
    lblHostLanCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lblHostLanCode->setStyleSheet("background: #0f172a; color: #a7f3d0; padding: 8px; border: 1px solid #334155; border-radius: 6px; letter-spacing: 2px;");

    btnCopyHostLanCode = new QPushButton("Copy LAN Code", codeBox);
    btnCopyHostLanCode->setStyleSheet(
        "QPushButton { font-weight: 600; padding: 6px; background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 5px; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    connect(btnCopyHostLanCode, &QPushButton::clicked, this, &DirectP2PDialog::onCopyHostLanCodeClicked);

    layCodeBox->addWidget(lblDescCode);
    layCodeBox->addWidget(lblHostRoomCode);
    layCodeBox->addWidget(btnCopyHostCode);
    layCodeBox->addWidget(lblDescLan);
    layCodeBox->addWidget(lblHostLanCode);
    layCodeBox->addWidget(btnCopyHostLanCode);
    layHostAct->addWidget(codeBox);

    lblHostUPnPStatus = new QLabel("UPnP: Ready", hostActiveWidget);
    lblHostPeersStatus = new QLabel("Connected Players: 0/7", hostActiveWidget);
    lblHostPeersStatus->setStyleSheet("font-weight: 600;");

    lblHostRoster = new QLabel(hostActiveWidget);
    lblHostRoster->setStyleSheet("background: #0f172a; color: #f8fafc; padding: 8px; border: 1px solid #334155; border-radius: 6px; font-family: Consolas, monospace;");
    lblHostRoster->setWordWrap(true);

    btnStopHost = new QPushButton("Close Room", hostActiveWidget);
    btnStopHost->setStyleSheet(
        "QPushButton { background-color: #dc2626; color: white; padding: 8px; font-weight: 600; border: none; border-radius: 5px; }"
        "QPushButton:hover { background-color: #b91c1c; }"
    );
    connect(btnStopHost, &QPushButton::clicked, this, &DirectP2PDialog::onStopHostClicked);

    layHostAct->addWidget(lblHostUPnPStatus);
    layHostAct->addWidget(lblHostPeersStatus);
    layHostAct->addWidget(lblHostRoster);
    layHostAct->addWidget(btnStopHost);

    layHost->addWidget(hostActiveWidget);
    layHost->addStretch();
    tabWidget->addTab(tabHost, "Host");

    // -------------------------------------------------------------------------
    // TAB 2: JOIN
    // -------------------------------------------------------------------------
    QWidget* tabJoin = new QWidget(tabWidget);
    QVBoxLayout* layJoin = new QVBoxLayout(tabJoin);
    layJoin->setSpacing(10);

    joinConfigWidget = new QWidget(tabJoin);
    QVBoxLayout* layJoinCfg = new QVBoxLayout(joinConfigWidget);
    layJoinCfg->setContentsMargins(0, 0, 0, 0);
    layJoinCfg->setSpacing(10);

    QFormLayout* formJoin = new QFormLayout();
    edJoinName = new QLineEdit(joinConfigWidget);
    edJoinName->setMaxLength(23);
    edJoinName->setText(mainWin->onlineDefaultName());
    edJoinName->setStyleSheet("padding: 6px; border: 1px solid #cbd5e1; border-radius: 4px;");

    edJoinCode = new QLineEdit(joinConfigWidget);
    edJoinCode->setPlaceholderText("e.g. SL-4LADD-A69NE or IP:Port");
    edJoinCode->setStyleSheet("padding: 6px; border: 1px solid #cbd5e1; border-radius: 4px; font-family: Consolas, monospace;");

    formJoin->addRow("Player Name:", edJoinName);
    formJoin->addRow("Room Code:", edJoinCode);
    layJoinCfg->addLayout(formJoin);

    btnTestLocalJoin = new QPushButton("Connect to Localhost (127.0.0.1)", joinConfigWidget);
    btnTestLocalJoin->setStyleSheet(
        "QPushButton { font-weight: 600; font-size: 12px; padding: 8px; background-color: #f0fdf4; color: #15803d; border: 1px solid #86efac; border-radius: 6px; }"
        "QPushButton:hover { background-color: #dcfce7; }"
    );
    connect(btnTestLocalJoin, &QPushButton::clicked, this, [this]() {
        edJoinCode->setText("127.0.0.1:7820");
        onJoinClicked();
    });
    layJoinCfg->addWidget(btnTestLocalJoin);

    lblJoinError = new QLabel(joinConfigWidget);
    lblJoinError->setWordWrap(true);
    lblJoinError->setStyleSheet("color: #dc2626; font-weight: 600; font-size: 12px; padding: 4px 0;");
    lblJoinError->setVisible(false);
    layJoinCfg->addWidget(lblJoinError);

    btnJoin = new QPushButton("Join Room", joinConfigWidget);
    btnJoin->setStyleSheet(
        "QPushButton { font-weight: 600; font-size: 13px; padding: 10px; background-color: #16a34a; color: white; border: none; border-radius: 6px; }"
        "QPushButton:hover { background-color: #15803d; }"
        "QPushButton:disabled { background-color: #94a3b8; }"
    );
    connect(btnJoin, &QPushButton::clicked, this, &DirectP2PDialog::onJoinClicked);
    layJoinCfg->addWidget(btnJoin);

    layJoin->addWidget(joinConfigWidget);

    joinActiveWidget = new QWidget(tabJoin);
    QVBoxLayout* layJoinAct = new QVBoxLayout(joinActiveWidget);
    layJoinAct->setContentsMargins(0, 0, 0, 0);
    layJoinAct->setSpacing(8);

    lblJoinStatus = new QLabel("Connecting to host...", joinActiveWidget);
    lblJoinStatus->setStyleSheet("font-weight: 600;");

    lblJoinRoster = new QLabel(joinActiveWidget);
    lblJoinRoster->setStyleSheet("background: #0f172a; color: #f8fafc; padding: 8px; border: 1px solid #334155; border-radius: 6px; font-family: Consolas, monospace;");
    lblJoinRoster->setWordWrap(true);

    btnLeaveJoin = new QPushButton("Leave Room", joinActiveWidget);
    btnLeaveJoin->setStyleSheet(
        "QPushButton { background-color: #dc2626; color: white; padding: 8px; font-weight: 600; border: none; border-radius: 5px; }"
        "QPushButton:hover { background-color: #b91c1c; }"
    );
    connect(btnLeaveJoin, &QPushButton::clicked, this, &DirectP2PDialog::onLeaveClicked);

    layJoinAct->addWidget(lblJoinStatus);
    layJoinAct->addWidget(lblJoinRoster);
    layJoinAct->addWidget(btnLeaveJoin);

    layJoin->addWidget(joinActiveWidget);
    layJoin->addStretch();
    tabWidget->addTab(tabJoin, "Join");

    mainLay->addWidget(tabWidget);

    // Dialog Footer
    QHBoxLayout* footLay = new QHBoxLayout();
    QPushButton* btnOverlay = new QPushButton("Open OBS Overlay", this);
    btnOverlay->setStyleSheet(
        "QPushButton { padding: 6px 12px; background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 5px; font-weight: 500; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    connect(btnOverlay, &QPushButton::clicked, this, &DirectP2PDialog::onOpenOverlayClicked);

    QPushButton* btnClose = new QPushButton("Close", this);
    btnClose->setStyleSheet(
        "QPushButton { padding: 6px 14px; background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 5px; font-weight: 500; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    connect(btnClose, &QPushButton::clicked, this, &QDialog::hide);

    footLay->addWidget(btnOverlay);
    footLay->addStretch();
    footLay->addWidget(btnClose);
    mainLay->addLayout(footLay);
}

void DirectP2PDialog::onStartHostClicked()
{
    QString name = edHostName->text().trimmed();
    if (name.isEmpty()) name = "Player";
    int port = DirectP2P::DEFAULT_PORT;
    bool useUPnP = chkUPnP->isChecked();

    // Save name to config
    Config::Table& globalCfg = mainWin->globalCfg;
    globalCfg.SetQString("Online.PlayerName", name);
    Config::Save();

    btnStartHost->setEnabled(false);
    btnStartHost->setText("Starting...");

    // Run IP lookup & UPnP in worker thread so UI doesn't freeze
    std::thread([this, name, port, useUPnP]() {
        std::string localIp = DirectP2P::GetLocalIP();
        std::string pubIp = DirectP2P::GetPublicIP(2500);
        std::string upnpMsg;
        bool upnpOk = false;

        if (useUPnP) {
            upnpOk = DirectP2P::UPnPOpenPort((uint16_t)port, localIp, upnpMsg);
        } else {
            upnpMsg = "UPnP: Disabled";
        }

        std::string internetIp = pubIp.empty() ? localIp : pubIp;
        std::string roomCodeInternet = DirectP2P::EncodeRoomCode(internetIp, (uint16_t)port);
        std::string roomCodeLan = DirectP2P::EncodeRoomCode(localIp, (uint16_t)port);

        QMetaObject::invokeMethod(this, [this, name, port, roomCodeInternet, roomCodeLan, upnpMsg, upnpOk]() {
            btnStartHost->setEnabled(true);
            btnStartHost->setText("Host Room");

            MpDirectHost(port, name.toStdString().c_str(), roomCodeInternet.c_str());

            lblHostRoomCode->setText(QString::fromStdString(roomCodeInternet));
            lblHostLanCode->setText(QString::fromStdString(roomCodeLan));
            if (upnpOk)
                lblHostUPnPStatus->setText(QString("<font color='#10b981'>●</font> %1").arg(QString::fromStdString(upnpMsg)));
            else
                lblHostUPnPStatus->setText(QString("<font color='#f59e0b'>●</font> %1").arg(QString::fromStdString(upnpMsg)));

            updateStatus();
        });
    }).detach();
}

void DirectP2PDialog::onStopHostClicked()
{
    int port = DirectP2P::DEFAULT_PORT;
    MpOnlineStop();
    DirectP2P::UPnPClosePort((uint16_t)port);
    updateStatus();
}

void DirectP2PDialog::onJoinClicked()
{
    QString name = edJoinName->text().trimmed();
    if (name.isEmpty()) name = "Player";
    QString codeStr = edJoinCode->text().trimmed();

    if (codeStr.isEmpty()) {
        lblJoinError->setText("Please enter a room code or IP address.");
        lblJoinError->setVisible(true);
        return;
    }

    std::string outIp;
    uint16_t outPort = DirectP2P::DEFAULT_PORT;
    if (!DirectP2P::DecodeRoomCode(codeStr.toStdString(), outIp, outPort)) {
        lblJoinError->setText("Invalid room code format. Expected SL-XXXXX-XXXXX or IP:Port.");
        lblJoinError->setVisible(true);
        return;
    }

    lblJoinError->setVisible(false);
    btnJoin->setEnabled(false);
    btnJoin->setText("Connecting...");

    Config::Table& globalCfg = mainWin->globalCfg;
    globalCfg.SetQString("Online.PlayerName", name);
    Config::Save();

    joinPending = true;
    joinElapsedSec = 0;

    MpDirectJoin(outIp.c_str(), outPort, name.toStdString().c_str(), codeStr.toStdString().c_str());
    updateStatus();
}

void DirectP2PDialog::onLeaveClicked()
{
    joinPending = false;
    MpOnlineStop();
    btnJoin->setEnabled(true);
    btnJoin->setText("Join Room");
    updateStatus();
}

void DirectP2PDialog::onCopyHostCodeClicked()
{
    QString code = lblHostRoomCode->text();
    QApplication::clipboard()->setText(code);
    btnCopyHostCode->setText("Copied!");
    QTimer::singleShot(2000, this, [this]() {
        btnCopyHostCode->setText("Copy Internet Code");
    });
}

void DirectP2PDialog::onCopyHostLanCodeClicked()
{
    QString code = lblHostLanCode->text();
    QApplication::clipboard()->setText(code);
    btnCopyHostLanCode->setText("Copied!");
    QTimer::singleShot(2000, this, [this]() {
        btnCopyHostLanCode->setText("Copy LAN Code");
    });
}

void DirectP2PDialog::onOpenOverlayClicked()
{
    QDesktopServices::openUrl(QUrl("http://localhost:8080/overlay"));
}

void DirectP2PDialog::onUpdateTimer()
{
    updateStatus();
}

void DirectP2PDialog::updateStatus()
{
    MpOnlineStatus st;
    MpOnlineGetStatus(&st);

    bool isHostActive = (st.mode == 3);
    bool isJoinActive = (st.mode == 4);

    if (joinPending)
    {
        joinElapsedSec++;
        if (st.peers > 0)
        {
            // Successfully connected!
            joinPending = false;
            btnJoin->setEnabled(true);
            btnJoin->setText("Join Room");
            lblJoinError->setVisible(false);
        }
        else if (st.mode == 0 || joinElapsedSec >= 24) // 12 seconds timeout (24 ticks of 500ms)
        {
            // Timed out or connection failed
            joinPending = false;
            MpOnlineStop();
            btnJoin->setEnabled(true);
            btnJoin->setText("Join Room");
            lblJoinError->setText("Connection timed out (12s). Unable to reach host. Check the room code or firewall.");
            lblJoinError->setVisible(true);
            isJoinActive = false;
        }
    }

    hostConfigWidget->setVisible(!isHostActive);
    hostActiveWidget->setVisible(isHostActive);

    joinConfigWidget->setVisible(!isJoinActive);
    joinActiveWidget->setVisible(isJoinActive);

    if (isHostActive)
    {
        if (st.code[0]) {
            lblHostRoomCode->setText(QString(st.code));
        }
        if (lblHostLanCode->text() == "SL-XXXXX-XXXXX") {
            std::string localIp = DirectP2P::GetLocalIP();
            std::string lanCode = DirectP2P::EncodeRoomCode(localIp, DirectP2P::DEFAULT_PORT);
            lblHostLanCode->setText(QString::fromStdString(lanCode));
        }
        lblHostPeersStatus->setText(QString("Connected Players: %1/7").arg(st.peers));

        QString rosterText = "<b>Players:</b><br>";
        for (int r = 1; r <= 8; r++) {
            if (!st.roster[r][0]) continue;
            QString roleTag = (r == 1) ? " [Host]" : "";
            QString pingTag = (st.rosterPing[r] > 0) ? QString(" [%1 ms]").arg(st.rosterPing[r]) : "";
            rosterText += QString("  • %1%2%3<br>").arg(st.roster[r]).arg(roleTag).arg(pingTag);
        }
        lblHostRoster->setText(rosterText);
    }
    else if (isJoinActive)
    {
        if (st.peers > 0) {
            lblJoinStatus->setText(QString("<font color='#10b981'>●</font> Connected to host (%1 players)").arg(st.peers + 1));
        } else {
            int secRemaining = qMax(0, 12 - (joinElapsedSec / 2));
            lblJoinStatus->setText(QString("<font color='#f59e0b'>●</font> Connecting to host (%1s)...").arg(secRemaining));
        }

        QString rosterText = "<b>Players:</b><br>";
        for (int r = 1; r <= 8; r++) {
            if (!st.roster[r][0]) continue;
            QString roleTag = (r == 1) ? " [Host]" : (r == st.myRole ? " [Me]" : "");
            QString pingTag = (st.rosterPing[r] > 0) ? QString(" [%1 ms]").arg(st.rosterPing[r]) : "";
            rosterText += QString("  • %1%2%3<br>").arg(st.roster[r]).arg(roleTag).arg(pingTag);
        }
        lblJoinRoster->setText(rosterText);
    }
}
