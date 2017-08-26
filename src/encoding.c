/*
 *      encoding.c
 *      
 *      Copyright 2012-2013 Alex <alex@linuxonly.ru>
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
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>

static const guint gsm7_utf8_table [128] = {
	0x0040, 0xc2a3, 0x0024, 0xc2a5, 0xc3a8, 0xc3a9, 0xc3b9, 0xc3ac, 0xc3b2, 0xc387,
	0x000a, 0xc398, 0xc3b8, 0x000d, 0xc385, 0xc3a5, 0xce94, 0x005f, 0xcea6, 0xce93,
	0xce9b, 0xcea9, 0xcea0, 0xcea8, 0xcea3, 0xce98, 0xce9e, 0x00a0, 0xc386, 0xc3a6,
	0xc39f, 0xc389, 0x0020, 0x0021, 0x0022, 0x0023, 0xc2a4, 0x0025, 0x0026, 0x0027,
	0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f, 0x0030, 0x0031,
	0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b,
	0x003c, 0x003d, 0x003e, 0x003f, 0xc2a1, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045,
	0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
	0x005a, 0xc384, 0xc396, 0xc391, 0xc39c, 0xc2a7, 0xc2bf, 0x0061, 0x0062, 0x0063,
	0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d,
	0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007a, 0xc3a4, 0xc3b6, 0xc3b1, 0xc3bc, 0xc3a0
};

static const guint gsm7_utf8_ext_table [2][10] = {
	{0x00000c, 0x00005e, 0x00007b, 0x00007d, 0x00005c, 0x00005b, 0x00007e, 0x00005d, 0x00007c, 0xe282ac},
	{    0x0a,     0x14,     0x28,     0x29,     0x2f,     0x3c,     0x3d,     0x3e,     0x40,     0x65}
};

static const gchar hextable[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};


static guint hex_to_dec(const guchar *input, gsize number);


static guint hex_to_dec(const guchar *input, gsize number)
{
	guint  k, b, value;
	gint hexptr;
	
	if ((input == NULL) || ((input != NULL) && (input[0] == '\0')) || (number == 0)) return 0;
	
	value = 0;
	k = 1;
	
	for (hexptr = (number-1); hexptr >= 0; hexptr--) {
		switch (input[hexptr]) {
			case '0': b = 0; break;
			case '1': b = 1; break;
			case '2': b = 2; break;
			case '3': b = 3; break;
			case '4': b = 4; break;
			case '5': b = 5; break;
			case '6': b = 6; break;
			case '7': b = 7; break;
			case '8': b = 8; break;
			case '9': b = 9; break;
			case 'a':
			case 'A': b = 10; break;
			case 'b':
			case 'B': b = 11; break;
			case 'c':
			case 'C': b = 12; break;
			case 'd':
			case 'D': b = 13; break;
			case 'e':
			case 'E': b = 14; break;
			case 'f':
			case 'F': b = 15; break;
			default: b = 0; break;
		}
		value = value + b*k;
		k *= 16;
	}
	
	
	return value;
}

guchar *utf8_to_ucs2(const guchar *input, gsize ilength, gsize *olength)
{
	guchar *output, *routput;
	guint iptr, optr;
	gushort value;
		
	if ((input == NULL) || (ilength == 0) || (olength == NULL)) return NULL;
	
	if (input[0] == '\0') return NULL;
	
	output = g_malloc0(ilength*2+1);
	
	if (output == NULL) return NULL;
	
	iptr = 0; optr = 0;
	
	while (iptr < ilength) {
		if (input[iptr] < 0x80) {
			value = input[iptr];
			output[optr] = '0';
			output[optr+1] = '0';
			output[optr+2] = hextable[(guchar)(value & 0xff)/16];
			output[optr+3] = hextable[(guchar)(value & 0xff)%16];
			iptr += 1;
			optr += 4;
		}
		
		if ((input[iptr] & 0xE0) == 0xE0) {
			if (!((input[iptr+1] == 0) || (input[iptr+2] == 0))) {
				value = ((input[iptr] & 0x0F) << 12) | ((input[iptr+1] & 0x3F) << 6) | (input[iptr+2] & 0x3F);
				output[optr] = hextable[(guchar)((value >> 8) & 0xff)/16];
				output[optr+1] = hextable[(guchar)((value >> 8) & 0xff)%16];
				output[optr+2] = hextable[(guchar)(value & 0xff)/16];
				output[optr+3] = hextable[(guchar)(value & 0xff)%16];
				optr += 4;
			}
			
			iptr += 3;
		}
		
		if ((input[0] & 0xC0) == 0xC0) {
			if (input[1] != 0) {
				value = ((input[iptr] & 0x1F) << 6) | (input[iptr+1] & 0x3F);
				output[optr] = hextable[(guchar)((value >> 8) & 0xff)/16];
				output[optr+1] = hextable[(guchar)((value >> 8) & 0xff)%16];
				output[optr+2] = hextable[(guchar)(value & 0xff)/16];
				output[optr+3] = hextable[(guchar)(value & 0xff)%16];
				optr += 4;
			}
			
			iptr += 2;
		}
	}
	
	output[optr] = '\0';
	
	routput = g_realloc(output, optr+1);
	
	if (routput != NULL) output = routput;
	
	*olength = optr;
	
	return output;
}

guchar *ucs2_to_utf8(const guchar *input, gsize ilength, gsize *olength)
{
	guchar *output, *routput;
	guint iptr, optr;
	guint value;
	
	if ((input == NULL) || (ilength == 0) || (olength == NULL)) return NULL;
	
	if ((input[0] == '\0') || (ilength%4 != 0)) return NULL;
	
	output = g_malloc0(ilength*2+1);
	
	iptr = 0; optr = 0;
	
	while (iptr < ilength) {
		value = hex_to_dec(input+iptr, 4);
		
		if (value < 0x80) {
			if ((value <= 0x20) && (value != 0x0A) && (value != 0x0D)) {
				output[optr] = 0x20;
			} else {
				output[optr] = value;
			}
			optr += 1;
		}
		
		if ((value >= 0x80) && (value < 0x800)) {
			output[optr] = (value >> 6) | 0xC0;
			output[optr+1] = (value & 0x3F) | 0x80;
			optr += 2;
		}
		
		if ((value >= 0x800) && (value < 0xFFFF)) {
			output[optr] = ((value >> 12)) | 0xE0;
			output[optr+1] = ((value >> 6) & 0x3F) | 0x80;
			output[optr+2] = ((value) & 0x3F) | 0x80;
			optr += 3;
		}
		
		iptr += 4;
	}
	
	output[optr] = '\0';
	
	routput = g_realloc(output, optr+1);
	
	if (routput != NULL) output = routput;
	
	*olength = optr;
	
	return output;
} 

guchar *utf8_to_gsm7(const guchar *input, gsize ilength, gsize *olength)
{
	guchar *output, *routput;
	guint iptr, optr;
	gushort x, value;
	
	if ((input == NULL) || (ilength == 0) || (olength == NULL)) return NULL;
	
	output = g_malloc0(ilength*2+1);
	
	if (output == NULL) return NULL;
	
	iptr = 0; optr = 0;
	
	while (iptr < ilength) {
		x = (iptr % 8) + 1;
		if (x < 8) {
			if ((iptr + 1) == ilength) {
				value = (input[iptr] >> (iptr % 8)) & 0xff;
				output[optr] = hextable[(guchar)(value & 0xff)/16];
				output[optr+1] = hextable[(guchar)(value & 0xff)%16];
				optr += 2;
			} else {
				value = (((input[iptr] >> (x - 1)) | (input[iptr+1] << (8 - x))) & 0xff) & 0xff;
				output[optr] = hextable[(guchar)(value & 0xff)/16];
				output[optr+1] = hextable[(guchar)(value & 0xff)%16];
				optr += 2;
			}
		}
		iptr++;
	}
	
	output[optr] = '\0';
	
	routput = g_realloc(output, optr+1);
	
	if (routput != NULL) output = routput;
	
	*olength = optr;
	
	return output;
}

guchar *gsm7_to_utf8(const guchar *input, gsize ilength, gsize *olength)
{
	guchar *output, *routput;
	guint iptr, optr;
	guint value, current, mask, next, left;
	
	if ((input == NULL) || (ilength == 0) || (olength == NULL)) return NULL;
	
	if ((input[0] == '\0') || (ilength%2 != 0)) return NULL;
	
	output = g_malloc0(ilength*4+1);
	
	if (output == NULL) return NULL;
	
	left = 7;
	mask = 0x7F;
	next = 0;
	
	iptr = 0; optr = 0;
	
	while (iptr < ilength) {
		if (mask != 0) {
			value = hex_to_dec(input+iptr, 2);
			current = (((value & mask) << (7 - left)) | next);
			next = (value & (~mask)) >> left;
			output[optr] = current;
			optr += 1;
			mask >>= 1;
			left -= 1;
			iptr += 2;
		} else {
			output[optr] = next;
			optr += 1;
			left = 7;
			mask = 0x7F;
			next = 0;
		}
	}
	
	output[optr] = '\0';
	
	routput = g_realloc(output, optr+1);
	
	if (routput != NULL) output = routput;
	
	*olength = optr;
	
	return output;
}

guchar *utf8_map_gsm7(const guchar *input, gsize ilength, gsize *olength)
{
	guchar *output, *routput;
	guint iptr, optr;
	guint value;
	guint i;
	gboolean detected, found;
	
	if ((input == NULL) || (ilength == 0) || (olength == NULL)) return NULL;
	
	if (input[0] == '\0') return NULL;
	
	output = g_malloc0(ilength*2+1);
	
	if (output == NULL) return NULL;
	
	iptr = 0; optr = 0;
	
	while (iptr < ilength) {
		detected = FALSE;
		if (input[iptr] <= 127) {
			value = input[iptr];
			detected = TRUE;
			iptr += 1;
		} else if ((input[iptr] >= 194) && (input[iptr] <= 223)) {
			value = (((input[iptr] << 8) & 0xff00) | (input[iptr+1])) & 0xffff;
			detected = TRUE;
			iptr += 2;
		} else if ((input[iptr] >= 224) && (input[iptr] <= 239)) {
			value = ((((input[iptr] << 16) & 0xff0000) | ((input[iptr+1] << 8) & 0x00ff00)) | (input[iptr+2])) & 0xffffff;
			detected = TRUE;
			iptr += 3;
		}  else if ((input[iptr] >= 240) && (input[iptr] <= 244)) {
			value = (((((input[iptr] << 24) & 0xff000000) | ((input[iptr+1] << 16) & 0x00ff0000)) | ((input[iptr+2] << 8) & 0x0000ff00)) | (input[iptr+3])) & 0xffffffff;
			detected = TRUE;
			iptr += 4;
		} 
		
		if (detected) {
			found = FALSE;
			for (i=0; i<10; i++) {
				if (gsm7_utf8_ext_table[0][i] == value) {
					output[optr] = 0x1b;
					output[optr+1] = (unsigned char)gsm7_utf8_ext_table[1][i];
					optr += 2;
					found = TRUE;
				}
			}
			
			if (!found) {
				for (i=0; i<128; i++) {
					if (gsm7_utf8_table[i] == value) {
						output[optr] = (unsigned char)i;
						optr += 1;
						found = TRUE;
					}
				}
			}
			
			if (!found) {
				output[optr] = 0x3f;
				optr += 1;
			}
		}
		
	}
	
	output[optr] = '\0';
	
	routput = g_realloc(output, optr+1);
	
	if (routput != NULL) output = routput;
	
	*olength = optr;
	
	return output;
}

gchar *encoding_ussd_gsm7_to_ucs2(gchar *srcstr)
{
	gchar *decstr1, *decstr2;
	gsize strsize;
	gboolean srcstrvalid;
	
	if (srcstr == NULL) return NULL;
	
	decstr1 = g_strdup(srcstr);
	strsize = strlen(srcstr);
	
	srcstrvalid = g_utf8_validate((const gchar *)srcstr, -1, (const gchar **)NULL);
	
	if (strsize > 0) {
		/*Map UTF8 symbols using GSM7 table*/
		decstr2 = (gchar *)utf8_map_gsm7((const guchar *)decstr1, strsize, &strsize);
		if (decstr2 != NULL) {
			/*Free temporary string and go next step*/
			g_free(decstr1);
		} else {
			/*Return undecoded hash*/
			return decstr1;
		}
		/*Translate UTF8 to GSM7*/
		decstr1 = (gchar *)utf8_to_gsm7((const guchar *)decstr2, strsize, &strsize);
		if (decstr1 != NULL) {
			/*Free temporary string and go next step*/
			g_free(decstr2);
		} else {
			/*String not decoded*/
			if (srcstrvalid) {
				/*Return valid source string*/
				g_free(decstr2);
				return g_strdup(srcstr);
			} else {
				/*Return undecoded hash*/
				return decstr2;
			}
		}
		/*Translate UCS2 to UTF8*/	
		decstr2 = (gchar *)ucs2_to_utf8((const guchar *)decstr1, strsize, &strsize);
		if (decstr2 != NULL) {
			if (g_utf8_validate((const gchar *)decstr2, -1, (const gchar **)NULL)) {
				/*Decoded string validated*/
				g_free(decstr1);
				return decstr2;
			} else {
				/*Decoded string not validated*/
				g_free(decstr2);
				if (srcstrvalid) {
					/*Return valid source string*/
					g_free(decstr1);
					return g_strdup(srcstr);
				} else {
					/*Return undecoded hash*/
					return decstr1;
				}
			}
		} else {
			/*String not decoded*/
			if (srcstrvalid) {
				/*Return valid source string*/
				g_free(decstr1);
				return g_strdup(srcstr);
			} else {
				/*Return undecoded hash*/
				return decstr1;
			}
		}
	}
	
	return decstr1;
}

guchar *bcd_to_utf8_ascii_part(const guchar *input, gsize ilength, gsize *olength)
{
	guchar *output, *routput;
	guint iptr, optr;
	guchar value;
	guchar buf[4];
	
	if ((input == NULL) || (ilength == 0) || (olength == NULL)) return NULL;
	
	if (input[0] == '\0') return NULL;
	
	//Test if number decoded correctly
	for (iptr=0; iptr<ilength; iptr++) {
		value = tolower(input[iptr]);
		if (((!isdigit(value)) && (value != 'a') && (value != 'b') && (value != 'c') && (value != '*') && (value != '#')) || (ilength <= 6)) {
			output = (guchar *)g_strdup((const gchar *)input);
			*olength = ilength;
			return output;
		}
	}
	
	output = g_malloc0(ilength);
	
	if (output == NULL) return NULL;
	
	iptr = 0;
	optr = 0;
	
	//Decode characters in range 32 ... 127
	while (iptr < ilength) {
		memset(buf, 0, sizeof(buf)); 
		if (isdigit(input[iptr])) {
			if ((input[iptr] == '1') && (ilength - iptr >= 3))  {
				strncpy((char *)buf, (const char *)input+iptr, 3);
				value = (guchar)atoi((const char *)buf);
				if (value <= 127) {
					output[optr] = value;
					optr++;
				}
				iptr += 3;
			} else if (ilength - iptr >= 2) {
				strncpy((char *)buf, (const char *)input+iptr, 2);
				value = (guchar)atoi((const char *)buf);
				if (value >= 32) {
					output[optr] = value;
					optr++;
				}
				iptr += 2;
			} else {
				output[optr] = '?';
				optr++;
				iptr++;
			}
		} else {
			break;
		}
	}
	
	output[optr] = '\0';
	
	routput = g_realloc(output, optr+1);
	
	if (routput != NULL) output = routput;
	
	*olength = optr;
	
	return output;
}

gchar *encoding_unescape_xml_markup(const gchar *srcstr, gsize srclen)
{
	guint iptr, optr, newlen, charleft;
	gchar *unescaped;
	
	/*XML escape characters:
	 "&lt;", "&gt;", "&amp;", "&quot;", "&apos;", "&#xD;", "&#x9;", "&#xA;"
	 '<',    '>',    '&',     '\"',     '\'',     '\r',    '\t',    '\n'
	*/
	
	if ((srcstr == NULL) || (srclen == 0)) return NULL;
	
	iptr = 0;
	newlen = 0;
	
	while (iptr < srclen) {
		if (srcstr[iptr] == '&') {
			charleft = srclen - iptr - 1;
			if (charleft >= 3) {
				if ((charleft >= 5) && (srcstr[iptr+1] == 'q') && (srcstr[iptr+2] == 'u') && (srcstr[iptr+3] == 'o') && (srcstr[iptr+4] == 't') && (srcstr[iptr+5] == ';')) {
					newlen += 1;
					iptr += 6;
				} else if ((charleft >= 5) && (srcstr[iptr+1] == 'a') && (srcstr[iptr+2] == 'p') && (srcstr[iptr+3] == 'o') && (srcstr[iptr+4] == 's') && (srcstr[iptr+5] == ';')) {
					newlen += 1;
					iptr += 6;
				} else if ((charleft >= 4) && (srcstr[iptr+1] == 'a') && (srcstr[iptr+2] == 'm') && (srcstr[iptr+3] == 'p') && (srcstr[iptr+4] == ';')) {
					newlen += 1;
					iptr += 5;
				} else if ((charleft >= 4) && (srcstr[iptr+1] == '#') && (srcstr[iptr+2] == 'x') && (srcstr[iptr+3] == 'D') && (srcstr[iptr+4] == ';')) {
					newlen += 1;
					iptr += 5;
				} else if ((charleft >= 4) && (srcstr[iptr+1] == '#') && (srcstr[iptr+2] == 'x') && (srcstr[iptr+3] == '9') && (srcstr[iptr+4] == ';')) {
					newlen += 1;
					iptr += 5;
				} else if ((charleft >= 4) && (srcstr[iptr+1] == '#') && (srcstr[iptr+2] == 'x') && (srcstr[iptr+3] == 'A') && (srcstr[iptr+4] == ';')) {
					newlen += 1;
					iptr += 5;
				} else if ((charleft >= 3) && (srcstr[iptr+1] == 'l') && (srcstr[iptr+2] == 't') && (srcstr[iptr+3] == ';')) {
					newlen += 1;
					iptr += 4;
				} else if ((charleft >= 3) && (srcstr[iptr+1] == 'g') && (srcstr[iptr+2] == 't') && (srcstr[iptr+3] == ';')) {
					newlen += 1;
					iptr += 4;
				} else {
					newlen += 1;
					iptr += 1;
				}
			} else {
				newlen += 1;
				iptr += 1;
			}
		} else {
			newlen += 1;
			iptr += 1;
		}
	}
	
	unescaped = g_malloc0(newlen+1);
	
	iptr = 0;
	optr = 0;
	newlen = 0;
	
	while (iptr < srclen) {
		if (srcstr[iptr] == '&') {
			charleft = srclen - iptr - 1;
			if (charleft >= 3) {
				if ((charleft >= 5) && (srcstr[iptr+1] == 'q') && (srcstr[iptr+2] == 'u') && (srcstr[iptr+3] == 'o') && (srcstr[iptr+4] == 't') && (srcstr[iptr+5] == ';')) {
					unescaped[optr] = '\"';
					optr += 1;
					iptr += 6;
				} else if ((charleft >= 5) && (srcstr[iptr+1] == 'a') && (srcstr[iptr+2] == 'p') && (srcstr[iptr+3] == 'o') && (srcstr[iptr+4] == 's') && (srcstr[iptr+5] == ';')) {
					unescaped[optr] = '\'';
					optr += 1;
					iptr += 6;
				} else if ((charleft >= 4) && (srcstr[iptr+1] == 'a') && (srcstr[iptr+2] == 'm') && (srcstr[iptr+3] == 'p') && (srcstr[iptr+4] == ';')) {
					unescaped[optr] = '&';
					optr += 1;
					iptr += 5;
				} else if ((charleft >= 4) && (srcstr[iptr+1] == '#') && (srcstr[iptr+2] == 'x') && (srcstr[iptr+3] == 'D') && (srcstr[iptr+4] == ';')) {
					unescaped[optr] = '\r';
					optr += 1;
					iptr += 5;
				} else if ((charleft >= 4) && (srcstr[iptr+1] == '#') && (srcstr[iptr+2] == 'x') && (srcstr[iptr+3] == '9') && (srcstr[iptr+4] == ';')) {
					unescaped[optr] = '\t';
					optr += 1;
					iptr += 5;
				} else if ((charleft >= 4) && (srcstr[iptr+1] == '#') && (srcstr[iptr+2] == 'x') && (srcstr[iptr+3] == 'A') && (srcstr[iptr+4] == ';')) {
					unescaped[optr] = '\n';
					optr += 1;
					iptr += 5;
				} else if ((charleft >= 3) && (srcstr[iptr+1] == 'l') && (srcstr[iptr+2] == 't') && (srcstr[iptr+3] == ';')) {
					unescaped[optr] = '<';
					optr += 1;
					iptr += 4;
				} else if ((charleft >= 3) && (srcstr[iptr+1] == 'g') && (srcstr[iptr+2] == 't') && (srcstr[iptr+3] == ';')) {
					unescaped[optr] = '>';
					optr += 1;
					iptr += 4;
				} else {
					unescaped[optr] = srcstr[iptr];
					optr += 1;
					iptr += 1;
				}
				
			} else {
				unescaped[optr] = srcstr[iptr];
				optr += 1;
				iptr += 1;
			}
		} else {
			unescaped[optr] = srcstr[iptr];
			optr += 1;
			iptr += 1;
		}
	}
	
	return unescaped;
}

gchar *encoding_clear_special_symbols(gchar *srcstr, gsize srclen)
{
	guint iptr;
	
	if ((srcstr == NULL) || (srclen == 0)) return NULL;
	
	iptr = 0;
	
	while (iptr < srclen) {
		if (srcstr[iptr] > 0) {
			if ((srcstr[iptr] == '\n') || (srcstr[iptr] == '\r') || (srcstr[iptr] == '\t')) {
				srcstr[iptr] = ' ';
			}
			iptr += 1;
		} else {
			switch (srcstr[iptr] & 0xF0) {
				case 0xE0:
					iptr += 3;
					break;
				case 0xF0:
					iptr += 4;
					break;
				default:
					iptr += 2;
					break;
			}
		}
	}
	
	return srcstr;
}
