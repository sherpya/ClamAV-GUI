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

#include <shlobj.h>

#define INIFILE TEXT(".\\ClamAV-GUI.ini")
#define CLAMSCAN TEXT("clamscan.exe")
#define FRESHCLAM TEXT("freshclam.exe")

#define MAX_OUTPUT 8192
#define MAX_CMDLINE 8192

/* shared */
HWND MainDlg = NULL;
LONG g_Busy = FALSE;

static HANDLE LogMutex;

static struct
{
    const TCHAR *param;
    UINT dialog;
} options[] =
    {
        {TEXT("-r"), IDC_RECURSE},
        {TEXT("--remove"), IDC_REMOVE},
        {TEXT("-i"), IDC_ONLYINFECTED},

        {TEXT("--no-mail"), IDC_NOMAIL},
        {TEXT("--no-pe"), IDC_NOPE},
        {TEXT("--no-ole2"), IDC_NOOLE2},
        {TEXT("--no-html"), IDC_NOHTML},
        {TEXT("--no-archive"), IDC_NOARCHIVES},

        {NULL, 0}};

void SelectScanFolder(void)
{
    BROWSEINFO pbi;
    TCHAR szPath[MAX_PATH];
    LPITEMIDLIST pidlTarget = NULL;
    ZeroMemory(&pbi, sizeof(pbi));

    pbi.hwndOwner = MainDlg;
    pbi.lpszTitle = TEXT("Select Folder to Scan");
    pbi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NONEWFOLDERBUTTON | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;

    pidlTarget = SHBrowseForFolder(&pbi);
    if (pidlTarget && SHGetPathFromIDList(pidlTarget, szPath))
        SetWindowText(GetDlgItem(MainDlg, IDC_TARGET), szPath);
}

void WriteStdOut(LPTSTR msg)
{
    WaitForSingleObject(LogMutex, INFINITE);
    HWND editw = GetDlgItem(MainDlg, IDC_STATUS);

    size_t length = GetWindowTextLength(editw) + 1;
    size_t slen = _tcslen(msg) + 2;

    TCHAR *buf, *output;

    buf = output = malloc((length + slen) * sizeof(TCHAR));
    if (!buf)
        return;

    GetWindowText(editw, buf, (int)length);
    buf[length] = 0;

    if (length > MAX_OUTPUT)
    {
        TCHAR *firstline = _tcsstr(buf, TEXT("\r\n"));
        if (firstline)
        {
            firstline += 2;
            output = firstline;
        }
    }

    _tcscat(output, msg);

    LockWindowUpdate(editw);
    SetWindowText(editw, output);
    free(buf);

    int lResult = (int)SendMessage(editw, (UINT)EM_GETLINECOUNT, (WPARAM)0, (LPARAM)0);
    SendMessage(editw, (UINT)EM_LINESCROLL, (WPARAM)0, (LPARAM)lResult);
    LockWindowUpdate(NULL);
    ReleaseMutex(LogMutex);
}

#define OOM()                                                          \
    {                                                                  \
        WriteStdOut(TEXT("Out of memory composing command line\r\n")); \
        return NULL;                                                   \
    }

TCHAR *MakeCmdLine(UINT id)
{
    static TCHAR cmdline[MAX_CMDLINE];
    size_t length = 0;

    cmdline[0] = 0;

    switch (id)
    {
    case IDC_SCAN:
    {
        _tcsncat(cmdline, CLAMSCAN, _tcslen(CLAMSCAN));
        length = GetWindowTextLength(GetDlgItem(MainDlg, IDC_CMDLINE));
        if (length++)
        {
            _tcsncat(cmdline, TEXT(" "), 1);
            size_t offset = _tcslen(cmdline);
            if ((offset + length) > (sizeof(cmdline) / sizeof(cmdline[0])))
                OOM();
            GetWindowText(GetDlgItem(MainDlg, IDC_CMDLINE), &cmdline[offset], (int)length);
        }

        for (int i = 0; options[i].param; i++)
        {
            if (IsDlgButtonChecked(MainDlg, options[i].dialog) == BST_CHECKED)
            {
                _tcsncat(cmdline, TEXT(" "), 1);
                _tcsncat(cmdline, options[i].param, MAX_CMDLINE - _tcslen(options[i].param) - 1);
            }
        }

        if (!(IsDlgButtonChecked(MainDlg, IDC_ALLDRIVES) == BST_CHECKED))
        {
            length = GetWindowTextLength(GetDlgItem(MainDlg, IDC_TARGET));
            if (!length++)
            {
                WriteStdOut(TEXT("Missing destination directory\r\n"));
                return NULL;
            }

            if ((_tcslen(cmdline) + length + 2) > (sizeof(cmdline) / sizeof(cmdline[0])))
                OOM();

            TCHAR *path = malloc((length + 1) * sizeof(TCHAR));
            if (!path)
                OOM();

            GetWindowText(GetDlgItem(MainDlg, IDC_TARGET), path, (int)length);
            path[length] = 0;

            _tcsncat(cmdline, TEXT(" "), 1);
            if (!_tcschr(path, TEXT(' ')))
                _tcsncat(cmdline, path, length);
            else
            {
                _tcsncat(cmdline, TEXT("\""), 1);
                _tcsncat(cmdline, path, length);
                _tcsncat(cmdline, TEXT("\""), 1);
            }
            free(path);
        }
        else
        {
            TCHAR lpBuffer[MAX_PATH], szDrive[MAX_PATH];
            /* Get Drives Mask */
            GetLogicalDriveStrings(MAX_PATH, lpBuffer);
            TCHAR *seek = lpBuffer;
            while (*seek)
            {
                szDrive[0] = 0;
                _tcscat(szDrive, seek);
                seek += _tcslen(szDrive) + 1;
                TCHAR uDrive = CharUpper(szDrive)[0];

                /* Skip Drive A: and B: but not other removable drives */
                if ((uDrive == TEXT('A')) || (uDrive == TEXT('B')))
                    continue;

                switch (GetDriveType(szDrive))
                {
                case DRIVE_REMOVABLE:
                case DRIVE_FIXED:
                case DRIVE_REMOTE:
                case DRIVE_RAMDISK:
                    if ((_tcslen(cmdline) + _tcslen(szDrive) + 1) > (sizeof(cmdline) / sizeof(cmdline[0])))
                        OOM();
                    _tcsncat(cmdline, TEXT(" "), 1);
                    _tcsncat(cmdline, szDrive, MAX_CMDLINE - _tcslen(szDrive) - 1);
                    break;
                }
            }
        }
        break;
    }
    case IDC_UPDATE:
        _tcsncat(cmdline, FRESHCLAM, _tcslen(FRESHCLAM));
        break;
    default:
        return NULL;
    }

    return cmdline;
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    BOOL save = (BOOL)lParam;
    DWORD id = GetWindowLong(hwnd, GWL_ID);

    TCHAR key[5];
    wsprintf(key, TEXT("%d"), id);

    switch (id)
    {
    case IDC_TARGET:
    case IDC_CMDLINE:
    {
        TCHAR text[MAX_PATH];
        text[0] = 0;
        if (save)
        {
            GetWindowText(hwnd, text, MAX_PATH - 1);
            WritePrivateProfileString(TEXT("dialogs"), key, text, INIFILE);
        }
        else
        {
            GetPrivateProfileString(TEXT("dialogs"), key, TEXT(""), text, MAX_PATH - 1, INIFILE);
            SetWindowText(hwnd, text);
        }
        return TRUE;
    }
    case IDC_ALLDRIVES:
    case IDC_RECURSE:
    case IDC_NOARCHIVES:
    case IDC_ONLYINFECTED:
    case IDC_REMOVE:
    case IDC_NOMAIL:
    case IDC_NOPE:
    case IDC_NOOLE2:
    case IDC_NOHTML:
    {
        INT def = (id == IDC_RECURSE) ? 1 : 0;
        if (save)
            WritePrivateProfileString(TEXT("dialogs"), key, (IsDlgButtonChecked(MainDlg, id) == BST_CHECKED) ? TEXT("1") : TEXT("0"), INIFILE);
        else
        {
            if (GetPrivateProfileInt(TEXT("dialogs"), key, def, INIFILE))
                CheckDlgButton(MainDlg, id, 1);
        }
        return TRUE;
    }

    default:
        return TRUE;
    }
}

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
        SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hIcon);
        return TRUE;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) != BN_CLICKED)
            return FALSE;

        switch (LOWORD(wParam))
        {
        case IDC_SCAN:
        case IDC_UPDATE:
        {
            if (InterlockedCompareExchange(&g_Busy, TRUE, FALSE) == TRUE)
                return TRUE;

            TCHAR *cmdline = MakeCmdLine(LOWORD(wParam));
            if (cmdline)
            {
                DWORD m_dwThreadId;
                HANDLE m_hThread = CreateThread(NULL, 0, PipeToClamAV, (LPVOID)cmdline, 0, &m_dwThreadId);
                if (m_hThread)
                    CloseHandle(m_hThread);
            }
            else
                InterlockedExchange(&g_Busy, FALSE);
            return TRUE;
        }
        case IDC_CANCEL:
        {
            if (InterlockedCompareExchange(&g_Busy, g_Busy, FALSE) == TRUE && (pi.hProcess != INVALID_HANDLE_VALUE))
                TerminateProcess(pi.hProcess, 1);
            return TRUE;
        }

        case IDC_BROWSE:
        {
            if (InterlockedCompareExchange(&g_Busy, g_Busy, FALSE) == FALSE)
                SelectScanFolder();
            return TRUE;
        }
        }
        return TRUE;
    }
    case WM_CLOSE:
    {
        if (pi.hProcess != INVALID_HANDLE_VALUE)
            TerminateProcess(pi.hProcess, 1);
        EnumChildWindows(hwndDlg, EnumChildProc, (LPARAM)TRUE);
        DestroyWindow(hwndDlg);
        return TRUE;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0); /* send a WM_QUIT to the message queue */
        return TRUE;
    }
    default:
        return FALSE;
    }
}

int APIENTRY
#ifdef UNICODE
wWinMain
#else
WinMain
#endif
    (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    SetThreadName("Main Thread");
    pi.hProcess = INVALID_HANDLE_VALUE;
    LogMutex = CreateMutex(NULL, FALSE, TEXT("ClamAVGuiLoggerMutex"));
    MainDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DialogProc);

    EnumChildWindows(MainDlg, EnumChildProc, (LPARAM)FALSE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (LogMutex)
        CloseHandle(LogMutex);

    return (int)msg.wParam;
}
