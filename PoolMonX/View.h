// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "NtDll.h"
#include "resource.h"

struct TagItem {
	SYSTEM_POOLTAG TagInfo;
	PCWSTR SourceName = L"";
	PCWSTR SourceDesc = L"";
	CStringA Tag;
	int Index;
};

struct CellColorKey {
	int Row, Column;

	bool operator ==(const CellColorKey& other) const {
		return Row == other.Row && Column == other.Column;
	}
};

template<>
struct std::hash<CellColorKey> {
	size_t operator()(const CellColorKey& key) const {
		return (key.Row << 10) ^ key.Column;
	}
};

struct CellColor : CellColorKey {
	COLORREF TextColor, BackColor;
	LOGFONT Font;
};

class CView : 
	public CWindowImpl<CView, CListViewCtrl>,
	public CCustomDraw<CView>,
	public CIdleHandler,
	public CUpdateUI<CView> {
public:
	enum ColumnType {
		TagName,
		PagedAllocs,
		PagedFrees,
		PagedDiff,
		PagedUsage,

		NonPagedAllocs,
		NonPagedFrees,
		NonPagedDiff,
		NonPagedUsage,

		SourceName,
		SourceDescription,
		NumColumns
	};

	DECLARE_WND_SUPERCLASS(nullptr, CListViewCtrl::GetWndClassName())

	void LoadPoolTagText();
	void UpdatePoolTags();
	void AddTag(const SYSTEM_POOLTAG& info, int index);
	void UpdateVisible();
	bool CompareItems(const TagItem& i1, const TagItem& i2);
	void DoSort();
	void SetToolBar(HWND hWnd);

	void AddCellColor(ULONG tag, const CellColor& cell, DWORD64 targetTime = 0);
	void RemoveCellColor(ULONG tag);

	size_t GetTotalPaged() const {
		return m_TotalPaged;
	}

	size_t GetTotalNonPaged() const {
		return m_TotalNonPaged;
	}

	BOOL PreTranslateMessage(MSG* pMsg);

	// CCustomDraw
	DWORD OnPrePaint(int id, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int id, LPNMCUSTOMDRAW cd);
	DWORD OnSubItemPrePaint(int id, LPNMCUSTOMDRAW cd);

	BOOL OnIdle() override;

	BEGIN_UPDATE_UI_MAP(CView)
		UPDATE_ELEMENT(ID_VIEW_PAUSE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_COPY, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_UPDATEINTERVAL_1SECOND, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_UPDATEINTERVAL_2SECONDS, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_UPDATEINTERVAL_5SECONDS, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_UPDATEINTERVAL_10SECONDS, UPDUI_MENUPOPUP)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP(CView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(CFindReplaceDialog::GetFindReplaceMsg(), OnFindDialogMessage)
		COMMAND_ID_HANDLER(ID_EDIT_FIND, OnEditFind)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PAUSE, OnViewPauseResume)
		COMMAND_RANGE_HANDLER(ID_UPDATEINTERVAL_1SECOND, ID_UPDATEINTERVAL_10SECONDS, OnChangeUpdateInterval);
		CHAIN_MSG_MAP(CUpdateUI<CView>)
		CHAIN_MSG_MAP_ALT(CCustomDraw<CView>, 1)
		REFLECTED_NOTIFY_CODE_HANDLER(LVN_GETDISPINFO, OnGetDisplayInfo)
		REFLECTED_NOTIFY_CODE_HANDLER(LVN_COLUMNCLICK, OnColumnClick)
		REFLECTED_NOTIFY_CODE_HANDLER(LVN_ITEMCHANGED, OnItemChanged)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	LRESULT OnFindDialogMessage(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnGetDisplayInfo(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnColumnClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnChangeUpdateInterval(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewPauseResume(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	struct TimerInfo {
		DWORD64 TargetTime;
		std::function<void(void)> Callback;
		COLORREF Color;
	};
	std::vector<TimerInfo> m_Timers;
	std::unordered_map<ULONG, CellColor> m_CellColors;

	int m_SortColumn = -1;
	CImageList m_Images;
	CFindReplaceDialog* m_pFindDialog{ nullptr };
	int m_UpdateInterval = 2000;
	size_t m_TotalPaged = 0, m_TotalNonPaged = 0;
	std::unordered_map<ULONG, std::shared_ptr<TagItem>> m_TagsMap;
	std::vector<std::shared_ptr<TagItem>> m_Tags, m_TagsView;
	std::map<CStringA, std::pair<CString, CString>> m_TagSource;

	SYSTEM_POOLTAG_INFORMATION* m_PoolTags{ nullptr };
	bool m_Running{ true };
	bool m_Ascending = true;
};
