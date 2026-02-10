#include "kailleraclient.h"
#include <windows.h>
#include <time.h>
#include <cstdio>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include "string.h"


#include "p2p_ui.h"
#include "kaillera_ui.h"
#include "player.h"

#include "common/nThread.h"
#include "common/k_socket.h"
#include "common/nSettings.h"

#include "errr.h"
#include <limits.h>
#include <stddef.h>


#define KAILLERA_DLLEXP __declspec(dllexport) __stdcall

#define KAILLERA

#define RECORDER


HINSTANCE hx;

//void __cdecl kprintf(char * arg_0, ...) {
//	char V8[1024];
//	time_t V4 = time(0);
//	tm * ecx = localtime(&V4);
//	sprintf(V8, "%02d/%02d/%02d-%02d:%02d:%02d> %s\r\n",ecx->tm_mday,ecx->tm_mon,ecx->tm_year % 0x64,ecx->tm_hour,ecx->tm_min,ecx->tm_sec, arg_0);
//	va_list args;
//	va_start (args, arg_0);
//	vprintf (V8, args);
//	va_end (args);
//}

//void OutputHexx(const void * inb, int len, bool usespace){
//	char outbb[2000];
//	char * outb = outbb;
//	char HEXDIGITS [] = {
//		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
//	};
//	char * xx = (char*)inb;
//	for (int x = 0; x <	len; x++) {
//		int dx = *xx++;
//		int hib = (dx & 0xF0)>>4;
//		int lob = (dx & 0x0F);
//		*outb++ = HEXDIGITS[hib];
//		*outb++ = HEXDIGITS[lob];
//		if (usespace)
//			*outb++ = ' ';
//	}
//	*outb = 0;
//	printf(outbb);
//}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

kailleraInfos infos_copy;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifdef KAILLERA
typedef struct {
	void (*GUI)();
	bool (*SSDSTEP)();
	int  (*MPV)(void*,int);
	void (*ChatSend)(char*);
	void (*EndGame)();
	bool (*RecordingEnabled)();
}n02_MODULE;

n02_MODULE active_mod;

n02_MODULE mod_kaillera;
n02_MODULE mod_p2p;
n02_MODULE mod_playback;
int mod_active_no = -1;
int active_mod_index;
bool mod_rerun;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifdef RECORDER
char convert[256];
HFILE out = HFILE_ERROR;
char recording_player_names[4][32];

class RecordingBufferC {
public:
	char buffer[500];

	char* position;

	void reset(){
		position = buffer;
	}

	int len(){
		ptrdiff_t diff = position - buffer;
		if (diff < 0)
			return 0;
		if (diff > INT_MAX)
			return INT_MAX;
		return (int)diff;
	}
	void put_char(char x){
		*position++ = x;
	}

	void put_short(short x){
		*(short*)position = x;
		position += 2;
	}

	void put_bytes(char* x, int len){
		for (int t=0;t<len;t++){
			*position++ = *x++;
		}
	}
	void write(){
		_lwrite(out, buffer, len());
		reset();
	}
}RecordingBuffer;

static void close_recording() {
	if (out != HFILE_ERROR) {
		if (RecordingBuffer.len() > 0)
			RecordingBuffer.write();
		_lclose(out);
		out = HFILE_ERROR;
	}
}

int WINAPI _gameCallback(char *game, int player, int numplayers){

	close_recording();

	if (active_mod.RecordingEnabled()) {
		n02_TRACE();
		RecordingBuffer.reset();

		char FileName[2000];
		CreateDirectory("records", 0);

		// Sanitize game name
		char GN[36];
		int j = 0;
		for (int i = 0; i < (int)strlen(game) && j < 35; i++) {
			if (isalpha(game[i]) || isalnum(game[i]))
				GN[j++] = game[i];
		}
		GN[j] = 0;

		// Build filename: YYMMDDHHMMSS-player1-player2-game.krec
		time_t t = time(0);
		tm * lt = localtime(&t);
		char datePart[16];
		strftime(datePart, sizeof(datePart), "%y%m%d%H%M%S", lt);

		char playerPart[256];
		playerPart[0] = 0;
		for (int p = 0; p < numplayers && p < 4; p++) {
			if (recording_player_names[p][0] != 0) {
				char sanitized[32];
				int sj = 0;
				for (int si = 0; recording_player_names[p][si] && sj < 31; si++) {
					char c = recording_player_names[p][si];
					if (isalpha(c) || isalnum(c))
						sanitized[sj++] = c;
				}
				sanitized[sj] = 0;
				if (sj > 0) {
					strcat(playerPart, "-");
					strcat(playerPart, sanitized);
				}
			}
		}

		wsprintf(FileName,".\\records\\%s%s-%s.krec", datePart, playerPart, GN);


		for(unsigned int x=0; x < strlen(FileName); x++){
			FileName[x] = convert[FileName[x]];
		}

		//open file
		OFSTRUCT of;
		out = OpenFile(FileName, &of, OF_WRITE|OF_CREATE);


		if(out==HFILE_ERROR){
			wsprintf(FileName+1000, "recording %s failed error = %i", FileName, GetLastError());
			MessageBox(0,FileName+1000, "recording - error",0);
		}


		char GameName[128];

		strncpy(GameName, (game != NULL) ? game : "", sizeof(GameName) - 1);
		GameName[sizeof(GameName) - 1] = 0;

		_lwrite(out, "KRC0", 4);
		_lwrite(out, infos_copy.appName, 128);
		_lwrite(out, GameName, 128);
		time_t mytime = time(NULL);
		_lwrite(out, (char*)&mytime, 4);
		_lwrite(out, (char*)&player, 4);
		_lwrite(out, (char*)&numplayers, 4);
		n02_TRACE();
	}

	if (infos_copy.gameCallback)
		return infos_copy.gameCallback(game, player, numplayers);
	return 0;

}

	void WINAPI _chatReceivedCallback(char *nick, char *text){
		if (out != HFILE_ERROR) {
			RecordingBuffer.put_char(8);
			{
				size_t nickLen = strlen(nick) + 1;
				size_t textLen = strlen(text) + 1;
				int nickLenI = (nickLen > (size_t)INT_MAX) ? INT_MAX : (int)nickLen;
				int textLenI = (textLen > (size_t)INT_MAX) ? INT_MAX : (int)textLen;
				RecordingBuffer.put_bytes(nick, nickLenI);
				RecordingBuffer.put_bytes(text, textLenI);
			}
		}
		if (infos_copy.chatReceivedCallback)
			infos_copy.chatReceivedCallback(nick, text);
	}
	void WINAPI _clientDroppedCallback(char *nick, int playernb){
		if (out != HFILE_ERROR) {
			RecordingBuffer.put_char(20);
			{
				size_t nickLen = strlen(nick) + 1;
				int nickLenI = (nickLen > (size_t)INT_MAX) ? INT_MAX : (int)nickLen;
				RecordingBuffer.put_bytes(nick, nickLenI);
			}
			RecordingBuffer.put_bytes((char*)&playernb, 4);
		}
		if (infos_copy.clientDroppedCallback)
			infos_copy.clientDroppedCallback(nick, playernb);
	}




#endif
#endif

void initialize_mode_cb(HWND hDlg){
#ifdef KAILLERA
	SendMessage(hDlg, CB_ADDSTRING, 0, (WPARAM)"1. P2P");
	SendMessage(hDlg, CB_ADDSTRING, 0, (WPARAM)"2. Server");
	SendMessage(hDlg, CB_ADDSTRING, 0, (WPARAM)"3. Playback");
	SendMessage(hDlg, CB_SETCURSEL, mod_active_no, 0);
	active_mod_index = mod_active_no;
#else
	SendMessage(hDlg, CB_ADDSTRING, 0, (WPARAM)"P2P");
	SendMessage(hDlg, CB_SETCURSEL, 0, 0);
#endif
}

bool activate_mode(int mode){
#ifdef KAILLERA
	if (mode != mod_active_no) {
		mod_active_no = mode;
		active_mod_index = mode;
		mod_rerun = true;

		if (mode == 0)
			active_mod = mod_p2p;
		else if (mode == 1)
			active_mod = mod_kaillera;
		else
			active_mod = mod_playback;

		return true;
	}
#endif
	return false;
}

int get_active_mode_index(){
	return active_mod_index;
}

void loadSettings() {
	nSettings::Initialize("okai");
	active_mod_index = nSettings::get_int("AM", 1);
	nSettings::Terminate();
}

void saveSettings() {
	nSettings::Initialize("okai");
	nSettings::set_int("AM", active_mod_index);
	nSettings::Terminate();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
char * gamelist = 0;




int KSSDFAST [16] = {
	0, 0, 1, 1,
		1, 0, 1, 0,
		2, 0, 2, 0,
		3, 3, 3, 3
};

KSSDFA_ KSSDFA;


class GuiThread : public nThread {
public:
	bool running;
	void run(void) {

		running = true;

#ifdef KAILLERA

		/*

		BYTE kst[256];
		GetKeyboardState(kst);

		if (kst[VK_CAPITAL]==0)
			active_mod = mod_p2p;
		else
			active_mod = mod_kaillera;

		*/

		__try {
			mod_rerun = true;
			do {
				mod_rerun = false;
				active_mod.GUI();
			} while (mod_rerun);
		}
		__except (n02ExceptionFilterFunction(GetExceptionInformation())) {
			MessageBox(GetActiveWindow(), "Please reopen kaillera", "GUI thread exception", 0);
		}

#else
		p2p_GUI();
#endif

		KSSDFA.state = 3;
		running = false;
		//*/
	}
	void st(void * parent) {
		KSSDFA.state = create()!=0? 0:3;
	}
} gui_thread;



extern "C" {
	kailleraInfos infos;

	void KAILLERA_DLLEXP kailleraGetVersion(char *version){
		memcpy(version, "0.9", 4);
	}
	int KAILLERA_DLLEXP kailleraInit(){
		k_socket::Initialize();
		loadSettings();

#ifdef KAILLERA

		mod_playback.MPV = player_MPV;
		mod_playback.GUI = player_GUI;
		mod_playback.EndGame = player_EndGame;
		mod_playback.SSDSTEP = player_SSDSTEP;
		mod_playback.ChatSend = player_ChatSend;
		mod_playback.RecordingEnabled = player_RecordingEnabled;

		mod_kaillera.GUI = kaillera_GUI;
		mod_kaillera.SSDSTEP = kaillera_SelectServerDlgStep;
		mod_kaillera.MPV = kaillera_modify_play_values;
		mod_kaillera.ChatSend = kaillera_game_chat_send;
		mod_kaillera.EndGame = kaillera_end_game;
		mod_kaillera.RecordingEnabled = kaillera_RecordingEnabled;

		mod_p2p.GUI = p2p_GUI;
		mod_p2p.SSDSTEP = p2p_SelectServerDlgStep;
		mod_p2p.MPV = p2p_modify_play_values;
		mod_p2p.ChatSend = p2p_send_chat;
		mod_p2p.EndGame = p2p_EndGame;
		mod_p2p.RecordingEnabled = p2p_RecordingEnabled;

		activate_mode(active_mod_index);

#ifdef RECORDER


		{
			for(int x = 0 ; x < 256; x++){
				convert[x] = '_';
			}
		}
		{
			for(char x = 'A' ; x <= 'Z'; x++){
				convert[x] = x;
			}
		}
		{
			for(char x = '0' ; x <= '9'; x++){
				convert[x] = x;
			}
		}
		{
			for(char x = 'a' ; x <= 'z'; x++){
				convert[x] = x;
			}
		}
			convert['['] = '[';
			convert[0] = 0;
			convert[']'] = ']';
			convert['.'] = '.';
			convert['\\'] = '\\';
			convert['-'] = '-';



#endif



#endif

		return 0;
	}
	void KAILLERA_DLLEXP kailleraShutdown(){
		k_socket::Cleanup();
		saveSettings();
		if (gamelist != 0)
			free(gamelist);
		gamelist = 0;
#ifdef KAILLERA
#ifdef RECORDER
		close_recording();
#endif
#endif
	}
			void KAILLERA_DLLEXP kailleraSetInfos(kailleraInfos *infos_){
				infos = *infos_;
				strncpy(APP, (infos.appName != NULL) ? infos.appName : "", 127);
				APP[127] = 0;

				if (gamelist != 0)
					free(gamelist);
			gamelist = 0;

			char * xx = infos.gameList;
			size_t l = 0;
			if (xx != 0) {
				size_t p;
				while ((p=strlen(xx))!= 0){
					l += p + 1;
					xx += p + 1;
				}
				l++;
				gamelist = (char*)malloc(l);
			memcpy(gamelist, infos.gameList, l);
		}
		infos.gameList = gamelist;
		infos_copy = infos;
#ifdef KAILLERA
#ifdef RECORDER
		infos.chatReceivedCallback = _chatReceivedCallback;
		infos.clientDroppedCallback = _clientDroppedCallback;
		infos.gameCallback = _gameCallback;
#endif
#endif
	}
	void KAILLERA_DLLEXP kailleraSelectServerDialog(void* parent){
		KSSDFA.state = 0;
		KSSDFA.input = 0;

		gui_thread.st(parent);

		nThread game_thread;
		game_thread.capture();
		game_thread.sleep(1);

		if (gui_thread.running) {
			do {
				__try {
					while (KSSDFA.state != 3) {
						int prev_state = KSSDFA.state;
						KSSDFA.state = KSSDFAST[((KSSDFA.state) << 2) | KSSDFA.input];
						KSSDFA.input = 0;
#ifdef RECORDER
						if (prev_state == 2 && KSSDFA.state != 2) {
							close_recording();
						}
#endif
						if (KSSDFA.state == 2) {
							MSG message;
							while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
								TranslateMessage(&message);
								DispatchMessage(&message);
							}
						}
						else if (KSSDFA.state == 1) {
							//kprintf("call gamecallback");
							KSSDFA.state = 2;
							if (infos.gameCallback)
								infos.gameCallback(GAME, playerno, numplayers);
						}
						else if (KSSDFA.state == 0) {
#ifdef RECORDER
							close_recording();
#endif
							MSG message;
							while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
								TranslateMessage(&message);
								DispatchMessage(&message);
							}
#ifndef KAILLERA
							if (p2p_SelectServerDlgStep()) {
#else
							if (active_mod.SSDSTEP()) {
#endif
								continue;
							}
						}

						// Use MsgWaitForMultipleObjects instead of sleep to avoid
						// sluggish window dragging while still yielding CPU
						MsgWaitForMultipleObjects(0, NULL, FALSE, 1, QS_ALLINPUT);
					}
				}
				__except (n02ExceptionFilterFunction(GetExceptionInformation())) {
					KSSDFA.state = 0;
				}
			} while (KSSDFA.state != 3);
		}
	}

	int  KAILLERA_DLLEXP kailleraModifyPlayValues(void *values, int size){
		//kaillera_core_debug("x");
		if (KSSDFA.state==2) {
#ifdef KAILLERA

#ifdef RECORDER

			short siz = active_mod.MPV(values, size);

			if (out != HFILE_ERROR) {
				RecordingBuffer.put_char(0x12);
				RecordingBuffer.put_short(siz);
				RecordingBuffer.put_bytes((char*)values, siz);
				RecordingBuffer.write();
			}
			return siz;

#else
			return active_mod.MPV(values, size);
#endif

#else
			return p2p_modify_play_values(values, size);
#endif
		} else return -1;
	}
	void KAILLERA_DLLEXP kailleraChatSend(char *text){
#ifdef KAILLERA
		active_mod.ChatSend(text);
#else
		p2p_ui_chat_send(text);
#endif
	}
	void KAILLERA_DLLEXP kailleraEndGame(){
#ifdef KAILLERA
#ifdef RECORDER
		close_recording();
#endif
		active_mod.EndGame();
#else
		p2p_EndGame();
#endif

	}
};


BOOL APIENTRY DllMain (HINSTANCE hInst,
	DWORD reason,
	LPVOID){
	if(reason==DLL_PROCESS_ATTACH)
		hx = hInst;
	return TRUE;
}


extern "C" {
	__declspec(dllexport) int z00_With = 0;
	__declspec(dllexport) int z01_stupidity = 0;
	__declspec(dllexport) int z02_even = 0;
	__declspec(dllexport) int z03_the = 0;
	__declspec(dllexport) int z04_gods = 0;
	__declspec(dllexport) int z05_contend = 0;
	__declspec(dllexport) int z06_in = 0;
	__declspec(dllexport) int z07_vain = 0;
}
