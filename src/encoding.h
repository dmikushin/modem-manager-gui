/*
 *      encoding.h
 *      
 *      Copyright 2012 Alex <alex@linuxonly.ru>
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

#ifndef __ENCODING_H__
#define __ENCODING_H__

#include <glib.h>

guchar *utf8_to_ucs2(const guchar *input, gsize ilength, gsize *olength);
guchar *ucs2_to_utf8(const guchar *input, gsize ilength, gsize *olength);
guchar *utf8_to_gsm7(const guchar *input, gsize ilength, gsize *olength);
guchar *gsm7_to_utf8(const guchar *input, gsize ilength, gsize *olength);
guchar *utf8_map_gsm7(const guchar *input, gsize ilength, gsize *olength);
gchar *encoding_ussd_gsm7_to_ucs2(gchar *srcstr);
guchar *bcd_to_utf8_ascii_part(const guchar *input, gsize ilength, gsize *olength);
gchar *encoding_unescape_xml_markup(const gchar *srcstr, gsize srclen);
gchar *encoding_clear_special_symbols(gchar *srcstr, gsize srclen);
#endif /* __ENCODING_H__ */
