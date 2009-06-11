#include "stdafx.h"

#include "base64decode.h"
#include "StrUtil.h"
/*
Based on http://base64.sourceforge.net/b64.c:
MODULE NAME:    b64.c

AUTHOR:         Bob Trower 08/04/01

PROJECT:        Crypt Data Packaging

COPYRIGHT:      Copyright (c) Trantor Standard Systems Inc., 2001

NOTE:           This source code may be used as you wish, subject to
                the MIT license.  See the LICENCE section below.
*/

static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

static void decodeblock( unsigned char in[4], unsigned char out[3] )
{   
    out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
    out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
    out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}

static void predecode(unsigned char *s)
{
	while (*s) {
		char v = *s;
		v = (unsigned char) ((v < 43 || v > 122) ? 0 : cd64[ v - 43 ]);
		if (v)
			v = (unsigned char) ((v == '$') ? 0 : v - 61);
		if (v)
			*s++ = v - 1;
		else
			*s++ = v;
	}
}

// Note: s will be modifed
char *b64decode(unsigned char *s)
{
	if (strempty((char*)s))
		return NULL;
	size_t slen = strlen((char*)s);
	if (slen % 4 != 0)
		return NULL;
	int rounds = slen / 4;
	int decodedLen = rounds * 3;
	unsigned char *res = (unsigned char*)malloc(decodedLen+1);
	unsigned char *tmp = res;
	predecode(s);
	while (rounds > 0) {
		decodeblock(s, tmp);
		s += 4;
		tmp += 3;
		--rounds;
	}
	*tmp = 0;
	return (char*)res;
}

char *b64decode(char *s)
{
	return b64decode((unsigned char*)s);
}


