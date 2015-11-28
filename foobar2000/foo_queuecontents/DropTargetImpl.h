#pragma once
#include "stdafx.h"

//contains information about the state of the active playlist at the time of drop
struct ActivePlaylistInformation {
	// pfc::infinite_size if the drop is the drop is not coming from active playlist
	t_size active_playlist_index;
	t_size active_playlist_item_count;
	pfc::bit_array_var_impl active_playlist_selection_mask;
	pfc::list_t<metadb_handle_ptr> active_playlist_selected_items;
};

// Contains parameters of the drag & drop operation, required
// for determining the drop effect
struct DragDropInformation {
	DWORD m_grfKeyState;
	CPoint m_point_screen;
	CPoint m_point_client; 
	DWORD m_pdwEffect;
	IDataObjectAsyncCapability* m_asyncOperation;
	pfc::list_t<t_size> m_selection;
	bool m_internal_dragging;
};

// Proxy class which will receive all Drop related events
class MetaDbHandleDropTarget {
public:
	STDMETHOD(DragLeave)() = 0;
	STDMETHOD(DragEnter)(DWORD grfKeyState, CPoint screen_pt, CPoint client_pt, DWORD *pdwEffect) = 0;
	STDMETHOD(DragOver)(DWORD grfKeyState, CPoint screen_pt, CPoint client_pt, DWORD *pdwEffect) = 0;
	STDMETHOD(Drop)(const pfc::list_base_const_t<metadb_handle_ptr>& handles, DWORD grfKeyState, CPoint screen_pt, CPoint client_pt, DWORD *pdwEffect, 
		const pfc::list_base_const_t<t_size>& selection, bool internal_dragging, const ActivePlaylistInformation & active_playlist_information) = 0;

	STDMETHOD(DetermineDropAction)(DWORD grfKeyState, CPoint pt_screen, CPoint pt_client, DWORD *pdwEffect, bool internal_dragging) = 0;
	
	// Following are for saving the state on the drop
	virtual bool GetInternalDragging() = 0;
	
	virtual void GetSelectedIndices(pfc::list_base_t<t_size>& p_out) = 0;
};

// Receives parsed metadb handles from IDataObject
class process_locations_notify_impl : public process_locations_notify {
public:
	process_locations_notify_impl(MetaDbHandleDropTarget* const proxy, const DragDropInformation & drag_drop_information, 
		const ActivePlaylistInformation & active_playlist_information);
	
	void on_completion(const pfc::list_base_const_t<metadb_handle_ptr> &p_items);
	void on_aborted();

private:
	MetaDbHandleDropTarget* const m_proxy;
	DragDropInformation m_drag_drop_information;
	ActivePlaylistInformation m_active_playlist_information;
};


class DropTargetImpl : public IDropTarget  {
private:
	pfc::refcounter m_refcount;
	HWND m_hWnd;
	MetaDbHandleDropTarget* const m_proxy;	

	DropTargetImpl(HWND hWnd, MetaDbHandleDropTarget* proxy);

public:
	static pfc::com_ptr_t<IDropTarget> g_create(HWND hWnd, MetaDbHandleDropTarget* const proxy);

	/////////////////////////////////////////////////////////
	// IUnknown methods

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	STDMETHOD_(ULONG, AddRef)();
	STDMETHOD_(ULONG, Release)();

	/////////////////////////////////////////////////////////
	// IDropTarget methods
	
	STDMETHOD(DragEnter)(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	STDMETHOD(DragLeave)();
	STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	STDMETHOD(Drop)(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

};



