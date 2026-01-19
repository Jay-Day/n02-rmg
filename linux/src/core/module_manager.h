#pragma once

#include <functional>

// Forward declarations
class QWidget;

// State machine inputs
#define KSSDFA_NONE       0
#define KSSDFA_END_GAME   1
#define KSSDFA_START_GAME 2

// State machine states
#define KSSDFA_STATE_POLLING      0
#define KSSDFA_STATE_GAME_PENDING 1
#define KSSDFA_STATE_GAME_RUNNING 2
#define KSSDFA_STATE_SHUTDOWN     3

// Module mode enumeration
enum class NetplayMode {
    P2P = 0,
    Kaillera = 1,
    Playback = 2
};

// Module function signatures
typedef std::function<void()> ModuleGuiFunc;
typedef std::function<bool()> ModuleStepFunc;
typedef std::function<int(void*, int)> ModuleMpvFunc;
typedef std::function<void(const char*)> ModuleChatFunc;
typedef std::function<void()> ModuleEndGameFunc;
typedef std::function<bool()> ModuleRecordingEnabledFunc;

// Module structure containing function pointers for each mode
struct N02Module {
    ModuleGuiFunc gui;
    ModuleStepFunc step;
    ModuleMpvFunc modifyPlayValues;
    ModuleChatFunc chatSend;
    ModuleEndGameFunc endGame;
    ModuleRecordingEnabledFunc recordingEnabled;
};

// Kaillera-compatible info structure for emulator callbacks
struct KailleraInfos {
    char* appName;
    char* gameList;
    int (*gameCallback)(char* game, int player, int numplayers);
    void (*chatReceivedCallback)(char* nick, char* text);
    void (*clientDroppedCallback)(char* nick, int playernb);
    void (*moreInfosCallback)(char* gamename);
};

// State machine class
class KSSDFA_State {
public:
    int state = 0;
    int input = 0;

    void transition();
    bool isPolling() const { return state == KSSDFA_STATE_POLLING; }
    bool isGamePending() const { return state == KSSDFA_STATE_GAME_PENDING; }
    bool isGameRunning() const { return state == KSSDFA_STATE_GAME_RUNNING; }
    bool isShutdown() const { return state == KSSDFA_STATE_SHUTDOWN; }

    void requestStartGame() { input = KSSDFA_START_GAME; }
    void requestEndGame() { input = KSSDFA_END_GAME; }
    void reset() { state = 0; input = 0; }
    void shutdown() { state = KSSDFA_STATE_SHUTDOWN; }
};

// Module manager class
class ModuleManager {
public:
    static ModuleManager& instance();

    // Initialize/cleanup
    bool initialize();
    void shutdown();

    // Mode management
    void setMode(NetplayMode mode);
    NetplayMode getMode() const { return currentMode; }
    const char* getModeName() const;

    // Get current module
    N02Module& currentModule() { return *activeModule; }

    // Kaillera API compatibility
    void setInfos(KailleraInfos* infos);
    KailleraInfos& getInfos() { return infos; }
    KailleraInfos& getInfosCopy() { return infosCopy; }

    // Game state
    char* getGameName() { return gameName; }
    void setGameName(const char* name);
    char* getAppName() { return appName; }
    void setAppName(const char* name);
    int getPlayerNumber() const { return playerNo; }
    void setPlayerNumber(int p) { playerNo = p; }
    int getNumPlayers() const { return numPlayers; }
    void setNumPlayers(int n) { numPlayers = n; }

    // State machine
    KSSDFA_State& stateMachine() { return kssdfa; }

    // Main loop step
    void step();

    // Rerun flag (for mode switching in GUI)
    bool shouldRerun() const { return rerunFlag; }
    void setRerun(bool val) { rerunFlag = val; }

private:
    ModuleManager();
    ~ModuleManager();
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;

    void initializeModules();

    NetplayMode currentMode = NetplayMode::Kaillera;
    N02Module* activeModule = nullptr;

    N02Module modP2P;
    N02Module modKaillera;
    N02Module modPlayback;

    KailleraInfos infos = {};
    KailleraInfos infosCopy = {};
    char* gameList = nullptr;

    char gameName[128] = {};
    char appName[128] = {};
    int playerNo = 0;
    int numPlayers = 0;

    KSSDFA_State kssdfa;
    bool rerunFlag = false;
    bool initialized = false;
};

// Global convenience accessors (implemented as functions for proper initialization)
KSSDFA_State& getKSSDFA();
char* getGAME();
char* getAPP();
int& getPlayerno();
int& getNumplayers();
KailleraInfos& getInfos();
void ModuleManager_updateGlobals();

// Macro shortcuts for convenience
#define KSSDFA getKSSDFA()
#define GAME getGAME()
#define APP getAPP()
#define playerno getPlayerno()
#define numplayers getNumplayers()
#define infos getInfos()
