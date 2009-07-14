/*
 * Clamav GUI Wrapper
 *
 * Copyright (c) 2006 Gianluigi Tiesi <sherpya@netfarm.it>
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

#include <ClamAV-GUI.h>
#include <resource.h>

#define BUFSIZE 1024

#define BAIL_OUT(code) { isScanning = FALSE; return code; }

HANDLE m_hEvtStop;
HANDLE hChildStdinWrDup = INVALID_HANDLE_VALUE, hChildStdoutRdDup = INVALID_HANDLE_VALUE;
PROCESS_INFORMATION pi;
DWORD exitcode = 0;

void RedirectStdOutput(BOOL freshclam)
{
    char chBuf[BUFSIZE];
    DWORD dwRead;
    DWORD dwAvail = 0;
    if (!PeekNamedPipe(hChildStdoutRdDup, NULL, 0, NULL, &dwAvail, NULL) || !dwAvail)
        return;
    if (!ReadFile(hChildStdoutRdDup, chBuf, min(BUFSIZE - 1, dwAvail), &dwRead, NULL) || !dwRead)
        return; 
    chBuf[dwRead] = 0;
    WriteStdOut(chBuf, freshclam);
}

DWORD WINAPI OutputThread(LPVOID lpvThreadParam)
{
    char msg[128];
    HANDLE Handles[2];
    Handles[0] = pi.hProcess;
    Handles[1] = m_hEvtStop;
    
    ResumeThread(pi.hThread);

    while(1)    
    {
        DWORD dwRc = WaitForMultipleObjects(2, Handles, FALSE, 100);
        RedirectStdOutput((BOOL) lpvThreadParam);
        if ((dwRc == WAIT_OBJECT_0) || (dwRc == WAIT_OBJECT_0 + 1) || (dwRc == WAIT_FAILED))
            break;
    } 
    CloseHandle(hChildStdoutRdDup);

    sprintf(msg, "\r\nProcess exited with %d code\r\n", exitcode);
    WriteStdOut(msg, FALSE);
    EnableWindow(GetDlgItem(MainDlg, IDC_SCAN), TRUE);
    isScanning = FALSE;
    return 1;
}

BOOL LaunchClamAV(LPSTR pszCmdLine, HANDLE hStdOut, HANDLE hStdErr)
{
    
    STARTUPINFO si;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = hStdOut;
    si.hStdError = hStdErr;
    si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

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

DWORD WINAPI PipeToClamAV(LPVOID lpvThreadParam)
{
    HANDLE hChildStdoutRd, hChildStdoutWr, hChildStderrWr;
    char *pszCmdLine = (char *) lpvThreadParam;

    if (!pszCmdLine) BAIL_OUT(-1);

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    /* Create a pipe for the child process's STDOUT */
    if(!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
        BAIL_OUT(-1);

    /* Duplicate stdout sto stderr */
    if (!DuplicateHandle(GetCurrentProcess(), hChildStdoutWr, GetCurrentProcess(), &hChildStderrWr, 0, TRUE, DUPLICATE_SAME_ACCESS))
        BAIL_OUT(-1);

    /* Duplicate the pipe HANDLE */
    if (!DuplicateHandle(GetCurrentProcess(), hChildStdoutRd, GetCurrentProcess(), &hChildStdoutRdDup, 0, FALSE, DUPLICATE_SAME_ACCESS))
        BAIL_OUT(-1);

    CloseHandle(hChildStdoutRd);

    EnableWindow(GetDlgItem(MainDlg, IDC_SCAN), FALSE);
    SetWindowText(GetDlgItem(MainDlg, IDC_STATUS), "");

    WriteStdOut(pszCmdLine, FALSE);
    WriteStdOut("\r\n\r\n", FALSE);
    
    LaunchClamAV(pszCmdLine, hChildStdoutWr, hChildStderrWr);
    BOOL freshclam = !_strnicmp(pszCmdLine, "freshclam", 9);
    delete pszCmdLine;
    CloseHandle(hChildStdoutWr);

    m_hEvtStop = CreateEvent(NULL, TRUE, FALSE, NULL);    
    DWORD m_dwThreadId;
    HANDLE m_hThread = CreateThread(NULL, 0, OutputThread, (LPVOID) freshclam, 0, &m_dwThreadId);

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitcode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    pi.hProcess = INVALID_HANDLE_VALUE;
    SetEvent(m_hEvtStop);
    return 0;
}
