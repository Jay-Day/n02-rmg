#pragma once

#include <cstdint>

#define P2P_CHAT_BUFFER_LEN 32
#define P2P_GAMESYNC_WAIT 900
#define P2P_GAMECB_WAIT 200
#define P2P_VERSION "v0r6-linux (" __DATE__ ")"

// Callbacks (to be implemented by UI layer)
void p2p_chat_callback(char* nick, char* msg);
int p2p_getSelectedDelay();
void p2p_game_callback(char* game, int playerno, int maxplayers);
void p2p_end_game_callback();
void p2p_client_dropped_callback(char* nick, int playerno);
void p2p_core_debug(const char* fmt, ...);

// Core API
bool p2p_core_cleanup();
bool p2p_core_initialize(bool host, int port, const char* appname, const char* gamename, const char* username);
bool p2p_core_connect(const char* ip, int port);
void p2p_print_core_status();
void p2p_retransmit();
void p2p_drop_game();
void p2p_set_ready(bool ready);
void p2p_ping();
void p2p_send_chat(const char* msg);
bool p2p_disconnect();
void p2p_step();
bool p2p_is_connected();
int p2p_get_frames_count();
void p2p_fodipp();
void p2p_fodipp_callback(char* host);

int p2p_modify_play_values(void* values, int size);
int p2p_core_get_port();

// Callbacks from network
void p2p_hosted_game_callback(char* game);
void p2p_ping_callback(int ping);
void p2p_send_ssrv_packet(char* cmd, int len, const char* host, int port);
void p2p_ssrv_packet_recv_callback(char* cmd, int len, void* sadr);
void p2p_send_ssrv_packet(char* cmd, int len, void* sadr);
void p2p_peer_left_callback();
void p2p_peer_joined_callback();
void p2p_peer_info_callback(char* peername, char* app);
