/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <windows.h>

extern IMAGE_DOS_HEADER __ImageBase;

#ifdef __GNUC__
__attribute((externally_visible))
#endif
#ifdef __i686__
__attribute((force_align_arg_pointer))
#endif
int
WinMainCRTStartup(void)
{
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    GetStartupInfo(&si);
    HINSTANCE hInstance = (HINSTANCE)&__ImageBase;
    LPSTR lpCmdLine = NULL; // GetCommandLine();
    int nCmdShow = si.wShowWindow;

    return WinMain(hInstance, NULL, lpCmdLine, nCmdShow);
}

void *malloc(size_t size)
{
    return HeapAlloc(GetProcessHeap(), 0, size);
}

void free(void *ptr)
{
    HeapFree(GetProcessHeap(), 0, ptr);
}

#if 0
#pragma function(memset)
void *memset(void *dest, register int val, register size_t len)
{
    register unsigned char *ptr = (unsigned char *)dest;
    while (len-- > 0)
        *ptr++ = val;
    return dest;
}
#endif

#ifdef UNICODE
#define _tcslen wcslen
#define _tcscat wcscat
#define _tcscpy wcscpy
#define _tcsncat wcsncat
#define _tcsnccmp wcsncmp
#define _tcsstr wcsstr
#define _tcschr wcschr
#else
#define _tcslen strlen
#define _tcscat strcat
#define _tcscpy strcpy
#define _tcsncat strncat
#define _tcsnccmp strncmp
#define _tcsstr strstr
#define _tcschr strchr

#ifdef _MSC_VER
#define strncat _mbsnbcat
#define strchr _mbschr
#define strstr _mbsstr
#endif
#endif

#ifndef _MSC_VER
size_t _tcslen(const TCHAR *str)
{
    const TCHAR *ptr = str;
    while (*str)
        ++str;

    return str - ptr;
}

TCHAR* _tcscat(TCHAR* s, const TCHAR* append)
{
    TCHAR* save = s;
    for (; *s; ++s)
        ;
    while ((*s++ = *append++) != '\0')
        ;
    return save;
}

TCHAR* _tcscpy(TCHAR* to, const TCHAR* from)
{
    TCHAR* save = to;
    for (; (*to = *from) != '\0'; ++from, ++to)
        ;
    return save;
}
#endif

TCHAR *_tcsncat(TCHAR *dst, const TCHAR *src, size_t n)
{
    if (n != 0)
    {
        TCHAR *d = dst;
        const TCHAR *s = src;
        while (*d != 0)
            d++;
        do
        {
            if ((*d = *s++) == 0)
                break;
            d++;
        } while (--n != 0);
        *d = 0;
    }
    return dst;
}

int _tcsnccmp(const TCHAR *s1, const TCHAR *s2, size_t n)
{
    if (n == 0)
        return 0;
    do
    {
        if (*s1 != *s2++)
            return *s1 - *--s2;
        if (*s1++ == 0)
            break;
    } while (--n != 0);
    return 0;
}

TCHAR *_tcsstr(const TCHAR *s, const TCHAR *find)
{
    TCHAR c, sc;
    size_t len;
    if ((c = *find++) != 0)
    {
        len = _tcslen(find);
        do
        {
            do
            {
                if ((sc = *s++) == 0)
                    return NULL;
            } while (sc != c);
        } while (_tcsnccmp(s, find, len) != 0);
        s--;
    }
    return (TCHAR *)s;
}

#ifdef UNICODE
wchar_t *wcschr(const wchar_t *p, wchar_t ch)
#else
char *strchr(const char *p, int ch)
#endif
{
    for (;; ++p)
    {
        if (*p == ch)
            return (TCHAR *)p;
        if (!*p)
            return NULL;
    }
}
