#ifndef DIRECT_P2P_DIALOG_H
#define DIRECT_P2P_DIALOG_H

#include <QDialog>
#include <QTimer>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>

class MainWindow;

class DirectP2PDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DirectP2PDialog(MainWindow* parent);
    ~DirectP2PDialog();

    static DirectP2PDialog* openDlg(MainWindow* parent);
    void setLanguage(const QString& lang);

private slots:
    void onStartHostClicked();
    void onStopHostClicked();
    void onJoinClicked();
    void onLeaveClicked();
    void onCopyHostCodeClicked();
    void onCopyHostLanCodeClicked();
    void onOpenOverlayClicked();
    void onUpdateTimer();
    void onLanguageIndexChanged(int index);

private:
    MainWindow* mainWin;
    QTimer* pollTimer;

    QString currentLang;

    // Header Widgets
    QLabel* descLbl;
    QLabel* lblLang;
    QComboBox* cmbLanguage;

    QTabWidget* tabWidget;

    // Host Tab Widgets
    QWidget* hostConfigWidget;
    QWidget* hostActiveWidget;
    QLabel* lblHostName;
    QLineEdit* edHostName;
    QCheckBox* chkUPnP;
    QPushButton* btnStartHost;
    QPushButton* btnStopHost;
    QGroupBox* codeBox;
    QLabel* lblDescCode;
    QLabel* lblHostRoomCode;
    QPushButton* btnCopyHostCode;
    QLabel* lblDescLan;
    QLabel* lblHostLanCode;
    QPushButton* btnCopyHostLanCode;
    QLabel* lblHostUPnPStatus;
    QLabel* lblHostPeersStatus;
    QLabel* lblHostRoster;

    // Join Tab Widgets
    QWidget* joinConfigWidget;
    QWidget* joinActiveWidget;
    QLabel* lblJoinName;
    QLineEdit* edJoinName;
    QLabel* lblJoinCode;
    QLineEdit* edJoinCode;
    QPushButton* btnJoin;
    QPushButton* btnTestLocalJoin;
    QPushButton* btnLeaveJoin;
    QLabel* lblJoinStatus;
    QLabel* lblJoinRoster;
    QLabel* lblJoinError;
    int joinElapsedSec = 0;
    bool joinPending = false;

    // Footer Widgets
    QPushButton* btnOverlay;
    QPushButton* btnClose;

    void setupUI();
    void updateStatus();
    void retranslateUI();
};

#endif // DIRECT_P2P_DIALOG_H
