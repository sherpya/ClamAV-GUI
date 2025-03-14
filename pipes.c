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
#define BAIL_OUT(code)      \
    {                       \
        isScanning = false; \
        return code;        \
    }

/* shared */
PROCESS_INFORMATION pi;

static HANDLE m_hEvtStop = NULL;
static HANDLE hChildStdoutRdDup;
static DWORD exitcode;

void RedirectStdOutput()
{
    char chBuf[BUFSIZE];
    DWORD dwRead, dwAvail = 0;

    if (!PeekNamedPipe(hChildStdoutRdDup, NULL, 0, NULL, &dwAvail, NULL) || !dwAvail)
        return;

    if (!ReadFile(hChildStdoutRdDup, chBuf, min(BUFSIZE - 1, dwAvail), &dwRead, NULL) || !dwRead)
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

DWORD WINAPI OutputThread(LPVOID lpvThreadParam)
{
    TCHAR msg[128];
    HANDLE Handles[2] = {pi.hProcess, m_hEvtStop};

    ResumeThread(pi.hThread);

    while (true)
    {
        DWORD dwRc = WaitForMultipleObjects(2, Handles, FALSE, 100);
        RedirectStdOutput((BOOL)(UINT_PTR)lpvThreadParam);
        if ((dwRc == WAIT_OBJECT_0) || (dwRc == WAIT_OBJECT_0 + 1) || (dwRc == WAIT_FAILED))
            break;
    }

    CloseHandle(hChildStdoutRdDup);

    wsprintf(msg, TEXT("\r\nProcess exited with %ld code\r\n"), exitcode);
    WriteStdOut(msg);
    EnableWindow(GetDlgItem(MainDlg, IDC_SCAN), TRUE);
    isScanning = false;

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
        return FALSE;

    return TRUE;
}

/* Thread Proc */
DWORD WINAPI PipeToClamAV(LPVOID lpvThreadParam)
{
    HANDLE hChildStdoutRd, hChildStdoutWr, hChildStderrWr;
    TCHAR *pszCmdLine = (TCHAR *)lpvThreadParam;

    if (!pszCmdLine)
        BAIL_OUT(1);

    SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

    /* Create a pipe for the child process's STDOUT */
    if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
        BAIL_OUT(1);

    /* Duplicate stdout to stderr */
    if (!DuplicateHandle(GetCurrentProcess(), hChildStdoutWr, GetCurrentProcess(), &hChildStderrWr, 0, TRUE, DUPLICATE_SAME_ACCESS))
        BAIL_OUT(1);

    /* Duplicate the pipe HANDLE */
    if (!DuplicateHandle(GetCurrentProcess(), hChildStdoutRd, GetCurrentProcess(), &hChildStdoutRdDup, 0, FALSE, DUPLICATE_SAME_ACCESS))
        BAIL_OUT(1);

    CloseHandle(hChildStdoutRd);

    EnableWindow(GetDlgItem(MainDlg, IDC_SCAN), FALSE);
    SetWindowText(GetDlgItem(MainDlg, IDC_STATUS), TEXT(""));

    WriteStdOut(pszCmdLine);
    WriteStdOut(TEXT("\r\n\r\n"));

    LaunchClamAV(pszCmdLine, hChildStdoutWr, hChildStderrWr);
    CloseHandle(hChildStdoutWr);

    m_hEvtStop = CreateEvent(NULL, TRUE, FALSE, NULL);

    DWORD m_dwThreadId;
    HANDLE m_hThread = CreateThread(NULL, 0, OutputThread, NULL, 0, &m_dwThreadId);

    if (!m_hThread)
        return 1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitcode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    pi.hProcess = INVALID_HANDLE_VALUE;

    if (m_hEvtStop)
        SetEvent(m_hEvtStop);

    CloseHandle(m_hThread);
    return 0;
}
