/*
 * windlg.c
 *
 * Microwindows Dialog function
 *
 * Copyright (C) 2003 - Gabriele Brugnoni
 *
 * gabrielebrugnoni@dveprojects.com
 * DVE Prog. El. - Varese, Italy
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define MWINCLUDECOLORS
#include "windows.h"		/* windef.h, winuser.h */
#include "device.h"

#define DEFAULT_FONT			DEFAULT_GUI_FONT
#define DEFDLG_FONT_QUALITY		ANTIALIASED_QUALITY

#define DWL_DLGDATA	12

#define DLG_DEF_STYLE	(0)

#define ISDLGCONTROL(hDlg, hCtrl)	(((hCtrl) != NULL) && ((hDlg) != (hCtrl)) && \
					 (MwGetTopWindow((hCtrl)) == (hDlg)))

#define MulDiv(x,m,d)	( ((((x)<0) && ((m)<0)) || (((x)>=0) && ((m)>=0))) ? \
			  ((((long)(x)*(long)(m))+(long)((d)/2))/(long)(d)) : \
			  ((((long)(x)*(long)(m))-(long)((d)/2))/(long)(d)) )
#define MulDivRD(x,m,d)	( ((long)(x) * (long)(m))/(long)(d) )

/*
 *  Struct with information about DLG
 */
typedef struct tagMWDLGDATA {
	HFONT hFnt;
	DWORD flags;
	BOOL running;
	HWND hWndFocus;
	int nResult;
} MWDLGDATA, *PMWDLGDATA;

#define DLGF_DESTROYFONT	0x80000000L

#define DLG_PMWDLGDATA(dlg)	((PMWDLGDATA)GetWindowLong(dlg, DWL_DLGDATA))
#define DLG_DLGPROC(dlg)	((DLGPROC)GetWindowLong(dlg, DWL_DLGPROC))
#define DLG_MSGRESULT(dlg)	((LRESULT)GetWindowLong(dlg, DWL_MSGRESULT))

static LRESULT CALLBACK mwDialogProc(HWND hWnd, UINT Msg, WPARAM wParam,
				     LPARAM lParam);

/*
 *  Used to find the top-control in dialogs.
 *  (focus in a COMBOBOX is on a CBB's child)
 */
static HWND
dlgGetCtrlTop(HWND hDlg, HWND hChild)
{
	HWND hwnd = hChild;
	while ((hwnd != NULL)) {
		if (hwnd->parent == hDlg)
			return hwnd;
		hwnd = hwnd->parent;
	}
	return hChild;
}

#define dlgGetCtrlFocus(hDlg)	dlgGetCtrlTop(hDlg, GetFocus())

/*
 * Initialize module
 */
BOOL WINAPI
MwInitializeDialogs(HINSTANCE hInstance)
{
#ifdef  WNDCLASSEX
	WNDCLASSEX wcl;
#else
	WNDCLASS wcl;
#endif

	MwRegisterStaticControl(hInstance);
	MwRegisterButtonControl(hInstance);
	MwRegisterEditControl(hInstance);
	MwRegisterListboxControl(hInstance);
	MwRegisterProgressBarControl(hInstance);
	MwRegisterComboboxControl(hInstance);

	memset(&wcl, 0, sizeof(wcl));
#ifdef  WNDCLASSEX
	wcl.cbSize = sizeof(wcl);
#endif
	wcl.style = CS_BYTEALIGNCLIENT;
	wcl.cbWndExtra = DWL_DLGDATA + 4;
	wcl.lpfnWndProc = (WNDPROC) mwDialogProc;
	wcl.hInstance = hInstance;
	wcl.lpszClassName = "GDLGCLASS";
	wcl.hbrBackground = GetStockObject(LTGRAY_BRUSH);

#ifdef  WNDCLASSEX
	return (RegisterClassEx(&wcl) != 0);
#else
	return (RegisterClass(&wcl) != 0);
#endif
}


LRESULT WINAPI
SendDlgItemMessage(HWND hwnd, int id, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HWND hctl = GetDlgItem(hwnd, id);
	if (hctl == NULL)
		return NULL;
	return SendMessage(hctl, Msg, wParam, lParam);
}


UINT WINAPI
GetDlgItemText(HWND hwnd, int id, LPTSTR pStr, int nSize)
{
	return GetWindowText(GetDlgItem(hwnd, id), pStr, nSize);
}


BOOL WINAPI
SetDlgItemText(HWND hwnd, int id, LPTSTR pStr)
{
	return SetWindowText(GetDlgItem(hwnd, id), pStr);
}


BOOL WINAPI
SetDlgItemInt(HWND hwnd, int id, UINT val, BOOL bSigned)
{
	char s[64];
	if (bSigned)
		sprintf(s, "%d", val);
	else
		sprintf(s, "%u", val);

	return SetWindowText(GetDlgItem(hwnd, id), s);
}


UINT WINAPI
GetDlgItemInt(HWND hwnd, int id, BOOL * pbTransl, BOOL bSigned)
{
	int x, n;
	UINT ux;
	char s[64];

	GetWindowText(GetDlgItem(hwnd, id), s, sizeof(s));
	if (bSigned)
		n = sscanf(s, "%d", &x);
	else
		n = sscanf(s, "%u", &ux);

	if (pbTransl != NULL)
		*pbTransl = (n == 1);
	if (n != 1)
		return 0;
	return (bSigned) ? (UINT) x : ux;
}


UINT
IsDlgButtonChecked(HWND hDlg, int id)
{
	return SendMessage(GetDlgItem(hDlg, id), BM_GETCHECK, 0, 0);
}

BOOL WINAPI
CheckDlgButton(HWND hDlg, int id, UINT mode)
{
	return SendMessage(GetDlgItem(hDlg, id), BM_SETCHECK, mode, 0);
}

BOOL WINAPI
CheckRadioButton(HWND hDlg, int idFirst, int idLast, int idCheck)
{
	int id;
	HWND hCheck;
	HWND obj;
	UINT dc;

	hCheck = GetDlgItem(hDlg, idCheck);
	if (hCheck == NULL)
		return FALSE;

	/*  First, remove all from previuos in group.  */
	obj = GetNextDlgGroupItem(hDlg, hCheck, TRUE);
	while (obj) {
		if ((idFirst == -1) || (obj->id >= idFirst) && (obj->id <= idLast)
		    && (((dc =
		       SendMessage(obj, WM_GETDLGCODE, 0, 0)) & DLGC_RADIOBUTTON) != 0))
			SendMessage(obj, BM_SETCHECK, BST_UNCHECKED, 0);

		obj = GetNextDlgGroupItem(hDlg, obj, TRUE);
	}

	/*  Remove on next */
	obj = GetNextDlgGroupItem(hDlg, hCheck, FALSE);
	while (obj) {
		if ((idFirst == -1) || (obj->id >= idFirst) && (obj->id <= idLast)
		    && (((dc =
		       SendMessage(obj, WM_GETDLGCODE, 0, 0)) & DLGC_RADIOBUTTON) != 0))
			SendMessage(obj, BM_SETCHECK, BST_UNCHECKED, 0);

		obj = GetNextDlgGroupItem(hDlg, obj, FALSE);
	}

	SendMessage(hCheck, BM_SETCHECK, BST_CHECKED, 0);
	return TRUE;
}


/*
 *  Return previous or next control in a dialog box group controls
 */
HWND WINAPI
GetNextDlgGroupItem(HWND hDlg, HWND hCtl, BOOL bPrevious)
{
	HWND obj, nobj;

	/*  Note that child list starts from last to first */
	if (!bPrevious) {
		nobj = hCtl;
		for (;;) {
			obj = hDlg->children;
			while (obj) {
				if (obj->siblings == nobj)
					break;
				obj = obj->siblings;
			}
			if (obj == NULL)
				break;

			if ((obj->style & WS_GROUP))
				return NULL;
			if (IsWindowEnabled(obj) && IsWindowVisible(obj) &&
			    !(SendMessage(obj, WM_GETDLGCODE, 0, 0) & DLGC_STATIC))
				return obj;

			nobj = obj;
		}
	} else {
		/* the siblings ptr is the previuos control in order */
		if ((hCtl->style & WS_GROUP) != 0)
			return NULL;

		obj = hCtl->siblings;
		while (obj) {
			if (IsWindowEnabled(obj) && IsWindowVisible(obj) &&
			    !(SendMessage(obj, WM_GETDLGCODE, 0, 0) & DLGC_STATIC))
				return obj;
			else if ((obj->style & WS_GROUP))
				return NULL;

			obj = obj->siblings;
		}
	}

	return NULL;
}

/*
 *  Search the first button in the childrens list.
 */
static HWND
firstDefButton(HWND hdlg)
{
	HWND hwnd = hdlg->children;

	while (hwnd) {
		if (hwnd->pClass
		    && !strcasecmp(hwnd->pClass->szClassName, "BUTTON")
		    && (hwnd->style & BS_DEFPUSHBUTTON))
			return hwnd;

		hwnd = hwnd->siblings;
	}

	return NULL;
}

/*
 *  Find the next children in the dialogbox.
 */
static HWND
nextTabStop(HWND hDlg, HWND hChild, BOOL bPrevious)
{
	HWND *pControls;
	HWND obj;
	UINT dlgCode;
	int n, i;

	pControls = (HWND *) malloc(256 * sizeof(HWND));
	if (pControls == NULL)
		return NULL;

	bPrevious = !bPrevious;	/* next ptr is previous in order list */

	n = 0;
	i = -1;
	obj = hDlg->children;
	while (obj && (n < 256)) {
		if ((obj == hChild))
			i = n;
		if ((obj->style & WS_TABSTOP) && IsWindowEnabled(obj)
		    && IsWindowVisible(obj))
			pControls[n++] = obj;
		obj = obj->siblings;
	}
	if (n > 0) {
		if (bPrevious) {
			if ((i == -1) || (i - 1 < 0))
				obj = pControls[n - 1];
			else
				obj = pControls[i - 1];
		} else {
			if ((i == -1) || (i + 1 >= n))
				obj = pControls[0];
			else
				obj = pControls[i + 1];
		}
	}

	free(pControls);

	dlgCode = SendMessage(obj, WM_GETDLGCODE, 0, 0);

	/* If it's a RB, find the one checked. */
	if ((dlgCode & DLGC_RADIOBUTTON)) {
		/* Go back until found object */
		HWND found = NULL;
		HWND sobj = hDlg->children;

		while (sobj && sobj != obj) {
			if (sobj->style & WS_GROUP)
				found = NULL;
			dlgCode = SendMessage(sobj, WM_GETDLGCODE, 0, 0);
			if ((dlgCode & DLGC_RADIOBUTTON)
			    && IsDlgButtonChecked(hDlg, sobj->id) == BST_CHECKED)
				found = sobj;
			sobj = sobj->siblings;
		}
		if (found)
			obj = found;
	}
	return obj;
}


static BOOL
parseAccelerator(HWND hWnd, int key)
{
	HWND obj;

	obj = hWnd->children;
	while (obj) {
		int dlgCode = SendMessage(obj, WM_GETDLGCODE, 0, 0);
		if (IsWindowVisible(obj)) {
			if ((dlgCode & DLGC_STATIC) ||
			    ((dlgCode & (DLGC_BUTTON | DLGC_RADIOBUTTON)) != 0
			     && IsWindowEnabled(obj))) {
				LPCTSTR pCaption = obj->szTitle;
				/*FIXME: strchr don't works with WCHAR */
				LPCTSTR pAccel = strchr(pCaption, '&');

				while ((pAccel != NULL) && (pAccel[1] == '&'))
					pAccel = strchr(pCaption, '&');

				if ((pAccel != NULL)
				    && (toupper(pAccel[1]) == toupper(key))) {
					if ((dlgCode & (DLGC_STATIC | DLGC_RADIOBUTTON)) != 0) {
						obj = nextTabStop(hWnd, obj, FALSE);
						if (obj)
							PostMessage(hWnd, WM_NEXTDLGCTL,
								    (WPARAM) obj, 1);
					} else
						PostMessage(hWnd, WM_COMMAND,
							    obj->id, (LPARAM) obj);
					return TRUE;
				}
			}
		}
		obj = obj->siblings;
	}

	return FALSE;
}

/*
 *  Restore focus to last control
 */
static void
dlgRestoreCtrlFocus(HWND hWnd)
{
	PMWDLGDATA pData;

	pData = DLG_PMWDLGDATA(hWnd);
	if (pData == NULL)
		return;
	if (pData->hWndFocus != NULL)
		SetFocus(pData->hWndFocus);
}

static void
dlgSaveCtrlFocus(HWND hWnd, HWND hFocus)
{
	PMWDLGDATA pData;

	pData = DLG_PMWDLGDATA(hWnd);
	if (pData == NULL)
		return;
	if (!ISDLGCONTROL(hWnd, hFocus))
		return;
	pData->hWndFocus = hFocus;
}


/*
 *  Handles default dialog messages
 */
BOOL CALLBACK
DefDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HWND hChild;
	RECT rc;

	switch (Msg) {
	case WM_COMMAND:
		if (wParam == IDCANCEL)
			EndDialog(hDlg, TRUE);
		return FALSE;

	case WM_ERASEBKGND:
		GetClientRect(hDlg, &rc);
		FastFillRect((HDC) wParam, &rc, GetSysColor(COLOR_BTNFACE));
		return TRUE;

	case WM_CTLCOLORSTATIC:
		return DefWindowProc(hDlg, Msg, wParam, lParam);

	case WM_INITDIALOG:
		return TRUE;

	case WM_NEXTDLGCTL:
		if ((LOWORD(lParam) != FALSE)) {
			if (IsWindow((HWND) wParam))
				SetFocus((HWND) wParam);
		} else {
			hChild = nextTabStop(hDlg, dlgGetCtrlFocus(hDlg), (wParam != 0));
			if (hChild)
				SetFocus(hChild);
		}
		dlgSaveCtrlFocus(hDlg, GetFocus());
		break;
	}

	return FALSE;
}

/*
 *  Destroy initial font if was created.
 */
static void
dlgDestroyInitFont(HWND hWnd, PMWDLGDATA pData)
{
	if ((pData->flags & DLGF_DESTROYFONT) != 0) {
		HDC hdc = GetDC(hWnd);

		SelectObject(hdc, GetStockObject(DEFAULT_FONT));
		DeleteObject(pData->hFnt);
		ReleaseDC(hWnd, hdc);
		pData->flags &= ~DLGF_DESTROYFONT;
	}
}

/*
 *  Returns the name of class found in item.
 */
static LPCSTR
dlgGetItemClass(PMWDLGITEMTEMPLEXTRA pItem)
{
	static LPCSTR defClass[] = {
		"BUTTON", "EDIT", "STATIC", "LISTBOX", "SCROLLBAR", "COMBOBOX"
	};

	if ((pItem->szClassName == NULL) || (strlen(pItem->szClassName) < 2))
		return "";

	if ((pItem->szClassName[0] == (TCHAR) - 1)) {
		unsigned idx = ((unsigned char) pItem->szClassName[1]) -
			DLGITEM_CLASS_FIRSTID;

		if ((idx < (sizeof(defClass) / sizeof(defClass[0]))))
			return defClass[idx];

		return "";
	}

	return pItem->szClassName;
}


DWORD
dlgItemStyle(PMWDLGITEMTEMPLATE pItem, PMWDLGITEMTEMPLEXTRA pItemExtra)
{
	if ((pItemExtra->szClassName[0] == (TCHAR) - 1) &&
	    (pItemExtra->szClassName[1] == (TCHAR) DLGITEM_CLASS_LISTBOX) ||
	    !strcmp(pItemExtra->szClassName, "LISTBOX")) {
		return pItem->style & ~(LBS_CHECKBOX | LBS_USEICON | LBS_AUTOCHECK);
	}
	return pItem->style;
}


/*
 *  Check if it should return a long value
 */
static BOOL
dlgReturnLValue(UINT msg)
{
	if ((msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
	    || (msg == WM_CTLCOLOR) || (msg == WM_INITDIALOG)
	    || (msg == WM_DRAWITEM) || (msg == WM_MEASUREITEM)
	    /*|| (msg == WM_VKEYTOITEM) || (msg == WM_CHARTOITEM)
	      || (msg == WM_QUERYDRAGICON) ||(msg == WM_COMPAREITEM)*/ )
		return FALSE;

	return TRUE;
}

/*
 *  Handler for dialog windows
 */
static LRESULT CALLBACK
mwDialogProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	PMWDLGDATA pData;
	LRESULT retV = 0;
	DLGPROC lpFnDlg = DLG_DLGPROC(hWnd);


	if (lpFnDlg)
		retV = lpFnDlg(hWnd, Msg, wParam, lParam);

	if (!retV)
		retV = DefDlgProc(hWnd, Msg, wParam, lParam);

	//  Some messages return the value of DLGPROC
	if (!dlgReturnLValue(Msg))
		return retV;

	if (!retV) {
		switch (Msg) {
		case WM_NCDESTROY:
			pData = DLG_PMWDLGDATA(hWnd);
			if (pData) {
				HWND hPar = GetParent(hWnd);

				dlgDestroyInitFont(hWnd, pData);
				free(pData);
				SetWindowLong(hWnd, DWL_DLGDATA, 0);
				SetWindowLong(hWnd, DWL_DLGPROC, 0);
				if (hPar)
					SetActiveWindow(hPar);
			} else
				EPRINTF("WARN: mwDialogProc: WM_NCDESTROY without dlgParams\n");
			break;

		case WM_SETFONT:
			pData = DLG_PMWDLGDATA(hWnd);
			if (pData == NULL)
				return 0;
			dlgDestroyInitFont(hWnd, pData);
			pData->hFnt = (HFONT) wParam;
			if (LOWORD(lParam) != 0)
				InvalidateRect(hWnd, NULL, TRUE);
			return TRUE;

		case WM_GETFONT:
			pData = DLG_PMWDLGDATA(hWnd);
			if (pData == NULL)
				return 0;
			return (LPARAM) pData->hFnt;

		case WM_SETFOCUS:
			dlgSaveCtrlFocus(hWnd, (HWND) wParam);
			dlgRestoreCtrlFocus(hWnd);
			return 0;

		case WM_SHOWWINDOW:
			if (!wParam)
				dlgSaveCtrlFocus(hWnd, GetFocus());
			break;

		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
				dlgSaveCtrlFocus(hWnd, GetFocus());
			else
				dlgRestoreCtrlFocus(hWnd);
			return 0;

		case WM_NCHITTEST:
			{
				POINT curpt;
				POINTSTOPOINT(curpt, lParam);
				if (PtInRect(&hWnd->clirect, curpt))
					return HTNOWHERE;
				return DefWindowProc(hWnd, Msg, wParam, lParam);
			}
		}
		return DefWindowProc(hWnd, Msg, wParam, lParam);
	}

	return DLG_MSGRESULT(hWnd);
}

/*
 *  Process messages sent to a dialog and childs
 */
BOOL WINAPI
IsDialogMessage(HWND hDlg, LPMSG lpMsg)
{
	static BOOL kShiftStatus = FALSE;
	HWND hChild;
	int dlgCode;

	if (lpMsg->hwnd == hDlg || ISDLGCONTROL(hDlg, lpMsg->hwnd)) {
		/*  first check if message is a key message...  */
		switch (lpMsg->message) {
		case WM_KEYUP:
			switch (lpMsg->wParam) {
			case VK_LSHIFT:
			case VK_RSHIFT:
			case VK_SHIFT:
				kShiftStatus = FALSE;
				break;
			}
			dlgCode = SendMessage(lpMsg->hwnd, WM_GETDLGCODE,
					    lpMsg->wParam, (LPARAM) lpMsg);
			if ((dlgCode & DLGC_WANTMESSAGE))
				goto skipKeyProcess;

			/* msg is sent to top-child */
			if (ISDLGCONTROL(hDlg, lpMsg->hwnd))
				lpMsg->hwnd = dlgGetCtrlTop(hDlg, lpMsg->hwnd);
			break;

		case WM_KEYDOWN:
			switch (lpMsg->wParam) {
			case VK_LSHIFT:
			case VK_RSHIFT:
			case VK_SHIFT:
				kShiftStatus = TRUE;
				break;
			}
			dlgCode = SendMessage(lpMsg->hwnd, WM_GETDLGCODE,
					    lpMsg->wParam, (LPARAM) lpMsg);
			if ((dlgCode & DLGC_WANTMESSAGE))
				goto skipKeyProcess;

			/* msg is sent to top-child */
			if (ISDLGCONTROL(hDlg, lpMsg->hwnd))
				lpMsg->hwnd = dlgGetCtrlTop(hDlg, lpMsg->hwnd);
			break;

		case WM_CHAR:
			dlgCode =
				SendMessage(lpMsg->hwnd, WM_GETDLGCODE,
					    lpMsg->wParam, (LPARAM) lpMsg);
			if ((dlgCode & DLGC_WANTMESSAGE))
				goto skipKeyProcess;

			/* msg is sent to top-child */
			if (ISDLGCONTROL(hDlg, lpMsg->hwnd))
				lpMsg->hwnd =
					dlgGetCtrlTop(hDlg, lpMsg->hwnd);
			break;

		case WM_SYSCHAR:
			dlgCode = SendMessage(lpMsg->hwnd, WM_GETDLGCODE,
					    lpMsg->wParam, (LPARAM) lpMsg);

			/* msg is sent to top-child */
			if (ISDLGCONTROL(hDlg, lpMsg->hwnd)
			    && !(dlgCode & DLGC_WANTMESSAGE))
				lpMsg->hwnd = dlgGetCtrlTop(hDlg, lpMsg->hwnd);
			break;
		}

		/*  then parse special key messages...  */
		switch (lpMsg->message) {
		case WM_KEYDOWN:
			switch (lpMsg->wParam) {
			case VK_TAB:
				if ((dlgCode & DLGC_WANTTAB))
					break;
				DPRINTF("Handle VK_TAB key\n");
				hChild = nextTabStop(hDlg, dlgGetCtrlFocus(hDlg),
						     kShiftStatus);
				if (hChild)
					SendMessage(hDlg, WM_NEXTDLGCTL,
						    (WPARAM) hChild, 1);
				return TRUE;

			case VK_DOWN:
			case VK_LEFT:
			case VK_UP:
			case VK_RIGHT:
				if ((dlgCode & DLGC_WANTARROWS))
					break;
				DPRINTF("Handle ARROWS key\n");
				hChild = GetNextDlgGroupItem(hDlg, dlgGetCtrlFocus(hDlg),
						((lpMsg->wParam == VK_LEFT)
						  || (lpMsg-> wParam == VK_UP)));
				if (hChild != NULL) {
					SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM) hChild, 1);
					if ((dlgCode & DLGC_RADIOBUTTON))
						PostMessage(hChild, WM_CHAR, ' ', 0);
				}
				return TRUE;

			case VK_RETURN:
				hChild = firstDefButton(hDlg);
				if (hChild) {
					PostMessage(hDlg, WM_COMMAND,
						    MAKELONG(GetDlgCtrlID(hChild), BN_CLICKED),
						    (LPARAM) hChild);
				} else
					SendMessage(hDlg, WM_COMMAND, IDOK, 0);
				return TRUE;

			case VK_CANCEL:
			case VK_ESCAPE:
				PostMessage(hDlg, WM_COMMAND, IDCANCEL,
					    (LPARAM) GetDlgItem(hDlg, IDCANCEL));
				return TRUE;
			}
			break;

		case WM_CHAR:
			if ((dlgCode & (DLGC_WANTCHARS)))
				break;
			if ((lpMsg->wParam == '\t')
			    && (dlgCode & DLGC_WANTTAB))
				break;
			if (parseAccelerator(hDlg, lpMsg->wParam))
				return TRUE;
			break;

		case WM_SYSCHAR:
			if (parseAccelerator(hDlg, lpMsg->wParam))
				return TRUE;
			DPRINTF("SYSCHAR %08X %08X\n", lpMsg->wParam,
			       lpMsg->lParam);
			break;
		}

skipKeyProcess:
		TranslateMessage(lpMsg);
		DispatchMessage(lpMsg);
		return TRUE;
	}

	return FALSE;
}

/*
 *  Do a modal DIALOG with arguments
 */
int WINAPI
DialogBoxParam(HINSTANCE hInstance, LPCTSTR lpTemplate, HWND hWndParent,
	       DLGPROC lpDialogFunc, LPARAM lParam)
{
	int retV = 0;
	MSG msg;
	DWORD ltm = 0;
	PMWDLGDATA params;
	HWND hDlg = CreateDialogParam(hInstance, lpTemplate, hWndParent,
				  lpDialogFunc, lParam);

	if (hDlg) {
		BOOL bSendIdle = TRUE;
		params = DLG_PMWDLGDATA(hDlg);
		params->running = TRUE;

		if (hWndParent)
			EnableWindow(hWndParent, FALSE);

		ShowWindow(hDlg, SW_SHOW);
		SetActiveWindow(hDlg);
		if ((params->hWndFocus != NULL)
		    && IsWindowEnabled(params->hWndFocus))
			SetFocus(params->hWndFocus);
		else
			PostMessage(hDlg, WM_NEXTDLGCTL, 0, 0);

		UpdateWindow(hDlg);

		while (IsWindow(hDlg) && params->running) {
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				if (!IsDialogMessage(hDlg, &msg)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				bSendIdle = TRUE;
				if (msg.message == WM_QUIT)
					break;

				if (msg.time != ltm)
					MwHandleTimers();

				ltm = msg.time;
			} else {
				if (bSendIdle) {
					SendMessage(hDlg, WM_ENTERIDLE, 0, 0);
					bSendIdle = FALSE;
				}
				MwHandleTimers();
#ifdef MW_CALL_IDLE_HANDLER
				idle_handler();
#endif
			}
		}
		if (!params->running) {
			retV = params->nResult;
			DestroyWindow(hDlg);
		}

		if (hWndParent)
			EnableWindow(hWndParent, TRUE);
	}

	return retV;
}


/*
 *  Do a modal DIALOG
 */
int WINAPI
DialogBox(HINSTANCE hInstance, LPCTSTR lpTemplate, HWND hWndParent,
	  DLGPROC lpDialogFunc)
{
	return DialogBoxParam(hInstance, lpTemplate, hWndParent, lpDialogFunc, 0);
}


/*
 *  Terminate a DIALOG
 */
BOOL WINAPI
EndDialog(HWND hDlg, int nResult)
{
	PMWDLGDATA params = DLG_PMWDLGDATA(hDlg);
	if (params) {
		params->running = FALSE;
		params->nResult = nResult;
		return TRUE;
	}

	return FALSE;
}


/*
 *  Get base units of dialogs
 */
LONG WINAPI
GetDlgBaseUnits(void)
{
	/* return averWidth and Height of system font.*/
	return MAKELONG(5, 13);
}


/*
 *  Convert dialog base units in screen units
 */
BOOL WINAPI
MapDialogRect(HWND hWnd, LPRECT lpRc)
{
	static const char defAlpha[] =
		" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	TEXTMETRIC tm;
	SIZE sz;
	HDC hdc;
	HFONT oldFnt;
	BOOL retV = FALSE;

	hdc = GetDC(hWnd);
	oldFnt = SelectObject(hdc,
			      (HFONT) SendMessage(hWnd, WM_GETFONT, 0, 0));
	if (GetTextExtentPoint(hdc, defAlpha, sizeof(defAlpha) - 1, &sz))
	//if( GetTextMetrics(hdc, &tm) )
	{
		//sz.cx = tm.tmAveCharWidth + tm.tmOverhang;
		//sz.cy = tm.tmHeight + tm.tmExternalLeading;
		sz.cx = (sz.cx +
			 (sizeof(defAlpha) - 1) / 2) / (sizeof(defAlpha) - 1);
		lpRc->left = MulDiv(lpRc->left, sz.cx, 4);
		lpRc->right = MulDiv(lpRc->right, sz.cx, 4);
		lpRc->top = MulDiv(lpRc->top, sz.cy, 8);
		lpRc->bottom = MulDiv(lpRc->bottom, sz.cy, 8);
		retV = TRUE;
	}

	SelectObject(hdc, oldFnt);
	ReleaseDC(hWnd, hdc);
	return retV;
}


HWND WINAPI
CreateDialog(HINSTANCE hInstance, LPCSTR lpTemplate, HWND hWndParent,
	     DLGPROC lpDialogFunc)
{
	return CreateDialogParam(hInstance, lpTemplate, hWndParent,
				 lpDialogFunc, 0);
}


HWND WINAPI
CreateDialogParam(HINSTANCE hInstance, LPCTSTR lpTemplate,
		  HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam)
{
	PMWDLGTEMPLATE pDlg;
	MWDLGTEMPLEXTRA dlgExtra;
	HWND hDlg, retV;
	HWND hFocus, hCtrl;
	HFONT hFnt;
	RECT rc;
	HDC hdc;
	PMWDLGDATA pData;
	HGLOBAL hResDlg;
	int i;

	HRSRC hRes = FindResource(hInstance, lpTemplate, RT_DIALOG);
	if (hRes == NULL)
		return NULL;

	hResDlg = LoadResource(hInstance, hRes);
	pDlg = (PMWDLGTEMPLATE) LockResource(hResDlg);
	if (pDlg == NULL)
		return NULL;

	resGetDlgTemplExtra(pDlg, &dlgExtra);

	hDlg = NULL;
	retV = NULL;
	hFocus = NULL;

	do {
		BOOL bVisible = ((pDlg->style & WS_VISIBLE) != 0);
		rc.left = pDlg->x;
		rc.top = pDlg->y;
		rc.right = rc.left + pDlg->cx;
		rc.bottom = rc.top + pDlg->cy;
		pDlg->style &= ~WS_VISIBLE;	/* dlg should be showed at end.*/

		hDlg = CreateWindowEx(pDlg->dwExtendedStyle, "GDLGCLASS",
				      dlgExtra.szDlgName,
				      pDlg->style | DLG_DEF_STYLE,
				      0, 0, 100, 100,
				      hWndParent, NULL, hInstance, NULL);
		if (hDlg == NULL)
			break;

		hdc = GetDC(hDlg);

		pData = (PMWDLGDATA) malloc(sizeof(MWDLGDATA));
		if (pData == NULL)
			break;

		SetWindowLong(hDlg, DWL_DLGDATA, (LPARAM) pData);
		SetWindowLong(hDlg, DWL_DLGPROC, (LPARAM) lpDialogFunc);
		SetWindowLong(hDlg, DWL_USER, dwInitParam);
		pData->flags = 0;
		pData->running = FALSE;
		pData->nResult = 0;
		pData->hWndFocus = NULL;

		// create a font or use default
		if ((dlgExtra.szFontName != NULL) &&
		    ((hFnt = CreateFont(-MulDivRD(dlgExtra.fontSize,
				 GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0,
				 0, 0, 0, 0, 0, 0, 0, 0, DEFDLG_FONT_QUALITY,
				 FF_DONTCARE | DEFAULT_PITCH,
				 dlgExtra.szFontName)) != NULL)) {
			pData->flags |= DLGF_DESTROYFONT;
		} else
			hFnt = GetStockObject(DEFAULT_FONT);

		pData->hFnt = hFnt;

		// calculate screen coords and resize window.
		SelectObject(hdc, pData->hFnt);

		MapDialogRect(hDlg, &rc);
		MoveWindow(hDlg, rc.left, rc.top,
			   rc.right - rc.left, 16 + rc.bottom - rc.top, FALSE);


		//  items creation
		for (i = 0; i < pDlg->cdit; i++) {
			PMWDLGITEMTEMPLATE pItem = dlgExtra.pItems[i];
			PMWDLGITEMTEMPLEXTRA pItemExtra =
				&dlgExtra.pItemsExtra[i];
			DWORD style = dlgItemStyle(pItem, pItemExtra);

			rc.left = pItem->x;
			rc.top = pItem->y;
			rc.right = rc.left + pItem->cx;
			rc.bottom = rc.top + pItem->cy;
			MapDialogRect(hDlg, &rc);
			hCtrl = CreateWindowEx(pItem->dwExtendedStyle,
				 dlgGetItemClass(pItemExtra),
				 pItemExtra->szCaption, style,
				 rc.left, rc.top,
				 rc.right - rc.left, rc.bottom - rc.top,
				 hDlg, (HMENU) (int) pItem->id,
				 hInstance, pItemExtra->lpData);

			if (hCtrl != NULL) {
				if ((hFocus == NULL)
				    && (pItem->style & WS_TABSTOP)) {
					hFocus = hCtrl;
				}

				SendMessage(hCtrl, WM_SETFONT,
					    (WPARAM) pData->hFnt, 0);
			} else
				EPRINTF("Error on creating item %d\n", i);
		}

		ReleaseDC(hDlg, hdc);

		if (bVisible) {
			MSG msg;
			ShowWindow(hDlg, SW_SHOW);
			//SetActiveWindow ( hDlg );
			UpdateWindow(hDlg);

			// MW needs to be painted
			while (PeekMessage(&msg, hDlg, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		//  Finally, send a WM_INITDIALOG message.
		//  If returned value is nonzero, sets the focus to the first available item.
		if (SendMessage(hDlg, WM_INITDIALOG, (WPARAM) hFocus, dwInitParam)
		    && (hFocus != NULL)) {
			pData->hWndFocus = hFocus;
		}

		retV = hDlg;
	} while (0);


	if (retV == NULL) {
		if (hDlg != NULL)
			DestroyWindow(hDlg);
	}


	resDiscardDlgTemplExtra(&dlgExtra);
	UnlockResource(hResDlg);
	FreeResource(hResDlg);
	return retV;
}
