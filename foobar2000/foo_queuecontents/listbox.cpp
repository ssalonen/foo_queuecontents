#include "stdafx.h"
#include "listbox.h"

//http://www.codeguru.com/forum/archive/index.php/t-131726.html
// http://www.differentpla.net/content/2004/03/using-drawdragrect-to-rubber-band-a-selection


CCustomListView::CCustomListView() : m_selected(false), m_internal_dragging(false),
	m_prev_insert_marker(-1), m_hook(), m_selecting(false), 
	m_ctrl_selecting(false),m_shift_selecting(false), 
	m_selection_start_index(-1),m_shift_selection_start_index(-1),m_focused_item(-1), m_host(NULL),m_callback(NULL) {}


void CCustomListView::SetCallback(ui_element_instance_callback_ptr p_callback) {
	m_callback = p_callback;
}

void CCustomListView::OnSetFocus(CWindow wndOld) {
	TRACK_CALL_TEXT("CCustomListView::OnSetFocus");
	DEBUG_PRINT << "OnSetFocus";
	// Acquire a ui_selection_holder that allows us to notify other components
	// of the selected tracks in our window, when it has the focus.
	m_shared_selection = static_api_ptr_t<ui_selection_manager>()->acquire();

	// Inform of the current selection to foobar
	metadb_handle_list currently_selected_metadbs;
	GetSelectedItemsMetaDbHandles(currently_selected_metadbs);
	SetSharedSelection(currently_selected_metadbs);

	ShowFocusRectangle();

	// Do the default action
	SetMsgHandled(FALSE);
}
void CCustomListView::OnKillFocus(CWindow wndFocus) {
	TRACK_CALL_TEXT("CCustomListView::OnKillFocus");
	DEBUG_PRINT << "OnKillFocus";
	m_shared_selection.release();	

	// Do the default action
	SetMsgHandled(FALSE);
}

void CCustomListView::DrawSelectionRectangle(CDC & cdc, const CRect & rect, const CRect & prev_rect) {
	TRACK_CALL_TEXT("CCustomListView::DrawSelectionRectangle");
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, m_selectionrectanglecolor);
	cdc.SelectPen(pen);
	cdc.SetBkMode(TRANSPARENT);


	// Erase the previous rect
	if(prev_rect != NULL) {
		CRect inflated(prev_rect);				
		inflated.InflateRect(1,1,1,1);	
		// Further optimization seemed to make this even slower!
		WIN32_OP_D( RedrawWindow(inflated) );
	}

	// Draw the new rect
	cdc.MoveTo(rect.TopLeft());
	cdc.LineTo(CPoint(rect.right, rect.top));
	cdc.LineTo(rect.BottomRight());
	cdc.LineTo(CPoint(rect.left, rect.bottom));
	cdc.LineTo(rect.TopLeft());	
}

void CCustomListView::SetHost(ui_element_host* host) {
	m_host = host;
}

int CCustomListView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	TRACK_CALL_TEXT("CCustomListView::OnCreate");
	m_droptarget_ptr = DropTargetImpl::g_create(*this, this);
	// Register drag drop target
	RegisterDragDrop(*this, m_droptarget_ptr.get_ptr());

	SetMsgHandled(FALSE);
	return FALSE;
}

void CCustomListView::OnDestroy() {
	TRACK_CALL_TEXT("CCustomListView::OnDestroy");
	m_shared_selection.release();
	// Unregister drag drop target
	RevokeDragDrop(*this);
	m_droptarget_ptr.release();
}

LRESULT CCustomListView::OnHeaderEndTrack(int /*wParam*/, LPNMHDR lParam, BOOL& bHandled) {
	TRACK_CALL_TEXT("CCustomListView::OnHeaderEndTrack");
	LPNMHEADER phdn = reinterpret_cast<LPNMHEADER>( lParam ); 
	int index = phdn->iItem;
	PFC_ASSERT(phdn != NULL);
	PFC_ASSERT(phdn->pitem != NULL);
	PFC_ASSERT(phdn->pitem->mask & HDI_WIDTH);

	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);

	// User expects that the current ratios are preserved
	bool settingsWidthOk = UpdateSettingsWidths();
	PFC_ASSERT( settingsWidthOk );


	RelayoutUIColumns();

	// save the new width
	m_host->save_configuration();



	// We let others process this message
	bHandled = FALSE;
	return FALSE;
}

LRESULT CCustomListView::OnHeaderDividerDblClick(int /*wParam*/, LPNMHDR lParam, BOOL& bHandled) {
	TRACK_CALL_TEXT("CCustomListView::OnHeaderDividerDblClick");
	DEBUG_PRINT << "OnHeaderDividerDblClick";
	LPNMHEADER phdn = reinterpret_cast<LPNMHEADER>( lParam ); 
	// We let other process this message
	SetMsgHandled(FALSE);
	bHandled = FALSE;

	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);

	if(phdn->iButton != 0 /*Left button*/) {
		DEBUG_PRINT << "OnHeaderDividerDblClick: No left button, stopping";
		SetMsgHandled(TRUE);
		bHandled = TRUE;
		return 0;
	}

	// We don't want to resize when there is no items in the list
	if(GetItemCount() == 0) {
		DEBUG_PRINT << "OnHeaderDividerDblClick: No items, stopping";
		SetMsgHandled(TRUE);
		bHandled = TRUE;
		return 0;
	}

	// User expects that the current ratios are preserved
	bool settingsWidthOk = UpdateSettingsWidths();
	PFC_ASSERT( settingsWidthOk );

	DEBUG_PRINT << "OnHeaderDividerDblClick: Resizing column " << phdn->iItem << " with inital width of " << GetColumnWidth(phdn->iItem);
	SetColumnWidth(phdn->iItem, LVSCW_AUTOSIZE);
	settings->m_columns[phdn->iItem].m_column_width = GetColumnWidth(phdn->iItem);
	DEBUG_PRINT << "OnHeaderDividerDblClick: Resized column has new width of " << GetColumnWidth(phdn->iItem);

	RelayoutUIColumns();

	// save the new width
	m_host->save_configuration();

	// We let other process this message since bHandled = FALSE
	return 0;
}

bool CCustomListView::UpdateSettingsWidths() {
	TRACK_CALL_TEXT("CCustomListView::UpdateSettingsWidths");
	bool ret = false;
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);
	CRect r;
	WTL::CHeaderCtrl header = GetHeader();
	int column_count = header.GetItemCount();

	PFC_ASSERT(column_count == settings->m_columns.get_count());

	WIN32_OP_D( GetClientRect(r) );

	// Sometimes width is zero, here we prevent it to save to configuration
	if(r.Width() > 0) {

		for(int i = 0; i < column_count; i++) {
			int ind = header.OrderToIndex(i);
			int width = GetColumnWidth(ind);//TODO:OrderToIndeX?			
			settings->m_columns[i].m_column_width = width;
		}

		settings->m_control_width = r.Width();
		ret = true;
	} else {
		DEBUG_PRINT << "CCustomListView::UpdateSettingsWidths: No changes made since GetClientRect returns 0-width size!";
		ret = false;
	}
	

	m_host->save_configuration();
	return ret;
}

int CCustomListView::IndexToOrder(int iIndex) {
	int columnCount = GetHeader().GetItemCount();

	for(int order = 0; order < columnCount; order++) {
		if(GetHeader().OrderToIndex(order) == iIndex) {
			return order;
		}
	}
	return -1;
}

LRESULT CCustomListView::OnHeaderEndDrag(int /*wParam*/, LPNMHDR lParam, BOOL& bHandled) {
	TRACK_CALL_TEXT("CCustomListView::OnHeaderEndDrag");
	LPNMHEADER pNMH = reinterpret_cast<LPNMHEADER>( lParam );
	DEBUG_PRINT << "OnHeaderEndDrag. m_header_dragging:" << m_header_dragging;

	_print_header_debug_info();

	// The method is called twice for some reason, this prevents it
	if(m_header_dragging) {
		PFC_ASSERT( pNMH->pitem->mask & HDI_ORDER );
		if(pNMH->pitem->mask & HDI_ORDER) {
			int columnCount = GetHeader().GetItemCount();
			pfc::array_staticsize_t<int> orderarray(columnCount);
			GetHeader().GetOrderArray(columnCount, &orderarray[0]);
			
			// Look matching column ORDER based on iItem
			int from_column_order = -1;
			for(int w = 0; w < columnCount; w++) {
				if(orderarray[w] == pNMH->iItem) {
					from_column_order = w;
				}
			}
			PFC_ASSERT(from_column_order < columnCount && from_column_order >= 0);

			// Find out corresponding target (specified as the index from the left)			
			int to_column_order = pNMH->pitem->iOrder;//DetermineItemHeaderDragIndex(pt);			
			PFC_ASSERT(to_column_order < columnCount && to_column_order >= 0);
			
	

			DEBUG_PRINT << "OnHeaderEndDrag: " << from_column_order << "->" << to_column_order << ". iItem:" << pNMH->iItem;

			// -1 indicates canceled drag
			if(from_column_order != -1 && to_column_order != -1) {
				pfc::list_t<t_size> tmp2;

				pfc::array_staticsize_t<t_size> columnPermutation(columnCount);
				static_api_ptr_t<playlist_manager>()->g_make_selection_move_permutation(&columnPermutation[0], columnCount, 
					bit_array_one(from_column_order), to_column_order - from_column_order);				

				pfc::reorder_t(orderarray, &columnPermutation[0], columnCount);

				// Order column order in settings
				ReorderUIColumns(columnPermutation);
			}
			m_header_dragging = false;
		}
	}

	// To allow the control to automatically place and reorder the item, return FALSE. To prevent the item from being placed, return TRUE.	
	bHandled = FALSE;
	return FALSE;
}

LRESULT CCustomListView::OnHeaderBeginDrag(int /*wParam*/, LPNMHDR lParam, BOOL& bHandled) {
	TRACK_CALL_TEXT("CCustomListView::OnHeaderBeginDrag");
	m_header_dragging = true;	

	// We let others process this message
	bHandled = FALSE;
	return FALSE;
}

void CCustomListView::AddUIColumn(long column_id) {
	TRACK_CALL_TEXT("CCustomListView::AddUIColumn");
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);

#ifdef _DEBUG
	console::formatter()<< "Adding a column with id " << column_id;
	PFC_ASSERT(!settings->column_exists(column_id));
	PFC_ASSERT(cfg_ui_columns.exists(column_id));
#endif	

	settings->m_columns.add_item(ui_column_settings(column_id));
	m_host->save_configuration();

	pfc::list_t<t_playback_queue_item> queue;
	static_api_ptr_t<playlist_manager>()->queue_get_contents(queue);

	// Add the actual column
	pfc::map_t<long, ui_column_definition>::const_iterator column_settings = cfg_ui_columns.find(column_id);
	int column_index = AddColumnHelper(pfc::stringcvt::string_os_from_utf8(column_settings->m_value.m_name), GetHeader().GetItemCount(), column_settings->m_value.m_alignment);

	// Update all the items for this column
	AddItems(queue, column_index);

	// Make sure we have correct widths
	bool settingsWidthOk = UpdateSettingsWidths();
	PFC_ASSERT( settingsWidthOk );

	RelayoutUIColumns();

	// To fix data after adding first column again, deletion of the 0-index column does not work as expected...
	if(settings->m_columns.get_size() == 1) {
		QueueRefresh();
	}
}

void CCustomListView::RemoveUIColumn(long column_id){
	TRACK_CALL_TEXT("CCustomListView::RemoveUIColumn");
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);
#ifdef _DEBUG
	console::formatter()<< "Removing a column with id " << column_id;
	PFC_ASSERT(settings->column_exists(column_id));
#endif

	t_size index = settings->column_index_from_id(column_id);
	if(index != pfc::infinite_size) {
		settings->m_columns.remove_by_idx(index);
	}

	WIN32_OP_D( DeleteColumn(GetHeader().OrderToIndex(index)) );

	m_host->save_configuration();

}

template<typename array_t>
void CCustomListView::ReorderUIColumns(const array_t& order){	
	TRACK_CALL_TEXT("CCustomListView::ReorderUIColumns");
	DEBUG_PRINT << "ReorderUIColumns";
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);


	PFC_ASSERT(settings->m_columns.get_size() == order.get_size());

	settings->m_columns.reorder(&order[0]);
	// Save the new order of the columns
	m_host->save_configuration();
}

void CCustomListView::RelayoutUIColumns() {
	TRACK_CALL_TEXT("CCustomListView::RelayoutUIColumns");
#ifdef _DEBUG
	console::formatter()<< "Relayout columns";
#endif	
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);

	// Checking that we have column header hidden/shown as set in the settings
	// Basically the setting shouldn't change without our notice but
	// check it anyways. Also, RelayoutUIColumns is called when the listbox is created by the ui element.
	if(settings->m_show_column_header) {
		DEBUG_PRINT << "m_show_column_header is enabled. Making sure that column header is visible";
		// Remove "no header" setting
		WIN32_OP_D( SetWindowLongPtr(GWL_STYLE, GetWindowLongPtr(GWL_STYLE)  & (~LVS_NOCOLUMNHEADER)) );
	} else {		
		DEBUG_PRINT << "m_show_column_header is disabled. Making sure that column header is hidden";
		// Add "no header" setting
		WIN32_OP_D( SetWindowLongPtr(GWL_STYLE, GetWindowLongPtr(GWL_STYLE) | LVS_NOCOLUMNHEADER) );
	}


	PFC_ASSERT(GetHeader().GetItemCount() == settings->m_columns.get_count());

	// Resize all columns
	t_size columnCount = settings->m_columns.get_count();
	bool relative_column_sizes = settings->m_relative_column_widths;

	pfc::list_t<t_size> removeThese;

	for(t_size i = 0; i < columnCount; i++) {
		// Check that the column exists, and mark
		// it to be removed if it doesn't exist
		long id = settings->m_columns[i].m_id;
		pfc::map_t<long, ui_column_definition>::const_iterator column_settings = cfg_ui_columns.find(id);

		if(!column_settings.is_valid()) {
#ifdef _DEBUG
			console::formatter() << "Column with id " << id << " no longer exists. Removing it";
#endif
			removeThese.add_item(id);			
		} else {
			// Column is defined in the configuration, everything is OK

			int index = GetHeader().OrderToIndex(i);
			// Resize the column
			CRect rect;
			GetClientRect(&rect);
			int column_nominal_width = settings->m_columns[i].m_column_width;
			int column_width = column_nominal_width;
			if(relative_column_sizes && settings->m_control_width > 0) {
				column_width = (int) (((double)column_width / (double)settings->m_control_width) * (double)rect.Width());
			}

			// Resize the actual column
			WIN32_OP_D( SetColumnWidth(index, column_width) ) ;	

			// Set the correct alignment
			LVCOLUMN column;
			column.mask = LVCF_FMT | LVCF_TEXT; // only the .fmt and .pszText is valid
			column.fmt = column_settings->m_value.m_alignment;
			pfc::stringcvt::string_os_from_utf8 os_string_temporary(column_settings->m_value.m_name);			

			// We need non-const....		
			column.pszText = const_cast<TCHAR*>(os_string_temporary.get_ptr());
			WIN32_OP_D( SetColumn(index, &column) );			
		}
	}

	// Remove non-existent columns
	t_size deleteCount = removeThese.get_count();
	for(t_size i = 0; i < deleteCount;i++) {
		RemoveUIColumn(removeThese[i]);
	}
}

void CCustomListView::ShowHideColumnHeader() {
	TRACK_CALL_TEXT("CCustomListView::ShowHideColumnHeader");
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);

	// Columns are currently shown if LVS_NOCOLUMNHEADER is NOT set
	bool columns_currently_shown = !((GetWindowLongPtr(GWL_STYLE) & LVS_NOCOLUMNHEADER) != 0);

	if(columns_currently_shown) {
		// Column header is currently SHOWN. Now we hide it.
		DEBUG_PRINT << "Hiding column header";
		WIN32_OP_D( SetWindowLongPtr(GWL_STYLE, GetWindowLongPtr(GWL_STYLE) | LVS_NOCOLUMNHEADER) );
		settings->m_show_column_header = false;		
	} else {		
		// Column header is currently hidden. Now we show it.
		DEBUG_PRINT << "Showing column header";
		WIN32_OP_D( SetWindowLongPtr(GWL_STYLE, GetWindowLongPtr(GWL_STYLE) & ~LVS_NOCOLUMNHEADER) );
		settings->m_show_column_header = true;		
	}

	DEBUG_PRINT << "Saving column header visibility change";
	m_host->save_configuration();
}

void CCustomListView::AppendShowColumnHeaderMenu(CMenuHandle menu, int ID) {
	TRACK_CALL_TEXT("CCustomListView::AppendShowColumnHeaderMenu");
	int header_checked = (GetWindowLongPtr(GWL_STYLE) & LVS_NOCOLUMNHEADER) ? MF_UNCHECKED : MF_CHECKED;
	menu.AppendMenu(header_checked, ID, _T("Show Column Header"));
}

void CCustomListView::BuildContextMenu(CPoint screen_point, CMenuHandle menu, unsigned p_id_base) {
	TRACK_CALL_TEXT("CCustomListView::BuildContextMenu");
#ifdef _DEBUG
	console::formatter() << "BuildContextMenu";
#endif
	CRect headerRect;	
	GetHeader().GetClientRect(headerRect);
	GetHeader().ClientToScreen(headerRect);

	if(headerRect.PtInRect(screen_point)) {
		BuildHeaderContextMenu(menu, p_id_base, screen_point);
	} else {
		CPoint pt(screen_point);
		LVHITTESTINFO hitTestInfo;
		ScreenToClient(&pt);
		hitTestInfo.pt = pt;

		// Test if we clicked an item
		if(HitTest(&hitTestInfo) >= 0) {
			// If right clicked to non-selected item -> deselect all other items
			pfc::list_t<t_size> selected;
			GetSelectedIndices(selected);
			if(!selected.have_item(hitTestInfo.iItem)) {
				SetSelectedIndices(pfc::list_single_ref_t<t_size>(hitTestInfo.iItem));
				SetItemState(hitTestInfo.iItem, LVIS_FOCUSED, LVIS_FOCUSED);
			}

			BuildListItemContextMenu(menu, p_id_base, pt);
		} else {
			BuildListNoItemContextMenu(menu, p_id_base, screen_point);
		}
	}
}

void CCustomListView::CommandContextMenu(CPoint screen_point, unsigned p_id, unsigned p_id_base) {
	TRACK_CALL_TEXT("CCustomListView::CommandContextMenu");
#ifdef _DEBUG
	console::formatter() << "CommandContextMenu";
#endif
	CRect headerRect;	
	GetHeader().GetClientRect(headerRect);
	GetHeader().ClientToScreen(headerRect);

	if(headerRect.PtInRect(screen_point)) {
		CommandHeaderContextMenu(p_id, p_id_base, screen_point);
	} else {
		CPoint pt(screen_point);
		UINT flags;
		ScreenToClient(&pt);
#ifdef _DEBUG
		console::formatter() << "Hit item with index " << HitTest(pt, &flags);
#endif
		// Test if we clicked an item
		if(HitTest(pt, &flags) >= 0) {
			CommandListItemContextMenu(p_id, p_id_base, pt);
		} else {
			CommandListNoItemContextMenu(p_id, p_id_base, screen_point);
		}
	}
}

void CCustomListView::EditModeContextMenuGetFocusPoint(CPoint & pt) {
	TRACK_CALL_TEXT("CCustomListView::EditModeContextMenuGetFocusPoint");
	CListViewCtrl list;
	list.Attach(*this);
	// Normalize the point
	CRect r;
	GetClientRect(r);
	if(!r.PtInRect(pt)) pt = CPoint(-1,-1);
	ListView_FixContextMenuPoint(list, pt);
	list.Detach();
}

void CCustomListView::BuildHeaderContextMenu(CMenuHandle menu, unsigned p_id_base, CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::BuildHeaderContextMenu");
#ifdef _DEBUG
	console::formatter() << "BuildHeaderContextMenu";
#endif
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);

	t_size column_count = cfg_ui_columns.get_count();
	// Since iterating a map has unpredictable order, we save the ids corresponding
	// to context menu items
	m_header_context_menu_column_ids = pfc::array_staticsize_t<long>(column_count);

	AppendShowColumnHeaderMenu(menu, ID_SHOW_COLUMN_HEADER + p_id_base);
	menu.AppendMenu(MF_SEPARATOR);

	// put all the defined columns in the context menu and check those that are
	// enabled in the ui element
	int i = 0;
	for(pfc::map_t<long, ui_column_definition>::const_iterator iter = cfg_ui_columns.first(); iter.is_valid(); iter++) {
		pfc::string8 column_name(iter->m_value.m_name);
		m_header_context_menu_column_ids[i] = iter->m_key;
		UINT checked = settings->column_exists(iter->m_key) ? MF_CHECKED : MF_UNCHECKED;
		menu.AppendMenuW(checked, ID_COLUMNS_FIRST+i+p_id_base, pfc::stringcvt::string_os_from_utf8(column_name));		

		i++;
	}

	menu.AppendMenuW(MF_SEPARATOR);
	menu.AppendMenuW(MF_STRING, ID_MORE+p_id_base, _T("More..."));

	menu.AppendMenuW(MF_SEPARATOR);
	UINT relative_checked = settings->m_relative_column_widths ? MF_CHECKED : MF_UNCHECKED;
	menu.AppendMenuW(relative_checked, ID_RELATIVE_WIDTHS+p_id_base, _T("Auto-scale Columns with Window Size"));

}

void CCustomListView::BuildListItemContextMenu(CMenuHandle menu, unsigned p_id_base, CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::BuildListItemContextMenu");
#ifdef _DEBUG
	console::formatter() << "BuildListItemContextMenu";
#endif
	pfc::list_t<t_size> selection;
	GetSelectedIndices(selection);

	pfc::list_t<t_playback_queue_item> queue;
	static_api_ptr_t<playlist_manager>()->queue_get_contents(queue);	

	// Show "Locate in Playlist" only if one item is selected
	bool show_locate_in_playlist = selection.get_count() == 1;
	PFC_ASSERT(selection.get_count() != 1 || selection[0] < queue.get_count());
	bool is_playlist_queue = queue[selection[0]].m_playlist != pfc::infinite_size;
	UINT locate_playlist_item = is_playlist_queue ? MF_STRING : MF_GRAYED;

	AppendShowColumnHeaderMenu(menu, ID_SHOW_COLUMN_HEADER + p_id_base);
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_REMOVE + p_id_base, _T("Remove"));
	menu.AppendMenu(MF_STRING, ID_MOVE_TOP + p_id_base, _T("Move to Top"));
	menu.AppendMenu(MF_STRING, ID_MOVE_UP + p_id_base, _T("Move Up"));
	menu.AppendMenu(MF_STRING, ID_MOVE_DOWN + p_id_base, _T("Move Down"));
	menu.AppendMenu(MF_STRING, ID_MOVE_BOTTOM + p_id_base, _T("Move to Bottom"));
	if(show_locate_in_playlist) {
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(locate_playlist_item, ID_LOCATE_IN_PLAYLIST + p_id_base, _T("Locate in Playlist"));
	}
}

void CCustomListView::BuildListNoItemContextMenu(CMenuHandle menu, unsigned p_id_base, CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::BuildListNoItemContextMenu");
#ifdef _DEBUG
	console::formatter() << "BuildListNoItemContextMenu";
#endif
	AppendShowColumnHeaderMenu(menu, ID_SHOW_COLUMN_HEADER + p_id_base);
}

void CCustomListView::CommandHeaderContextMenu(unsigned p_id, unsigned p_id_base, CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::CommandHeaderContextMenu");
#ifdef _DEBUG
	console::formatter() << "CommandHeaderContextMenu";
#endif
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);

	// Check what command has been chosen. If cmd == 0, then no command
	// was chosen.
	if(p_id == ID_SHOW_COLUMN_HEADER + p_id_base) {
		ShowHideColumnHeader();
	}
	else if (p_id == ID_MORE + p_id_base) {
#ifdef _DEBUG
		console::formatter() << "Context menu: More...";
#endif
		static_api_ptr_t<ui_control>()->show_preferences(guid_queuecontents_preferences);

	} else if(p_id == ID_RELATIVE_WIDTHS + p_id_base) {
		settings->m_relative_column_widths = !settings->m_relative_column_widths;
		// Whether we are enabling or disabling auto-sizing of columns we still
		// need to save current widths and use them for RelayoutUIColumns
		bool settingsWidthOk = UpdateSettingsWidths();
		PFC_ASSERT( settingsWidthOk );
		// Update column sizes to settings etc.
		RelayoutUIColumns();
		// Save configuration
		m_host->save_configuration();

	} else if(p_id >= ID_COLUMNS_FIRST + p_id_base && p_id <= ID_COLUMNS_LAST + p_id_base) {
		long column_id = m_header_context_menu_column_ids[p_id - ID_COLUMNS_FIRST - p_id_base];
		PFC_ASSERT(cfg_ui_columns.exists(column_id));
#ifdef _DEBUG
		console::formatter() << "Context menu: column with index " <<
			" name " << cfg_ui_columns.find(column_id)->m_value.m_name;
#endif


		bool existedPreviously = settings->column_exists(column_id);
		if(existedPreviously) {
			RemoveUIColumn(column_id);
		} else {
			AddUIColumn(column_id);
		}

	}
}

void CCustomListView::CommandListItemContextMenu(unsigned p_id, unsigned p_id_base, CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::CommandListItemContextMenu");
#ifdef _DEBUG
	console::formatter() << "CommandListItemContextMenu";
#endif
	// Check what command has been chosen. If cmd == 0, then no command
	// was chosen.
	if(p_id == ID_SHOW_COLUMN_HEADER + p_id_base) {
		ShowHideColumnHeader();
	} else if (p_id == ID_REMOVE + p_id_base) {
#ifdef _DEBUG
		console::formatter()<< "Context menu: Remove";
#endif
		DeleteSelected();
	} else if(p_id == ID_MOVE_TOP + p_id_base|| p_id == ID_MOVE_BOTTOM + p_id_base ||
		p_id == ID_MOVE_UP + p_id_base || p_id == ID_MOVE_DOWN + p_id_base) {
#ifdef _DEBUG
		console::formatter()<< "Context menu: Move Top";
#endif
		pfc::list_t<t_size> selection;

		// Reordering result of the drag operation
		pfc::list_t<t_size> ordering;		

		// Selection of the dragged items after reorder
		pfc::list_t<t_size> newSelection;

		GetSelectedIndices(selection);

		t_size new_index = 0;
		
		PFC_ASSERT( selection.get_count() > 0 );
		if(p_id == ID_MOVE_TOP + p_id_base) {
			new_index = 0;
			queue_helpers::queue_move_items_reordering(new_index, selection, newSelection, ordering);
		} else if(p_id == ID_MOVE_UP + p_id_base) {
			queue_helpers::move_items_hold_structure_reordering(true, selection, newSelection, ordering);
		} else if(p_id == ID_MOVE_DOWN + p_id_base) {
			queue_helpers::move_items_hold_structure_reordering(false, selection, newSelection, ordering);
		} else if(p_id == ID_MOVE_BOTTOM + p_id_base) {
			new_index = GetItemCount();
			queue_helpers::queue_move_items_reordering(new_index, selection, newSelection, ordering);
		}		

		// When the refresh occurs in the listbox it tries to hold these selections
		SetSelectedIndices(newSelection);

		// Update the queue and let the new order propagate through
		// queue events
		queue_helpers::queue_reorder(ordering);

	} else if(p_id == ID_LOCATE_IN_PLAYLIST + p_id_base) {
		pfc::list_t<t_size> selection;
		GetSelectedIndices(selection);

		pfc::list_t<t_playback_queue_item> queue;
		static_api_ptr_t<playlist_manager>()->queue_get_contents(queue);	

		DEBUG_PRINT << "Locate in playlist command. Selected: " << selection.get_count() <<
			". In queue: " << queue.get_count();
		
		if(selection.get_count() != 1) return; // Shouldn't happen.
			
		DEBUG_PRINT << "First selected (index): " << selection[0];
		
		PFC_ASSERT(selection[0] < queue.get_count());
		t_playback_queue_item queue_item = queue[selection[0]];
		DEBUG_PRINT << "Queue item corresponding the first selected, playlist: " << queue_item.m_playlist << ". item " << queue_item.m_item;
		if(queue_item.m_playlist == pfc::infinite_size || queue_item.m_item == pfc::infinite_size) return;

		DEBUG_PRINT << "Trying to show the playlist";
		static_api_ptr_t<playlist_manager>()->playlist_set_focus_item(queue_item.m_playlist, queue_item.m_item);
		static_api_ptr_t<playlist_manager>()->playlist_ensure_visible(queue_item.m_playlist, queue_item.m_item);
		static_api_ptr_t<playlist_manager>()->set_active_playlist(queue_item.m_playlist);
	}
}

void CCustomListView::CommandListNoItemContextMenu(unsigned p_id, unsigned p_id_base, CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::CommandListNoItemContextMenu");
#ifdef _DEBUG
	console::formatter() << "CommandListNoItemContextMenu";
#endif
	if(p_id == ID_SHOW_COLUMN_HEADER + p_id_base) {
		ShowHideColumnHeader();
	}
}


void CCustomListView::OnContextMenu(CWindow wnd, CPoint p_pt) {
	TRACK_CALL_TEXT("CCustomListView::OnContextMenu");
#ifdef _DEBUG
	console::formatter() << "OnContextMenu. GetCursorPos = " << p_pt;
#endif

	CPoint pt(p_pt);
	CMenu menu;
	menu.CreatePopupMenu();
	unsigned p_base_id = 0;

	// Ripped from SDK/misc.h ui_element_instance_standard_context_menu
	bool fromKeyboard;
	if (p_pt.x == -1 || p_pt.y == -1) {
		fromKeyboard = true;
		DEBUG_PRINT << "Keyboard context menu";
		EditModeContextMenuGetFocusPoint(pt);
	} else {
		fromKeyboard = false;
		pt = p_pt;
	}


	CMenuHandle menuhandle(menu);
	BuildContextMenu(pt, menuhandle, p_base_id);

	unsigned p_id = TrackPopupMenu(menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON, 
		pt.x, pt.y, 0, *this, 0);

#ifdef _DEBUG
	console::formatter() << "p_id " << p_id << ", p_base_id " << p_base_id;
#endif

	CommandContextMenu(pt, p_id, p_base_id);
}

STDMETHODIMP CCustomListView::DetermineDropAction(DWORD grfKeyState, CPoint pt_screen, CPoint pt_client, DWORD *pdwEffect, bool internal_dragging) {
	TRACK_CALL_TEXT("CCustomListView::DetermineDropAction");
	HWND wnd = WindowFromPoint(pt_screen);	

	// pdwEffect must not be null
	if(!pdwEffect) {
		return E_INVALIDARG;
	}

#ifdef _DEBUG
	console::formatter() << "Is internal dragging?" << internal_dragging
		<< ". Copy?" << ((*pdwEffect & DROPEFFECT_COPY) != 0) 
		<< " Move?" << ((*pdwEffect & DROPEFFECT_MOVE) != 0)
		<< " Link?" << ((*pdwEffect & DROPEFFECT_LINK) != 0);

#endif

	bool shiftPressed = (grfKeyState & MK_SHIFT) != 0;
	bool ctrlPressed = (grfKeyState & MK_CONTROL) != 0;

	if(internal_dragging) {
		// we are dropping to this control, support COPY by ctrl+
		// otherwise try to MOVE
		if((*pdwEffect & DROPEFFECT_COPY) && ctrlPressed) {
			*pdwEffect = DROPEFFECT_COPY;
		} else if(*pdwEffect & DROPEFFECT_MOVE) {
			*pdwEffect = DROPEFFECT_MOVE;
		} else if(*pdwEffect & DROPEFFECT_COPY) {
			// Allow copy if nothing else is OK
			*pdwEffect = DROPEFFECT_COPY;
		} else {
			*pdwEffect = DROPEFFECT_NONE;
		}
	} else {
		// If the drop source is not this listbox, behave a bit differenly
		// PREFER COPY (queueing)
		// - Unless shift is pressed and MOVE allowed, which means that user explicitly asks for MOVE
		if((*pdwEffect & DROPEFFECT_COPY) && !shiftPressed) {
			*pdwEffect = DROPEFFECT_COPY;
		}else if(shiftPressed && (*pdwEffect & DROPEFFECT_MOVE)) {
			*pdwEffect = DROPEFFECT_MOVE;
		}else if(*pdwEffect & DROPEFFECT_COPY) {
			*pdwEffect = DROPEFFECT_COPY;
		} else if(*pdwEffect & DROPEFFECT_MOVE) {
			*pdwEffect = DROPEFFECT_MOVE;
		} else {
			*pdwEffect = DROPEFFECT_NONE;
		}

	}	

	return S_OK;

}

STDMETHODIMP CCustomListView::DragEnter(DWORD grfKeyState, CPoint pt_screen, CPoint pt_client, DWORD *pdwEffect) {
	TRACK_CALL_TEXT("CCustomListView::DragEnter");
#ifdef _DEBUG
	console::formatter() << "DragEnter" ;
	console::formatter() << "Queue size:" << static_api_ptr_t<playlist_manager>()->queue_get_count();
#endif
	HRESULT result = DetermineDropAction(grfKeyState, pt_screen, pt_client, pdwEffect, m_internal_dragging);	

	int index = DetermineItemInsertionIndex(pt_client);
	if(index != -1) {
		EnsureVisible(index, true);
		UpdateInsertMarker(index);
	}

	return result;
}

STDMETHODIMP CCustomListView::DragLeave() {
	TRACK_CALL_TEXT("CCustomListView::DragLeave");
	// clear out insertion marker
	UpdateInsertMarker(-1);
	DEBUG_PRINT << "DragLeave" ;
	return S_OK;
}

STDMETHODIMP CCustomListView::DragOver(DWORD grfKeyState, CPoint pt_screen, CPoint pt_client, DWORD *pdwEffect) {
	TRACK_CALL_TEXT("CCustomListView::DragOver");
#ifdef _DEBUG
	console::formatter() << "DragOver" ;
	console::formatter() << "Queue size:" << static_api_ptr_t<playlist_manager>()->queue_get_count();
#endif
	// Do exactly the same as in DragEnter
	HRESULT result = DetermineDropAction(grfKeyState, pt_screen, pt_client, pdwEffect, m_internal_dragging);
	int index = DetermineItemInsertionIndex(pt_client);
#ifdef _DEBUG
	console::formatter() << "DragOver, hit index " << index;
#endif
	if(index != -1) {
		EnsureVisible(index, true);
		UpdateInsertMarker(index);
	}

	return result;

}

STDMETHODIMP CCustomListView::Drop(const pfc::list_base_const_t<metadb_handle_ptr>& handles, 
	DWORD grfKeyState, CPoint pt_screen, CPoint pt_client, DWORD *pdwEffect, 
	const pfc::list_base_const_t<t_size>& selection, bool internal_dragging, const ActivePlaylistInformation & active_playlist_information) {

		TRACK_CALL_TEXT("CCustomListView::Drop");
		DEBUG_PRINT << "Drop" ;
		HRESULT action = DetermineDropAction(grfKeyState, pt_screen, pt_client, pdwEffect, internal_dragging);
		if(action == E_UNEXPECTED) {
			return action;
		}

		bool from_playlist = (active_playlist_information.active_playlist_selected_items.get_count() != 0 && active_playlist_information.active_playlist_index != pfc::infinite_size &&
			pfc::list_base_const_t<metadb_handle_ptr>::g_equals(handles, active_playlist_information.active_playlist_selected_items));	

		if(from_playlist) {
			// Since this drop function can be called after the actual drop moment has gone
			// we make sure that the element are still there in the playlist
			static_api_ptr_t<playlist_manager> playlist_api;
			pfc::list_t<metadb_handle_ptr> tmp;
			playlist_api->playlist_get_items(active_playlist_information.active_playlist_index,
				tmp, active_playlist_information.active_playlist_selection_mask);

			from_playlist = pfc::list_base_const_t<metadb_handle_ptr>::g_equals(tmp, active_playlist_information.active_playlist_selected_items);
		}	

		bool from_queue_playlist = ( from_playlist && cfg_playlist_enabled
			&& ( queuecontents_lock::find_playlist() == active_playlist_information.active_playlist_index ) );

		DEBUG_PRINT << "Does drag correspond to active playlist?" << from_playlist;
		DEBUG_PRINT << "Does drag correspond to queue playlist?" << from_queue_playlist;


#ifdef _DEBUG
		DEBUG_PRINT << "Queue size:" << static_api_ptr_t<playlist_manager>()->queue_get_count();		
#endif

		int drag_index = DetermineItemInsertionIndex(pt_client);

		if(*pdwEffect & DROPEFFECT_COPY) {
			//*pdwEffect = DROPEFFECT_COPY;
			// Get the drop index and add there some items	
#ifdef _DEBUG
			console::formatter() << "Drop, COPY";
			if(handles.get_count()) {
				console::formatter() << "first item:" << handles.get_item(0)->get_path();
			}
#endif
			if(internal_dragging) {
				// To preserve playlist information in the queue handles we "duplicate" entries to correct places
				queue_helpers::queue_duplicate_items(drag_index, selection);
			} else {
				DEBUG_PRINT << "MOVE + NOT internal drag -> queue_insert";
				// If we have drop from playlist & it's not the queue playlist
				if(from_queue_playlist) {
					DEBUG_PRINT << "\"Duplicating\" queue items";
					// We "duplicate" the queue items (holding reference to the original playlists) corresponding to dropped items
					queue_helpers::queue_duplicate_items(drag_index, 
						active_playlist_information.active_playlist_selection_mask, active_playlist_information.active_playlist_item_count);
				} else if(from_playlist) {
					DEBUG_PRINT << "Queueing playlist items";
					// We queue the playlist items using their position in the playlist
					queue_helpers::queue_insert_items(drag_index, active_playlist_information.active_playlist_index, active_playlist_information.active_playlist_selection_mask, 
						active_playlist_information.active_playlist_item_count);
				} else {
					DEBUG_PRINT << "Queueing handles";
					// We queue the items using handles
					queue_helpers::queue_insert_items(drag_index, handles);
				}
			}


		} else if(*pdwEffect & DROPEFFECT_MOVE) {
			//*pdwEffect = DROPEFFECT_MOVE;
			if(internal_dragging) {
				// Moving inside this control corresponds to queue reordering


				// Do drag & drop with the selected item
				//

				// Reordering result of the drag operation
				pfc::list_t<t_size> ordering;		

				// Selection of the dragged items after reorder
				pfc::list_t<t_size> newSelection;


				queue_helpers::queue_move_items_reordering(drag_index, selection, newSelection, ordering);

#ifdef _DEBUG
				console::formatter() << "Drop, internal MOVE";
				console::formatter() << "drop index "<< drag_index;

				console::formatter() << "Queue size:" << static_api_ptr_t<playlist_manager>()->queue_get_count();
				console::formatter() << "handles size "<< handles.get_count();
				console::formatter() << "selected count "<< selection.get_count();

				console::formatter() << "Selection as follows:";
				for(t_size i = 0; i < selection.get_count(); i++) {
					console::formatter() << selection[i];
				}
				console::formatter() << "Ordering items as follows:";
				for(t_size i = 0; i < ordering.get_count(); i++) {
					console::formatter() << ordering[i];
				}
#endif

				// When the refresh occurs in the listbox it tries to hold these selections
				SetSelectedIndices(newSelection);

				if(order_helper::g_is_identity(ordering)) {
					Invalidate(); // to remove insertmarker
				}else {
					// Update the queue and let the new order propagate through
					// queue events
					queue_helpers::queue_reorder(ordering);
				}

			} else { // not internal dragging
				DEBUG_PRINT << "MOVE + !internal drag -> queue_insert";
				// Drop from outside
				// Corresponds to adding stuff to queue

				// If we have drop from playlist & it's not the queue playlist
				if(from_queue_playlist) {
					DEBUG_PRINT << "\"Duplicating\" queue items";
					// We "duplicate" the queue items (holding reference to the original playlists) corresponding to dropped items
					queue_helpers::queue_duplicate_items(drag_index, 
						active_playlist_information.active_playlist_selection_mask, active_playlist_information.active_playlist_item_count);
				} else if(from_playlist) {
					DEBUG_PRINT << "Queueing playlist items";
					// We queue the playlist items using their position in the playlist
					queue_helpers::queue_insert_items(drag_index, active_playlist_information.active_playlist_index, active_playlist_information.active_playlist_selection_mask, 
						active_playlist_information.active_playlist_item_count);
				} else {
					DEBUG_PRINT << "Queueing handles";
					// We queue the items using handles
					queue_helpers::queue_insert_items(drag_index, handles);
				}
		}



}
	else {
		*pdwEffect = DROPEFFECT_NONE;
	}

	SetInternalDragging(false);

	return S_OK;
}

void CCustomListView::QueueRefresh() {
	TRACK_CALL_TEXT("CCustomListView::QueueRefresh");
	pfc::list_t<t_playback_queue_item> queue;
	pfc::list_t<t_size> selection;
	static_api_ptr_t<playlist_manager>()->queue_get_contents(queue);

	// Save selection
	GetSelectedIndices(selection);

	// Save focus
	m_focused_item = GetNextItem(-1, LVNI_FOCUSED);

	m_metadbs.remove_all();
	DeleteAllItems();

	queue_helpers::extract_metadb_handles(queue, m_metadbs);

	AddItems(queue);

	// Try to restore selection
	SetSelectedIndices(selection);

	// Try to restore focus marker
	BOOL ret = SetItemState(min(GetItemCount() - 1, m_focused_item), LVIS_FOCUSED, LVIS_FOCUSED);
	DEBUG_PRINT << "SetItemState(focused) successful: " << ret << ". Focus index was: " << m_focused_item;
	ShowFocusRectangle();

	// Update m_selection
	GetSelectedIndices(m_selection);

}

void CCustomListView::AddItems(pfc::list_t<t_playback_queue_item> items, t_size column_order) {
	TRACK_CALL_TEXT("CCustomListView::AddItems - column overload");
	t_size count = items.get_count();
	int column_index = GetHeader().OrderToIndex(column_order);
	for(t_size i = 0; i < count; i++) {		
		AddItem(items[i], i+1, column_index); // Items must be passed in index order!
	}
}

void CCustomListView::AddItems(pfc::list_t<t_playback_queue_item> items) {
	TRACK_CALL_TEXT("CCustomListView::AddItems");
	t_size count = items.get_count();
	for(t_size i = 0; i < count; i++) {
		AddItem(items[i], i+1);
	}
}


void CCustomListView::AddItem(t_playback_queue_item handle, t_size index) {
	TRACK_CALL_TEXT("CCustomListView::AddItem");
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);


	PFC_ASSERT(GetHeader().GetItemCount() == settings->m_columns.get_count());

	t_size columnCount = settings->m_columns.get_count();

	for(t_size column_index = 0; column_index < columnCount; column_index++) {
		DEBUG_PRINT << "AddItem on row " << index << " in column (index) " << column_index;
		AddItem(handle, index, column_index); // Items must be passed in index order!
	}
}

void CCustomListView::AddItem(t_playback_queue_item handle, t_size index, t_size column_index) {
	TRACK_CALL_TEXT("CCustomListView::AddItem - column overload");
	pfc::string8 itemString;
	service_ptr_t<titleformat_object> format;
	m_hook.setData(index, handle);

	int column_order = IndexToOrder(column_index);

	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);

	long columnId = settings->m_columns[column_order].m_id;

	pfc::map_t<long, ui_column_definition>::const_iterator column_definition = cfg_ui_columns.find(columnId);

	PFC_ASSERT(column_definition.is_valid());
	if(!column_definition.is_valid()) {
		// The column is not valid, remove it from configuration
		settings->m_columns.remove_by_idx(column_order);
	}

	// We use our own format hook for interpreting %queue_index% etc. as it doesn't work default 
	static_api_ptr_t<titleformat_compiler>()->compile( format, column_definition->m_value.m_pattern);	
	handle.m_handle->format_title(&m_hook, itemString, format, NULL);

	DEBUG_PRINT << "CListViewCtrl::AddItem on row " << (index-1) << " in column (index) " << column_index;
	CListViewCtrl::AddItem(index-1, column_index, pfc::stringcvt::string_os_from_utf8(itemString));

	format.release();
}



UINT CCustomListView::OnGetDlgCode(LPMSG lpMsg) {
	TRACK_CALL_TEXT("CCustomListView::OnGetDlgCode");
#ifdef _DEBUG
	//console::formatter() << "CCustomListView OnGetDlgCode";
#endif	
	UINT result = 0;
	// Ensure we get all key down messages
	if(lpMsg != NULL && lpMsg->message == WM_KEYDOWN) {
		result = DLGC_WANTALLKEYS;
#ifdef _DEBUG
		//console::formatter() << "CCustomListView OnGetDlgCode DLGC_WANTALLKEYS";
#endif
	} else {
		SetMsgHandled(FALSE);
	}	

	return result;
}

void CCustomListView::OnCaptureChanged(CWindow /*wnd*/){ 
	TRACK_CALL_TEXT("CCustomListView::OnCaptureChanged");
	// If for some reason we lose mouse capture, we ensure that
	// that dragging is canceled
	//if(m_dragging) {
	//	EndDrag();
	//}
	//m_shared_selection.release();
}

void CCustomListView::SetColors(COLORREF backroundcolor, COLORREF selectioncolor,
	COLORREF textcolor, COLORREF insertmarkercolor, COLORREF selectionrectanglecolor, COLORREF textselectedcolor){

		TRACK_CALL_TEXT("CCustomListView::SetColors");
		m_backgroundcolor = backroundcolor;
		m_selectioncolor = selectioncolor;
		m_textcolor = textcolor;
		m_insertmarkercolor = insertmarkercolor;
		m_selectionrectanglecolor = selectionrectanglecolor;
		m_textselectedcolor = textselectedcolor;

		SetBkColor(m_backgroundcolor);
}


DWORD CCustomListView::OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/)
{   	
	return  CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
}


DWORD CCustomListView::OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw)
{
	TRACK_CALL_TEXT("CCustomListView::OnItemPrePaint");
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>( lpNMCustomDraw );
	bool bHeader;
	// Let's not touch headers
	{
		CRect r(pLVCD->nmcd.rc);
		CRect tmp(r);
		CRect headerRect;
		CRect intersection;
		ClientToScreen(tmp);

		GetHeader().GetClientRect(&headerRect);
		GetHeader().ClientToScreen(&headerRect);
		intersection.IntersectRect(headerRect, tmp);
		bHeader = !intersection.IsRectEmpty();

	}

	if(!bHeader) {		
		DEBUG_PRINT << "OnItemPrePaint, not header";
		if((pLVCD->nmcd.dwItemSpec % 2) == 0) {
			if(pLVCD->nmcd.uItemState & CDIS_SELECTED) {
				pLVCD->clrTextBk = default_ui_hacks::DetermineAlternativeBackgroundColor(m_selectioncolor);
				pLVCD->clrText = m_textselectedcolor;
			} else {
				pLVCD->clrTextBk = default_ui_hacks::DetermineAlternativeBackgroundColor(m_backgroundcolor);
				pLVCD->clrText = m_textcolor;
			}
		} else {
			if( pLVCD->nmcd.uItemState & CDIS_SELECTED) {
				pLVCD->clrTextBk = m_selectioncolor;
				pLVCD->clrText = m_textselectedcolor;
			} else {
				pLVCD->clrTextBk = m_backgroundcolor;
				pLVCD->clrText = m_textcolor;
			}

		}



		m_selected = (pLVCD->nmcd.uItemState & CDIS_SELECTED) != 0;
		m_focused = (pLVCD->nmcd.uItemState & CDIS_FOCUS) != 0;

		// Unselect to remove ugly blue color (we restore it in post-paint)
		pLVCD->nmcd.uItemState &= ~CDIS_SELECTED;
		pLVCD->nmcd.uItemState &= ~CDIS_FOCUS;
	}

	// Tell Windows to paint the control itself.
	return CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
}


DWORD CCustomListView::OnItemPostPaint(int idCtrl, LPNMCUSTOMDRAW lpNMCustomDraw) {
	TRACK_CALL_TEXT("CCustomListView::OnItemPostPaint");
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>( lpNMCustomDraw );


	// Restore selected if it was removed in prepaint stage
	if(m_selected) {
		pLVCD->nmcd.uItemState |= CDIS_SELECTED;
	}

	CDCHandle cdc(pLVCD->nmcd.hdc);
	int old = cdc.SaveDC();
	CRect r(pLVCD->nmcd.rc);
	bool bHeader;

	// Let's not touch headers
	{
		CRect tmp(r);
		CRect headerRect;
		CRect intersection;
		ClientToScreen(tmp);

		GetHeader().GetClientRect(&headerRect);
		GetHeader().ClientToScreen(&headerRect);
		intersection.IntersectRect(headerRect, tmp);
		bHeader = !intersection.IsRectEmpty();

	}

	if(!bHeader) {

		DEBUG_PRINT << "OnItemPostPaint, not header";
		// Fix standard background drawing to fill the little 
		// part in the left
		CRect fill_fix(r);		
		fill_fix.right = fill_fix.left + 4;
		cdc.FillSolidRect(fill_fix, pLVCD->clrTextBk);

		r.DeflateRect(0,0,1,1);

		if(m_focused) {
			pLVCD->nmcd.uItemState |= CDIS_FOCUS;


			if(pLVCD->nmcd.uItemState & CDIS_FOCUS) {
				DEBUG_PRINT << "DRAWING FOCUS RECT";

				// Draw our own focus rectangle which is solid
				CPen pen;
				pen.CreatePen(PS_SOLID, 1, m_selectionrectanglecolor);
				cdc.SelectPen(pen);			
				cdc.MoveTo(r.TopLeft());
				cdc.LineTo(CPoint(r.right, r.top));
				cdc.LineTo(r.BottomRight());
				cdc.LineTo(CPoint(r.left, r.bottom));
				cdc.LineTo(r.TopLeft());
			}

		}




		cdc.RestoreDC(old);

	}

	return CDRF_DODEFAULT;
	//return CDRF_SKIPDEFAULT;
}


void CCustomListView::SetInternalDragging(bool value) {
	m_internal_dragging = value;
}

bool CCustomListView::GetInternalDragging() {
	return m_internal_dragging;
}

template<class t_list>
void CCustomListView::GetSelectedIndices(t_list& p_out) {
	TRACK_CALL_TEXT("CCustomListView::GetSelectedIndices - template");
	p_out.remove_all();
	int pos = GetNextItem(-1, LVNI_SELECTED);
	while(pos >= 0) {						
		t_size index = pfc::downcast_guarded<t_size>(pos);
		p_out.add_item(index);
		pos = GetNextItem(pos, LVNI_SELECTED);
	}	
}

void CCustomListView::GetSelectedIndices(pfc::list_base_t<t_size>& p_out) {
	TRACK_CALL_TEXT("CCustomListView::GetSelectedIndices - list");
	GetSelectedIndices< pfc::list_base_t<t_size> >(p_out);
}


// Used for selection rectangle to select everything between start and end
// start and end can be also -1, and they don't have to be ordered
// if min(start,end)=-1 and max(start,end) != -1, we select only -1
// if max(start,end) != -1, we dont't select anything
// if m_ctrl_selecting seleection status is xorred.
// In addition focuses 'end' index (the second)

void CCustomListView::SelectIndices(int start, int end, bool applyCtrlSelection /*= false*/) {
	TRACK_CALL_TEXT("CCustomListView::SelectIndices");
	// Select everything between start and end
	int start1 = min(start, end);
	int end1 = max(start, end);
	int itemCount = GetItemCount();

	// Unselect everything, unless we ctrl select
	if(!m_ctrl_selecting) {
		DeselectAll();
	}

	// if end1 == -1 it means that start is also smaller than zero...nothing to select
	if(end1 == -1) {
		return;
	}

	// not a typo, we mean the second coordinate
	if(0 <= end && end < itemCount) {
		SetItemState(-1, 0, LVIS_FOCUSED);
		SetItemState(end, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
		PFC_ASSERT(GetItemState(end, LVIS_FOCUSED) == LVIS_FOCUSED);
#ifdef _DEBUG
		console::formatter() << "Focusing " << end;
#endif
	}

	// if start1 = -1 it means that we should only select the end point
	//if(start1 == -1) {
	//	start1 = end1;
	//}


#ifdef _DEBUG
	console::formatter() << "Selecting " << start << "..." << end;
#endif

	// If we are ctrl+selecting we should restore orig. selections...
	// We don't want do this if shift is also pressed (union selection)
	if(m_ctrl_selecting && !m_shift_selecting) {
		for(int index_int = 0; index_int < start1; index_int++) {
			t_size index = pfc::downcast_guarded<t_size>(index_int);
			bool originallySelected = m_selection.contains(index);
			UINT state = originallySelected ? LVIS_SELECTED : ~LVIS_SELECTED;
			SetItemState(index, state, LVIS_SELECTED);
		}
		for(int index_int = end1+1; index_int < itemCount; index_int++) {
			t_size index = pfc::downcast_guarded<t_size>(index_int);
			bool originallySelected = m_selection.contains(index);
			UINT state = originallySelected ? LVIS_SELECTED : ~LVIS_SELECTED;
			SetItemState(index, state, LVIS_SELECTED);
		}
	}


	for(int index_int = start1; index_int <= end1; index_int++) {
		if(index_int < 0 || index_int >= itemCount) continue;

		t_size index = index_int;

		if(m_ctrl_selecting && !m_shift_selecting) {
			// If ctrl is pressed while selected, we use 'inversion-selection', every originally unselected item is selected
			// and vice versa
			bool originallySelected = m_selection.contains(index);
#ifdef _DEBUG
			console::formatter() << "Original selection state?" << originallySelected;
#endif
			UINT newState = originallySelected ? ~LVIS_SELECTED : LVIS_SELECTED;

			SetItemState(index, newState, LVIS_SELECTED);
			if(applyCtrlSelection) {
				if(!originallySelected) {
					m_selection.add_item(index);
				} else {
					m_selection.remove_item(index);
				}
			}
		} else {
			m_selection.insert(index);
			SetItemState(index, LVIS_SELECTED, LVIS_SELECTED);
		}
#ifdef _DEBUG
		console::formatter() << "Selecting " << index << ". Ctrl?" << m_ctrl_selecting << ". Shift?" << m_shift_selecting;
		//selected_handles.add_item(queue[index].m_handle);
#endif

	}

	// Inform of the current selection to foobar
	metadb_handle_list currently_selected_metadbs;
	GetSelectedItemsMetaDbHandles(currently_selected_metadbs);
	SetSharedSelection(currently_selected_metadbs);
}

void CCustomListView::SetSelectedIndices(const pfc::list_base_const_t<t_size>& p_in) {	
	TRACK_CALL_TEXT("CCustomListView::SetSelectedIndices");
	t_size itemCount = GetItemCount();
	t_size indicesCount = p_in.get_count();
	static_api_ptr_t<playlist_manager> playlist_api;
	pfc::list_t<t_playback_queue_item> queue;
	metadb_handle_list selected_handles;
	playlist_api->queue_get_contents(queue);


	// Unselect everything
	SetItemState(-1, 0, LVIS_SELECTED);

	for(t_size i = 0; i < indicesCount; i++) {
		t_size index = p_in[i];
		if(index >= 0 && index < itemCount) {
			SetItemState(index, LVIS_SELECTED, LVIS_SELECTED);
			selected_handles.add_item(queue[index].m_handle);
#ifdef _DEBUG
			console::formatter() << "Selecting " << index;			
#endif
		}
	}


	// Inform other components of the selection
	SetSharedSelection(selected_handles);
}


// We check what index corresponds to the mouse cursor
// Even situation where mouse cursor is outside the window are handled properly
int CCustomListView::DetermineItemSelectionIndex(CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::DetermineItemSelectionIndex");

	UINT flags=0;
#ifdef _DEBUG
	console::formatter() << "pt " << point.x << "," << point.y;
#endif
	int index = HitTest( point, &flags );	
	// No valid item found? Perhaps the mouse is outside the list
	if(index == -1)
	{
		CRect r;
		GetClientRect(&r);
		// Since LVHT_TOLEFT and LVHT_TORIGHT doesn't work we use little workaround...
		bool onLeft = r.right > point.x && r.bottom > point.y && r.top < point.y;
		bool onRight = r.left < point.x && r.bottom > point.y && r.top < point.y;
		int top = GetTopIndex();
		// last might be > itemcount but it doesn't matter
		// since we handle this properly in other functions
		int	last = top + GetCountPerPage();		

		if(index == -1 && (onLeft || onRight)) {
			LONG x_test = (r.left+1);
			UINT flags2;
			int index2 = HitTest( CPoint(x_test, point.y), &flags2 );
#ifdef _DEBUG
			console::formatter() << "Mouse on the sides. Trying to check the index: " << index2;
			console::formatter() << "Client l,r=" << r.left << "," << r.right;
#endif
			if(index2 != -1) {
				index = index2;
			}		
		}

		if(index == -1) {
			// Mouse is under the listview, so pretend it's over the last item
			// in view
			if(flags & LVHT_BELOW) {
				// The position is below the control's client area.
				index = last + 1;
			} else if(flags & LVHT_ABOVE) {
				// Mouse is above the listview, so pretend it's over the top item in
				// view - 1
				index = top - 1;
			}  else if(flags & LVHT_NOWHERE) {			
				if(r.top > point.y) {//TODO IS TOP THE CORRECT CHOISE?
					// mouse is top of the first item
					index = top - 1;
				} else {
					// The position is inside the list-view
					// control's client window, but it is not over a list item.
					// It is not above the first item.
					// Thus, it must mean that we are inserting to last position.
					index = last + 1;
				}
			}	
		}

		index = min(index, GetItemCount());

#ifdef _DEBUG
		console::formatter() << "top:" << top << " last:" << last << " below:"<< ((flags&LVHT_BELOW)!=0) << " above:"<< ((flags&LVHT_ABOVE)!=0)
			<< " left:"<< onLeft  << " right:"<< onRight << " nowhere:" << 
			((flags & LVHT_NOWHERE)!=0) << " top2 " << (r.bottom > point.y) << " bottom2 " << (!(r.bottom > point.y));
		console::formatter() << "result " << index;
#endif

	}

	return index;
}

// returns index of insertion point 0... item count
int CCustomListView::DetermineItemInsertionIndex(CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::DetermineItemInsertionIndex");
	UINT flags=0;
#ifdef _DEBUG
	console::formatter() << "pt " << point.x << "," << point.y;
#endif
	int index = HitTest( point, &flags );	
	// No valid item found? Perhaps the mouse is outside the list
	if(index == -1)
	{
		int top = GetTopIndex();
		// last might be > itemcount but it doesn't matter
		// since we handle this properly in other functions
		int	last = top + GetCountPerPage();
		// Mouse is under the listview, so pretend it's over the last item
		// in view
#ifdef _DEBUG
		//console::formatter() << "top:" << top << " last:" << last << " below:"<< ((flags&LVHT_BELOW)!=0) << " above:"<< ((flags&LVHT_ABOVE)!=0);
#endif
		if(flags & LVHT_BELOW) {
			// The position is below the control's client area.
			index = last + 1;
		} else if(flags & LVHT_ABOVE) {
			// Mouse is above the listview, so pretend it's over the top item in
			// view - 1
			index = top - 1;
		} else if(flags & LVHT_NOWHERE) {
			CRect r;
			GetItemRect(top, &r, LVIR_BOUNDS);

			if(r.bottom > point.y) {
				// mouse is top of the first item
				index = top - 1;
			} else {
				// The position is inside the list-view
				// control's client window, but it is not over a list item.
				// It is not above the first item.
				// Thus, it must mean that we are inserting to last position.
				index = last + 1;
			}
		}

	}

	return min(index, GetItemCount());
}

// Following copied and transformed to WTL from http://www.codeproject.com/KB/list/Dragger.aspx
void CCustomListView::OnMouseMove(UINT nFlags, CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::OnMouseMove");
	if(m_selecting && !m_shift_selecting) {
		// Update ctrl key
		m_ctrl_selecting = (GetKeyState(VK_CONTROL) & 0x8000) != 0;


#ifdef _DEBUG
		console::formatter() << "OnMouseMove, selecting: " << m_selecting << "(x,y)=(" << point.x << "," << point.y << ")";
#endif
		UpdateSelection(point);
	}


}

// Remember that this might be called before Drop method so
// be careful if you are doing anything drop related
void CCustomListView::OnLButtonUp(UINT nFlags, CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::OnLButtonUp");
	m_ctrl_selecting = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

	if(m_selecting) {

		CRect rect(0,0,0,0);
		CClientDC dc(*this);		

		//dc.DrawDragRect(&rect, CSize(1,1), m_selection_lastRect, CSize(1,1));	
		DrawSelectionRectangle(dc, rect, m_selection_lastRect);

		ReleaseCapture();
		DEBUG_PRINT << "OnLButtonUp, reset selecting." ;

	}
	//if(m_dragging) {
	//	EndDrag();
	// Do after-operations
	//	OnDragEnded();
	//}
	//SetInternalDragging(false);
	m_selecting = false;
	m_ctrl_selecting = false;
	m_shift_selecting = false;
}

void CCustomListView::OnLButtonDown(UINT nFlags, CPoint point) {
	TRACK_CALL_TEXT("CCustomListView::OnLButtonDown");
	int index = DetermineItemSelectionIndex(point);

	SetFocus();
	ShowFocusRectangle();

	GetSelectedIndices(m_selection);

	bool hit_selected = GetItemState(index, LVIS_SELECTED) != 0;
	m_shift_selecting =  (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	m_ctrl_selecting = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

#ifdef _DEBUG
	console::formatter() << "Selected Items:";
	for(pfc::avltree_t<t_size>::const_iterator iter = m_selection.first(); iter != m_selection.last(); iter++) {
		console::formatter() << *iter;
	}
#endif


	// If user hits non-selected item it will mean that we are starting a selection, not a drag
	// this emulates foobar playlist behaviour
	// Selection will also start if ctrl is pressed but then items are "selection xorred"
	// (already selected items will come unselected etc.)
	if(!hit_selected || m_ctrl_selecting || m_shift_selecting) {
		int itemCount = GetItemCount();
		// we are not dragging.
		SetInternalDragging(false);
		// Start selecting when we click non-selected item
		m_selecting = true;

		m_selection_start_index = index;
		m_selection_start_point = point;
		CRect rect(point, point);

		// Get the mouse capture on list control
		SetCapture();

		// Capture key downs?
		SetFocus();

		// If shift was pressed and we clicked an item
		// override 
		if(m_shift_selecting) {
#ifdef _DEBUG
			console::formatter() << "Shift select processed";
#endif

			if(m_shift_selection_start_index < 0) 
				m_shift_selection_start_index = index;
			// We don't want to mess up shift selection if index is out of bounds
			if(index != itemCount) {
				// Select m_shift_selection_start_index...index, and put focus to index
				SelectIndices(m_shift_selection_start_index, index);
			}

		} else {
			// Reset shift selection start index (marking the start of 'shift area')
			m_shift_selection_start_index = index;
			// No shift selection detected -> Draw selection rectangle and
			// select corresponding items
#ifdef _DEBUG
			console::formatter() << "Starting selection from " << index << ", capturing mouse";
#endif
			CClientDC dc(*this);		
			//dc.GetCurrentBrush()
			SelectIndices(m_selection_start_index, m_selection_start_index);
			//CBrush brush;
			//brush.CreateSolidBrush(m_textcolor);
			//dc.DrawDragRect(&rect, CSize(1,1), NULL, CSize(1,1));	
			DrawSelectionRectangle(dc, rect, NULL);

			m_selection_lastRect = rect;
		}

		return;
	}

	// We clicked already-selected item, let's do drag detection
	if (DragDetect(*this, point)) {
		// do drag test if NOT selected
		// and call DoDragDrop

		// Get the mouse capture on list control
		SetCapture();

		// Capture key downs?
		SetFocus();

		// Create an IDataObject that contains the selected tracks.
		static_api_ptr_t<playlist_incoming_item_filter> piif;

		// create_dataobject_ex() returns a smart pointer unlike create_dataobject()
		// which returns a raw COM pointer. The less chance we have to accidentally
		// get the reference counting wrong, the better.
		metadb_handle_list handles;
		GetSelectedItemsMetaDbHandles(handles);
		pfc::com_ptr_t<IDataObject> pDataObject = piif->create_dataobject_ex(handles);

		// Create an IDropSource.
		// The constructor of DropSourceImpl is hidden by design; we use the
		// provided factory method which returns a smart pointer.
		pfc::com_ptr_t<IDropSource> pDropSource = DropSourceImpl::g_create(m_hWnd, this);

		DWORD effect;

		// We assume we are dragging the object to this control
		// (the mouse is positioned inside this control
		// at the start so it's a valid assumption). Internal dragging
		// flag is updated during the drag by the DragSourceNotifyImpl behind the scenes.
		SetInternalDragging(true);
		// Perform the drag&drop operation.
		DoDragDrop(pDataObject.get_ptr(), pDropSource.get_ptr(), DROPEFFECT_COPY | DROPEFFECT_MOVE, &effect);		

		// If we MOVED items OUTSIDE this control
		// Note: it he drag is canceled, effect would be DROPEFFECT_NONE
		if(effect == DROPEFFECT_MOVE && !m_internal_dragging) {
			DeleteSelected();			
		}

	} else {
#ifdef _DEBUG
		console::formatter() << "Clicked selected item and no drag drop was detected";
#endif
		SetSelectedIndices(pfc::list_single_ref_t<t_size>(index));
	}
	// if it selected selection rentagle should appear if possible
}

// See http://www.vbforums.com/showthread.php?t=563077
void CCustomListView::ShowFocusRectangle() {
	::SendMessage(*this, WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);
}

#ifdef _DEBUG

void CCustomListView::_print_header_debug_info() {
	int columnCount = GetHeader().GetItemCount();
	pfc::array_staticsize_t<int> orderarray(columnCount);
	GetHeader().GetOrderArray(columnCount, &orderarray[0]);
	DEBUG_PRINT << "== ORDERARRAY ==";
	for(int w = 0; w < columnCount; w++) {
		CRect r;
		int index = GetHeader().OrderToIndex(w);
		GetHeader().GetItemRect(index, &r);
		DEBUG_PRINT << "orderArray[order=" << w << "] = index " << orderarray[w] << " = " << index << ". x=" << r.left;
	}
	DEBUG_PRINT << "== /ORDERARRAY ==";

	DEBUG_PRINT << "== SETTINGS ==";
	PFC_ASSERT(m_host != NULL);	
	ui_element_settings* settings;
	m_host->get_configuration(&settings);
	for(t_size w = 0; w < settings->m_columns.get_count(); w++) {
		long column_id = settings->m_columns[w].m_id;
		PFC_ASSERT(cfg_ui_columns.exists(column_id));
		pfc::map_t<long, ui_column_definition>::const_iterator column_settings = cfg_ui_columns.find(column_id);		
		DEBUG_PRINT << "cfg_ui_columns[order=" << w << "] = id " << column_id << ". Name: " << column_settings->m_value.m_name; 
	}
	
	DEBUG_PRINT << "== /SETTINGS ==";
}
#else
void CCustomListView::_print_header_debug_info(CCustomListView & list) {}
#endif


void CCustomListView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {	
	TRACK_CALL_TEXT("CCustomListView::OnKeyDown");
	char character = static_cast<char>(nChar);
	bool controlPressed = ((GetKeyState(VK_CONTROL) & 0x8000) != 0);
	bool altPressed = ((GetKeyState(VK_MENU) & 0x8000) != 0);
	bool shiftPressed = ((GetKeyState(VK_SHIFT) & 0x8000) != 0);
	bool isA =  (character == 'a' || character == 'A');
	bool isDown = ((GetKeyState(VK_DOWN) & 0x8000) != 0);
	bool isUp = ((GetKeyState(VK_UP) & 0x8000) != 0);
	bool isPgUp = ((GetKeyState(VK_PRIOR) & 0x8000) != 0);
	bool isPgDown = ((GetKeyState(VK_NEXT) & 0x8000) != 0);
	int direction = (isUp || isPgUp) ? -1 : 1;
	bool page = isPgUp || isPgDown;

#ifdef _DEBUG
	bool isF5Down = ((GetKeyState(VK_F5) & 0x8000) != 0);
	bool isF6Down = ((GetKeyState(VK_F6) & 0x8000) != 0);
	bool isF7Down = ((GetKeyState(VK_F7) & 0x8000) != 0);
	bool isF8Down = ((GetKeyState(VK_F8) & 0x8000) != 0);
	bool isF9Down = ((GetKeyState(VK_F9) & 0x8000) != 0);
	bool isDbgCommand = isF5Down ||isF6Down || isF7Down || isF8Down || isF9Down;
#endif

	SetFocus();
	ShowFocusRectangle();

	DEBUG_PRINT << "Down?" << isDown << ".Up?" << isUp << ".PgDown?" << isPgDown
		<< ".PgUp?" << isPgUp << ".Ctrl?" << controlPressed
		<< ".Alt?" << altPressed << ".Shift?" << shiftPressed;

	if(isA && controlPressed && !altPressed && !shiftPressed) {
		// CTRL+A
		DEBUG_PRINT << "Selecting everything";
		SelectAll();
	} else if(nChar == VK_DELETE) {
		DEBUG_PRINT << "Deleting Selected";
		DeleteSelected();
	} else if(controlPressed && shiftPressed && (isDown || isUp)) {
		DEBUG_PRINT << "MoveSelectedItems"; 
		MoveSelectedItems(isUp);
	} else if(controlPressed && shiftPressed && (isPgDown || isPgUp)) {
		DEBUG_PRINT << "MoveFocusRectangle, PgUp/PgDown"; 
		// Default actions will do just fine
		SetMsgHandled(FALSE);
	} else if(!shiftPressed  && controlPressed && (isDown || isUp)) {
		DEBUG_PRINT << "MoveFocusRectangle, Up/Down"; 
		DEBUG_PRINT << "Listview doesn't know what to do with this OnKeyDown => SetMsgHandled(FALSE)";
		SetMsgHandled(FALSE);
	} else if(!controlPressed && shiftPressed && (isDown || isUp || isPgDown || isPgUp)) {
		DEBUG_PRINT << "SelectRelativeToCurrentlyFocusedItem, Shift"; 						
		DEBUG_PRINT << "SelectRelativeToCurrentlyFocusedItem, Direction " << direction; 
		SelectRelativeToCurrentlyFocusedItem(direction, page, true);
	} else if(!controlPressed && !shiftPressed && (isDown || isUp || isPgDown || isPgUp)) {	
		DEBUG_PRINT << "SelectRelativeToCurrentlyFocusedItem, No Shift"; 
		SelectRelativeToCurrentlyFocusedItem(direction, page, false);
	} else if(nChar == VK_ESCAPE) {
		DEBUG_PRINT << "Escape pressed. Closing the window!";
		if(m_host->is_popup()) {
			m_host->close();
		} else {
			DEBUG_PRINT << "Window closing cancelled (embedded element).";
		}
#ifdef _DEBUG
	} else if(isDbgCommand) {
		DEBUG_PRINT << "DEBUG-KEY-COMMANDS: F5 (print col info), F6 (add item to subitem0), F7 (add item to subitem 1), F8 (add item to subitem2), F9 (clear all items)";
		pfc::list_t<t_playback_queue_item> items;
		metadb_handle_list some_meta_handles;
		static_api_ptr_t<playlist_manager>()->activeplaylist_get_items(some_meta_handles, bit_array_range(0, 1));
		t_playback_queue_item queue_item;
		queue_item.m_handle = some_meta_handles[0];
		items.add_item(queue_item);

		if(isF5Down) {
			_print_header_debug_info();
		} else if (isF6Down) {			
			AddItems(items, 0);			
		} else if(isF7Down) {
			AddItems(items, 1);

		} else if(isF8Down) {
			AddItems(items, 2);
			
		} else if(isF9Down) {
			SelectAll();
			DeleteSelected();
		}
#endif
	} else {
		DEBUG_PRINT << "Listview doesn't know what to do with this OnKeyDown => SetMsgHandled(FALSE)";
		// We don't know what to do about it so let's someone else handle it
		SetMsgHandled(FALSE);
	}

	if(!IsMsgHandled()) {
		// Fix for uie_element. For some reason this is not needed in dui_elment
		// WPARAM = nChar when looking at macro crackers
		if(static_api_ptr_t<keyboard_shortcut_manager>()->on_keydown_auto(nChar)) {
			DEBUG_PRINT << "Listview keydown event passed to keyboard_shortcut_manager.";
			SetMsgHandled(TRUE);
		} else {
			DEBUG_PRINT << "Listview keydown event passed to keyboard_shortcut_manager but it rejected it. Trying default handler.";
		}
	}
}

void CCustomListView::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
	TRACK_CALL_TEXT("CCustomListView::OnSysKeyDown");
	// We are not interested in WM_SYSKEYDOWN but we want to pass it to foobar to handle
	// e.g. ctrl + a  -> onsyskeydown (always on top by default)

	if(static_api_ptr_t<keyboard_shortcut_manager>()->on_keydown_auto(nChar)) {
		DEBUG_PRINT << "Listview syskeydown event passed to keyboard_shortcut_manager.";
		SetMsgHandled(TRUE);
	} else {
		DEBUG_PRINT << "Listview syskeydown event passed to keyboard_shortcut_manager but it rejected it.";
		SetMsgHandled(FALSE);
	}
}

void CCustomListView::MoveSelectedItems(bool up){
	TRACK_CALL_TEXT("CCustomListView::MoveSelectedItems");
	pfc::list_t<t_size> selected;
	GetSelectedIndices(selected);

	pfc::list_t<t_size> ordering;
	pfc::list_t<t_size> new_selection;

	queue_helpers::move_items_hold_structure_reordering(up, selected, new_selection, ordering);
	// When the refresh occurs in the listbox it tries to hold these selections
	SetSelectedIndices(new_selection);

	// Update the queue and let the new order propagate through
	// queue events
	queue_helpers::queue_reorder(ordering);
}

// direction = 0 <-> focused, direction < 0 upper, direction > 0 lower
void CCustomListView::SelectRelativeToCurrentlyFocusedItem(int direction, bool page, bool unionselection) {
	TRACK_CALL_TEXT("CCustomListView::SelectRelativeToCurrentlyFocusedItem");
	int focused = GetNextItem(-1, LVNI_FOCUSED);
	int countperpage = GetCountPerPage();

	if(focused < 0) {
		DEBUG_PRINT << "SelectRelativeToCurrentlyFocusedItem. No focused item";
		focused = 0;
	}
	DEBUG_PRINT << "SelectRelativeToCurrentlyFocusedItem. Focus index " << focused;

	if(direction == 0) {
		SetSelectedIndices(pfc::list_single_ref_t<t_size>(focused));
		WIN32_OP_D( EnsureVisible(focused, false) );
		return;
	}

	if(m_shift_selection_start_index < 0)
		m_shift_selection_start_index = focused;

	bool up = direction < 0;
	DEBUG_PRINT << "SelectRelativeToCurrentlyFocusedItem.Up?" << up;
	int increment = page ? countperpage : 1;
	t_size endindex;
	if(up) {
		endindex = pfc::max_t<int>(0, focused-increment);
	} else {
		endindex = pfc::min_t<t_size>(GetItemCount()-1, focused+increment);		
	}

	if(!unionselection)
		m_shift_selection_start_index = endindex;

	m_shift_selecting = unionselection;
	SelectIndices(m_shift_selection_start_index, endindex);
	m_shift_selecting = false;
	WIN32_OP_D( SetItemState(endindex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED) );

	EnsureVisible(endindex, false);
}

void CCustomListView::SelectAll() {	
	TRACK_CALL_TEXT("CCustomListView::SelectAll");
	bool ret = SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED) != 0;
	t_size itemcount = GetItemCount(); 
	for(t_size i = 0; i < itemcount;i++) 
		m_selection.insert(i);
	//TODO: update shared selection

#ifdef _DEBUG
	console::formatter() << "Select all successfull:" << ret;
#endif
}

void CCustomListView::DeselectAll() {	
	TRACK_CALL_TEXT("CCustomListView::DeselectAll");
	m_selection.remove_all();
	BOOL ret = SetItemState(-1, 0, LVIS_SELECTED);		
	//TODO: update shared selection

#ifdef _DEBUG
	console::formatter() << "Deselect all successfull:" << ret;
#endif
}

// Deletes selected items from the queue.
// The deletion operation will propagate through queue notifications
void CCustomListView::DeleteSelected() {
	TRACK_CALL_TEXT("CCustomListView::DeleteSelected");
	bit_array_bittable selected(GetItemCount());
	UINT i = 0;

	// Add all selected items to this array
	int selectedCount = 0;
	int pos = GetNextItem(-1, LVNI_ALL | LVNI_SELECTED);
	int focus_before_delete = GetNextItem(-1, LVNI_FOCUSED);
	int firstSelected = pos;
	while(pos >= 0) {
		// Calculate focused item after delete
		if(pos < focus_before_delete && focus_before_delete != -1) {
			focus_before_delete--;
		}
		selected.set(pos, true);
		selectedCount++;
		pos = GetNextItem(pos, LVNI_ALL | LVNI_SELECTED);		
	}

	if(focus_before_delete == -1) {
		focus_before_delete =  GetItemCount() - 1;
	}

	// Remove selection so it is not restored on update
	DeselectAll();

	// Set focus rectangle
	int lastIndexAfterRemoval = GetItemCount() - selectedCount - 1;
	m_focused_item = min(lastIndexAfterRemoval, focus_before_delete);	
	DEBUG_PRINT << "Setting m_focused_item to: " << m_focused_item;

	BOOL ret = SetItemState(m_focused_item, LVIS_FOCUSED, LVIS_FOCUSED);
	DEBUG_PRINT << "CCustomListView::DeleteSelected: SetItemState(focused) successful: " << ret << ". Focus index was: " << m_focused_item;

	queue_helpers::queue_remove_mask(selected);						
}

void CCustomListView::UpdateInsertMarker(int Item) {
	TRACK_CALL_TEXT("CCustomListView::UpdateInsertMarker");
	if(Item == -1) {
		if(m_prev_insert_marker >= 0) {
			DrawInsertMarker(m_prev_insert_marker, false);		
		}
	}
	else if(Item != m_prev_insert_marker) {
		// Erase previous marker
		if(m_prev_insert_marker >= 0) {
			DrawInsertMarker(m_prev_insert_marker, false);		
		}
#ifdef _DEBUG
		console::formatter() << "Insert marker, " << Item;
#endif
		// Draw the new one
		DrawInsertMarker(Item, true);
		// Update previous insert pos.
		m_prev_insert_marker = Item;
	}

}

void CCustomListView::DrawInsertMarker(int Item, bool enable) {
	TRACK_CALL_TEXT("CCustomListView::DrawInsertMarker");
	ATLASSERT(Item >= 0);

	CClientDC cdc(*this);
	CRect	cr, r;
	int items, y;

	GetClientRect(&cr);
	items = GetItemCount();
	y;
	if (Item < items) {
		GetItemRect(Item, &r, LVIR_BOUNDS);
		y = r.top;
	} else {	
		// insert after last item
		GetItemRect(items - 1, &r, LVIR_BOUNDS);
		y = r.bottom;
	}

	if (y >= cr.bottom)	// if below control
		y = cr.bottom - 1;	// clamp to bottom edge


	if (enable) { // draw marker
#ifdef _DEBUG
		console::formatter() << "Drawing marker " << Item;
#endif
		CPen pen, prevPen;

		pen.CreatePen(PS_SOLID, LINEWEIGHT, m_insertmarkercolor);
		//brush.CreateSolidBrush(lineColor);
		prevPen = cdc.SelectPen(pen);		

		cdc.SetBkMode(TRANSPARENT);
		// draw the actual line
		cdc.MoveTo(cr.left, y);	
		cdc.LineTo(cr.right, y);

		// restore previous pen
		cdc.SelectPen(prevPen);			
	} else {	
		// Erase insertion marker
#ifdef _DEBUG
		console::formatter() << "Erasing marker " << Item;
#endif
		// Just redraw the rect of the line
		CRect	rect(cr.left, y - (LINEWEIGHT/2) - 1, cr.right, y + (LINEWEIGHT/2) + 1);
		RedrawWindow(rect, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | 
			RDW_ERASENOW |RDW_UPDATENOW | RDW_ERASE) ;
	}
}

void CCustomListView::UpdateSelection(CPoint point, bool applyCtrlSelection /*= false*/) {
	TRACK_CALL_TEXT("CCustomListView::UpdateSelection");
	if(GetCapture() == *this) {
		int selection_end_index = DetermineItemSelectionIndex(point);
#ifdef _DEBUG
		console::formatter() << "index " << selection_end_index;
#endif
		EnsureVisible(selection_end_index, true);

		CRect rect(m_selection_start_point, point);
		rect.NormalizeRect();
		if(rect != m_selection_lastRect) {
			SelectIndices(m_selection_start_index, selection_end_index);
			RedrawWindow(0, 0, RDW_UPDATENOW | RDW_ALLCHILDREN);

			// if we are applying ctrl selection we are not interested in drawing the rect
			if(!applyCtrlSelection && m_selection_lastRect != rect) {
				CClientDC dc(*this);
				//CBrush brush;
				//brush.CreateSolidBrush(m_textcolor);
				//dc.DrawDragRect(&rect, CSize(1,1), &m_selection_lastRect, CSize(1,1));			
				DrawSelectionRectangle(dc, rect, m_selection_lastRect);
				m_selection_lastRect = rect;			
			}
		}
	}
}


void CCustomListView::SetSharedSelection(metadb_handle_list_cref p_items) {
	TRACK_CALL_TEXT("CCustomListView::SetSharedSelection");
	// Only notify other components about changes in our selection,
	// if our window is the active one.
	if (::GetFocus() == m_hWnd && m_shared_selection.is_valid()) {
		m_shared_selection->set_selection_ex(p_items, contextmenu_item::caller_undefined);
	}
}


void CCustomListView::GetSelectedItemsMetaDbHandles(metadb_handle_list_ref p_out) {
	TRACK_CALL_TEXT("CCustomListView::GetSelectedItemsMetaDbHandles");
	pfc::list_t<t_size> selected;
	GetSelectedIndices(selected);
	GetMetaDbHandles(selected, p_out);
}

void CCustomListView::GetMetaDbHandles(const pfc::list_base_const_t<t_size>& indices, metadb_handle_list_ref p_out) {
	TRACK_CALL_TEXT("CCustomListView::GetMetaDbHandles");
	t_size indices_count = indices.get_count();

	// Transform indices to a bitarray
	bit_array_bittable indices_bit_array(GetItemCount());

	for(t_size i = 0; i < indices_count; i++) {
		indices_bit_array.set(indices[i], true);
	}

	metadb_handle_list tmp;
	// Retrieve the queue playback items
	m_metadbs.get_items_mask(tmp, indices_bit_array);

	// list has overloaded =, so this works 'as expected'
	p_out = tmp;

}

metadb_handle_list_ref CCustomListView::GetAllMetadbs() {
	return m_metadbs;
}
