#include "stdafx.h"
#include "DropTargetImpl.h"


// We copy the pdfEffect value
process_locations_notify_impl::process_locations_notify_impl(
	MetaDbHandleDropTarget* const proxy,
	const DragDropInformation & drag_drop_information,
	const ActivePlaylistInformation & active_playlist_information) : 

	  m_proxy(proxy), 
	  m_drag_drop_information(drag_drop_information), 
	  m_active_playlist_information(active_playlist_information) { }

void process_locations_notify_impl::on_completion(const pfc::list_base_const_t<metadb_handle_ptr> &p_items) {
	TRACK_CALL_TEXT("process_locations_notify_impl::on_completion");
#ifdef _DEBUG
	console::formatter() << "process_locations_notify_impl, on_completion, Queue size:" << static_api_ptr_t<playlist_manager>()->queue_get_count()
		<< " Selection size " << m_drag_drop_information.m_selection.get_count();
#endif

	// We have processed IDataObject, let's forward it to proxy and also
	// passing the state that was during the Drop.
	 HRESULT result = m_proxy->Drop(p_items,
		 m_drag_drop_information.m_grfKeyState,
		 m_drag_drop_information.m_point_screen,
		 m_drag_drop_information.m_point_client, 
		 &m_drag_drop_information.m_pdwEffect,
		 m_drag_drop_information.m_selection,
		 m_drag_drop_information.m_internal_dragging,
		 m_active_playlist_information);

	 if(m_drag_drop_information.m_asyncOperation != NULL) {
	 	// m_pdwEffect contains the accepted effect
	 	m_drag_drop_information.m_asyncOperation->EndOperation(result, NULL, m_drag_drop_information.m_pdwEffect);
	 }
}
void process_locations_notify_impl::on_aborted() {
	TRACK_CALL_TEXT("process_locations_notify_impl::on_aborted");
	if(m_drag_drop_information.m_asyncOperation != NULL) {
		m_drag_drop_information.m_asyncOperation->EndOperation(S_OK, NULL, DROPEFFECT_NONE);
	}
}


DropTargetImpl::DropTargetImpl(HWND hWnd, MetaDbHandleDropTarget* p_proxy) : m_refcount(1), m_hWnd(hWnd), m_proxy(p_proxy)
{
}

pfc::com_ptr_t<IDropTarget> DropTargetImpl::g_create(HWND hWnd, MetaDbHandleDropTarget* proxy)
{
	TRACK_CALL_TEXT("DropTargetImpl::g_create");
	pfc::com_ptr_t<DropTargetImpl> temp;
	temp.attach(new DropTargetImpl(hWnd, proxy));
	return temp;
}


/////////////////////////////////////////////////////////
// IUnknown methods

// We support the IUnknown and IDropTarget interfaces.
STDMETHODIMP DropTargetImpl::QueryInterface(REFIID iid, void ** ppvObject) {
	if (ppvObject==0) return E_INVALIDARG;
	else if (iid == IID_IUnknown) {
		AddRef();
		*ppvObject = (IUnknown*)this;
		return S_OK;
	} else if (iid == IID_IDropTarget) {
		AddRef();
		*ppvObject = (IDropTarget*)this;
		return S_OK;
	} else return E_NOINTERFACE;
}

// Increase reference count.
STDMETHODIMP_(ULONG) DropTargetImpl::AddRef() {
	return ++m_refcount;
}

// Decrease reference count.
// Delete object, if reference count reaches zero.
STDMETHODIMP_(ULONG) DropTargetImpl::Release() {
	LONG rv = --m_refcount;
	if (rv == 0) {
		delete this;	
	}
	return rv;
}

// IDropTarget methods
STDMETHODIMP DropTargetImpl::DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
	TRACK_CALL_TEXT("DropTargetImpl::DragEnter");
	static_api_ptr_t<playlist_incoming_item_filter> incoming_item_filter;
	
	if(incoming_item_filter->process_dropped_files_check(pDataObj)) {
		// IDataObject contains data that is convertible to metadb_handles				
		CPoint point_screen(pt.x, pt.y);
		CPoint point_client(pt.x, pt.y);
		ScreenToClient(m_hWnd, &point_client);
		
		return m_proxy->DragEnter(grfKeyState, point_screen, point_client, pdwEffect);
	} else {
		// We do not accept the data
		*pdwEffect = DROPEFFECT_NONE;
		return S_OK;
	}
}

STDMETHODIMP DropTargetImpl::DragLeave() {
	TRACK_CALL_TEXT("DropTargetImpl::DragLeave");
	return m_proxy->DragLeave();
}


STDMETHODIMP DropTargetImpl::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
	TRACK_CALL_TEXT("DropTargetImpl::DragOver");
	CPoint point_screen(pt.x, pt.y);
	CPoint point_client(pt.x, pt.y);	
	ScreenToClient(m_hWnd, &point_client);

	return m_proxy->DragOver(grfKeyState, point_screen, point_client, pdwEffect);
}

STDMETHODIMP DropTargetImpl::Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
	TRACK_CALL_TEXT("DropTargetImpl::Drop");
	CPoint point_screen(pt.x, pt.y);
	CPoint point_client(pt.x, pt.y);
	ScreenToClient(m_hWnd, &point_client);

	HWND parent = WindowFromPoint(point_screen);
	
	static_api_ptr_t<playlist_incoming_item_filter_v2> incoming_item_filter;

	pfc::list_t<t_size> selection;
	bool internal_dragging = m_proxy->GetInternalDragging();	
	m_proxy->GetSelectedIndices(selection);

#ifdef _DEBUG
	console::formatter() << "DropTargetImpl::Drop, selection count " << selection.get_count();
#endif

	// Save the state which defines the effects at the drop moment
	DragDropInformation drag_drop_information;
	drag_drop_information.m_grfKeyState = grfKeyState;
	drag_drop_information.m_point_screen = point_screen;
	drag_drop_information.m_point_client = point_client;
	drag_drop_information.m_pdwEffect = *pdwEffect;
	drag_drop_information.m_asyncOperation = (IDataObjectAsyncCapability*) NULL;
	drag_drop_information.m_selection = selection;
	drag_drop_information.m_internal_dragging = internal_dragging;

	
	ActivePlaylistInformation active_playlist_information;
	static_api_ptr_t<playlist_manager> playlist_api;
	static_api_ptr_t<ui_selection_manager> selection_manager;

	// HACK: Now this is a hack. We store active playlist selected content
	// for Drop. If it turns out that IDataObject has the same metadb_handles (in the same order)
	// we just assume it must be from the playlist.
	// In addition we make sure that the selected items in the active playlist correspond to
	// selection_manager's selection (which should be selected items of the focused ui element).

	// This process does not absolutely guarantee that we know when we are dropping from the playlist
	// but it should be sufficient in all regular situations.
	
	// It will fail with the following cases:
	// 1. Using of playlist viewer that doesn't support foobar2000 selection_manager
	// 2. We are dropping from somewhere else that is *not* a playlist but the playlist has the exact same
	//    items selected and they are ordered the same. In this case we will come into conclusion that the drag&drop is from the
	//    playlist (which is wrong).
	
	// Save the active playlist selection
	active_playlist_information.active_playlist_index = playlist_api->get_active_playlist();
	if(active_playlist_information.active_playlist_index != pfc::infinite_size) {
		DEBUG_PRINT << "DropTargetImpl::Drop, storing active playlist selection";
		pfc::list_t<metadb_handle_ptr> tmp;
		playlist_api->playlist_get_selection_mask(active_playlist_information.active_playlist_index,
			active_playlist_information.active_playlist_selection_mask);
		
		playlist_api->playlist_get_selected_items(active_playlist_information.active_playlist_index, 
			active_playlist_information.active_playlist_selected_items);

		// We must also store the playlist item count to use the mask
		active_playlist_information.active_playlist_item_count = playlist_api->playlist_get_item_count(active_playlist_information.active_playlist_index);

		selection_manager->get_selection(tmp);

		bool from_playlist = pfc::list_base_const_t<metadb_handle_ptr>::g_equals(tmp, active_playlist_information.active_playlist_selected_items);

		if(!from_playlist) {
			DEBUG_PRINT << "DropTargetImpl::Drop, Drag&Drop cannot be from the active playlist since the selected items in the active playlist "
				<< "doesn't correspond to selection_manager's selection.";
			// As the selection doesn't seem to come from active playlist, we reset all the info
			active_playlist_information.active_playlist_selected_items.remove_all();
			active_playlist_information.active_playlist_index = pfc::infinite_size;
			active_playlist_information.active_playlist_item_count = 0;
		}
	}

	
	service_ptr_t<process_locations_notify_impl> notify = 
			new service_impl_t<process_locations_notify_impl>(m_proxy, drag_drop_information, active_playlist_information);
	
	// asynchronously process IDataObject and let it handle the rest.
	incoming_item_filter->process_dropped_files_async(pDataObj, NULL, parent, notify);

	HRESULT result = m_proxy->DetermineDropAction(grfKeyState, point_screen, point_client, pdwEffect, internal_dragging);
	return result;


	/* // For now, let's ignore IAsyncOperation stuff alltogether since it isn't supported by foobar:
	   // IDataObject emitted from playlists doens't contain that interface

	// See http://msdn.microsoft.com/en-us/library/bb776904(VS.85).aspx#async for more information
	BOOL asyncOperationSupported;
	IAsyncOperation* asyncOperation;
	HRESULT result = pDataObj->QueryInterface(IID_IAsyncOperation, (void **) &asyncOperation);

	if(result == E_NOINTERFACE) {
		syncronousProcessing = false;
	}

	if(SUCCEEDED(result))  {
		result = asyncOperation->GetAsyncMode(&asyncOperationSupported);
		if(FAILED(result)) return E_UNEXPECTED;

		if(asyncOperationSupported == VARIANT_TRUE) {
			result = asyncOperation->StartOperation(NULL);
			if(FAILED(result)) return E_UNEXPECTED;
	#ifdef _DEBUG
			console::formatter() << "Starting async processing of IDataObject";
	#endif
			
			service_ptr_t<process_locations_notify_impl> notify = 
				new service_impl_t<process_locations_notify_impl>(m_proxy, grfKeyState,	point_screen, point_client, pdwEffect, asyncOperation);
			
			incoming_item_filter->process_dropped_files_async(pDataObj, NULL, parent, notify);

			
		} else {
	#ifdef _DEBUG
			console::formatter() << "Starting syncronous processing of IDataObject";
	#endif

			// syncronous processing
			pfc::list_t<metadb_handle_ptr> handles;
			bool hasMetadbHandles = incoming_item_filter->process_dropped_files(pDataObj, handles, false, parent);
			
			if(hasMetadbHandles) {
				return m_proxy->Drop(handles, grfKeyState, point_screen, point_client, pdwEffect);
			} else {
				*pdwEffect = DROPEFFECT_NONE;
				return S_OK;
			}
		}
	} else {
		HRESULT action = DetermineDropAction(grfKeyState, pt_screen, pt_client, pdwEffect);
	}

	
	if(FAILED(result)) return E_UNEXPECTED;
	*/
	
}



