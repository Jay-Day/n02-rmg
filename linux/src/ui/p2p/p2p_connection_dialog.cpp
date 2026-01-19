#include "p2p_connection_dialog.h"
#include "../../core/p2p/p2p_core.h"
#include "../../common/stats.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QCloseEvent>
#include <QScrollBar>
#include <QApplication>

// Global dialog pointer for callbacks
P2PConnectionDialog* g_p2pConnectionDialog = nullptr;

// Smoothing option storage
static int p2p_option_smoothing = 0;

P2PConnectionDialog::P2PConnectionDialog(bool isHost, const QString& game,
    const QString& ip, const QString& username, int port, QWidget* parent)
    : QDialog(parent)
    , isHost(isHost)
    , game(game)
    , ip(ip)
    , username(username)
    , port(port)
    , coreInitialized(false)
    , lastFrameCount(0)
    , lastPacketCount(0)
{
    g_p2pConnectionDialog = this;
    setupUi();

    if (!initializeConnection()) {
        QMessageBox::critical(this, "Error", "Failed to initialize connection.");
    }
}

P2PConnectionDialog::~P2PConnectionDialog() {
    cleanup();
    g_p2pConnectionDialog = nullptr;
}

void P2PConnectionDialog::setupUi() {
    setWindowTitle(isHost ? "Hosting Game" : "Connecting...");
    setMinimumSize(500, 400);

    auto* mainLayout = new QVBoxLayout(this);

    // Game info
    auto* infoLayout = new QHBoxLayout();
    gameLabel = new QLabel("Game: " + game);
    infoLayout->addWidget(gameLabel);
    infoLayout->addStretch();
    statusLabel = new QLabel("Ping: -- ms");
    infoLayout->addWidget(statusLabel);
    mainLayout->addLayout(infoLayout);

    // Chat display
    chatDisplay = new QTextEdit();
    chatDisplay->setReadOnly(true);
    chatDisplay->setMinimumHeight(200);
    mainLayout->addWidget(chatDisplay, 1);

    // Chat input
    auto* chatLayout = new QHBoxLayout();
    chatInput = new QLineEdit();
    chatInput->setPlaceholderText("Type message and press Enter...");
    connect(chatInput, &QLineEdit::returnPressed, this, &P2PConnectionDialog::onSendChat);
    chatLayout->addWidget(chatInput);

    auto* sendButton = new QPushButton("Send");
    connect(sendButton, &QPushButton::clicked, this, &P2PConnectionDialog::onSendChat);
    chatLayout->addWidget(sendButton);
    mainLayout->addLayout(chatLayout);

    // Controls
    auto* controlsLayout = new QHBoxLayout();

    readyCheck = new QCheckBox("Ready");
    connect(readyCheck, &QCheckBox::toggled, this, &P2PConnectionDialog::onReadyToggled);
    controlsLayout->addWidget(readyCheck);

    recordCheck = new QCheckBox("Record");
    recordCheck->setChecked(true);
    controlsLayout->addWidget(recordCheck);

    controlsLayout->addStretch();

    // Host-only controls
    if (isHost) {
        auto* smoothingLabel = new QLabel("Smoothing:");
        controlsLayout->addWidget(smoothingLabel);

        smoothingCombo = new QComboBox();
        smoothingCombo->addItem("None");
        smoothingCombo->addItem("If near");
        smoothingCombo->addItem("Always");
        smoothingCombo->addItem("Extra");
        connect(smoothingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &P2PConnectionDialog::onSmoothingChanged);
        controlsLayout->addWidget(smoothingCombo);

        enlistCheck = new QCheckBox("Enlist");
        enlistCheck->setToolTip("List game on server browser");
        controlsLayout->addWidget(enlistCheck);
    }

    mainLayout->addLayout(controlsLayout);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    pingButton = new QPushButton("Ping");
    connect(pingButton, &QPushButton::clicked, this, &P2PConnectionDialog::onPingClicked);
    buttonLayout->addWidget(pingButton);

    statsButton = new QPushButton("Stats");
    connect(statsButton, &QPushButton::clicked, []() {
        // TODO: Show stats dialog
    });
    buttonLayout->addWidget(statsButton);

    buttonLayout->addStretch();

    dropButton = new QPushButton("Drop Game");
    connect(dropButton, &QPushButton::clicked, this, &P2PConnectionDialog::onDropGame);
    buttonLayout->addWidget(dropButton);

    auto* leaveButton = new QPushButton("Leave");
    connect(leaveButton, &QPushButton::clicked, this, &QDialog::close);
    buttonLayout->addWidget(leaveButton);

    mainLayout->addLayout(buttonLayout);

    // Update timer
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &P2PConnectionDialog::onTimerTick);
    updateTimer->start(1000);
}

bool P2PConnectionDialog::initializeConnection() {
    QByteArray appBytes = QByteArray(128, '\0');  // Will be filled by emulator
    QByteArray gameBytes = game.toUtf8();
    QByteArray userBytes = username.toUtf8();

    if (!p2p_core_initialize(isHost, port, appBytes.constData(),
                             gameBytes.constData(), userBytes.constData())) {
        appendSystemMessage("Error initializing sockets");
        return false;
    }

    coreInitialized = true;

    if (isHost) {
        appendSystemMessage(QString("Hosting %1 on port %2").arg(game).arg(p2p_core_get_port()));
        appendSystemMessage("WARNING: Hosting requires ports to be forwarded and enabled in firewalls.");
    } else {
        // Parse IP:port
        QString host = ip;
        int targetPort = 27886;

        int colonIdx = ip.indexOf(':');
        if (colonIdx >= 0) {
            host = ip.left(colonIdx);
            targetPort = ip.mid(colonIdx + 1).toInt();
            if (targetPort == 0) targetPort = 27886;
        }

        appendSystemMessage(QString("Connecting to %1:%2").arg(host).arg(targetPort));

        if (!p2p_core_connect(host.toUtf8().constData(), targetPort)) {
            appendSystemMessage("Error connecting to specified host/port");
            return false;
        }
    }

    return true;
}

void P2PConnectionDialog::cleanup() {
    if (coreInitialized) {
        p2p_disconnect();
        p2p_core_cleanup();
        coreInitialized = false;
    }
    updateTimer->stop();
}

void P2PConnectionDialog::closeEvent(QCloseEvent* event) {
    cleanup();
    event->accept();
}

void P2PConnectionDialog::appendChat(const QString& nick, const QString& message) {
    QString html = QString("<b>&lt;%1&gt;</b> %2<br>")
        .arg(nick.toHtmlEscaped(), message.toHtmlEscaped());
    chatDisplay->insertHtml(html);
    chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
}

void P2PConnectionDialog::appendSystemMessage(const QString& message) {
    QString html = QString("<span style='color: gray;'>%1</span><br>")
        .arg(message.toHtmlEscaped());
    chatDisplay->insertHtml(html);
    chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
}

void P2PConnectionDialog::updatePing(int ping, int packetLoss) {
    statusLabel->setText(QString("Ping: %1 ms  PL: %2").arg(ping).arg(packetLoss));
}

void P2PConnectionDialog::updateFps(int fps, int pps) {
    statusLabel->setText(QString("%1 fps / %2 pps").arg(fps).arg(pps));
}

void P2PConnectionDialog::onPeerJoined(const QString& peerName, const QString& peerApp) {
    setWindowTitle(QString("Connected to %1 (%2)").arg(peerName, peerApp));
    appendSystemMessage(QString("Peer joined: %1").arg(peerName));
    QApplication::beep();
}

void P2PConnectionDialog::onPeerLeft() {
    setWindowTitle(isHost ? "Hosting Game" : "Disconnected");
    appendSystemMessage("Peer left");
    QApplication::beep();
}

void P2PConnectionDialog::onGameStarted(const QString& gameName, int playerNo, int numPlayers) {
    gameLabel->setText(QString("Game: %1 (Player %2 of %3)").arg(gameName).arg(playerNo).arg(numPlayers));
    appendSystemMessage(QString("Game started: Player %1 of %2").arg(playerNo).arg(numPlayers));
}

void P2PConnectionDialog::onGameEnded() {
    appendSystemMessage("Game ended");
    readyCheck->setChecked(false);
}

void P2PConnectionDialog::onSendChat() {
    QString text = chatInput->text().trimmed();
    if (text.isEmpty()) return;

    chatInput->clear();

    // Handle commands
    if (text.startsWith("/")) {
        QString cmd = text.mid(1);
        if (cmd == "pcs") {
            p2p_print_core_status();
            return;
        } else if (cmd == "ping") {
            p2p_ping();
            return;
        } else if (cmd == "retr") {
            p2p_retransmit();
            return;
        }
    }

    if (p2p_is_connected()) {
        p2p_send_chat(text.toUtf8().constData());
    }
}

void P2PConnectionDialog::onReadyToggled(bool checked) {
    p2p_set_ready(checked);
}

void P2PConnectionDialog::onDropGame() {
    p2p_drop_game();
}

void P2PConnectionDialog::onPingClicked() {
    p2p_ping();
}

void P2PConnectionDialog::onTimerTick() {
    if (!coreInitialized) return;

    // Step the P2P core
    p2p_step();

    if (p2p_is_connected()) {
        // Update display periodically
        int currentFrames = p2p_get_frames_count();
        if (currentFrames != lastFrameCount) {
            int fps = currentFrames - lastFrameCount;
            int pps = SOCK_SEND_PACKETS - lastPacketCount;
            updateFps(fps, pps);
            lastFrameCount = currentFrames;
            lastPacketCount = SOCK_SEND_PACKETS;
        }
    }
}

void P2PConnectionDialog::onSmoothingChanged(int index) {
    p2p_option_smoothing = index;
}

// Callback implementations for p2p_core
// These are declared in main.cpp but we can update them to use the dialog

int p2p_getSelectedDelay() {
    return p2p_option_smoothing;
}
