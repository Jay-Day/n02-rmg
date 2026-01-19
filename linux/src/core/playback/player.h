#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

// Recording file format (.krec)
// Header (272 bytes):
//   0-3:     "KRC0" magic
//   4-131:   Application name (128 bytes)
//   132-259: Game name (128 bytes)
//   260-263: Timestamp (4 bytes, Unix time)
//   264-267: Player number (4 bytes)
//   268-271: Number of players (4 bytes)
// Records:
//   Type 0x12: Input frame (short length + data)
//   Type 0x08: Chat message (nick string + msg string)
//   Type 0x14: Player dropped (nick string + int player number)

struct RecordingInfo {
    std::string appName;
    std::string gameName;
    uint32_t timestamp;
    int playerNumber;
    int numPlayers;
    size_t fileSize;
};

// Callbacks for playback events
typedef std::function<void(const char*, const char*)> PlaybackChatCallback;
typedef std::function<void(const char*, int)> PlaybackDropCallback;

class PlaybackPlayer {
public:
    PlaybackPlayer();
    ~PlaybackPlayer();

    // Load a recording file
    bool load(const char* filename);

    // Get recording info (after load)
    const RecordingInfo& getInfo() const { return info; }

    // Check if currently playing
    bool isPlaying() const { return playing; }

    // Start/stop playback
    void start();
    void stop();

    // Called each frame to get input values
    // Returns number of bytes written to values, or -1 if done
    int modifyPlayValues(void* values, int size);

    // Set callbacks
    void setChatCallback(PlaybackChatCallback cb) { chatCallback = cb; }
    void setDropCallback(PlaybackDropCallback cb) { dropCallback = cb; }

    // List recordings in a directory
    static std::vector<std::string> listRecordings(const char* directory);

    // Read recording info without full load
    static bool readInfo(const char* filename, RecordingInfo& info);

private:
    char* buffer;
    char* ptr;
    char* end;
    bool playing;
    RecordingInfo info;

    PlaybackChatCallback chatCallback;
    PlaybackDropCallback dropCallback;

    void loadBytes(void* dest, unsigned int len);
    void loadString(char* dest, unsigned int maxLen);
    int loadInt();
    unsigned char loadChar();
    unsigned short loadShort();
};

// Legacy C-style interface for compatibility
extern "C" {
    int player_MPV(void* values, int size);
    void player_EndGame();
    bool player_SSDSTEP();
    void player_ChatSend(char* text);
    bool player_RecordingEnabled();
    bool player_isPlaying();
}
