#pragma once

#include "stdafx.h"
#include "default_ui_hacks.h"
#include "queue_helpers.h"
#include "DropSourceImpl.h"
#include "DropTargetImpl.h"
#include "config.h"
#include "queuecontents_titleformat_hook.h" 
#include "ui_element_host.h"
#include "reorder_helpers.h"
#include "queuecontents_lock.h"



/*
Some (useful?) resources:
http://www.codeproject.com/KB/wtl/customdrawlist_wtl.aspx Custom drawn controls
http://www.codeproject.com/KB/list/Dragger.aspx Dragger stuff
*/

// No list item context menu
enum {
	// Show/Hide Column Header
	ID_SHOW_COLUMN_HEADER = 1
};

// List item context menu
enum {
	// let's not redefine
	/*ID_SHOW_COLUMN_HEADER*/

	// ID for "Remove"
	ID_REMOVE = 2,
	// ID for "Move to Top" and 
	ID_MOVE_TOP = 3,
	// ID for "Move Up"
	ID_MOVE_UP = 4,
	// ID for "Move Down"
	ID_MOVE_DOWN = 5,
	// ID for "Move to Bottom"
	ID_MOVE_BOTTOM = 6		

	/*
	// The range ID_CONTEXT_FIRST through ID_CONTEXT_LAST is reserved
	// for menu entries from menu_manager.
	ID_CONTEXT_FIRST,
	ID_CONTEXT_LAST = ID_CONTEXT_FIRST + 1000,
	*/
};

// Header item context menu
enum {
	// let's not redefine
	/*ID_SHOW_COLUMN_HEADER*/
	// Reserved for columns
	ID_COLUMNS_FIRST = 2,
	// Reserved for columns
	ID_COLUMNS_LAST = ID_COLUMNS_FIRST + 1000,	
	ID_MORE = ID_COLUMNS_LAST + 1,
	ID_RELATIVE_WIDTHS = ID_COLUMNS_LAST + 2, // "Auto-scale Columns with Window Size"

};

class CCustomListView : public WTL::CCustomDraw<CCustomListView>, public ATL::CWindowImpl<CCustomListView, WTL::CListViewCtrl>,
	public MetaDbHandleDropTarget, public DropSourceInternalDraggingNotify
{
public:	

	BEGIN_MSG_MAP_EX(CCustomListView) 
		//NOTIFY_CODE_HANDLER(LVN_BEGINDRAG, OnBeginDrag)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_SYSKEYDOWN(OnSysKeyDown)
		MSG_WM_KEYDOWN(OnKeyDown)
		MSG_WM_GETDLGCODE(OnGetDlgCode)
		MSG_WM_MOUSEMOVE(OnMouseMove)
		MSG_WM_LBUTTONUP(OnLButtonUp)
		MSG_WM_DESTROY(OnDestroy)
		if(uMsg == WM_CONTEXTMENU) {			
			if(m_callback.is_valid() && m_callback->is_edit_mode_enabled()) {
				SetMsgHandled(TRUE);
				lResult = ::DefWindowProc(hWnd, uMsg, wParam, lParam);
			
				if(IsMsgHandled()) {
					return TRUE;
				}
			}
		}
		MSG_WM_SETFOCUS(OnSetFocus)
		MSG_WM_KILLFOCUS(OnKillFocus)
		MSG_WM_CONTEXTMENU(OnContextMenu)
		MSG_WM_LBUTTONDOWN(OnLButtonDown)
		MSG_WM_CAPTURECHANGED(OnCaptureChanged)
		NOTIFY_CODE_HANDLER(HDN_ENDTRACK, OnHeaderEndTrack) // Column resize has ended
		NOTIFY_CODE_HANDLER(HDN_ENDDRAG, OnHeaderEndDrag) // Just before Columns are reordered, after the drag ended
		NOTIFY_CODE_HANDLER(HDN_BEGINDRAG, OnHeaderBeginDrag) // Colum drag started
		NOTIFY_CODE_HANDLER(HDN_DIVIDERDBLCLICK, OnHeaderDividerDblClick);
		CHAIN_MSG_MAP(CCustomDraw<CCustomListView>)		
	END_MSG_MAP()      

	CCustomListView();

	void SetColors(COLORREF backroundcolor,
		COLORREF selectioncolor, COLORREF textcolor, COLORREF insertmarkercolor, COLORREF selectionrectanglecolor, COLORREF textselectedcolor);	

	// CCustomDraw
	DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
	DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw);
	DWORD OnItemPostPaint(int idCtrl, LPNMCUSTOMDRAW lpNMCustomDraw);	

	// gets selected indices in the index-ascending order
	template<class t_list>
	void GetSelectedIndices(t_list& p_out);

	// MetaDbHandleDropTarget, we work-around through no virtual member function templates restriction
	virtual void GetSelectedIndices(pfc::list_base_t<t_size>& p_out);
	
	void GetSelectedItemsMetaDbHandles(metadb_handle_list_ref p_out);
	void GetMetaDbHandles(const pfc::list_base_const_t<t_size>& indices, metadb_handle_list_ref p_out);

	void SetSelectedIndices(const pfc::list_base_const_t<t_size>& p_in);
	
	// Drop target stuff
	STDMETHOD(DragLeave)();
	STDMETHOD(DragEnter)(DWORD grfKeyState, CPoint pt_screen, CPoint pt_client, DWORD *pdwEffect);
	STDMETHOD(DragOver)(DWORD grfKeyState, CPoint pt_screen, CPoint pt_client, DWORD *pdwEffect);
	STDMETHOD(Drop)(const pfc::list_base_const_t<metadb_handle_ptr>& handles, DWORD grfKeyState, CPoint pt_screen, CPoint pt_client, DWORD *pdwEffect, 
		const pfc::list_base_const_t<t_size>& selection, bool internal_dragging, const ActivePlaylistInformation & active_playlist_information);
	// DetermineDropAction is called from the DropTargetImpl to determine proper effect
	// Currently we ignore Drop method's effects
	STDMETHOD(DetermineDropAction)(DWORD grfKeyState, CPoint pt_screen, CPoint pt_client, DWORD *pdwEffect, bool internal_dragging);

	// DropSourceInternalDraggingNotify
	virtual void SetInternalDragging(bool value);
	virtual bool GetInternalDragging();

	// Called when queue is changed
	void QueueRefresh();	

	void SetHost(ui_element_host* host);

	// Column modifications
	void AddUIColumn(long column_id);
	void RemoveUIColumn(long column_id);
	template<typename array_t>
	void ReorderUIColumns(const array_t& order);
	void RelayoutUIColumns();

	// Show or hides colum header based on current status
	void ShowHideColumnHeader();
	void AppendShowColumnHeaderMenu(CMenuHandle menu, int ID);

	int AddColumnHelper(LPCTSTR strItem, int nItem, int nSubItem = -1,		
			int nFmt = LVCFMT_LEFT) {			
		int nMask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		return AddColumn(strItem, nItem, nSubItem, nMask, nFmt);
	}

	void BuildContextMenu(CPoint screen_point, CMenuHandle menu, unsigned p_id_base);
	void CommandContextMenu(CPoint screen_point, unsigned p_id, unsigned p_id_base);
	void EditModeContextMenuGetFocusPoint(CPoint & pt);
	void SetCallback(ui_element_instance_callback_ptr p_callback);

	// Updates column widths, and width of the ui element
	// Saves the modification.
	// Returns true on success.
	bool UpdateSettingsWidths();

	metadb_handle_list_ref GetAllMetadbs();
	
private:
	int OnCreate(LPCREATESTRUCT lpCreateStruct);

	void DrawSelectionRectangle(CDC & cdc, const CRect & rect, const CRect & prev_rect);
	
	void OnContextMenu(CWindow wnd, CPoint point);

	void BuildHeaderContextMenu(CMenuHandle menu, unsigned p_id_base, CPoint point);
	void BuildListItemContextMenu(CMenuHandle menu, unsigned p_id_base, CPoint point);
	void BuildListNoItemContextMenu(CMenuHandle menu, unsigned p_id_base, CPoint point);

	void CommandHeaderContextMenu(unsigned p_id, unsigned p_id_base, CPoint point);
	void CommandListItemContextMenu(unsigned p_id, unsigned p_id_base, CPoint point);
	void CommandListNoItemContextMenu(unsigned p_id, unsigned p_id_base, CPoint point);

	// Selection related
	void OnMouseMove(UINT nFlags, CPoint point);
	void OnLButtonUp(UINT nFlags, CPoint point);
	void OnLButtonDown(UINT nFlags, CPoint point);
	void UpdateInsertMarker(int i);
	void DrawInsertMarker(int i, bool enable);
	void UpdateSelection(CPoint end_point, bool applyCtrlSelection = false);
	void SelectIndices(int start, int end, bool applyCtrlSelection = false);
	void OnCaptureChanged(CWindow wnd);
	void OnDestroy();

	void MoveSelectedItems(bool up);

	void OnSetFocus(CWindow wndOld);
	void OnKillFocus(CWindow wndFocus);

	int IndexToOrder(int index);

	// direction = 0 <-> focused, direction < 0 upper, direction > 0 lower
	void SelectRelativeToCurrentlyFocusedItem(int direction, bool page, bool unionselection); 

	LRESULT OnHeaderEndTrack(int /*wParam*/, LPNMHDR lParam, BOOL& bHandled);
	LRESULT OnHeaderEndDrag(int /*wParam*/, LPNMHDR lParam, BOOL& bHandled);
	LRESULT OnHeaderBeginDrag(int /*wParam*/, LPNMHDR lParam, BOOL& bHandled);
	LRESULT OnHeaderDividerDblClick(int /*wParam*/, LPNMHDR lParam, BOOL& bHandled);

	int DetermineItemSelectionIndex(CPoint point);
	int DetermineItemInsertionIndex(CPoint point);
	
	// Methods for adding items to listview control:
	// Called when adding a new column
	void AddItems(pfc::list_t<t_playback_queue_item> items, t_size column_order);
	void AddItems(pfc::list_t<t_playback_queue_item> items);
	void AddItem(t_playback_queue_item item, t_size index);
	// Adds Item to a certain column
	void AddItem(t_playback_queue_item item, t_size index, t_size column_index);

	// Other
	// process ESC etc. messages
	void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	// Ensure that we get all KeyDown messages
	UINT OnGetDlgCode(LPMSG lpMsg);
	// Delete selected
	void DeleteSelected();
	// Select all items
	void SelectAll();
	
	void DeselectAll();

	void ShowFocusRectangle();

	void SetSharedSelection(metadb_handle_list_cref p_items);	

	void _print_header_debug_info(); // for debugging columns

	// This is used to notify other components of the selection
	// in our window. In this overly simplistic case, our selection
	// will be empty, when playback is stopped. Otherwise it will
	// contain the playing track.
	ui_selection_holder::ptr m_shared_selection;

	COLORREF m_backgroundcolor;
	COLORREF m_selectioncolor;
	COLORREF m_textcolor;
	COLORREF m_textselectedcolor;
	COLORREF m_insertmarkercolor;
	COLORREF m_selectionrectanglecolor;

	// flag used in custom draw
	bool m_selected;
	bool m_focused;
	// are we dragging inside the control at the moment
	bool m_internal_dragging;

	// index of the prev. insertion marker
	int m_prev_insert_marker;
	pfc::com_ptr_t<IDropTarget> m_droptarget_ptr;
	metadb_handle_list m_metadbs;

	queuecontents_titleformat_hook m_hook;

	// are we selecting currently
	bool m_selecting;
	// are we selecting with ctrl
	bool m_ctrl_selecting;
	// are we selecting with shift
	bool m_shift_selecting;

	bool m_header_dragging;
	
	// Used for holding state in ctrl+selections
	pfc::avltree_t<t_size> m_selection;
	// start index of the selection rectangle
	int m_selection_start_index;
	// start index of the shift selection
	int m_shift_selection_start_index;
	// for restoring focus rectangle
	int m_focused_item;

	// For rectangle drawing...
	CPoint m_selection_start_point;
	CRect m_selection_lastRect;

	// insert marker line weight
	static const int LINEWEIGHT = 3;

	ui_element_host* m_host;

	pfc::array_staticsize_t<long> m_header_context_menu_column_ids;

	ui_element_instance_callback_ptr m_callback;	
};

