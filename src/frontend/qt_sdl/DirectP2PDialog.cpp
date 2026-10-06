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
    setWindowTitle("Direct P2P Soullocke - Connexion Directe");
    resize(520, 480);
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

    // Header banner
    QLabel* titleLbl = new QLabel("<h2>🎮 Direct P2P Soullocke</h2>", this);
    QLabel* descLbl = new QLabel("Connexion directe joueur à joueur sans serveur externe.<br>"
                                 "Ouverture automatique du port (UPnP) et Code de Salon rapide.", this);
    descLbl->setWordWrap(true);
    descLbl->setStyleSheet("color: #888; margin-bottom: 6px;");
    mainLay->addWidget(titleLbl);
    mainLay->addWidget(descLbl);

    tabWidget = new QTabWidget(this);

    // -------------------------------------------------------------------------
    // TAB 1: HOST
    // -------------------------------------------------------------------------
    QWidget* tabHost = new QWidget(tabWidget);
    QVBoxLayout* layHost = new QVBoxLayout(tabHost);

    // Host Config Section
    hostConfigWidget = new QWidget(tabHost);
    QVBoxLayout* layHostCfg = new QVBoxLayout(hostConfigWidget);
    layHostCfg->setContentsMargins(0, 0, 0, 0);

    QFormLayout* formHost = new QFormLayout();
    edHostName = new QLineEdit(hostConfigWidget);
    edHostName->setMaxLength(23);
    edHostName->setText(mainWin->onlineDefaultName());

    spHostPort = new QSpinBox(hostConfigWidget);
    spHostPort->setRange(1024, 65535);
    spHostPort->setValue(DirectP2P::DEFAULT_PORT);

    chkUPnP = new QCheckBox("Activer l'UPnP automatique (ouverture de port sur la Box)", hostConfigWidget);
    chkUPnP->setChecked(true);

    formHost->addRow("Votre Pseudo :", edHostName);
    formHost->addRow("Port TCP :", spHostPort);
    formHost->addRow("", chkUPnP);
    layHostCfg->addLayout(formHost);

    btnStartHost = new QPushButton("🚀 Créer le Salon P2P", hostConfigWidget);
    btnStartHost->setStyleSheet("QPushButton { font-weight: bold; font-size: 13px; padding: 8px; background-color: #0d6efd; color: white; border-radius: 4px; } QPushButton:hover { background-color: #0b5ed7; }");
    connect(btnStartHost, &QPushButton::clicked, this, &DirectP2PDialog::onStartHostClicked);
    layHostCfg->addWidget(btnStartHost);

    layHost->addWidget(hostConfigWidget);

    // Host Active Section
    hostActiveWidget = new QWidget(tabHost);
    QVBoxLayout* layHostAct = new QVBoxLayout(hostActiveWidget);
    layHostAct->setContentsMargins(0, 0, 0, 0);

    QGroupBox* codeBox = new QGroupBox("Code de Salon pour vos amis", hostActiveWidget);
    QVBoxLayout* layCodeBox = new QVBoxLayout(codeBox);

    lblHostRoomCode = new QLabel("SL-XXXXX-XXXXX", codeBox);
    QFont fCode = lblHostRoomCode->font();
    fCode.setPointSize(15);
    fCode.setBold(true);
    lblHostRoomCode->setFont(fCode);
    lblHostRoomCode->setAlignment(Qt::AlignCenter);
    lblHostRoomCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lblHostRoomCode->setStyleSheet("background: #111; color: #00ffcc; padding: 10px; border: 2px dashed #00ffcc; border-radius: 6px; letter-spacing: 2px;");

    btnCopyHostCode = new QPushButton("📋 Copier le Code de Salon", codeBox);
    btnCopyHostCode->setStyleSheet("font-weight: bold; padding: 6px;");
    connect(btnCopyHostCode, &QPushButton::clicked, this, &DirectP2PDialog::onCopyHostCodeClicked);

    layCodeBox->addWidget(lblHostRoomCode);
    layCodeBox->addWidget(btnCopyHostCode);
    layHostAct->addWidget(codeBox);

    lblHostUPnPStatus = new QLabel("UPnP : En attente...", hostActiveWidget);
    lblHostPeersStatus = new QLabel("Joueurs connectés : 0/7", hostActiveWidget);
    lblHostPeersStatus->setStyleSheet("font-weight: bold;");

    lblHostRoster = new QLabel(hostActiveWidget);
    lblHostRoster->setStyleSheet("background: #1c1c1c; padding: 8px; border-radius: 4px; font-family: monospace;");
    lblHostRoster->setWordWrap(true);

    btnStopHost = new QPushButton("🛑 Fermer le Salon", hostActiveWidget);
    btnStopHost->setStyleSheet("QPushButton { background-color: #dc3545; color: white; padding: 6px; font-weight: bold; border-radius: 4px; } QPushButton:hover { background-color: #bb2d3b; }");
    connect(btnStopHost, &QPushButton::clicked, this, &DirectP2PDialog::onStopHostClicked);

    layHostAct->addWidget(lblHostUPnPStatus);
    layHostAct->addWidget(lblHostPeersStatus);
    layHostAct->addWidget(lblHostRoster);
    layHostAct->addWidget(btnStopHost);

    layHost->addWidget(hostActiveWidget);
    layHost->addStretch();
    tabWidget->addTab(tabHost, "👑 Héberger (Host)");

    // -------------------------------------------------------------------------
    // TAB 2: JOIN
    // -------------------------------------------------------------------------
    QWidget* tabJoin = new QWidget(tabWidget);
    QVBoxLayout* layJoin = new QVBoxLayout(tabJoin);

    joinConfigWidget = new QWidget(tabJoin);
    QVBoxLayout* layJoinCfg = new QVBoxLayout(joinConfigWidget);
    layJoinCfg->setContentsMargins(0, 0, 0, 0);

    QFormLayout* formJoin = new QFormLayout();
    edJoinName = new QLineEdit(joinConfigWidget);
    edJoinName->setMaxLength(23);
    edJoinName->setText(mainWin->onlineDefaultName());

    edJoinCode = new QLineEdit(joinConfigWidget);
    edJoinCode->setPlaceholderText("ex: SL-4LADD-A69NE ou IP:Port");

    formJoin->addRow("Votre Pseudo :", edJoinName);
    formJoin->addRow("Code de Salon (ou IP) :", edJoinCode);
    layJoinCfg->addLayout(formJoin);

    QLabel* joinHelp = new QLabel("Collez le code de salon à 10 caractères partagé par l'hôte.<br>"
                                  "L'émulateur décode l'adresse et le port automatiquement.", joinConfigWidget);
    joinHelp->setWordWrap(true);
    joinHelp->setStyleSheet("color: #777; margin-bottom: 6px;");
    layJoinCfg->addWidget(joinHelp);

    btnJoin = new QPushButton("🚀 Rejoindre la Partie", joinConfigWidget);
    btnJoin->setStyleSheet("QPushButton { font-weight: bold; font-size: 13px; padding: 8px; background-color: #198754; color: white; border-radius: 4px; } QPushButton:hover { background-color: #157347; }");
    connect(btnJoin, &QPushButton::clicked, this, &DirectP2PDialog::onJoinClicked);
    layJoinCfg->addWidget(btnJoin);

    layJoin->addWidget(joinConfigWidget);

    joinActiveWidget = new QWidget(tabJoin);
    QVBoxLayout* layJoinAct = new QVBoxLayout(joinActiveWidget);
    layJoinAct->setContentsMargins(0, 0, 0, 0);

    lblJoinStatus = new QLabel("Connexion à l'hôte...", joinActiveWidget);
    lblJoinStatus->setStyleSheet("font-weight: bold;");

    lblJoinRoster = new QLabel(joinActiveWidget);
    lblJoinRoster->setStyleSheet("background: #1c1c1c; padding: 8px; border-radius: 4px; font-family: monospace;");
    lblJoinRoster->setWordWrap(true);

    btnLeaveJoin = new QPushButton("🛑 Quitter la Partie", joinActiveWidget);
    btnLeaveJoin->setStyleSheet("QPushButton { background-color: #dc3545; color: white; padding: 6px; font-weight: bold; border-radius: 4px; } QPushButton:hover { background-color: #bb2d3b; }");
    connect(btnLeaveJoin, &QPushButton::clicked, this, &DirectP2PDialog::onLeaveClicked);

    layJoinAct->addWidget(lblJoinStatus);
    layJoinAct->addWidget(lblJoinRoster);
    layJoinAct->addWidget(btnLeaveJoin);

    layJoin->addWidget(joinActiveWidget);
    layJoin->addStretch();
    tabWidget->addTab(tabJoin, "🤝 Rejoindre (Join)");

    mainLay->addWidget(tabWidget);

    // Dialog Footer
    QHBoxLayout* footLay = new QHBoxLayout();
    QPushButton* btnOverlay = new QPushButton("📺 Ouvrir l'Overlay OBS", this);
    connect(btnOverlay, &QPushButton::clicked, this, &DirectP2PDialog::onOpenOverlayClicked);

    QPushButton* btnClose = new QPushButton("Fermer la fenêtre", this);
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
    int port = spHostPort->value();
    bool useUPnP = chkUPnP->isChecked();

    // Save name to config
    Config::Table& globalCfg = mainWin->globalCfg;
    globalCfg.SetQString("Online.PlayerName", name);
    Config::Save();

    btnStartHost->setEnabled(false);
    btnStartHost->setText("Démarrage en cours...");

    // Run IP lookup & UPnP in worker thread so UI doesn't freeze
    std::thread([this, name, port, useUPnP]() {
        std::string localIp = DirectP2P::GetLocalIP();
        std::string pubIp = DirectP2P::GetPublicIP(2500);
        std::string upnpMsg;
        bool upnpOk = false;

        if (useUPnP) {
            upnpOk = DirectP2P::UPnPOpenPort((uint16_t)port, localIp, upnpMsg);
        } else {
            upnpMsg = "UPnP désactivé (port configuré manuellement)";
        }

        std::string targetIp = pubIp.empty() ? localIp : pubIp;
        std::string roomCode = DirectP2P::EncodeRoomCode(targetIp, (uint16_t)port);

        QMetaObject::invokeMethod(this, [this, name, port, roomCode, upnpMsg, upnpOk]() {
            btnStartHost->setEnabled(true);
            btnStartHost->setText("🚀 Créer le Salon P2P");

            MpDirectHost(port, name.toStdString().c_str(), roomCode.c_str());

            lblHostRoomCode->setText(QString::fromStdString(roomCode));
            if (upnpOk)
                lblHostUPnPStatus->setText(QString("<font color='#00ff88'>●</font> %1").arg(QString::fromStdString(upnpMsg)));
            else
                lblHostUPnPStatus->setText(QString("<font color='#ffaa00'>▲</font> %1").arg(QString::fromStdString(upnpMsg)));

            updateStatus();
        });
    }).detach();
}

void DirectP2PDialog::onStopHostClicked()
{
    int port = spHostPort->value();
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
        QMessageBox::warning(this, "Direct P2P", "Veuillez entrer un Code de Salon ou une adresse IP.");
        return;
    }

    std::string outIp;
    uint16_t outPort = DirectP2P::DEFAULT_PORT;
    if (!DirectP2P::DecodeRoomCode(codeStr.toStdString(), outIp, outPort)) {
        QMessageBox::warning(this, "Code Invalide",
            "Le code saisi est invalide.\n"
            "Format attendu : SL-XXXXX-XXXXX (10 caractères) ou une adresse IP:port.");
        return;
    }

    Config::Table& globalCfg = mainWin->globalCfg;
    globalCfg.SetQString("Online.PlayerName", name);
    Config::Save();

    MpDirectJoin(outIp.c_str(), outPort, name.toStdString().c_str(), codeStr.toStdString().c_str());
    updateStatus();
}

void DirectP2PDialog::onLeaveClicked()
{
    MpOnlineStop();
    updateStatus();
}

void DirectP2PDialog::onCopyHostCodeClicked()
{
    QString code = lblHostRoomCode->text();
    QApplication::clipboard()->setText(code);
    btnCopyHostCode->setText("✓ Code Copié !");
    QTimer::singleShot(2000, this, [this]() {
        btnCopyHostCode->setText("📋 Copier le Code de Salon");
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

    hostConfigWidget->setVisible(!isHostActive);
    hostActiveWidget->setVisible(isHostActive);

    joinConfigWidget->setVisible(!isJoinActive);
    joinActiveWidget->setVisible(isJoinActive);

    if (isHostActive)
    {
        if (st.code[0]) {
            lblHostRoomCode->setText(QString(st.code));
        }
        lblHostPeersStatus->setText(QString("Joueurs connectés : %1/7").arg(st.peers));

        QString rosterText = "<b>Participants :</b><br>";
        for (int r = 1; r <= 8; r++) {
            if (!st.roster[r][0]) continue;
            QString roleTag = (r == 1) ? " (👑 Hôte)" : "";
            QString pingTag = (st.rosterPing[r] > 0) ? QString(" [%1 ms]").arg(st.rosterPing[r]) : "";
            rosterText += QString("  Rôle %1: %2%3%4<br>").arg(r).arg(st.roster[r]).arg(roleTag).arg(pingTag);
        }
        lblHostRoster->setText(rosterText);
    }
    else if (isJoinActive)
    {
        if (st.peers > 0) {
            lblJoinStatus->setText(QString("<font color='#00ff88'>●</font> Connecté à l'hôte ! (Joueurs : %1)").arg(st.peers + 1));
        } else {
            lblJoinStatus->setText("<font color='#ffaa00'>●</font> Connexion à l'hôte en cours...");
        }

        QString rosterText = "<b>Participants :</b><br>";
        for (int r = 1; r <= 8; r++) {
            if (!st.roster[r][0]) continue;
            QString roleTag = (r == 1) ? " (👑 Hôte)" : (r == st.myRole ? " (Moi)" : "");
            QString pingTag = (st.rosterPing[r] > 0) ? QString(" [%1 ms]").arg(st.rosterPing[r]) : "";
            rosterText += QString("  Rôle %1: %2%3%4<br>").arg(r).arg(st.roster[r]).arg(roleTag).arg(pingTag);
        }
        lblJoinRoster->setText(rosterText);
    }
}
