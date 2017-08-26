/*
 *      libpaths.h
 *      
 *      Copyright 2013 Alex <alex@linuxonly.ru>
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

#ifndef __LIBPATHS_H__
#define __LIBPATHS_H__

#include <glib.h>

struct _mmgui_libpaths_cache {
	gint fd;
	gchar *mapping;
	gsize mapsize;
	time_t modtime;
	GHashTable *cache;
	gchar *safename;
	/*Local cache*/
	gchar *localfilename;
	gboolean updatelocal;
	GKeyFile *localkeyfile;
};

typedef struct _mmgui_libpaths_cache *mmgui_libpaths_cache_t;

mmgui_libpaths_cache_t mmgui_libpaths_cache_new(gchar *libname, ...);
void mmgui_libpaths_cache_close(mmgui_libpaths_cache_t libcache);
gchar *mmgui_libpaths_cache_get_library_full_path(mmgui_libpaths_cache_t libcache, gchar *libname);
gboolean mmgui_libpaths_cache_check_library_version(mmgui_libpaths_cache_t libcache, gchar *libname, gint major, gint minor, gint release);

#endif /* __LIBPATHS_H__ */
