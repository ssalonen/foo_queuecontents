#include "stdafx.h"
#include "ui_element_configuration.h"

bool ui_element_settings::column_exists(long column_id) {
	TRACK_CALL_TEXT("ui_element_settings::column_exists");
	return column_index_from_id(column_id) != pfc::infinite_size;
}

t_size ui_element_settings::column_index_from_id(long column_id) {
	TRACK_CALL_TEXT("ui_element_settings::column_index_from_id");
	t_size count = m_columns.get_count();
	for(t_size i = 0; i < count; i++) {
		if(m_columns[i].m_id == column_id)
			return i;
	}

	return pfc::infinite_size;
}
