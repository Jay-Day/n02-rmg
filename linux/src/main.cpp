#include <QApplication>
#include <cstdio>
#include <cstdarg>
#include "common/settings.h"
#include "common/k_socket.h"
#include "common/timer.h"
#include "ui/mainwindow.h"
#include "ui/p2p/p2p_connection_dialog.h"
#include "ui/kaillera/kaillera_server_dialog.h"
#include "ui/kaillera/kaillera_server_select_dialog.h"

// ============================================================================
// P2P callbacks - forward to dialog when available
// ============================================================================

void p2p_chat_callback(char* nick, char* msg) {
    printf("[P2P Chat] %s: %s\n", nick, msg);
    if (g_p2pConnectionDialog) {
        QMetaObject::invokeMethod(g_p2pConnectionDialog, [=]() {
            g_p2pConnectionDialog->appendChat(QString::fromUtf8(nick), QString::fromUtf8(msg));
        }, Qt::QueuedConnection);
    }
}

void p2p_game_callback(char* game, int playerno, int maxplayers) {
    printf("[P2P] Game started: %s (player %d of %d)\n", game, playerno, maxplayers);
    if (g_p2pConnectionDialog) {
        QMetaObject::invokeMethod(g_p2pConnectionDialog, [=]() {
            g_p2pConnectionDialog->onGameStarted(QString::fromUtf8(game), playerno, maxplayers);
        }, Qt::QueuedConnection);
    }
}

void p2p_end_game_callback() {
    printf("[P2P] Game ended\n");
    if (g_p2pConnectionDialog) {
        QMetaObject::invokeMethod(g_p2pConnectionDialog, [=]() {
            g_p2pConnectionDialog->onGameEnded();
        }, Qt::QueuedConnection);
    }
}

void p2p_client_dropped_callback(char* nick, int playerno) {
    printf("[P2P] Client dropped: %s (player %d)\n", nick, playerno);
}

void p2p_core_debug(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    printf("[P2P] %s\n", buffer);
    if (g_p2pConnectionDialog) {
        QString msg = QString::fromUtf8(buffer);
        QMetaObject::invokeMethod(g_p2pConnectionDialog, [=]() {
            g_p2pConnectionDialog->appendSystemMessage(msg);
        }, Qt::QueuedConnection);
    }
}

void p2p_hosted_game_callback(char* game) {
    printf("[P2P] Hosted game: %s\n", game);
}

void p2p_ping_callback(int ping) {
    printf("[P2P] Ping: %d ms\n", ping);
    if (g_p2pConnectionDialog) {
        QMetaObject::invokeMethod(g_p2pConnectionDialog, [=]() {
            g_p2pConnectionDialog->updatePing(ping, 0);
        }, Qt::QueuedConnection);
    }
}

void p2p_ssrv_packet_recv_callback(char*, int, void*) {
    // Stub - server packet received
}

void p2p_peer_left_callback() {
    printf("[P2P] Peer left\n");
    if (g_p2pConnectionDialog) {
        QMetaObject::invokeMethod(g_p2pConnectionDialog, [=]() {
            g_p2pConnectionDialog->onPeerLeft();
        }, Qt::QueuedConnection);
    }
}

void p2p_peer_joined_callback() {
    printf("[P2P] Peer joined\n");
}

void p2p_peer_info_callback(char* peername, char* app) {
    printf("[P2P] Peer info: %s (%s)\n", peername, app);
    if (g_p2pConnectionDialog) {
        QString name = QString::fromUtf8(peername);
        QString appName = QString::fromUtf8(app);
        QMetaObject::invokeMethod(g_p2pConnectionDialog, [=]() {
            g_p2pConnectionDialog->onPeerJoined(name, appName);
        }, Qt::QueuedConnection);
    }
}

// ============================================================================
// Kaillera callbacks - forward to dialog when available
// ============================================================================

void kaillera_core_debug(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    printf("[KAILLERA] %s\n", buffer);
}

void kaillera_error_callback(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    printf("[KAILLERA ERROR] %s\n", buffer);
    if (g_kailleraServerDialog) {
        QString msg = QString::fromUtf8(buffer);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onError(msg);
        }, Qt::QueuedConnection);
    }
}

void kaillera_user_add_callback(char* name, int ping, int status, unsigned short id, char conn) {
    printf("[KAILLERA] User added: %s (ping: %d)\n", name, ping);
    if (g_kailleraServerDialog) {
        QString userName = QString::fromUtf8(name);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onUserAdd(userName, ping, status, id, conn);
        }, Qt::QueuedConnection);
    }
}

void kaillera_game_add_callback(char* gname, unsigned int id, char* emulator, char* owner, char* users, char status) {
    printf("[KAILLERA] Game added: %s by %s\n", gname, owner);
    if (g_kailleraServerDialog) {
        QString gameName = QString::fromUtf8(gname);
        QString emu = QString::fromUtf8(emulator);
        QString own = QString::fromUtf8(owner);
        QString usr = QString::fromUtf8(users);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onGameAdd(gameName, id, emu, own, usr, status);
        }, Qt::QueuedConnection);
    }
}

void kaillera_chat_callback(char* name, char* msg) {
    printf("[KAILLERA Chat] %s: %s\n", name, msg);
    if (g_kailleraServerDialog) {
        QString nick = QString::fromUtf8(name);
        QString message = QString::fromUtf8(msg);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onChat(nick, message);
        }, Qt::QueuedConnection);
    }
}

void kaillera_game_chat_callback(char* name, char* msg) {
    printf("[KAILLERA Game Chat] %s: %s\n", name, msg);
    if (g_kailleraServerDialog) {
        QString nick = QString::fromUtf8(name);
        QString message = QString::fromUtf8(msg);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onGameChat(nick, message);
        }, Qt::QueuedConnection);
    }
}

void kaillera_motd_callback(char* name, char* msg) {
    printf("[KAILLERA MOTD] %s: %s\n", name, msg);
    if (g_kailleraServerDialog) {
        QString message = QString::fromUtf8(msg);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onMotd(message);
        }, Qt::QueuedConnection);
    }
}

void kaillera_user_join_callback(char* name, int ping, unsigned short id, char conn) {
    printf("[KAILLERA] User joined: %s (ping: %d)\n", name, ping);
    if (g_kailleraServerDialog) {
        QString userName = QString::fromUtf8(name);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onUserJoin(userName, ping, id, conn);
        }, Qt::QueuedConnection);
    }
}

void kaillera_user_leave_callback(char* name, char* quitmsg, unsigned short id) {
    printf("[KAILLERA] User left: %s (%s)\n", name, quitmsg);
    if (g_kailleraServerDialog) {
        QString userName = QString::fromUtf8(name);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onUserLeave(userName, id);
        }, Qt::QueuedConnection);
    }
}

void kaillera_game_create_callback(char* gname, unsigned int id, char* emulator, char* owner) {
    printf("[KAILLERA] Game created: %s by %s\n", gname, owner);
    if (g_kailleraServerDialog) {
        QString gameName = QString::fromUtf8(gname);
        QString emu = QString::fromUtf8(emulator);
        QString own = QString::fromUtf8(owner);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onGameCreate(gameName, id, emu, own);
        }, Qt::QueuedConnection);
    }
}

void kaillera_user_game_close_callback() {
    printf("[KAILLERA] User game closed\n");
}

void kaillera_game_close_callback(unsigned int id) {
    printf("[KAILLERA] Game closed: %u\n", id);
    if (g_kailleraServerDialog) {
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onGameClose(id);
        }, Qt::QueuedConnection);
    }
}

void kaillera_user_game_create_callback() {
    printf("[KAILLERA] User created game\n");
}

void kaillera_game_status_change_callback(unsigned int id, char status, int players, int maxplayers) {
    printf("[KAILLERA] Game status change: %u (status: %d, players: %d/%d)\n", id, status, players, maxplayers);
    if (g_kailleraServerDialog) {
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onGameStatusChange(id, status, players, maxplayers);
        }, Qt::QueuedConnection);
    }
}

void kaillera_user_game_closed_callback() {
    printf("[KAILLERA] User's game was closed\n");
}

void kaillera_user_game_joined_callback() {
    printf("[KAILLERA] User joined game\n");
}

void kaillera_player_add_callback(char* name, int ping, unsigned short id, char conn) {
    printf("[KAILLERA] Player added: %s (ping: %d)\n", name, ping);
    if (g_kailleraServerDialog) {
        QString playerName = QString::fromUtf8(name);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onPlayerAdd(playerName, ping, id, conn);
        }, Qt::QueuedConnection);
    }
}

void kaillera_player_joined_callback(char* username, int ping, unsigned short id, char conn) {
    printf("[KAILLERA] Player joined: %s (ping: %d)\n", username, ping);
    if (g_kailleraServerDialog) {
        QString playerName = QString::fromUtf8(username);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onPlayerJoin(playerName, ping, id, conn);
        }, Qt::QueuedConnection);
    }
}

void kaillera_player_left_callback(char* user, unsigned short id) {
    printf("[KAILLERA] Player left: %s\n", user);
    if (g_kailleraServerDialog) {
        QString playerName = QString::fromUtf8(user);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onPlayerLeft(playerName, id);
        }, Qt::QueuedConnection);
    }
}

void kaillera_user_kicked_callback() {
    printf("[KAILLERA] User was kicked\n");
    if (g_kailleraServerDialog) {
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onKicked();
        }, Qt::QueuedConnection);
    }
}

void kaillera_login_stat_callback(char* lsmsg) {
    printf("[KAILLERA] Login stat: %s\n", lsmsg);
    if (g_kailleraServerDialog) {
        QString msg = QString::fromUtf8(lsmsg);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onLoginStat(msg);
        }, Qt::QueuedConnection);
    }
}

void kaillera_player_dropped_callback(char* user, int playerNo) {
    printf("[KAILLERA] Player dropped: %s (player %d)\n", user, playerNo);
    if (g_kailleraServerDialog) {
        QString playerName = QString::fromUtf8(user);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onPlayerDropped(playerName, playerNo);
        }, Qt::QueuedConnection);
    }
}

void kaillera_game_callback(char* game, char player, char players) {
    printf("[KAILLERA] Game callback: %s (player %d of %d)\n", game, player, players);
    if (g_kailleraServerDialog) {
        QString gameName = QString::fromUtf8(game);
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onGameStarted(gameName, player, players);
        }, Qt::QueuedConnection);
    }
}

void kaillera_game_netsync_wait_callback(int tx) {
    printf("[KAILLERA] Netsync wait: %d\n", tx);
}

void kaillera_end_game_callback() {
    printf("[KAILLERA] Game ended\n");
    if (g_kailleraServerDialog) {
        QMetaObject::invokeMethod(g_kailleraServerDialog, [=]() {
            g_kailleraServerDialog->onGameEnded();
        }, Qt::QueuedConnection);
    }
}

// ============================================================================
// Main application entry point
// ============================================================================

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("N02 Kaillera Client");
    app.setApplicationVersion("0.6.0");
    app.setOrganizationName("N02");

    // Initialize subsystems
    Settings::initialize();
    k_socket::Initialize();

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();

    int result = app.exec();

    // Cleanup
    k_socket::Cleanup();
    Settings::terminate();

    return result;
}
