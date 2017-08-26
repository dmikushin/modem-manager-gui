/*
 *      info-page.h
 *      
 *      Copyright 2012-2014 Alex <alex@linuxonly.ru>
 *      
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
 
#ifndef __INFO_PAGE_H__
#define __INFO_PAGE_H__

#include <gtk/gtk.h>

#include "main.h"

//INFO
void mmgui_main_info_update_for_modem(mmgui_application_t mmguiapp);
void mmgui_main_info_handle_signal_level_change(mmgui_application_t mmguiapp, mmguidevice_t device);
void mmgui_main_info_handle_network_mode_change(mmgui_application_t mmguiapp, mmguidevice_t device);
void mmgui_main_info_handle_network_registration_change(mmgui_application_t mmguiapp, mmguidevice_t device);
void mmgui_main_info_handle_location_change(mmgui_application_t mmguiapp, mmguidevice_t device);
void mmgui_main_info_state_clear(mmgui_application_t mmguiapp);

#endif /* __INFO_PAGE_H__ */
