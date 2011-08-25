#pragma once
#include "stdafx.h"

class DropSourceInternalDraggingNotify {
public:
	virtual void SetInternalDragging(bool value) = 0;
};

class 	 DropSourceNotifyImpl : public IDropSourceNotify {
private:
	DropSourceNotifyImpl(HWND hWnd,  DropSourceInternalDraggingNotify* notify);
	
	DropSourceInternalDraggingNotify* m_notify;
	pfc::refcounter m_refcount;
	HWND m_hWnd;
	

public:
	
	static pfc::com_ptr_t<IDropSourceNotify> g_create(HWND hWnd,  DropSourceInternalDraggingNotify* notify);

	/////////////////////////////////////////////////////////
	// IUnknown methods

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	STDMETHOD_(ULONG, AddRef)();
	STDMETHOD_(ULONG, Release)();

	/////////////////////////////////////////////////////////
	// IDropSourceNotify methods
	STDMETHOD(DragEnterTarget)(HWND  hwndTarget);
	STDMETHOD(DragLeaveTarget)();

};

