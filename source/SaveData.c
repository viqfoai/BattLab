#include <windows.h>
#include <CommCtrl.h>
#include <stdio.h>
#include "TempChamberDriver.h"
#include "BatCycleController.h"
#include "AddTestStep.h"
#include "SaveData.h"
#include "TestExecute.h"
#include "resource.h"

const WORD wEncryptWord = 0xAA55;
HWND hProgBar = NULL, hDlg = NULL;

BOOL WINAPI SaveCloseLogFile(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD dwCurrPos, dwFileSize;
	switch (uMsg)
	{
	case WM_INITDIALOG:
		hDlg = hWnd;
		hProgBar = GetDlgItem(hWnd, IDC_PROGRESS_SAVING_DATA);
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SaveLogFileProc, (LPVOID)lParam, 0, NULL);
		break;
	case WM_UPDATE_PROGRESS_BAR:
		dwCurrPos = wParam;
		dwFileSize = lParam;
		DWORD dwProgPos = (MAXWORD / dwFileSize) ? dwCurrPos : MAXWORD * dwCurrPos / dwFileSize;
		SendMessage(hProgBar, PBM_SETPOS, dwProgPos, 0);
		break;
	case WM_CLOSE:
		EndDialog(hWnd, ERROR_FAIL);
		break;
	}

	return FALSE;
}

DWORD SaveLogFileProc(LPARAM lParam)
{
	BOOL bResult = FALSE;
	REC_ITEM *pRecItem;

	pRecItem = (REC_ITEM *)lParam;
	if (!pRecItem || !pRecItem->hLogFile)
		return ERROR_INVALID_PARAMETER;

	char szSrcFilePath[MAX_FILE_NAME_LENGTH] = { 0 };
	char szTrueSrcFilePath[MAX_FILE_NAME_LENGTH] = { 0 };
	char szTgtFilePath[MAX_FILE_NAME_LENGTH] = { 0 };
	//char szShortSrcFilePath[MAX_FILE_NAME_LENGTH] = { 0 };
	//char szTrueShortSrcFilePath[MAX_FILE_NAME_LENGTH] = { 0 };
	BYTE *pBufferByte, bEncryptByte = HIBYTE(wEncryptWord);
	DWORD dwByteNumber, uiFileSize;
	HANDLE hTgtFile;
	GetFinalPathNameByHandleA(pRecItem->hFakeLogFile, szSrcFilePath, MAX_FILE_NAME_LENGTH, FILE_NAME_NORMALIZED);
	GetFinalPathNameByHandleA(pRecItem->hLogFile, szTrueSrcFilePath, MAX_FILE_NAME_LENGTH, FILE_NAME_NORMALIZED);
	sprintf(szTgtFilePath, "%s%s", szSrcFilePath, "enc");
	hTgtFile = CreateFile(szTgtFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	UINT uiPointer = SetFilePointer(pRecItem->hLogFile, 0, NULL, FILE_BEGIN);
	uiFileSize = GetFileSize(pRecItem->hLogFile, NULL);
	UINT i = 0;
	LPSTR pBuffer = malloc(1024 * 1024);
	if (!pBuffer)
	{
		MessageBox(NULL, "Insufficient Memory", NULL, MB_OK);
		PostMessage(hDlg, WM_CLOSE, 0, 0);
		return ERROR_INSUFFICIENT_BUFFER;
	}

	pBufferByte = pBuffer;
	while (i < uiFileSize)
	{
		bResult = ReadFile(pRecItem->hLogFile, pBuffer, 1024 * 1024, &dwByteNumber, NULL);
		for (UINT j = 0; j < dwByteNumber; j++)
		{
			bEncryptByte = (bEncryptByte == HIBYTE(wEncryptWord)) ? LOBYTE(wEncryptWord) : HIBYTE(wEncryptWord);
			pBufferByte[j] ^= bEncryptByte;
			i++;
		}

		WriteFile(hTgtFile, pBuffer, dwByteNumber, &dwByteNumber, NULL);

		PostMessage(hDlg, WM_UPDATE_PROGRESS_BAR, i, uiFileSize);
	}

	free(pBuffer);
	CloseHandle(pRecItem->hLogFile);
	pRecItem->hLogFile = NULL;
	CloseHandle(pRecItem->hFakeLogFile);
	pRecItem->hFakeLogFile = NULL;
	//GetShortPathName(szSrcFilePath, szShortSrcFilePath, MAX_FILE_NAME_LENGTH);
	DeleteFile(szSrcFilePath);
	//SetFileShortName(hTgtFile, szShortSrcFilePath);
	CloseHandle(hTgtFile);
	DeleteFile(szTrueSrcFilePath);
	hTgtFile = NULL;

	PostMessage(hDlg, WM_CLOSE, 0, 0);
	return S_OK;
}