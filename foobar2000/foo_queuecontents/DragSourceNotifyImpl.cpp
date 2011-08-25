#include "stdafx.h"
#include "DropSourceNotifyImpl.h"


DropSourceNotifyImpl::DropSourceNotifyImpl(HWND p_hWnd, DropSourceInternalDraggingNotify* notify) :
  m_refcount(1), m_hWnd(p_hWnd),  m_notify(notify)
{
}

pfc::com_ptr_t<IDropSourceNotify> DropSourceNotifyImpl::g_create(HWND hWnd, DropSourceInternalDraggingNotify* notify)
{
	pfc::com_ptr_t<IDropSourceNotify> temp;
	temp.attach(new DropSourceNotifyImpl(hWnd, notify));
	return temp;
}
STDMETHODIMP DropSourceNotifyImpl::DragEnterTarget(HWND  hwndTarget) {
	if(m_notify == NULL)
		return S_OK;

	bool isInternal = (hwndTarget == m_hWnd) != 0;
	m_notify->SetInternalDragging(isInternal);

	return S_OK;
}

STDMETHODIMP DropSourceNotifyImpl::DragLeaveTarget() {
	if(m_notify == NULL)
		return S_OK;

	m_notify->SetInternalDragging(false);

	return S_OK;
}

/////////////////////////////////////////////////////////
// IUnknown methods

// We support the IUnknown and IDropSourceNotify interfaces.
STDMETHODIMP DropSourceNotifyImpl::QueryInterface(REFIID iid, void ** ppvObject) {
	if (ppvObject==0) return E_INVALIDARG;
	else if (iid == IID_IUnknown) {
		AddRef();
		*ppvObject = reinterpret_cast<IUnknown*>( this );
		return S_OK;
	} else if(iid == IID_IDropSourceNotify) {
		AddRef();
		*ppvObject = reinterpret_cast<IDropSourceNotify*>( this );
		return S_OK;
	}
	else return E_NOINTERFACE;
}

// Increase reference count.
STDMETHODIMP_(ULONG) DropSourceNotifyImpl::AddRef() {
	return ++m_refcount;
}

// Decrease reference count.
// Delete object, if reference count reaches zero.
STDMETHODIMP_(ULONG) DropSourceNotifyImpl::Release() {
	LONG rv = --m_refcount;
	if (rv == 0) { 
		delete this;

	}
	return rv;
}



