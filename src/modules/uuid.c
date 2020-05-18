/*
 *      uuid.c
 *      
 *      Copyright 2017 Alex <alex@linuxonly.ru>
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
#include <string.h>

GRand *mmgui_uuid_init(void)
{
	return g_rand_new_with_seed(g_get_real_time() / 1000);
}

gchar *mmgui_uuid_generate(GRand *rng)
{
	guint symseq, symval;
	gchar uuidbuf[37];
	const gchar uuidtemplate[] = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
	const gchar uuidvalues[] = "0123456789abcdef";
	
	if (rng == NULL) return NULL;
	
	memset(uuidbuf, 0, sizeof(uuidbuf));
	
	for (symseq = 0; symseq < sizeof(uuidtemplate); symseq++) {
		symval = g_rand_int_range(rng, 0, 32767) % 16;
		switch (uuidtemplate[symseq]) {
			case 'x':
				uuidbuf[symseq] = uuidvalues[symval];
				break;
			case 'y':
				uuidbuf[symseq] = uuidvalues[(symval & 0x03) | 0x08];
				break;
			default:
				uuidbuf[symseq] = uuidtemplate[symseq];
				break;
		}
	}
	
	return g_strdup(uuidbuf);
}
