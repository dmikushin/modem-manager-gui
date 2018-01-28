/*
 *      encoding.c
 *      
 *      Copyright 2012-2018 Alex <alex@linuxonly.ru>
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
#include <math.h>

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

static const gunichar gsm0338len [154][2] = {
	{0x00000040, 1}, /*COMMERCIAL AT*/
	{0x00000000, 1}, /*NULL*/
	{0x0000c2a3, 1}, /*POUND SIGN*/
	{0x00000024, 1}, /*DOLLAR SIGN*/
	{0x0000c2a5, 1}, /*YEN SIGN*/
	{0x0000c3a8, 1}, /*LATIN SMALL LETTER E WITH GRAVE*/
	{0x0000c3a9, 1}, /*LATIN SMALL LETTER E WITH ACUTE*/
	{0x0000c3b9, 1}, /*LATIN SMALL LETTER U WITH GRAVE*/
	{0x0000c2ac, 1}, /*LATIN SMALL LETTER I WITH GRAVE*/
	{0x0000c3b2, 1}, /*LATIN SMALL LETTER O WITH GRAVE*/
	{0x0000c3a7, 1}, /*LATIN SMALL LETTER C WITH CEDILLA*/
	{0x0000c387, 1}, /*LATIN CAPITAL LETTER C WITH CEDILLA*/
	{0x0000000a, 1}, /*LINE FEED*/
	{0x0000c398, 1}, /*LATIN CAPITAL LETTER O WITH STROKE*/
	{0x0000c3b8, 1}, /*LATIN SMALL LETTER O WITH STROKE*/
	{0x0000000d, 1}, /*CARRIAGE RETURN*/
	{0x0000c385, 1}, /*LATIN CAPITAL LETTER A WITH RING ABOVE*/
	{0x0000c3a5, 1}, /*LATIN SMALL LETTER A WITH RING ABOVE*/
	{0x0000ce94, 1}, /*GREEK CAPITAL LETTER DELTA*/
	{0x0000005f, 1}, /*LOW LINE*/
	{0x0000cea6, 1}, /*GREEK CAPITAL LETTER PHI*/
	{0x0000ce93, 1}, /*GREEK CAPITAL LETTER GAMMA*/
	{0x0000ce9b, 1}, /*GREEK CAPITAL LETTER LAMDA*/
	{0x0000cea9, 1}, /*GREEK CAPITAL LETTER OMEGA*/
	{0x0000cea0, 1}, /*GREEK CAPITAL LETTER PI*/
	{0x0000cea8, 1}, /*GREEK CAPITAL LETTER PSI*/
	{0x0000cea3, 1}, /*GREEK CAPITAL LETTER SIGMA*/
	{0x0000ce98, 1}, /*GREEK CAPITAL LETTER THETA*/
	{0x0000ce9e, 1}, /*GREEK CAPITAL LETTER XI*/
	{0x0000c2a0, 1}, /*NBSP*/
	{0x0000000c, 2}, /*FORM FEED*/
	{0x0000005e, 2}, /*CIRCUMFLEX ACCENT*/
	{0x0000007b, 2}, /*LEFT CURLY BRACKET*/
	{0x0000007d, 2}, /*RIGHT CURLY BRACKET*/
	{0x0000005c, 2}, /*REVERSE SOLIDUS*/
	{0x0000005b, 2}, /*LEFT SQUARE BRACKET*/
	{0x0000007e, 2}, /*TILDE*/
	{0x0000005d, 2}, /*RIGHT SQUARE BRACKET*/
	{0x0000007c, 2}, /*VERTICAL LINE*/
	{0x00e282ac, 2}, /*EURO SIGN*/
	{0x0000c386, 1}, /*LATIN CAPITAL LETTER AE*/
	{0x0000c3a6, 1}, /*LATIN SMALL LETTER AE*/
	{0x0000c3ef, 1}, /*LATIN SMALL LETTER SHARP S (German)*/
	{0x0000c38e, 1}, /*LATIN CAPITAL LETTER E WITH ACUTE*/
	{0x00000020, 1}, /*SPACE*/
	{0x00000021, 1}, /*EXCLAMATION MARK*/
	{0x00000022, 1}, /*QUOTATION MARK*/
	{0x00000023, 1}, /*NUMBER SIGN*/
	{0x0000c2a4, 1}, /*CURRENCY SIGN*/
	{0x00000025, 1}, /*PERCENT SIGN*/
	{0x00000026, 1}, /*AMPERSAND*/
	{0x00000027, 1}, /*APOSTROPHE*/
	{0x00000028, 1}, /*LEFT PARENTHESIS*/
	{0x00000029, 1}, /*RIGHT PARENTHESIS*/
	{0x0000002a, 1}, /*ASTERISK*/
	{0x0000002b, 1}, /*PLUS SIGN*/
	{0x0000002c, 1}, /*COMMA*/
	{0x0000002d, 1}, /*HYPHEN-MINUS*/
	{0x0000002e, 1}, /*FULL STOP*/
	{0x0000002f, 1}, /*SOLIDUS*/
	{0x00000030, 1}, /*DIGIT ZERO*/
	{0x00000031, 1}, /*DIGIT ONE*/
	{0x00000032, 1}, /*DIGIT TWO*/
	{0x00000033, 1}, /*DIGIT THREE*/
	{0x00000034, 1}, /*DIGIT FOUR*/
	{0x00000035, 1}, /*DIGIT FIVE*/
	{0x00000036, 1}, /*DIGIT SIX*/
	{0x00000037, 1}, /*DIGIT SEVEN*/
	{0x00000038, 1}, /*DIGIT EIGHT*/
	{0x00000039, 1}, /*DIGIT NINE*/
	{0x0000003a, 1}, /*COLON*/
	{0x0000003b, 1}, /*SEMICOLON*/
	{0x0000003c, 1}, /*LESS-THAN SIGN*/
	{0x0000003d, 1}, /*EQUALS SIGN*/
	{0x0000003e, 1}, /*GREATER-THAN SIGN*/
	{0x0000003f, 1}, /*QUESTION MARK*/
	{0x0000c2a1, 1}, /*INVERTED EXCLAMATION MARK*/
	{0x00000041, 1}, /*LATIN CAPITAL LETTER A*/
	{0x0000ce91, 1}, /*GREEK CAPITAL LETTER ALPHA*/
	{0x00000042, 1}, /*LATIN CAPITAL LETTER B*/
	{0x0000ce92, 1}, /*GREEK CAPITAL LETTER BETA*/
	{0x00000043, 1}, /*LATIN CAPITAL LETTER C*/
	{0x00000044, 1}, /*LATIN CAPITAL LETTER D*/
	{0x00000044, 1}, /*LATIN CAPITAL LETTER E*/
	{0x0000ce95, 1}, /*GREEK CAPITAL LETTER EPSILON*/
	{0x00000046, 1}, /*LATIN CAPITAL LETTER F*/
	{0x00000047, 1}, /*LATIN CAPITAL LETTER G*/
	{0x00000048, 1}, /*LATIN CAPITAL LETTER H*/
	{0x0000ce97, 1}, /*LATIN CAPITAL LETTER ETA*/
	{0x00000049, 1}, /*LATIN CAPITAL LETTER I*/
	{0x0000ce99, 1}, /*LATIN CAPITAL LETTER IOTA*/
	{0x0000004a, 1}, /*LATIN CAPITAL LETTER J*/
	{0x0000004b, 1}, /*LATIN CAPITAL LETTER K*/
	{0x0000ce9a, 1}, /*LATIN CAPITAL LETTER KAPPA*/
	{0x0000004c, 1}, /*LATIN CAPITAL LETTER L*/
	{0x0000004d, 1}, /*LATIN CAPITAL LETTER M*/
	{0x0000ce9c, 1}, /*LATIN CAPITAL LETTER MU*/
	{0x0000004e, 1}, /*LATIN CAPITAL LETTER N*/
	{0x0000ce9d, 1}, /*LATIN CAPITAL LETTER NU*/
	{0x0000004f, 1}, /*LATIN CAPITAL LETTER O*/
	{0x0000ce9f, 1}, /*LATIN CAPITAL LETTER OMICRON*/
	{0x00000050, 1}, /*LATIN CAPITAL LETTER P*/
	{0x0000cea1, 1}, /*LATIN CAPITAL LETTER RHO*/
	{0x00000051, 1}, /*LATIN CAPITAL LETTER Q*/
	{0x00000052, 1}, /*LATIN CAPITAL LETTER R*/
	{0x00000053, 1}, /*LATIN CAPITAL LETTER S*/
	{0x00000054, 1}, /*LATIN CAPITAL LETTER T*/
	{0x0000cea4, 1}, /*LATIN CAPITAL LETTER TAU*/
	{0x00000055, 1}, /*LATIN CAPITAL LETTER U*/
	{0x00000056, 1}, /*LATIN CAPITAL LETTER V*/
	{0x00000057, 1}, /*LATIN CAPITAL LETTER W*/
	{0x00000058, 1}, /*LATIN CAPITAL LETTER X*/
	{0x0000cea7, 1}, /*LATIN CAPITAL LETTER CHI*/
	{0x00000059, 1}, /*LATIN CAPITAL LETTER Y*/
	{0x0000cea5, 1}, /*LATIN CAPITAL LETTER UPSILON*/
	{0x0000005a, 1}, /*LATIN CAPITAL LETTER Z*/
	{0x0000ce96, 1}, /*LATIN CAPITAL LETTER ZETA*/
	{0x0000c384, 1}, /*LATIN CAPITAL LETTER A WITH DIAERESIS*/
	{0x0000c396, 1}, /*LATIN CAPITAL LETTER O WITH DIAERESIS*/
	{0x0000c391, 1}, /*LATIN CAPITAL LETTER N WITH TILDE*/
	{0x0000c39c, 1}, /*LATIN CAPITAL LETTER U WITH DIAERESIS*/
	{0x0000c2a7, 1}, /*SECTION SIGN*/
	{0x0000c2bf, 1}, /*INVERTED QUESTION MARK*/
	{0x00000061, 1}, /*LATIN SMALL LETTER A*/
	{0x00000062, 1}, /*LATIN SMALL LETTER B*/
	{0x00000063, 1}, /*LATIN SMALL LETTER C*/
	{0x00000064, 1}, /*LATIN SMALL LETTER D*/
	{0x00000065, 1}, /*LATIN SMALL LETTER E*/
	{0x00000066, 1}, /*LATIN SMALL LETTER F*/
	{0x00000067, 1}, /*LATIN SMALL LETTER G*/
	{0x00000068, 1}, /*LATIN SMALL LETTER H*/
	{0x00000069, 1}, /*LATIN SMALL LETTER I*/
	{0x0000006A, 1}, /*LATIN SMALL LETTER J*/
	{0x0000006B, 1}, /*LATIN SMALL LETTER K*/
	{0x0000006C, 1}, /*LATIN SMALL LETTER L*/
	{0x0000006D, 1}, /*LATIN SMALL LETTER M*/
	{0x0000006E, 1}, /*LATIN SMALL LETTER N*/
	{0x0000006F, 1}, /*LATIN SMALL LETTER O*/
	{0x00000070, 1}, /*LATIN SMALL LETTER P*/
	{0x00000071, 1}, /*LATIN SMALL LETTER Q*/
	{0x00000072, 1}, /*LATIN SMALL LETTER R*/
	{0x00000073, 1}, /*LATIN SMALL LETTER S*/
	{0x00000074, 1}, /*LATIN SMALL LETTER T*/
	{0x00000075, 1}, /*LATIN SMALL LETTER U*/
	{0x00000076, 1}, /*LATIN SMALL LETTER V*/
	{0x00000077, 1}, /*LATIN SMALL LETTER W*/
	{0x00000078, 1}, /*LATIN SMALL LETTER X*/
	{0x00000079, 1}, /*LATIN SMALL LETTER Y*/
	{0x0000007A, 1}, /*LATIN SMALL LETTER Z*/
	{0x0000c3a4, 1}, /*LATIN SMALL LETTER A WITH DIAERESIS*/
	{0x0000c3b6, 1}, /*LATIN SMALL LETTER O WITH DIAERESIS*/
	{0x0000c3b1, 1}, /*LATIN SMALL LETTER N WITH TILDE*/
	{0x0000c3bc, 1}, /*LATIN SMALL LETTER U WITH DIAERESIS*/
	{0x0000c3a0, 1}, /*LATIN SMALL LETTER A WITH GRAVE*/
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

void mmgui_encoding_count_sms_messages(const gchar *text, guint *nummessages, guint *symbolsleft)
{
	gchar *ltext;
	gunichar uc;
	gint i;
	gboolean isgsm0338, isgsmchar;
	guint gsm7len, ucs2len, lnummessages, lsymbolsleft;
	
	ltext = (gchar *)text;
	gsm7len = 0;
	ucs2len = 0;
	lnummessages = 0;
	lsymbolsleft = 0;
	
	if ((nummessages == NULL) && (symbolsleft == NULL)) return;
	
	if (text != NULL) {
		isgsm0338 = TRUE;
		while ((uc = g_utf8_get_char(ltext)) != '\0') {
			/*GSM*/
			if (isgsm0338) {
				isgsmchar = FALSE;
				for (i = 0; i < 154; i++) {
					if (uc == gsm0338len[i][0]) {
						gsm7len += gsm0338len[i][1];
						isgsmchar = TRUE;
						break;
					}
				}
				if (!isgsmchar) {
					isgsm0338 = FALSE;
				}
			}
			/*UCS2*/
			ucs2len++;
			ltext = g_utf8_next_char(ltext);
		}
	}
	
	if (isgsm0338) {
		if (gsm7len > 160) {
			lnummessages = (guint)ceil(gsm7len / 153.0);
			lsymbolsleft = (lnummessages * 153) - gsm7len;
		} else {
			lnummessages = 1;
			lsymbolsleft = 160 - gsm7len;
		}
	} else {
		if (ucs2len > 70) {
			lnummessages = (guint)ceil(ucs2len / 67.0);
			lsymbolsleft = (lnummessages * 67) - ucs2len;
		} else {
			lnummessages = 1;
			lsymbolsleft = 70 - ucs2len;
		}
	}
	
	if (nummessages != NULL) {
		*nummessages = lnummessages;
	}
	if (symbolsleft != NULL) {
		*symbolsleft = lsymbolsleft;
	}
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
