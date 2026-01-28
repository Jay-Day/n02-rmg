#include "kailleraclient.h"

#include "common/nSettings.h"
#include "p2p_ui.h"
#include <windows.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include "string.h"
#include "common/nSTL.h"


#include "uihlp.h"
#include "stats.h"


extern HINSTANCE hx;

static const char* kNoSpaceEditOldProcProp = "n02_NoSpaceEditOldProc";

static void StripSpacesInPlace(char* s) {
	if (s == NULL)
		return;
	size_t writeIndex = 0;
	for (size_t readIndex = 0; s[readIndex] != 0; readIndex++) {
		if (s[readIndex] != ' ')
			s[writeIndex++] = s[readIndex];
	}
	s[writeIndex] = 0;
}

static void RemoveSpacesInEditControl(HWND hEdit) {
	int textLength = GetWindowTextLength(hEdit);
	if (textLength <= 0)
		return;

	int bufferChars = textLength + 1;
	char* buffer = (char*)malloc((size_t)bufferChars);
	if (buffer == NULL)
		return;

	GetWindowText(hEdit, buffer, bufferChars);

	DWORD selStart = 0;
	DWORD selEnd = 0;
	SendMessage(hEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

	DWORD removedBeforeStart = 0;
	DWORD removedBeforeEnd = 0;
	for (DWORD i = 0; i < (DWORD)textLength; i++) {
		if (i < selStart && buffer[i] == ' ')
			removedBeforeStart++;
		if (i < selEnd && buffer[i] == ' ')
			removedBeforeEnd++;
	}

	int writeIndex = 0;
	for (int readIndex = 0; readIndex < textLength; readIndex++) {
		if (buffer[readIndex] != ' ')
			buffer[writeIndex++] = buffer[readIndex];
	}
	buffer[writeIndex] = 0;

	if (writeIndex != textLength) {
		SetWindowText(hEdit, buffer);
		DWORD newSelStart = (selStart > removedBeforeStart) ? (selStart - removedBeforeStart) : 0;
		DWORD newSelEnd = (selEnd > removedBeforeEnd) ? (selEnd - removedBeforeEnd) : 0;
		if (newSelStart > (DWORD)writeIndex)
			newSelStart = (DWORD)writeIndex;
		if (newSelEnd > (DWORD)writeIndex)
			newSelEnd = (DWORD)writeIndex;
		SendMessage(hEdit, EM_SETSEL, newSelStart, newSelEnd);
	}

	free(buffer);
}

static LRESULT CALLBACK NoSpaceEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	WNDPROC oldProc = (WNDPROC)GetPropA(hWnd, kNoSpaceEditOldProcProp);
	if (oldProc == NULL)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	switch (uMsg) {
		case WM_CHAR:
			if (wParam == ' ')
				return 0;
			break;
		case WM_PASTE:
		{
			LRESULT result = CallWindowProc(oldProc, hWnd, uMsg, wParam, lParam);
			RemoveSpacesInEditControl(hWnd);
			return result;
		}
		case WM_SETTEXT:
		{
			LRESULT result = CallWindowProc(oldProc, hWnd, uMsg, wParam, lParam);
			RemoveSpacesInEditControl(hWnd);
			return result;
		}
		case WM_NCDESTROY:
		{
			RemovePropA(hWnd, kNoSpaceEditOldProcProp);
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)oldProc);
			return CallWindowProc(oldProc, hWnd, uMsg, wParam, lParam);
		}
	};

	return CallWindowProc(oldProc, hWnd, uMsg, wParam, lParam);
}

static void InstallNoSpaceFilterForEdit(HWND hEdit) {
	if (hEdit == NULL)
		return;
	if (GetPropA(hEdit, kNoSpaceEditOldProcProp) != NULL)
		return;

	WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)NoSpaceEditProc);
	SetPropA(hEdit, kNoSpaceEditOldProcProp, (HANDLE)oldProc);
}

static void UpdateModeRadioButtons(HWND hDlg){
	int mode = get_active_mode_index();
	if (mode < 0 || mode > 2)
		mode = 1;
	CheckRadioButton(hDlg, RB_MODE_P2P, RB_MODE_PLAYBACK, RB_MODE_P2P + mode);
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void outp(char * line);
static void AppendP2PFormattedLine(char* fmt, va_list args) {
	char msg[2048];
	msg[0] = 0;
	vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, args);

	char ts[20];
	get_timestamp(ts, sizeof(ts));

	char line[4096];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "%s%s\r\n", ts, msg);
	outp(line);
}
void __cdecl outpf(char * arg_0, ...) {
	va_list args;
	va_start (args, arg_0);
	AppendP2PFormattedLine(arg_0, args);
	va_end (args);
}
void __cdecl p2p_core_debug(char * arg_0, ...) {
	va_list args;
	va_start (args, arg_0);
	AppendP2PFormattedLine(arg_0, args);
	va_end (args);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void flash_window(HWND handle) {
	FLASHWINFO fi;
	fi.cbSize = sizeof(fi);
	fi.hwnd = handle;
	fi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
	fi.uCount = 0;
	fi.dwTimeout = 0;
	FlashWindowEx(&fi);
}

static bool g_p2p_ssrv_copy_myip_pending = false;

static const char* kN02TraversalHost = "103.101.203.41";
static const int kN02TraversalPort = 6264;

static bool g_p2p_trav_host_enabled = false;
static bool g_p2p_trav_join_enabled = false;
static char g_p2p_trav_code[32] = { 0 };
static char g_p2p_trav_token[64] = { 0 };

static DWORD g_p2p_trav_next_reg_ms = 0;
static DWORD g_p2p_trav_next_keep_ms = 0;

static DWORD g_p2p_trav_next_join_ms = 0;
static DWORD g_p2p_trav_join_deadline_ms = 0;
static char g_p2p_trav_join_code[32] = { 0 };
static bool g_p2p_trav_join_got_host = false;
static char g_p2p_trav_join_token[64] = { 0 };
static char g_p2p_trav_join_host_ip[32] = { 0 };
static int g_p2p_trav_join_host_port = 0;
static DWORD g_p2p_trav_next_connect_ms = 0;

static char g_p2p_trav_host_peer_ip[32] = { 0 };
static int g_p2p_trav_host_peer_port = 0;
static DWORD g_p2p_trav_host_peer_deadline_ms = 0;
static DWORD g_p2p_trav_next_host_punch_ms = 0;

static void CopyTextToClipboard(HWND owner, const char* text) {
	if (text == NULL || *text == 0)
		return;

	if (!OpenClipboard(owner))
		return;

	EmptyClipboard();

	const size_t len = strlen(text) + 1;
	HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, len);
	if (hClipboardData) {
		void* data = GlobalLock(hClipboardData);
		if (data) {
			memcpy(data, text, len);
			GlobalUnlock(hClipboardData);
			if (SetClipboardData(CF_TEXT, hClipboardData) == NULL) {
				GlobalFree(hClipboardData);
			}
		} else {
			GlobalFree(hClipboardData);
		}
	}

	CloseClipboard();
}

static bool LooksLikeTraversalCode(const char* s) {
	if (s == NULL || *s == 0)
		return false;

	// Avoid treating hostnames (e.g. "example.com") as codes.
	for (const char* p = s; *p; p++) {
		if (*p == '.' || *p == ':' || *p == '/')
			return false;
	}

	int alnumCount = 0;
	for (const char* p = s; *p; p++) {
		unsigned char ch = (unsigned char)*p;
		if (isalnum(ch)) {
			alnumCount++;
			continue;
		}
		if (*p == '-' || *p == '_')
			continue;
		return false;
	}

	if (alnumCount < 6 || alnumCount > 16)
		return false;

	return true;
}

static void p2p_trav_reset_state() {
	g_p2p_trav_host_enabled = false;
	g_p2p_trav_join_enabled = false;
	g_p2p_trav_code[0] = 0;
	g_p2p_trav_token[0] = 0;
	g_p2p_trav_next_reg_ms = 0;
	g_p2p_trav_next_keep_ms = 0;
	g_p2p_trav_next_join_ms = 0;
	g_p2p_trav_join_deadline_ms = 0;
	g_p2p_trav_join_code[0] = 0;
	g_p2p_trav_join_got_host = false;
	g_p2p_trav_join_token[0] = 0;
	g_p2p_trav_join_host_ip[0] = 0;
	g_p2p_trav_join_host_port = 0;
	g_p2p_trav_next_connect_ms = 0;
	g_p2p_trav_host_peer_ip[0] = 0;
	g_p2p_trav_host_peer_port = 0;
	g_p2p_trav_host_peer_deadline_ms = 0;
	g_p2p_trav_next_host_punch_ms = 0;
}

static void p2p_trav_send_to_server(const char* msg) {
	if (msg == NULL || *msg == 0)
		return;

	size_t msgLen = strlen(msg);
	if (msgLen == 0 || msgLen > (size_t)INT_MAX)
		return;

	outpf("NAT traversal -> %s:%i: %s", kN02TraversalHost, kN02TraversalPort, msg);

	// IMPORTANT: traversal messages must NOT include the trailing NUL byte.
	p2p_send_ssrv_packet((char*)msg, (int)msgLen, (char*)kN02TraversalHost, kN02TraversalPort);
}

static void p2p_trav_send_reg() {
	char msg[128];
	_snprintf_s(msg, sizeof(msg), _TRUNCATE, "N02TRAV1|REG|%u", (unsigned int)GetTickCount());
	p2p_trav_send_to_server(msg);
}

static void p2p_trav_send_keep() {
	if (g_p2p_trav_token[0] == 0)
		return;
	char msg[128];
	_snprintf_s(msg, sizeof(msg), _TRUNCATE, "N02TRAV1|KEEP|%s", g_p2p_trav_token);
	p2p_trav_send_to_server(msg);
}

static void p2p_trav_send_close() {
	if (g_p2p_trav_token[0] == 0)
		return;
	char msg[128];
	_snprintf_s(msg, sizeof(msg), _TRUNCATE, "N02TRAV1|CLOSE|%s", g_p2p_trav_token);
	p2p_trav_send_to_server(msg);
}

static void p2p_trav_send_join() {
	if (g_p2p_trav_join_code[0] == 0)
		return;
	char msg[160];
	_snprintf_s(msg, sizeof(msg), _TRUNCATE, "N02TRAV1|JOIN|%s|%u", g_p2p_trav_join_code, (unsigned int)GetTickCount());
	p2p_trav_send_to_server(msg);
}

static void p2p_trav_punch_endpoint(const char* hostIp, int hostPort, const char* token) {
	if (hostIp == NULL || *hostIp == 0 || hostPort <= 0)
		return;
	if (token == NULL)
		token = "";

	outpf("NAT traversal -> %s:%i: punch x10", hostIp, hostPort);

	char msg[160];
	_snprintf_s(msg, sizeof(msg), _TRUNCATE, "N02TRAV1|PUNCH|%s", token);
	const int sendLen = (int)strlen(msg);
	if (sendLen <= 0)
		return;

	for (int i = 0; i < 10; i++) {
		p2p_send_ssrv_packet(msg, sendLen, (char*)hostIp, hostPort);
	}
}

static bool TryExtractIPv4AndPort(const char* s, char* out_ip, size_t out_ip_len, int* out_port) {
	if (s == NULL || out_ip == NULL || out_ip_len == 0)
		return false;

	out_ip[0] = 0;
	if (out_port)
		*out_port = 0;

	for (const char* p = s; *p; p++) {
		if (*p < '0' || *p > '9')
			continue;

		unsigned int octets[4];
		const char* cur = p;
		bool ok = true;
		for (int i = 0; i < 4; i++) {
			unsigned int val = 0;
			int digits = 0;
			while (*cur >= '0' && *cur <= '9' && digits < 3) {
				val = val * 10 + (unsigned int)(*cur - '0');
				cur++;
				digits++;
			}
			if (digits == 0 || val > 255) {
				ok = false;
				break;
			}
			octets[i] = val;
			if (i < 3) {
				if (*cur != '.') {
					ok = false;
					break;
				}
				cur++;
			}
		}
		if (!ok)
			continue;

		_snprintf_s(out_ip, out_ip_len, _TRUNCATE, "%u.%u.%u.%u", octets[0], octets[1], octets[2], octets[3]);

		// Optional ":port" (or " port")
		while (*cur == ' ' || *cur == '\t')
			cur++;
		if (*cur == ':') {
			cur++;
		} else if (*cur != 0 && (*cur < '0' || *cur > '9')) {
			return true;
		}

		if (out_port) {
			unsigned int port = 0;
			int digits = 0;
			while (*cur >= '0' && *cur <= '9' && digits < 5) {
				port = port * 10 + (unsigned int)(*cur - '0');
				cur++;
				digits++;
			}
			if (digits > 0 && port > 0 && port <= 65535)
				*out_port = (int)port;
		}
		return true;
	}

	return false;
}



//int PACKETLOSSCOUNT;
//int PACKETMISOTDERCOUNT;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HOST;
char GAME[128];
char APP[128];
int playerno;
int numplayers;
char IP[128];
char USERNAME[128];
int PORT;
///////////////////////////////////////////////////////////////////////////////
bool COREINIT = false;
int PING_TIME;
int p2p_option_smoothing;
int p2p_frame_delay_override;
int p2p_30fps_mode;
int p2p_cdlg_peer_joined;
int p2p_sdlg_frameno = 0;
int p2p_sdlg_pps = 0;
///////////////////////////////////////////////////////////////////////////////
HWND p2p_ui_connection_dlg;
HWND p2p_ui_con_chatinp;
HWND p2p_ui_con_richedit;
HWND p2p_ui_modeseltab;
nLVw p2p_ui_storedusers;
HWND p2p_ui_ss_dlg;
///////////////////////////////////////////////////////////////////////////////







void p2p_ui_pcs(){
	
	outpf("============= Core status begin ===============");

	p2p_print_core_status();

	unsigned int sta =  KSSDFA.state;
	unsigned int inp = KSSDFA.input;
	outpf("KSSDFA { state: %i, input: %i }", sta, inp);
	//outpf("PACKETLOSSCOUNT=%u", PACKETLOSSCOUNT);
	//outpf("PACKETMISOTDERCOUNT=%u", PACKETMISOTDERCOUNT);
	outpf("============ Core status end =================");
	
}




void p2p_ping_callback(int PING){
	char buf[200];
	wsprintf(buf, "ping: %i ms pl: %i", PING, PACKETLOSSCOUNT);
	SetWindowText(GetDlgItem(p2p_ui_connection_dlg, SA_PST), buf);
}



void p2p_chat_callback(char * nick, char * msg){
	outpf("<%s> %s",nick, msg);
	if (KSSDFA.state==2 && infos.chatReceivedCallback) {
		infos.chatReceivedCallback(nick, msg);
	}
}

//bool p2p_add_delay_callback(){
	//return SendMessage(GetDlgItem(p2p_ui_connection_dlg, IDC_ADDDELAY), BM_GETCHECK, 0, 0)==BST_CHECKED;
	//return true;
//}
int p2p_getSelectedDelay() {
	// "Host smoothing" UI removed; always treat as None.
	return 0;
}
void p2p_game_callback(char * game, int playernop, int maxplayersp){
	strncpy(GAME, (game != NULL) ? game : "", sizeof(GAME) - 1);
	GAME[sizeof(GAME) - 1] = 0;
	playerno = playernop;
	numplayers = maxplayersp;
	KSSDFA.input = KSSDFA_START_GAME;
}

void p2p_end_game_callback(){
	KSSDFA.input = KSSDFA_END_GAME;
	KSSDFA.state = 0;
	SendMessage(GetDlgItem(p2p_ui_connection_dlg, IDC_READY), BM_SETCHECK, BST_UNCHECKED, 0);
}

void p2p_client_dropped_callback(char * nick, int playerno){
	if (infos.clientDroppedCallback) {
		infos.clientDroppedCallback(nick, playerno);
	}
}

void p2p_ui_chat_send(char * xxx){
	if (strcmp(xxx, "/pcs")==0) {
		p2p_ui_pcs();
		return;
	} else if (strncmp(xxx, "/ssrv", 5)==0) {
		char* ptr = xxx + 6;
		char* host = ptr;
		while (*++ptr != ' '); *ptr++ = 0;
		int port = atoi(ptr);
			while (*++ptr != ' '); *ptr++ = 0;
			char* cmd = ptr;
			{
				size_t cmdLen = strlen(cmd) + 1;
				int sendLen = (cmdLen > (size_t)INT_MAX) ? INT_MAX : (int)cmdLen;
				p2p_send_ssrv_packet(cmd, sendLen, host, port);
			}
			return;
		} else if (strcmp(xxx, "/stats")==0) {
		StatsDisplayThreadBegin();
		return;
	} else if (strcmp(xxx, "/halfdelay")==0) {
		p2p_30fps_mode = !p2p_30fps_mode;
		outpf("Half delay mode %s (for 30fps games: Mario Kart, 1080, THPS)", p2p_30fps_mode ? "ENABLED" : "DISABLED");
		return;
	}
	if (p2p_is_connected()) {
		if (*xxx == '/') {
			xxx++;
			if (strcmp(xxx, "ping")==0) {
				p2p_ping();
			} else if (strcmp(xxx, "retr")==0) {
				p2p_retransmit();
			} else {
				outpf("Unknown command \"%s\"", xxx);
				p2p_send_chat(xxx-1);
			}
		} else {
			p2p_send_chat(xxx);
		}
	}
}

void p2p_EndGame(){
	outpf("dropping game");
	p2p_drop_game();
}
bool p2p_SelectServerDlgStep(){
	if (COREINIT) {
		p2p_step();
		return true;
	}
	return false;
}



bool p2p_RecordingEnabled(){
	return SendMessage(GetDlgItem(p2p_ui_connection_dlg, CHK_REC), BM_GETCHECK, 0, 0)==BST_CHECKED;
}

void p2p_ssrv_send(char* cmd) {
	char xxx[2048];
	wsprintf(xxx, "/ssrv %s %s", "kaillerareborn.2manygames.fr 27887", cmd);
	p2p_ui_chat_send(xxx);
}

void p2p_ssrv_whatismyip(){
	g_p2p_ssrv_copy_myip_pending = true;
	p2p_ssrv_send("WHATISMYIP");
}

void p2p_ssrv_packet_recv_callback(char *cmd, int len, void *sadr){
	char cmd_buf[2048];
	if (cmd == NULL || len <= 0)
		return;
	{
		int copy_len = len;
		if (copy_len > (int)sizeof(cmd_buf) - 1)
			copy_len = (int)sizeof(cmd_buf) - 1;
		memcpy(cmd_buf, cmd, (size_t)copy_len);
		cmd_buf[copy_len] = 0;
		cmd = cmd_buf;
	}

	// NAT traversal (N02TRAV1) uses the same "raw/SSRV" UDP path.
	if (strncmp(cmd, "N02TRAV1|", 9) == 0) {
		char originalCmd[2048];
		strncpy(originalCmd, cmd, sizeof(originalCmd) - 1);
		originalCmd[sizeof(originalCmd) - 1] = 0;

		// Split in-place.
		char* parts[8] = { 0 };
		int partCount = 0;
		parts[partCount++] = cmd;
		for (char* p = cmd; *p && partCount < (int)(sizeof(parts) / sizeof(parts[0])); p++) {
			if (*p == '|') {
				*p = 0;
				parts[partCount++] = p + 1;
			}
		}

		if (partCount >= 2) {
			const char* type = parts[1];

			if (strcmp(type, "REGOK") == 0 && partCount >= 6) {
				if (!g_p2p_trav_host_enabled) {
					outpf("NAT traversal <- REGOK (ignored; not hosting-by-code)");
					return;
				}

				strncpy(g_p2p_trav_code, parts[2], sizeof(g_p2p_trav_code) - 1);
				g_p2p_trav_code[sizeof(g_p2p_trav_code) - 1] = 0;
				strncpy(g_p2p_trav_token, parts[3], sizeof(g_p2p_trav_token) - 1);
				g_p2p_trav_token[sizeof(g_p2p_trav_token) - 1] = 0;

				outpf("NAT traversal <- REGOK code=%s observed=%s:%s", g_p2p_trav_code, parts[4], parts[5]);
				CopyTextToClipboard(p2p_ui_connection_dlg, g_p2p_trav_code);
				outpf("Copied connect code to clipboard");

				// Start keepalive cadence.
				g_p2p_trav_next_keep_ms = 0;
				return;
			}

			if (strcmp(type, "HOST") == 0 && partCount >= 5) {
				if (!g_p2p_trav_join_enabled) {
					outpf("NAT traversal <- HOST (ignored; not joining-by-code)");
					return;
				}

				const char* token = parts[2];
				const char* hostIp = parts[3];
				const int hostPort = atoi(parts[4]);

				outpf("NAT traversal <- HOST %s:%i", hostIp, hostPort);
				strncpy(g_p2p_trav_join_token, token, sizeof(g_p2p_trav_join_token) - 1);
				g_p2p_trav_join_token[sizeof(g_p2p_trav_join_token) - 1] = 0;
				strncpy(g_p2p_trav_join_host_ip, hostIp, sizeof(g_p2p_trav_join_host_ip) - 1);
				g_p2p_trav_join_host_ip[sizeof(g_p2p_trav_join_host_ip) - 1] = 0;
				g_p2p_trav_join_host_port = hostPort;
				g_p2p_trav_join_got_host = true;

				// Stop asking the server once we have a host endpoint; we may still need to retry the actual P2P handshake.
				g_p2p_trav_next_join_ms = 0;

				// Try immediately (and WM_TIMER will retry for a short window if needed).
				outpf("NAT traversal: connecting to %s:%i", g_p2p_trav_join_host_ip, g_p2p_trav_join_host_port);
				p2p_trav_punch_endpoint(g_p2p_trav_join_host_ip, g_p2p_trav_join_host_port, g_p2p_trav_join_token);
				if (!p2p_core_connect(g_p2p_trav_join_host_ip, g_p2p_trav_join_host_port)) {
					outpf("NAT traversal: error connecting to %s:%i", g_p2p_trav_join_host_ip, g_p2p_trav_join_host_port);
				}

				return;
			}

			if (strcmp(type, "PEER") == 0 && partCount >= 5) {
				if (!g_p2p_trav_host_enabled) {
					outpf("NAT traversal <- PEER (ignored; not hosting-by-code)");
					return;
				}

				const char* token = parts[2];
				const char* peerIp = parts[3];
				const int peerPort = atoi(parts[4]);

				if (g_p2p_trav_token[0] != 0 && strcmp(token, g_p2p_trav_token) != 0) {
					outpf("NAT traversal: ignoring PEER for unknown token");
					return;
				}

				outpf("NAT traversal <- PEER %s:%i", peerIp, peerPort);
				outpf("NAT traversal: peer %s:%i", peerIp, peerPort);

				strncpy(g_p2p_trav_host_peer_ip, peerIp, sizeof(g_p2p_trav_host_peer_ip) - 1);
				g_p2p_trav_host_peer_ip[sizeof(g_p2p_trav_host_peer_ip) - 1] = 0;
				g_p2p_trav_host_peer_port = peerPort;
				g_p2p_trav_host_peer_deadline_ms = GetTickCount() + 15000;
				g_p2p_trav_next_host_punch_ms = 0;

				p2p_trav_punch_endpoint(peerIp, peerPort, token);
				return;
			}

			if (strcmp(type, "ERR") == 0 && partCount >= 3) {
				outpf("NAT traversal <- ERR %s", parts[2]);
				outpf("NAT traversal: server response was: %s", originalCmd);
				return;
			}

			if (strcmp(type, "OK") == 0) {
				outpf("NAT traversal <- OK");
				return;
			}

			// No-op payload used only to open NAT mappings; can be frequent.
			if (strcmp(type, "PUNCH") == 0) {
				return;
			}
		}

		outpf("NAT traversal <- (unhandled) %s", originalCmd);
		return;
	}

	if (strcmp(cmd, "PINGRQ")==0){
		p2p_send_ssrv_packet("xxxxxxxxxx",11, sadr);
		return;
	} else if (strncmp(cmd, "MSG", 3)==0){
		return;
	}

	if (g_p2p_ssrv_copy_myip_pending) {
		char ip[32];
		int port = 0;
		if (TryExtractIPv4AndPort(cmd, ip, sizeof(ip), &port)) {
			if (port <= 0)
				port = p2p_core_get_port();
			if (port > 0) {
				char ip_port[64];
				_snprintf_s(ip_port, sizeof(ip_port), _TRUNCATE, "%s:%d", ip, port);
				CopyTextToClipboard(p2p_ui_connection_dlg, ip_port);
				outpf("Copied %s to clipboard", ip_port);
			}
		}
		// Best-effort: don't keep a dangling "copy pending" flag if the reply is unexpected/unsupported.
		g_p2p_ssrv_copy_myip_pending = false;
	}

	p2p_core_debug("SSRV: %s", cmd);
}

void p2p_ssrv_enlistgame(){
	char buf[500];
	wsprintf(buf, "ENLIST %s|%s|%s", GAME, APP, USERNAME);
	p2p_ssrv_send(buf);
}

static int GetP2PEnlistPort() {
	int port = p2p_core_get_port();
	if (port <= 0)
		port = PORT;
	return port;
}

void p2p_ssrv_enlistgamef() {
	char buf[500];
	wsprintf(buf, "ENLISP %s|%s|%s|%i", GAME, APP, USERNAME, GetP2PEnlistPort());
	p2p_ssrv_send(buf);
}

void p2p_ssrv_unenlistgame(){
	p2p_ssrv_send("UNENLIST");
}

void p2p_enlist_game() {
	const int enlistPort = GetP2PEnlistPort();
	if (enlistPort == 27886) {
		p2p_ssrv_enlistgame();
	} else {
		p2p_ssrv_enlistgamef();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void p2p_peer_joined_callback(){
	MessageBeep(MB_OK);
	if (HOST && SendMessage(GetDlgItem(p2p_ui_connection_dlg,CHK_ENLIST), BM_GETCHECK, 0, 0)==BST_CHECKED)
		p2p_ssrv_unenlistgame();
	p2p_cdlg_peer_joined = 1;
}

void p2p_peer_left_callback(){
	MessageBeep(MB_OK);
	p2p_core_debug("Peer left");
	if (HOST && SendMessage(GetDlgItem(p2p_ui_connection_dlg,CHK_ENLIST), BM_GETCHECK, 0, 0)==BST_CHECKED)
		p2p_enlist_game();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
void IniaialzeConnectionDialog(HWND hDlg){
	p2p_ui_connection_dlg = hDlg;
	if (p2p_core_initialize(HOST, PORT, APP, GAME, USERNAME)){
		//ShowWindow(GetDlgItem(hDlg, IDC_ADDDELAY), HOST? SW_SHOW:SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, CHK_ENLIST), HOST ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDC_SSERV_WHATSMYIP), HOST ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDC_HOSTT), HOST ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDC_P2P_FDLY_LBL), HOST ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDC_P2P_FDLY), HOST ? SW_SHOW : SW_HIDE);
		
		if (HOST) {
			outpf("Hosting %s on port %i", GAME, p2p_core_get_port());
			if (g_p2p_trav_host_enabled) {
				outpf("NAT traversal: enabled (share the connect code once it appears).");
				p2p_trav_send_reg();
				g_p2p_trav_next_reg_ms = GetTickCount() + 3000;
			} else {
				outpf("WARNING: Hosting requires hosting ports to be forwarded and enabled in firewalls.");
			}
			SetDlgItemText(hDlg, IDC_GAME, GAME);
		} else {

			if (g_p2p_trav_join_enabled) {
				outpf("NAT traversal: looking up host for code %s", g_p2p_trav_join_code);
				p2p_trav_send_join();
				g_p2p_trav_next_join_ms = GetTickCount() + 3000;
				g_p2p_trav_join_deadline_ms = GetTickCount() + 15000;
			} else {

			char * host;
			host = IP;
			int port = 27886;
			while (*++host != ':' && *host != 0);
			if (*host == ':') {
				*host++ = 0x00;
				port = atoi(host);
				port = port==0?27886:port;
			}
			host = IP;
//			76.81.211.10:27886
			outpf("Connecting to %s:%i", host, port);

			if (!p2p_core_connect(host, port)){

				MessageBox(hDlg, "Error connecting to specified host/port", host, 0);
				//EndDialog(hDlg, 0);
				return;
			}

			}
		}
		
		COREINIT = true;

	} else outpf("Error initializing sockets");
	


}

char peername[32];
char peerapp[128];
void p2p_peer_info_callback(char* p33rname, char* app) {
	strncpy(peername, (p33rname != NULL) ? p33rname : "", sizeof(peername) - 1);
	peername[sizeof(peername) - 1] = 0;
	strncpy(peerapp, (app != NULL) ? app : "", sizeof(peerapp) - 1);
	peerapp[sizeof(peerapp) - 1] = 0;
}

void outp(char * line){
	if (p2p_ui_con_richedit == NULL || line == NULL)
		return;
	re_append(p2p_ui_con_richedit, line, 0);
}


UINT_PTR p2p_cdlg_timer;
int p2p_cdlg_timer_step;
LRESULT CALLBACK ConnectionDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_INITDIALOG:
			{
				// "Host smoothing" UI removed; always treat as None.
				p2p_option_smoothing = 0;

				// Frame delay dropdown (host only, per-session)
				p2p_frame_delay_override = 0;
				p2p_30fps_mode = 0;
				HWND hFdlyCombo = GetDlgItem(hDlg, IDC_P2P_FDLY);
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"Auto");
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"1 frame (0-33ms)");
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"2 frames (34-67ms)");
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"3 frames (68-99ms)");
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"4 frames (100-133ms)");
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"5 frames (134-167ms)");
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"6 frames (168-199ms)");
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"7 frames (200-233ms)");
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"8 frames (234-267ms)");
				SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"9 frames (268+ms)");
				SendMessage(hFdlyCombo, CB_SETCURSEL, 0, 0);

				p2p_ui_con_richedit = GetDlgItem(hDlg, IDC_RICHEDIT2);
				p2p_ui_con_chatinp = GetDlgItem(hDlg, IDC_CHATI);
				IniaialzeConnectionDialog(hDlg);
				re_enable_hyperlinks(p2p_ui_con_richedit);
				p2p_cdlg_timer = SetTimer(hDlg, 0, 1000, 0);
			}

			break;
		case WM_CLOSE:
			if (p2p_disconnect()){
				// Ensure any override doesn't persist across runs
				p2p_frame_delay_override = 0;
				p2p_30fps_mode = 0;

				if (HOST && g_p2p_trav_host_enabled) {
					p2p_trav_send_close();
				}
				p2p_trav_reset_state();

				if (SendMessage(GetDlgItem(hDlg,CHK_ENLIST), BM_GETCHECK, 0, 0)==BST_CHECKED) {
					p2p_ssrv_unenlistgame();
				}

				KillTimer(hDlg, p2p_cdlg_timer);
				//kprintf(__FILE__ ":%i", __LINE__);
				EndDialog(hDlg, 0);
				p2p_core_cleanup();
			}
			KSSDFA.state = 0;
			break;
	case WM_TIMER:
		{
			p2p_cdlg_timer_step++;

			// NAT traversal housekeeping (host keepalive / join retries)
			const DWORD now = GetTickCount();
			if (HOST && g_p2p_trav_host_enabled) {
				if (g_p2p_trav_token[0] == 0) {
					if (g_p2p_trav_next_reg_ms == 0 || now >= g_p2p_trav_next_reg_ms) {
						p2p_trav_send_reg();
						g_p2p_trav_next_reg_ms = now + 3000;
					}
				} else {
					if (g_p2p_trav_next_keep_ms == 0 || now >= g_p2p_trav_next_keep_ms) {
						p2p_trav_send_keep();
						g_p2p_trav_next_keep_ms = now + 10000;
					}
				}
				// While waiting for the peer's initial LOGN_REQ, keep punching their endpoint for a short window.
				if (!p2p_is_connected() &&
					g_p2p_trav_host_peer_ip[0] != 0 &&
					g_p2p_trav_host_peer_port > 0 &&
					g_p2p_trav_host_peer_deadline_ms != 0 &&
					now < g_p2p_trav_host_peer_deadline_ms) {
					if (g_p2p_trav_next_host_punch_ms == 0 || now >= g_p2p_trav_next_host_punch_ms) {
						p2p_trav_punch_endpoint(g_p2p_trav_host_peer_ip, g_p2p_trav_host_peer_port, g_p2p_trav_token);
						g_p2p_trav_next_host_punch_ms = now + 1000;
					}
				}
			} else if (!HOST && g_p2p_trav_join_enabled && !p2p_is_connected()) {
				if (g_p2p_trav_join_deadline_ms != 0 && now >= g_p2p_trav_join_deadline_ms) {
					outpf("NAT traversal: timed out (try direct IP/port-forwarding or server mode)");
					g_p2p_trav_join_enabled = false;
				} else if (!g_p2p_trav_join_got_host) {
					if (g_p2p_trav_next_join_ms == 0 || now >= g_p2p_trav_next_join_ms) {
						p2p_trav_send_join();
						g_p2p_trav_next_join_ms = now + 3000;
					}
				} else {
					// We have a host endpoint; retry the punch + LOGN_REQ a few times (first packet is often dropped
					// if the host NAT hasn't seen an outbound packet to us yet).
					if (g_p2p_trav_next_connect_ms == 0 || now >= g_p2p_trav_next_connect_ms) {
						p2p_trav_punch_endpoint(g_p2p_trav_join_host_ip, g_p2p_trav_join_host_port, g_p2p_trav_join_token);
						p2p_core_connect(g_p2p_trav_join_host_ip, g_p2p_trav_join_host_port);
						g_p2p_trav_next_connect_ms = now + 1000;
					}
				}
			} else if (!HOST && g_p2p_trav_join_enabled && p2p_is_connected()) {
				// Connected; stop traversal retries.
				g_p2p_trav_join_enabled = false;
				g_p2p_trav_join_deadline_ms = 0;
			}

			if (KSSDFA.state == 0) {
				if (p2p_is_connected()){
					SetDlgItemText(hDlg, IDC_GAME, GAME);
					p2p_ping();
					{
						char xx[256];
						wsprintf(xx, "Connected to %s (%s)", peername, peerapp);
						SetWindowText(p2p_ui_connection_dlg, xx);
					}
					if (HOST && p2p_cdlg_peer_joined) {
						p2p_cdlg_peer_joined = 0;
						flash_window(hDlg);
					}
					} else {
						if (p2p_cdlg_timer_step%30==0 && !p2p_is_connected() && HOST && SendMessage(GetDlgItem(hDlg,CHK_ENLIST), BM_GETCHECK, 0, 0)==BST_CHECKED) {
							p2p_enlist_game();
						}
					}
				} else if (KSSDFA.state == 2) {
				int jf;
				if ((jf = p2p_get_frames_count()) != p2p_sdlg_frameno){
					char xxx[32];
					wsprintf(xxx, "%i fps/%i pps", jf - p2p_sdlg_frameno, SOCK_SEND_PACKETS - p2p_sdlg_pps);
					SetWindowText(GetDlgItem(p2p_ui_connection_dlg, SA_PST), xxx);
					p2p_sdlg_frameno = jf;
					p2p_sdlg_pps = SOCK_SEND_PACKETS;
				}
			}
			break;
		}
	case WM_NOTIFY:
		re_handle_link_click(lParam);
		break;
	case WM_COMMAND:
		////kprintf(__FILE__ ":%i", __LINE__);//localhost:27888
		switch (LOWORD(wParam)) {
		case IDC_P2P_FDLY:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				p2p_frame_delay_override = (int)SendMessage(GetDlgItem(hDlg, IDC_P2P_FDLY), CB_GETCURSEL, 0, 0);
				if (p2p_frame_delay_override == CB_ERR) p2p_frame_delay_override = 0;
			}
			break;
		case IDC_CHAT:
			{
				char xxx[251];
				GetWindowText(p2p_ui_con_chatinp, xxx, 251);
				p2p_ui_chat_send(xxx);
				SetWindowText(p2p_ui_con_chatinp, "");
			}
			break;
		case IDC_RETR:
			{
				p2p_ui_chat_send("/retr");
			}
			break;
		case IDC_PING:
			{
				p2p_ui_chat_send("/ping");
			}
			break;
		case IDC_DROPGAME:
			p2p_drop_game();
			break;
		case IDC_STATS:
			StatsDisplayThreadBegin();
			break;
		case IDC_SSERV_WHATSMYIP:
			p2p_ssrv_whatismyip();
			break;
			case IDC_READY:
				{
					p2p_set_ready(SendMessage(GetDlgItem(hDlg, IDC_READY), BM_GETCHECK, 0, 0)==BST_CHECKED);
				}
				break;
			case CHK_ENLIST:
				{
					if (SendMessage(GetDlgItem(hDlg,CHK_ENLIST), BM_GETCHECK, 0, 0)==BST_CHECKED) {
						p2p_enlist_game();
					} else {
					p2p_ssrv_unenlistgame();
				}
			}
			break;
		};
		break;
	};	
	return 0;
}
///////////////////////////////////////////////////

void InitializeP2PSubsystem(HWND hDlg, bool host, bool hostByCode){
	ShowWindow(p2p_ui_ss_dlg, SW_HIDE);

	p2p_trav_reset_state();
	g_p2p_trav_host_enabled = host && hostByCode;

	HOST = host;

	if (!host) {
		PORT = 0;
	} else {
		PORT = GetDlgItemInt(p2p_ui_ss_dlg, IDC_PORT, 0, FALSE);
	}

	GetWindowText(GetDlgItem(p2p_ui_ss_dlg, IDC_GAME), GAME, 127);
	GetWindowText(GetDlgItem(p2p_ui_ss_dlg, IDC_IP), IP, 127);
	GetWindowText(GetDlgItem(p2p_ui_ss_dlg, IDC_USRNAME), USERNAME, 31);

	StripSpacesInPlace(IP);

	if (!HOST && LooksLikeTraversalCode(IP)) {
		g_p2p_trav_join_enabled = true;
		strncpy(g_p2p_trav_join_code, IP, sizeof(g_p2p_trav_join_code) - 1);
		g_p2p_trav_join_code[sizeof(g_p2p_trav_join_code) - 1] = 0;
	}

	//kprintf(__FILE__ ":%i", __LINE__);//localhost:27888

	if (HOST) {
		if (gamelist != 0) {
			char * xx = gamelist;
			size_t p;
			while ((p = strlen(xx)) != 0){
				if (strcmp(xx, GAME)==0){
					//kprintf(__FILE__ ":%i", __LINE__);//localhost:27888
					DialogBox(hx, (LPCTSTR)CONNECTION_DIALOG, NULL, (DLGPROC)ConnectionDialogProc);
					ShowWindow(p2p_ui_ss_dlg, SW_SHOW);
					return;
				}
				xx += p + 1;
			}
		}
		MessageBox(hDlg, "Pick a valid game", 0,0);
		ShowWindow(p2p_ui_ss_dlg, SW_SHOW);
		return;
	}

	DialogBox(hx, (LPCTSTR)CONNECTION_DIALOG, NULL, (DLGPROC)ConnectionDialogProc);

	ShowWindow(p2p_ui_ss_dlg, SW_SHOW);
}

// Backwards-compat wrapper for other modules (e.g. waiting-games dialog).
void InitializeP2PSubsystem(HWND hDlg, bool host) {
	InitializeP2PSubsystem(hDlg, host, false);
}


///////////////////////////////////////////////////////////////
#define P2PSelectionDialogProcSetModee(hDlg, ITEM, Mode) ShowWindow(GetDlgItem(hDlg, ITEM), Mode);

void P2PSelectionDialogProcSetMode(HWND hDlg, bool connector){

	if (connector){
		P2PSelectionDialogProcSetModee(hDlg, IDC_PORT, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_HOSTPORT_LBL, SW_HIDE);

		P2PSelectionDialogProcSetModee(hDlg, IDC_ULIST, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_CONNECT, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_ULIST, SW_SHOW);		
		P2PSelectionDialogProcSetModee(hDlg, IDC_ADD, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_EDIT, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_DELETE, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_IP, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_PIPL, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_STOREDL, SW_SHOW);
		//P2PSelectionDialogProcSetModee(hDlg, BTN_SSRVWG, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_GAMEL, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_GAME, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_HOST_CODE, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_HOST, SW_HIDE);
		

	} else {
		P2PSelectionDialogProcSetModee(hDlg, IDC_PORT, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_HOSTPORT_LBL, SW_SHOW);


		P2PSelectionDialogProcSetModee(hDlg, IDC_ULIST, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_CONNECT, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_ULIST, SW_HIDE);		
		P2PSelectionDialogProcSetModee(hDlg, IDC_ADD, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_EDIT, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_DELETE, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_IP, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_PIPL, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_STOREDL, SW_HIDE);
		//P2PSelectionDialogProcSetModee(hDlg, BTN_SSRVWG, SW_HIDE);
		P2PSelectionDialogProcSetModee(hDlg, IDC_GAMEL, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_GAME, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_HOST_CODE, SW_SHOW);
		P2PSelectionDialogProcSetModee(hDlg, IDC_HOST, SW_SHOW);

	}
}
///////////////////////////////////////////////
typedef struct {
	char username[32];
	char hostname[128];
}P2PHBS;

odlist<P2PHBS, 32> P2PStoredUsers;


void P2PDisplayP2PStoredUsers(){
	p2p_ui_storedusers.DeleteAllRows();
	for (int x = 0; x< P2PStoredUsers.length; x++){
		p2p_ui_storedusers.AddRow(P2PStoredUsers[x].username, x);
		p2p_ui_storedusers.FillRow(P2PStoredUsers[x].hostname,1, x);
	}
}


P2PHBS P2PHBS_temp;


LRESULT CALLBACK P2PStoredUsersModifyDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg==WM_INITDIALOG){
		InstallNoSpaceFilterForEdit(GetDlgItem(hDlg, IDC_IP));
		if (lParam){
			SetWindowText(GetDlgItem(hDlg, IDC_NAME), P2PHBS_temp.username);
			SetWindowText(GetDlgItem(hDlg, IDC_IP), P2PHBS_temp.hostname);
			SetWindowText(hDlg, "Edit");
		} else SetWindowText(hDlg, "Add");
	} else if (uMsg==WM_CLOSE){
		EndDialog(hDlg, 0);
	} else if (uMsg==WM_COMMAND){
		if (LOWORD(wParam)==IDOK){
			GetWindowText(GetDlgItem(hDlg, IDC_NAME), P2PHBS_temp.username,32);
			GetWindowText(GetDlgItem(hDlg, IDC_IP), P2PHBS_temp.hostname,128);
			EndDialog(hDlg, 1);
		} else if (LOWORD(wParam)==IDCANCEL)
			EndDialog(hDlg, 0);
	}
	return 0;
}

void P2PStoredUsersListEdit(){
	int i = p2p_ui_storedusers.SelectedRow();
	if (i>=0&&i<P2PStoredUsers.length) {
		P2PHBS_temp = P2PStoredUsers[i];
		if (DialogBoxParam(hx, (LPCTSTR)P2P_ITEM_EDIT, p2p_ui_ss_dlg, (DLGPROC)P2PStoredUsersModifyDlgProc, 1)){
			P2PStoredUsers.set(P2PHBS_temp, i);
		}
	}
	P2PDisplayP2PStoredUsers();
}

void P2PStoredUsersListAdd(){
	memset(&P2PHBS_temp, 0, sizeof(P2PHBS));
	if (DialogBoxParam(hx, (LPCTSTR)P2P_ITEM_EDIT, p2p_ui_ss_dlg, (DLGPROC)P2PStoredUsersModifyDlgProc, 0)){
		P2PStoredUsers.add(P2PHBS_temp);
	}
	P2PDisplayP2PStoredUsers();
}


void P2PStoredUsersListSelect(HWND hDlg){
	int i = p2p_ui_storedusers.SelectedRow();
	if (i>=0&&i<P2PStoredUsers.length)
		SetDlgItemText(hDlg, IDC_IP, P2PStoredUsers[i].hostname);
}

void P2PStoredUsersListDelete(){
	int i = p2p_ui_storedusers.SelectedRow();
	if (i>=0&&i<P2PStoredUsers.length)
		P2PStoredUsers.removei(i);
	P2PDisplayP2PStoredUsers();
}

void P2PLoadStoredUsersList(){
	P2PStoredUsers.clear();
	int count = nSettings::get_int("ULISTC", 0);
	for (int x=1;x<=count;x++){
		char idt[32];
		P2PHBS sx;
		wsprintf(idt, "ULUN%i", x);
		nSettings::get_str(idt,sx.username, "UserName");
		wsprintf(idt, "ULHN%i", x);
		nSettings::get_str(idt,sx.hostname, "127.0.0.1");
		P2PStoredUsers.add(sx);
	}
	P2PDisplayP2PStoredUsers();
}

void P2PSaveStoredUsersList(){
	nSettings::set_int("ULISTC",P2PStoredUsers.length);
	for (int x=0;x<P2PStoredUsers.length;x++){
		char idt[32];
		P2PHBS sx = P2PStoredUsers[x];
		wsprintf(idt, "ULUN%i", x+1);
		nSettings::set_str(idt,sx.username);
		wsprintf(idt, "ULHN%i", x+1);
		nSettings::set_str(idt,sx.hostname);
	}
}
/////////////////////////////////////////////////

void p2p_hosted_game_callback(char * game){
	if (game == NULL)
		game = (char*)"";
	strncpy(GAME, game, sizeof(GAME) - 1);
	GAME[sizeof(GAME) - 1] = 0;
	if (gamelist != 0) {
		char * xx = gamelist;
		size_t p;
		while ((p = strlen(xx)) != 0){
			if (strcmp(game, xx)==0) {
				return;
			}
			xx += p + 1;
		}
	}
	
	outpf("ERROR: Game not found on your local list");
	outpf("ERROR: Game not found on your local list");
	outpf("ERROR: Game not found on your local list");
	outpf("ERROR: Game not found on your local list");
}

/////////////////////////////////////////

LRESULT CALLBACK P2PSelectionDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_INITDIALOG:
		{

			p2p_ui_ss_dlg = hDlg;
			InstallNoSpaceFilterForEdit(GetDlgItem(hDlg, IDC_IP));

			SetWindowText(hDlg, N02_WINDOW_TITLE);
			
			nSettings::Initialize();

			SetDlgItemInt(hDlg, IDC_PORT, nSettings::get_int("IDC_PORT", 27886), false);

			nSettings::get_str("IDC_IP", IP,"127.0.0.1:27886");
			SetDlgItemText(hDlg, IDC_IP, IP);

				HWND hxx = GetDlgItem(hDlg, IDC_GAME);
				if (gamelist != 0) {
					nSettings::get_str("IDC_GAME", GAME, "");
					char * xx = gamelist;
					size_t p;
					while ((p = strlen(xx)) != 0){
						SendMessage(hxx, CB_ADDSTRING, 0, (WPARAM)xx);
						if (strcmp(GAME, xx)==0) {
							SetWindowText(hxx, GAME);
						}
						xx += p + 1;
					}
				}
					{
						DWORD xxx = 32;
						GetUserName(USERNAME, &xxx);
						char un[128];
						nSettings::get_str("IDC_USRNAME", un, USERNAME);
						strncpy(USERNAME, un, 31);
						USERNAME[31] = 0;
						SetWindowText(GetDlgItem(hDlg, IDC_USRNAME), USERNAME);
					}
	
				nTab tabb;
				tabb.handle = p2p_ui_modeseltab = GetDlgItem(hDlg, IDC_TAB1);
				tabb.AddTab("Ho&st", 0);
				tabb.AddTab("Conn&ect", 1);
			tabb.SelectTab(min(max(nSettings::get_int("IDC_TAB", 0),0),1));
			P2PSelectionDialogProcSetMode(hDlg, tabb.SelectedTab()!=0);

			p2p_ui_storedusers.handle = GetDlgItem(hDlg, IDC_ULIST);
			p2p_ui_storedusers.AddColumn("Name", 200);
			p2p_ui_storedusers.AddColumn("IP", 180);
			p2p_ui_storedusers.FullRowSelect();

				
				P2PLoadStoredUsersList();

				UpdateModeRadioButtons(hDlg);

			}
			break;
		case WM_CLOSE:
				GetWindowText(GetDlgItem(hDlg, IDC_USRNAME), USERNAME, 31);
				nSettings::set_str("IDC_USRNAME", USERNAME);
				nSettings::set_int("IDC_PORT", GetDlgItemInt(hDlg, IDC_PORT, 0, FALSE));
			GetWindowText(GetDlgItem(hDlg, IDC_GAME), GAME, 127);
		nSettings::set_str("IDC_GAME", GAME);
		GetWindowText(GetDlgItem(hDlg, IDC_IP), IP, 127);
		nSettings::set_str("IDC_IP", IP);

		nTab tabb;
		tabb.handle = p2p_ui_modeseltab;
		nSettings::set_int("IDC_TAB", tabb.SelectedTab());

		P2PSaveStoredUsersList();


		EndDialog(hDlg, 0);


		nSettings::Terminate();
		break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
			case IDC_CONNECT:
				InitializeP2PSubsystem(hDlg, false, false);
				break;
			case IDC_HOST:
				InitializeP2PSubsystem(hDlg, true, false);
				break;
			case IDC_HOST_CODE:
				InitializeP2PSubsystem(hDlg, true, true);
				break;
		case IDC_ADD:
			P2PStoredUsersListAdd();
			break;
		case IDC_EDIT:
			P2PStoredUsersListEdit();
			break;
		case BTN_SSRVWG:
			p2p_ShowWaitingGamesList();
			break;
			case IDC_DELETE:
				P2PStoredUsersListDelete();
				break;
			case RB_MODE_P2P:
				if (activate_mode(0))
					SendMessage(hDlg, WM_CLOSE, 0, 0);
				break;
			case RB_MODE_CLIENT:
				if (activate_mode(1))
					SendMessage(hDlg, WM_CLOSE, 0, 0);
				break;
			case RB_MODE_PLAYBACK:
				if (activate_mode(2))
					SendMessage(hDlg, WM_CLOSE, 0, 0);
				break;
			};
			break;
	case WM_NOTIFY:
		NMHDR * nParam = (NMHDR*)lParam;
		if (nParam->hwndFrom == p2p_ui_modeseltab && nParam->code == TCN_SELCHANGE){
			nTab tabb;
			tabb.handle = p2p_ui_modeseltab;
			P2PSelectionDialogProcSetMode(hDlg, tabb.SelectedTab()!=0);			
		}
		if (nParam->hwndFrom == p2p_ui_storedusers.handle){
			if (nParam->code == NM_CLICK) {
				P2PStoredUsersListSelect(hDlg);
			}
		}
		break;
		};
		return 0;
}

void p2p_GUI(){
	INITCOMMONCONTROLSEX icx;
	icx.dwSize = sizeof(icx);
	icx.dwICC = ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
	InitCommonControlsEx(&icx);

	HMODULE p2p_riched_hm = LoadLibrary("riched32.dll");

	DialogBox(hx, (LPCTSTR)MAIN_DIALOG, 0, (DLGPROC)P2PSelectionDialogProc);

	FreeLibrary(p2p_riched_hm);
}
