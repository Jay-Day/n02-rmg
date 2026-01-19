#include "player.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

// Global instance for legacy C interface
static PlaybackPlayer g_player;

PlaybackPlayer::PlaybackPlayer()
    : buffer(nullptr)
    , ptr(nullptr)
    , end(nullptr)
    , playing(false)
{
}

PlaybackPlayer::~PlaybackPlayer() {
    if (buffer) {
        std::free(buffer);
        buffer = nullptr;
    }
}

void PlaybackPlayer::loadBytes(void* dest, unsigned int len) {
    if (ptr + 10 < end) {
        unsigned int p = std::min(len, (unsigned int)(end - ptr));
        std::memcpy(dest, ptr, p);
        ptr += p;
    }
}

void PlaybackPlayer::loadString(char* dest, unsigned int maxLen) {
    maxLen = std::min(maxLen, (unsigned int)strlen(ptr) + 1);
    maxLen = std::min(maxLen, (unsigned int)(end - ptr + 1));
    loadBytes(dest, maxLen);
    dest[maxLen] = '\0';
}

int PlaybackPlayer::loadInt() {
    int x = 0;
    loadBytes(&x, 4);
    return x;
}

unsigned char PlaybackPlayer::loadChar() {
    unsigned char x = 0;
    loadBytes(&x, 1);
    return x;
}

unsigned short PlaybackPlayer::loadShort() {
    unsigned short x = 0;
    loadBytes(&x, 2);
    return x;
}

bool PlaybackPlayer::load(const char* filename) {
    // Clean up any previous buffer
    if (buffer) {
        std::free(buffer);
        buffer = nullptr;
    }
    playing = false;

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return false;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize < 272) {
        fclose(fp);
        return false;
    }

    // Allocate and read file
    buffer = (char*)std::malloc(fileSize + 66);
    if (!buffer) {
        fclose(fp);
        return false;
    }

    size_t bytesRead = fread(buffer, 1, fileSize, fp);
    fclose(fp);

    if (bytesRead != (size_t)fileSize) {
        std::free(buffer);
        buffer = nullptr;
        return false;
    }

    end = buffer + fileSize;

    // Verify magic
    if (memcmp(buffer, "KRC0", 4) != 0) {
        std::free(buffer);
        buffer = nullptr;
        return false;
    }

    // Read header
    ptr = buffer + 4;
    char appName[128];
    loadString(appName, 128);
    info.appName = appName;

    ptr = buffer + 132;
    char gameName[128];
    loadString(gameName, 128);
    info.gameName = gameName;

    ptr = buffer + 260;
    info.timestamp = loadInt();
    info.playerNumber = loadInt();
    info.numPlayers = loadInt();
    info.fileSize = fileSize;

    // Position ptr at start of record data
    ptr = buffer + 272;

    return true;
}

void PlaybackPlayer::start() {
    if (buffer) {
        playing = true;
        ptr = buffer + 272;  // Reset to start of records
    }
}

void PlaybackPlayer::stop() {
    playing = false;
}

int PlaybackPlayer::modifyPlayValues(void* values, int size) {
    (void)size;  // Currently unused

    if (!playing) {
        return -1;
    }

    if (ptr + 10 >= end) {
        playing = false;
        return -1;
    }

    unsigned char recordType = loadChar();

    switch (recordType) {
        case 0x12: {
            // Input frame
            int len = loadShort();
            if (len < 0) {
                playing = false;
                return -1;
            }
            if (len > 0) {
                loadBytes(values, len);
            }
            return len;
        }

        case 0x14: {
            // Player dropped (20 decimal)
            char playerNick[100];
            loadString(playerNick, 100);
            int playerNo = loadInt();
            if (dropCallback) {
                dropCallback(playerNick, playerNo);
            }
            // Recurse to get next actual input
            return modifyPlayValues(values, size);
        }

        case 0x08: {
            // Chat message
            char nick[100];
            char msg[500];
            loadString(nick, 100);
            loadString(msg, 500);
            if (chatCallback) {
                chatCallback(nick, msg);
            }
            // Recurse to get next actual input
            return modifyPlayValues(values, size);
        }

        default:
            // Unknown record type - stop playback
            playing = false;
            return -1;
    }
}

std::vector<std::string> PlaybackPlayer::listRecordings(const char* directory) {
    std::vector<std::string> result;

    DIR* dir = opendir(directory);
    if (!dir) {
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip directories
        if (entry->d_type == DT_DIR) {
            continue;
        }

        std::string filename = entry->d_name;
        // Check for .krec extension
        if (filename.length() > 5 &&
            filename.substr(filename.length() - 5) == ".krec") {
            result.push_back(filename);
        }
    }

    closedir(dir);

    // Sort by name
    std::sort(result.begin(), result.end());

    return result;
}

bool PlaybackPlayer::readInfo(const char* filename, RecordingInfo& outInfo) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return false;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize < 272) {
        fclose(fp);
        return false;
    }

    // Read only header
    char header[272];
    if (fread(header, 1, 272, fp) != 272) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    // Verify magic
    if (memcmp(header, "KRC0", 4) != 0) {
        return false;
    }

    // Parse header
    outInfo.appName = std::string(header + 4, strnlen(header + 4, 128));
    outInfo.gameName = std::string(header + 132, strnlen(header + 132, 128));
    std::memcpy(&outInfo.timestamp, header + 260, 4);
    std::memcpy(&outInfo.playerNumber, header + 264, 4);
    std::memcpy(&outInfo.numPlayers, header + 268, 4);
    outInfo.fileSize = fileSize;

    return true;
}

// Legacy C-style interface implementation
extern "C" {

int player_MPV(void* values, int size) {
    return g_player.modifyPlayValues(values, size);
}

void player_EndGame() {
    g_player.stop();
}

bool player_SSDSTEP() {
    // Playback doesn't need network stepping
    return false;
}

void player_ChatSend(char*) {
    // Chat sending is not supported in playback mode
}

bool player_RecordingEnabled() {
    return false;
}

bool player_isPlaying() {
    return g_player.isPlaying();
}

}
