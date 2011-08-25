#pragma once

#include "stdafx.h"
#include "PropertyItem.h"
#include "PropertyItemEditors.h"

class CPropertyEditWindowWithAutomaticSave : 
   public CPropertyEditWindow
{
public:
	DECLARE_WND_SUPERCLASS(_T("WTL_InplacePropertyEditWithAutomaticSave"), _T("WTL_InplacePropertyEdit"))

	BEGIN_MSG_MAP(CPropertyEditWindowWithAutomaticSave)
	  	if(uMsg == WM_KEYDOWN) 
		{ 
			bHandled = TRUE; 
			lResult = OnKeyDown(hWnd, uMsg, wParam, lParam, bHandled); 
			if(bHandled) 
				return TRUE; 
		}
	  CHAIN_MSG_MAP(CPropertyEditWindow) // Let's do default processing of CPropertyEditWindow
	END_MSG_MAP()

	LRESULT OnKillFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		//DEBUG_PRINT << "WTL_InplacePropertyEditWithAutomaticSave: OnKillFocus, Trying to enforce saving";
		//SendMessage(WM_KEYDOWN, VK_RETURN, 0);
		// Let's do default processing of CPropertyEditWindow
		bHandled = FALSE;
		return 0;
   }

   LRESULT OnKeyDown(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) 
	 {
		bHandled = FALSE;
		switch(wParam) {
			case VK_UP:
			case VK_DOWN:    
				// We do not allow up/down keys when we are focused
				if ( ::GetFocus() == m_hWnd) {
					bHandled = TRUE;
				}
				//// If we have modifications we save them by sending VK_RETURN
				//if( GetModify() ) {
				//	DEBUG_PRINT << "WTL_InplacePropertyEditWithAutomaticSave: OnKeyDown (navigation), Trying to enforce saving";
				//	// We also send VK_RETURN to text is committed before navigation
				//	SendMessage(WM_KEYDOWN, VK_RETURN, 0);			
				//	// Return focus immediately
				//	::SetFocus(m_hWnd);
				//}
				break;
		}		

		return 0;
	}
};


class CPropertyEditItemWithAutomaticSave : public CPropertyEditItem
{
public:
   CPropertyEditItemWithAutomaticSave(LPCTSTR pstrName, LPARAM lParam) : 
      CPropertyEditItem(pstrName, lParam)
   {
   }

   CPropertyEditItemWithAutomaticSave(LPCTSTR pstrName, CComVariant vValue, LPARAM lParam) : 
      CPropertyEditItem(pstrName, lParam)
   {
      m_val = vValue;
   }

   // Basically copy-pasted from CPropertyEditItem.h, it just uses CPropertyEditWindowWithAutomaticSave
   HWND CreateInplaceControl(HWND hWnd, const RECT& rc) 
   {
      // Get default text
      UINT cchMax = GetDisplayValueLength() + 1;
      LPTSTR pszText = (LPTSTR) _alloca(cchMax * sizeof(TCHAR));
      ATLASSERT(pszText);
      if( !GetDisplayValue(pszText, cchMax) ) return NULL;
      // Create EDIT control
      CPropertyEditWindowWithAutomaticSave* win = new CPropertyEditWindowWithAutomaticSave();
      ATLASSERT(win);
      RECT rcWin = rc;
      m_hwndEdit = win->Create(hWnd, rcWin, pszText, WS_VISIBLE | WS_CHILD | ES_LEFT | ES_AUTOHSCROLL);
      ATLASSERT(::IsWindow(m_hwndEdit));
      // Simple hack to validate numbers
      switch( m_val.vt ) {
      case VT_UI1:
      case VT_UI2:
      case VT_UI4:
         win->ModifyStyle(0, ES_NUMBER);
         break;
      }
      return m_hwndEdit;
   }  
};



inline HPROPERTY PropCreateSimpleWithAutosave(LPCTSTR pstrName, LPCTSTR pstrValue, LPARAM lParam = 0)
{
   return new CPropertyEditItemWithAutomaticSave(pstrName, CComVariant(pstrValue), lParam);
}

