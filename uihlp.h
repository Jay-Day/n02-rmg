#pragma once

#include "richedit.h"
#include "commctrl.h"

#include <windows.h>
#include <stdio.h>
#include <limits.h>

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
		//kprintf(line);
		size_t len = strlen(line);
		LONG addLen = (len > (size_t)LONG_MAX) ? LONG_MAX : (LONG)len;
		CHARRANGE cr;
		GETTEXTLENGTHEX gtx;
		gtx.codepage = CP_ACP;
		gtx.flags = GTL_PRECISE;
		cr.cpMin = GetWindowTextLength(hwnd);//SendMessage(p2p_ui_con_richedit, EM_GETTEXTLENGTHEX, (WPARAM)&gtx, 0);
		cr.cpMax = cr.cpMin + addLen;
		SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&cr);
		CHARFORMATA crf;
		memset(&crf, 0, sizeof(crf));
		crf.cbSize = sizeof(crf);
		crf.dwMask = CFM_COLOR | CFM_EFFECTS;
		crf.dwEffects = 0;  // Clear CFE_AUTOCOLOR
		crf.crTextColor = color;
		SendMessage(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&crf);
		SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM)line);
		//SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&cr);
		//SendMessage(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&crf);

		SendMessage(hwnd, WM_VSCROLL, SB_BOTTOM, 0);
	}

inline void get_timestamp(char* buffer, size_t size) {
	SYSTEMTIME st;
	GetLocalTime(&st);
	int hour = st.wHour % 12;
	if (hour == 0) hour = 12;
	const char* ampm = (st.wHour >= 12) ? "PM" : "AM";
	sprintf(buffer, "[%d:%02d %s] ", hour, st.wMinute, ampm);
}
