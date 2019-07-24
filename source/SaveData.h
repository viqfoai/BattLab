#pragma once

#define MAX_FILE_NAME_LENGTH	1024

DWORD SaveLogFileProc(LPARAM lParam);
BOOL WINAPI SaveCloseLogFile(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);