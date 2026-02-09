#include "kaillera_ui.h"

#include <windows.h>
#include "uihlp.h"
#include "stats.h"

#include "resource.h"
#include "kailleraclient.h"
#include "common/nSTL.h"
#include "common/k_socket.h"
#include "common/nThread.h"
#include "common/nSettings.h"

static bool IsNonGameLobbyName(const char* name);

bool KAILLERA_CORE_INITIALIZED = false;
bool inGame = false;


//extern int PACKETMISOTDERCOUNT;


bool kaillera_SelectServerDlgStep(){
	if (KAILLERA_CORE_INITIALIZED) {
		kaillera_step();
		return true;
	}
	Sleep(100);
	return true;
	return false;
}

char * CONNECTION_TYPES [] = 
{
	"",
	"LAN",
	"Exc",
	"Good",
	"Avg",
	"Low",
	"Bad"
};

char * USER_STATUS [] =
{
	"Playing",
	"Idle",
	"Playing"
};
char * GAME_STATUS [] = 
{
	"Waiting",
	"Playing",
	"Playing",
};

//===========================================================================
//===========================================================================
//===========================================================================
//===========================================================================
HWND kaillera_sdlg;
char kaillera_sdlg_NAME[128];
int kaillera_sdlg_port;
char kaillera_sdlg_ip[128];
nLVw kaillera_sdlg_gameslv;
nLVw kaillera_sdlg_userslv;
HWND kaillera_sdlg_partchat;



HWND kaillera_sdlg_CHK_REC;
HWND kaillera_sdlg_RE_GCHAT;
HWND kaillera_sdlg_TXT_GINP;
nLVw kaillera_sdlg_LV_GULIST;
HWND kaillera_sdlg_BTN_START;
HWND kaillera_sdlg_BTN_DROP;
HWND kaillera_sdlg_BTN_LEAVE;
HWND kaillera_sdlg_BTN_KICK;
HWND kaillera_sdlg_BTN_LAGSTAT;
HWND kaillera_sdlg_BTN_OPTIONS;
HWND kaillera_sdlg_BTN_ADVERTISE;
HWND kaillera_sdlg_ST_SPEED;
HWND kaillera_sdlg_ST_DELAY;
HWND kaillera_sdlg_BTN_GCHAT;
HWND kaillera_sdlg_MINGUIUPDATE;
HWND kaillera_sdlg_TXT_MSG;
HWND kaillera_sdlg_JOINMSG_LBL;
UINT_PTR kaillera_sdlg_sipd_timer;
int kaillera_sdlg_frameno = 0;
int kaillera_sdlg_pps = 0;
int kaillera_sdlg_delay = -1;
int kaillera_spoof_ping = 0;  // 0 = auto (no spoofing), >0 = spoof ping in ms
int kaillera_30fps_mode = 0;  // 0 = normal, 1 = halve delay for 30fps ROMs
bool MINGUIUPDATE;
bool hosting = false;
bool kaillera_sdlg_toggle = false;

static int g_flash_on_user_join = 0;
static int g_beep_on_user_join = 1;
static bool g_join_message_host_context = false;

enum {
	MENU_ID_USERS_COL_BASE = 20000,
	MENU_ID_LOBBY_COL_BASE = 20100,
	MENU_ID_GAMES_COL_BASE = 20200
};

struct KailleraColumnMenuItem {
	UINT id;
	const char* title;
	int defaultWidth;
};

static KailleraColumnMenuItem g_users_column_menu[] = {
	{ MENU_ID_USERS_COL_BASE + 0, "Name", 80 },
	{ MENU_ID_USERS_COL_BASE + 1, "Ping", 35 },
	{ MENU_ID_USERS_COL_BASE + 2, "UID", 44 },
	{ MENU_ID_USERS_COL_BASE + 3, "Status", 85 },
	{ MENU_ID_USERS_COL_BASE + 4, "Connection", 55 },
};

static KailleraColumnMenuItem g_lobby_column_menu[] = {
	{ MENU_ID_LOBBY_COL_BASE + 0, "Nick", 100 },
	{ MENU_ID_LOBBY_COL_BASE + 1, "Ping", 60 },
	{ MENU_ID_LOBBY_COL_BASE + 2, "Connection", 60 },
	{ MENU_ID_LOBBY_COL_BASE + 3, "Delay", 120 },
};

static KailleraColumnMenuItem g_games_column_menu[] = {
	{ MENU_ID_GAMES_COL_BASE + 0, "Game", 285 },
	{ MENU_ID_GAMES_COL_BASE + 1, "GameID", 60 },
	{ MENU_ID_GAMES_COL_BASE + 2, "Emulator", 130 },
	{ MENU_ID_GAMES_COL_BASE + 3, "User", 150 },
	{ MENU_ID_GAMES_COL_BASE + 4, "Status", 50 },
	{ MENU_ID_GAMES_COL_BASE + 5, "Users", 45 },
};

static int g_users_column_restore_widths[5] = { 80, 35, 44, 85, 55 };
static int g_lobby_column_restore_widths[4] = { 100, 60, 60, 120 };
static int g_games_column_restore_widths[6] = { 285, 60, 130, 150, 50, 45 };
static const int KAILLERA_MIN_COLUMN_WIDTH = 16;
static bool g_kaillera_allow_zero_width_columns = false;
static bool g_hidden_column_track_active = false;
static HWND g_hidden_column_track_list = NULL;
static int g_hidden_column_track_item = -1;
static int g_hidden_column_track_prev_item = -1;
static int g_hidden_column_track_start_prev_width = 0;
static int g_hidden_column_track_start_cursor_x = 0;

static void ExecuteOptions();
static void ApplyKailleraDialogResizeLayout(HWND hDlg, int clientWidth, int clientHeight);
static void SyncColumnRestoreWidths(HWND listHandle, int* restoreWidths, KailleraColumnMenuItem* columns, int count);
static int FindPreviousVisibleColumn(HWND listHandle, int fromColumn);

enum {
	KAILLERA_ANCHOR_LEFT = 1 << 0,
	KAILLERA_ANCHOR_RIGHT = 1 << 1,
	KAILLERA_ANCHOR_TOP = 1 << 2,
	KAILLERA_ANCHOR_BOTTOM = 1 << 3
};

struct KailleraResizeItem {
	int id;
	unsigned int anchors;
	RECT baseRect;
};

static KailleraResizeItem g_kaillera_resize_items[] = {
	{ RE_PART, KAILLERA_ANCHOR_LEFT | KAILLERA_ANCHOR_TOP | KAILLERA_ANCHOR_RIGHT, { 0, 0, 0, 0 } },
	{ LV_ULIST, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_TOP, { 0, 0, 0, 0 } },
	{ TXT_CHAT, KAILLERA_ANCHOR_LEFT | KAILLERA_ANCHOR_TOP | KAILLERA_ANCHOR_RIGHT, { 0, 0, 0, 0 } },
	{ IDC_CHAT, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_TOP, { 0, 0, 0, 0 } },
	{ IDC_CREATE, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_TOP, { 0, 0, 0, 0 } },
	{ LV_GLIST, KAILLERA_ANCHOR_LEFT | KAILLERA_ANCHOR_TOP | KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ RE_GCHAT, KAILLERA_ANCHOR_LEFT | KAILLERA_ANCHOR_TOP | KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ LV_GULIST, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_TOP | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ TXT_GINP, KAILLERA_ANCHOR_LEFT | KAILLERA_ANCHOR_BOTTOM | KAILLERA_ANCHOR_RIGHT, { 0, 0, 0, 0 } },
	{ BTN_GCHAT, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ BTN_START, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_TOP, { 0, 0, 0, 0 } },
	{ BTN_DROP, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_TOP, { 0, 0, 0, 0 } },
	{ BTN_LEAVE, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_TOP, { 0, 0, 0, 0 } },
	{ BTN_KICK, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_TOP, { 0, 0, 0, 0 } },
	{ BTN_LAGSTAT, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_TOP, { 0, 0, 0, 0 } },
	{ CHK_REC, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ ST_SPEED, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ ST_DELAY, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ BTN_OPTIONS, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ BTN_ADVERTISE, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ IDC_JOINMSG_LBL, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
	{ TXT_MSG, KAILLERA_ANCHOR_RIGHT | KAILLERA_ANCHOR_BOTTOM, { 0, 0, 0, 0 } },
};

static SIZE g_kaillera_base_client_size = { 0, 0 };
static SIZE g_kaillera_min_track_size = { 0, 0 };
static bool g_kaillera_resize_initialized = false;
static int g_kaillera_top_right_width = 0;
static int g_kaillera_top_right_margin = 0;
static int g_kaillera_top_gap_repart_to_list = 0;
static int g_kaillera_top_gap_txtchat_to_chat = 0;
static int g_kaillera_top_gap_chat_to_create = 0;
static int g_kaillera_top_gap_create_to_list = 0;
static int g_kaillera_bottom_right_width = 0;
static int g_kaillera_bottom_right_margin = 0;
static int g_kaillera_bottom_gap_regchat_to_list = 0;
static int g_kaillera_bottom_gap_txtginp_to_btn = 0;
static int g_kaillera_bottom_gap_btn_to_list = 0;
static const int KAILLERA_SPLITTER_GRAB_WIDTH = 4;

enum KailleraSplitterDragMode {
	KAILLERA_SPLITTER_NONE = 0,
	KAILLERA_SPLITTER_TOP = 1,
	KAILLERA_SPLITTER_BOTTOM = 2
};

static KailleraSplitterDragMode g_kaillera_active_splitter = KAILLERA_SPLITTER_NONE;

static int ClampInt(int value, int minimum, int maximum) {
	if (value < minimum)
		return minimum;
	if (value > maximum)
		return maximum;
	return value;
}

static KailleraResizeItem* FindKailleraResizeItem(int controlId) {
	const size_t itemCount = sizeof(g_kaillera_resize_items) / sizeof(g_kaillera_resize_items[0]);
	for (size_t i = 0; i < itemCount; ++i) {
		if (g_kaillera_resize_items[i].id == controlId)
			return &g_kaillera_resize_items[i];
	}
	return NULL;
}

static int GetTopRightPanelWidthForClient(int clientWidth) {
	const int minRightWidth = 120;
	int maxRightWidth = clientWidth - g_kaillera_top_right_margin - 140;
	if (maxRightWidth < minRightWidth)
		maxRightWidth = minRightWidth;
	return ClampInt(g_kaillera_top_right_width, minRightWidth, maxRightWidth);
}

static int GetTopSplitterXForClient(int clientWidth) {
	const int rightWidth = GetTopRightPanelWidthForClient(clientWidth);
	return clientWidth - g_kaillera_top_right_margin - rightWidth;
}

static int GetBottomRightPanelWidthForClient(int clientWidth) {
	const int minRightWidth = 120;
	int maxRightWidth = clientWidth - g_kaillera_bottom_right_margin - 140;
	if (maxRightWidth < minRightWidth)
		maxRightWidth = minRightWidth;
	return ClampInt(g_kaillera_bottom_right_width, minRightWidth, maxRightWidth);
}

static int GetBottomSplitterXForClient(int clientWidth) {
	const int rightWidth = GetBottomRightPanelWidthForClient(clientWidth);
	return clientWidth - g_kaillera_bottom_right_margin - rightWidth;
}

static bool IsTopSplitterHit(HWND hDlg, int mouseX, int mouseY) {
	RECT clientRect;
	if (!GetClientRect(hDlg, &clientRect))
		return false;

	KailleraResizeItem* rePartItem = FindKailleraResizeItem(RE_PART);
	KailleraResizeItem* txtChatItem = FindKailleraResizeItem(TXT_CHAT);
	if (rePartItem == NULL || txtChatItem == NULL)
		return false;

	const int splitterX = GetTopSplitterXForClient(clientRect.right - clientRect.left);
	const int hotTop = rePartItem->baseRect.top;
	const int hotBottom = txtChatItem->baseRect.bottom;
	return mouseX >= splitterX - KAILLERA_SPLITTER_GRAB_WIDTH &&
		mouseX <= splitterX + KAILLERA_SPLITTER_GRAB_WIDTH &&
		mouseY >= hotTop &&
		mouseY <= hotBottom;
}

static bool IsBottomSplitterHit(HWND hDlg, int mouseX, int mouseY) {
	RECT clientRect;
	if (!GetClientRect(hDlg, &clientRect))
		return false;

	KailleraResizeItem* gameChatItem = FindKailleraResizeItem(RE_GCHAT);
	KailleraResizeItem* gameInputItem = FindKailleraResizeItem(TXT_GINP);
	if (gameChatItem == NULL || gameInputItem == NULL)
		return false;

	const int clientWidth = clientRect.right - clientRect.left;
	const int clientHeight = clientRect.bottom - clientRect.top;
	const int deltaY = clientHeight - g_kaillera_base_client_size.cy;
	const int splitterX = GetBottomSplitterXForClient(clientWidth);
	const int hotTop = gameChatItem->baseRect.top;
	const int hotBottom = gameInputItem->baseRect.bottom + deltaY;
	return mouseX >= splitterX - KAILLERA_SPLITTER_GRAB_WIDTH &&
		mouseX <= splitterX + KAILLERA_SPLITTER_GRAB_WIDTH &&
		mouseY >= hotTop &&
		mouseY <= hotBottom;
}

static void ApplyTopSectionSplitterLayout(HWND hDlg, int clientWidth) {
	KailleraResizeItem* rePartItem = FindKailleraResizeItem(RE_PART);
	KailleraResizeItem* usersListItem = FindKailleraResizeItem(LV_ULIST);
	KailleraResizeItem* txtChatItem = FindKailleraResizeItem(TXT_CHAT);
	KailleraResizeItem* chatBtnItem = FindKailleraResizeItem(IDC_CHAT);
	KailleraResizeItem* createBtnItem = FindKailleraResizeItem(IDC_CREATE);
	if (rePartItem == NULL || usersListItem == NULL || txtChatItem == NULL || chatBtnItem == NULL || createBtnItem == NULL)
		return;

	const int rightPanelWidth = GetTopRightPanelWidthForClient(clientWidth);
	g_kaillera_top_right_width = rightPanelWidth;

	const int usersListLeft = clientWidth - g_kaillera_top_right_margin - rightPanelWidth;
	const int usersListTop = usersListItem->baseRect.top;
	const int usersListHeight = usersListItem->baseRect.bottom - usersListItem->baseRect.top;
	SetWindowPos(GetDlgItem(hDlg, LV_ULIST), NULL, usersListLeft, usersListTop, rightPanelWidth, usersListHeight,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);

	const int rePartLeft = rePartItem->baseRect.left;
	const int rePartTop = rePartItem->baseRect.top;
	int rePartRight = usersListLeft - g_kaillera_top_gap_repart_to_list;
	if (rePartRight < rePartLeft + 80)
		rePartRight = rePartLeft + 80;
	SetWindowPos(GetDlgItem(hDlg, RE_PART), NULL, rePartLeft, rePartTop,
		rePartRight - rePartLeft, rePartItem->baseRect.bottom - rePartTop,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);

	const int createWidth = createBtnItem->baseRect.right - createBtnItem->baseRect.left;
	const int chatWidth = chatBtnItem->baseRect.right - chatBtnItem->baseRect.left;
	const int createLeft = usersListLeft - g_kaillera_top_gap_create_to_list - createWidth;
	const int chatLeft = createLeft - g_kaillera_top_gap_chat_to_create - chatWidth;
	int txtChatRight = chatLeft - g_kaillera_top_gap_txtchat_to_chat;
	if (txtChatRight < txtChatItem->baseRect.left + 80)
		txtChatRight = txtChatItem->baseRect.left + 80;

	SetWindowPos(GetDlgItem(hDlg, IDC_CREATE), NULL, createLeft, createBtnItem->baseRect.top,
		createWidth, createBtnItem->baseRect.bottom - createBtnItem->baseRect.top,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
	SetWindowPos(GetDlgItem(hDlg, IDC_CHAT), NULL, chatLeft, chatBtnItem->baseRect.top,
		chatWidth, chatBtnItem->baseRect.bottom - chatBtnItem->baseRect.top,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
	SetWindowPos(GetDlgItem(hDlg, TXT_CHAT), NULL, txtChatItem->baseRect.left, txtChatItem->baseRect.top,
		txtChatRight - txtChatItem->baseRect.left, txtChatItem->baseRect.bottom - txtChatItem->baseRect.top,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
}

static void ApplyBottomSectionSplitterLayout(HWND hDlg, int clientWidth, int deltaY) {
	KailleraResizeItem* gameChatItem = FindKailleraResizeItem(RE_GCHAT);
	KailleraResizeItem* gameUsersItem = FindKailleraResizeItem(LV_GULIST);
	KailleraResizeItem* gameInputItem = FindKailleraResizeItem(TXT_GINP);
	KailleraResizeItem* gameChatBtnItem = FindKailleraResizeItem(BTN_GCHAT);
	if (gameChatItem == NULL || gameUsersItem == NULL || gameInputItem == NULL || gameChatBtnItem == NULL)
		return;

	const int rightPanelWidth = GetBottomRightPanelWidthForClient(clientWidth);
	g_kaillera_bottom_right_width = rightPanelWidth;

	const int usersLeft = clientWidth - g_kaillera_bottom_right_margin - rightPanelWidth;
	const int usersTop = gameUsersItem->baseRect.top;
	const int usersHeight = (gameUsersItem->baseRect.bottom - gameUsersItem->baseRect.top) + deltaY;
	SetWindowPos(GetDlgItem(hDlg, LV_GULIST), NULL, usersLeft, usersTop, rightPanelWidth, usersHeight,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);

	const int gameChatLeft = gameChatItem->baseRect.left;
	const int gameChatTop = gameChatItem->baseRect.top;
	int gameChatRight = usersLeft - g_kaillera_bottom_gap_regchat_to_list;
	if (gameChatRight < gameChatLeft + 80)
		gameChatRight = gameChatLeft + 80;
	const int gameChatHeight = (gameChatItem->baseRect.bottom - gameChatItem->baseRect.top) + deltaY;
	SetWindowPos(GetDlgItem(hDlg, RE_GCHAT), NULL, gameChatLeft, gameChatTop,
		gameChatRight - gameChatLeft, gameChatHeight,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);

	const int chatBtnWidth = gameChatBtnItem->baseRect.right - gameChatBtnItem->baseRect.left;
	const int chatBtnHeight = gameChatBtnItem->baseRect.bottom - gameChatBtnItem->baseRect.top;
	const int chatBtnLeft = usersLeft - g_kaillera_bottom_gap_btn_to_list - chatBtnWidth;
	const int chatBtnTop = gameChatBtnItem->baseRect.top + deltaY;
	SetWindowPos(GetDlgItem(hDlg, BTN_GCHAT), NULL, chatBtnLeft, chatBtnTop, chatBtnWidth, chatBtnHeight,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);

	const int gameInputTop = gameInputItem->baseRect.top + deltaY;
	int gameInputRight = chatBtnLeft - g_kaillera_bottom_gap_txtginp_to_btn;
	if (gameInputRight < gameInputItem->baseRect.left + 80)
		gameInputRight = gameInputItem->baseRect.left + 80;
	SetWindowPos(GetDlgItem(hDlg, TXT_GINP), NULL, gameInputItem->baseRect.left, gameInputTop,
		gameInputRight - gameInputItem->baseRect.left, gameInputItem->baseRect.bottom - gameInputItem->baseRect.top,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
}

static void ApplyJoinMessageLayout(HWND hDlg, int deltaY) {
	KailleraResizeItem* joinLabelItem = FindKailleraResizeItem(IDC_JOINMSG_LBL);
	KailleraResizeItem* joinMsgItem = FindKailleraResizeItem(TXT_MSG);
	if (joinLabelItem == NULL || joinMsgItem == NULL)
		return;

	RECT gameUsersRect;
	HWND gameUsersList = GetDlgItem(hDlg, LV_GULIST);
	if (gameUsersList == NULL || !GetWindowRect(gameUsersList, &gameUsersRect))
		return;
	MapWindowPoints(HWND_DESKTOP, hDlg, (LPPOINT)&gameUsersRect, 2);

	const int joinOffset = joinMsgItem->baseRect.left - joinLabelItem->baseRect.left;
	int joinMsgWidth = (gameUsersRect.right - gameUsersRect.left) - joinOffset;
	if (joinMsgWidth < 40)
		joinMsgWidth = 40;

	const int joinLabelWidth = joinLabelItem->baseRect.right - joinLabelItem->baseRect.left;
	const int joinLabelHeight = joinLabelItem->baseRect.bottom - joinLabelItem->baseRect.top;
	const int joinMsgHeight = joinMsgItem->baseRect.bottom - joinMsgItem->baseRect.top;

	SetWindowPos(GetDlgItem(hDlg, IDC_JOINMSG_LBL), NULL,
		gameUsersRect.left, joinLabelItem->baseRect.top + deltaY,
		joinLabelWidth, joinLabelHeight,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
	SetWindowPos(GetDlgItem(hDlg, TXT_MSG), NULL,
		gameUsersRect.left + joinOffset, joinMsgItem->baseRect.top + deltaY,
		joinMsgWidth, joinMsgHeight,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
}

static void SaveListViewColumnWidths(HWND listHandle, const char* keyPrefix, int count) {
	if (listHandle == NULL || keyPrefix == NULL || count <= 0)
		return;

	for (int i = 0; i < count; ++i) {
		char key[64];
		wsprintf(key, "%s%i", keyPrefix, i);
		nSettings::set_int(key, ListView_GetColumnWidth(listHandle, i));
	}
}

static void LoadListViewColumnWidths(HWND listHandle, const char* keyPrefix, int count) {
	if (listHandle == NULL || keyPrefix == NULL || count <= 0)
		return;

	g_kaillera_allow_zero_width_columns = true;
	for (int i = 0; i < count; ++i) {
		char key[64];
		wsprintf(key, "%s%i", keyPrefix, i);
		int savedWidth = nSettings::get_int(key, -1);
		if (savedWidth > 0 && savedWidth < KAILLERA_MIN_COLUMN_WIDTH)
			savedWidth = KAILLERA_MIN_COLUMN_WIDTH;
		if (savedWidth >= 0)
			ListView_SetColumnWidth(listHandle, i, savedWidth);
	}
	g_kaillera_allow_zero_width_columns = false;
}

static void SaveKailleraDialogLayout(HWND hDlg) {
	if (hDlg == NULL || !g_kaillera_resize_initialized)
		return;

	WINDOWPLACEMENT wp;
	memset(&wp, 0, sizeof(wp));
	wp.length = sizeof(wp);
	if (GetWindowPlacement(hDlg, &wp)) {
		const int w = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
		const int h = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
		if (w > 100 && h > 100) {
			nSettings::set_int((char*)"KSDLG_X", wp.rcNormalPosition.left);
			nSettings::set_int((char*)"KSDLG_Y", wp.rcNormalPosition.top);
			nSettings::set_int((char*)"KSDLG_W", w);
			nSettings::set_int((char*)"KSDLG_H", h);
		}
	}

	nSettings::set_int((char*)"KSDLG_TOPRW", g_kaillera_top_right_width);
	nSettings::set_int((char*)"KSDLG_BOTRW", g_kaillera_bottom_right_width);

	SaveListViewColumnWidths(GetDlgItem(hDlg, LV_ULIST), "KSDLG_UCOL_", 5);
	SaveListViewColumnWidths(GetDlgItem(hDlg, LV_GLIST), "KSDLG_GCOL_", 6);
	SaveListViewColumnWidths(GetDlgItem(hDlg, LV_GULIST), "KSDLG_PCOL_", 4);
}

static void LoadKailleraDialogLayout(HWND hDlg) {
	if (hDlg == NULL || !g_kaillera_resize_initialized)
		return;

	const int savedW = nSettings::get_int((char*)"KSDLG_W", 0);
	const int savedH = nSettings::get_int((char*)"KSDLG_H", 0);
	if (savedW > 100 && savedH > 100) {
		RECT windowRect = { 0, 0, 0, 0 };
		GetWindowRect(hDlg, &windowRect);
		const int savedX = nSettings::get_int((char*)"KSDLG_X", windowRect.left);
		const int savedY = nSettings::get_int((char*)"KSDLG_Y", windowRect.top);
		SetWindowPos(hDlg, NULL, savedX, savedY, savedW, savedH,
			SWP_NOZORDER | SWP_NOACTIVATE);
	}

	g_kaillera_top_right_width = nSettings::get_int((char*)"KSDLG_TOPRW", g_kaillera_top_right_width);
	g_kaillera_bottom_right_width = nSettings::get_int((char*)"KSDLG_BOTRW", g_kaillera_bottom_right_width);

	LoadListViewColumnWidths(GetDlgItem(hDlg, LV_ULIST), "KSDLG_UCOL_", 5);
	LoadListViewColumnWidths(GetDlgItem(hDlg, LV_GLIST), "KSDLG_GCOL_", 6);
	LoadListViewColumnWidths(GetDlgItem(hDlg, LV_GULIST), "KSDLG_PCOL_", 4);
	SyncColumnRestoreWidths(GetDlgItem(hDlg, LV_ULIST), g_users_column_restore_widths, g_users_column_menu, 5);
	SyncColumnRestoreWidths(GetDlgItem(hDlg, LV_GLIST), g_games_column_restore_widths, g_games_column_menu, 6);
	SyncColumnRestoreWidths(GetDlgItem(hDlg, LV_GULIST), g_lobby_column_restore_widths, g_lobby_column_menu, 4);

	RECT clientRect;
	if (GetClientRect(hDlg, &clientRect)) {
		ApplyKailleraDialogResizeLayout(hDlg,
			clientRect.right - clientRect.left,
			clientRect.bottom - clientRect.top);
	}
}

static bool GetChildRectInClient(HWND hDlg, int controlId, RECT* outRect) {
	HWND hCtrl = GetDlgItem(hDlg, controlId);
	if (hCtrl == NULL || outRect == NULL)
		return false;
	if (!GetWindowRect(hCtrl, outRect))
		return false;
	MapWindowPoints(HWND_DESKTOP, hDlg, (LPPOINT)outRect, 2);
	return true;
}

static void InitializeKailleraDialogResizeLayout(HWND hDlg) {
	RECT clientRect;
	RECT windowRect;
	if (!GetClientRect(hDlg, &clientRect))
		return;
	if (!GetWindowRect(hDlg, &windowRect))
		return;

	g_kaillera_base_client_size.cx = clientRect.right - clientRect.left;
	g_kaillera_base_client_size.cy = clientRect.bottom - clientRect.top;
	g_kaillera_min_track_size.cx = windowRect.right - windowRect.left;
	g_kaillera_min_track_size.cy = windowRect.bottom - windowRect.top;

	const size_t itemCount = sizeof(g_kaillera_resize_items) / sizeof(g_kaillera_resize_items[0]);
	for (size_t i = 0; i < itemCount; ++i) {
		GetChildRectInClient(hDlg, g_kaillera_resize_items[i].id, &g_kaillera_resize_items[i].baseRect);
	}

	KailleraResizeItem* rePartItem = FindKailleraResizeItem(RE_PART);
	KailleraResizeItem* usersListItem = FindKailleraResizeItem(LV_ULIST);
	KailleraResizeItem* txtChatItem = FindKailleraResizeItem(TXT_CHAT);
	KailleraResizeItem* chatBtnItem = FindKailleraResizeItem(IDC_CHAT);
	KailleraResizeItem* createBtnItem = FindKailleraResizeItem(IDC_CREATE);
	if (rePartItem != NULL && usersListItem != NULL && txtChatItem != NULL && chatBtnItem != NULL && createBtnItem != NULL) {
		g_kaillera_top_right_width = usersListItem->baseRect.right - usersListItem->baseRect.left;
		g_kaillera_top_right_margin = g_kaillera_base_client_size.cx - usersListItem->baseRect.right;
		g_kaillera_top_gap_repart_to_list = usersListItem->baseRect.left - rePartItem->baseRect.right;
		g_kaillera_top_gap_txtchat_to_chat = chatBtnItem->baseRect.left - txtChatItem->baseRect.right;
		g_kaillera_top_gap_chat_to_create = createBtnItem->baseRect.left - chatBtnItem->baseRect.right;
		g_kaillera_top_gap_create_to_list = usersListItem->baseRect.left - createBtnItem->baseRect.right;
	}

	KailleraResizeItem* gameChatItem = FindKailleraResizeItem(RE_GCHAT);
	KailleraResizeItem* gameUsersItem = FindKailleraResizeItem(LV_GULIST);
	KailleraResizeItem* gameInputItem = FindKailleraResizeItem(TXT_GINP);
	KailleraResizeItem* gameChatBtnItem = FindKailleraResizeItem(BTN_GCHAT);
	if (gameChatItem != NULL && gameUsersItem != NULL && gameInputItem != NULL && gameChatBtnItem != NULL) {
		g_kaillera_bottom_right_width = gameUsersItem->baseRect.right - gameUsersItem->baseRect.left;
		g_kaillera_bottom_right_margin = g_kaillera_base_client_size.cx - gameUsersItem->baseRect.right;
		g_kaillera_bottom_gap_regchat_to_list = gameUsersItem->baseRect.left - gameChatItem->baseRect.right;
		g_kaillera_bottom_gap_txtginp_to_btn = gameChatBtnItem->baseRect.left - gameInputItem->baseRect.right;
		g_kaillera_bottom_gap_btn_to_list = gameUsersItem->baseRect.left - gameChatBtnItem->baseRect.right;
	}

	g_kaillera_active_splitter = KAILLERA_SPLITTER_NONE;
	g_kaillera_resize_initialized = true;
}

static void ApplyKailleraDialogResizeLayout(HWND hDlg, int clientWidth, int clientHeight) {
	if (!g_kaillera_resize_initialized)
		return;

	const int deltaX = clientWidth - g_kaillera_base_client_size.cx;
	const int deltaY = clientHeight - g_kaillera_base_client_size.cy;
	const size_t itemCount = sizeof(g_kaillera_resize_items) / sizeof(g_kaillera_resize_items[0]);

	for (size_t i = 0; i < itemCount; ++i) {
		const KailleraResizeItem& item = g_kaillera_resize_items[i];
		HWND hCtrl = GetDlgItem(hDlg, item.id);
		if (hCtrl == NULL)
			continue;

		RECT rect = item.baseRect;
		const bool anchorLeft = (item.anchors & KAILLERA_ANCHOR_LEFT) != 0;
		const bool anchorRight = (item.anchors & KAILLERA_ANCHOR_RIGHT) != 0;
		const bool anchorTop = (item.anchors & KAILLERA_ANCHOR_TOP) != 0;
		const bool anchorBottom = (item.anchors & KAILLERA_ANCHOR_BOTTOM) != 0;

		if (anchorLeft && anchorRight) {
			rect.right += deltaX;
		} else if (!anchorLeft && anchorRight) {
			rect.left += deltaX;
			rect.right += deltaX;
		}

		if (anchorTop && anchorBottom) {
			rect.bottom += deltaY;
		} else if (!anchorTop && anchorBottom) {
			rect.top += deltaY;
			rect.bottom += deltaY;
		}

		const int width = rect.right - rect.left;
		const int height = rect.bottom - rect.top;
		if (width <= 0 || height <= 0)
			continue;

		SetWindowPos(hCtrl, NULL, rect.left, rect.top, width, height,
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
	}

	ApplyTopSectionSplitterLayout(hDlg, clientWidth);
	ApplyBottomSectionSplitterLayout(hDlg, clientWidth, deltaY);
	ApplyJoinMessageLayout(hDlg, deltaY);

	// Redraw once after all children have moved to avoid flat-button paint artifacts while resizing.
	RedrawWindow(hDlg, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static bool IsKailleraDialogFocused(){
	HWND fg = GetForegroundWindow();
	if (fg == NULL)
		return false;
	return GetAncestor(fg, GA_ROOT) == kaillera_sdlg;
}

static void LoadJoinNotifySettings(){
	int flash = nSettings::get_int("FLASH");
	int beep = nSettings::get_int("BEEP");

	g_flash_on_user_join = (flash == -1) ? 0 : (flash != 0);
	g_beep_on_user_join = (beep == -1) ? 1 : (beep != 0);
}

static void LoadJoinMessageSettingForContext(bool hostContext){
	g_join_message_host_context = hostContext;

	if (kaillera_sdlg_TXT_MSG == NULL)
		return;

	char msg[128];
	if (hostContext) {
		nSettings::get_str((char*)"JOINMSG_HOST", msg, (char*)"");
	} else {
		nSettings::get_str((char*)"JOINMSG_JOIN", msg, (char*)"");
	}
	SetWindowText(kaillera_sdlg_TXT_MSG, msg);
}

static void SaveJoinMessageSetting(){
	if (kaillera_sdlg_TXT_MSG == NULL)
		return;

	char msg[128];
	GetWindowText(kaillera_sdlg_TXT_MSG, msg, (int)sizeof(msg));
	nSettings::set_str(g_join_message_host_context ? (char*)"JOINMSG_HOST" : (char*)"JOINMSG_JOIN", msg);
}

static void SwitchJoinMessageContext(bool hostContext){
	if (g_join_message_host_context == hostContext)
		return;
	SaveJoinMessageSetting();
	LoadJoinMessageSettingForContext(hostContext);
}

static void FlashKailleraDialogIfNotFocused(){
	if (!g_flash_on_user_join)
		return;
	if (IsKailleraDialogFocused())
		return;

	FLASHWINFO fwi;
	fwi.cbSize = sizeof(fwi);
	fwi.hwnd = kaillera_sdlg;
	fwi.dwFlags = FLASHW_TIMERNOFG | FLASHW_TRAY;
	fwi.uCount = 0;
	fwi.dwTimeout = 0;
	FlashWindowEx(&fwi);
}
//=======================================================================
bool kaillera_RecordingEnabled(){
	return SendMessage(GetDlgItem(kaillera_sdlg, CHK_REC), BM_GETCHECK, 0, 0)==BST_CHECKED;
}
int kaillera_sdlg_MODE;
void kaillera_sdlgGameMode(bool toggle = false){
	kaillera_sdlg_MODE = 0;
	if (!toggle){
		SetWindowText(GetDlgItem(kaillera_sdlg, IDC_CREATE), "Swap");
		kaillera_sdlg_toggle = false;
	} else {
		kaillera_sdlg_toggle = false;
	}
	ShowWindow(kaillera_sdlg_CHK_REC,SW_SHOW);
	ShowWindow(kaillera_sdlg_RE_GCHAT,SW_SHOW);
	ShowWindow(kaillera_sdlg_TXT_GINP,SW_SHOW);
	ShowWindow(kaillera_sdlg_LV_GULIST.handle,SW_SHOW);
	ShowWindow(kaillera_sdlg_BTN_START,SW_SHOW);
	ShowWindow(kaillera_sdlg_BTN_DROP,SW_SHOW);
	ShowWindow(kaillera_sdlg_BTN_LEAVE,SW_SHOW);
	ShowWindow(kaillera_sdlg_BTN_KICK,SW_SHOW);
	ShowWindow(kaillera_sdlg_BTN_LAGSTAT,SW_SHOW);
	ShowWindow(kaillera_sdlg_BTN_OPTIONS,SW_SHOW);
	ShowWindow(kaillera_sdlg_BTN_ADVERTISE,SW_SHOW);
	ShowWindow(kaillera_sdlg_ST_SPEED,SW_SHOW);
	ShowWindow(kaillera_sdlg_ST_DELAY,SW_SHOW);
	ShowWindow(kaillera_sdlg_BTN_GCHAT,SW_SHOW);
	ShowWindow(kaillera_sdlg_MINGUIUPDATE,SW_SHOW);
	ShowWindow(kaillera_sdlg_TXT_MSG,SW_SHOW);
	ShowWindow(kaillera_sdlg_JOINMSG_LBL,SW_SHOW);
	ShowWindow(kaillera_sdlg_gameslv.handle,SW_HIDE);
}

void kaillera_sdlgNormalMode(bool toggle = false){
	if (!toggle){
		kaillera_sdlg_MODE = 1;
		hosting = false;
		SetWindowText(GetDlgItem(kaillera_sdlg, IDC_CREATE), "Create");
	} else {
		kaillera_sdlg_toggle = true;
	}
	ShowWindow(kaillera_sdlg_CHK_REC,SW_HIDE);
	ShowWindow(kaillera_sdlg_RE_GCHAT,SW_HIDE);
	ShowWindow(kaillera_sdlg_TXT_GINP,SW_HIDE);
	ShowWindow(kaillera_sdlg_LV_GULIST.handle,SW_HIDE);
	ShowWindow(kaillera_sdlg_BTN_START,SW_HIDE);
	ShowWindow(kaillera_sdlg_BTN_DROP,SW_HIDE);
	ShowWindow(kaillera_sdlg_BTN_LEAVE,SW_HIDE);
	ShowWindow(kaillera_sdlg_BTN_KICK,SW_HIDE);
	ShowWindow(kaillera_sdlg_BTN_LAGSTAT,SW_HIDE);
	ShowWindow(kaillera_sdlg_BTN_OPTIONS,SW_HIDE);
	ShowWindow(kaillera_sdlg_BTN_ADVERTISE,SW_HIDE);
	ShowWindow(kaillera_sdlg_ST_SPEED,SW_HIDE);
	ShowWindow(kaillera_sdlg_ST_DELAY,SW_HIDE);
	ShowWindow(kaillera_sdlg_BTN_GCHAT,SW_HIDE);
	ShowWindow(kaillera_sdlg_MINGUIUPDATE,SW_HIDE);
	ShowWindow(kaillera_sdlg_TXT_MSG,SW_HIDE);
	ShowWindow(kaillera_sdlg_JOINMSG_LBL,SW_HIDE);
	ShowWindow(kaillera_sdlg_gameslv.handle,SW_SHOW);
}
//===================================

int kaillera_sdlg_gameslvColumn;
int kaillera_sdlg_gameslvColumnTypes[7] = {1, 0, 1, 1, 1, 0, 0};  // 1=string, 0=numeric; columns: Game, GameID, Emulator, User, Status, Users
int kaillera_sdlg_gameslvColumnOrder[7];

static int kaillera_game_status_rank(const char *status){
	if (status == NULL)
		return INT_MAX;
	if (strcmp(status, "Waiting") == 0)
		return 0;
	if (strcmp(status, "Playing") == 0)
		return 1;
	return 2;
}

int CALLBACK kaillera_sdlg_gameslvCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort){
	const int sortColumn = (int)lParamSort;

	int ind1 = kaillera_sdlg_gameslv.Find(lParam1);
	int ind2 = kaillera_sdlg_gameslv.Find(lParam2);
	if (ind1 == -1 || ind2 == -1)
		return 0;

	char ItemText1[128];
	char ItemText2[128];

		
	kaillera_sdlg_gameslv.CheckRow(ItemText1, 128, sortColumn, ind1);
	kaillera_sdlg_gameslv.CheckRow(ItemText2, 128, sortColumn, ind2);

	const bool ascending = kaillera_sdlg_gameslvColumnOrder[sortColumn] != 0;
	if (sortColumn == 4) {
		int rank1 = kaillera_game_status_rank(ItemText1);
		int rank2 = kaillera_game_status_rank(ItemText2);

		if (rank1 != rank2)
			return ascending ? (rank1 > rank2 ? 1 : -1) : (rank1 > rank2 ? -1 : 1);

		// Tie-break by game name to keep status grouping stable.
		char name1[128];
		char name2[128];
		kaillera_sdlg_gameslv.CheckRow(name1, 128, 0, ind1);
		kaillera_sdlg_gameslv.CheckRow(name2, 128, 0, ind2);
		int nameCmp = strcmp(name1, name2);
		if (nameCmp != 0)
			return ascending ? nameCmp : -nameCmp;
		return 0;
	}

	if (kaillera_sdlg_gameslvColumnTypes[sortColumn]) {
		if (ascending)
			return strcmp(ItemText1, ItemText2);
		else
			return -1*strcmp(ItemText1, ItemText2);
	} else {
		ind1 = atoi(ItemText1);
		ind2 = atoi(ItemText2);

		if (ascending)
			return (ind1==ind2? 0 : (ind1>ind2? 1 : -1));
		else
			return (ind1==ind2? 0 : (ind1>ind2? -1 : 1));

	}
}

void kaillera_sdlg_gameslvReSort() {
	ListView_SortItems(kaillera_sdlg_gameslv.handle, kaillera_sdlg_gameslvCompareFunc, kaillera_sdlg_gameslvColumn);
}

void kaillera_sdlg_gameslvSort(int column) {
	kaillera_sdlg_gameslvColumn = column;

	if (kaillera_sdlg_gameslvColumnOrder[column])
		kaillera_sdlg_gameslvColumnOrder[column] = 0;
	else
		kaillera_sdlg_gameslvColumnOrder[column] = 5;

	kaillera_sdlg_gameslvReSort();
}
//========================


int kaillera_sdlg_userslvColumn;
int kaillera_sdlg_userslvColumnTypes[5] = {1, 0, 0, 1, 1};  // Name, Ping, UID, Status, Connection
int kaillera_sdlg_userslvColumnOrder[5];

int CALLBACK kaillera_sdlg_userslvCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort){
	const int sortColumn = (int)lParamSort;
	int ind1 = kaillera_sdlg_userslv.Find(lParam1);
	int ind2 = kaillera_sdlg_userslv.Find(lParam2);
	if (ind1 == -1 || ind2 == -1)
		return 0;
	char ItemText1[128];
	char ItemText2[128];
	kaillera_sdlg_userslv.CheckRow(ItemText1, 128, sortColumn, ind1);
	kaillera_sdlg_userslv.CheckRow(ItemText2, 128, sortColumn, ind2);
	if (kaillera_sdlg_userslvColumnTypes[sortColumn]) {
		if (kaillera_sdlg_userslvColumnOrder[sortColumn])
			return strcmp(ItemText1, ItemText2);
		else
			return -1* strcmp(ItemText1, ItemText2);
	} else {
		ind1 = atoi(ItemText1);
		ind2 = atoi(ItemText2);
		if (kaillera_sdlg_userslvColumnOrder[sortColumn])
			return (ind1==ind2? 0 : (ind1>ind2? 1 : -1));
		else
			return (ind1==ind2? 0 : (ind1>ind2? -1 : 1));
	}
}

void kaillera_sdlg_userslvReSort() {
	ListView_SortItems(kaillera_sdlg_userslv.handle, kaillera_sdlg_userslvCompareFunc, kaillera_sdlg_userslvColumn);
}

void kaillera_sdlg_userslvSort(int column) {
	kaillera_sdlg_userslvColumn = column;

	if (kaillera_sdlg_userslvColumnOrder[column])
		kaillera_sdlg_userslvColumnOrder[column] = 0;
	else
		kaillera_sdlg_userslvColumnOrder[column] = 5;

	kaillera_sdlg_userslvReSort();
}

//====================

void kaillera_outp(char * line){
	re_append(kaillera_sdlg_partchat, line, 0);
}


void kaillera_goutp(char * line){
	re_append(kaillera_sdlg_RE_GCHAT, line, 0);
}

static const COLORREF KAILLERA_COLOR_GREEN = 0x00009900; // matches kaillera_ui_motd()
static const COLORREF KAILLERA_COLOR_DARK_BLUE = RGB(0, 0, 102); // join/leave in lobby chat

static void AppendFormattedLine(HWND hwnd, COLORREF color, char* fmt, va_list args) {
	char msg[2048];
	msg[0] = 0;
	vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, args);

	char ts[20];
	get_timestamp(ts, sizeof(ts));

	char line[4096];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "%s%s\r\n", ts, msg);
	re_append(hwnd, line, color);
}


void __cdecl kaillera_gdebug(char * arg_0, ...) {
	va_list args;
	va_start (args, arg_0);
	AppendFormattedLine(kaillera_sdlg_RE_GCHAT, 0x00000000, arg_0, args);
	va_end (args);
}

void __cdecl kaillera_gdebug_color(COLORREF color, char* arg_0, ...) {
	va_list args;
	va_start(args, arg_0);
	AppendFormattedLine(kaillera_sdlg_RE_GCHAT, color, arg_0, args);
	va_end(args);
}
void __cdecl kaillera_ui_gdebug(char* arg_0, ...) {
	va_list args;
	va_start(args, arg_0);
	AppendFormattedLine(kaillera_sdlg_RE_GCHAT, 0x00000000, arg_0, args);
	va_end(args);
}

void __cdecl kaillera_ui_gdebug_color(COLORREF color, char* arg_0, ...) {
	va_list args;
	va_start(args, arg_0);
	AppendFormattedLine(kaillera_sdlg_RE_GCHAT, color, arg_0, args);
	va_end(args);
}

void __cdecl kaillera_core_debug(char * arg_0, ...) {
	va_list args;
	va_start (args, arg_0);
	AppendFormattedLine(kaillera_sdlg_partchat, 0x00FF0000, arg_0, args);
	va_end (args);
}
void __cdecl kaillera_ui_motd(char * arg_0, ...) {
	va_list args;
	va_start (args, arg_0);
	AppendFormattedLine(kaillera_sdlg_partchat, KAILLERA_COLOR_GREEN, arg_0, args);
	va_end (args);
}
void __cdecl kaillera_error_callback(char * arg_0, ...) {
	va_list args;
	va_start (args, arg_0);
	AppendFormattedLine(kaillera_sdlg_partchat, 0x000000FF, arg_0, args);
	va_end (args);
}

void __cdecl kaillera_ui_debug(char * arg_0, ...) {
	va_list args;
	va_start (args, arg_0);
	AppendFormattedLine(kaillera_sdlg_partchat, 0x00000000, arg_0, args);
	va_end (args);
}

void __cdecl kaillera_outpf(char * arg_0, ...) {
	va_list args;
	va_start (args, arg_0);
	AppendFormattedLine(kaillera_sdlg_partchat, 0x00000000, arg_0, args);
	va_end (args);
}


void kaillera_user_add_callback(char*name, int ping, int status, unsigned short id, char conn){
	char bfx[500];
	int x;
	kaillera_sdlg_userslv.AddRow(name, id);
	x = kaillera_sdlg_userslv.Find(id);
	wsprintf(bfx, "%i", ping);
	kaillera_sdlg_userslv.FillRow(bfx, 1, x);  // Ping
	wsprintf(bfx, "%u", id);
	kaillera_sdlg_userslv.FillRow(bfx, 2, x);  // UID
	kaillera_sdlg_userslv.FillRow(USER_STATUS[status], 3, x);  // Status
	kaillera_sdlg_userslv.FillRow(CONNECTION_TYPES[conn], 4, x);  // Connection

}
void kaillera_game_add_callback(char*gname, unsigned int id, char*emulator, char*owner, char*users, char status){
	int x;

	kaillera_sdlg_gameslv.AddRow(gname, id);
	x = kaillera_sdlg_gameslv.Find(id);

	char idStr[16];
	wsprintf(idStr, "%u", id);
	kaillera_sdlg_gameslv.FillRow(idStr, 1, x);
	kaillera_sdlg_gameslv.FillRow(emulator, 2, x);
	kaillera_sdlg_gameslv.FillRow(owner, 3, x);
	kaillera_sdlg_gameslv.FillRow(GAME_STATUS[status], 4, x);
	kaillera_sdlg_gameslv.FillRow(users, 5, x);
	kaillera_sdlg_gameslvReSort();
}
void kaillera_game_create_callback(char*gname, unsigned int id, char*emulator, char*owner){
	if (MINGUIUPDATE && KSSDFA.state==2) 
		return;
	kaillera_game_add_callback(gname, id, emulator, owner, "1/2", 0);
}

void kaillera_chat_callback(char*name, char * msg){
	if (MINGUIUPDATE && KSSDFA.state==2)
		return;
	kaillera_outpf("<%s> %s", name, msg);
}
void kaillera_game_chat_callback(char*name, char * msg){
	if (name != NULL && _stricmp(name, "server") == 0) {
		kaillera_gdebug_color(KAILLERA_COLOR_GREEN, "<%s> %s", name, msg);
	} else {
		kaillera_gdebug("<%s> %s", name, msg);
	}
	if (KSSDFA.state==2 && infos.chatReceivedCallback) {
		infos.chatReceivedCallback(name, msg);
	}
}
void kaillera_motd_callback(char*name, char * msg){
	if (MINGUIUPDATE && KSSDFA.state==2)
		return;

	// Check if this is a private message (format: "TO: <user>(id): msg" or "<user>(id): msg")
	bool is_pm = false;
	if (strncmp(msg, "TO: <", 5) == 0) {
		is_pm = true;  // Outgoing PM confirmation
	} else if (msg[0] == '<' && strstr(msg, "): ") != NULL) {
		is_pm = true;  // Incoming PM
	}

	if (is_pm) {
		// Display private messages in bright green
		char ts[20];
		get_timestamp(ts, sizeof(ts));

		char line[4096];
		_snprintf_s(line, sizeof(line), _TRUNCATE, "%s- %s\r\n", ts, msg);
		re_append(kaillera_sdlg_partchat, line, KAILLERA_COLOR_GREEN);
	} else {
		kaillera_ui_motd("- %s", msg);
	}
}
void kaillera_user_join_callback(char*name, int ping, unsigned short id, char conn){
	if (MINGUIUPDATE && KSSDFA.state==2) 
		return;
	kaillera_user_add_callback(name, ping, 1, id, conn);
	kaillera_ui_debug("* Joins: %s (Ping: %i)", name, ping);
	kaillera_sdlg_userslvReSort();
}
void kaillera_user_leave_callback(char*name, char*quitmsg, unsigned short id){
	if (MINGUIUPDATE && KSSDFA.state==2)
		return;
	kaillera_sdlg_userslv.DeleteRow(kaillera_sdlg_userslv.Find(id));
	kaillera_ui_debug("* Parts: %s", name);
}
void kaillera_game_close_callback(unsigned int id){
	if (MINGUIUPDATE && KSSDFA.state==2) 
		return;
	kaillera_sdlg_gameslv.DeleteRow(kaillera_sdlg_gameslv.Find(id));
}
void kaillera_game_status_change_callback(unsigned int id, char status, int players, int maxplayers){
	if (MINGUIUPDATE && KSSDFA.state==2) 
		return;
	char * GAME_STATUS [] = 
	{
		"Waiting",
		"Playing",
		"Playing",
	};
	int x = kaillera_sdlg_gameslv.Find(id);
	kaillera_sdlg_gameslv.FillRow(GAME_STATUS[status], 4, x);
	char users [32];
	wsprintf(users, "%i/%i", players, maxplayers);
	kaillera_sdlg_gameslv.FillRow(users, 5, x);
	kaillera_sdlg_gameslvReSort();
}

void kaillera_user_game_create_callback(){
	inGame = true;
	SwitchJoinMessageContext(true);
	hosting = true;
	kaillera_sdlgGameMode();
	kaillera_sdlg_LV_GULIST.DeleteAllRows();
	SetWindowText(kaillera_sdlg_RE_GCHAT, "");
	EnableWindow(kaillera_sdlg_BTN_KICK, TRUE);
	EnableWindow(kaillera_sdlg_BTN_START, !IsNonGameLobbyName(GAME));

	ExecuteOptions();
}
void kaillera_user_game_closed_callback(){
	inGame = false;
	SwitchJoinMessageContext(false);
	hosting = false;
	kaillera_sdlgNormalMode();
}

void kaillera_user_game_joined_callback(){
	inGame = true;
	SwitchJoinMessageContext(false);
	hosting = false;
	kaillera_sdlgGameMode();
	kaillera_sdlg_LV_GULIST.DeleteAllRows();
	SetWindowText(kaillera_sdlg_RE_GCHAT, "");
	EnableWindow(kaillera_sdlg_BTN_KICK, FALSE);
	EnableWindow(kaillera_sdlg_BTN_START, FALSE);
}

void kaillera_player_add_callback(char *name, int ping, unsigned short id, char conn){
	char bfx[32];
	kaillera_sdlg_LV_GULIST.AddRow(name, id);
	int x = kaillera_sdlg_LV_GULIST.Find(id);
	wsprintf(bfx, "%i", ping);
	kaillera_sdlg_LV_GULIST.FillRow(bfx, 1, x);
	kaillera_sdlg_LV_GULIST.FillRow(CONNECTION_TYPES[conn], 2, x);
	int thrp = (ping * 60 / 1000 / conn) + 2;
	wsprintf(bfx, "%i frames", thrp * conn - 1);
	kaillera_sdlg_LV_GULIST.FillRow(bfx, 3, x);
}
void kaillera_player_joined_callback(char * username, int ping, unsigned short uid, char connset){
	kaillera_ui_gdebug_color(KAILLERA_COLOR_DARK_BLUE, "* Joins: %s", username);
	kaillera_player_add_callback(username, ping, uid, connset);
	if (hosting && kaillera_sdlg_TXT_MSG != NULL) {
		char msg[128];
		GetWindowText(kaillera_sdlg_TXT_MSG, msg, (int)sizeof(msg));
		if (msg[0] != 0)
			kaillera_game_chat_send(msg);
	}
	if (g_beep_on_user_join)
		MessageBeep(MB_OK);
	FlashKailleraDialogIfNotFocused();
	if (kaillera_is_game_running())
		kaillera_kick_user(uid);
}
void kaillera_player_left_callback(char * user, unsigned short id){
	kaillera_ui_gdebug_color(KAILLERA_COLOR_DARK_BLUE, "* Parts: %s", user);
	kaillera_sdlg_LV_GULIST.DeleteRow (kaillera_sdlg_LV_GULIST.Find(id));
}
void kaillera_user_kicked_callback(){
	inGame = false;
	SwitchJoinMessageContext(false);
	hosting = false;
	kaillera_error_callback("* You have been kicked out of the game");
	KSSDFA.input = KSSDFA_END_GAME;
	KSSDFA.state = 0;
	kaillera_sdlgNormalMode();
}
void kaillera_login_stat_callback(char*lsmsg){
	kaillera_core_debug("* %s", lsmsg);
}
void kaillera_player_dropped_callback(char * user, int gdpl){
	kaillera_gdebug("* Dropped: %s (Player %i)", user, gdpl);
	if (infos.clientDroppedCallback)
		infos.clientDroppedCallback(user,gdpl);
	if (gdpl == playerno) {
		KSSDFA.input = KSSDFA_END_GAME;
		KSSDFA.state = 0;
	}
}
void kaillera_game_callback(char * game, char player, char players){
	if (game!= 0)
	{
		strncpy(GAME, game, sizeof(GAME) - 1);
		GAME[sizeof(GAME) - 1] = 0;
	}
	playerno = player;
	numplayers = players;
	kaillera_gdebug("* Starting: %s (%i/%i)", GAME, playerno, numplayers);
	kaillera_gdebug("- press \"Drop\" if emulator fails");

	if (MINGUIUPDATE)
		kaillera_gdebug("- Minimal gui update mode is on. GUI related items outside your game will be disabled. Please reconnect to server once you've finished playing.");

	if (kaillera_RecordingEnabled())
		kaillera_gdebug("- your game will be recorded");

	KSSDFA.input = KSSDFA_START_GAME;
}
void kaillera_game_netsync_wait_callback(int tx){
	SetWindowText(kaillera_sdlg_ST_SPEED, "waiting for others");
	int secs = tx / 1000;
	int ssecs = (tx % 1000) / 100;
	char xxx[32];
	wsprintf(xxx,"%03i.%is", secs, ssecs);
	SetWindowText(kaillera_sdlg_ST_DELAY, xxx);
	kaillera_sdlg_delay = -1;
}
void kaillera_end_game_callback(){
	KSSDFA.input = KSSDFA_END_GAME;
	KSSDFA.state = 0;
}
//=============================================================

// Menu item IDs for game list menu
#define MENU_ID_JOIN 1
#define MENU_ID_SEND_MSG 2
#define MENU_ID_FINDUSER 3
#define MENU_ID_IGNORE 4
#define MENU_ID_UNIGNORE 5
#define MENU_ID_CREATE_AWAY 6
#define MENU_ID_CREATE_CHAT 7
#define MENU_ID_CREATE_BASE 1000

static const char* kNonGameLobbyAway = "*Away (leave messages)";
static const char* kNonGameLobbyChat = "*Chat (not game)";

static bool IsNonGameLobbyName(const char* name) {
	if (name == NULL || *name == 0)
		return false;
	return strcmp(name, kNonGameLobbyAway) == 0 || strcmp(name, kNonGameLobbyChat) == 0;
}

HMENU kaillera_sdlg_CreateGamesMenu = 0;
int kaillera_sdlg_GamesCount = 0;
void kaillera_sdlg_create_games_list_menu() {
	{
		kaillera_sdlg_GamesCount = 0;
		char * xx = gamelist;
		if (xx != NULL) {
			size_t p;
			while ((p = strlen(xx)) != 0){
				xx += p + 1;
				kaillera_sdlg_GamesCount++;
			}
		}
	}

	MENUITEMINFO mi;
	char * cx = gamelist;
	HMENU ht = kaillera_sdlg_CreateGamesMenu = CreatePopupMenu();
	char last_char = 0;
	char new_char_buffer[4];
	mi.fMask = MIIM_ID | MIIM_TYPE | MFT_STRING;
	mi.fType = MFT_STRING;
	int counter = MENU_ID_CREATE_BASE;
	if (cx != NULL) {
		while (*cx != 0) {
			mi.wID = counter;
			mi.dwTypeData = cx;
			mi.dwItemData = 0;
			if (*cx != last_char && kaillera_sdlg_GamesCount > 20){
				new_char_buffer[0] = *cx;
				new_char_buffer[1] = 0;
				ht = CreatePopupMenu();
				AppendMenu(kaillera_sdlg_CreateGamesMenu, MF_POPUP, (UINT_PTR) ht, new_char_buffer);
				last_char = *cx;
			}
			AppendMenu(ht, MF_STRING, counter, cx);
			cx += strlen(cx) + 1;
			counter++;
		}
	}

	if (kaillera_sdlg_GamesCount > 20) {
		HMENU starMenu = CreatePopupMenu();
		AppendMenu(kaillera_sdlg_CreateGamesMenu, MF_POPUP, (UINT_PTR)starMenu, "*");
		AppendMenu(starMenu, MF_STRING, MENU_ID_CREATE_AWAY, kNonGameLobbyAway);
		AppendMenu(starMenu, MF_STRING, MENU_ID_CREATE_CHAT, kNonGameLobbyChat);
	} else {
		if (kaillera_sdlg_GamesCount > 0)
			AppendMenu(kaillera_sdlg_CreateGamesMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(kaillera_sdlg_CreateGamesMenu, MF_STRING, MENU_ID_CREATE_AWAY, kNonGameLobbyAway);
		AppendMenu(kaillera_sdlg_CreateGamesMenu, MF_STRING, MENU_ID_CREATE_CHAT, kNonGameLobbyChat);
	}
}

void kailelra_sdlg_join_selected_game(){
	int sel = kaillera_sdlg_gameslv.SelectedRow();
	if (sel >= 0 && sel < kaillera_sdlg_gameslv.RowsCount() && !inGame) {
		unsigned int id = (unsigned int)(UINT_PTR)kaillera_sdlg_gameslv.RowNo(sel);
		char temp[128];
		kaillera_sdlg_gameslv.CheckRow(temp, 128, 4, sel);  // Status column
		if (strcmp(temp, "Waiting") != 0) {
			kaillera_error_callback("Joining running game is not allowed");
			return;
		}

		kaillera_sdlg_gameslv.CheckRow(temp, 128, 0, sel);  // Game column
		if (IsNonGameLobbyName(temp)) {
			strncpy(GAME, temp, sizeof(GAME) - 1);
			GAME[sizeof(GAME) - 1] = 0;
			kaillera_join_game(id);
			return;
		}
		char * cx = gamelist;
		while (*cx != 0) {
				if (strcmp(cx, temp) == 0) {
					strncpy(GAME, temp, sizeof(GAME) - 1);
					GAME[sizeof(GAME) - 1] = 0;
					kaillera_sdlg_gameslv.CheckRow(temp, 128, 2, sel);  // Emulator column
					if (strcmp(temp, APP) != 0) {
					if (MessageBox(kaillera_sdlg, "Emulator/version mismatch and the game may desync.\nDo you want to continue?", "Error", MB_YESNO | MB_ICONEXCLAMATION) != IDYES)
						return;
				}
				kaillera_join_game(id);
				return;
			}
			cx += strlen(cx) + 1;
		}
		kaillera_error_callback("The rom '%s' is not in your list.", temp);
	}
}

void kaillera_sdlg_show_games_list_menu(HWND handle, bool incjoin = false){
	POINT pi;
	GetCursorPos(&pi);
	HMENU mmainmenuu = kaillera_sdlg_CreateGamesMenu;
	if(incjoin){
		mmainmenuu = CreatePopupMenu();
		MENUITEMINFO mi;
		mi.cbSize = sizeof(MENUITEMINFO);
		mi.fMask = MIIM_TYPE | MFT_STRING | MIIM_SUBMENU | MIIM_ID;
		mi.fType = MFT_STRING;
		mi.hSubMenu = kaillera_sdlg_CreateGamesMenu;
		mi.dwTypeData = "Create";
		mi.wID = 0; // Submenu doesn't need ID
		InsertMenuItem(mmainmenuu, 0, TRUE, &mi);
		mi.fMask = MIIM_TYPE | MFT_STRING | MIIM_ID;
		mi.hSubMenu = NULL;
		mi.dwTypeData = "Join";
		mi.wID = MENU_ID_JOIN;
		InsertMenuItem(mmainmenuu, 1, TRUE, &mi);
	}
	int result = TrackPopupMenu(mmainmenuu, TPM_RETURNCMD, pi.x, pi.y, 0, handle, NULL);
	if(result != 0){
		if(result == MENU_ID_JOIN){
			kailelra_sdlg_join_selected_game();
		} else if (result == MENU_ID_CREATE_AWAY || result == MENU_ID_CREATE_CHAT) {
			if (!inGame) {
				const char* lobby = (result == MENU_ID_CREATE_AWAY) ? kNonGameLobbyAway : kNonGameLobbyChat;
				strncpy(GAME, lobby, sizeof(GAME) - 1);
				GAME[sizeof(GAME) - 1] = 0;
				kaillera_create_game(GAME);
			}
		} else if (result >= MENU_ID_CREATE_BASE) {
			if (!inGame) {
					// Get the game name from the submenu item
					char gameName[256];
					GetMenuStringA(kaillera_sdlg_CreateGamesMenu, result, gameName, sizeof(gameName), MF_BYCOMMAND);
					strncpy(GAME, gameName, sizeof(GAME) - 1);
					GAME[sizeof(GAME) - 1] = 0;
					kaillera_create_game(GAME);
				}
		}
	}
}
void kaillera_sdlg_destroy_games_list_menu(){

}

static void SyncColumnRestoreWidths(HWND listHandle, int* restoreWidths, KailleraColumnMenuItem* columns, int count) {
	if (listHandle == NULL || restoreWidths == NULL || columns == NULL || count <= 0)
		return;

	for (int i = 0; i < count; ++i) {
		const int width = ListView_GetColumnWidth(listHandle, i);
		if (width > 0) {
			restoreWidths[i] = (width < KAILLERA_MIN_COLUMN_WIDTH) ? KAILLERA_MIN_COLUMN_WIDTH : width;
		} else if (restoreWidths[i] <= 0) {
			restoreWidths[i] = columns[i].defaultWidth;
		}
	}
}

static int FindPreviousVisibleColumn(HWND listHandle, int fromColumn) {
	if (listHandle == NULL || fromColumn <= 0)
		return -1;
	for (int i = fromColumn - 1; i >= 0; --i) {
		if (ListView_GetColumnWidth(listHandle, i) > 0)
			return i;
	}
	return -1;
}

static void ShowColumnVisibilityMenu(
	HWND hDlg,
	HWND listHandle,
	KailleraColumnMenuItem* columns,
	int* restoreWidths,
	int count) {
	if (hDlg == NULL || listHandle == NULL || columns == NULL || restoreWidths == NULL || count <= 0)
		return;

	SyncColumnRestoreWidths(listHandle, restoreWidths, columns, count);

	HMENU menu = CreatePopupMenu();
	for (int i = 0; i < count; ++i) {
		const int width = ListView_GetColumnWidth(listHandle, i);
		UINT flags = MF_STRING;
		if (width > 0)
			flags |= MF_CHECKED;
		AppendMenuA(menu, flags, columns[i].id, columns[i].title);
	}

	POINT pi;
	GetCursorPos(&pi);
	const int result = TrackPopupMenu(menu, TPM_RETURNCMD, pi.x, pi.y, 0, hDlg, NULL);
	if (result != 0) {
		for (int i = 0; i < count; ++i) {
			if ((int)columns[i].id != result)
				continue;

			const int width = ListView_GetColumnWidth(listHandle, i);
			if (width > 0) {
				int visibleCount = 0;
				for (int j = 0; j < count; ++j) {
					if (ListView_GetColumnWidth(listHandle, j) > 0)
						visibleCount++;
				}
				if (visibleCount <= 1)
					break;

				restoreWidths[i] = width;
				g_kaillera_allow_zero_width_columns = true;
				ListView_SetColumnWidth(listHandle, i, 0);
				g_kaillera_allow_zero_width_columns = false;
			} else {
				int restoreWidth = restoreWidths[i];
				if (restoreWidth <= 0)
					restoreWidth = columns[i].defaultWidth;
				if (restoreWidth > 0 && restoreWidth < KAILLERA_MIN_COLUMN_WIDTH)
					restoreWidth = KAILLERA_MIN_COLUMN_WIDTH;
				g_kaillera_allow_zero_width_columns = true;
				ListView_SetColumnWidth(listHandle, i, restoreWidth);
				g_kaillera_allow_zero_width_columns = false;
			}
			break;
		}

		SyncColumnRestoreWidths(listHandle, restoreWidths, columns, count);
		InvalidateRect(listHandle, NULL, TRUE);
	}

	DestroyMenu(menu);
}

void kaillera_sdlg_show_users_list_menu(HWND hDlg) {
	int sel = kaillera_sdlg_userslv.SelectedRow();
	if (sel < 0 || sel >= kaillera_sdlg_userslv.RowsCount()) {
		return;
	}

	POINT pi;
	GetCursorPos(&pi);

	HMENU menu = CreatePopupMenu();
	AppendMenu(menu, MF_STRING, MENU_ID_SEND_MSG, "Send Message");
	AppendMenu(menu, MF_STRING, MENU_ID_FINDUSER, "Find user");
	AppendMenu(menu, MF_STRING, MENU_ID_IGNORE, "Ignore");
	AppendMenu(menu, MF_STRING, MENU_ID_UNIGNORE, "Unignore");

	int result = TrackPopupMenu(menu, TPM_RETURNCMD, pi.x, pi.y, 0, hDlg, NULL);
	if (result != 0) {
		// Get the user ID and username from the selected row
		unsigned short userId = (unsigned short)(UINT_PTR)kaillera_sdlg_userslv.RowNo(sel);
		char username[128];
		kaillera_sdlg_userslv.CheckRow(username, (int)sizeof(username), 0, sel);

		if (result == MENU_ID_SEND_MSG) {
			// Fill the chat input with /msg <UserID>
			char msgCmd[64];
			wsprintf(msgCmd, "/msg %u ", userId);
			SetWindowText(GetDlgItem(hDlg, TXT_CHAT), msgCmd);

			// Set focus to the chat input
			SetFocus(GetDlgItem(hDlg, TXT_CHAT));

			// Move cursor to end of text
			SendMessage(GetDlgItem(hDlg, TXT_CHAT), EM_SETSEL, (WPARAM)strlen(msgCmd), (LPARAM)strlen(msgCmd));
		} else if (result == MENU_ID_FINDUSER) {
			char command[256];
			wsprintf(command, "/finduser %s", username);
			kaillera_chat_send(command);
		} else if (result == MENU_ID_IGNORE) {
			char command[64];
			wsprintf(command, "/ignore %u", userId);
			kaillera_chat_send(command);
		} else if (result == MENU_ID_UNIGNORE) {
			char command[64];
			wsprintf(command, "/unignore %u", userId);
			kaillera_chat_send(command);
		}
	}

	DestroyMenu(menu);
}

bool reconn = false;
//===========================================================================================
void kaillera_ui_chat_send(char * text){
	if (*text == '/') {
		if (strcmp(text, "/pcs")==0) {
			kaillera_core_debug("============= Core status begin ===============");
			kaillera_print_core_status();
			unsigned int sta =  KSSDFA.state;
			unsigned int inp = KSSDFA.input;
			kaillera_core_debug("KSSDFA { state: %i, input: %i }", sta, inp);
			kaillera_core_debug("PACKETLOSSCOUNT=%u", PACKETLOSSCOUNT);
			kaillera_core_debug("PACKETMISOTDERCOUNT=%u", PACKETMISOTDERCOUNT);
			kaillera_core_debug("============ Core status end =================");
			return;
		} else if (strcmp(text, "/swm")==0) {
			if (kaillera_sdlg_MODE!=0)
				kaillera_sdlgGameMode();
			else
				kaillera_sdlgNormalMode();
			return;
		} else if (strcmp(text, "/clear")==0) {
			SetWindowText(kaillera_sdlg_partchat, "");
			return;
		} else if (strcmp(text, "/stats") == 0) {
			StatsDisplayThreadBegin();
			return;
		}
	}

	kaillera_chat_send(text);
}

static void LoadOptions(HWND hwnd){
	char str[64];
	int maxplayers = nSettings::get_int("MAXPLAYERS");
	int maxping = nSettings::get_int("MAXPING");
	int flash = nSettings::get_int("FLASH");
	int beep = nSettings::get_int("BEEP");

	if (maxplayers == -1) maxplayers = 4;
	wsprintf(str, "%d", maxplayers);
	SetWindowText(GetDlgItem(hwnd, IDC_MAXPLAYERS), str);

	if (maxping == -1) maxping = 999;
	wsprintf(str, "%d", maxping);
	SetWindowText(GetDlgItem(hwnd, IDC_MAXPING), str);

	HWND flashCombo = GetDlgItem(hwnd, IDC_FLASH);
	SendMessage(flashCombo, CB_ADDSTRING, 0, (LPARAM)"False");
	SendMessage(flashCombo, CB_ADDSTRING, 0, (LPARAM)"True");
	SendMessage(flashCombo, CB_SETCURSEL, (flash == 1) ? 1 : 0, 0);

	HWND beepCombo = GetDlgItem(hwnd, IDC_BEEP);
	SendMessage(beepCombo, CB_ADDSTRING, 0, (LPARAM)"False");
	SendMessage(beepCombo, CB_ADDSTRING, 0, (LPARAM)"True");
	SendMessage(beepCombo, CB_SETCURSEL, (beep == 0) ? 0 : 1, 0);
}

static void SaveOptions(HWND hwnd){
	char str[64];

	GetWindowText(GetDlgItem(hwnd, IDC_MAXPLAYERS), str, (int)sizeof(str));
	int maxplayers = atoi(str);

	GetWindowText(GetDlgItem(hwnd, IDC_MAXPING), str, (int)sizeof(str));
	int maxping = atoi(str);

	int flashSel = (int)SendMessage(GetDlgItem(hwnd, IDC_FLASH), CB_GETCURSEL, 0, 0);
	int flash = (flashSel == 1) ? 1 : 0;

	int beepSel = (int)SendMessage(GetDlgItem(hwnd, IDC_BEEP), CB_GETCURSEL, 0, 0);
	int beep = (beepSel == 1) ? 1 : 0;

	nSettings::set_int("MAXPLAYERS", maxplayers);
	nSettings::set_int("MAXPING", maxping);
	nSettings::set_int("FLASH", flash);
	nSettings::set_int("BEEP", beep);

	LoadJoinNotifySettings();
}

static void ExecuteOptions(){
	if (!hosting)
		return;

	int maxplayers = nSettings::get_int("MAXPLAYERS");
	int maxping = nSettings::get_int("MAXPING");

	char cmd[128];
	if (maxplayers != -1){
		wsprintf(cmd, "/maxusers %d", maxplayers);
		kaillera_game_chat_send(cmd);
	}
	if (maxping != -1){
		wsprintf(cmd, "/maxping %d", maxping);
		kaillera_game_chat_send(cmd);
	}
}

static INT_PTR CALLBACK OptionsDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam){
	(void)lParam;
	switch (uMsg){
		case WM_INITDIALOG:
			LoadOptions(hDlg);
			return (INT_PTR)TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam)){
				case IDOK:
					SaveOptions(hDlg);
					ExecuteOptions();
					EndDialog(hDlg, 0);
					return (INT_PTR)TRUE;
				case IDCANCEL:
					EndDialog(hDlg, 0);
					return (INT_PTR)TRUE;
			}
			break;
	}
	return (INT_PTR)FALSE;
}
//===========================================================================================

LRESULT CALLBACK KailleraServerDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		{
			RECT originalClientRect = { 0, 0, 0, 0 };
			GetClientRect(hDlg, &originalClientRect);

			// Add WS_EX_APPWINDOW and remove WS_EX_TOOLWINDOW to show in Windows taskbar
			LONG_PTR exStyle = GetWindowLongPtr(hDlg, GWL_EXSTYLE);
			exStyle |= WS_EX_APPWINDOW;
			exStyle &= ~WS_EX_TOOLWINDOW;
			SetWindowLongPtr(hDlg, GWL_EXSTYLE, exStyle);

			// Enable standard resize/maximize chrome for the Kaillera main window.
			LONG_PTR style = GetWindowLongPtr(hDlg, GWL_STYLE);
			style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
			SetWindowLongPtr(hDlg, GWL_STYLE, style);
			SetWindowPos(hDlg, NULL, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

			// Keep the original client area so bottom-row controls don't get clipped after enabling WS_THICKFRAME.
			RECT resizedClientRect = { 0, 0, 0, 0 };
			if (GetClientRect(hDlg, &resizedClientRect)) {
				const int originalClientWidth = originalClientRect.right - originalClientRect.left;
				const int originalClientHeight = originalClientRect.bottom - originalClientRect.top;
				const int resizedClientWidth = resizedClientRect.right - resizedClientRect.left;
				const int resizedClientHeight = resizedClientRect.bottom - resizedClientRect.top;
				const int growWidth = originalClientWidth - resizedClientWidth;
				const int growHeight = originalClientHeight - resizedClientHeight;
				if (growWidth > 0 || growHeight > 0) {
					RECT windowRect = { 0, 0, 0, 0 };
					if (GetWindowRect(hDlg, &windowRect)) {
						const int windowWidth = windowRect.right - windowRect.left;
						const int windowHeight = windowRect.bottom - windowRect.top;
						SetWindowPos(hDlg, NULL, 0, 0, windowWidth + growWidth, windowHeight + growHeight,
							SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
					}
				}
			}

			kaillera_sdlg = hDlg;
			LoadJoinNotifySettings();
			{
				char xx[256];
				wsprintf(xx, "Connecting to %s", kaillera_sdlg_NAME);
				SetWindowText(hDlg, xx);
			}
			kaillera_sdlg_userslv.handle = GetDlgItem(hDlg, LV_ULIST);
			kaillera_sdlg_userslv.AddColumn("Name", 80);
			kaillera_sdlg_userslv.AddColumn("Ping", 35);
			kaillera_sdlg_userslv.AddColumn("UID", 44);
			kaillera_sdlg_userslv.AddColumn("Status", 85);
			kaillera_sdlg_userslv.AddColumn("Connection", 0);
			kaillera_sdlg_userslv.FullRowSelect();
			kaillera_sdlg_userslvColumn = 1;
			kaillera_sdlg_userslvColumnOrder[1] = 1;

			kaillera_sdlg_gameslv.handle = GetDlgItem(hDlg, LV_GLIST);
			kaillera_sdlg_gameslv.AddColumn("Game", 285);
			kaillera_sdlg_gameslv.AddColumn("GameID", 60);
			kaillera_sdlg_gameslv.AddColumn("Emulator", 130);
			kaillera_sdlg_gameslv.AddColumn("User", 150);
			kaillera_sdlg_gameslv.AddColumn("Status", 50);
			kaillera_sdlg_gameslv.AddColumn("Users", 45);
			kaillera_sdlg_gameslv.FullRowSelect();
			kaillera_sdlg_gameslvColumn = 4;
			kaillera_sdlg_gameslvColumnOrder[4] = 5;

			kaillera_sdlg_partchat = GetDlgItem(hDlg, RE_PART);
			re_enable_hyperlinks(kaillera_sdlg_partchat);

			//Sleep(100);
			
			kaillera_sdlg_CHK_REC = GetDlgItem(hDlg, CHK_REC);
			kaillera_sdlg_RE_GCHAT = GetDlgItem(hDlg, RE_GCHAT);
			kaillera_sdlg_TXT_GINP = GetDlgItem(hDlg, TXT_GINP);
			kaillera_sdlg_LV_GULIST.handle = GetDlgItem(hDlg, LV_GULIST);
			kaillera_sdlg_BTN_START = GetDlgItem(hDlg, BTN_START);
			kaillera_sdlg_BTN_DROP = GetDlgItem(hDlg, BTN_DROP);
			kaillera_sdlg_BTN_LEAVE = GetDlgItem(hDlg, BTN_LEAVE);
			kaillera_sdlg_BTN_KICK = GetDlgItem(hDlg, BTN_KICK);
			kaillera_sdlg_BTN_LAGSTAT = GetDlgItem(hDlg, BTN_LAGSTAT);
			kaillera_sdlg_BTN_OPTIONS = GetDlgItem(hDlg, BTN_OPTIONS);
			kaillera_sdlg_BTN_ADVERTISE = GetDlgItem(hDlg, BTN_ADVERTISE);
			kaillera_sdlg_ST_SPEED = GetDlgItem(hDlg, ST_SPEED);
			kaillera_sdlg_BTN_GCHAT = GetDlgItem(hDlg, BTN_GCHAT);
			kaillera_sdlg_ST_DELAY = GetDlgItem(hDlg, ST_DELAY);
			kaillera_sdlg_MINGUIUPDATE = GetDlgItem(hDlg, CHK_MINGUIUPD);
			kaillera_sdlg_TXT_MSG = GetDlgItem(hDlg, TXT_MSG);
			kaillera_sdlg_JOINMSG_LBL = GetDlgItem(hDlg, IDC_JOINMSG_LBL);
			LoadJoinMessageSettingForContext(false);

			re_enable_hyperlinks(kaillera_sdlg_RE_GCHAT);


			kaillera_sdlg_LV_GULIST.AddColumn("Nick", 100);
			kaillera_sdlg_LV_GULIST.AddColumn("Ping", 60);
			kaillera_sdlg_LV_GULIST.AddColumn("Connection", 0);
			kaillera_sdlg_LV_GULIST.AddColumn("Delay", 120);
			kaillera_sdlg_LV_GULIST.FullRowSelect();

			kaillera_sdlgNormalMode();
			kaillera_sdlg_create_games_list_menu();
			kaillera_core_connect(kaillera_sdlg_ip, kaillera_sdlg_port);

			kaillera_sdlg_sipd_timer = SetTimer(hDlg, 0, 1000, 0);

			g_hidden_column_track_active = false;
			g_hidden_column_track_list = NULL;
			g_hidden_column_track_item = -1;
			g_hidden_column_track_prev_item = -1;
			g_hidden_column_track_start_prev_width = 0;
			g_hidden_column_track_start_cursor_x = 0;

			MINGUIUPDATE = false;
			InitializeKailleraDialogResizeLayout(hDlg);
			LoadKailleraDialogLayout(hDlg);

			return 0;
			
		}
	case WM_GETMINMAXINFO:
		if (g_kaillera_resize_initialized) {
			MINMAXINFO* minMaxInfo = (MINMAXINFO*)lParam;
			minMaxInfo->ptMinTrackSize.x = g_kaillera_min_track_size.cx;
			minMaxInfo->ptMinTrackSize.y = g_kaillera_min_track_size.cy;
			return 0;
		}
		break;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) {
			RECT clientRect;
			if (GetClientRect(hDlg, &clientRect)) {
				ApplyKailleraDialogResizeLayout(hDlg,
					clientRect.right - clientRect.left,
					clientRect.bottom - clientRect.top);
			}
		}
		break;
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT && g_kaillera_resize_initialized) {
			POINT cursorPos;
			if (GetCursorPos(&cursorPos)) {
				ScreenToClient(hDlg, &cursorPos);
				if (IsTopSplitterHit(hDlg, cursorPos.x, cursorPos.y) || IsBottomSplitterHit(hDlg, cursorPos.x, cursorPos.y)) {
					SetCursor(LoadCursor(NULL, IDC_SIZEWE));
					return TRUE;
				}
			}
		}
		break;
	case WM_LBUTTONDOWN:
		if (g_kaillera_resize_initialized) {
			const int mouseX = (int)(short)LOWORD(lParam);
			const int mouseY = (int)(short)HIWORD(lParam);
			if (IsTopSplitterHit(hDlg, mouseX, mouseY)) {
				g_kaillera_active_splitter = KAILLERA_SPLITTER_TOP;
				SetCapture(hDlg);
				return 0;
			}
			if (IsBottomSplitterHit(hDlg, mouseX, mouseY)) {
				g_kaillera_active_splitter = KAILLERA_SPLITTER_BOTTOM;
				SetCapture(hDlg);
				return 0;
			}
		}
		break;
	case WM_MOUSEMOVE:
		if (g_kaillera_active_splitter != KAILLERA_SPLITTER_NONE && g_kaillera_resize_initialized) {
			const int mouseX = (int)(short)LOWORD(lParam);
			RECT clientRect;
			if (GetClientRect(hDlg, &clientRect)) {
				const int clientWidth = clientRect.right - clientRect.left;
				int maxRightWidth;
				int newRightWidth;
				if (g_kaillera_active_splitter == KAILLERA_SPLITTER_TOP) {
					newRightWidth = clientWidth - g_kaillera_top_right_margin - mouseX;
					maxRightWidth = clientWidth - g_kaillera_top_right_margin - 140;
					if (maxRightWidth < 120)
						maxRightWidth = 120;
					newRightWidth = ClampInt(newRightWidth, 120, maxRightWidth);
					if (newRightWidth != g_kaillera_top_right_width) {
						g_kaillera_top_right_width = newRightWidth;
						ApplyKailleraDialogResizeLayout(hDlg, clientWidth, clientRect.bottom - clientRect.top);
					}
				} else if (g_kaillera_active_splitter == KAILLERA_SPLITTER_BOTTOM) {
					newRightWidth = clientWidth - g_kaillera_bottom_right_margin - mouseX;
					maxRightWidth = clientWidth - g_kaillera_bottom_right_margin - 140;
					if (maxRightWidth < 120)
						maxRightWidth = 120;
					newRightWidth = ClampInt(newRightWidth, 120, maxRightWidth);
					if (newRightWidth != g_kaillera_bottom_right_width) {
						g_kaillera_bottom_right_width = newRightWidth;
						ApplyKailleraDialogResizeLayout(hDlg, clientWidth, clientRect.bottom - clientRect.top);
					}
				}
			}
			return 0;
		}
		break;
	case WM_LBUTTONUP:
		if (g_kaillera_active_splitter != KAILLERA_SPLITTER_NONE) {
			g_kaillera_active_splitter = KAILLERA_SPLITTER_NONE;
			ReleaseCapture();
			return 0;
		}
		break;
	case WM_CAPTURECHANGED:
		g_kaillera_active_splitter = KAILLERA_SPLITTER_NONE;
		break;
	case WM_TIMER:
		{
			if (kaillera_sdlg_MODE==1) {
				char xx[256];
				if (kaillera_is_connected())
					wsprintf(xx, "Connected to %s (%i users & %i games)", kaillera_sdlg_NAME, kaillera_sdlg_userslv.RowsCount(), kaillera_sdlg_gameslv.RowsCount());
				else
					wsprintf(xx, "Connecting to %s", kaillera_sdlg_NAME);
				SetWindowText(kaillera_sdlg, xx);
			} else {
				char xx[256];
				wsprintf(xx, "Game %s", GAME);
				SetWindowText(kaillera_sdlg, xx);

				int jf;
				if ((jf = kaillera_get_frames_count()) != kaillera_sdlg_frameno) {
					char xxx[32];
					wsprintf(xxx,"%i fps/%i pps", jf - kaillera_sdlg_frameno, SOCK_SEND_PACKETS - kaillera_sdlg_pps);
					SetWindowText(kaillera_sdlg_ST_SPEED, xxx);
					kaillera_sdlg_frameno = jf;
					kaillera_sdlg_pps = SOCK_SEND_PACKETS;
				}
				if ((jf = kaillera_get_delay()) != kaillera_sdlg_delay) {
					char xxx[32];
					wsprintf(xxx,"%i frames", kaillera_sdlg_delay = jf);
					SetWindowText(kaillera_sdlg_ST_DELAY, xxx);
				}
			}
		}
		break;
	case WM_CLOSE:
		SaveJoinMessageSetting();
		SaveKailleraDialogLayout(hDlg);

		KillTimer(hDlg, kaillera_sdlg_sipd_timer);
		
		kaillera_sdlg_destroy_games_list_menu();

		if (KSSDFA.state != 0)
			kaillera_end_game();

		inGame = false;

		KSSDFA.state = 0;
		KSSDFA.input = KSSDFA_END_GAME;

		EndDialog(hDlg, 0);

		break;
			case WM_COMMAND:
			switch (LOWORD(wParam)) {
			case TXT_MSG:
				if (HIWORD(wParam) == EN_KILLFOCUS)
					SaveJoinMessageSetting();
				break;
				case IDC_CREATE:
					if (kaillera_sdlg_MODE == 1){
						kaillera_sdlg_show_games_list_menu(hDlg);
					} else {
						if (!kaillera_sdlg_toggle)
							kaillera_sdlgNormalMode(true);
						else
							kaillera_sdlgGameMode(true);
					}
					break;
				case IDC_CHAT:
					{
						char buffrr[2024];
						GetWindowText(GetDlgItem(hDlg, TXT_CHAT), buffrr, 2024);
						size_t l = strlen(buffrr);
						if (l>0) {
							size_t p = (l < 127) ? l : 127;
							char sbf[128];
							memcpy(sbf, buffrr, p);
							sbf[p] = 0;
							kaillera_ui_chat_send(sbf);
						if (l > p) {
							l -= p;
							memcpy(buffrr, buffrr+p, l+1);
						} else
							buffrr[0] = 0;
							SetWindowText(GetDlgItem(hDlg, TXT_CHAT), buffrr);
							break;
						}
						
					}				
				case BTN_GCHAT:
					{
						char buffrr[2024];
						GetWindowText(kaillera_sdlg_TXT_GINP, buffrr, 2024);
						size_t l = strlen(buffrr);
						if (l>0) {
							// Handle /halfdelay command locally
							if (strcmp(buffrr, "/halfdelay") == 0) {
								kaillera_30fps_mode = !kaillera_30fps_mode;
								kaillera_core_debug("Half delay mode %s (for 30fps games: Mario Kart, 1080, THPS)", kaillera_30fps_mode ? "ENABLED" : "DISABLED");
								SetWindowText(kaillera_sdlg_TXT_GINP, "");
								break;
							}
							size_t p = (l < 127) ? l : 127;
							char sbf[128];
							memcpy(sbf, buffrr, p);
							sbf[p] = 0;
							kaillera_game_chat_send(sbf);
						if (l > p) {
							l -= p;
							memcpy(buffrr, buffrr+p, l+1);
						} else
							buffrr[0] = 0;
						SetWindowText(kaillera_sdlg_TXT_GINP, buffrr);
					}
					break;
				}
			case BTN_LEAVE:
				SaveJoinMessageSetting();
				kaillera_leave_game();
				kaillera_sdlgNormalMode();
				kaillera_30fps_mode = 0;
				KSSDFA.input = KSSDFA_END_GAME;
				KSSDFA.state = 0;
				break;
			case BTN_DROP:
				kaillera_game_drop();
				break;
			case BTN_START:
				if (IsNonGameLobbyName(GAME)) {
					kaillera_error_callback("This lobby has no game to start.");
				} else {
					kaillera_start_game();
				}
				break;
			case BTN_LAGSTAT:
				{
					char cmd[] = "/lagstat";
					kaillera_game_chat_send(cmd);
				}
				break;
			case BTN_OPTIONS:
				DialogBox(hx, (LPCTSTR)KAILLERA_OPTIONS, hDlg, (DLGPROC)OptionsDialogProc);
				break;
			case BTN_ADVERTISE:
				{
					char ad[512];
					char hostname[128];
					hostname[0] = 0;
					int maxplayers = kaillera_sdlg_LV_GULIST.RowsCount();
					if (maxplayers > 0) {
						kaillera_sdlg_LV_GULIST.CheckRow(hostname, (int)sizeof(hostname), 0, 0);
					}

					if (hosting) {
						wsprintf(ad, "%s - %d player(s)", GAME, maxplayers);
					} else {
						wsprintf(ad, "<%s> | %s - %d player(s)", hostname, GAME, maxplayers);
					}

					kaillera_chat_send(ad);
				}
				break;
				case BTN_KICK:
					{
					int x = kaillera_sdlg_LV_GULIST.SelectedRow();
					if (x > 0 && x < kaillera_sdlg_LV_GULIST.RowsCount()) {
						kaillera_kick_user((unsigned short)(UINT_PTR)kaillera_sdlg_LV_GULIST.RowNo(x));
					}
					}
					break;
				case CHK_MINGUIUPD:
					// Option removed from UI; always treat as unchecked.
					MINGUIUPDATE = false;
					break;
			};
			break;
			case WM_NOTIFY:
			if (re_handle_link_click(lParam))
				break;
			{
				LPNMHDR hdr = (LPNMHDR)lParam;
				HWND gamesHeader = (kaillera_sdlg_gameslv.handle != NULL) ? ListView_GetHeader(kaillera_sdlg_gameslv.handle) : NULL;
				HWND usersHeader = (kaillera_sdlg_userslv.handle != NULL) ? ListView_GetHeader(kaillera_sdlg_userslv.handle) : NULL;
				HWND lobbyHeader = (kaillera_sdlg_LV_GULIST.handle != NULL) ? ListView_GetHeader(kaillera_sdlg_LV_GULIST.handle) : NULL;
				HWND headerOwnerList = NULL;
				if (hdr->hwndFrom == gamesHeader)
					headerOwnerList = kaillera_sdlg_gameslv.handle;
				else if (hdr->hwndFrom == usersHeader)
					headerOwnerList = kaillera_sdlg_userslv.handle;
				else if (hdr->hwndFrom == lobbyHeader)
					headerOwnerList = kaillera_sdlg_LV_GULIST.handle;

				if ((hdr->code == HDN_BEGINTRACKA || hdr->code == HDN_BEGINTRACKW) && headerOwnerList != NULL) {
					NMHEADERA* nmh = (NMHEADERA*)lParam;
					g_hidden_column_track_active = false;
					if (!g_kaillera_allow_zero_width_columns && nmh->iItem >= 0 &&
						ListView_GetColumnWidth(headerOwnerList, nmh->iItem) == 0) {
						const int prevVisible = FindPreviousVisibleColumn(headerOwnerList, nmh->iItem);
						if (prevVisible >= 0) {
							POINT cursorPos;
							if (GetCursorPos(&cursorPos)) {
								g_hidden_column_track_active = true;
								g_hidden_column_track_list = headerOwnerList;
								g_hidden_column_track_item = nmh->iItem;
								g_hidden_column_track_prev_item = prevVisible;
								g_hidden_column_track_start_prev_width = ListView_GetColumnWidth(headerOwnerList, prevVisible);
								g_hidden_column_track_start_cursor_x = cursorPos.x;
							}
						}
					}
				}

				if ((hdr->code == HDN_ENDTRACKA || hdr->code == HDN_ENDTRACKW || hdr->code == HDN_ENDDRAG) &&
					headerOwnerList != NULL) {
					g_hidden_column_track_active = false;
					g_hidden_column_track_list = NULL;
					g_hidden_column_track_item = -1;
					g_hidden_column_track_prev_item = -1;
				}

				if ((hdr->code == HDN_TRACKA || hdr->code == HDN_TRACKW) &&
					headerOwnerList != NULL) {
					NMHEADERA* nmh = (NMHEADERA*)lParam;
					if (!g_kaillera_allow_zero_width_columns && g_hidden_column_track_active &&
						headerOwnerList == g_hidden_column_track_list &&
						nmh->iItem == g_hidden_column_track_item &&
						g_hidden_column_track_prev_item >= 0) {
						POINT cursorPos;
						if (GetCursorPos(&cursorPos)) {
							int nextWidth = g_hidden_column_track_start_prev_width + (cursorPos.x - g_hidden_column_track_start_cursor_x);
							if (nextWidth < KAILLERA_MIN_COLUMN_WIDTH)
								nextWidth = KAILLERA_MIN_COLUMN_WIDTH;
							ListView_SetColumnWidth(headerOwnerList, g_hidden_column_track_prev_item, nextWidth);
						}
						if (nmh->pitem != NULL && (nmh->pitem->mask & HDI_WIDTH) != 0)
							nmh->pitem->cxy = 0;
					}
				}

				if ((hdr->code == HDN_ITEMCHANGINGA || hdr->code == HDN_ITEMCHANGINGW) &&
					headerOwnerList != NULL) {
					NMHEADERA* nmh = (NMHEADERA*)lParam;
					if (nmh->pitem != NULL && (nmh->pitem->mask & HDI_WIDTH) != 0) {
						int currentWidth = -1;
						if (nmh->iItem >= 0)
							currentWidth = ListView_GetColumnWidth(headerOwnerList, nmh->iItem);

						// Hidden columns can only be changed by explicit menu/load paths.
						if (!g_kaillera_allow_zero_width_columns && currentWidth == 0) {
							// If user drags a border for a hidden column, resize the previous visible column instead.
							const bool trackingThisHiddenColumn =
								g_hidden_column_track_active &&
								headerOwnerList == g_hidden_column_track_list &&
								nmh->iItem == g_hidden_column_track_item &&
								g_hidden_column_track_prev_item >= 0;

							if (!trackingThisHiddenColumn) {
								const int prevVisible = FindPreviousVisibleColumn(headerOwnerList, nmh->iItem);
								if (prevVisible >= 0) {
									POINT cursorPos;
									if (GetCursorPos(&cursorPos)) {
										g_hidden_column_track_active = true;
										g_hidden_column_track_list = headerOwnerList;
										g_hidden_column_track_item = nmh->iItem;
										g_hidden_column_track_prev_item = prevVisible;
										g_hidden_column_track_start_prev_width = ListView_GetColumnWidth(headerOwnerList, prevVisible);
										// If the notification already has a positive width delta, account for it so the first drag event takes effect.
										const int initialDelta = (nmh->pitem->cxy > 0) ? nmh->pitem->cxy : 0;
										g_hidden_column_track_start_cursor_x = cursorPos.x - initialDelta;
									}
								}
							}

							if (g_hidden_column_track_active &&
								headerOwnerList == g_hidden_column_track_list &&
								nmh->iItem == g_hidden_column_track_item &&
								g_hidden_column_track_prev_item >= 0) {
								POINT cursorPos;
								if (GetCursorPos(&cursorPos)) {
									int nextWidth = g_hidden_column_track_start_prev_width + (cursorPos.x - g_hidden_column_track_start_cursor_x);
									if (nextWidth < KAILLERA_MIN_COLUMN_WIDTH)
										nextWidth = KAILLERA_MIN_COLUMN_WIDTH;
									ListView_SetColumnWidth(headerOwnerList, g_hidden_column_track_prev_item, nextWidth);
								}
							}
							nmh->pitem->cxy = 0;
						} else if (nmh->pitem->cxy == 0 && !g_kaillera_allow_zero_width_columns) {
							nmh->pitem->cxy = (currentWidth == 0) ? 0 : KAILLERA_MIN_COLUMN_WIDTH;
						} else if (nmh->pitem->cxy > 0 && nmh->pitem->cxy < KAILLERA_MIN_COLUMN_WIDTH) {
							nmh->pitem->cxy = KAILLERA_MIN_COLUMN_WIDTH;
						}
					}
				}
				if (hdr->code == NM_RCLICK && hdr->hwndFrom == gamesHeader) {
					ShowColumnVisibilityMenu(hDlg, kaillera_sdlg_gameslv.handle, g_games_column_menu, g_games_column_restore_widths, 6);
					break;
				}
				if (hdr->code == NM_RCLICK && hdr->hwndFrom == usersHeader) {
					ShowColumnVisibilityMenu(hDlg, kaillera_sdlg_userslv.handle, g_users_column_menu, g_users_column_restore_widths, 5);
					break;
				}
				if (hdr->code == NM_RCLICK && hdr->hwndFrom == lobbyHeader) {
					ShowColumnVisibilityMenu(hDlg, kaillera_sdlg_LV_GULIST.handle, g_lobby_column_menu, g_lobby_column_restore_widths, 4);
					break;
				}
			}
			if(((LPNMHDR)lParam)->code==NM_DBLCLK && ((LPNMHDR)lParam)->hwndFrom==kaillera_sdlg_gameslv.handle){
				kailelra_sdlg_join_selected_game();
			}
			if(((LPNMHDR)lParam)->code==LVN_COLUMNCLICK && ((LPNMHDR)lParam)->hwndFrom==kaillera_sdlg_gameslv.handle){
				kaillera_sdlg_gameslvSort(((NMLISTVIEW*)lParam)->iSubItem);
			}
			if(((LPNMHDR)lParam)->code==LVN_COLUMNCLICK && ((LPNMHDR)lParam)->hwndFrom==kaillera_sdlg_userslv.handle){
				kaillera_sdlg_userslvSort(((NMLISTVIEW*)lParam)->iSubItem);
			}
			if(((LPNMHDR)lParam)->code==NM_RCLICK && (((LPNMHDR)lParam)->hwndFrom==kaillera_sdlg_gameslv.handle)){
				kaillera_sdlg_show_games_list_menu(hDlg, kaillera_sdlg_gameslv.SelectedRow() >= 0 && kaillera_sdlg_gameslv.SelectedRow() < kaillera_sdlg_gameslv.RowsCount());
			}
			if(((LPNMHDR)lParam)->code==NM_RCLICK && (((LPNMHDR)lParam)->hwndFrom==kaillera_sdlg_userslv.handle)){
				kaillera_sdlg_show_users_list_menu(hDlg);
			}
			break;
		};
		return 0;
}



HWND kaillera_ssdlg;

static void UpdateModeRadioButtons(HWND hDlg){
	int mode = get_active_mode_index();
	if (mode < 0 || mode > 2)
		mode = 1;
	CheckRadioButton(hDlg, RB_MODE_P2P, RB_MODE_PLAYBACK, RB_MODE_P2P + mode);
}

void ConnectToServer(char * ip, int port, HWND pDlg,char * name) {
	KAILLERA_CORE_INITIALIZED = true;

	strncpy(kaillera_sdlg_NAME, (name != NULL) ? name : "", sizeof(kaillera_sdlg_NAME) - 1);
	kaillera_sdlg_NAME[sizeof(kaillera_sdlg_NAME) - 1] = 0;

	// Get ping spoof setting from dropdown (0=auto, 1-9=frame delay)
	int fdly_index = (int)SendMessage(GetDlgItem(kaillera_ssdlg, IDC_QUITMSG), CB_GETCURSEL, 0, 0);
	if (fdly_index > 0 && fdly_index <= 9) {
		// Calculate spoof ping: x * 16 - 8 = ms
		kaillera_spoof_ping = fdly_index * 16 - 8;
		kaillera_set_spoof_ping(kaillera_spoof_ping);
	} else {
		kaillera_set_spoof_ping(0);  // Auto - no spoofing
	}

	char un[32];
	GetWindowText(GetDlgItem(kaillera_ssdlg, IDC_USRNAME), un, 32);
	un[31]=0;
	const char conset = 1; // Always treat as LAN (highest packet rate)
		if (kaillera_core_initialize(0, APP, un, conset)) {
			//Sleep(150);
			kaillera_sdlg_port = port;
			strncpy(kaillera_sdlg_ip, (ip != NULL) ? ip : "", sizeof(kaillera_sdlg_ip) - 1);
			kaillera_sdlg_ip[sizeof(kaillera_sdlg_ip) - 1] = 0;

		// Hide the server selection dialog (and intermediate dialog if different)
		ShowWindow(kaillera_ssdlg, SW_HIDE);
		if (pDlg != kaillera_ssdlg) {
			ShowWindow(pDlg, SW_HIDE);
		}

		DialogBox(hx, (LPCTSTR)KAILLERA_SDLG, pDlg, (DLGPROC)KailleraServerDialogProc);
		//disconnect
		char quitmsg[128] = "Open Kaillera - n02 " KAILLERA_VERSION;
		// Frame delay field repurposed - use default quit message
		kaillera_disconnect(quitmsg);
		kaillera_core_cleanup();
		kaillera_30fps_mode = 0;

		KSSDFA.state = 0;
		KSSDFA.input = KSSDFA_END_GAME;
		//core cleanup

		// Show the server selection dialog again
		ShowWindow(kaillera_ssdlg, SW_SHOW);
		if (pDlg != kaillera_ssdlg) {
			ShowWindow(pDlg, SW_SHOW);
		}
	} else {
		MessageBox(pDlg, "Core Initialization Failed", 0, 0);
	}
	KAILLERA_CORE_INITIALIZED = false;
}
//===============================================================================
//===============================================================================
//===============================================================================

typedef struct {
	char servname[32];
	char hostname[128];
}KLSNST;

KLSNST KLSNST_temp;
odlist<KLSNST, 32> KLSList;
nLVw KLSListLv;

// Forward declaration
void KLSListSave();

void KLSListDisplay(){
	KLSListLv.DeleteAllRows();
	for (int x = 0; x< KLSList.length; x++){
		KLSListLv.AddRow(KLSList[x].servname, x);
		KLSListLv.FillRow(KLSList[x].hostname, 1, x);
		KLSListLv.FillRow("-", 2, x);  // Ping
	}
}

void KLSListRefreshStatus(){
	for (int x = 0; x < KLSList.length; x++){
		char host_copy[128];
		strncpy(host_copy, KLSList[x].hostname, 127);
		host_copy[127] = 0;

		char* host = host_copy;
		int port = 27888;

		// Parse host:port
		char* colon = strchr(host_copy, ':');
		if (colon) {
			*colon = 0;
			port = atoi(colon + 1);
			if (port == 0) port = 27888;
		}

		int ping = kaillera_ping_server(host, port, 500);
		char pingstr[32];
		if (ping < 500)
			wsprintf(pingstr, "%ims", ping);
		else
			strcpy(pingstr, "N/A");
		KLSListLv.FillRow(pingstr, 2, x);
	}
}





LRESULT CALLBACK KLSListModifyDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg==WM_INITDIALOG){
		SetWindowText(GetDlgItem(hDlg, IDC_NAME), KLSNST_temp.servname);
		SetWindowText(GetDlgItem(hDlg, IDC_IP), KLSNST_temp.hostname);
		if (lParam)	SetWindowText(hDlg, "Edit");
		else SetWindowText(hDlg, "Add");
	} else if (uMsg==WM_CLOSE){
		EndDialog(hDlg, 0);
	} else if (uMsg==WM_COMMAND){
		if (LOWORD(wParam)==IDOK){
			GetWindowText(GetDlgItem(hDlg, IDC_NAME), KLSNST_temp.servname,32);
			GetWindowText(GetDlgItem(hDlg, IDC_IP), KLSNST_temp.hostname,128);
			EndDialog(hDlg, 1);
		} else if (LOWORD(wParam)==IDCANCEL)
			EndDialog(hDlg, 0);
	}
	return 0;
}


void KLSListEdit(){
	int i = KLSListLv.SelectedRow();
	if (i>=0&&i<KLSList.length) {
		KLSNST_temp = KLSList[i];
		if (DialogBoxParam(hx, (LPCTSTR)P2P_ITEM_EDIT, kaillera_ssdlg, (DLGPROC)KLSListModifyDlgProc, 1)){
			KLSList.set(KLSNST_temp, i);
			KLSListSave();
		}
	}
	KLSListDisplay();
}


void KLSListAdd(){
	memset(&KLSNST_temp, 0, sizeof(KLSNST));
	if (DialogBoxParam(hx, (LPCTSTR)P2P_ITEM_EDIT, kaillera_ssdlg, (DLGPROC)KLSListModifyDlgProc, 0)){
		KLSList.add(KLSNST_temp);
		KLSListSave();
	}
	KLSListDisplay();
}


void KLSListAdd(char * name, char * hostt){
	strncpy(KLSNST_temp.servname, (name != NULL) ? name : "", sizeof(KLSNST_temp.servname) - 1);
	KLSNST_temp.servname[sizeof(KLSNST_temp.servname) - 1] = 0;
	strncpy(KLSNST_temp.hostname, (hostt != NULL) ? hostt : "", sizeof(KLSNST_temp.hostname) - 1);
	KLSNST_temp.hostname[sizeof(KLSNST_temp.hostname) - 1] = 0;
	KLSList.add(KLSNST_temp);
	KLSListSave();
	KLSListDisplay();
}


void KLSListPING(){
	int i = KLSListLv.SelectedRow();
	if (i>=0&&i<KLSList.length) {
		
		KLSNST xx = KLSList[i];
		
		char * host = xx.hostname;
		int port = 27888;
		while (*++host != ':' && *host != 0);
		if (*host == ':') {
			*host++ = 0x00;
			port = atoi(host);
			port = port==0?27886:port;
		}
		host = xx.hostname;

		int ping = kaillera_ping_server(host, port);
		char pingstr[128];
		wsprintf(pingstr, "%ims", ping);
		KLSListLv.FillRow(pingstr, 2, i);  // Ping column

		//CreateProcess(0, "cmd /k tracert", 0, 0, 0, 0, 0, 0, 0, 0);
	}
}

void KLSListTRACE() {
	int i = KLSListLv.SelectedRow();
	if (i>=0&&i<KLSList.length) {

		KLSNST xx = KLSList[i];

		char* host = xx.hostname;
		int port = 27888;
		while (*++host != ':' && *host != 0);
		if (*host == ':') {
			*host++ = 0x00;
			port = atoi(host);
			port = port == 0?27888 : port;
		}
		host = xx.hostname;

		char pingstr[128];
		wsprintf(pingstr, "cmd /k tracert -w 250 %s", host);

		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));
		CreateProcess(NULL, pingstr, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}


void KLSListDelete(){
	int i = KLSListLv.SelectedRow();
	if (i>=0&&i<KLSList.length) {
		KLSList.removei(i);
		KLSListSave();
	}
	KLSListDisplay();
}

void KLSListConnect(){
	int i = KLSListLv.SelectedRow();
	if (i>=0&&i<KLSList.length) {
		KLSNST xx = KLSList[i];
		
		char * host = xx.hostname;
		int port = 27888;
		while (*++host != ':' && *host != 0);
		if (*host == ':') {
			*host++ = 0x00;
			port = atoi(host);
			port = port==0?27888:port;
		}
		host = xx.hostname;

		ConnectToServer(host, port, kaillera_ssdlg, xx.servname);
	}
}


void KLSListSelect(HWND hDlg){
	int i = KLSListLv.SelectedRow();
	if (i>=0&&i<KLSList.length)
		SetDlgItemText(hDlg, IDC_IP, KLSList[i].hostname);
}

void KLSListDblClick(HWND hDlg){
	int i = KLSListLv.SelectedRow();
	if (i>=0&&i<KLSList.length)
		SetDlgItemText(hDlg, IDC_IP, KLSList[i].hostname);
}

void KLSListLoad(){
	KLSList.clear();

	int initialized = nSettings::get_int("SLInit", 0);
	if (initialized) {
		// Load servers from n02.ini
		int count = nSettings::get_int("SLC", 0);
		for (int x=1;x<=count;x++){
			char idt[32];
			KLSNST sx;
			wsprintf(idt, "SLS%i", x);
			nSettings::get_str(idt,sx.servname, "UserName");
			wsprintf(idt, "SLH%i", x);
			nSettings::get_str(idt,sx.hostname, "127.0.0.1");
			KLSList.add(sx);
		}
	} else {
		// Seed default servers on first run
		KLSNST sx;
		strcpy(sx.servname, "Chicago SSB"); strcpy(sx.hostname, "92.38.176.115:27888"); KLSList.add(sx);
		strcpy(sx.servname, "SSBL Georgia Netplay"); strcpy(sx.hostname, "45.61.60.96:27888"); KLSList.add(sx);
		strcpy(sx.servname, "Miami Secret"); strcpy(sx.hostname, "185.144.159.190:27888"); KLSList.add(sx);
		strcpy(sx.servname, "Seattle"); strcpy(sx.hostname, "23.227.163.253:27888"); KLSList.add(sx);
		strcpy(sx.servname, "San Fran"); strcpy(sx.hostname, "165.227.60.3:27888"); KLSList.add(sx);
		nSettings::set_int("SLInit", 1);
		KLSListSave();
	}
	KLSListDisplay();
	KLSListRefreshStatus();
}

void KLSListSave(){
	nSettings::set_int("SLC",KLSList.length);
	for (int x=0;x<KLSList.length;x++){
		char idt[32];
		KLSNST sx = KLSList[x];
		wsprintf(idt, "SLS%i", x+1);
		nSettings::set_str(idt,sx.servname);
		wsprintf(idt, "SLH%i", x+1);
		nSettings::set_str(idt,sx.hostname);
	}
}
//---------------------------------------


LRESULT CALLBACK AboutDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CLOSE:
		EndDialog(hDlg, 0);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case BTN_CLOSE:
			EndDialog(hDlg, 0);
			break;
		case BTN_LICENSE:
			ShellExecute(NULL, NULL, "http://p2p.kaillera.ru/license.html", NULL, NULL, SW_SHOWNORMAL);
			break;
		case BTN_OKAI:
			ShellExecute(NULL, NULL, "http://sourceforge.net/projects/okai/", NULL, NULL, SW_SHOWNORMAL);
			break;
		case BTN_USEAGE:
			ShellExecute(NULL, NULL, "http://p2p.kaillera.ru/", NULL, NULL, SW_SHOWNORMAL);
			break;
		};
		break;
	};
	return 0;
}
void ShowAboutDialog(HWND hDlg) {
	DialogBox(hx, (LPCTSTR)KAILLERA_ABOUT, hDlg, (DLGPROC)AboutDialogProc);
}



//////////////

LRESULT CALLBACK CustomIPDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_CLOSE:
			EndDialog(hDlg, 0);
			break;
		case WM_COMMAND:
			if (LOWORD(wParam)==BTN_CONNECT){
				char * host = KLSNST_temp.hostname;
				GetWindowText(GetDlgItem(hDlg, IDC_IP), host, 128);
				int port = 27888;
				while (*++host != ':' && *host != 0);
				if (*host == ':') {
					*host++ = 0x00;
					port = atoi(host);
					port = port==0?27888:port;
				}
				host = KLSNST_temp.hostname;
				if (strlen(host) > 2)
					ConnectToServer(host, port, kaillera_ssdlg,host);
				EndDialog(hDlg, 0);
			}
			break;
	};
	return 0;
}
void ShowCustomIPDialog(HWND hDlg) {
	DialogBox(hx, (LPCTSTR)KAILLERA_CUSTOMIP, hDlg, (DLGPROC)CustomIPDialogProc);
}

//////////////////////////////////////////
LRESULT CALLBACK MasterSLDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MasterWGLDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowMasterSLDialog(HWND hDlg) {
	DialogBox(hx, (LPCTSTR)KAILLERA_MLIST, hDlg, (DLGPROC)MasterSLDialogProc);
}
void ShowMasterWGLDialog(HWND hDlg) {
	DialogBox(hx, (LPCTSTR)KAILLERA_MLIST, hDlg, (DLGPROC)MasterWGLDialogProc);
}

//////////////////////////////////////////

LRESULT CALLBACK KailleraServerSelectDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		{
			// Add WS_EX_APPWINDOW and remove WS_EX_TOOLWINDOW to show in Windows taskbar
			LONG_PTR exStyle = GetWindowLongPtr(hDlg, GWL_EXSTYLE);
			exStyle |= WS_EX_APPWINDOW;
			exStyle &= ~WS_EX_TOOLWINDOW;
			SetWindowLongPtr(hDlg, GWL_EXSTYLE, exStyle);

			kaillera_ssdlg = hDlg;

			SetWindowText(hDlg, N02_WINDOW_TITLE);
			
			nSettings::Initialize("SC");
			
			/*
			
			  SetDlgItemInt(hDlg, IDC_PORT, nSettings::get_int("IDC_PORT", 27886), false);
			  
				nSettings::get_str("IDC_IP", IP,"127.0.0.1:27886");
				SetDlgItemText(hDlg, IDC_IP, IP);
				
				  HWND hxx = GetDlgItem(hDlg, IDC_GAME);
				  if (gamelist != 0) {
				  nSettings::get_str("IDC_GAME", GAME, "");
				  char * xx = gamelist;
				  int p;
				  while ((p=strlen(xx))!= 0){
				  SendMessage(hxx, CB_ADDSTRING, 0, (WPARAM)xx);
				  if (strcmp(GAME, xx)==0) {
				  SetWindowText(hxx, GAME);
				  }
				  xx += p+ 1;
				  }
				  }
			*/
			
				{
					DWORD xxx = 32;
					char USERNAME[32];
					GetUserName(USERNAME, &xxx);
					char un[128];
					nSettings::get_str("USRN", un, USERNAME);
					strncpy(USERNAME, un, 31);
					USERNAME[31] = 0;
					SetWindowText(GetDlgItem(hDlg, IDC_USRNAME), USERNAME);
				}
			
				{
					// Ping spoof dropdown (0 = auto, 1-9 = target frame delay)
					HWND hFdlyCombo = GetDlgItem(hDlg, IDC_QUITMSG);
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"Auto");
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"1 frame (8ms)");
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"2 frames (24ms)");
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"3 frames (40ms)");
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"4 frames (56ms)");
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"5 frames (72ms)");
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"6 frames (88ms)");
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"7 frames (104ms)");
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"8 frames (120ms)");
					SendMessage(hFdlyCombo, CB_ADDSTRING, 0, (LPARAM)"9 frames (136ms)");
					int saved_fdly = nSettings::get_int("FDLY", 0);
					if (saved_fdly < 0 || saved_fdly > 9) {
						saved_fdly = 0;
					}
					SendMessage(hFdlyCombo, CB_SETCURSEL, saved_fdly, 0);
				}

			
			KLSListLv.handle = GetDlgItem(hDlg, LV_ULIST);
			KLSListLv.AddColumn("Name", 160);
			KLSListLv.AddColumn("IP", 150);
			KLSListLv.AddColumn("Ping", 60);
			KLSListLv.FullRowSelect();
			
				
				KLSListLoad();

				
				UpdateModeRadioButtons(hDlg);
				
			}
			break;
	case WM_CLOSE:
		{
			char tbuf[128];
			// Save ping spoof setting (combobox index)
			int fdly_index = (int)SendMessage(GetDlgItem(hDlg, IDC_QUITMSG), CB_GETCURSEL, 0, 0);
			if (fdly_index == CB_ERR) fdly_index = 0;
			nSettings::set_int("FDLY", fdly_index);

			GetWindowText(GetDlgItem(hDlg, IDC_USRNAME), tbuf, 128);
			nSettings::set_str("USRN", tbuf);

		}
		
		KLSListSave();
		
		EndDialog(hDlg, 0);
		nSettings::Terminate();
		
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case BTN_ADD:
				KLSListAdd();
				break;
			case BTN_EDIT:
				KLSListEdit();
				break;
			case BTN_DELETE:
				KLSListDelete();
				break;
			case BTN_PING:
				KLSListPING();
				break;
			case BTN_TRACE:
				KLSListTRACE();
				break;
			case BTN_CONNECT:
				KLSListConnect();
				break;
			case BTN_ABOUT:
				ShowAboutDialog(hDlg);
				break;
			case BTN_CUSTOM:
				ShowCustomIPDialog(hDlg);
				break;
			case BTN_MLIST:
				ShowMasterSLDialog(hDlg);
				break;
			case BTN_WGAMES:
				ShowMasterWGLDialog(hDlg);
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

			if(((LPNMHDR)lParam)->code==NM_DBLCLK && ((LPNMHDR)lParam)->hwndFrom==KLSListLv.handle){
				KLSListConnect();
			}
			if(((LPNMHDR)lParam)->code==NM_RCLICK && ((LPNMHDR)lParam)->hwndFrom==KLSListLv.handle){
				KLSListPING();
			}

			
		/*
		NMHDR * nParam = (NMHDR*)lParam;
		if (nParam->hwndFrom == p2p_ui_modeseltab && nParam->code == TCN_SELCHANGE){
		nTab tabb;
		tabb.handle = p2p_ui_modeseltab;
		P2PSelectionDialogProcSetMode(hDlg, tabb.SelectedTab()!=0);	
		}
		if (nParam->hwndFrom == p2p_ui_storedusers.handle){
		if (nParam->code == NM_CLICK) {
		KLSListListSelect(hDlg);
		}
		}
			*/
			break;
		};
		return 0;
}











void kaillera_GUI(){
	INITCOMMONCONTROLSEX icx;
	icx.dwSize = sizeof(icx);
	icx.dwICC = ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
	InitCommonControlsEx(&icx);

	HMODULE p2p_riched_hm = LoadLibrary("riched32.dll");

	DialogBox(hx, (LPCTSTR)KAILLERA_SSDLG, 0, (DLGPROC)KailleraServerSelectDialogProc);

	FreeLibrary(p2p_riched_hm);
}
