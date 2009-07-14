/*
 * Clamav GUI Wrapper
 *
 * Copyright (c) 2006-2009 Gianluigi Tiesi <sherpya@netfarm.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this software; if not, write to the
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_DEPRECATE

#pragma warning(disable: 4311 4312) /* yes, yes I use a bool as a pointer */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include <io.h>
#include <fcntl.h>

#define CLAMSCAN    "clamscan.exe"
#define FRESHCLAM   "freshclam.exe"

extern HWND MainDlg;
extern BOOL isScanning;
extern void WriteStdOut(LPSTR msg, BOOL freshclam);
extern void GetDestinationFolder(void);
DWORD WINAPI PipeToClamAV(LPVOID lpvThreadParam);
extern HANDLE m_hEvtStop;
extern PROCESS_INFORMATION pi;
