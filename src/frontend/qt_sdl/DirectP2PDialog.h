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

class MainWindow;

class DirectP2PDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DirectP2PDialog(MainWindow* parent);
    ~DirectP2PDialog();

    static DirectP2PDialog* openDlg(MainWindow* parent);

private slots:
    void onStartHostClicked();
    void onStopHostClicked();
    void onJoinClicked();
    void onLeaveClicked();
    void onCopyHostCodeClicked();
    void onCopyHostLanCodeClicked();
    void onOpenOverlayClicked();
    void onUpdateTimer();

private:
    MainWindow* mainWin;
    QTimer* pollTimer;

    QTabWidget* tabWidget;

    // Host Tab Widgets
    QWidget* hostConfigWidget;
    QWidget* hostActiveWidget;
    QLineEdit* edHostName;
    QCheckBox* chkUPnP;
    QPushButton* btnStartHost;
    QPushButton* btnStopHost;
    QLabel* lblHostRoomCode;
    QPushButton* btnCopyHostCode;
    QLabel* lblHostLanCode;
    QPushButton* btnCopyHostLanCode;
    QLabel* lblHostUPnPStatus;
    QLabel* lblHostPeersStatus;
    QLabel* lblHostRoster;

    // Join Tab Widgets
    QWidget* joinConfigWidget;
    QWidget* joinActiveWidget;
    QLineEdit* edJoinName;
    QLineEdit* edJoinCode;
    QPushButton* btnJoin;
    QPushButton* btnTestLocalJoin;
    QPushButton* btnLeaveJoin;
    QLabel* lblJoinStatus;
    QLabel* lblJoinRoster;
    QLabel* lblJoinError;
    int joinElapsedSec = 0;
    bool joinPending = false;

    void setupUI();
    void updateStatus();
};

#endif // DIRECT_P2P_DIALOG_H
