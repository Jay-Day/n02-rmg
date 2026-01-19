#include "kaillera_server_dialog.h"
#include "kaillera_server_select_dialog.h"
#include "../../core/kaillera/kaillera_core.h"
#include "../../common/stats.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include <QScrollBar>
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>

// Global dialog pointer for callbacks
KailleraServerDialog* g_kailleraServerDialog = nullptr;

static const char* USER_STATUS[] = { "Playing", "Idle", "Playing" };
static const char* GAME_STATUS[] = { "Waiting", "Playing", "Playing" };

KailleraServerDialog::KailleraServerDialog(const QString& serverName, const QString& host,
    int port, const QString& username, int connSetting, QWidget* parent)
    : QDialog(parent)
    , serverName(serverName)
    , host(host)
    , port(port)
    , username(username)
    , connSetting(connSetting)
    , coreInitialized(false)
    , inGame(false)
    , isGameOwner(false)
    , lastFrameCount(0)
    , lastPacketCount(0)
    , lastDelay(-1)
{
    g_kailleraServerDialog = this;
    setupUi();

    if (!initializeConnection()) {
        appendLobbyChat("Failed to initialize connection", Qt::red);
    }
}

KailleraServerDialog::~KailleraServerDialog() {
    cleanup();
    g_kailleraServerDialog = nullptr;
}

void KailleraServerDialog::setupUi() {
    setWindowTitle(QString("Connecting to %1").arg(serverName));
    setMinimumSize(800, 600);

    auto* mainLayout = new QVBoxLayout(this);

    stackedWidget = new QStackedWidget();
    mainLayout->addWidget(stackedWidget);

    setupLobbyPage();
    setupGamePage();

    stackedWidget->setCurrentWidget(lobbyPage);

    // Timer for updates
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &KailleraServerDialog::onTimerTick);
    updateTimer->start(50);  // Poll network every 50ms for responsiveness
}

void KailleraServerDialog::setupLobbyPage() {
    lobbyPage = new QWidget();
    auto* layout = new QVBoxLayout(lobbyPage);

    auto* splitter = new QSplitter(Qt::Horizontal);

    // Left side - Users
    auto* usersGroup = new QGroupBox("Users");
    auto* usersLayout = new QVBoxLayout(usersGroup);
    usersTable = new QTableWidget();
    usersTable->setColumnCount(4);
    usersTable->setHorizontalHeaderLabels({"Name", "Ping", "Conn", "Status"});
    usersTable->horizontalHeader()->setStretchLastSection(true);
    usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    usersTable->setSortingEnabled(true);
    usersLayout->addWidget(usersTable);
    splitter->addWidget(usersGroup);

    // Right side - Games
    auto* gamesGroup = new QGroupBox("Games");
    auto* gamesLayout = new QVBoxLayout(gamesGroup);
    gamesTable = new QTableWidget();
    gamesTable->setColumnCount(5);
    gamesTable->setHorizontalHeaderLabels({"Game", "Emulator", "Owner", "Status", "Users"});
    gamesTable->horizontalHeader()->setStretchLastSection(true);
    gamesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    gamesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    gamesTable->setContextMenuPolicy(Qt::CustomContextMenu);
    gamesTable->setSortingEnabled(true);
    connect(gamesTable, &QTableWidget::customContextMenuRequested,
            this, &KailleraServerDialog::onGameTableContextMenu);
    connect(gamesTable, &QTableWidget::cellDoubleClicked,
            this, &KailleraServerDialog::onGameTableDoubleClicked);
    gamesLayout->addWidget(gamesTable);

    auto* gameButtonLayout = new QHBoxLayout();
    auto* createButton = new QPushButton("Create Game");
    connect(createButton, &QPushButton::clicked, this, &KailleraServerDialog::onCreateGame);
    gameButtonLayout->addWidget(createButton);

    auto* joinButton = new QPushButton("Join Game");
    connect(joinButton, &QPushButton::clicked, this, &KailleraServerDialog::onJoinGame);
    gameButtonLayout->addWidget(joinButton);
    gameButtonLayout->addStretch();
    gamesLayout->addLayout(gameButtonLayout);

    splitter->addWidget(gamesGroup);
    splitter->setSizes({300, 500});

    layout->addWidget(splitter, 1);

    // Chat area
    auto* chatGroup = new QGroupBox("Chat");
    auto* chatLayout = new QVBoxLayout(chatGroup);

    lobbyChat = new QTextEdit();
    lobbyChat->setReadOnly(true);
    lobbyChat->setMaximumHeight(150);
    chatLayout->addWidget(lobbyChat);

    auto* inputLayout = new QHBoxLayout();
    lobbyChatInput = new QLineEdit();
    lobbyChatInput->setPlaceholderText("Type message...");
    connect(lobbyChatInput, &QLineEdit::returnPressed, this, &KailleraServerDialog::onSendLobbyChat);
    inputLayout->addWidget(lobbyChatInput);

    auto* sendButton = new QPushButton("Send");
    connect(sendButton, &QPushButton::clicked, this, &KailleraServerDialog::onSendLobbyChat);
    inputLayout->addWidget(sendButton);
    chatLayout->addLayout(inputLayout);

    layout->addWidget(chatGroup);

    stackedWidget->addWidget(lobbyPage);
}

void KailleraServerDialog::setupGamePage() {
    gamePage = new QWidget();
    auto* layout = new QVBoxLayout(gamePage);

    auto* topLayout = new QHBoxLayout();

    // Players list
    auto* playersGroup = new QGroupBox("Players");
    auto* playersLayout = new QVBoxLayout(playersGroup);
    playersTable = new QTableWidget();
    playersTable->setColumnCount(4);
    playersTable->setHorizontalHeaderLabels({"Name", "Ping", "Conn", "Delay"});
    playersTable->horizontalHeader()->setStretchLastSection(true);
    playersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    playersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    playersTable->setSortingEnabled(true);
    playersTable->setMaximumWidth(350);
    playersLayout->addWidget(playersTable);
    topLayout->addWidget(playersGroup);

    // Game chat
    auto* gameChatGroup = new QGroupBox("Game Chat");
    auto* gameChatLayout = new QVBoxLayout(gameChatGroup);

    gameChat = new QTextEdit();
    gameChat->setReadOnly(true);
    gameChatLayout->addWidget(gameChat);

    auto* gameInputLayout = new QHBoxLayout();
    gameChatInput = new QLineEdit();
    gameChatInput->setPlaceholderText("Type message...");
    connect(gameChatInput, &QLineEdit::returnPressed, this, &KailleraServerDialog::onSendGameChat);
    gameInputLayout->addWidget(gameChatInput);

    auto* gameSendButton = new QPushButton("Send");
    connect(gameSendButton, &QPushButton::clicked, this, &KailleraServerDialog::onSendGameChat);
    gameInputLayout->addWidget(gameSendButton);
    gameChatLayout->addLayout(gameInputLayout);

    topLayout->addWidget(gameChatGroup, 1);
    layout->addLayout(topLayout, 1);

    // Controls
    auto* controlLayout = new QHBoxLayout();

    recordCheck = new QCheckBox("Record");
    recordCheck->setChecked(true);
    controlLayout->addWidget(recordCheck);

    controlLayout->addStretch();

    statusLabel = new QLabel("--");
    controlLayout->addWidget(statusLabel);

    delayLabel = new QLabel("--");
    controlLayout->addWidget(delayLabel);

    layout->addLayout(controlLayout);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    startButton = new QPushButton("Start Game");
    connect(startButton, &QPushButton::clicked, this, &KailleraServerDialog::onStartGame);
    buttonLayout->addWidget(startButton);

    dropButton = new QPushButton("Drop");
    connect(dropButton, &QPushButton::clicked, this, &KailleraServerDialog::onDropGame);
    buttonLayout->addWidget(dropButton);

    kickButton = new QPushButton("Kick");
    connect(kickButton, &QPushButton::clicked, this, &KailleraServerDialog::onKickPlayer);
    buttonLayout->addWidget(kickButton);

    buttonLayout->addStretch();

    leaveButton = new QPushButton("Leave Game");
    connect(leaveButton, &QPushButton::clicked, this, &KailleraServerDialog::onLeaveGame);
    buttonLayout->addWidget(leaveButton);

    layout->addLayout(buttonLayout);

    stackedWidget->addWidget(gamePage);
}

bool KailleraServerDialog::initializeConnection() {
    // Must provide an app/emulator name - servers require this
    if (!kaillera_core_initialize(port, "N02 Client", username.toUtf8().constData(), connSetting)) {
        return false;
    }

    coreInitialized = true;

    if (!kaillera_core_connect(host.toUtf8().constData(), port)) {
        appendLobbyChat("Error connecting to server", Qt::red);
        return false;
    }

    return true;
}

void KailleraServerDialog::cleanup() {
    if (coreInitialized) {
        kaillera_disconnect("Goodbye");
        kaillera_core_cleanup();
        coreInitialized = false;
    }
    updateTimer->stop();
}

void KailleraServerDialog::closeEvent(QCloseEvent* event) {
    if (inGame) {
        kaillera_end_game();
    }
    cleanup();
    event->accept();
}

void KailleraServerDialog::showLobbyPage() {
    stackedWidget->setCurrentWidget(lobbyPage);
    inGame = false;
}

void KailleraServerDialog::showGamePage(bool isOwner) {
    isGameOwner = isOwner;
    stackedWidget->setCurrentWidget(gamePage);
    playersTable->setRowCount(0);
    gameChat->clear();
    inGame = true;

    startButton->setEnabled(isOwner);
    kickButton->setEnabled(isOwner);
}

// Callback implementations
void KailleraServerDialog::onUserAdd(const QString& name, int ping, int status,
                                      unsigned short id, int conn) {
    int row = usersTable->rowCount();
    usersTable->insertRow(row);
    usersTable->setItem(row, 0, new QTableWidgetItem(name));

    // Use setData for proper numeric sorting
    auto* pingItem = new QTableWidgetItem();
    pingItem->setData(Qt::DisplayRole, ping);
    usersTable->setItem(row, 1, pingItem);

    usersTable->setItem(row, 2, new QTableWidgetItem(CONNECTION_TYPES[conn]));
    usersTable->setItem(row, 3, new QTableWidgetItem(USER_STATUS[status]));
    usersTable->item(row, 0)->setData(Qt::UserRole, id);
}

void KailleraServerDialog::onUserJoin(const QString& name, int ping, unsigned short id, int conn) {
    onUserAdd(name, ping, 1, id, conn);
    appendLobbyChat(QString("* %1 joined (Ping: %2)").arg(name).arg(ping), Qt::gray);
}

void KailleraServerDialog::onUserLeave(const QString& name, unsigned short id) {
    for (int row = 0; row < usersTable->rowCount(); row++) {
        if (usersTable->item(row, 0)->data(Qt::UserRole).toUInt() == id) {
            usersTable->removeRow(row);
            break;
        }
    }
    appendLobbyChat(QString("* %1 left").arg(name), Qt::gray);
}

void KailleraServerDialog::onGameAdd(const QString& name, unsigned int id,
                                      const QString& emulator, const QString& owner,
                                      const QString& users, int status) {
    int row = gamesTable->rowCount();
    gamesTable->insertRow(row);
    gamesTable->setItem(row, 0, new QTableWidgetItem(name));
    gamesTable->setItem(row, 1, new QTableWidgetItem(emulator));
    gamesTable->setItem(row, 2, new QTableWidgetItem(owner));
    gamesTable->setItem(row, 3, new QTableWidgetItem(GAME_STATUS[status]));
    gamesTable->setItem(row, 4, new QTableWidgetItem(users));
    gamesTable->item(row, 0)->setData(Qt::UserRole, id);
}

void KailleraServerDialog::onGameCreate(const QString& name, unsigned int id,
                                         const QString& emulator, const QString& owner) {
    onGameAdd(name, id, emulator, owner, "1/2", 0);
}

void KailleraServerDialog::onGameClose(unsigned int id) {
    for (int row = 0; row < gamesTable->rowCount(); row++) {
        if (gamesTable->item(row, 0)->data(Qt::UserRole).toUInt() == id) {
            gamesTable->removeRow(row);
            break;
        }
    }
}

void KailleraServerDialog::onGameStatusChange(unsigned int id, int status, int players, int maxPlayers) {
    for (int row = 0; row < gamesTable->rowCount(); row++) {
        if (gamesTable->item(row, 0)->data(Qt::UserRole).toUInt() == id) {
            gamesTable->item(row, 3)->setText(GAME_STATUS[status]);
            gamesTable->item(row, 4)->setText(QString("%1/%2").arg(players).arg(maxPlayers));
            break;
        }
    }
}

void KailleraServerDialog::onChat(const QString& nick, const QString& msg) {
    appendLobbyChat(QString("<%1> %2").arg(nick, msg));
}

void KailleraServerDialog::onMotd(const QString& msg) {
    appendLobbyChat(QString("- %1").arg(msg), QColor(0x33, 0x66, 0x33));
}

void KailleraServerDialog::onGameChat(const QString& nick, const QString& msg) {
    appendGameChat(QString("<%1> %2").arg(nick, msg));
}

void KailleraServerDialog::onPlayerAdd(const QString& name, int ping, unsigned short id, int conn) {
    int row = playersTable->rowCount();
    playersTable->insertRow(row);
    playersTable->setItem(row, 0, new QTableWidgetItem(name));

    // Use setData for proper numeric sorting
    auto* pingItem = new QTableWidgetItem();
    pingItem->setData(Qt::DisplayRole, ping);
    playersTable->setItem(row, 1, pingItem);

    playersTable->setItem(row, 2, new QTableWidgetItem(CONNECTION_TYPES[conn]));

    int delay = (ping * 60 / 1000 / conn) + 2;
    auto* delayItem = new QTableWidgetItem();
    int delayFrames = delay * conn - 1;
    delayItem->setData(Qt::DisplayRole, delayFrames);
    delayItem->setText(QString("%1 frames").arg(delayFrames));
    playersTable->setItem(row, 3, delayItem);

    playersTable->item(row, 0)->setData(Qt::UserRole, id);
}

void KailleraServerDialog::onPlayerJoin(const QString& name, int ping, unsigned short id, int conn) {
    appendGameChat(QString("* %1 joined").arg(name), Qt::gray);
    onPlayerAdd(name, ping, id, conn);
    QApplication::beep();
}

void KailleraServerDialog::onPlayerLeft(const QString& name, unsigned short id) {
    appendGameChat(QString("* %1 left").arg(name), Qt::gray);
    for (int row = 0; row < playersTable->rowCount(); row++) {
        if (playersTable->item(row, 0)->data(Qt::UserRole).toUInt() == id) {
            playersTable->removeRow(row);
            break;
        }
    }
}

void KailleraServerDialog::onPlayerDropped(const QString& name, int playerNo) {
    appendGameChat(QString("* %1 dropped (Player %2)").arg(name).arg(playerNo), Qt::red);
}

void KailleraServerDialog::onGameStarted(const QString& game, int playerNo, int numPlayers) {
    appendGameChat(QString("* Starting: %1 (%2/%3)").arg(game).arg(playerNo).arg(numPlayers));
    appendGameChat("- Press \"Drop\" if emulator fails", Qt::gray);
    if (recordCheck->isChecked()) {
        appendGameChat("- Your game will be recorded", Qt::gray);
    }
}

void KailleraServerDialog::onGameEnded() {
    appendGameChat("* Game ended", Qt::gray);
}

void KailleraServerDialog::onKicked() {
    appendLobbyChat("* You have been kicked from the game", Qt::red);
    showLobbyPage();
}

void KailleraServerDialog::onLoginStat(const QString& msg) {
    appendLobbyChat(QString("* %1").arg(msg), Qt::blue);
}

void KailleraServerDialog::onError(const QString& msg) {
    appendLobbyChat(msg, Qt::red);
}

void KailleraServerDialog::appendLobbyChat(const QString& text, const QColor& color) {
    QString html = QString("<span style='color: %1;'>%2</span><br>")
        .arg(color.name(), text.toHtmlEscaped());
    lobbyChat->insertHtml(html);
    lobbyChat->verticalScrollBar()->setValue(lobbyChat->verticalScrollBar()->maximum());
}

void KailleraServerDialog::appendGameChat(const QString& text, const QColor& color) {
    QString html = QString("<span style='color: %1;'>%2</span><br>")
        .arg(color.name(), text.toHtmlEscaped());
    gameChat->insertHtml(html);
    gameChat->verticalScrollBar()->setValue(gameChat->verticalScrollBar()->maximum());
}

void KailleraServerDialog::onSendLobbyChat() {
    QString text = lobbyChatInput->text().trimmed();
    if (text.isEmpty()) return;
    lobbyChatInput->clear();
    kaillera_chat_send(text.toUtf8().constData());
}

void KailleraServerDialog::onSendGameChat() {
    QString text = gameChatInput->text().trimmed();
    if (text.isEmpty()) return;
    gameChatInput->clear();
    kaillera_game_chat_send(text.toUtf8().constData());
}

void KailleraServerDialog::onCreateGame() {
    // TODO: Show game selection menu
    QMessageBox::information(this, "Create Game",
        "Game creation requires a game list from the emulator.");
}

void KailleraServerDialog::onJoinGame() {
    int row = gamesTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "Error", "Please select a game to join.");
        return;
    }

    QString status = gamesTable->item(row, 3)->text();
    if (status != "Waiting") {
        QMessageBox::warning(this, "Error", "Cannot join a game that is already playing.");
        return;
    }

    unsigned int id = gamesTable->item(row, 0)->data(Qt::UserRole).toUInt();
    kaillera_join_game(id);
}

void KailleraServerDialog::onLeaveGame() {
    kaillera_leave_game();
    showLobbyPage();
}

void KailleraServerDialog::onStartGame() {
    kaillera_start_game();
}

void KailleraServerDialog::onDropGame() {
    kaillera_game_drop();
}

void KailleraServerDialog::onKickPlayer() {
    int row = playersTable->currentRow();
    if (row <= 0) return;  // Can't kick yourself (row 0)

    unsigned short id = playersTable->item(row, 0)->data(Qt::UserRole).toUInt();
    kaillera_kick_user(id);
}

void KailleraServerDialog::onTimerTick() {
    if (!coreInitialized) return;

    kaillera_step();

    if (kaillera_is_connected()) {
        setWindowTitle(QString("Connected to %1 (%2 users, %3 games)")
            .arg(serverName)
            .arg(usersTable->rowCount())
            .arg(gamesTable->rowCount()));
    }

    if (inGame) {
        int frames = kaillera_get_frames_count();
        if (frames != lastFrameCount) {
            int fps = frames - lastFrameCount;
            int pps = SOCK_SEND_PACKETS - lastPacketCount;
            statusLabel->setText(QString("%1 fps / %2 pps").arg(fps).arg(pps));
            lastFrameCount = frames;
            lastPacketCount = SOCK_SEND_PACKETS;
        }

        int delay = kaillera_get_delay();
        if (delay != lastDelay) {
            delayLabel->setText(QString("%1 frames").arg(delay));
            lastDelay = delay;
        }
    }
}

void KailleraServerDialog::onGameTableContextMenu(const QPoint& pos) {
    Q_UNUSED(pos);
    // TODO: Show context menu with Create/Join options
}

void KailleraServerDialog::onGameTableDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row >= 0) {
        onJoinGame();
    }
}
