#pragma once

#include "richedit.h"
#include "commctrl.h"

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

class nTab{
public:
	HWND handle;

	inline void AddTab(char * name, int index){
		TCITEM tie;
		tie.mask = TCIF_TEXT;
		tie.pszText = name;
		tie.lParam = index;
		TabCtrl_InsertItem(handle, index, &tie);
	}

	inline void DeleteAllTabs(){
		TabCtrl_DeleteAllItems(handle);
	}

	inline void SelectTab(int index){
		TabCtrl_SetCurSel(handle,index);
	}
	inline int SelectedTab(){
		return TabCtrl_GetCurSel(handle);
	}
};



class nLVw {
protected:
	int cols;
	int format;
	int mask;
	int recz;

public:
    HWND handle;

	void initialize(){
		cols=0;
		recz = 0;
	}

	int AddRow (char * content){
		LVITEM lvI;
		lvI.mask        = LVIF_TEXT | LVIF_PARAM;
		lvI.iItem       = recz++;
		lvI.iSubItem    = 0;
		lvI.pszText		= content;
		lvI.lParam		= lvI.iItem;

		return ListView_InsertItem(handle, &lvI);
	}


	int AddRow(char*content, LPARAM rowno){
		LVITEM lvI;
		lvI.mask        = LVIF_TEXT | LVIF_PARAM;
		lvI.iItem       = (int)recz;
		lvI.iSubItem    = 0;
		lvI.pszText		= content;
		lvI.lParam		= rowno;

		recz++;

		ListView_InsertItem(handle, &lvI);
		return (int)(recz - 1);
	}

	int Find(LPARAM rowno){
		LVFINDINFO fi;
		fi.flags = LVFI_PARAM;
		fi.lParam = rowno;
		return ListView_FindItem(handle, -1, &fi);
	}

	LPARAM RowNo(int index){
		LVITEMA lvI;
		lvI.mask        = LVIF_PARAM;
		lvI.iItem       = index;
		lvI.iSubItem    = 0;
		SendMessage(handle, LVM_GETITEM, 0, (LPARAM)(&lvI));
		return lvI.lParam;
	}

	int RowsCount(){
		return ListView_GetItemCount(handle);
	}


	void DeleteRow(int rowno){
		recz--;
		ListView_DeleteItem(handle, rowno);
	}

	void FillRow (char * content, int coli, int rowi){
		/*
		LVITEMA lvI;
		lvI.mask        = LVIF_TEXT;
		lvI.iItem       = rowi;
		lvI.iSubItem    = coli;
		lvI.pszText		= content;
		SendMessage(handle, LVM_SETITEM, 0, (LPARAM)(&lvI));*/
		ListView_SetItemText(handle, rowi, coli, content);
	}

	void CheckRow (char * content, int llen, int coli, int rowi){
		LVITEMA lvI;
		lvI.mask        = LVIF_TEXT;
		lvI.iItem       = rowi;
		lvI.iSubItem    = coli;
		lvI.pszText		= content;
		lvI.cchTextMax	= llen;
		SendMessage(handle, LVM_GETITEM, 0, (LPARAM)(&lvI));
	}

	void AddColumn(char * name, int colw){
		LVCOLUMNA lvc;
		lvc.mask		= LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvc.iSubItem	= cols++;
		lvc.pszText		= name;
		lvc.cx			= colw;
		lvc.fmt			= LVCFMT_LEFT;

		ListView_InsertColumn(handle, (cols-1), &lvc);
	}

	void DeleteAllRows(){
		ListView_DeleteAllItems(handle);
		recz=0;
	}

	int SelectedRow(){
		return ListView_GetSelectionMark(handle);
	}
	void SelectRow(int i){
		ListView_EnsureVisible(handle, i, FALSE);
	}


	void FullRowSelect(void){
		LRESULT l = SendMessage(handle, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
        SendMessage(handle, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, l|LVS_EX_FULLROWSELECT);
	}

	void FullRowUnSelect(void){
		LRESULT l = SendMessage(handle, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
		l = (l & ~LVS_EX_FULLROWSELECT);
        SendMessage(handle, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, l);
	}

};




	inline void re_append(HWND hwnd, char * line, COLORREF color = 0){
		// Preserve the user's selection (p2pkaillera behavior). This prevents the output window
		// from "stealing" selection when the user highlights/copies text.
		CHARRANGE prev;
		SendMessage(hwnd, EM_EXGETSEL, 0, (LPARAM)&prev);

		CHARRANGE cr;
		cr.cpMin = GetWindowTextLength(hwnd);
		cr.cpMax = cr.cpMin;
		SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&cr);

		CHARFORMATA crf;
		memset(&crf, 0, sizeof(crf));
		crf.cbSize = sizeof(crf);
		// Important: don't touch CFM_EFFECTS here; auto URL detection uses CFE_LINK and we
		// don't want to wipe it.
		crf.dwMask = CFM_COLOR;
		crf.crTextColor = color;

		SendMessage(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&crf);
		SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM)line);

		// Restore previous selection and scroll to bottom.
		SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&prev);
		SendMessage(hwnd, WM_VSCROLL, SB_BOTTOM, 0);
	}

inline void re_enable_hyperlinks(HWND hwnd){
	if (hwnd == NULL)
		return;
	SendMessage(hwnd, EM_AUTOURLDETECT, TRUE, 0);
	LRESULT mask = SendMessage(hwnd, EM_GETEVENTMASK, 0, 0);
	SendMessage(hwnd, EM_SETEVENTMASK, 0, mask | ENM_LINK);
}

inline bool re_handle_link_click(LPARAM lParam){
	NMHDR* hdr = (NMHDR*)lParam;
	if (hdr == NULL)
		return false;
	if (hdr->code != EN_LINK)
		return false;

	ENLINK* enl = (ENLINK*)lParam;
	if (enl->msg != WM_LBUTTONDOWN)
		return false;

	LONG chars = enl->chrg.cpMax - enl->chrg.cpMin;
	if (chars <= 0)
		return true;

	const LONG kMaxChars = 1023;
	if (chars > kMaxChars)
		chars = kMaxChars;

	char link[kMaxChars + 1];
	link[0] = 0;

	TEXTRANGEA tr;
	tr.chrg.cpMin = enl->chrg.cpMin;
	tr.chrg.cpMax = enl->chrg.cpMin + chars;
	tr.lpstrText = link;
	SendMessageA(hdr->hwndFrom, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
	link[kMaxChars] = 0;

	const char* toOpen = link;
	char withScheme[sizeof(link) + 8];
	if (_strnicmp(link, "www.", 4) == 0) {
		memcpy(withScheme, "http://", 7);
		strncpy(withScheme + 7, link, sizeof(withScheme) - 8);
		withScheme[sizeof(withScheme) - 1] = 0;
		toOpen = withScheme;
	}

	ShellExecuteA(hdr->hwndFrom, "open", toOpen, NULL, NULL, SW_SHOWNORMAL);

	enl->msg = 0;
	return true;
}

inline void get_timestamp(char* buffer, size_t size) {
	SYSTEMTIME st;
	GetLocalTime(&st);
	int hour = st.wHour % 12;
	if (hour == 0) hour = 12;
	const char* ampm = (st.wHour >= 12) ? "PM" : "AM";
	if (size == 0)
		return;
	_snprintf_s(buffer, size, _TRUNCATE, "[%d:%02d %s] ", hour, st.wMinute, ampm);
}
