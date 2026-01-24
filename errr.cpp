/******************************************************************************
***********   .d8888b.   .d8888b    *******************************************
**********   d88P  Y88b d88P  Y88b   ******************************************
*            888    888        888   ******************************************
*    8888b.  888    888      .d88P   ******************************************
*   888 "88b 888    888  .od888P"  ********************************************
*   888  888 888    888 d88P"    **********************************************
*   888  888 Y88b  d88P 888"        *******************************************
*   888  888  "Y8888P"  888888888            Open Kaillera: http://okai.sf.net
******************************************************************************/
#include "errr.h"
#ifndef KAILLERA
#define KAILLERA
#endif
#include "uihlp.h"
#include "resource.h"


#include <cstdio>
#include <limits.h>

//#include "common/nprintf.h"
// our most sexing debug/logging function
void __cdecl kprintf(char * arg_0, ...) {
	//print on console
#if 1
	char V88[2048];
	va_list args;
	va_start(args, arg_0);
	V88[0] = 0;
	if (arg_0 != NULL)
		vsnprintf_s(V88, sizeof(V88), _TRUNCATE, arg_0, args);
	va_end(args);

	//log to file
#if 1
	static HFILE hx = HFILE_ERROR;

		if (hx == HFILE_ERROR) {
			OFSTRUCT of;
			hx = OpenFile("keaa.txt", &of, OF_CREATE|OF_WRITE);
		}
		size_t len = strlen(V88);
		UINT writeLen = (len > (size_t)UINT_MAX) ? UINT_MAX : (UINT)len;
		_lwrite(hx, V88, writeLen);
	#endif
	#endif
	}

typedef struct {
	char * file;
	int line;
} n02_TRACE_st;

#define n02_TRACE_stack_len 16
#define n02_TRACE_stack_mask 15

n02_TRACE_st n02_TRACE_stack[n02_TRACE_stack_len];
unsigned int n02_TRACE_stack_pos = 0;

void _n02_TRACE(char * file, int line){
	n02_TRACE_stack[n02_TRACE_stack_pos&n02_TRACE_stack_mask].file = file;
	n02_TRACE_stack[n02_TRACE_stack_pos&n02_TRACE_stack_mask].line = line;
	n02_TRACE_stack_pos++;
}


//void DumpTrace(){
//	char EBX[2000];
//	kprintf("Dumping trace");
//	for (unsigned int x = 1; x <= n02_TRACE_stack_len && x <=n02_TRACE_stack_pos ; x++ ) {
//		wsprintf(EBX, "%u: %s[%i]\r\n", (n02_TRACE_stack_pos-x), n02_TRACE_stack[(n02_TRACE_stack_pos-x)&n02_TRACE_stack_mask].file, n02_TRACE_stack[(n02_TRACE_stack_pos-x)&n02_TRACE_stack_mask].line);
//		kprintf(EBX);
//	}
//}

extern HINSTANCE hx;

#define ExceptionCodeToStr_h(X) if (code==X) return #X;

char ExceptionCodeToStr_t[16];

char * ExceptionCodeToStr(int code) {
	ExceptionCodeToStr_h(EXCEPTION_ACCESS_VIOLATION);
	ExceptionCodeToStr_h(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
	ExceptionCodeToStr_h(EXCEPTION_BREAKPOINT);
	ExceptionCodeToStr_h(EXCEPTION_DATATYPE_MISALIGNMENT);
	ExceptionCodeToStr_h(EXCEPTION_FLT_DENORMAL_OPERAND);
	ExceptionCodeToStr_h(EXCEPTION_FLT_DIVIDE_BY_ZERO);
	ExceptionCodeToStr_h(EXCEPTION_FLT_INEXACT_RESULT);
	ExceptionCodeToStr_h(EXCEPTION_FLT_INVALID_OPERATION);
	ExceptionCodeToStr_h(EXCEPTION_FLT_OVERFLOW);
	ExceptionCodeToStr_h(EXCEPTION_FLT_STACK_CHECK);
	ExceptionCodeToStr_h(EXCEPTION_FLT_UNDERFLOW);
	ExceptionCodeToStr_h(EXCEPTION_ILLEGAL_INSTRUCTION);
	ExceptionCodeToStr_h(EXCEPTION_IN_PAGE_ERROR);
	ExceptionCodeToStr_h(EXCEPTION_INT_DIVIDE_BY_ZERO);
	ExceptionCodeToStr_h(EXCEPTION_INT_OVERFLOW);
	ExceptionCodeToStr_h(EXCEPTION_INVALID_DISPOSITION);
	ExceptionCodeToStr_h(EXCEPTION_NONCONTINUABLE_EXCEPTION);
	ExceptionCodeToStr_h(EXCEPTION_PRIV_INSTRUCTION);
	ExceptionCodeToStr_h(EXCEPTION_SINGLE_STEP);
	ExceptionCodeToStr_h(EXCEPTION_STACK_OVERFLOW);
	wsprintf(ExceptionCodeToStr_t, "%i", code);
	return ExceptionCodeToStr_t;
}

LRESULT CALLBACK ErrorReporterDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_INITDIALOG:
			if (lParam != 0) {
				_EXCEPTION_POINTERS *EI = (_EXCEPTION_POINTERS*)lParam;
				char EBX[2000];
				wsprintf(EBX, "EXCEPTION_RECORD {\r\n"
								"\tExceptionCode=%s;\r\n"
								"\tExceptionFlags=%i;\r\n"
								"\tExceptionAddress=0x%08x;\r\n"
								"\tExceptionInformation=%i;\r\n"
								"};",
								ExceptionCodeToStr(EI->ExceptionRecord->ExceptionCode),
								EI->ExceptionRecord->ExceptionFlags,
								EI->ExceptionRecord->ExceptionAddress,
								(EI->ExceptionRecord->ExceptionInformation!=0 && EI->ExceptionRecord->NumberParameters>0)? EI->ExceptionRecord->ExceptionInformation[0]:-1
								);

				HWND ct = GetDlgItem(hDlg, TXT_CHAT);

				re_append(ct, EBX, 0x000000FF);
				kprintf(EBX);
//#ifdef TRACE
				re_append(ct, "TRACE STACK: \r\n", 0x0000FF00);
				kprintf("TRACE STACK: \r\n");
				for (unsigned int x = 1; x <= n02_TRACE_stack_len && x <=n02_TRACE_stack_pos ; x++ ) {
					wsprintf(EBX, "%u: %s[%i]\r\n", (n02_TRACE_stack_pos-x), n02_TRACE_stack[(n02_TRACE_stack_pos-x)&n02_TRACE_stack_mask].file, n02_TRACE_stack[(n02_TRACE_stack_pos-x)&n02_TRACE_stack_mask].line);
					kprintf(EBX);
					re_append(ct, EBX, 0x00FF0000);
				}
//#endif
				//flash_window(hDlg);

				SendMessage(ct, WM_VSCROLL, SB_TOP, 0);

			}
			break;
		case WM_CLOSE:
			EndDialog(hDlg, 0);
			break;
		case WM_COMMAND:
			if (LOWORD(wParam)==BTN_REPORT){

			} else if (LOWORD(wParam)==BTN_CLOSE) {
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			} else if (LOWORD(wParam)==BTN_LEAVE){
				ExitProcess(1);
			}
			break;
	};
	return 0;
}

int n02ExceptionFilterFunction(_EXCEPTION_POINTERS *ExceptionInfo) {
	DialogBoxParam(hx, (LPCTSTR)N02_ERRORDLG, 0, (DLGPROC)ErrorReporterDialogProc, (LPARAM)ExceptionInfo);
	return EXCEPTION_EXECUTE_HANDLER;
}
