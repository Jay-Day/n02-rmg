#include "player.h"
#include "kailleraclient.h"
#include "resource.h"
#include "uihlp.h"
#include <time.h>
#include "errr.h"

static void UpdateModeRadioButtons(HWND hDlg){
	int mode = get_active_mode_index();
	if (mode < 0 || mode > 2)
		mode = 1;
	CheckRadioButton(hDlg, RB_MODE_P2P, RB_MODE_PLAYBACK, RB_MODE_P2P + mode);
}

bool player_playing = false;
static bool player_was_dropped[16] = {};

class PlayBackBufferC {
public:
	char * buffer;
	char * ptr;
	char * end;
    
	void load_bytes(void* arg_0, unsigned int arg_4) {
		if (ptr + 10 < end) {
			int p = min(arg_4, (unsigned int)(end - ptr));
			memcpy(arg_0, ptr, p);
			ptr += p;
		}
	}
	void load_str(char* arg_0, unsigned int arg_4) {
		arg_4 = min(arg_4, (unsigned int)strlen(ptr) + 1);
		arg_4 = min(arg_4, (unsigned int)(end - ptr + 1));
		load_bytes(arg_0, arg_4);
		arg_0[arg_4] = 0x00;
	}
	int load_int(){
        int x;
        load_bytes(&x,4);
        return x;
    }
    unsigned char load_char(){
        unsigned char x;
        load_bytes(&x,1);
        return x;
    }
    unsigned short load_short(){
        unsigned short x;
        load_bytes(&x,2);
        return x;
    }
	
} PlayBackBuffer;

//==============================================


//..............................................

///////////////////////////////////////////////////////////////////////////////



#define MAX_RECORDS 512
nLVw RecordsListDlg_list;
HWND RecordsListDlg;
char record_filenames[MAX_RECORDS][260];
void player_play(char * fn){
	n02_TRACE();
	//char * fn = BrowseFile(0);
	if(fn== 0)
		return;
	
	OFSTRUCT of;
	HFILE in = OpenFile(fn, &of, OF_READ);
	
	if (in == HFILE_ERROR) 
		return;
	

	long len = _llseek(in, 0, 2);
	_llseek(in, 0, 0);
	if (len < 256+16) {
		_lclose(in);
		MessageBox(RecordsListDlg, "File too short", "Error", MB_OK | MB_ICONSTOP);
		return;
	}
	
	PlayBackBuffer.buffer = (char*)malloc(len+66);
	
	_lread(in, PlayBackBuffer.buffer, len);
	
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 4;
	PlayBackBuffer.end = PlayBackBuffer.buffer + len;
	
	_lclose(in);
	
	char APPC[128];
	
	PlayBackBuffer.load_str(APPC, 128);
	
	if (strcmp(APP, APPC)!= 0) {
		char wdr[2000];
		wsprintf(wdr, "Application name mismatch.\nExpected \"%s\" but recieved \"%s\".\nUsing a different emulator for playback may cause things to behave in an unexpected manner.\nDo you want to continue?", APPC, APP);
		if (MessageBox(RecordsListDlg, wdr, "Error", MB_YESNO | MB_ICONEXCLAMATION) != IDYES) {
			free(PlayBackBuffer.buffer);
			return;
		}
	}
	
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 132;
	
	
	PlayBackBuffer.load_str(GAME,128);
	
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 264;
	
	playerno = PlayBackBuffer.load_int();
	numplayers = PlayBackBuffer.load_int();
	
	player_playing = true;
	memset(player_was_dropped, 0, sizeof(player_was_dropped));

	KSSDFA.input = KSSDFA_START_GAME;
	
	//while(player_playing)
		//Sleep(2000);
	
	//free (PlayBackBuffer.buffer);
	n02_TRACE();
}
void RecordsList_PlaySelected(){
	n02_TRACE();
	if (player_playing) return;
	int s = RecordsListDlg_list.SelectedRow();
	if (s >= 0 && s < RecordsListDlg_list.RowsCount() && s < MAX_RECORDS){
		char filename[2000];
		wsprintf(filename, ".\\records\\%s", record_filenames[s]);
		player_play(filename);
	}
}

void RecordsList_Populate_fn(char * fn, int i) {
	n02_TRACE();
	if (i < MAX_RECORDS) {
		strncpy(record_filenames[i], fn, 259);
		record_filenames[i][259] = 0;
	}
	char filename[2000];
	wsprintf(filename, ".\\records\\%s", fn);
	
	CreateDirectory(".\\records", 0);

	OFSTRUCT of;
	HFILE in = OpenFile(filename, &of, OF_READ);
	if (in == HFILE_ERROR) {
		RecordsListDlg_list.AddRow(fn, i);
		RecordsListDlg_list.FillRow("Error", 1, i);
		return;
	}
	long len = _llseek(in, 0, 2);
	_llseek(in, 0, 0);
	if (len < 300) {
		_lclose(in);
		RecordsListDlg_list.AddRow(fn, i);
		RecordsListDlg_list.FillRow("File too short", 1, i);
		return;
	}
	char* filebuf = (char*)malloc(len + 1);
	if (!filebuf) { _lclose(in); return; }
	_lread(in, filebuf, len);
	_lclose(in);
	PlayBackBuffer.buffer = filebuf;
	PlayBackBuffer.ptr = filebuf;
	PlayBackBuffer.end = filebuf + len;
	char VER[4];
	PlayBackBuffer.load_str(VER, 4);
	if (strcmp(VER, "KRC0") != 0)
		return;
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 4;
	char APPC[128];
	PlayBackBuffer.load_str(APPC, 128);
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 132;	
	PlayBackBuffer.load_str(GAME,128);
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 260;
	time_t timee = PlayBackBuffer.load_int();
	playerno = PlayBackBuffer.load_int();
	numplayers = PlayBackBuffer.load_int();
	// Col 0: Date - parse from filename (YYMMDDHHMMSS-...), fall back to header timestamp
	{
		bool parsed = false;
		// New format: 12 digits then '-'
		if (strlen(fn) > 13) {
			bool allDigits = true;
			for (int d = 0; d < 12; d++) {
				if (!isdigit(fn[d])) { allDigits = false; break; }
			}
			if (allDigits && fn[12] == '-') {
				sprintf(filename, "%c%c/%c%c/%c%c %c%c:%c%c",
					fn[0], fn[1], fn[2], fn[3], fn[4], fn[5],
					fn[6], fn[7], fn[8], fn[9]);
				parsed = true;
			}
		}
		if (!parsed) {
			tm * ecx = localtime(&timee);
			if (ecx) {
				sprintf(filename, "%02d/%02d/%02d", ecx->tm_year % 100, ecx->tm_mon + 1, ecx->tm_mday);
			} else {
				strcpy(filename, "?");
			}
		}
	}
	RecordsListDlg_list.AddRow(filename, i);

	// Col 1: Players - parse from filename (YYMMDDHHMMSS-player1-player2-game.krec)
	{
		char players[256];
		players[0] = 0;
		int nameStart = 0;
		// New format: 12 digits + '-' (13 chars before players)
		if (strlen(fn) > 13) {
			bool allDigits = true;
			for (int d = 0; d < 12; d++) {
				if (!isdigit(fn[d])) { allDigits = false; break; }
			}
			if (allDigits && fn[12] == '-') nameStart = 13;
		}
		if (nameStart > 0) {
			const char* p = fn + nameStart;
			const char* ext = strstr(fn, ".krec");
			const char* lastDash = NULL;
			for (const char* scan = p; scan < (ext ? ext : fn + strlen(fn)); scan++) {
				if (*scan == '-') lastDash = scan;
			}
			if (lastDash && lastDash > p) {
				int plen = (int)(lastDash - p);
				if (plen > 255) plen = 255;
				strncpy(players, p, plen);
				players[plen] = 0;
				char display[256];
				display[0] = 0;
				char* tok = strtok(players, "-");
				while (tok) {
					if (display[0] != 0) strcat(display, ", ");
					strcat(display, tok);
					tok = strtok(NULL, "-");
				}
				strcpy(players, display);
			} else {
				strcpy(players, "?");
			}
		} else {
			strcpy(players, "?");
		}
		RecordsListDlg_list.FillRow(players, 1, i);
	}

	// Col 2: Game name from header
	RecordsListDlg_list.FillRow(GAME, 2, i);

	// Col 3: Duration - scan records and count input frames
	{
		int frames = 0;
		char* scan = filebuf + 272; // skip header
		char* scanEnd = filebuf + len;
		while (scan + 1 < scanEnd) {
			unsigned char type = (unsigned char)*scan++;
			if (type == 0x12) {
				if (scan + 2 > scanEnd) break;
				unsigned short rlen = *(unsigned short*)scan;
				scan += 2;
				if (rlen > 0) {
					if (scan + rlen > scanEnd) break;
					scan += rlen;
				}
				frames++;
			} else if (type == 0x14) { // drop: null-terminated nick + 4 bytes
				while (scan < scanEnd && *scan != 0) scan++;
				if (scan < scanEnd) scan++; // skip null
				scan += 4; // player number
			} else if (type == 0x08) { // chat: two null-terminated strings
				while (scan < scanEnd && *scan != 0) scan++;
				if (scan < scanEnd) scan++; // skip null
				while (scan < scanEnd && *scan != 0) scan++;
				if (scan < scanEnd) scan++; // skip null
			} else {
				break; // unknown record type
			}
		}
		int totalSec = frames / 60;
		int mins = totalSec / 60;
		int secs = totalSec % 60;
		sprintf(filename, "%d:%02d", mins, secs);
		RecordsListDlg_list.FillRow(filename, 3, i);
	}

	// Col 4: Size (file size)
	if (len <= 1024) {
		wsprintf(filename, "%i B", len);
	}
	else {
		len /= 1024;
		if (len < 100) {
			sprintf(filename, "%i kB", len);
		}
		else {
			int mb = len / 1000;
			int frc = (len % 1000) / 100;
			sprintf(filename, "%i.%i MB", mb, frc);
		}
	}
	RecordsListDlg_list.FillRow(filename, 4, i);
	free(filebuf);
	n02_TRACE();
}

static int fn_compare_desc(const void *a, const void *b) {
	// Reverse strcmp so newest (highest timestamp) comes first
	return strcmp((const char*)b, (const char*)a);
}

void RecordsList_Populate(){
	n02_TRACE();
	RecordsListDlg_list.DeleteAllRows();
	CreateDirectory(".\\records", 0);

	// Collect filenames
	char collected[MAX_RECORDS][260];
	int count = 0;

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(".\\records\\*", &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && count < MAX_RECORDS) {
				strncpy(collected[count], FindFileData.cFileName, 259);
				collected[count][259] = 0;
				count++;
			}
		} while (FindNextFile(hFind, &FindFileData) != 0);
		FindClose(hFind);
	}

	// Sort descending (newest first)
	qsort(collected, count, sizeof(collected[0]), fn_compare_desc);

	for (int i = 0; i < count; i++) {
		RecordsList_Populate_fn(collected[i], i);
	}
}
void RecordsList_DeleteSelected(){
	int s = RecordsListDlg_list.SelectedRow();
	if (s >= 0 && s < RecordsListDlg_list.RowsCount() && s < MAX_RECORDS){
		char filename[2000];
		wsprintf(filename, ".\\records\\%s", record_filenames[s]);
		DeleteFile(filename);
		RecordsList_Populate();
	}
}

LRESULT CALLBACK RecordsListDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		{
			RecordsListDlg = hDlg;
			RecordsListDlg_list.handle = GetDlgItem(hDlg, LV_GLIST);
			RecordsListDlg_list.AddColumn("Date", 80);
			RecordsListDlg_list.AddColumn("Players", 160);
			RecordsListDlg_list.AddColumn("Game", 200);
			RecordsListDlg_list.AddColumn("Duration", 60);
			RecordsListDlg_list.AddColumn("Size", 60);
			RecordsListDlg_list.FullRowSelect();
			RecordsList_Populate();

			UpdateModeRadioButtons(hDlg);

		}
		break;
	case WM_CLOSE:
		EndDialog(hDlg, 0);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCREFRESH:
			{
				RecordsList_Populate();
			}
			break;
		case BTN_PLAY:
			RecordsList_PlaySelected();
			break;
		case BTN_STOP:
			player_EndGame();
			break;
		case BTN_DELETE:
			RecordsList_DeleteSelected();
			break;
		case RB_MODE_P2P:
			if (player_playing) player_EndGame();
			if (activate_mode(0))
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;
		case RB_MODE_CLIENT:
			if (player_playing) player_EndGame();
			if (activate_mode(1))
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;
		case RB_MODE_PLAYBACK:
			if (player_playing) player_EndGame();
			if (activate_mode(2))
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;
		};
		break;
	case WM_NOTIFY:
		if(((LPNMHDR)lParam)->code==NM_DBLCLK && ((LPNMHDR)lParam)->hwndFrom==RecordsListDlg_list.handle){
			RecordsList_PlaySelected();
		}
		break;
	};
	return 0;
}

void player_GUI(){
	
	INITCOMMONCONTROLSEX icx;
	icx.dwSize = sizeof(icx);
	icx.dwICC = ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
	InitCommonControlsEx(&icx);
	
	HMODULE p2p_riched_hm = LoadLibrary("riched32.dll");
	
	DialogBox(hx, (LPCTSTR)RECORDER_PLAYBACK, 0, (DLGPROC)RecordsListDlgProc);
	
	FreeLibrary(p2p_riched_hm);
}

int player_MPV(void*values,int size){
	n02_TRACE();
	if (player_playing){
		if (PlayBackBuffer.ptr + 10 < PlayBackBuffer.end) {
			char b = PlayBackBuffer.load_char();
			if (b==0x12) {
				int l = PlayBackBuffer.load_short();
				if (l < 0) {
					player_EndGame();
					return -1;
				}
				if (l > 0)
					PlayBackBuffer.load_bytes((char*)values, l);//access error
				return l;
			}
			if (b==20) {
				char playernick[100];
				PlayBackBuffer.load_str(playernick, 100);
				int pn = PlayBackBuffer.load_int();
				if (pn >= 1 && pn <= 16)
					player_was_dropped[pn - 1] = true;
				if (pn == playerno) {
					// Recording player dropped - end playback
					player_EndGame();
					return -1;
				}
				// Other player dropped - skip, continue playback
				return player_MPV(values, size);
			}
			if (b==8) {
				char nick[100];
				char msg[500];
				PlayBackBuffer.load_str(nick, 100);
				PlayBackBuffer.load_str(msg, 500);
				infos.chatReceivedCallback(nick, msg);
				return player_MPV(values, size);
			}
		} else player_EndGame();
	}
	return -1;
}
void player_EndGame(){
	n02_TRACE();
	player_playing = false;
	// Notify emulator of any players not already dropped by the recording
	for (int i = numplayers; i >= 1; i--) {
		if (i <= 16 && player_was_dropped[i - 1])
			continue;
		if (infos.clientDroppedCallback) {
			char dropname[32];
			wsprintf(dropname, "Player %d", i);
			infos.clientDroppedCallback(dropname, i);
		}
	}
	KSSDFA.input = KSSDFA_END_GAME;
	KSSDFA.state = 0;
}
bool player_SSDSTEP(){
	n02_TRACE();
	return false;
}
void player_ChatSend(char*){
	
}
bool player_RecordingEnabled(){
	return false;
}
