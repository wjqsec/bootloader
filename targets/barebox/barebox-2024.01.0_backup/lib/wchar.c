/*
 * wchar.c - wide character support
 *
 * Copyright (c) 2014 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <wchar.h>
#include <malloc.h>
#include <string.h>

size_t wcslen(const wchar_t *s)
{
	size_t len = 0;

	while (*s++)
		len++;

	return len;
}

size_t wcsnlen(const wchar_t * s, size_t count)
{
	const wchar_t *sc;

	for (sc = s; count-- && *sc != L'\0'; ++sc)
		/* nothing */;
	return sc - s;
}

wchar_t *strdup_wchar(const wchar_t *src)
{
	int len = wcslen(src);
	wchar_t *tmp, *dst;

	if (!(dst = malloc((len + 1) * sizeof(wchar_t))))
		return NULL;

	tmp = dst;

	while ((*dst++ = *src++))
		/* nothing */;

	return tmp;
}

int mbtowc(wchar_t *pwc, const char *s, size_t n)
{
	if (!s)
		return 0; /* we don't mantain a non-trivial shift state */

	if (n < 1)
		return -1;

	*pwc = *s;
	return 1;
}

int wctomb(char *s, wchar_t wc)
{
	*s = wc & 0xFF;
	return 1;
}

char *strcpy_wchar_to_char(char *dst, const wchar_t *src)
{
	char *ret = dst;

	while (*src)
		wctomb(dst++, *src++);

	*dst = 0;

	return ret;
}

wchar_t *strcpy_char_to_wchar(wchar_t *dst, const char *src)
{
	wchar_t *ret = dst;

	while (*src)
		mbtowc(dst++, src++, 1);

	*dst = 0;

	return ret;
}

wchar_t *strdup_char_to_wchar(const char *src)
{
	wchar_t *dst = malloc((strlen(src) + 1) * sizeof(wchar_t));

	if (!dst)
		return NULL;

	strcpy_char_to_wchar(dst, src);

	return dst;
}

char *strdup_wchar_to_char(const wchar_t *src)
{
	char *dst = malloc((wcslen(src) + 1));

	if (!dst)
		return NULL;

	strcpy_wchar_to_char(dst, src);

	return dst;
}
