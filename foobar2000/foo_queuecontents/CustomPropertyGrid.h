#pragma once

#include "stdafx.h"
#include "PropertyGrid.h"

// Handles ESC so that it is bubbled to the parent
// Important in the preferences
class CCustomPropertyGrid : public CPropertyGridCtrl {
	BEGIN_MSG_MAP(CCustomPropertyGrid)
	  	if(uMsg == WM_KEYDOWN) 
		{ 
			bHandled = TRUE; 
			lResult = OnKeyDown(hWnd, uMsg, wParam, lParam, bHandled); 
			if(bHandled) 
				return TRUE; 
		}
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		CHAIN_MSG_MAP(CPropertyGridCtrl)
	END_MSG_MAP()

	LRESULT OnKeyDown(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) 
	{		
		bHandled = TRUE;
		LRESULT lResult = 0;
		BOOL grid_window_handled = FALSE;
		// We process other keys in the following order
		// 1. CPropertyGridCtrl WM_USER_PROP_NAVIGATE
		// 2. CPropertyGridCtrl WM_KEYDOWN 
		// 3. Bubble to Parent window (if ESC we set focus to parent first)
		
		// Copied from PropertyItemEditors
		// We send WM_USER_PROP_NAVIGATE message corresponding the key pressed
		switch(wParam) {
//			case VK_TAB: // We want to use tab for losing the focus of the control
			case VK_UP:
			case VK_DOWN:
			case VK_LEFT:
			case VK_RIGHT:
				grid_window_handled = CPropertyGridCtrl::ProcessWindowMessage(hWnd, WM_USER_PROP_NAVIGATE, LOWORD(wParam), lParam, lResult);			
		}
		DEBUG_PRINT << "CCustomPropertyGrid: CPropertyGridCtrl ProcessWindowMessage WM_USER_PROP_NAVIGATE " << grid_window_handled; 

		if(!grid_window_handled) {
			grid_window_handled = CPropertyGridCtrl::ProcessWindowMessage(hWnd, uMsg, wParam, lParam, lResult);			
			DEBUG_PRINT << "CCustomPropertyGrid: CPropertyGridCtrl ProcessWindowMessage WM_KEYDOWN " << grid_window_handled; 
		}

		if(grid_window_handled) {
			return lResult;
		} else {
			boolean returnFocus = false;
			if(wParam == VK_ESCAPE) {
				DEBUG_PRINT << "CCustomPropertyGrid: Has focus?" << (GetFocus() == m_hWnd);
				// We must set focus to parent to handle keydowns properly in the parent
				if((GetFocus() == m_hWnd)) {
					::SetFocus(GetParent());
					returnFocus = true;
				}
			}
			
			bHandled = TRUE;
			if(GetParent() != NULL) {
				DEBUG_PRINT << "CCustomPropertyGrid: Parent handled " << GetParent().PostMessageW(uMsg, wParam, lParam);
			}

			if(returnFocus) {
				// Set focus back to this control
				::SetFocus(m_hWnd);
			}
			
		}

		return lResult;
	}	
	
   LRESULT OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
   {
   	   // If we have proper m_hwndInplace (in-place editor), we commit the current value
	   // before the default processing of PropertyGrid which will lose the changes!
	   if( m_hwndInplace != NULL && ::IsWindow(m_hwndInplace) ) {
			SendMessage(WM_USER_PROP_UPDATEPROPERTY, 0, (LPARAM) m_hwndInplace);
	   }
	   bHandled  = FALSE;
	   return 0;
	}
};
