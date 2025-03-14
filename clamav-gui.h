#ifndef _CLAMAV_GUI_H_
#define _CLAMAV_GUI_H_

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdbool.h>

#include "resource.h"

extern HWND MainDlg;
extern LONG g_Busy;

extern PROCESS_INFORMATION pi;

extern void WriteStdOut(LPTSTR msg);
extern DWORD WINAPI PipeToClamAV(LPVOID lpvThreadParam);

#ifdef _DEBUG
#define SetThreadName(s) SetThreadDescription(GetCurrentThread(), TEXT(##s))
#else
#define SetThreadName(s)
#endif

#endif // _CLAMAV_GUI_H__
