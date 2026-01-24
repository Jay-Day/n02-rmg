#include "kailleraclient.h"

#include "common/nSettings.h"
#include "p2p_ui.h"
#include <windows.h>
#include <limits.h>
#include <stdarg.h>
#include "string.h"
#include "common/nSTL.h"


#include "uihlp.h"
#include "stats.h"


extern HINSTANCE hx;

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
int p2p_option_forcePort;
int p2p_frame_delay_override;
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
	p2p_ssrv_send("WHATISMYIP");
}

void p2p_ssrv_packet_recv_callback(char *cmd, int len, void *sadr){
	if (strcmp(cmd, "PINGRQ")==0){
		p2p_send_ssrv_packet("xxxxxxxxxx",11, sadr);
		return;
	} else if (strncmp(cmd, "MSG", 3)==0){
		return;
	}


	p2p_core_debug("SSRV: %s", cmd);
}

void p2p_ssrv_enlistgame(){
	char buf[500];
	wsprintf(buf, "ENLIST %s|%s|%s", GAME, APP, USERNAME);
	p2p_ssrv_send(buf);
}

void p2p_ssrv_enlistgamef() {
	char buf[500];
	wsprintf(buf, "ENLIST %s|%s|%s|%i", GAME, APP, USERNAME, PORT);
	p2p_ssrv_send(buf);
}

void p2p_ssrv_unenlistgame(){
	p2p_ssrv_send("UNENLIST");
}

void p2p_enlist_game() {
	if (p2p_option_forcePort) {
		p2p_ssrv_enlistgamef();
	}
	else {
		p2p_ssrv_enlistgame();
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
		p2p_ssrv_enlistgame();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
void IniaialzeConnectionDialog(HWND hDlg){
	p2p_ui_connection_dlg = hDlg;
	if (p2p_core_initialize(HOST, PORT, APP, GAME, USERNAME)){
		//ShowWindow(GetDlgItem(hDlg, IDC_ADDDELAY), HOST? SW_SHOW:SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, CHK_ENLIST), HOST ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, CHK_ENLISTF), HOST ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDC_SSERV_WHATSMYIP), HOST ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDC_HOSTT), HOST ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDC_P2P_FDLY_LBL), HOST ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDC_P2P_FDLY), HOST ? SW_SHOW : SW_HIDE);
		
		if (HOST) {
			outpf("Hosting %s on port %i", GAME, p2p_core_get_port());
			outpf("WARNING: Hosting requires hosting ports to be forwarded and enabled in firewalls.");
			SetDlgItemText(hDlg, IDC_GAME, GAME);
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

			// Load frame delay override (0 = auto-calculated)
			p2p_frame_delay_override = nSettings::get_int("P2P_FDLY", 0);
			if (p2p_frame_delay_override == 0) {
				SetWindowText(GetDlgItem(hDlg, IDC_P2P_FDLY), "");
			} else {
				char fdly_str[16];
				sprintf(fdly_str, "%d", p2p_frame_delay_override);
				SetWindowText(GetDlgItem(hDlg, IDC_P2P_FDLY), fdly_str);
			}
			SendMessage(GetDlgItem(hDlg, IDC_P2P_FDLY), EM_LIMITTEXT, 2, 0);

			p2p_ui_con_richedit = GetDlgItem(hDlg, IDC_RICHEDIT2);
			p2p_ui_con_chatinp = GetDlgItem(hDlg, IDC_CHATI);
			IniaialzeConnectionDialog(hDlg);
			SendMessage(p2p_ui_con_richedit, EM_AUTOURLDETECT, TRUE, FALSE);
			p2p_cdlg_timer = SetTimer(hDlg, 0, 1000, 0);
		}
		
		break;
	case WM_CLOSE:
		if (p2p_disconnect()){
			// Save frame delay override
			{
				char fdly_buf[16];
				GetWindowText(GetDlgItem(hDlg, IDC_P2P_FDLY), fdly_buf, 16);
				p2p_frame_delay_override = atoi(fdly_buf);
				nSettings::set_int("P2P_FDLY", p2p_frame_delay_override);
			}

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
						p2p_ssrv_enlistgame();
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
	case WM_COMMAND:
		////kprintf(__FILE__ ":%i", __LINE__);//localhost:27888
		switch (LOWORD(wParam)) {
		case IDC_P2P_FDLY:
			if (HIWORD(wParam) == EN_CHANGE) {
				char fdly_buf[16];
				GetWindowText(GetDlgItem(hDlg, IDC_P2P_FDLY), fdly_buf, 16);
				p2p_frame_delay_override = atoi(fdly_buf);
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
		case CHK_ENLISTF:
			p2p_option_forcePort = SendMessage(GetDlgItem(hDlg, CHK_ENLISTF), BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
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

void InitializeP2PSubsystem(HWND hDlg, bool host){
	ShowWindow(p2p_ui_ss_dlg, SW_HIDE);

	HOST = host;

	if (!host) {
		PORT = 0;
	} else {
		PORT = GetDlgItemInt(p2p_ui_ss_dlg, IDC_PORT, 0, FALSE);
	}

	GetWindowText(GetDlgItem(p2p_ui_ss_dlg, IDC_GAME), GAME, 127);
	GetWindowText(GetDlgItem(p2p_ui_ss_dlg, IDC_IP), IP, 127);
	GetWindowText(GetDlgItem(p2p_ui_ss_dlg, IDC_USRNAME), USERNAME, 31);


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

			SetWindowText(hDlg, "n02.p2p " P2P_VERSION);
			
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
				InitializeP2PSubsystem(hDlg, false);
				break;
			case IDC_HOST:
				InitializeP2PSubsystem(hDlg, true);
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
