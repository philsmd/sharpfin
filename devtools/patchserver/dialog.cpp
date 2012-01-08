/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 *  
 * This Library is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this source files. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include "launcher.h"
#include <stdio.h>

static LRESULT CALLBACK inputDlgProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
static char buffer[MAX] ;
static char *mytitle, *myprompt ;

static LRESULT CALLBACK selectDlgProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
char *mydnsaddress ;
char **mylist ;
int myentries ;
int myselection ;

LRESULT CALLBACK inputDlgProc(HWND hWndDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch(Msg) {
	case WM_INITDIALOG:
		SetDlgItemText(hWndDlg, IDC_PROMPT, myprompt) ;
		return TRUE;
	case WM_COMMAND:
		switch(wParam) {
		case IDOK:
			GetDlgItemText(hWndDlg, IDC_TEXT, buffer, MAX-1) ;
			EndDialog(hWndDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}
 
char *inputstring(HINSTANCE parent, char *title, char *prompt) {
	HWND hWnd;
	int i ;
	char buf[MAX] ;
	
	mytitle=title ;
	myprompt=prompt ;
	
	buffer[0]='\0' ;
	
	i=DialogBox(parent, MAKEINTRESOURCE(IDD_DLGINPUT),
	          0, reinterpret_cast<DLGPROC>(inputDlgProc));
		
	return buffer ;

}

char *getfilename(char *ext, char *title) {
	OPENFILENAME opf;
	static char filename[MAX];
	memset((void *)&opf, 0, sizeof(opf)) ;
	
	opf.lStructSize=sizeof(opf) ;
	opf.hwndOwner=0 ;
	opf.lpstrFilter=ext ;
	opf.lpstrFile=filename ; 
	opf.nMaxFile=MAX-1 ;
	opf.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY ;
	opf.lpstrDefExt="patch" ;

	if(GetOpenFileName(&opf)) {
		return filename ;
	} else {
		return NULL ;
	}
}

void onItemSelected(HWND hWndDlg) {
	// Get selection index
	myselection=SendDlgItemMessage(hWndDlg, IDC_LIST, LB_GETCURSEL, 0, 0) ;
	if (myselection==LB_ERR) {
		myselection=-1 ;
	} else {
		// Get the index of the URL
		myselection=SendDlgItemMessage(hWndDlg, IDC_LIST, LB_GETITEMDATA, (WPARAM)myselection, 0) ;
	}
	// Get DNS
	GetDlgItemText(hWndDlg, IDC_DNS, mydnsaddress, MAX-1) ;
	EndDialog(hWndDlg, 0);
}

LRESULT CALLBACK selectDlgProc(HWND hWndDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
	int i, p ;
	
	switch(Msg) {
	case WM_INITDIALOG:
		// mylist contains URL, description, URL, description and we only want to print the descriptions
		for (i=1; i<myentries; i+=2) {
			// Add Entry
			p=SendDlgItemMessage(hWndDlg, IDC_LIST, LB_ADDSTRING, (WPARAM)i, (LPARAM)mylist[i]);
			// Attach index of URL in array to entry (allows listbox to be sorted)
			SendDlgItemMessage(hWndDlg, IDC_LIST, LB_SETITEMDATA, (WPARAM)p, (LPARAM)i-1); 

		}
		
		// Set the DNS field
		SetDlgItemText(hWndDlg, IDC_DNS, mydnsaddress) ;
		return TRUE;
	

	case WM_COMMAND:
		if ((LOWORD(wParam)==IDC_LIST)&&(HIWORD(wParam)==LBN_DBLCLK)) {
			onItemSelected(hWndDlg);
			return TRUE;
		}
		switch(wParam) {
		case IDCANCEL:
			myselection=-2 ;
			EndDialog(hWndDlg, 0) ;
			return TRUE ;
		case IDOK:
			onItemSelected(hWndDlg);
			return TRUE;
		}
		break;
	}
	return FALSE;
}
 
int selectpatch(HINSTANCE parent, char *dnsaddress, int entries, char **list) {
	HWND hWnd;
	int i ;
	mydnsaddress=dnsaddress ;
	myentries=entries ;
	mylist=list ;
	myselection=-1 ;
	i=DialogBox(parent,MAKEINTRESOURCE(IDD_DLGSELECT),0,reinterpret_cast<DLGPROC>(selectDlgProc));
	return myselection;
}
