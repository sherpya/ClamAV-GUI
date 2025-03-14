/*
 * Simple Clamav GUI
 *
 * Copyright (c) 2006-2025 Gianluigi Tiesi <sherpya@gmail.com>
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

#include "clamav-gui.h"

#define BUFSIZE 1024
#define BAIL_OUT(code)                       \
    {                                        \
        InterlockedExchange(&g_Busy, FALSE); \
        return code;                         \
    }

/* shared */
PROCESS_INFORMATION pi;
static DWORD exitcode;

typedef struct _PIPEDATA
{
    HANDLE EventStop;
    HANDLE Pipe;
} PIPEDATA;

static inline void FormatLastError(TCHAR *func)
{
    TCHAR message[1024];
    size_t length = sizeof(message) / sizeof(message[0]);

    message[0] = 0;
    _tcsncat(message, func, length - _tcslen(func));
    _tcsncat(message, TEXT(" failed: "), 9);

    size_t offset = _tcslen(message);
    length -= offset;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(), 0, &message[offset], (DWORD)length, NULL);
    WriteStdOut(message);
}

void RedirectStdOutput(PIPEDATA *pipedata)
{
    char chBuf[BUFSIZE];
    DWORD dwRead, dwAvail = 0;

    if (!PeekNamedPipe(pipedata->Pipe, NULL, 0, NULL, &dwAvail, NULL) || !dwAvail)
        return;

    if (!ReadFile(pipedata->Pipe, chBuf, min(BUFSIZE - 1, dwAvail), &dwRead, NULL) || !dwRead)
        return;

    chBuf[dwRead] = 0;
#ifdef UNICODE
    WCHAR *chBufWide = malloc((dwRead + 1) * sizeof(WCHAR));
    if (!chBufWide)
        return;
    if (!MultiByteToWideChar(CP_ACP, 0, chBuf, dwRead, chBufWide, dwRead))
    {
        free(chBufWide);
        return;
    }
    chBufWide[dwRead] = 0;
    WriteStdOut(chBufWide);
    free(chBufWide);
#else
    WriteStdOut(chBuf);
#endif
}

/* thread proc */
DWORD WINAPI OutputThread(LPVOID lpvThreadParam)
{
    PIPEDATA *pipedata = (PIPEDATA *)lpvThreadParam;
    HANDLE Handles[2] = {pi.hProcess, pipedata->EventStop};

    SetThreadName("Output Thread");
    ResumeThread(pi.hThread);

    while (true)
    {
        DWORD dwRc = WaitForMultipleObjects(2, Handles, FALSE, 100);
        RedirectStdOutput(pipedata);
        if ((dwRc == WAIT_OBJECT_0) || (dwRc == WAIT_OBJECT_0 + 1) || (dwRc == WAIT_FAILED))
            break;
    }

    TCHAR msg[128];
    wsprintf(msg, TEXT("\r\nProcess exited with %ld code\r\n"), exitcode);
    WriteStdOut(msg);

    EnableWindow(GetDlgItem(MainDlg, IDC_SCAN), TRUE);
    InterlockedExchange(&g_Busy, FALSE);
    return 0;
}

bool LaunchClamAV(LPTSTR pszCmdLine, HANDLE hStdOut, HANDLE hStdErr)
{
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = hStdOut;
    si.hStdError = hStdErr;
    si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    if (!CreateProcess(NULL, pszCmdLine,
                       NULL, NULL,
                       TRUE,
                       CREATE_NEW_CONSOLE | CREATE_SUSPENDED,
                       NULL, NULL,
                       &si,
                       &pi))
    {
        FormatLastError(TEXT("CreateProcess"));
        return false;
    }

    return true;
}

/* thread proc */
DWORD WINAPI PipeToClamAV(LPVOID lpvThreadParam)
{
    DWORD result = FALSE;
    HANDLE hChildStdoutRd, hChildStdoutWr, hChildStderrWr;
    TCHAR *pszCmdLine = (TCHAR *)lpvThreadParam;
    PIPEDATA pipedata = {NULL, NULL};

    SetThreadName("Pipe Thread");

    if (!pszCmdLine)
        BAIL_OUT(FALSE);

    SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

    /* Create a pipe for the child process's STDOUT */
    if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
        BAIL_OUT(FALSE);

    /* Duplicate stdout to stderr */
    if (!DuplicateHandle(GetCurrentProcess(), hChildStdoutWr, GetCurrentProcess(), &hChildStderrWr, 0, TRUE, DUPLICATE_SAME_ACCESS))
    {
        CloseHandle(hChildStdoutRd);
        CloseHandle(hChildStdoutWr);
        BAIL_OUT(FALSE);
    }

    /* Duplicate the pipe HANDLE */
    if (!DuplicateHandle(GetCurrentProcess(), hChildStdoutRd, GetCurrentProcess(), &pipedata.Pipe, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        CloseHandle(hChildStdoutRd);
        CloseHandle(hChildStdoutWr);
        CloseHandle(hChildStderrWr);
        BAIL_OUT(FALSE);
    }

    CloseHandle(hChildStdoutRd);

    EnableWindow(GetDlgItem(MainDlg, IDC_SCAN), FALSE);
    EnableWindow(GetDlgItem(MainDlg, IDC_UPDATE), FALSE);
    SetWindowText(GetDlgItem(MainDlg, IDC_STATUS), TEXT(""));

    WriteStdOut(pszCmdLine);
    WriteStdOut(TEXT("\r\n\r\n"));

    do
    {
        if (!LaunchClamAV(pszCmdLine, hChildStdoutWr, hChildStderrWr))
            break;

        if (!(pipedata.EventStop = CreateEvent(NULL, TRUE, FALSE, NULL)))
        {
            FormatLastError(TEXT("CreateEvent"));
            break;
        }

        DWORD dwThreadId;
        HANDLE hThread = CreateThread(NULL, 0, OutputThread, &pipedata, 0, &dwThreadId);

        if (!hThread)
        {
            FormatLastError(TEXT("CreateThread"));
            break;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitcode);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        pi.hProcess = INVALID_HANDLE_VALUE;
        SetEvent(pipedata.EventStop);
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        result = TRUE;
        break;
    } while (0);

    CloseHandle(hChildStdoutWr);
    CloseHandle(hChildStderrWr);
    CloseHandle(pipedata.Pipe);
    if (pipedata.EventStop)
        CloseHandle(pipedata.EventStop);

    EnableWindow(GetDlgItem(MainDlg, IDC_SCAN), TRUE);
    EnableWindow(GetDlgItem(MainDlg, IDC_UPDATE), TRUE);
    return result;
}
