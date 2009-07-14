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

#include <ClamAV-GUI.h>
#include <shlobj.h>
#include <resource.h>
#include <string>

#define INIFILE ".\\ClamAV-GUI.ini"

HWND MainDlg;
BOOL isScanning = FALSE;
HANDLE LogMutex;

#define MAX_OUTPUT 8192

typedef struct _Option
{
    const char *param;
    UINT dialog;
} Option;

static Option options[] =
{
    { "-r",             IDC_RECURSE         },
    { "--remove",       IDC_REMOVE          },
    { "-i",             IDC_ONLYINFECTED    },

    { "--no-mail",      IDC_NOMAIL          },
    { "--no-pe",        IDC_NOPE            },
    { "--no-ole2",      IDC_NOOLE2          },
    { "--no-html",      IDC_NOHTML          },
    { "--no-archive",   IDC_NOARCHIVES      },

    { NULL, 0 }
};

void GetDestinationFolder(void)
{
    BROWSEINFO pbi;
    char szPath[MAX_PATH];
    LPITEMIDLIST pidlTarget = NULL;
    ZeroMemory(&pbi, sizeof(pbi));

    pbi.hwndOwner = MainDlg;
    pbi.lpszTitle = "Select Target Folder\0";
    pbi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NONEWFOLDERBUTTON | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;

    pidlTarget = SHBrowseForFolder(&pbi);
    if (pidlTarget && SHGetPathFromIDList(pidlTarget, szPath))
        SetWindowText(GetDlgItem(MainDlg, IDC_TARGET), szPath);
}

void WriteStdOut(LPSTR msg, BOOL freshclam)
{
    char *firstline = NULL;
    WaitForSingleObject(LogMutex, INFINITE);
    HWND editw = GetDlgItem(MainDlg, IDC_STATUS);

    char *buff = 0, *buf = 0;
    int length = GetWindowTextLength(editw);
    length++;
    size_t slen = strlen(msg) + 2;

    buff = buf = new char[length + slen];
    if(!buf) return;

    GetWindowText(editw, buf, length);
    buf[length - 1] = 0;

    if(length > MAX_OUTPUT)
    {
        firstline = strstr(buf, "\r\n");
        if (firstline) firstline += 2;
        buf = firstline;
    }

    if (!buf)
    {
        delete buff;
        SetWindowText(editw, "");
        ReleaseMutex(LogMutex);
        return;
    }

    strcat(buf, msg);
    std::string out;
    if (freshclam)
    {
        char o[2], *m, *m1;
        o[1] = 0;
        for (m = buf; *m; m++)
        {
            m1 = m + 1;
            if ((*m == '\r') && (!*m1 || (*m1 != '\n')))
                out.append("\r\n");
            else
            {
                o[0] = *m;
                out.append(o);
            }
        }
    }
    else
        out.append(buf);
    delete buff;

    LockWindowUpdate(editw);
    SetWindowText(editw, out.c_str());
    int lResult = (int) SendMessage(editw, (UINT) EM_GETLINECOUNT, (WPARAM) 0, (LPARAM) 0);
    SendMessage(editw, (UINT) EM_LINESCROLL, (WPARAM) 0, (LPARAM) lResult);
    LockWindowUpdate(NULL);
    ReleaseMutex(LogMutex);
    return;
}

char *MakeCmdLine(UINT id)
{
    std::string cmdline;
    int length = 0;
    int i = 0;

    switch (id)
    {
        case IDC_SCAN:
        {
            cmdline.append(CLAMSCAN);
            length = GetWindowTextLength(GetDlgItem(MainDlg, IDC_CMDLINE)) + 1;
            if (length)
            {
                char *useropt = new char[length + 1];
                GetWindowText(GetDlgItem(MainDlg, IDC_CMDLINE), useropt, length);
                useropt[length] = 0;
                cmdline.push_back(' ');
                cmdline.append(useropt);
                delete useropt;
            }

            for (i = 0; options[i].param; i++)
            {
                if (IsDlgButtonChecked(MainDlg, options[i].dialog) == BST_CHECKED)
                {
                    cmdline.push_back(' ');
                    cmdline.append(options[i].param);
                }
            }

            if (!IsDlgButtonChecked(MainDlg, IDC_ALLDRIVES) == BST_CHECKED)
            {
                length = GetWindowTextLength(GetDlgItem(MainDlg, IDC_TARGET));
                if (!length)
                {
                    WriteStdOut("Missing destination directory\r\n", FALSE);
                    return NULL;
                }
                length++;

                char *path = new char[length + 1];
                GetWindowText(GetDlgItem(MainDlg, IDC_TARGET), path, length);
                path[length] = 0;

                cmdline.push_back(' ');
                if (!strchr(path, ' '))
                    cmdline.append(path);
                else
                {
                    cmdline.push_back('"');
                    cmdline.append(path);
                    cmdline.push_back('"');
                }
                delete path;
            }
            else
            {
                char lpBuffer[MAX_PATH], szDrive[MAX_PATH];
                /* Get Drives Mask */
                GetLogicalDriveStrings(MAX_PATH, lpBuffer);
                char *seek = lpBuffer;
                while (*seek)
                {
                    szDrive[0] = 0;
                    strcat(szDrive, seek);
                    seek += strlen(szDrive) + 1;

                    /* Skip Drive A: but not other removable drives */
                    if (tolower(szDrive[0]) == 'a') continue;

                    switch(GetDriveType(szDrive))
                    {
                    case DRIVE_REMOVABLE:
                    case DRIVE_FIXED:
                    case DRIVE_REMOTE:
                    case DRIVE_RAMDISK:
                        cmdline.push_back(' ');
                        cmdline.append(szDrive);
                        break;
                    }
                }
            }
            break;
        }
        case IDC_UPDATE:
            cmdline.append(FRESHCLAM);
            break;
        default:
            return NULL;
    }

    char *cmd = new char[cmdline.size() + 1];
    cmd[0] = 0;
    strcat(cmd, cmdline.c_str());
    return cmd;
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    BOOL save = (BOOL) lParam;
    DWORD id = GetWindowLong(hwnd, GWL_ID);
    char key[5];
    _itoa(id, key, 10);
    switch (id)
    {
        case IDC_TARGET:
        case IDC_CMDLINE:
        {
            char text[MAX_PATH] = "";
            if (save)
            {
                GetWindowText(hwnd, text, MAX_PATH - 1);
                WritePrivateProfileString("dialogs", key, text, INIFILE);
            }
            else
            {
                GetPrivateProfileString("dialogs", key, "", text, MAX_PATH - 1, INIFILE);
                SetWindowText(hwnd, text);
            }
            break;
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
                WritePrivateProfileString("dialogs", key, (IsDlgButtonChecked(MainDlg, id) == BST_CHECKED) ? "1" : "0", INIFILE);
            else
            {
                if (GetPrivateProfileInt("dialogs", key, def, INIFILE))
                    CheckDlgButton(MainDlg, id, 1);
            }
            break;
        }

        default:
            break;
    }

    return TRUE;
}

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_INITDIALOG:
        {
            HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
            SendMessage(hwndDlg, WM_SETICON, WPARAM(ICON_SMALL), LPARAM (hIcon));
            break;
        }

        case WM_COMMAND:
        {
            if (HIWORD(wParam) == BN_CLICKED)
            {
                switch (LOWORD(wParam))
                {
                case IDC_SCAN:
                case IDC_UPDATE:
                    {
                        if (isScanning) return true;
                        isScanning = TRUE;
                        DWORD m_dwThreadId;
                        HANDLE m_hThread = CreateThread(NULL, 0, PipeToClamAV, (LPVOID) MakeCmdLine(LOWORD(wParam)), 0, &m_dwThreadId);
                        CloseHandle(m_hThread);
                        break;
                    }
                case IDC_CANCEL:
                    {
                        if (!isScanning) return true;
                        if (pi.hProcess != INVALID_HANDLE_VALUE)
                            TerminateProcess(pi.hProcess, -1);
                        break;
                    }
                case IDC_BROWSE:
                    {
                        if (isScanning) return true;
                        GetDestinationFolder();
                    }
                }
            }
            return true;
        }
        case WM_CLOSE:
        {
            if (pi.hProcess != INVALID_HANDLE_VALUE)
                TerminateProcess(pi.hProcess, -1);
            EnumChildWindows(hwndDlg, EnumChildProc, (LPARAM) TRUE);
            DestroyWindow(hwndDlg);
            return true;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);       /* send a WM_QUIT to the message queue */
            return true;
        }
        default:
            return false;
    }
    return true;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MSG msg;
    pi.hProcess = INVALID_HANDLE_VALUE;
    LogMutex = CreateMutex(NULL, FALSE, "ClamAVGuiLoggerMutex");
    MainDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DialogProc);
    EnumChildWindows(MainDlg, EnumChildProc, (LPARAM) FALSE);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    CloseHandle(LogMutex);
    return (int) msg.wParam;
}

