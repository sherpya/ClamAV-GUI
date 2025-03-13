#ifndef _CLAMAV_GUI_H_
#define _CLAMAV_GUI_H_

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdbool.h>

#include "resource.h"

extern HWND MainDlg;
extern bool isScanning;

extern PROCESS_INFORMATION pi;

extern void WriteStdOut(LPTSTR msg);
extern DWORD WINAPI PipeToClamAV(LPVOID lpvThreadParam);

// broken defines?
#undef _stprintf
#ifdef UNICODE
#define _stprintf swprintf
#else
#define _stprintf snprintf
#endif

#endif // _CLAMAV_GUI_H__
