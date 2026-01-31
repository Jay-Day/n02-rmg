#include "stats.h"
#include "resource.h"
#include "errr.h"
#include "uihlp.h"
#include "common/nThread.h"
#include <stdarg.h>
#include <string.h>



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

static CRITICAL_SECTION g_stats_lock;
static bool g_stats_lock_init = false;
static char g_stats_extra[4096];
static size_t g_stats_extra_len = 0;

static void StatsEnsureLock() {
	if (!g_stats_lock_init) {
		InitializeCriticalSection(&g_stats_lock);
		g_stats_lock_init = true;
		g_stats_extra[0] = 0;
		g_stats_extra_len = 0;
	}
}

void StatsAppendLine(const char* fmt, ...) {
	if (fmt == NULL || *fmt == 0)
		return;

	StatsEnsureLock();

	char line[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf_s(line, sizeof(line), _TRUNCATE, fmt, args);
	va_end(args);

	const size_t line_len = strlen(line);
	if (line_len == 0)
		return;

	EnterCriticalSection(&g_stats_lock);
	{
		const size_t needed = line_len + 2; // + "\r\n"
		if (needed >= sizeof(g_stats_extra)) {
			size_t copy_len = sizeof(g_stats_extra) - 3;
			memcpy(g_stats_extra, line + (line_len - copy_len), copy_len);
			g_stats_extra[copy_len] = '\r';
			g_stats_extra[copy_len + 1] = '\n';
			g_stats_extra[copy_len + 2] = 0;
			g_stats_extra_len = copy_len + 2;
		} else {
			if (g_stats_extra_len + needed >= sizeof(g_stats_extra)) {
				size_t overflow = (g_stats_extra_len + needed) - (sizeof(g_stats_extra) - 1);
				if (overflow >= g_stats_extra_len) {
					g_stats_extra_len = 0;
				} else {
					memmove(g_stats_extra, g_stats_extra + overflow, g_stats_extra_len - overflow);
					g_stats_extra_len -= overflow;
				}
			}
			memcpy(g_stats_extra + g_stats_extra_len, line, line_len);
			g_stats_extra_len += line_len;
			g_stats_extra[g_stats_extra_len++] = '\r';
			g_stats_extra[g_stats_extra_len++] = '\n';
			g_stats_extra[g_stats_extra_len] = 0;
		}
	}
	LeaveCriticalSection(&g_stats_lock);
}

void StatsClearExtra() {
	StatsEnsureLock();
	EnterCriticalSection(&g_stats_lock);
	g_stats_extra_len = 0;
	g_stats_extra[0] = 0;
	LeaveCriticalSection(&g_stats_lock);
}

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
			char SOUTP [4000];
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
			b += strlen(b);
			StatsEnsureLock();
			EnterCriticalSection(&g_stats_lock);
			if (g_stats_extra_len > 0) {
				_snprintf_s(b, sizeof(SOUTP) - (b - SOUTP), _TRUNCATE, "\n\np2p\n--------------------\n%s", g_stats_extra);
			}
			LeaveCriticalSection(&g_stats_lock);
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
				StatsClearExtra();
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
