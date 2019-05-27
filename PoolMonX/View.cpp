// View.cpp : implementation of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "View.h"

#pragma comment(lib, "ntdll")

void CView::LoadPoolTagText() {
	auto hModule = _Module.GetResourceInstance();
	auto hResource = ::FindResource(hModule, MAKEINTRESOURCE(IDR_POOLTAG), L"TXT");
	ATLASSERT(hResource);

	auto hGlobal = ::LoadResource(hModule, hResource);
	ATLASSERT(hGlobal);

	auto data = static_cast<const char*>(::LockResource(hGlobal));
	auto next = data;
	for (; next; data = next + 1) {
		next = strchr(data, '\n');

		if (strncmp(data, "//", 2) == 0 || _strnicmp(data, "rem", 3) == 0
			|| strncmp(data, "\r\n", 2) == 0)
			continue;

		// read the tag
		std::string tag(data, data + 4);

		// locate the first dash
		auto dash1 = strchr(data, '-');
		if (dash1 == nullptr)
			continue;

		// locate second dash
		auto dash2 = strchr(dash1 + 1, '-');
		if (dash2 == nullptr)
			continue;

		if (dash2 > next) {
			dash2 = dash1;
			dash1 = nullptr;
		}

		CStringA trimmedTag(tag.c_str());
		trimmedTag.TrimLeft();
		trimmedTag += L"  ";
		if (trimmedTag.GetLength() > 4)
			trimmedTag = trimmedTag.Mid(0, 4);
		CStringW trimmedDriverName(L"");
		if (dash1) {
			std::string driverName(dash1 + 1, dash2);
			trimmedDriverName = driverName.c_str();
			trimmedDriverName.Trim();
		}

		std::string driverDesc(dash2 + 1, next - 1);
		CStringW trimmedDriverDesc(driverDesc.c_str());
		trimmedDriverDesc.Trim();

		m_TagSource.insert({ trimmedTag, std::make_pair(CString(trimmedDriverName), CString(trimmedDriverDesc)) });
	}
}

void CView::UpdatePoolTags() {
	m_CellColors.clear();
	if (m_PoolTags == nullptr) {
		LoadPoolTagText();
		m_PoolTags = static_cast<SYSTEM_POOLTAG_INFORMATION*>(::VirtualAlloc(nullptr, 1 << 22, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		if (m_PoolTags == nullptr) {
			AtlMessageBox(m_hWnd, L"Not enough memory");
			m_Error = true;
			return;
		}
	}
	ULONG len;
	auto status = NtQuerySystemInformation(SystemInformationClass::SystemPoolTagInformation, m_PoolTags, 1 << 22, &len);
	if (status) {
		AtlMessageBox(m_hWnd, L"Failed in getting pool information");
		m_Error = true;
		return;
	}

	int count = m_PoolTags->Count;
	if (m_Tags.empty()) {
		m_Tags.reserve(count + 16);
		m_TagsMap.reserve(count + 16);

		for (auto i = 0; i < count; i++) {
			const auto& info = m_PoolTags->TagInfo[i];
			m_TotalPaged += info.PagedUsed;
			m_TotalNonPaged += info.NonPagedUsed;
			AddTag(info, i);
		}
		m_TagsView = m_Tags;
		SetItemCount(count);
	}
	else {
		std::unordered_set<int> bitmap;
		int size = static_cast<int>(m_Tags.size());
		bitmap.reserve(size);
		for (int i = 0; i < size; i++)
			bitmap.insert(i);

		m_TotalPaged = m_TotalNonPaged = 0;
		for (auto i = 0; i < count; i++) {
			const auto& info = m_PoolTags->TagInfo[i];
			m_TotalPaged += info.PagedUsed;
			m_TotalNonPaged += info.NonPagedUsed;

			auto it = m_TagsMap.find(info.TagUlong);
			if (it == m_TagsMap.end()) {
				// new tag
				AddTag(info, i);
				//SetColor(i, RGB(0, 255, 0), 2000);
			}
			else {
				auto& newinfo = it->second->TagInfo;
				bool changes = ::memcmp(&info, &newinfo, sizeof(info)) != 0;
				if (changes) {
					CellColor cell;
					cell.BackColor = info.NonPagedUsed + info.PagedUsed > newinfo.PagedUsed + newinfo.NonPagedUsed ? RGB(0, 255, 255) : RGB(255, 192, 0);
					AddCellColor(info.TagUlong, cell);
				}

				bitmap.erase(it->second->Index);
				it->second->TagInfo = info;
				it->second->Index = i;
			}
		}

		int bias = 0;
		for (auto index : bitmap) {
			int i = index - bias;
			m_TagsMap.erase(m_Tags[i]->TagInfo.TagUlong);
			m_Tags.erase(m_Tags.begin() + i);
			bias++;
		}

		SetItemCountEx(count, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

		UpdateVisible();
	}
}

void CView::AddTag(const SYSTEM_POOLTAG& info, int index) {
	char tag[5] = { 0 };
	::CopyMemory(tag, &info.Tag, 4);
	auto item = std::make_shared<TagItem>();
	item->Tag = CStringA(tag);

	item->TagInfo = info;
	item->Index = index;

	if (auto it = m_TagSource.find(item->Tag); it != m_TagSource.end()) {
		item->SourceName = it->second.first;
		item->SourceDesc = it->second.second;
	}

	m_Tags.push_back(item);
	m_TagsView.push_back(item);
	m_TagsMap.insert({ info.TagUlong, item });
}

void CView::UpdateVisible() {
	int page = GetCountPerPage();
	int start = GetTopIndex();
	RedrawItems(start, start + page);
}

BOOL CView::PreTranslateMessage(MSG* pMsg) {
	if (m_pFindDialog && m_pFindDialog->IsDialogMessageW(pMsg))
		return TRUE;

	return FALSE;
}

DWORD CView::OnPrePaint(int id, LPNMCUSTOMDRAW cd) {
	return CDRF_NOTIFYITEMDRAW ;
}

DWORD CView::OnItemPrePaint(int id, LPNMCUSTOMDRAW cd) {
	if ((cd->dwDrawStage & CDDS_ITEM) == 0)
		return CDRF_DODEFAULT;

	auto lcd = (LPNMLVCUSTOMDRAW)cd;
	int row = static_cast<int>(cd->dwItemSpec);
	int col = lcd->iSubItem;

	auto& item = m_TagsView[row];
	if (auto it = m_CellColors.find(item->TagInfo.TagUlong); it != m_CellColors.end()) {
		lcd->clrTextBk = it->second.BackColor;
	}

	return CDRF_DODEFAULT;
}

DWORD CView::OnSubItemPrePaint(int id, LPNMCUSTOMDRAW cd) {
	return 0;
}

BOOL CView::OnIdle() {
	UIUpdateToolBar();

	return FALSE;
}

LRESULT CView::OnFindDialogMessage(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &) {
	if (m_pFindDialog->IsTerminating()) {
		m_pFindDialog = nullptr;
		return 0;
	}

	auto searchDown = m_pFindDialog->SearchDown();
	int start = GetSelectedIndex();
	CString find(m_pFindDialog->GetFindString());
	auto ignoreCase = !m_pFindDialog->MatchCase();
	if (ignoreCase)
		find.MakeLower();

	int from = searchDown ? start + 1 : start - 1 + GetItemCount();
	int to = searchDown ? GetItemCount() + start : start + 1;
	int step = searchDown ? 1 : -1;

	int findIndex = -1;
	for (int i = from; i != to; i += step) {
		int index = i % GetItemCount();
		const auto& item = m_TagsView[index];
		CString text(item->Tag);
		if (ignoreCase)
			text.MakeLower();
		if (text.Find(find) >= 0) {
			findIndex = index;
			break;
		}
		text = item->SourceName;
		if (ignoreCase)
			text.MakeLower();
		if (text.Find(find) >= 0) {
			findIndex = index;
			break;
		}
	}

	if (findIndex >= 0)
		SelectItem(findIndex);
	else
		AtlMessageBox(m_hWnd, L"Not found");

	return 0;
}

LRESULT CView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

	SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_HEADERINALLVIEWS | LVS_EX_HEADERDRAGDROP | LVS_EX_DOUBLEBUFFER);

	InsertColumn(ColumnType::TagName, L"Tag", LVCFMT_CENTER, 80);
	InsertColumn(ColumnType::PagedAllocs, L"Paged Allocs", LVCFMT_RIGHT, 100);
	InsertColumn(ColumnType::PagedFrees, L"Paged Frees", LVCFMT_RIGHT, 100);
	InsertColumn(ColumnType::PagedDiff, L"Paged Diff", LVCFMT_RIGHT, 80);
	InsertColumn(ColumnType::PagedUsage, L"Paged Usage", LVCFMT_RIGHT, 100);

	InsertColumn(ColumnType::NonPagedAllocs, L"Non Paged Allocs", LVCFMT_RIGHT, 100);
	InsertColumn(ColumnType::NonPagedFrees, L"Non Paged Frees", LVCFMT_RIGHT, 100);
	InsertColumn(ColumnType::NonPagedDiff, L"Non Paged Diff", LVCFMT_RIGHT, 80);
	InsertColumn(ColumnType::NonPagedUsage, L"Non Paged Usage", LVCFMT_RIGHT, 100);

	InsertColumn(ColumnType::SourceName, L"Source", LVCFMT_LEFT, 150);
	InsertColumn(ColumnType::SourceDescription, L"Source Description", LVCFMT_LEFT, 350);

	UpdatePoolTags();

	UISetRadioMenuItem(ID_UPDATEINTERVAL_2SECONDS, ID_UPDATEINTERVAL_1SECOND, ID_UPDATEINTERVAL_10SECONDS);
	SetTimer(1, m_UpdateInterval, nullptr);

	CMessageLoop* pLoop = _Module.GetMessageLoop();
	pLoop->AddIdleHandler(this);

	return 0;
}

LRESULT CView::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL &) {
	if (wParam == 1)
		UpdatePoolTags();

	return 0;
}

LRESULT CView::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled) {
	if (m_PoolTags)
		::VirtualFree(m_PoolTags, 0, MEM_DECOMMIT | MEM_RELEASE);

	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop);
	//pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);
	bHandled = FALSE;

	return 1;
}

LRESULT CView::OnGetDisplayInfo(int, LPNMHDR nmhdr, BOOL &) {
	auto disp = (NMLVDISPINFO*)nmhdr;
	auto& item = disp->item;
	auto index = item.iItem;
	auto& info = *m_TagsView[index];

	if (disp->item.mask & LVIF_TEXT) {
		switch (disp->item.iSubItem) {
			case ColumnType::TagName:
				StringCchCopyW(item.pszText, item.cchTextMax, CString(info.Tag));
				break;

			case ColumnType::PagedAllocs:
				StringCchPrintf(item.pszText, item.cchTextMax, L"%u", info.TagInfo.PagedAllocs);
				break;

			case ColumnType::PagedFrees:
				StringCchPrintf(item.pszText, item.cchTextMax, L"%u", info.TagInfo.PagedFrees);
				break;

			case ColumnType::PagedDiff:
				StringCchPrintf(item.pszText, item.cchTextMax, L"%u", info.TagInfo.PagedAllocs - info.TagInfo.PagedFrees);
				break;

			case ColumnType::PagedUsage:
			{
				auto value = info.TagInfo.PagedUsed;
				if (value < 1 << 12)
					StringCchPrintf(item.pszText, item.cchTextMax, L"%lld B", value);
				else
					StringCchPrintf(item.pszText, item.cchTextMax, L"%lld KB", value >> 10);

				break;
			}

			case ColumnType::NonPagedAllocs:
				StringCchPrintf(item.pszText, item.cchTextMax, L"%u", info.TagInfo.NonPagedAllocs);
				break;

			case ColumnType::NonPagedFrees:
				StringCchPrintf(item.pszText, item.cchTextMax, L"%u", info.TagInfo.NonPagedFrees);
				break;

			case ColumnType::NonPagedDiff:
				StringCchPrintf(item.pszText, item.cchTextMax, L"%u", info.TagInfo.NonPagedAllocs - info.TagInfo.NonPagedFrees);
				break;

			case ColumnType::NonPagedUsage:
			{
				auto value = info.TagInfo.NonPagedUsed;
				if (value < 1 << 12)
					StringCchPrintf(item.pszText, item.cchTextMax, L"%lld B", value);
				else
					StringCchPrintf(item.pszText, item.cchTextMax, L"%lld KB", value >> 10);
				break;
			}

			case ColumnType::SourceName:
				item.pszText = (PWSTR)info.SourceName;
				break;

			case ColumnType::SourceDescription:
				item.pszText = (PWSTR)info.SourceDesc;
				break;
		}
	}
	return 0;
}

LRESULT CView::OnColumnClick(int, LPNMHDR nmhdr, BOOL &) {
	auto lv = (NMLISTVIEW*)nmhdr;
	int column = lv->iSubItem;
	int oldSortColumn = m_SortColumn;
	if (column == m_SortColumn)
		m_Ascending = !m_Ascending;
	else {
		m_SortColumn = column;
		m_Ascending = true;
	}

	HDITEM h;
	h.mask = HDI_FORMAT;
	auto header = GetHeader();
	header.GetItem(m_SortColumn, &h);
	h.fmt = (h.fmt & HDF_JUSTIFYMASK) | HDF_STRING | (m_Ascending ? HDF_SORTUP : HDF_SORTDOWN);
	header.SetItem(m_SortColumn, &h);

	if (oldSortColumn >= 0 && oldSortColumn != m_SortColumn) {
		h.mask = HDI_FORMAT;
		header.GetItem(oldSortColumn, &h);
		h.fmt = (h.fmt & HDF_JUSTIFYMASK) | HDF_STRING;
		header.SetItem(oldSortColumn, &h);
	}

	DoSort();
	UpdateVisible();

	return 0;
}

LRESULT CView::OnViewFilter(WORD, WORD, HWND, BOOL &) {
	if (m_TagsView.size() == m_Tags.size()) {
		for (size_t i = 0; i < m_TagsView.size(); i++) {
			if (m_TagsView[i]->Tag.GetLength() > 0 && m_TagsView[i]->Tag[0] == L'S') {
				m_TagsView.erase(m_TagsView.begin() + i);
				i--;
			}
		}
	}
	else {
		m_TagsView = m_Tags;
	}

	DoSort();
	SetItemCountEx((int)m_TagsView.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	UpdateVisible();
	return 0;
}

LRESULT CView::OnEditFind(WORD, WORD, HWND, BOOL &) {
	if (m_pFindDialog == nullptr) {
		auto dlg = m_pFindDialog = new CFindReplaceDialog;
		dlg->Create(TRUE, L"", nullptr, FR_DOWN | FR_NOWHOLEWORD, m_hWnd);
		dlg->ShowWindow(SW_SHOW);
	}
	else {
		m_pFindDialog->SetActiveWindow();
		m_pFindDialog->SetFocus();
	}

	return 0;
}

LRESULT CView::OnChangeUpdateInterval(WORD, WORD id, HWND, BOOL &) {
	static const int intervals[] = {
		1000, 2000, 5000, 10000
	};
	ATLASSERT(id - ID_UPDATEINTERVAL_1SECOND >= 0 && id - ID_UPDATEINTERVAL_1SECOND < _countof(intervals));

	m_UpdateInterval = intervals[id - ID_UPDATEINTERVAL_1SECOND];
	if (m_Running)
		SetTimer(1, m_UpdateInterval, nullptr);
	UISetRadioMenuItem(id, ID_UPDATEINTERVAL_1SECOND, ID_UPDATEINTERVAL_10SECONDS);

	return 0;
}

LRESULT CView::OnViewPauseResume(WORD, WORD, HWND, BOOL &) {
	m_Running = !m_Running;
	if (m_Running)
		SetTimer(1, 1000, nullptr);
	else
		KillTimer(1);

	UISetCheck(ID_VIEW_PAUSE, !m_Running);

	return 0;
}

bool CView::CompareItems(const TagItem& item1, const TagItem& item2) {
	int result;
	switch (m_SortColumn) {
		case ColumnType::TagName:
			result = item2.Tag.CompareNoCase(item1.Tag);
			return m_Ascending ? (result > 0) : (result < 0);

		case ColumnType::SourceName:
			result = ::_wcsicmp(item2.SourceName, item1.SourceName);
			return m_Ascending ? (result > 0) : (result < 0);

		case ColumnType::SourceDescription:
			result = ::_wcsicmp(item2.SourceDesc, item1.SourceDesc);
			return m_Ascending ? (result > 0) : (result < 0);

		case ColumnType::PagedAllocs:
			if (m_Ascending)
				return item1.TagInfo.PagedAllocs < item2.TagInfo.PagedAllocs;
			else
				return item1.TagInfo.PagedAllocs > item2.TagInfo.PagedAllocs;

		case ColumnType::PagedFrees:
			if (m_Ascending)
				return item1.TagInfo.PagedFrees < item2.TagInfo.PagedFrees;
			else
				return item1.TagInfo.PagedFrees > item2.TagInfo.PagedFrees;

		case ColumnType::PagedUsage:
			if (m_Ascending)
				return item1.TagInfo.PagedUsed < item2.TagInfo.PagedUsed;
			else
				return item1.TagInfo.PagedUsed > item2.TagInfo.PagedUsed;

		case ColumnType::PagedDiff:
			if (m_Ascending)
				return item1.TagInfo.PagedAllocs - item1.TagInfo.PagedFrees < item2.TagInfo.PagedAllocs - item2.TagInfo.PagedFrees;
			else
				return item1.TagInfo.PagedAllocs - item1.TagInfo.PagedFrees > item2.TagInfo.PagedAllocs - item2.TagInfo.PagedFrees;

		case ColumnType::NonPagedAllocs:
			if (m_Ascending)
				return item1.TagInfo.NonPagedAllocs < item2.TagInfo.NonPagedAllocs;
			else
				return item1.TagInfo.NonPagedAllocs > item2.TagInfo.NonPagedAllocs;

		case ColumnType::NonPagedFrees:
			if (m_Ascending)
				return item1.TagInfo.NonPagedFrees < item2.TagInfo.NonPagedFrees;
			else
				return item1.TagInfo.NonPagedFrees > item2.TagInfo.NonPagedFrees;

		case ColumnType::NonPagedDiff:
			if (m_Ascending)
				return item1.TagInfo.NonPagedAllocs - item1.TagInfo.NonPagedFrees < item2.TagInfo.NonPagedAllocs - item2.TagInfo.NonPagedFrees;
			else
				return item1.TagInfo.NonPagedAllocs - item1.TagInfo.NonPagedFrees > item2.TagInfo.NonPagedAllocs - item2.TagInfo.NonPagedFrees;

		case ColumnType::NonPagedUsage:
			if (m_Ascending)
				return item1.TagInfo.NonPagedUsed < item2.TagInfo.NonPagedUsed;
			else
				return item1.TagInfo.NonPagedUsed > item2.TagInfo.NonPagedUsed;

	}

	ATLASSERT(false);
	return false;
}

void CView::DoSort() {
	std::sort(m_TagsView.begin(), m_TagsView.end(), [this](const auto& i1, const auto& i2) {
		return CompareItems(*i1, *i2);
		});
}

void CView::SetToolBar(HWND hWnd) {
	UIAddToolBar(hWnd);
}

void CView::AddCellColor(ULONG tag, const CellColor & cell, DWORD64 targetTime) {
	m_CellColors.insert({ tag, cell });
	if (targetTime > 0) {
		TimerInfo timer;
		timer.TargetTime = targetTime;
		timer.Callback = [this, tag]() {
			RemoveCellColor(tag);
		};
		m_Timers.push_back(timer);
	}
}

void CView::RemoveCellColor(ULONG tag) {
	m_CellColors.erase(tag);
}
