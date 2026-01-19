#include "kaillera_core.h"
#include "k_message.h"
#include "common/k_framecache.h"
#include "common/timer.h"
#include "common/containers.h"
#include "common/stats.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <algorithm>

#define n02_TRACE()

#define KAILLERA_CONNECTION_RESP_MAX_DELAY 15000
#define KAILLERA_LOGIN_RESP_MAX_DELAY 10000
#define KAILLERA_GAME_LOSTCON_TIMEOUT 10000
#define KAILLERA_TIMEOUT_RESET 60000
#define KAILLERA_TIMEOUT_NETSYNC 120000
#define KAILLERA_TIMEOUT_NETSYNC_RETR_INTERVAL 5000

// Frame delay override (0 = use server value)
int kaillera_frame_delay_override = 0;

// Forward declarations for time functions (defined in p2p_core.cpp or shared)
static uint32_t kaillera_initial_time = 0;

static void kaillera_InitializeTime() {
    kaillera_initial_time = Timer::getTickCount();
}

static int kaillera_GetTime() {
    return Timer::getTickCount() - kaillera_initial_time;
}

struct KAILLERAC_ {
    k_message* connection;
    int frameno;
    int dframeno;
    int throughput;
    int pps;
    int REQDATALEN;
    int DATALEN;
    k_framecache USERDATA;
    oslist<char*, 256> kaillera_incoming_data_cache;
    oslist<char*, 256> kaillera_gv_queue;
    int playerno;
    int PORT;
    char IP[128];
    char APP[128];
    char GAME[128];
    char USERNAME[32];
    char conset;
    int USERSTAT;    // 0 = dc; 1 = connected; 2 = logged in; 3 = in game
    int PLAYERSTAT;  // 0 = idle; 1 = netsync; 2 = playing
    bool owner;
    bool game_id_requested;
    unsigned int game_id;
    bool user_id_requested;
    bool leave_game_requested;
    bool has_dropped;
    unsigned short user_id;
    unsigned int tmoutrsttime;
};

static KAILLERAC_ KAILLERAC;
static bool kaillera_core_initialized = false;

// State machine input (for game start)
#define KSSDFA_START_GAME 2
struct {
    int input;
} KSSDFA;

void kaillera_print_core_status() {
    kaillera_core_debug("USS/PS: %i/%i", KAILLERAC.USERSTAT, KAILLERAC.PLAYERSTAT);
    kaillera_core_debug("ICACHE: %i - %i", KAILLERAC.kaillera_incoming_data_cache.length,
                        KAILLERAC.kaillera_gv_queue.length);
}

bool kaillera_is_connected() {
    return KAILLERAC.USERSTAT > 0;
}

int kaillera_get_frames_count() {
    return KAILLERAC.frameno;
}

int kaillera_get_delay() {
    return KAILLERAC.dframeno;
}

bool kaillera_core_cleanup() {
    n02_TRACE();
    if (kaillera_core_initialized) {
        if (KAILLERAC.connection) {
            delete KAILLERAC.connection;
        }
        KAILLERAC.connection = nullptr;

        while (KAILLERAC.kaillera_incoming_data_cache.length > 0) {
            std::free(KAILLERAC.kaillera_incoming_data_cache[0]);
            KAILLERAC.kaillera_incoming_data_cache.removei(0);
        }

        while (KAILLERAC.kaillera_gv_queue.length > 0) {
            std::free(KAILLERAC.kaillera_gv_queue[0]);
            KAILLERAC.kaillera_gv_queue.removei(0);
        }

        kaillera_core_initialized = false;
    }
    return true;
}

bool kaillera_disconnect(const char* quitmsg) {
    n02_TRACE();
    if (KAILLERAC.USERSTAT >= 1) {
        if (KAILLERAC.USERSTAT > 1) {
            k_instruction ls;
            ls.type = USERLEAV;
            ls.store_short(-1);
            ls.store_string(quitmsg);
            KAILLERAC.connection->send_instruction(&ls);
            KAILLERAC.USERSTAT = 0;
            KAILLERAC.PLAYERSTAT = -1;
            return true;
        }
    } else {
        KAILLERAC.USERSTAT = 0;
        KAILLERAC.PLAYERSTAT = -1;
        return true;
    }
    return true;
}

bool kaillera_core_initialize(int port, const char* appname, const char* username, char connection_setting) {
    kaillera_InitializeTime();

    if (kaillera_core_initialized) {
        kaillera_core_cleanup();
    }

    KAILLERAC.PORT = port;
    KAILLERAC.conset = connection_setting;

    std::strncpy(KAILLERAC.APP, appname, 128);
    std::strncpy(KAILLERAC.USERNAME, username, 32);

    KAILLERAC.connection = new k_message;
    if (!KAILLERAC.connection->initialize(KAILLERAC.PORT)) {
        return false;
    }
    KAILLERAC.PORT = KAILLERAC.connection->get_port();

    KAILLERAC.kaillera_incoming_data_cache.clear();
    KAILLERAC.kaillera_gv_queue.clear();

    kaillera_core_initialized = true;
    return true;
}

int kaillera_core_get_port() {
    return KAILLERAC.PORT;
}

bool kaillera_core_connect(const char* ip, int port) {
    kaillera_core_debug("Connecting to %s:%i", ip, port);

    std::strncpy(KAILLERAC.IP, ip, 128);

    k_socket ksock;
    ksock.initialize(0, 2048);

    if (ksock.set_address(ip, port)) {
        ksock.send("HELLO0.83", 10);

        uint32_t tout = kaillera_GetTime();

        while ((!k_socket::check_sockets(0, 100) || !ksock.has_data()) &&
               (uint32_t)(kaillera_GetTime() - tout) < KAILLERA_CONNECTION_RESP_MAX_DELAY)
            ;

        if (ksock.has_data()) {
            char srsp[256];
            int srspl = 256;
            sockaddr_in addr;

            kaillera_core_debug("server replied");

            if (ksock.check_recv(srsp, &srspl, false, &addr)) {
                if (std::strncmp("HELLOD00D", srsp, 9) == 0) {
                    kaillera_core_debug("logging in");

                    addr.sin_port = htons(std::atoi(srsp + 9));
                    KAILLERAC.connection->set_addr(&addr);
                    KAILLERAC.USERSTAT = 1;
                    KAILLERAC.PLAYERSTAT = -1;

                    k_instruction ki;
                    ki.type = USERLOGN;
                    ki.store_string(KAILLERAC.APP);
                    ki.store_char(KAILLERAC.conset);
                    ki.set_username(KAILLERAC.USERNAME);

                    KAILLERAC.connection->send_instruction(&ki);
                    return true;
                }
            } else {
                if (std::strcmp("TOO", srsp) == 0) {
                    kaillera_error_callback("Server is full");
                } else {
                    kaillera_error_callback("error connecting to server");
                }
            }
        }
        return false;
    }
    return false;
}

static void kaillera_ProcessGeneralInstruction(k_instruction* ki) {
    n02_TRACE();
    switch (ki->type) {
    case USERJOIN: {
        unsigned short id = ki->load_short();
        int ping = ki->load_int();
        int conn = ki->load_char();
        kaillera_user_join_callback(ki->user, ping, id, conn);
        break;
    }
    case USERLEAV: {
        unsigned short id = ki->load_short();
        char quitmsg[128];
        ki->load_str(quitmsg, 128);
        kaillera_user_leave_callback(ki->user, quitmsg, id);
        break;
    }
    case GAMEMAKE: {
        char gname[128];
        ki->load_str(gname, 128);
        char emulator[128];
        ki->load_str(emulator, 128);
        unsigned int id = ki->load_int();

        kaillera_game_create_callback(gname, id, emulator, ki->user);

        if (KAILLERAC.game_id_requested) {
            KAILLERAC.game_id = id;
            if (std::strcmp(gname, KAILLERAC.GAME) == 0 &&
                std::strcmp(emulator, KAILLERAC.APP) == 0 &&
                std::strcmp(ki->user, KAILLERAC.USERNAME) == 0) {
                KAILLERAC.game_id_requested = false;
                KAILLERAC.user_id_requested = true;
                KAILLERAC.USERSTAT = 3;
                KAILLERAC.PLAYERSTAT = 0;
                kaillera_user_game_create_callback();
            }
        }
        break;
    }
    case INSTRUCTION_GAMESTAT: {
        unsigned int id = ki->load_int();
        char status = ki->load_char();
        int players = ki->load_char();
        int maxplayers = ki->load_char();
        kaillera_game_status_change_callback(id, status, players, maxplayers);
        break;
    }
    case INSTRUCTION_GAMESHUT: {
        unsigned int id = ki->load_int();
        kaillera_game_close_callback(id);
        if (id == KAILLERAC.game_id) {
            kaillera_user_game_closed_callback();
            kaillera_end_game_callback();
            KAILLERAC.USERSTAT = 2;
            KAILLERAC.PLAYERSTAT = -1;
        }
        break;
    }
    case GAMEBEGN: {
        KAILLERAC.PLAYERSTAT = 1;
        KAILLERAC.throughput = ki->load_short();
        int server_delay = (KAILLERAC.throughput + 1) * KAILLERAC.conset - 1;

        if (kaillera_frame_delay_override > 0) {
            KAILLERAC.dframeno = kaillera_frame_delay_override;
            kaillera_core_debug("Server delay: %i frames, using override: %i frames",
                                server_delay, KAILLERAC.dframeno);
        } else {
            KAILLERAC.dframeno = server_delay;
            kaillera_core_debug("Server says: delay is %i frames", KAILLERAC.dframeno);
        }

        KAILLERAC.playerno = ki->load_char();
        int players = ki->load_char();
        kaillera_game_callback(nullptr, KAILLERAC.playerno, players);
        break;
    }
    case GAMRSLST: {
        if (!KAILLERAC.owner) {
            kaillera_user_game_joined_callback();
        }

        KAILLERAC.USERSTAT = 3;
        KAILLERAC.PLAYERSTAT = 0;
        int numplayers = ki->load_int();
        for (int x = 0; x < numplayers; x++) {
            char name[32];
            ki->load_str(name, 32);
            int ping = ki->load_int();
            unsigned short id = ki->load_short();
            int conn = ki->load_char();
            kaillera_player_add_callback(name, ping, id, conn);
        }
        break;
    }
    case GAMRJOIN: {
        ki->load_int();
        char username[32];
        ki->load_str(username, 32);
        int ping = ki->load_int();
        unsigned short uid = ki->load_short();
        char connset = ki->load_char();

        kaillera_player_joined_callback(username, ping, uid, connset);

        if (KAILLERAC.user_id_requested) {
            KAILLERAC.user_id = uid;
            if (KAILLERAC.conset == connset && std::strcmp(username, KAILLERAC.USERNAME) == 0) {
                KAILLERAC.user_id_requested = false;
                KAILLERAC.USERSTAT = 3;
                KAILLERAC.PLAYERSTAT = 0;
            }
        }
        break;
    }
    case GAMRLEAV: {
        unsigned short id;
        kaillera_player_left_callback(ki->user, id = ki->load_short());
        if (id == KAILLERAC.user_id && !KAILLERAC.leave_game_requested) {
            kaillera_user_kicked_callback();
            KAILLERAC.USERSTAT = 2;
            KAILLERAC.PLAYERSTAT = -1;
        }
        break;
    }
    case GAMRDROP: {
        int gdpl = ki->load_char();
        if (KAILLERAC.playerno == gdpl) {
            KAILLERAC.USERSTAT = 3;
            KAILLERAC.PLAYERSTAT = 0;
            kaillera_end_game_callback();
        } else {
            kaillera_player_dropped_callback(ki->user, gdpl);
        }
        break;
    }
    case PARTCHAT:
        kaillera_chat_callback(ki->user, ki->buffer);
        break;
    case GAMECHAT:
        kaillera_game_chat_callback(ki->user, ki->buffer);
        break;
    case MOTDLINE:
        kaillera_motd_callback(ki->user, ki->buffer);
        break;
    case LONGSUCC: {
        KAILLERAC.USERSTAT = KAILLERAC.USERSTAT == 1 ? 2 : KAILLERAC.USERSTAT;
        int userZ = ki->load_int();
        int gameZ = ki->load_int();
        for (int x = 0; x < userZ; x++) {
            char name[32];
            ki->load_str(name, 32);
            int ping = ki->load_int();
            int status = ki->load_char();
            unsigned short id = ki->load_short();
            int conn = ki->load_char();
            kaillera_user_add_callback(name, ping, status, id, conn);
        }
        for (int x = 0; x < gameZ; x++) {
            char gname[128];
            ki->load_str(gname, 128);
            unsigned int id = ki->load_int();
            char emulator[128];
            ki->load_str(emulator, 128);
            char owner[32];
            ki->load_str(owner, 32);
            char users[20];
            ki->load_str(users, 20);
            int status = ki->load_char();
            kaillera_game_add_callback(gname, id, emulator, owner, users, status);
        }
        KAILLERAC.tmoutrsttime = kaillera_GetTime();
        break;
    }
    case SERVPING: {
        k_instruction pong;
        pong.type = USERPONG;
        int x = 0;
        while (x < 4) {
            pong.store_int(x++);
        }
        KAILLERAC.connection->send_instruction(&pong);
        break;
    }
    case LOGNSTAT: {
        ki->load_short();
        char lsmsg[128];
        ki->load_str(lsmsg, 128);
        kaillera_login_stat_callback(lsmsg);
        break;
    }
    default:
        break;
    }
}

void kaillera_step() {
    n02_TRACE();
    k_socket::check_sockets(0, 200);
    while (KAILLERAC.connection && KAILLERAC.connection->has_data()) {
        k_instruction ki;
        sockaddr_in saddr;
        if (KAILLERAC.connection->receive_instruction(&ki, false, &saddr)) {
            kaillera_ProcessGeneralInstruction(&ki);
        }
    }
    if (KAILLERAC.USERSTAT > 1 &&
        (uint32_t)(kaillera_GetTime() - KAILLERAC.tmoutrsttime) > KAILLERA_TIMEOUT_RESET) {
        KAILLERAC.tmoutrsttime = kaillera_GetTime();
        k_instruction trst;
        trst.type = TMOUTRST;
        KAILLERAC.connection->send_instruction(&trst);
    }
}

void kaillera_chat_send(const char* text) {
    if (KAILLERAC.USERSTAT > 1) {
        k_instruction sgc;
        sgc.type = PARTCHAT;
        sgc.store_string(text);
        KAILLERAC.connection->send_instruction(&sgc);
    }
}

void kaillera_game_chat_send(const char* text) {
    if (KAILLERAC.USERSTAT > 1) {
        k_instruction sgc;
        sgc.type = GAMECHAT;
        sgc.store_string(text);
        KAILLERAC.connection->send_instruction(&sgc);
    }
}

void kaillera_kick_user(unsigned short id) {
    if (KAILLERAC.USERSTAT > 1) {
        k_instruction sgc;
        sgc.type = GAMRKICK;
        sgc.store_short(id);
        KAILLERAC.connection->send_instruction(&sgc);
    }
}

void kaillera_join_game(unsigned int id) {
    KAILLERAC.game_id_requested = false;
    KAILLERAC.game_id = id;
    KAILLERAC.leave_game_requested = false;
    KAILLERAC.has_dropped = false;
    KAILLERAC.owner = false;

    k_instruction jog;
    jog.type = GAMRJOIN;
    jog.store_int(id);
    jog.store_char(0);
    jog.store_int(0);
    jog.store_short(-1);
    jog.store_char(KAILLERAC.conset);

    KAILLERAC.connection->send_instruction(&jog);
}

void kaillera_create_game(const char* name) {
    std::strcpy(KAILLERAC.GAME, name);
    KAILLERAC.game_id_requested = true;
    KAILLERAC.has_dropped = false;

    k_instruction cg;
    cg.type = GAMEMAKE;
    cg.store_string(name);
    cg.store_char(0);
    cg.store_int(-1);

    KAILLERAC.owner = true;
    KAILLERAC.connection->send_instruction(&cg);
}

void kaillera_leave_game() {
    KAILLERAC.leave_game_requested = true;
    k_instruction lg;
    lg.type = INSTRUCTION_GAMRLEAV;
    lg.store_short(-1);
    KAILLERAC.connection->send_instruction(&lg);

    kaillera_player_dropped_callback(KAILLERAC.USERNAME, KAILLERAC.playerno);
    KAILLERAC.PLAYERSTAT = 0;
    kaillera_end_game_callback();
    kaillera_user_game_closed_callback();
}

void kaillera_start_game() {
    KAILLERAC.leave_game_requested = false;

    k_instruction kx;
    kx.type = INSTRUCTION_GAMEBEGN;
    kx.store_int(-1);
    KAILLERAC.connection->send_instruction(&kx);

    if (KAILLERAC.has_dropped) {
        KAILLERAC.has_dropped = false;
        KAILLERAC.PLAYERSTAT = 1;
        KSSDFA.input = KSSDFA_START_GAME;
    }
}

void kaillera_game_drop() {
    KAILLERAC.leave_game_requested = false;
    k_instruction kx;
    kx.type = GAMRDROP;
    kx.store_char(0);
    kx.store_char(0);
    KAILLERAC.connection->send_instruction(&kx);
    kaillera_player_dropped_callback(KAILLERAC.USERNAME, KAILLERAC.playerno);
    KAILLERAC.PLAYERSTAT = 0;
    KAILLERAC.has_dropped = true;
    kaillera_end_game_callback();
}

void kaillera_end_game() {
    k_instruction kx;
    kx.type = GAMRDROP;
    kx.store_char(0);
    kx.store_char(0);
    KAILLERAC.connection->send_instruction(&kx);
    kaillera_end_game_callback();
}

bool kaillera_is_game_running() {
    return KAILLERAC.USERSTAT == 3 && KAILLERAC.PLAYERSTAT > 1;
}

static inline void kaillera_GameStartSequence(int size) {
    n02_TRACE();
    KAILLERAC.DATALEN = size;
    KAILLERAC.REQDATALEN = size * KAILLERAC.conset;
    KAILLERAC.USERDATA.reset();
    KAILLERAC.frameno = 0;

    while (KAILLERAC.kaillera_incoming_data_cache.length > 0) {
        std::free(KAILLERAC.kaillera_incoming_data_cache[0]);
        KAILLERAC.kaillera_incoming_data_cache.removei(0);
    }

    while (KAILLERAC.kaillera_gv_queue.length > 0) {
        std::free(KAILLERAC.kaillera_gv_queue[0]);
        KAILLERAC.kaillera_gv_queue.removei(0);
    }

    KAILLERAC.kaillera_incoming_data_cache.clear();
    KAILLERAC.kaillera_gv_queue.clear();

    k_instruction kx;
    kx.type = GAMRSRDY;
    uint32_t ti = kaillera_GetTime();
    uint32_t tit = KAILLERA_TIMEOUT_NETSYNC_RETR_INTERVAL + ti;
    KAILLERAC.connection->send_instruction(&kx);

    while (KAILLERAC.PLAYERSTAT == 1) {
        if (k_socket::check_sockets(0, 100) && KAILLERAC.connection->has_data()) {
            k_instruction ki;
            sockaddr_in saddr;
            if (KAILLERAC.connection->receive_instruction(&ki, false, &saddr)) {
                if (ki.type == GAMRSRDY) {
                    KAILLERAC.PLAYERSTAT = 2;
                    kaillera_core_debug("All players are ready");
                    break;
                } else {
                    kaillera_ProcessGeneralInstruction(&ki);
                }
            }
        }
        int tx;
        if ((tx = kaillera_GetTime() - ti) > KAILLERA_TIMEOUT_NETSYNC) {
            KAILLERAC.PLAYERSTAT = 0;
            break;
        } else {
            kaillera_game_netsync_wait_callback(KAILLERA_TIMEOUT_NETSYNC - tx);
            if ((uint32_t)kaillera_GetTime() >= tit) {
                tit = KAILLERA_TIMEOUT_NETSYNC_RETR_INTERVAL + kaillera_GetTime();
                KAILLERAC.connection->resend_message(5);
            }
        }
    }
}

static inline void kaillera_ProcessGameInstruction(k_instruction* ki) {
    n02_TRACE();
    if (ki->type == GAMEDATA) {
        short len = ki->load_short();
        char* kd;

        if (KAILLERAC.kaillera_incoming_data_cache.length == 256) {
            kd = KAILLERAC.kaillera_incoming_data_cache[0];
            KAILLERAC.kaillera_incoming_data_cache.removei(0);
            if (*((short*)kd) != len) {
                kd = (char*)std::realloc(kd, len + 2);
            }
        } else {
            kd = (char*)std::malloc(len + 2);
        }

        *((short*)kd) = len;
        char* kdd = kd + 2;
        ki->load_bytes(kdd, len);

        KAILLERAC.kaillera_incoming_data_cache.add(kd);
        int ilen = len / KAILLERAC.conset;

        for (int x = 0; x < len; x += ilen) {
            char* kds = (char*)std::malloc(ilen + 2);
            *((short*)kds) = ilen;
            std::memcpy(kds + 2, kdd + x, ilen);
            KAILLERAC.kaillera_gv_queue.add(kds);
        }
    } else if (ki->type == GAMCDATA) {
        int index = ki->load_char();
        char* kd = KAILLERAC.kaillera_incoming_data_cache[index];
        short len = *((short*)kd);
        char* kdd = kd + 2;
        int ilen = len / KAILLERAC.conset;

        for (int x = 0; x < len; x += ilen) {
            char* kds = (char*)std::malloc(ilen + 2);
            *((short*)kds) = ilen;
            std::memcpy(kds + 2, kdd + x, ilen);
            KAILLERAC.kaillera_gv_queue.add(kds);
        }
    } else {
        kaillera_ProcessGeneralInstruction(ki);
    }
}

int kaillera_modify_play_values(void* values, int size) {
    n02_TRACE();
    if (KAILLERAC.USERSTAT > 2 && KAILLERAC.PLAYERSTAT > 0) {
        if (KAILLERAC.PLAYERSTAT == 2) {
            KAILLERAC.USERDATA.put_data(values, size);
            if (KAILLERAC.USERDATA.pos >= KAILLERAC.REQDATALEN) {
                k_instruction kx;
                kx.type = GAMEDATA;
                kx.store_short(KAILLERAC.REQDATALEN);
                kx.store_bytes(KAILLERAC.USERDATA.buffer, KAILLERAC.REQDATALEN);
                KAILLERAC.connection->send_instruction(&kx);
                KAILLERAC.USERDATA.reset();
            }

            int pix = 0;
            int ttx = 0;
            do {
                if (KAILLERAC.connection->has_data() ||
                    (k_socket::check_sockets(0, pix) && KAILLERAC.connection->has_data())) {
                    k_instruction ki;
                    sockaddr_in saddr;
                    if (KAILLERAC.connection->receive_instruction(&ki, false, &saddr)) {
                        kaillera_ProcessGameInstruction(&ki);
                    }
                }
                if (KAILLERAC.frameno < KAILLERAC.dframeno) {
                    KAILLERAC.frameno++;
                    return 0;
                }
                if (KAILLERAC.kaillera_gv_queue.length <= 0) {
                    if (ttx == 0) {
                        pix++;
                        ttx = kaillera_GetTime();
                    } else {
                        uint32_t tttx = kaillera_GetTime();
                        if (tttx - ttx > 10) {
                            ttx = tttx;
                            KAILLERAC.connection->resend_message(5);
                            pix++;
                            if (pix == 500) {
                                kaillera_core_debug("Lost Connection");
                                kaillera_game_drop();
                                return -1;
                            }
                        }
                    }
                }
            } while ((KAILLERAC.kaillera_gv_queue.length <= 0) && KAILLERAC.PLAYERSTAT > 1);

            if (KAILLERAC.PLAYERSTAT > 1 && KAILLERAC.kaillera_gv_queue.length > 0) {
                char* kd = KAILLERAC.kaillera_gv_queue[0];
                KAILLERAC.kaillera_gv_queue.removei(0);
                int l = *((short*)kd);
                std::memcpy(values, kd + 2, l);
                std::free(kd);
                KAILLERAC.frameno++;
                return l;
            } else {
                return -1;
            }
        } else {
            kaillera_GameStartSequence(size);
            return kaillera_modify_play_values(values, size);
        }
    }
    return -1;
}

int kaillera_ping_server(const char* host, int port, int limit) {
    k_socket psk;
    psk.initialize(0);
    psk.set_address(host, port);
    k_socket::check_sockets(0, 0);
    uint32_t ti = Timer::getTickCount();
    psk.send("PING", 5);

    while (!psk.has_data() && (Timer::getTickCount() - ti) < (uint32_t)limit) {
        k_socket::check_sockets(0, 10);
    }

    return Timer::getTickCount() - ti;
}
