#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "TempChamberDriver.h"
#include "UARTTransceiver.h"
#include "BatCycleController.h"
#include "BasicFunction.h"
#include "UARTProcess.h"

DWORD WINAPI TempReadProcess(LPARAM lParam)
{
	TAB *TempAccessBlock;
	HRESULT Result;
	DWORD ReadBytes, ErrorCode;
	OVERLAPPED OverLappedResult = { 0 };		//must be 0ed here, otherwise, GetOverlapped() funcition will return with errorcode of 6, which means invalid handle, 'cause the envent handle in the Overlapped variable is uninitialised, typically 0xcccccccc.
	HANDLE *phTempMeter, *phTempEvent, *phTempMutex;
	BOOL *pbCloseCOMHandle;
	char szTempString[MAX_TEMP_STRING_LENGTH] = { 0 };

	if (NULL == (LPVOID)lParam)
	{
		MessageBox(NULL, "Incorrect Parameter", NULL, MB_OK);
		return ERROR_INVALID_PARAMETER;
	}
	else
	{
		TempAccessBlock = (TAB *)lParam;
		phTempMeter = TempAccessBlock->phCom;
		phTempMutex = TempAccessBlock->phTempMutex;
		pbCloseCOMHandle = &(TempAccessBlock->bCloseCOMHandle);  //this is a ping-pong switch for handshaking process when close the handle of COM port
		phTempEvent = TempAccessBlock->phEventTemp;
	}

	while (TRUE)
	{
		WaitForSingleObject(*phTempEvent, INFINITE);		//this event indicates the COM for Temperature measurment has been opened

		if (NULL == *phTempMeter)
		{
			ResetEvent(*phTempEvent);
			continue;
		}

		memset(szTempString, 0, MAX_TEMP_STRING_LENGTH);

		Result = ReadFile(*phTempMeter, szTempString, MAX_TEMP_STRING_LENGTH, &ReadBytes, &OverLappedResult);

		if (!Result)
		{
			ErrorCode = GetLastError();
			if (ERROR_IO_PENDING == ErrorCode)
			{
				Result = GetOverlappedResult(*phTempMeter, &OverLappedResult, &ReadBytes, TRUE);
				if (!Result)
				{
					MessageBox(NULL, "Get OverlpaaedResultFaile When Read!", NULL, MB_OK);
				}
				else
				{
					WaitForSingleObject(*phTempMutex, 2000);			//this mutex enables access to the pbCloseCOMHandle and szTempString. the COM can only be closed wich Mutex obtained by the thread.
					if (!(*pbCloseCOMHandle))
					{
						strncpy(TempAccessBlock->szTempString, szTempString, ReadBytes);
					}
					else
					{
						CloseHandle(*phTempMeter);
						*phTempMeter = NULL;
						*pbCloseCOMHandle = FALSE;
						ResetEvent(*phTempEvent);
					}
					EnforceReleaseMutex(*phTempMutex);
				}
			}
			else
			{
				CloseHandle(*phTempMeter);
				*phTempMeter = NULL;
				*pbCloseCOMHandle = FALSE;
				ResetEvent(*phTempEvent);
				MessageBox(NULL, "Read COM Fail", NULL, MB_OK);
			}
		}
		else
		{
			WaitForSingleObject(*phTempMutex, 2000);
			if (!(*pbCloseCOMHandle))
			{
				strncpy(TempAccessBlock->szTempString, szTempString, ReadBytes);
			}
			else
			{
				CloseHandle(*phTempMeter);
				*phTempMeter = NULL;
				*pbCloseCOMHandle = FALSE;
				ResetEvent(*phTempEvent);
			}
			EnforceReleaseMutex(*phTempMutex);
		}
	}

	return ERROR_SUCCESS;
}