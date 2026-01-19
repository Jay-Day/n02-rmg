#include "module_manager.h"
#include "../common/settings.h"
#include "../common/k_socket.h"
#include "p2p/p2p_core.h"
#include "kaillera/kaillera_core.h"
#include "playback/player.h"
#include <cstring>
#include <cstdlib>

// State transition table
// [state][input] -> next_state
// Inputs: 0=none, 1=end_game, 2=start_game
static const int KSSDFAST[16] = {
    0, 0, 1, 1,   // state 0 (polling): stay, stay, -> pending, -> pending
    1, 0, 1, 0,   // state 1 (pending): stay, -> polling, stay, -> polling
    2, 0, 2, 0,   // state 2 (running): stay, -> polling, stay, -> polling
    3, 3, 3, 3    // state 3 (shutdown): always stay
};

void KSSDFA_State::transition() {
    state = KSSDFAST[(state << 2) | input];
    input = 0;
}

// Singleton instance
ModuleManager& ModuleManager::instance() {
    static ModuleManager mgr;
    return mgr;
}

ModuleManager::ModuleManager() {
}

ModuleManager::~ModuleManager() {
    shutdown();
}

bool ModuleManager::initialize() {
    if (initialized) {
        return true;
    }

    // Initialize sockets
    k_socket::Initialize();

    // Load settings
    Settings::initialize("n02");
    int savedMode = Settings::getInt("active_mode", 1);
    currentMode = static_cast<NetplayMode>(savedMode);

    // Initialize modules
    initializeModules();

    // Set initial active module
    setMode(currentMode);

    initialized = true;
    return true;
}

void ModuleManager::shutdown() {
    if (!initialized) {
        return;
    }

    // Save settings
    Settings::setInt("active_mode", static_cast<int>(currentMode));
    Settings::terminate();

    // Free game list
    if (gameList) {
        std::free(gameList);
        gameList = nullptr;
    }

    // Cleanup sockets
    k_socket::Cleanup();

    initialized = false;
}

void ModuleManager::initializeModules() {
    // P2P module
    modP2P.gui = []() {
        // TODO: Launch P2P Qt GUI
    };
    modP2P.step = []() -> bool {
        p2p_step();
        return p2p_is_connected();
    };
    modP2P.modifyPlayValues = [](void* values, int size) -> int {
        return p2p_modify_play_values(values, size);
    };
    modP2P.chatSend = [](const char* text) {
        p2p_send_chat(text);
    };
    modP2P.endGame = []() {
        p2p_drop_game();
    };
    modP2P.recordingEnabled = []() -> bool {
        return true;
    };

    // Kaillera module
    modKaillera.gui = []() {
        // TODO: Launch Kaillera Qt GUI
    };
    modKaillera.step = []() -> bool {
        kaillera_step();
        return kaillera_is_connected();
    };
    modKaillera.modifyPlayValues = [](void* values, int size) -> int {
        return kaillera_modify_play_values(values, size);
    };
    modKaillera.chatSend = [](const char* text) {
        kaillera_game_chat_send(text);
    };
    modKaillera.endGame = []() {
        kaillera_end_game();
    };
    modKaillera.recordingEnabled = []() -> bool {
        return true;
    };

    // Playback module
    modPlayback.gui = []() {
        // TODO: Launch Playback Qt GUI
    };
    modPlayback.step = []() -> bool {
        return player_SSDSTEP();
    };
    modPlayback.modifyPlayValues = [](void* values, int size) -> int {
        return player_MPV(values, size);
    };
    modPlayback.chatSend = [](const char* text) {
        player_ChatSend(const_cast<char*>(text));
    };
    modPlayback.endGame = []() {
        player_EndGame();
    };
    modPlayback.recordingEnabled = []() -> bool {
        return player_RecordingEnabled();
    };
}

void ModuleManager::setMode(NetplayMode mode) {
    if (mode == currentMode && activeModule != nullptr) {
        return;
    }

    currentMode = mode;
    rerunFlag = true;

    switch (mode) {
        case NetplayMode::P2P:
            activeModule = &modP2P;
            break;
        case NetplayMode::Kaillera:
            activeModule = &modKaillera;
            break;
        case NetplayMode::Playback:
            activeModule = &modPlayback;
            break;
    }
}

const char* ModuleManager::getModeName() const {
    switch (currentMode) {
        case NetplayMode::P2P:
            return "P2P";
        case NetplayMode::Kaillera:
            return "Kaillera Client";
        case NetplayMode::Playback:
            return "Playback";
    }
    return "Unknown";
}

void ModuleManager::setInfos(KailleraInfos* infosPtr) {
    infos = *infosPtr;
    std::strncpy(appName, infos.appName ? infos.appName : "", 127);

    // Copy game list
    if (gameList) {
        std::free(gameList);
        gameList = nullptr;
    }

    if (infos.gameList) {
        // Calculate game list length (double-null terminated)
        char* xx = infos.gameList;
        int l = 0;
        int p;
        while ((p = std::strlen(xx)) != 0) {
            l += p + 1;
            xx += p + 1;
        }
        l++;

        gameList = static_cast<char*>(std::malloc(l));
        std::memcpy(gameList, infos.gameList, l);
        infos.gameList = gameList;
    }

    infosCopy = infos;
}

void ModuleManager::setGameName(const char* name) {
    std::strncpy(gameName, name ? name : "", 127);
}

void ModuleManager::setAppName(const char* name) {
    std::strncpy(appName, name ? name : "", 127);
}

void ModuleManager::step() {
    kssdfa.transition();

    switch (kssdfa.state) {
        case KSSDFA_STATE_POLLING:
            // Call module's step function
            if (activeModule && activeModule->step) {
                activeModule->step();
            }
            break;

        case KSSDFA_STATE_GAME_PENDING:
            // Transition to running and call game callback
            kssdfa.state = KSSDFA_STATE_GAME_RUNNING;
            if (infos.gameCallback) {
                infos.gameCallback(gameName, playerNo, numPlayers);
            }
            break;

        case KSSDFA_STATE_GAME_RUNNING:
            // Just process events, handled by Qt event loop
            break;

        case KSSDFA_STATE_SHUTDOWN:
            // Exit
            break;
    }
}

// Static storage for global accessors
// These are initialized lazily when first accessed

static KSSDFA_State* g_kssdfa_ptr = nullptr;
static char* g_game_ptr = nullptr;
static char* g_app_ptr = nullptr;
static int g_playerno = 0;
static int g_numplayers = 0;
static KailleraInfos* g_infos_ptr = nullptr;

KSSDFA_State& getKSSDFA() {
    if (!g_kssdfa_ptr) {
        g_kssdfa_ptr = &ModuleManager::instance().stateMachine();
    }
    return *g_kssdfa_ptr;
}

char* getGAME() {
    if (!g_game_ptr) {
        g_game_ptr = ModuleManager::instance().getGameName();
    }
    return g_game_ptr;
}

char* getAPP() {
    if (!g_app_ptr) {
        g_app_ptr = ModuleManager::instance().getAppName();
    }
    return g_app_ptr;
}

int& getPlayerno() {
    return g_playerno;
}

int& getNumplayers() {
    return g_numplayers;
}

KailleraInfos& getInfos() {
    if (!g_infos_ptr) {
        g_infos_ptr = &ModuleManager::instance().getInfos();
    }
    return *g_infos_ptr;
}

// Update global pointers when ModuleManager is used
void ModuleManager_updateGlobals() {
    g_kssdfa_ptr = &ModuleManager::instance().stateMachine();
    g_game_ptr = ModuleManager::instance().getGameName();
    g_app_ptr = ModuleManager::instance().getAppName();
    g_infos_ptr = &ModuleManager::instance().getInfos();
}
