#pragma once
#include "stdafx.h"
#include "ui_element_configuration.h"
#include "ui_element_host.h"
#include "window_manager.h"
#include "resource.h"
#include "listbox.h"
#include "queue_helpers.h"
#include "../columns_ui-sdk/ui_extension.h"

class ui_element_base : public window_manager_window, public ui_element_host,
	public metadb_io_callback_dynamic_impl_base {
public:		
	// what to do when window_manager requests refresh
	virtual void Refresh();
	// when columns might have been changed
	virtual void ColumnsChanged();
	
	// persist UI element configuration
	virtual void save_configuration() = 0;
	virtual void get_configuration(ui_element_settings** configuration);
	virtual HWND get_wnd() = 0;
	virtual void InvalidateWnd();

	virtual bool is_popup();
	virtual void close();

	// metadb_io_callback_dynamic
	virtual void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook);
protected:
	// Added one extra parameter, wnd, for determining size of the m_listview
	// It is also used for the parent of the listview
	// If wnd is NULL get_wnd() is used instead
	virtual BOOL OnInitDialog(CWindow, LPARAM, HWND wnd = NULL);
	virtual void OnSize(UINT nType, CSize size);
	virtual BOOL OnEraseBkgnd(CDCHandle);
	
	// Implementors: call m_listview's SetColors and SetFont
	virtual void RefreshVisuals() = 0;	
	// when window dies
	virtual void OnFinalMessage(HWND /*hWnd*/);

	virtual bool ActivatePlaylistUIElement() = 0;

	CCustomListView m_listview;	
	ui_element_settings m_settings;

	bool inited_successfully;
};
