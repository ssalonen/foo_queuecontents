#pragma once
#include "stdafx.h"
#include "DropSourceNotifyImpl.h"

// IDropSource implementation for drag&drop.
class DropSourceImpl : public IDropSource {
private:
	
	pfc::refcounter m_refcount;
	HWND m_hWnd;
	pfc::com_ptr_t<IDropSourceNotify> m_dropSourceNotify;

	DropSourceImpl(HWND hWnd, DropSourceInternalDraggingNotify* notify);

public:
	static pfc::com_ptr_t<IDropSource> g_create(HWND hWnd, DropSourceInternalDraggingNotify* notify);

	/////////////////////////////////////////////////////////
	// IUnknown methods

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	STDMETHOD_(ULONG, AddRef)();
	STDMETHOD_(ULONG, Release)();

	/////////////////////////////////////////////////////////
	// IDropSource methods

	STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed, DWORD grfKeyState);
	STDMETHOD(GiveFeedback)(DWORD dwEffect);


	
};
