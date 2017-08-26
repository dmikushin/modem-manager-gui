/*
 *      libpaths.c
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

#include <glib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include "libpaths.h"

#define MMGUI_LIBPATHS_CACHE_FILE         "/etc/ld.so.cache"
#define MMGUI_LIBPATHS_CACHE_PATH_TEMP    "/usr/lib/%s.so"
#define MMGUI_LIBPATHS_LIB_PATH_TEMP_MMR  "%s/%s.so.%i.%i.%i"
#define MMGUI_LIBPATHS_LIB_PATH_TEMP_MM   "%s/%s.so.%i.%i"
#define MMGUI_LIBPATHS_LIB_PATH_TEMP_M    "%s/%s.so.%i"
#define MMGUI_LIBPATHS_LIB_PATH_TEMP      "%s/%s.so"
/*Cache file*/
#define MMGUI_LIBPATHS_LOCAL_CACHE_XDG    ".cache"
#define MMGUI_LIBPATHS_LOCAL_CACHE_DIR    "modem-manager-gui"
#define MMGUI_LIBPATHS_LOCAL_CACHE_FILE   "libpaths.conf"
#define MMGUI_LIBPATHS_LOCAL_CACHE_PERM   0755
#define MMGUI_LIBPATHS_LOCAL_CACHE_VER    3
/*Cache file sections*/
#define MMGUI_LIBPATHS_FILE_ROOT_SECTION  "cache"
#define MMGUI_LIBPATHS_FILE_TIMESTAMP     "timestamp"
#define MMGUI_LIBPATHS_FILE_VERSION       "version"
#define MMGUI_LIBPATHS_FILE_PATH          "path"
#define MMGUI_LIBPATHS_FILE_MAJOR_VER     "majorver"
#define MMGUI_LIBPATHS_FILE_MINOR_VER     "minorver"
#define MMGUI_LIBPATHS_FILE_RELEASE_VER   "releasever"


struct _mmgui_libpaths_entry {
	gchar *id;
	gchar *libpath;
	gint   majorver;
	gint   minorver;
	gint   releasever;
};

typedef struct _mmgui_libpaths_entry *mmgui_libpaths_entry_t;


static gboolean mmgui_libpaths_cache_open_local_cache_file(mmgui_libpaths_cache_t libcache, guint64 dbtimestamp)
{
	const gchar *homepath; 
	gchar *confpath;
	guint64 localtimestamp;
	gint filever;
	GError *error;
	
	if (libcache == NULL) return FALSE;
	
	homepath = g_get_home_dir();
	
	if (homepath == NULL) return FALSE;
	
	confpath = g_build_filename(homepath, MMGUI_LIBPATHS_LOCAL_CACHE_XDG, MMGUI_LIBPATHS_LOCAL_CACHE_DIR, NULL);
	
	if (g_mkdir_with_parents(confpath, MMGUI_LIBPATHS_LOCAL_CACHE_PERM) != 0) {
		g_debug("No write access to program settings directory");
		g_free(confpath);
		return FALSE;
	}
	
	g_free(confpath);
	
	libcache->localfilename = g_build_filename(homepath, MMGUI_LIBPATHS_LOCAL_CACHE_XDG, MMGUI_LIBPATHS_LOCAL_CACHE_DIR, MMGUI_LIBPATHS_LOCAL_CACHE_FILE, NULL);
	
	libcache->localkeyfile = g_key_file_new();
	
	error = NULL;
	
	if (!g_key_file_load_from_file(libcache->localkeyfile, libcache->localfilename, G_KEY_FILE_NONE, &error)) {
		libcache->updatelocal = TRUE;
		g_debug("Local cache file loading error: %s", error->message);
		g_error_free(error);
	} else {
		error = NULL;
		if (g_key_file_has_key(libcache->localkeyfile, MMGUI_LIBPATHS_FILE_ROOT_SECTION, MMGUI_LIBPATHS_FILE_TIMESTAMP, &error)) {
			error = NULL;
			localtimestamp = g_key_file_get_uint64(libcache->localkeyfile, MMGUI_LIBPATHS_FILE_ROOT_SECTION, MMGUI_LIBPATHS_FILE_TIMESTAMP, &error);
			if (error == NULL) {
				if (localtimestamp == dbtimestamp) {
					/*Timestamp is up to date - check version*/
					filever = g_key_file_get_integer(libcache->localkeyfile, MMGUI_LIBPATHS_FILE_ROOT_SECTION, MMGUI_LIBPATHS_FILE_VERSION, &error);
					if (error == NULL) {
						if (filever >= MMGUI_LIBPATHS_LOCAL_CACHE_VER) {
							/*Acceptable version of file*/
							libcache->updatelocal = FALSE;
						} else {
							/*Old version of file*/
							libcache->updatelocal = TRUE;
						}
					} else {
						/*Unknown version of file*/
						libcache->updatelocal = TRUE;
						g_debug("Local cache contain unreadable version identifier: %s", error->message);
						g_error_free(error);
					}
				} else {
					/*Wrong timestamp*/
					libcache->updatelocal = TRUE;
				}
			} else {
				/*Unknown timestamp*/
				libcache->updatelocal = TRUE;
				g_debug("Local cache contain unreadable timestamp: %s", error->message);
				g_error_free(error);
			}
		} else {
			libcache->updatelocal = TRUE;
			g_debug("Local cache does not contain timestamp: %s", error->message);
			g_error_free(error);
		}
	}
	
	return !libcache->updatelocal;
}

static gboolean mmgui_libpaths_cache_close_local_cache_file(mmgui_libpaths_cache_t libcache, gboolean update)
{
	gchar *filedata;
	gsize datasize;
	GError *error;
	
	if (libcache == NULL) return FALSE;
	if ((libcache->localfilename == NULL) || (libcache->localkeyfile == NULL)) return FALSE;
	
	if (update) {
		/*Save timestamp*/
		g_key_file_set_int64(libcache->localkeyfile, MMGUI_LIBPATHS_FILE_ROOT_SECTION, MMGUI_LIBPATHS_FILE_TIMESTAMP, (gint64)libcache->modtime);
		/*Save version of file*/
		g_key_file_set_integer(libcache->localkeyfile, MMGUI_LIBPATHS_FILE_ROOT_SECTION, MMGUI_LIBPATHS_FILE_VERSION, MMGUI_LIBPATHS_LOCAL_CACHE_VER);
		/*Write to file*/
		error = NULL;
		filedata = g_key_file_to_data(libcache->localkeyfile, &datasize, &error);
		if (filedata != NULL) {
			if (!g_file_set_contents(libcache->localfilename, filedata, datasize, &error)) {
				g_debug("No data saved to local cache file file: %s", error->message);
				g_error_free(error);
			}
		}
		g_free(filedata);
	}
	/*Free resources*/
	g_free(libcache->localfilename);
	g_key_file_free(libcache->localkeyfile);
	
	return TRUE;
}

static gboolean mmgui_libpaths_cache_add_to_local_cache_file(mmgui_libpaths_cache_t libcache, mmgui_libpaths_entry_t cachedlib)
{
	if ((libcache == NULL) || (cachedlib == NULL)) return FALSE;
	
	if ((libcache->updatelocal) && (libcache->localkeyfile != NULL)) {
		if (cachedlib->id != NULL) {
			/*Library path*/
			if (cachedlib->libpath != NULL) {
				g_key_file_set_string(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_PATH, cachedlib->libpath);
			}
			/*Library major version*/
			g_key_file_set_integer(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_MAJOR_VER, cachedlib->majorver);
			/*Library minor version*/
			g_key_file_set_integer(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_MINOR_VER, cachedlib->minorver);
			/*Library release version*/
			g_key_file_set_integer(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_RELEASE_VER, cachedlib->releasever);
		}
	}
	
	return TRUE;
}

static gboolean mmgui_libpaths_cache_get_from_local_cache_file(mmgui_libpaths_cache_t libcache, mmgui_libpaths_entry_t cachedlib)
{
	GError *error;
	
	if ((libcache == NULL) || (cachedlib == NULL)) return FALSE;
	
	if ((!libcache->updatelocal) && (libcache->localkeyfile != NULL)) {
		if (cachedlib->id != NULL) {
			/*Library path*/
			error = NULL;
			if (g_key_file_has_key(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_PATH, &error)) {
				error = NULL;
				cachedlib->libpath = g_key_file_get_string(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_PATH, &error);
				if (error != NULL) {
					g_debug("Local cache contain unreadable library path: %s", error->message);
					g_error_free(error);
				}
			} else {
				cachedlib->libpath = NULL;
				if (error != NULL) {
					g_debug("Local cache does not contain library path: %s", error->message);
					g_error_free(error);
				}
			}
			/*Library major version*/
			error = NULL;
			if (g_key_file_has_key(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_MAJOR_VER, &error)) {
				error = NULL;
				cachedlib->majorver = g_key_file_get_integer(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_MAJOR_VER, &error);
				if (error != NULL) {
					g_debug("Local cache contain unreadable library major version: %s", error->message);
					g_error_free(error);
				}
			} else {
				cachedlib->majorver = -1;
				if (error != NULL) {
					g_debug("Local cache does not contain library major version: %s", error->message);
					g_error_free(error);
				}
			}
			/*Library minor version*/
			error = NULL;
			if (g_key_file_has_key(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_MINOR_VER, &error)) {
				error = NULL;
				cachedlib->minorver = g_key_file_get_integer(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_MINOR_VER, &error);
				if (error != NULL) {
					g_debug("Local cache contain unreadable library minor version: %s", error->message);
					g_error_free(error);
				}
			} else {
				cachedlib->minorver = -1;
				if (error != NULL) {
					g_debug("Local cache does not contain library minor version: %s", error->message);
					g_error_free(error);
				}
			}
			/*Library release version*/
			error = NULL;
			if (g_key_file_has_key(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_RELEASE_VER, &error)) {
				error = NULL;
				cachedlib->releasever = g_key_file_get_integer(libcache->localkeyfile, cachedlib->id, MMGUI_LIBPATHS_FILE_RELEASE_VER, &error);
				if (error != NULL) {
					g_debug("Local cache contain unreadable library major version: %s", error->message);
					g_error_free(error);
				}
			} else {
				cachedlib->releasever = -1;
				if (error != NULL) {
					g_debug("Local cache does not contain library major version: %s", error->message);
					g_error_free(error);
				}
			}
		}
	}
	
	return TRUE;
}

static void mmgui_libpaths_cache_destroy_entry(gpointer data)
{
	mmgui_libpaths_entry_t cachedlib;
	
	cachedlib = (mmgui_libpaths_entry_t)data;
	
	if (cachedlib == NULL) return;
	
	if (cachedlib->id != NULL) {
		g_free(cachedlib->id);
	}
	if (cachedlib->libpath != NULL) {
		g_free(cachedlib->libpath);
	}
	cachedlib->majorver = -1;
	cachedlib->minorver = -1;
	cachedlib->releasever = -1;
	g_free(cachedlib);
}

static gboolean mmgui_libpaths_cache_get_entry(mmgui_libpaths_cache_t libcache, const gchar *libpath)
{
	mmgui_libpaths_entry_t cachedlib;
	const gchar versep[2] = ".";
	gchar *pathendptr;
	gchar *path;
	gchar *linktarget;
	gchar *soextptr;
	gchar *name;
	gchar *verptr;
	gchar *version;
	gchar *token;
	gint *verarray[3];
	gint i;
	gboolean mustbecached;
	
	
	if ((libcache == NULL) || (libpath == NULL)) return FALSE;
	
	pathendptr = strrchr(libpath, '/');
	
	if (pathendptr == NULL) {
		return FALSE;
	}
	
	path = g_malloc0(pathendptr - libpath + 1);
	strncpy(path, libpath, pathendptr - libpath);
	
	linktarget = NULL;
	
	if (g_file_test(libpath, G_FILE_TEST_IS_SYMLINK)) {
		linktarget = g_file_read_link(libpath, NULL);
	}
	
	if (linktarget != NULL) {
		soextptr = strstr(linktarget, ".so");
	} else {
		soextptr = strstr(libpath, ".so");
	}
		
	if (soextptr == NULL) {
		if (linktarget != NULL) {
			g_free(linktarget);
		}
		g_free(path);
		return FALSE;
	}
	
	if (linktarget != NULL) {
		name = g_malloc0(soextptr  - linktarget + 1);
		strncpy(name, linktarget, soextptr - linktarget);
	} else {
		name = g_malloc0(soextptr  - pathendptr);
		strncpy(name, pathendptr + 1, soextptr - pathendptr - 1);
	}
	
	cachedlib = (mmgui_libpaths_entry_t)g_hash_table_lookup(libcache->cache, name);
	
	if (cachedlib == NULL) {
		if (linktarget != NULL) {
			g_free(linktarget);
		}
		g_free(path);
		g_free(name);
		return FALSE;
	}
	
	g_free(name);
	
	mustbecached = FALSE;
	
	if (cachedlib->libpath == NULL) {
		cachedlib->libpath = path;
		mustbecached = TRUE;
	} else {
		g_free(path);
	}
	
	verptr = strchr(soextptr + 1, '.');
	
	if (verptr != NULL) {
		version = g_strdup(verptr + 1);
		
		token = strtok(version, versep);
		
		verarray[0] = &cachedlib->majorver;
		verarray[1] = &cachedlib->minorver;
		verarray[2] = &cachedlib->releasever;
		
		i = 0;
		while ((token != NULL) && (i < sizeof(verarray)/sizeof(int *))) {
			*(verarray[i]) = atoi(token);
			token = strtok(NULL, versep);
			i++;
		}
		g_free(version);
		
		mustbecached = TRUE;
	}
	
	if (linktarget != NULL) {
		g_free(linktarget);
	}
	
	if (mustbecached) {
		mmgui_libpaths_cache_add_to_local_cache_file(libcache, cachedlib);
	}
	
	return TRUE;
}

static guint mmgui_libpaths_cache_parse_db(mmgui_libpaths_cache_t libcache)
{
	guint ptr, start, end, entry, entries;
	/*gchar *entryhash, *entryname, *entryext;*/
	
	if (libcache == NULL) return 0;
	
	/*Cache file must be terminated with value 0x00*/
	if (libcache->mapping[libcache->mapsize-1] != 0x00) {
		g_debug("Cache file seems to be non-valid\n");
		return 0;
	}
	
	start = 0;
	end = libcache->mapsize-1;
	entries = 0;
	entry = 0;
		
	for (ptr = libcache->mapsize-1; ptr > 0; ptr--) {
		if (libcache->mapping[ptr] == 0x00) {
			/*String separator - value 0x00*/
			if ((end - ptr) == 1) {
				/*Termination sequence - two values 0x00 0x00*/
				if (start < end) {
					if (libcache->mapping[start] == '/') {
						mmgui_libpaths_cache_get_entry(libcache, libcache->mapping+start);
						entries++;
					}
					entry++;
				}
				break;
			} else {
				/*Regular cache entry*/
				if (start < end) {
					if (libcache->mapping[start] == '/') {
						mmgui_libpaths_cache_get_entry(libcache, libcache->mapping+start);
						entries++;
					}
					entry++;
				}
				/*Set end pointer to string end*/
				end = ptr;
			}
		} else if (isprint(libcache->mapping[ptr])) {
			/*Move start pointer because this value is print symbol*/
			start = ptr;
		}
	}
	
	return entries;
}

mmgui_libpaths_cache_t mmgui_libpaths_cache_new(gchar *libname, ...)
{
	va_list libnames;
	gchar *currentlib;
	struct stat statbuf;
	gboolean localcopy;
	mmgui_libpaths_cache_t libcache;
	mmgui_libpaths_entry_t cachedlib;
		
	if (libname == NULL) return NULL;
	
	libcache = (mmgui_libpaths_cache_t)g_new0(struct _mmgui_libpaths_cache, 1);
	
	if (stat(MMGUI_LIBPATHS_CACHE_FILE, &statbuf) == -1) {
		g_debug("Failed to get library paths cache file size\n");
		g_free(libcache);
		return NULL;
	}
	
	libcache->modtime = statbuf.st_mtime;
	
	localcopy = mmgui_libpaths_cache_open_local_cache_file(libcache, (guint64)libcache->modtime);
	
	if (!localcopy) {
		/*Open system cache*/
		libcache->fd = open(MMGUI_LIBPATHS_CACHE_FILE, O_RDONLY);
		if (libcache->fd == -1) {
			g_debug("Failed to open library paths cache file\n");
			mmgui_libpaths_cache_close_local_cache_file(libcache, FALSE);
			g_free(libcache);
			return NULL;
		}
		/*Memory mapping size*/
		libcache->mapsize = (size_t)statbuf.st_size;
		
		if (libcache->mapsize == 0) {
			g_debug("Failed to map empty library paths cache file\n");
			mmgui_libpaths_cache_close_local_cache_file(libcache, FALSE);
			close(libcache->fd);
			g_free(libcache);
			return NULL;
		}
		/*Map file into memory*/
		libcache->mapping = mmap(NULL, libcache->mapsize, PROT_READ, MAP_PRIVATE, libcache->fd, 0);
		
		if (libcache->mapping == MAP_FAILED) {
			g_debug("Failed to map library paths cache file into memory\n");
			mmgui_libpaths_cache_close_local_cache_file(libcache, FALSE);
			close(libcache->fd);
			g_free(libcache);
			return NULL;
		}
	}
	
	/*When no entry found in cache, form safe name adding .so extension*/
	libcache->safename = NULL;
	
	/*Cache for requested libraries*/
	libcache->cache = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)mmgui_libpaths_cache_destroy_entry);
	
	va_start(libnames, libname);
	
	/*Dont forget about first library name*/
	currentlib = libname;
	
	do {
		/*Allocate structure*/
		cachedlib = (mmgui_libpaths_entry_t)g_new0(struct _mmgui_libpaths_entry, 1);
		cachedlib->id = g_strdup(currentlib);
		cachedlib->libpath = NULL;
		cachedlib->majorver = -1;
		cachedlib->minorver = -1;
		cachedlib->releasever = -1;
		g_hash_table_insert(libcache->cache, cachedlib->id, cachedlib);
		/*If available, get from local cache*/
		if (localcopy) {
			mmgui_libpaths_cache_get_from_local_cache_file(libcache, cachedlib);
		}
		/*Next library name*/
		currentlib = va_arg(libnames, gchar *);
	} while (currentlib != NULL);
	
	va_end(libnames);
	
	if (!localcopy) {
		/*Parse system database*/
		mmgui_libpaths_cache_parse_db(libcache);
		/*Save local cache*/
		mmgui_libpaths_cache_close_local_cache_file(libcache, TRUE);
	} else {
		/*Close used cache file*/
		mmgui_libpaths_cache_close_local_cache_file(libcache, FALSE);
	}
	
	return libcache;
}

void mmgui_libpaths_cache_close(mmgui_libpaths_cache_t libcache)
{
	if (libcache == NULL) return;
	
	if (libcache->safename != NULL) {
		g_free(libcache->safename);
	}
	
	g_hash_table_destroy(libcache->cache);
	
	munmap(libcache->mapping, libcache->mapsize);
	
	g_free(libcache);
}

gchar *mmgui_libpaths_cache_get_library_full_path(mmgui_libpaths_cache_t libcache, gchar *libname)
{
	mmgui_libpaths_entry_t cachedlib;
	
	if ((libcache == NULL) || (libname == NULL)) return NULL;
	
	cachedlib = (mmgui_libpaths_entry_t)g_hash_table_lookup(libcache->cache, libname);
	
	if (cachedlib != NULL) {
		if (cachedlib->libpath != NULL) {
			/*Safe library path*/
			if (libcache->safename != NULL) {
				g_free(libcache->safename);
			}
			/*Handle version variations*/
			if ((cachedlib->majorver != -1) && (cachedlib->minorver != -1) && (cachedlib->releasever != -1)) {
				libcache->safename = g_strdup_printf(MMGUI_LIBPATHS_LIB_PATH_TEMP_MMR, cachedlib->libpath, libname, cachedlib->majorver, cachedlib->minorver, cachedlib->releasever);
			} else if ((cachedlib->majorver != -1) && (cachedlib->minorver != -1)) {
				libcache->safename = g_strdup_printf(MMGUI_LIBPATHS_LIB_PATH_TEMP_MM, cachedlib->libpath, libname, cachedlib->majorver, cachedlib->minorver);
			} else if (cachedlib->majorver != -1) {
				libcache->safename = g_strdup_printf(MMGUI_LIBPATHS_LIB_PATH_TEMP_M, cachedlib->libpath, libname, cachedlib->majorver);
			} else {
				libcache->safename = g_strdup_printf(MMGUI_LIBPATHS_LIB_PATH_TEMP, cachedlib->libpath, libname);
			}
			
			return libcache->safename;
		} else {
			/*Safe library path*/
			if (libcache->safename != NULL) {
				g_free(libcache->safename);
			}
			libcache->safename = g_strdup_printf(MMGUI_LIBPATHS_CACHE_PATH_TEMP, libname);
			return libcache->safename;
		}
	} else {
		/*Safe library path*/
		if (libcache->safename != NULL) {
			g_free(libcache->safename);
		}
		libcache->safename = g_strdup_printf(MMGUI_LIBPATHS_CACHE_PATH_TEMP, libname);
		return libcache->safename;
	}
}

gboolean mmgui_libpaths_cache_check_library_version(mmgui_libpaths_cache_t libcache, gchar *libname, gint major, gint minor, gint release)
{
	mmgui_libpaths_entry_t cachedlib;
	gboolean compatiblever;
	
	if ((libcache == NULL) || (libname == NULL) || (major == -1)) return FALSE;
	
	cachedlib = (mmgui_libpaths_entry_t)g_hash_table_lookup(libcache->cache, libname);
	
	if (cachedlib == NULL) return FALSE;
	if (cachedlib->majorver == -1) return FALSE;
	
	compatiblever = (cachedlib->majorver >= major);
	
	if ((cachedlib->majorver == major) && (minor != -1) && (cachedlib->minorver != -1)) {
		compatiblever = compatiblever && (cachedlib->minorver >= minor);
		if ((cachedlib->minorver == minor) && (release != -1) && (cachedlib->releasever != -1)) {
			compatiblever = compatiblever && (cachedlib->releasever >= release); 
		}
	}
		
	return compatiblever;
}
