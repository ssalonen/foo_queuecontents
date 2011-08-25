#include "stdafx.h"
#include "DropSourceImpl.h"





DropSourceImpl::DropSourceImpl(HWND p_hWnd, DropSourceInternalDraggingNotify* notify) :
  m_refcount(1), m_hWnd(p_hWnd), m_dropSourceNotify(DropSourceNotifyImpl::g_create(p_hWnd, notify))
{
}

pfc::com_ptr_t<IDropSource> DropSourceImpl::g_create(HWND hWnd, DropSourceInternalDraggingNotify* notify)
{
	pfc::com_ptr_t<IDropSource> temp;
	temp.attach(new DropSourceImpl(hWnd, notify));
	return temp;
}


/////////////////////////////////////////////////////////
// IUnknown methods

// We support the IUnknown and IDropSource interfaces.
STDMETHODIMP DropSourceImpl::QueryInterface(REFIID iid, void ** ppvObject) {
	if (ppvObject==0) return E_INVALIDARG;
	else if (iid == IID_IUnknown) {
		AddRef();
		*ppvObject = reinterpret_cast<IUnknown*>( this );
		return S_OK;
	} else if (iid == IID_IDropSource) {
		AddRef();
		*ppvObject = reinterpret_cast<IDropSource*>( this );
		return S_OK;
	} else if(iid == IID_IDropSourceNotify && m_dropSourceNotify.is_valid()) {
		return m_dropSourceNotify->QueryInterface(iid, ppvObject);
	}
	else return E_NOINTERFACE;
}

// Increase reference count.
STDMETHODIMP_(ULONG) DropSourceImpl::AddRef() {
	return ++m_refcount;
}

// Decrease reference count.
// Delete object, if reference count reaches zero.
STDMETHODIMP_(ULONG) DropSourceImpl::Release() {
	LONG rv = --m_refcount;
	if (rv == 0) { 
		m_dropSourceNotify.release();
		delete this;
	}
	return rv;
}

// IDropSource methods

// Determine whether the drag operation should be continued.
// Return S_OK to continue, DRAGDROP_S_CANCEL to cancel the operation,
// or DRAGDROP_S_DROP to perform a drop.
STDMETHODIMP DropSourceImpl::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) {
	// Cancel if escape was pressed.
	if (fEscapePressed) {
		return DRAGDROP_S_CANCEL;
	} else if (grfKeyState & MK_RBUTTON) {
		// Cancel if right mouse button was pressed.
		return DRAGDROP_S_CANCEL;	
	} else if (!(grfKeyState & MK_LBUTTON)) {		
		// Drop if left mouse button was released.
		return DRAGDROP_S_DROP;
	} else { 
		// Continue otherwise.
		return S_OK;
	}
}

// Provide visual feedback (through mouse pointer) about the state of the
// drag operation.
STDMETHODIMP DropSourceImpl::GiveFeedback(DWORD dwEffect) {
	// Let OLE show the default cursor.
	return DRAGDROP_S_USEDEFAULTCURSORS;
}

