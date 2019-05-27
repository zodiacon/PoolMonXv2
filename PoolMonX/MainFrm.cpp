// MainFrm.cpp : implementation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"
#include "View.h"
#include "MainFrm.h"

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (m_view.PreTranslateMessage(pMsg))
		return TRUE;

	return CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg);

}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

bool CMainFrame::WriteString(CAtlFile & file, PCWSTR text) {
	return SUCCEEDED(file.Write(text, static_cast<DWORD>((::wcslen(text) + 1)) * sizeof(WCHAR), (DWORD*)nullptr));
}

LRESULT CMainFrame::OnTimer(UINT, WPARAM, LPARAM, BOOL &) {
	CString text;
	text.Format(L"%d Tags", m_view.GetItemCount());
	m_StatusBar.SetPaneText(ID_PANE_ITEMS, text);

	text.Format(L"Paged: %u MB", (unsigned)(m_view.GetTotalPaged() >> 20));
	m_StatusBar.SetPaneText(ID_PANE_PAGED_TOTAL, text);

	text.Format(L"Non Paged: %u MB", (unsigned)(m_view.GetTotalNonPaged() >> 20));
	m_StatusBar.SetPaneText(ID_PANE_NONPAGED_TOTAL, text);

	return 0;
}

void CMainFrame::SetPaneWidths(CMultiPaneStatusBarCtrl& sb, int* arrWidths) {
	int nPanes = sb.m_nPanes;

	// find the size of the borders
	int arrBorders[3];
	sb.GetBorders(arrBorders);

	// calculate right edge of default pane (0)
	arrWidths[0] += arrBorders[2];
	for (int i = 1; i < nPanes; i++)
		arrWidths[0] += arrWidths[i];

	// calculate right edge of remaining panes (1 thru nPanes-1)
	for (int j = 1; j < nPanes; j++)
		arrWidths[j] += arrBorders[2] + arrWidths[j - 1];

	// set the pane widths
	sb.SetParts(nPanes, arrWidths);
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	// create command bar window
	HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	// attach menu
	m_CmdBar.AttachMenu(GetMenu());
	// load command bar images
	m_CmdBar.LoadImages(IDR_MAINFRAME_SMALL);
	// remove old menu
	SetMenu(nullptr);

	HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

	CreateSimpleStatusBar(nullptr);

	m_StatusBar.SubclassWindow(m_hWndStatusBar);
	
	int panes[] = {
		/*ID_DEFAULT_PANE,*/ ID_PANE_PAGED_TOTAL, ID_PANE_NONPAGED_TOTAL, ID_PANE_ITEMS
	};
	m_StatusBar.SetPanes(panes, _countof(panes), FALSE);

	int widths[] = { 150, 300, 400 };
	m_StatusBar.SetParts(_countof(panes), widths);
	//SetPaneWidths(m_StatusBar, widths);

	m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA,
		WS_EX_CLIENTEDGE);

	UIAddToolBar(hWndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	CReBarCtrl rebar = m_hWndToolBar;
	rebar.LockBands(true);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	m_view.SetToolBar(hWndToolBar);

	SetTimer(1, 2000, nullptr);

	return 0;
}

LRESULT CMainFrame::OnSize(UINT, WPARAM, LPARAM, BOOL &) {
	if (IsWindow()) {
		//UpdateLayout();
		RECT rc;
		GetClientRect(&rc);
		m_view.MoveWindow(0, 0, rc.right, rc.bottom);
	}
	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD, WORD, HWND, BOOL &) {
	auto style = GetWindowLongPtr(GWL_EXSTYLE);
	bool topmost = (style & WS_EX_TOPMOST) == 0;
	SetWindowPos(topmost ? HWND_TOPMOST : HWND_NOTOPMOST, CRect(), SWP_NOREPOSITION | SWP_NOREDRAW | SWP_NOSIZE | SWP_NOMOVE);

	UISetCheck(ID_OPTIONS_ALWAYSONTOP, topmost);

	return 0;
}

LRESULT CMainFrame::OnFileSave(WORD, WORD, HWND, BOOL &) {
	CSimpleFileDialog dlg(FALSE, L"csv", L"pool.csv", OFN_EXPLORER | OFN_HIDEREADONLY, L"CVS files\0*.csv\0All Files\0*.*\0");
	if (dlg.DoModal() == IDOK) {
		CAtlFile file;

		if (FAILED(file.Create(dlg.m_ofn.lpstrFile, GENERIC_WRITE, 0, CREATE_ALWAYS))) {
			AtlMessageBox(m_hWnd, L"Failed to open file");
			return 0;
		}
		int column = 0;
		TCHAR text[128];
		while (true) {
			LVCOLUMN col;
			col.mask = LVCF_TEXT;
			col.cchTextMax = 128;
			col.pszText = text;
			if (!m_view.GetColumn(column, &col))
				break;
			WriteString(file, col.pszText);
			WriteString(file, L",");
			column++;
		}
		WriteString(file, L"\n");

		int count = m_view.GetItemCount();
		for (int i = 0; i < count; i++) {
			for (int c = 0; c < column; c++) {
				m_view.GetItemText(i, c, text, _countof(text));
				WriteString(file, text);
				WriteString(file, L",");
			}
			WriteString(file, L"\n");
		}
	}
	return 0;
}

LRESULT CMainFrame::OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	// TODO: add code to initialize document

	return 0;
}

LRESULT CMainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	static BOOL bVisible = TRUE;	// initially visible
	bVisible = !bVisible;
	CReBarCtrl rebar = m_hWndToolBar;
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// toolbar is 2nd added band
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}
