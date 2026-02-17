#include "player.h"
#include "kailleraclient.h"
#include "resource.h"
#include "uihlp.h"
#include <time.h>
#include <shellapi.h>
#include "errr.h"
#include "common/nSettings.h"

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
	
	HANDLE in = CreateFile(fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (in == INVALID_HANDLE_VALUE)
		return;

	DWORD len = SetFilePointer(in, 0, NULL, FILE_END);
	SetFilePointer(in, 0, NULL, FILE_BEGIN);
	if (len < 272) {
		CloseHandle(in);
		MessageBox(RecordsListDlg, "File too short", "Error", MB_OK | MB_ICONSTOP);
		return;
	}

	PlayBackBuffer.buffer = (char*)malloc(len+66);

	DWORD bytesRead;
	ReadFile(in, PlayBackBuffer.buffer, len, &bytesRead, NULL);

	PlayBackBuffer.end = PlayBackBuffer.buffer + len;

	CloseHandle(in);

	// Detect format version
	bool isKRC1 = (memcmp(PlayBackBuffer.buffer, "KRC1", 4) == 0);
	DWORD headerSize = isKRC1 ? 400 : 272;

	if (len < headerSize) {
		free(PlayBackBuffer.buffer);
		MessageBox(RecordsListDlg, "File too short", "Error", MB_OK | MB_ICONSTOP);
		return;
	}

	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 4;

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

	// Skip player names in KRC1 - records start at offset 400
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + headerSize;

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
	if (s >= 0 && s < RecordsListDlg_list.RowsCount()){
		int idx = (int)RecordsListDlg_list.RowNo(s);
		if (idx >= 0 && idx < MAX_RECORDS){
			char filename[2000];
			wsprintf(filename, ".\\records\\%s", record_filenames[idx]);
			player_play(filename);
		}
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

	HANDLE in = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (in == INVALID_HANDLE_VALUE) {
		RecordsListDlg_list.AddRow(fn, i);
		RecordsListDlg_list.FillRow("Error", 1, i);
		return;
	}
	DWORD len = SetFilePointer(in, 0, NULL, FILE_END);
	SetFilePointer(in, 0, NULL, FILE_BEGIN);
	if (len < 300) {
		CloseHandle(in);
		RecordsListDlg_list.AddRow(fn, i);
		RecordsListDlg_list.FillRow("File too short", 1, i);
		return;
	}
	char* filebuf = (char*)malloc(len + 1);
	if (!filebuf) { CloseHandle(in); return; }
	DWORD bytesRead;
	ReadFile(in, filebuf, len, &bytesRead, NULL);
	CloseHandle(in);
	PlayBackBuffer.buffer = filebuf;
	PlayBackBuffer.ptr = filebuf;
	PlayBackBuffer.end = filebuf + len;
	char VER[5];
	memcpy(VER, filebuf, 4);
	VER[4] = 0;
	bool isKRC1 = (strcmp(VER, "KRC1") == 0);
	if (strcmp(VER, "KRC0") != 0 && !isKRC1) {
		free(filebuf);
		return;
	}
	DWORD headerSize = isKRC1 ? 400 : 272;
	if (len < headerSize) {
		free(filebuf);
		return;
	}
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 4;
	char APPC[128];
	PlayBackBuffer.load_str(APPC, 128);
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 132;	
	PlayBackBuffer.load_str(GAME,128);
	PlayBackBuffer.ptr = PlayBackBuffer.buffer + 260;
	time_t timee = PlayBackBuffer.load_int();
	playerno = PlayBackBuffer.load_int();
	numplayers = PlayBackBuffer.load_int();
	// Col 0: Date - KRC1 uses header timestamp, KRC0 parses filename with header fallback
	{
		bool parsed = false;
		if (!isKRC1 && strlen(fn) > 13) {
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
				sprintf(filename, "%02d/%02d/%02d %02d:%02d", ecx->tm_year % 100, ecx->tm_mon + 1, ecx->tm_mday, ecx->tm_hour, ecx->tm_min);
			} else {
				strcpy(filename, "?");
			}
		}
	}
	RecordsListDlg_list.AddRow(filename, i);

	// Col 1: Players - read from header (KRC1) or parse filename (KRC0)
	{
		char players[256];
		players[0] = 0;
		if (isKRC1) {
			// Read player names from header offsets 272-399 (4 x 32 bytes)
			for (int p = 0; p < numplayers && p < 4; p++) {
				char name[33];
				memcpy(name, filebuf + 272 + p * 32, 32);
				name[32] = 0;
				if (name[0] == 0) continue;
				if (players[0] != 0) strcat(players, ", ");
				strncat(players, name, 255 - strlen(players));
			}
			if (players[0] == 0) strcpy(players, "?");
		} else {
			int nameStart = 0;
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
		}
		RecordsListDlg_list.FillRow(players, 1, i);
	}

	// Col 2: Game name from header
	RecordsListDlg_list.FillRow(GAME, 2, i);

	// Col 3: Duration - scan records and count input frames
	{
		int frames = 0;
		char* scan = filebuf + headerSize; // skip header
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
		if (len < 1000) {
			sprintf(filename, "%i kB", len);
		}
		else {
			int mb = len / 1000;
			int frc = (len % 1000) / 100;
			sprintf(filename, "%i.%i MB", mb, frc);
		}
	}
	RecordsListDlg_list.FillRow(filename, 4, i);

	// Col 5: Filename
	RecordsListDlg_list.FillRow(fn, 5, i);
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
	if (s >= 0 && s < RecordsListDlg_list.RowsCount()){
		int idx = (int)RecordsListDlg_list.RowNo(s);
		if (idx >= 0 && idx < MAX_RECORDS){
			char filename[2000];
			wsprintf(filename, ".\\records\\%s", record_filenames[idx]);
			DeleteFile(filename);
			RecordsList_Populate();
		}
	}
}

#define PB_NUM_COLS 6
#define IDM_COL_TOGGLE 40100

static const char* colKeys[PB_NUM_COLS] = { "PBColDate", "PBColPlayers", "PBColGame", "PBColDuration", "PBColSize", "PBColFilename" };
static const char* colVisKeys[PB_NUM_COLS] = { "PBVisDate", "PBVisPlayers", "PBVisGame", "PBVisDuration", "PBVisSize", "PBVisFilename" };
static const int colDefaults[PB_NUM_COLS] = { 80, 160, 200, 60, 60, 150 };
static const char* colNames[PB_NUM_COLS] = { "Date", "Players", "Game", "Duration", "Size", "Filename" };
static bool col_visible[PB_NUM_COLS];
static int col_saved_width[PB_NUM_COLS];

static int sort_column = -1;
static bool sort_ascending = true;

static int CALLBACK RecordsList_CompareItems(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	char text1[256], text2[256];
	ListView_GetItemText(RecordsListDlg_list.handle, (int)lParam1, sort_column, text1, sizeof(text1));
	ListView_GetItemText(RecordsListDlg_list.handle, (int)lParam2, sort_column, text2, sizeof(text2));
	int result = strcmp(text1, text2);
	return sort_ascending ? result : -result;
}

static void SavePlaybackState(HWND hDlg) {
	for (int i = 0; i < PB_NUM_COLS; i++) {
		int w = ListView_GetColumnWidth(RecordsListDlg_list.handle, i);
		if (w > 0) {
			nSettings::set_int((char*)colKeys[i], w);
			col_saved_width[i] = w;
		}
		nSettings::set_int((char*)colVisKeys[i], col_visible[i] ? 1 : 0);
	}
	RECT rc;
	GetWindowRect(hDlg, &rc);
	nSettings::set_int((char*)"PBWinW", rc.right - rc.left);
	nSettings::set_int((char*)"PBWinH", rc.bottom - rc.top);
}

static void ShowColumnContextMenu(HWND hDlg) {
	HMENU hMenu = CreatePopupMenu();
	for (int i = 0; i < PB_NUM_COLS; i++) {
		UINT flags = MF_STRING;
		if (col_visible[i]) flags |= MF_CHECKED;
		AppendMenu(hMenu, flags, IDM_COL_TOGGLE + i, colNames[i]);
	}
	POINT pt;
	GetCursorPos(&pt);
	int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hDlg, NULL);
	DestroyMenu(hMenu);
	if (cmd >= IDM_COL_TOGGLE && cmd < IDM_COL_TOGGLE + PB_NUM_COLS) {
		int col = cmd - IDM_COL_TOGGLE;
		col_visible[col] = !col_visible[col];
		if (col_visible[col]) {
			int w = col_saved_width[col];
			if (w < 20) w = colDefaults[col];
			ListView_SetColumnWidth(RecordsListDlg_list.handle, col, w);
		} else {
			col_saved_width[col] = ListView_GetColumnWidth(RecordsListDlg_list.handle, col);
			if (col_saved_width[col] < 20) col_saved_width[col] = colDefaults[col];
			ListView_SetColumnWidth(RecordsListDlg_list.handle, col, 0);
		}
	}
}

static int lv_margin_left, lv_margin_top, lv_margin_right, lv_margin_bottom;

static void ResizeListView(HWND hDlg) {
	RECT rc;
	GetClientRect(hDlg, &rc);
	int x = lv_margin_left;
	int y = lv_margin_top;
	int w = rc.right - lv_margin_left - lv_margin_right;
	int h = rc.bottom - lv_margin_top - lv_margin_bottom;
	if (w < 50) w = 50;
	if (h < 50) h = 50;
	MoveWindow(RecordsListDlg_list.handle, x, y, w, h, TRUE);
}

LRESULT CALLBACK RecordsListDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		{
			RecordsListDlg = hDlg;
			RecordsListDlg_list.initialize();
			RecordsListDlg_list.handle = GetDlgItem(hDlg, LV_GLIST);

			// Calculate ListView margins from initial layout
			{
				RECT dlgRc, lvRc;
				GetClientRect(hDlg, &dlgRc);
				GetWindowRect(RecordsListDlg_list.handle, &lvRc);
				POINT lvPos = { lvRc.left, lvRc.top };
				ScreenToClient(hDlg, &lvPos);
				lv_margin_left = lvPos.x;
				lv_margin_top = lvPos.y;
				lv_margin_right = dlgRc.right - (lvPos.x + (lvRc.right - lvRc.left));
				lv_margin_bottom = dlgRc.bottom - (lvPos.y + (lvRc.bottom - lvRc.top));
			}

			// Restore saved window size
			{
				int w = nSettings::get_int((char*)"PBWinW", 0);
				int h = nSettings::get_int((char*)"PBWinH", 0);
				if (w > 200 && h > 150) {
					SetWindowPos(hDlg, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
					ResizeListView(hDlg);
				}
			}

			for (int i = 0; i < PB_NUM_COLS; i++) {
				col_visible[i] = nSettings::get_int((char*)colVisKeys[i], 1) != 0;
				col_saved_width[i] = nSettings::get_int((char*)colKeys[i], colDefaults[i]);
				if (col_saved_width[i] < 20) col_saved_width[i] = colDefaults[i];
				int w = col_visible[i] ? col_saved_width[i] : 0;
				RecordsListDlg_list.AddColumn((char*)colNames[i], w);
			}

			RecordsListDlg_list.FullRowSelect();
			RecordsList_Populate();

			// Default sort: date descending (newest first)
			sort_column = 0;
			sort_ascending = false;
			ListView_SortItemsEx(RecordsListDlg_list.handle, RecordsList_CompareItems, 0);

			UpdateModeRadioButtons(hDlg);

		}
		break;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
			ResizeListView(hDlg);
		break;
	case WM_CLOSE:
		SavePlaybackState(hDlg);
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
		case BTN_OPENFOLDER:
			{
				CreateDirectory(".\\records", 0);
				ShellExecute(hDlg, "open", ".\\records", NULL, NULL, SW_SHOWNORMAL);
			}
			break;
		case RB_MODE_P2P:
			if (player_playing) player_EndGame();
			SavePlaybackState(hDlg);
			if (activate_mode(0))
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;
		case RB_MODE_CLIENT:
			if (player_playing) player_EndGame();
			SavePlaybackState(hDlg);
			if (activate_mode(1))
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;
		case RB_MODE_PLAYBACK:
			if (player_playing) player_EndGame();
			SavePlaybackState(hDlg);
			if (activate_mode(2))
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;
		};
		break;
	case WM_NOTIFY:
		if(((LPNMHDR)lParam)->code==NM_DBLCLK && ((LPNMHDR)lParam)->hwndFrom==RecordsListDlg_list.handle){
			RecordsList_PlaySelected();
		}
		if(((LPNMHDR)lParam)->code==NM_RCLICK && ((LPNMHDR)lParam)->hwndFrom==ListView_GetHeader(RecordsListDlg_list.handle)){
			ShowColumnContextMenu(hDlg);
		}
		if(((LPNMHDR)lParam)->code==LVN_COLUMNCLICK && ((LPNMHDR)lParam)->hwndFrom==RecordsListDlg_list.handle){
			NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
			if (pnmv->iSubItem == sort_column) {
				sort_ascending = !sort_ascending;
			} else {
				sort_column = pnmv->iSubItem;
				sort_ascending = true;
			}
			ListView_SortItemsEx(RecordsListDlg_list.handle, RecordsList_CompareItems, 0);
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
