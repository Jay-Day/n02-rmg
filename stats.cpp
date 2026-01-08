#include "stats.h"
#include "resource.h"
#include "errr.h"
#include "uihlp.h"
#include "common/nThread.h"



int PACKETLOSSCOUNT;
int PACKETMISOTDERCOUNT;
int PACKETINCDSCCOUNT;
int PACKETIADSCCOUNT;

int SOCK_RECV_PACKETS;
int SOCK_RECV_BYTES;
int SOCK_RECV_RETR;
int SOCK_SEND_PACKETS;
int SOCK_SEND_BYTES;
int SOCK_SEND_RETR;

int SOCK_SEND_PPS;
int GAME_FPS;


extern HINSTANCE hx;

LRESULT CALLBACK n02StatsDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);


bool StatsThread_running = false;
class StatsThread: public nThread {
public:
	
	HWND n02_stats_dlg;

	void run(){
		StatsThread_running = true;
		__try {
			//=====================================================================
			DialogBox(hx, (LPCTSTR)N02_STATSDLG, 0, (DLGPROC)n02StatsDialogProc);
			//=====================================================================
		} __except(n02ExceptionFilterFunction(GetExceptionInformation())) {
			MessageBox(0, "StatsThread Exception", 0, 0);
			StatsThread_running = false;
			return;
		}
		StatsThread_running = false;
	}
	void start(){
		if (!StatsThread_running)
			create();
		else 
			eend();
	}
	void eend(){
		if (StatsThread_running){
			EndDialog(n02_stats_dlg, 0);
			sleep(1);
			if (StatsThread_running)
				destroy();
		}
		StatsThread_running = false;
	}
} stats_thread;

#define POH 28 

LRESULT CALLBACK n02StatsDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static UINT_PTR timer = 0;
	static HWND redit = 0;

	static int SOCK_RECV_PACKETS_LAST;
	static int SOCK_RECV_BYTES_LAST;
	static int SOCK_SEND_PACKETS_LAST;
	static int SOCK_SEND_BYTES_LAST;

	switch (uMsg) {
	case WM_INITDIALOG:
		{
			stats_thread.n02_stats_dlg = hDlg;
			redit = GetDlgItem(hDlg, TXT_CHAT);
			timer = SetTimer(hDlg, 0, 1000, 0);
			return 0;
		}
	case WM_TIMER:
		{
			char SOUTP [2000];
			char * b = SOUTP;
			int dpackets = SOCK_RECV_PACKETS-SOCK_RECV_PACKETS_LAST;
			int dbytes = SOCK_RECV_BYTES-SOCK_RECV_BYTES_LAST;
			SOCK_RECV_BYTES_LAST = SOCK_RECV_BYTES;
			SOCK_RECV_PACKETS_LAST = SOCK_RECV_PACKETS;
			wsprintf(b, "in\n--------------------\nrate: %i/s (%i B/s)\ntotal: %i (%i Bytes)\n\n", dpackets, (dpackets * POH) + dbytes, SOCK_RECV_PACKETS, SOCK_RECV_BYTES + (SOCK_RECV_PACKETS * POH));

			b += strlen(b);
			dpackets = SOCK_SEND_PACKETS-SOCK_SEND_PACKETS_LAST;
			dbytes = SOCK_SEND_BYTES-SOCK_SEND_BYTES_LAST;
			SOCK_SEND_BYTES_LAST = SOCK_SEND_BYTES;
			SOCK_SEND_PACKETS_LAST = SOCK_SEND_PACKETS;
			wsprintf(b, "out\n--------------------\nrate: %i/s (%i B/s)\ntotal: %i (%i Bytes)\n\n", dpackets, (dpackets * POH) + dbytes, SOCK_SEND_PACKETS, SOCK_SEND_BYTES + (SOCK_SEND_PACKETS * POH));

			b += strlen(b);

			wsprintf(b, "others\n--------------------\nloss: %i\nretransmits:%i/%i\nmisorder:%i\naddrdism:%i\ncoundism:%i", 
				PACKETLOSSCOUNT,
				SOCK_SEND_RETR, SOCK_RECV_RETR,
				PACKETMISOTDERCOUNT,
				PACKETIADSCCOUNT,
				PACKETINCDSCCOUNT);
			SetWindowText(redit, SOUTP);
		}
		break;
	case WM_CLOSE:
		KillTimer(hDlg, timer);
		EndDialog(hDlg, 0);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case BTN_RESET:
			PACKETLOSSCOUNT = 0;
			PACKETMISOTDERCOUNT = 0;
			PACKETINCDSCCOUNT = 0;
			PACKETIADSCCOUNT = 0;
			
			SOCK_RECV_PACKETS = 0;
			SOCK_RECV_BYTES = 0;
			SOCK_RECV_RETR = 0;
			SOCK_SEND_PACKETS = 0;
			SOCK_SEND_BYTES = 0;
			SOCK_SEND_RETR = 0;
			break;
		case BTN_CLOSE:
			SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;
		};
		break;
	};
	return 0;
}

void StatsDisplayThreadBegin(){
	stats_thread.start();
}

void StatsDisplayThreadEnd(){
	stats_thread.eend();
}