#pragma once

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTimer>

class P2PConnectionDialog : public QDialog {
    Q_OBJECT

public:
    P2PConnectionDialog(bool isHost, const QString& game, const QString& ip,
                        const QString& username, int port, QWidget* parent = nullptr);
    ~P2PConnectionDialog() override;

    // Called from callbacks
    void appendChat(const QString& nick, const QString& message);
    void appendSystemMessage(const QString& message);
    void updatePing(int ping, int packetLoss);
    void updateFps(int fps, int pps);
    void onPeerJoined(const QString& peerName, const QString& peerApp);
    void onPeerLeft();
    void onGameStarted(const QString& game, int playerNo, int numPlayers);
    void onGameEnded();

signals:
    void chatReceived(const QString& nick, const QString& message);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onSendChat();
    void onReadyToggled(bool checked);
    void onDropGame();
    void onPingClicked();
    void onTimerTick();
    void onSmoothingChanged(int index);

private:
    void setupUi();
    bool initializeConnection();
    void cleanup();

    // Connection info
    bool isHost;
    QString game;
    QString ip;
    QString username;
    int port;
    bool coreInitialized;

    // UI elements
    QTextEdit* chatDisplay;
    QLineEdit* chatInput;
    QCheckBox* readyCheck;
    QCheckBox* recordCheck;
    QPushButton* dropButton;
    QPushButton* pingButton;
    QPushButton* statsButton;
    QComboBox* smoothingCombo;
    QLabel* statusLabel;
    QLabel* gameLabel;
    QTimer* updateTimer;

    // Host-only UI
    QCheckBox* enlistCheck;

    // Stats tracking
    int lastFrameCount;
    int lastPacketCount;
};

// Global dialog pointer for callbacks
extern P2PConnectionDialog* g_p2pConnectionDialog;
