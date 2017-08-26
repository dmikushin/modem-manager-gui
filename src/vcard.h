/*
 *      vcard.h
 *      
 *      Copyright 2014 Alex <alex@linuxonly.ru>
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

#include <glib.h>

#ifndef __VCARD_H__
#define __VCARD_H__

gint vcard_parse_string(const gchar *string, GSList **contacts, gchar *group);
gint vcard_parse_list(GSList *vcardrows, GSList **contacts, gchar *group);

#endif /* __VCARD_H__ */
