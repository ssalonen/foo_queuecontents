#pragma once

#include "stdafx.h"
#include "ui_element_base.h"
#include "../columns_ui-sdk/ui_extension.h"

// {8A29DF21-FBEE-4F9C-A356-A777FF263188}
static const GUID uie_guid = 
{ 0x8a29df21, 0xfbee, 0x4f9c, { 0xa3, 0x56, 0xa7, 0x77, 0xff, 0x26, 0x31, 0x88 } };

// {68CB2DBD-F26E-420D-9002-4663E65A138F}
static const GUID uie_font_client_guid = 
		{ 0x68cb2dbd, 0xf26e, 0x420d, { 0x90, 0x2, 0x46, 0x63, 0xe6, 0x5a, 0x13, 0x8f } };

// {5273EE23-3FD6-48AE-A91E-4D6549F05694}
static const GUID uie_colours_client_guid = 
		{ 0x5273ee23, 0x3fd6, 0x48ae, { 0xa9, 0x1e, 0x4d, 0x65, 0x49, 0xf0, 0x56, 0x94 } };

class queuecontents_uie_colours_client : public columns_ui::colours::client {
	
	virtual void get_name(pfc::string_base & out) const {
		out.set_string(COMPONENTNAME);	
	}

	virtual const GUID & get_client_guid() const	{		
		return uie_colours_client_guid;
	}

	virtual t_size get_supported_bools() const {
		return 0;
	}
	virtual bool get_themes_supported() const {
		return false;
	}
	
	virtual void on_colour_changed(t_size mask) const {
		window_manager::VisualsChanged();
	}
	
	virtual void on_bool_changed(t_size mask) const {
		// ???
	}
};

class queuecontents_uie_fonts_client : public columns_ui::fonts::client  {

	virtual void get_name(pfc::string_base & out) const {
		out.set_string(COMPONENTNAME);	
	}
	
	virtual const GUID & get_client_guid() const	{				
		return uie_font_client_guid;
	}

	virtual columns_ui::fonts::font_type_t get_default_font_type() const {
		return columns_ui::fonts::font_type_items;
	}

	virtual void on_font_changed() const {
		window_manager::VisualsChanged();
	}
};

class uie_element : public ui_element_base, public ui_extension::container_ui_extension { 
//WS_EX_CONTROLPARENT 
private:
	static const GUID extension_guid;
	virtual class_data & get_class_data() const 	{
		__implement_get_class_data(_T("{8A29DF21-FBEE-4F9C-A356-A777FF263188}"), false);
	}	
	LRESULT on_message(HWND wnd, UINT msg,WPARAM wp, LPARAM lp);
		
public:
	// ui_element_base
	virtual void RefreshVisuals() ;	
	virtual void save_configuration();
	virtual HWND get_wnd();
	HWND get_wnd() const;
	virtual bool ActivatePlaylistUIElement();


	virtual const GUID & get_extension_guid() const	{
		return extension_guid;
	}

	virtual void get_name(pfc::string_base & out) const {
		out.set_string(COMPONENTNAME);	
	}

	virtual void get_category(pfc::string_base & out)const {
		out.set_string("Panels");
	}

	unsigned get_type () const {
		/** In this case we are only of type type_panel */
		return ui_extension::type_panel;
	}

	virtual bool have_config_popup(){return false;}
	virtual bool show_config_popup(HWND wnd_parent){return false;};

	// From ui_extension::extension_base
	virtual void set_config(stream_reader * p_reader, t_size p_size, abort_callback & p_abort){
		stream_reader_formatter<false> reader(*p_reader, p_abort);
		if(p_size == 0) {
			// Defaults
			ui_element_settings defaults;
			m_settings = defaults;
			m_settings.m_columns.remove_all();
			if(cfg_ui_columns.first().is_valid()) {
				m_settings.m_columns.add_item(ui_column_settings(cfg_ui_columns.first()->m_key));
			} else {
				// object.m_columns is empty
			}
		}
		reader >> m_settings;

	};
	virtual void get_config(stream_writer * p_writer, abort_callback & p_abort) const {
		stream_writer_formatter<false> writer(*p_writer, p_abort);
		writer << m_settings;
	};
};



