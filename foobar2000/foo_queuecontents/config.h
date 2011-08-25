#pragma once
#include "stdafx.h"
#include "cfg_objMapWithDefault.h"

// definition of a column, these are referenced from ui elements
class ui_column_definition {
public:
	ui_column_definition(pfc::string8 name, pfc::string8 pattern, t_int alignment):
	  m_name(name), m_pattern(pattern), m_alignment(alignment) {}
	
	ui_column_definition(){};

	pfc::string8 m_name;
	pfc::string8 m_pattern;
	t_int m_alignment;
};

FB2K_STREAM_WRITER_OVERLOAD(ui_column_definition);
FB2K_STREAM_READER_OVERLOAD(ui_column_definition);

// Must correspond to listview alignment definitions, so don't change these!
static const t_int COLUMN_ALIGNMENT_LEFT = LVCFMT_LEFT;
static const t_int COLUMN_ALIGNMENT_CENTER = LVCFMT_CENTER;
static const t_int COLUMN_ALIGNMENT_RIGHT = LVCFMT_RIGHT;



// {E1311749-AF6A-4C41-BB69-3E98A539E04D}
static const GUID guid_cfg_playlist_enabled = 
{ 0xe1311749, 0xaf6a, 0x4c41, { 0xbb, 0x69, 0x3e, 0x98, 0xa5, 0x39, 0xe0, 0x4d } };

// {C0EEDD4A-7E48-425f-ACF5-20CD83C12EAB}
static const GUID guid_cfg_playlist_name = 
{ 0xc0eedd4a, 0x7e48, 0x425f, { 0xac, 0xf5, 0x20, 0xcd, 0x83, 0xc1, 0x2e, 0xab } };

// Legacy setting.
// {8E696057-24A6-4D6E-8EC7-E9E13598CCB0}
//static const GUID guid_cfg_ui_format = 
//{ 0x8e696057, 0x24a6, 0x4d6e, { 0x8e, 0xc7, 0xe9, 0xe1, 0x35, 0x98, 0xcc, 0xb0 } };

// {29DDDC31-BB34-453E-BE1A-D7DB0BAB9718}
static const GUID guid_cfg_ui_columns = 
{ 0x29dddc31, 0xbb34, 0x453e, { 0xbe, 0x1a, 0xd7, 0xdb, 0xb, 0xab, 0x97, 0x18 } };


static const int default_cfg_playlist_enabled = 0;
static const char* default_cfg_playlist_name = "Queue";

// default columns
static t_storage_impl<long, ui_column_definition> default_cfg_ui_columns[] = {
	t_storage_impl<long, ui_column_definition>::create(0, ui_column_definition(pfc::string8("Queue Item"), pfc::string8("%queue_index% - %title%"), COLUMN_ALIGNMENT_LEFT)),
	t_storage_impl<long, ui_column_definition>::create(1, ui_column_definition(pfc::string8("Queue Index"), pfc::string8("%queue_index%"), COLUMN_ALIGNMENT_LEFT)),
	t_storage_impl<long, ui_column_definition>::create(2, ui_column_definition(pfc::string8("Queue Total"), pfc::string8("%queue_total%"), COLUMN_ALIGNMENT_LEFT)), 
	t_storage_impl<long, ui_column_definition>::create(3, ui_column_definition(pfc::string8("Album Artist"), pfc::string8("%album artist%"), COLUMN_ALIGNMENT_LEFT)),
	t_storage_impl<long, ui_column_definition>::create(4, ui_column_definition(pfc::string8("Album"), pfc::string8("%album%"), COLUMN_ALIGNMENT_LEFT)),
	t_storage_impl<long, ui_column_definition>::create(5, ui_column_definition(pfc::string8("Title"), pfc::string8("%title%"), COLUMN_ALIGNMENT_LEFT)),
	t_storage_impl<long, ui_column_definition>::create(6, ui_column_definition(pfc::string8("Track No"), pfc::string8("%tracknumber%"), COLUMN_ALIGNMENT_LEFT)),	
	t_storage_impl<long, ui_column_definition>::create(7, ui_column_definition(pfc::string8("Codec"), pfc::string8("%codec%"), COLUMN_ALIGNMENT_LEFT))
};


extern cfg_string cfg_playlist_name;
extern cfg_bool cfg_playlist_enabled;

extern cfg_objMapWithDefault< pfc::map_t<long, ui_column_definition> > cfg_ui_columns;