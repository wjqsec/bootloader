/*
 * xfuncs.c - safe malloc funcions
 *
 * based on busybox
 *
 * Copyright (c) 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
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
#define pr_fmt(fmt) "xfuncs: " fmt

#include <common.h>
#include <malloc.h>
#include <module.h>
#include <wchar.h>

static void __noreturn enomem_panic(size_t size)
{
	pr_emerg("out of memory\n");
	if (size)
		pr_emerg("Unable to allocate %zu bytes\n", size);

	malloc_stats();

	panic("out of memory");
}

void *xmalloc(size_t size)
{
	void *p = NULL;

	if (!(p = malloc(size)))
		enomem_panic(size);

	return p;
}
EXPORT_SYMBOL(xmalloc);

void *xrealloc(void *ptr, size_t size)
{
	void *p = NULL;

	if (!(p = realloc(ptr, size)))
		enomem_panic(size);

	return p;
}
EXPORT_SYMBOL(xrealloc);

void *xzalloc(size_t size)
{
	void *ptr = xmalloc(size);
	memset(ptr, 0, size);
	return ptr;
}
EXPORT_SYMBOL(xzalloc);

char *xstrdup(const char *s)
{
	char *p;

	if (!s)
		return NULL;

	p = strdup(s);
	if (!p)
		enomem_panic(strlen(s) + 1);

	return p;
}
EXPORT_SYMBOL(xstrdup);

char *xstrndup(const char *s, size_t n)
{
	int m;
	char *t;

	/* We can just xmalloc(n+1) and strncpy into it, */
	/* but think about xstrndup("abc", 10000) wastage! */
	m = n;
	t = (char*) s;
	while (m) {
		if (!*t) break;
		m--;
		t++;
	}
	n -= m;
	t = xmalloc(n + 1);
	t[n] = '\0';

	return memcpy(t, s, n);
}
EXPORT_SYMBOL(xstrndup);

void* xmemalign(size_t alignment, size_t bytes)
{
	void *p = memalign(alignment, bytes);
	if (!p)
		enomem_panic(bytes);

	return p;
}
EXPORT_SYMBOL(xmemalign);

void *xmemdup(const void *orig, size_t size)
{
	void *buf = xmalloc(size);

	memcpy(buf, orig, size);

	return buf;
}
EXPORT_SYMBOL(xmemdup);

char *xvasprintf(const char *fmt, va_list ap)
{
	char *p;

	p = bvasprintf(fmt, ap);
	if (!p)
		enomem_panic(0);
	return p;
}
EXPORT_SYMBOL(xvasprintf);

char *xasprintf(const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = xvasprintf(fmt, ap);
	va_end(ap);

	return p;
}
EXPORT_SYMBOL(xasprintf);

wchar_t *xstrdup_wchar(const wchar_t *s)
{
	wchar_t *p = strdup_wchar(s);

	if (!p)
		enomem_panic((wcslen(s) + 1) * sizeof(wchar_t));

	return p;
}
EXPORT_SYMBOL(xstrdup_wchar);

wchar_t *xstrdup_char_to_wchar(const char *s)
{
	wchar_t *p = strdup_char_to_wchar(s);

	if (!p)
		enomem_panic((strlen(s) + 1) * sizeof(wchar_t));

	return p;
}
EXPORT_SYMBOL(xstrdup_char_to_wchar);

char *xstrdup_wchar_to_char(const wchar_t *s)
{
	char *p = strdup_wchar_to_char(s);

	if (!p)
		enomem_panic((wcslen(s) + 1) * sizeof(wchar_t));

	return p;
}
EXPORT_SYMBOL(xstrdup_wchar_to_char);
