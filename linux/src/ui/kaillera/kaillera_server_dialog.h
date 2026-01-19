#pragma once

#include <QDialog>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QMenu>

class KailleraServerDialog : public QDialog {
    Q_OBJECT

public:
    KailleraServerDialog(const QString& serverName, const QString& host, int port,
                         const QString& username, int connSetting, QWidget* parent = nullptr);
    ~KailleraServerDialog() override;

    // Callbacks from kaillera_core
    void onUserAdd(const QString& name, int ping, int status, unsigned short id, int conn);
    void onUserJoin(const QString& name, int ping, unsigned short id, int conn);
    void onUserLeave(const QString& name, unsigned short id);
    void onGameAdd(const QString& name, unsigned int id, const QString& emulator,
                   const QString& owner, const QString& users, int status);
    void onGameCreate(const QString& name, unsigned int id, const QString& emulator, const QString& owner);
    void onGameClose(unsigned int id);
    void onGameStatusChange(unsigned int id, int status, int players, int maxPlayers);
    void onChat(const QString& nick, const QString& msg);
    void onMotd(const QString& msg);
    void onGameChat(const QString& nick, const QString& msg);
    void onPlayerAdd(const QString& name, int ping, unsigned short id, int conn);
    void onPlayerJoin(const QString& name, int ping, unsigned short id, int conn);
    void onPlayerLeft(const QString& name, unsigned short id);
    void onPlayerDropped(const QString& name, int playerNo);
    void onGameStarted(const QString& game, int playerNo, int numPlayers);
    void onGameEnded();
    void onKicked();
    void onLoginStat(const QString& msg);
    void onError(const QString& msg);

    void appendLobbyChat(const QString& text, const QColor& color = Qt::black);
    void appendGameChat(const QString& text, const QColor& color = Qt::black);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onSendLobbyChat();
    void onSendGameChat();
    void onCreateGame();
    void onJoinGame();
    void onLeaveGame();
    void onStartGame();
    void onDropGame();
    void onKickPlayer();
    void onTimerTick();
    void onGameTableContextMenu(const QPoint& pos);
    void onGameTableDoubleClicked(int row, int column);

private:
    void setupUi();
    void setupLobbyPage();
    void setupGamePage();
    bool initializeConnection();
    void cleanup();
    void showLobbyPage();
    void showGamePage(bool isOwner);

    // Connection info
    QString serverName;
    QString host;
    int port;
    QString username;
    int connSetting;
    bool coreInitialized;
    bool inGame;
    bool isGameOwner;

    // UI - main
    QStackedWidget* stackedWidget;
    QTimer* updateTimer;

    // UI - lobby page
    QWidget* lobbyPage;
    QTableWidget* usersTable;
    QTableWidget* gamesTable;
    QTextEdit* lobbyChat;
    QLineEdit* lobbyChatInput;

    // UI - game page
    QWidget* gamePage;
    QTableWidget* playersTable;
    QTextEdit* gameChat;
    QLineEdit* gameChatInput;
    QPushButton* startButton;
    QPushButton* dropButton;
    QPushButton* leaveButton;
    QPushButton* kickButton;
    QCheckBox* recordCheck;
    QLabel* statusLabel;
    QLabel* delayLabel;

    // Games menu
    QMenu* createGameMenu;

    // Stats
    int lastFrameCount;
    int lastPacketCount;
    int lastDelay;
};

// Global dialog pointer for callbacks
extern KailleraServerDialog* g_kailleraServerDialog;
