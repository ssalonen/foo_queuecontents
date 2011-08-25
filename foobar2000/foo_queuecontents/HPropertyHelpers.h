#pragma once
#include "stdafx.h"
#include "PropertyItem.h"

class HPropertyHelpers {
public:
	static void GetDisplayValue(const HPROPERTY & prop, pfc::string8 & out) {
		t_size value_length = prop->GetDisplayValueLength();	
		
		// Remember to reserve space for ending NULL
		pfc::array_staticsize_t<TCHAR> buffer(value_length+1);		
		LPTSTR buffer_ptr = buffer.get_ptr();
		
		prop->GetDisplayValue(buffer_ptr, value_length+1);

		out.set_string(pfc::stringcvt::string_utf8_from_os(buffer_ptr));
	
	}
};

