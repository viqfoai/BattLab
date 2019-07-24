#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <visa.h>
#include "UARTTransceiver.h"
#include "TempChamberDriver.h"
#include "BatCycleController.h"
#include "BasicFunction.h"
#include "UARTProcess.h"
#include <InstrDrv.h>

extern BOOL bDisplayCounter;
extern BOOL bHideCondition;
extern char *szConditionType[];
extern TERM_COND *GetConditionPointerWithID(TERM_COND *pTopCondition, UINT uiConditionID);
extern UINT GetMaxConditionID(TERM_COND *pTermCondition);
extern TEST_PREFERENCE TestPreference;

extern char szPowerIDString[];
extern char szLoadIDString[];
extern char szVMeterIDString[];
extern char szIMeterIDString[];
extern PENCDATA pEncData;
char szYiHengModelString[MAX_MODEL_STRING_LENGTH] = "BluePard";

DATA_BUFFER VoltBuffer = { 0 }, CurrBuffer = { 0 }, TempBuffer = { 0 };

void ClearList(Element **StrList)
{
	Element *TempHolder = NULL, *PrevTempHolder = NULL;

	while (NULL != *StrList)
	{
		TempHolder = *StrList;
		PrevTempHolder = TempHolder;
		while (NULL != TempHolder->Next)
		{
			PrevTempHolder = TempHolder;
			TempHolder = TempHolder->Next;
		}

		free(TempHolder);

		if (TempHolder != PrevTempHolder)
		{
			PrevTempHolder->Next = NULL;
		}
		else
		{
			*StrList = NULL;
		}
	}
}

void AddStringToList(Element **StrList, char *NameString)
{
	Element *TempHolder;

	if (0 == strlen(NameString))
		return;

	if (NULL == *StrList)
	{
		*StrList = Malloc(sizeof(Element));
		(*StrList)->Next = NULL;
		TempHolder = *StrList;
	}
	else
	{
		TempHolder = *StrList;
		while (NULL != TempHolder->Next)
		{
			TempHolder = TempHolder->Next;
		}

		TempHolder->Next = Malloc(sizeof(Element));
		TempHolder = TempHolder->Next;
		TempHolder->Next = NULL;
	}

	TempHolder->Name = Malloc(strlen(NameString) + 1);
	strcpy(TempHolder->Name, NameString);
}

char *FindStringInList(Element *ListHeader, UINT AppointedPostionInList, UINT *ActualPositionInList)	//the number starting from 0
{
	Element *ElementPointer;
	char *StringPointer;
	UINT i = 0;

	if (NULL == ListHeader)
		return NULL;

	ElementPointer = ListHeader;

	for (i = 0; i < AppointedPostionInList; i++)
	{
		if (NULL != ElementPointer->Next)
		{
			ElementPointer = ElementPointer->Next;
		}
		else
			break;
	}

	StringPointer = ElementPointer->Name;
	*ActualPositionInList = i;

	if (*ActualPositionInList < AppointedPostionInList)
		return NULL;
	else
		return StringPointer;
}

HRESULT FindInstruments(ViSession *RM, ElementString *InstrDescList)
{
	ViStatus status;
	ViFindList fList;
	ViChar desc[VI_FIND_BUFLEN];
	UINT numInstrs;

	status = viOpenDefaultRM(RM);

	if (status < VI_ERROR_INV_OBJECT)
		return ERROR_DEV_NOT_EXIST;

	status = viFindRsrc(*RM, "GPIB?*INSTR", &fList, &numInstrs, desc);

	if (status < VI_SUCCESS) 
	{
		/* Error finding resources ... exiting */
		viClose(*RM);
		return ERROR_DEV_NOT_EXIST;
	}

	AddStringToList(InstrDescList, desc);

	if (numInstrs > MAX_INSTR_NUM)
	{
		viClose(fList);
		viClose(*RM);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	while (--numInstrs)
	{
		status = viFindNext(fList, desc);
		
		if (status < VI_SUCCESS)
		{
			viClose(fList);
			viClose(*RM);
			return ERROR_BAD_DEV_TYPE;
		}

		AddStringToList(InstrDescList, desc);
		
	}

	viClose(fList);
	/* Do not close defaultRM, as that would close instr too */
	return ERROR_SUCCESS;

}

HRESULT FindCOM(ElementString *PortList, ElementString *COMList)
{
	HKEY hKey;
	DWORD RetVal = ERROR_SUCCESS, Counter = 0, PortNameLength = MAX_BUFFER_SIZE, COMNameLength = MAX_BUFFER_SIZE, Type;
	CHAR PortName[MAX_BUFFER_SIZE] = { 0 }, COMName[MAX_BUFFER_SIZE] = { 0 };
	//FILETIME ft;

	ClearList(PortList);
	ClearList(COMList);

	RetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Hardware\\DeviceMap\\SerialComm", 0, KEY_READ, &hKey);

	if (ERROR_SUCCESS != RetVal)
		return RetVal;

	//QueryKey(hKey);

	while (TRUE)
	{

		PortNameLength = MAX_BUFFER_SIZE;
		COMNameLength = MAX_BUFFER_SIZE;
#ifdef RELEASE_DEBUG
		char szString[MAX_BUFFER_SIZE];
		sprintf(szString, "BMW: PortNameLength = %d, COMNameLength = %d", PortNameLength, COMNameLength);
		OutputDebugString(szString);
#endif
		memset(PortName, 0, MAX_BUFFER_SIZE);
		memset(COMName, 0, MAX_BUFFER_SIZE);
		//RetVal = RegEnumValue(hKey, Counter, PortName, &PortNameLength, NULL, &Type, COMName, &COMNameLength);
		RetVal = RegEnumValue(hKey, Counter, PortName, &PortNameLength, NULL, NULL, NULL, NULL);
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: after RegEnumValue Fnction");
#endif
		if (-1 == RetVal || ERROR_NO_MORE_ITEMS == RetVal)
			break;

		//RegGetValue(hKey, NULL, PortName, RRF_RT_ANY, &Type, NULL, &COMNameLength);
		RegGetValue(hKey, "", PortName, RRF_RT_ANY, &Type, COMName, &COMNameLength);
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: after RegGetValue Fnction");
#endif
		AddStringToList(PortList, PortName);
		AddStringToList(COMList, COMName);

		Counter++;
	}

	RegCloseKey(hKey);

	return RetVal;
}

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

void QueryKey(HKEY hKey)
{
	TCHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
	DWORD    cbName;                   // size of name string 
	TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
	DWORD    cchClassName = MAX_PATH;  // size of class string 
	DWORD    cSubKeys = 0;               // number of subkeys 
	DWORD    cbMaxSubKey;              // longest subkey size 
	DWORD    cchMaxClass;              // longest class string 
	DWORD    cValues;              // number of values for key 
	DWORD    cchMaxValue;          // longest value name 
	DWORD    cbMaxValueData;       // longest value data 
	DWORD    cbSecurityDescriptor; // size of security descriptor 
	FILETIME ftLastWriteTime;      // last write time 
	DWORD	 dwType;
	DWORD i, retCode;

	TCHAR  achValue[MAX_VALUE_NAME];
	DWORD cchValue = MAX_VALUE_NAME;
	TCHAR  szCOMName[MAX_VALUE_NAME];
	DWORD cchCOMName = MAX_VALUE_NAME;

	// Get the class name and the value count. 
	retCode = RegQueryInfoKey(
		hKey,                    // key handle 
		achClass,                // buffer for class name 
		&cchClassName,           // size of class string 
		NULL,                    // reserved 
		&cSubKeys,               // number of subkeys 
		&cbMaxSubKey,            // longest subkey size 
		&cchMaxClass,            // longest class string 
		&cValues,                // number of values for this key 
		&cchMaxValue,            // longest value name 
		&cbMaxValueData,         // longest value data 
		&cbSecurityDescriptor,   // security descriptor 
		&ftLastWriteTime);       // last write time 

								 // Enumerate the subkeys, until RegEnumKeyEx fails.

	if (cSubKeys)
	{
		printf("\nNumber of subkeys: %d\n", cSubKeys);

		for (i = 0; i<cSubKeys; i++)
		{
			cbName = MAX_KEY_LENGTH;
			retCode = RegEnumKeyEx(hKey, i,
				achKey,
				&cbName,
				NULL,
				NULL,
				NULL,
				&ftLastWriteTime);
			if (retCode == ERROR_SUCCESS)
			{
				char szDebugString[1024] = { 0 };
				sprintf(szDebugString, TEXT("BMW: (%d) %s\n"), i + 1, achKey);
				OutputDebugString(szDebugString);
				//_tprintf(TEXT("(%d) %s\n"), i + 1, achKey);
			}
		}
	}

	// Enumerate the key values. 

	if (cValues)
	{
		//printf("\nNumber of values: %d\n", cValues);

		for (i = 0, retCode = ERROR_SUCCESS; i<cValues; i++)
		{
			cchValue = MAX_VALUE_NAME;
			achValue[0] = '\0';
			retCode = RegEnumValue(hKey, i,
				achValue,
				&cchValue,
				NULL,
				&dwType,
				szCOMName,
				&cchCOMName);

			if (retCode == ERROR_SUCCESS)
			{
				//_tprintf(TEXT("(%d) %s\n"), i + 1, achValue);
				char szDebugString[1024] = { 0 };
				sprintf(szDebugString, TEXT("BMW: (%d) %s, %s\n"), i + 1, achValue, szCOMName);
				OutputDebugString(szDebugString);
			}
		}
	}
}

void UpdateComboBoxWithDevNameList(HWND hCombo, Element *DevNameList)
{
	Element *StrList = NULL;
	long RetVal = ERROR_SUCCESS;

	if (NULL == DevNameList)
		return;

	StrList = DevNameList;

	SendMessage(hCombo, CB_RESETCONTENT, 0, 0);

	while (NULL != StrList)
	{
		RetVal = SendMessage(hCombo, CB_FINDSTRING, 0, (LPARAM)StrList->Name);

		if (CB_ERR == RetVal)
		{
			SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)StrList->Name);
		}

		StrList = StrList->Next;
	}
}

HRESULT GetComboString(HWND hwnd, char *CombString)
{
	HRESULT ItemIndex;
	
	if (NULL == CombString)
		return ERROR_INVALID_PARAMETER;

	ItemIndex = SendMessage(hwnd, CB_GETCURSEL, 0, 0);

	if (CB_ERR == ItemIndex)
		return ERROR_RESOURCE_NOT_AVAILABLE;

	SendMessage(hwnd, CB_GETLBTEXT, (WPARAM)ItemIndex, (LPARAM)CombString);

	return ERROR_SUCCESS;
}

void GetSystemDataString(char *szData)
{
	SYSTEMTIME SysTime;

	GetLocalTime(&SysTime);
	sprintf(szData, "%4d/%02d/%02d", SysTime.wYear, SysTime.wMonth, SysTime.wDay);
}

void GetSystemTimeString(char *szTime)
{
	SYSTEMTIME SysTime;

	GetLocalTime(&SysTime);
	sprintf(szTime, "%02d:%02d:%02d", SysTime.wHour, SysTime.wMinute, SysTime.wSecond);
}

void GetTimer(BOOL bTimerStarted, UINT *pTimer )
{
	UINT Timer = *pTimer;
	static UINT uiStartTime;

	if (!bTimerStarted)
	{
		Timer = 0;
		uiStartTime = GetTickCount();
	}
	else
	{
		Timer = GetTickCount() - uiStartTime;
	}
	*pTimer = Timer;
	return;
}

void WriteRecordItem(HANDLE hFile, BOOL bTestStarted, BOOL SaveDate, REC_ITEM RecordItem)
{
	char szTempString[32] = { 0 };
	DWORD dWrittenBytes = 0, i;

	if (!bTestStarted)
		return;

	if (NULL == hFile)
		return;
#ifdef RELEASE_DEBUG
	OutputDebugString("BMW:In WriteRecordItem");
#endif // RELEASE_DEBUG

	if (!RecordItem.uiHighFrequenceyReadCounter)
	{
		if ((RecordItem.uiLogTimer) % (RecordItem.uiLogInterval))
			return;
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW:In WriteRecordItem 1");
#endif // RELEASE_DEBUG

		if (SaveDate)
		{
			sprintf(szTempString, "%s	", RecordItem.szDate);
			WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);
		}

		memset(szTempString, 0, 32);
		sprintf(szTempString, "%s	", RecordItem.szTime);
		WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

		/*
		memset(szTempString, 0, 32);
		sprintf(szTempString, "%d	", RecordItem.uiLogTimer);
		WriteFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL);
		*/
		memset(szTempString, 0, 32);
		sprintf(szTempString, "%d	", RecordItem.uiLogTimerMilliSec / 1000);
		WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

		if (RecordItem.bHighFrequencyDataNeeded)
		{
			memset(szTempString, 0, 32);
			sprintf(szTempString, "%.4lf	", RecordItem.uiLogTimerMilliSec / 1000.0);
			WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);
		}

		memset(szTempString, 0, 32);
		sprintf(szTempString, "%d	", RecordItem.iVoltage);
		WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

		memset(szTempString, 0, 32);
		sprintf(szTempString, "%.3lf	", RecordItem.dblCurrent);
		WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

		memset(szTempString, 0, 32);
		sprintf(szTempString, "%5.1f", RecordItem.iTemperature / 10.0);
		WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

		if (RecordItem.bMarkDsgState)
		{
			DWORD dwDsgState;
			dwDsgState = (RecordItem.dblCurrent >= 0) ? 0 : 3 - (INT)(log((INT)(((float)(-RecordItem.dblCurrent)) / ((float)(TestPreference.uBatteryCapaicty) / 100.0)) + 0.5) / log(3));
			memset(szTempString, 0, 32);
			sprintf(szTempString, "	%d", dwDsgState);
			WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);
		}

		memset(szTempString, 0, 32);
		sprintf(szTempString, "\n");
		WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

/*
		if (0 != RecordItem.uiHighFrequenceyReadCounter)
		{
			RecordItem.bHighFrequencyDataNeeded = TRUE;
		}
*/
	}
	else
	{
		float fVoltage, fCurrent;
		char *pVoltageBuffer, *pCurrentBuffer;

		pVoltageBuffer = RecordItem.VoltReading;
		pCurrentBuffer = RecordItem.CurrReading;

		for (i = 0; i < RecordItem.dwVoltReadingCount; i++)
		{
			if (SaveDate)
			{
				sprintf(szTempString, "%s	", RecordItem.szDate);
				WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);
			}

			memset(szTempString, 0, 32);
			sprintf(szTempString, "%s	", RecordItem.szTime);
			WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

			DOUBLE doubleHiFreqSampleInterval = ((LONG64)RecordItem.uiLogTimerMilliSec * 1000 + i * 977) / 1000000.0;
			memset(szTempString, 0, 32);
			sprintf(szTempString, "%d	", (UINT)doubleHiFreqSampleInterval);
			WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

			memset(szTempString, 0, 32);
			sprintf(szTempString, "%.4lf	", doubleHiFreqSampleInterval);
			WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

			memset(szTempString, 0, 32);
			sscanf(pVoltageBuffer, "%f,", &fVoltage);
			sprintf(szTempString, "%d	",(INT)(fVoltage * 1000));
			WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);
			pVoltageBuffer += ELEMENT_LENGTH;

			if (RecordItem.dwVoltReadingCount == RecordItem.dwCurrReadingCount)
			{
				memset(szTempString, 0, 32);
				sscanf(pCurrentBuffer, "%f,", &fCurrent);
				sprintf(szTempString, "%d	", (INT)(fCurrent * 1000));
				WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);
			}
			else
			{
				memset(szTempString, 0, 32);
				sprintf(szTempString, "%.3lf	", RecordItem.dblCurrent);
				WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);
			}
			pCurrentBuffer += ELEMENT_LENGTH;

			memset(szTempString, 0, 32);
			sprintf(szTempString, "%5.1f", RecordItem.iTemperature / 10.0);
			WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

			if (RecordItem.bMarkDsgState)
			{
				DWORD dwDsgState;
				dwDsgState = (RecordItem.dblCurrent >= 0) ? 0 : 3 - (INT)(log((INT)(((float)(-RecordItem.dblCurrent)) / ((float)(TestPreference.uBatteryCapaicty) / 100.0)) + 0.5) / log(3));
				memset(szTempString, 0, 32);
				sprintf(szTempString, "	%d", dwDsgState);
				WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);
			}

			memset(szTempString, 0, 32);
			sprintf(szTempString, "\n");
			WriteEncFile(hFile, szTempString, strlen(szTempString), &dWrittenBytes, NULL, pEncData);

		}
	}
}

BOOL UpdateVoltage(BOOL bAllInstrumentsOpened, ViSession viVoltMeter, char *szVoltage)
{
	float Voltage = 0;
	ViStatus viStatus;

	if (VI_NULL == viVoltMeter)
		return FALSE;

	if (bAllInstrumentsOpened)
	{
		viStatus = viScanf(viVoltMeter, "%f\n", &Voltage);
		if (VI_SUCCESS != viStatus)
		{
			viPrintf(viVoltMeter, "MEAS:CURR:DC?\n");
			viScanf(viVoltMeter, "%f\n", &Voltage);
		}
		viPrintf(viVoltMeter, "MEAS:VOLT:DC?\n");
		sprintf(szVoltage, "%4.0f", Voltage * 1000);
		return TRUE;
	}
	else
	{
		szVoltage = "";
		return FALSE;
	}
}

BOOL UpdateCurrent(BOOL bAllInstrumentsOpened, ViSession viCurrMeter, char *szCurrent)
{
	float Current = 0;
	ViStatus viStatus;

	if (VI_NULL == viCurrMeter)
		return FALSE;

	if (bAllInstrumentsOpened)
	{
		viStatus = viScanf(viCurrMeter, "%f\n", &Current);
		if (VI_SUCCESS != viStatus)
		{
			viStatus = viPrintf(viCurrMeter, "MEAS:CURR:DC?\n");
			viStatus = viScanf(viCurrMeter, "%f\n", &Current);
		}
		else
			viStatus = viPrintf(viCurrMeter, "MEAS:CURR:DC?\n");
		sprintf(szCurrent, "%4.0f", Current * 1000);
		return TRUE;
	}
	else
	{
		szCurrent = "";
		return FALSE;
	}
}

BOOL UpdateTemperature(BOOL bAllInstrumentsOpened, TAB *pTempAccessBlock, char *szTemperature)
{
	float Temperature = 0;
	char *szFirstTempString = NULL;

	if (NULL == pTempAccessBlock)
		return FALSE;

	if (bAllInstrumentsOpened)
	{
		EnforceReleaseMutex(pTempAccessBlock->phTempMutex);

		WaitForSingleObject(pTempAccessBlock->phTempMutex, INFINITE);

		szFirstTempString = strstr(pTempAccessBlock->szTempString, "t1=");
		if (NULL == szFirstTempString)
		{
			szTemperature = "";
			return FALSE;
		}
		sscanf(szFirstTempString, "t1=%f", &Temperature);
		//sprintf(szTemperature, "%5.1f", Temperature * 10.0 + 0.50);
		sprintf(szTemperature, "%5.1f", Temperature * 10.0);

		return TRUE;
	}
	else
	{
		szTemperature = "";
		return FALSE;
	}
}

BOOL UpdateRecordItem(BOOL bAllIntrumentsOpened, BOOL bLogStarted, TAB *pTempAccessBlock, REC_ITEM *pRecordItem, char *szDate, char *szTime)
{
	char szVoltage[32] = { 0 }, szCurrent[32] = { 0 }, szTemperature[32] = { 0 };
	static UINT uiVoltAnalyseWindow = MINIMUM_ANALYSE_WINDOW_LENGTH, uiCurrAnalyseWindow = MINIMUM_ANALYSE_WINDOW_LENGTH, uiTempAnalyseWindow = MINIMUM_ANALYSE_WINDOW_LENGTH;
	float fVoltage, fCurrent;
	INT iVoltSlop, iVoltIntercept, iVoltRMSE, iCurrSlop, iCurrIntercept, iCurrRMSE, iTempSlop, iTempIntercept, iTempRMSE;

	strcpy(pRecordItem->szDate, szDate);

	strcpy(pRecordItem->szTime, szTime);
	
	if (!bAllIntrumentsOpened)
		return FALSE;

	DllGetTimer(pRecordItem->bLogStarted, &pRecordItem->uiTempLogTimerMilliSec);
	pRecordItem->uiLogTimerMilliSec = pRecordItem->uiTempLogTimerMilliSec;
	pRecordItem->uiLogTimer = pRecordItem->uiTempLogTimerMilliSec / 1000;
	
//	if (!pRecordItem->bHighFrequencyDataNeeded)
//	{

	sscanf(pRecordItem->VoltReading, "%f", &(fVoltage));
	pRecordItem->iVoltage = (INT)(fVoltage * 1000);
	if (pRecordItem->bChargerConnected || pRecordItem->bLoadConnected)
	{
		sscanf(pRecordItem->CurrReading, "%f", &(fCurrent));
		pRecordItem->dblCurrent = fCurrent * 1000;
	}
	else
	{
		pRecordItem->dblCurrent = 0;
	}

	if (!UpdateTemperature(bAllIntrumentsOpened, pTempAccessBlock, szTemperature))
		return FALSE;

	sscanf(szTemperature, "%d", &(pRecordItem->iTemperature));

	pRecordItem->iAvgVoltage = AverageOnTimeWindow(pRecordItem->iVoltage, pRecordItem->iAvgVoltage, &(pRecordItem->uiAverageVoltageCounter), pRecordItem->uiVoltageAverageWindowLength);
	pRecordItem->iAvgCurrent = AverageOnTimeWindow((INT)(pRecordItem->dblCurrent), pRecordItem->iAvgCurrent, &(pRecordItem->uiAverageCurrentCounter), pRecordItem->uiCurrentAverageWindowLength);
	pRecordItem->iAvgTemperature = AverageOnTimeWindow(pRecordItem->iTemperature, pRecordItem->iAvgTemperature, &(pRecordItem->uiAverageTempCounter), pRecordItem->uiTempAverageWindowLength);

	AddDataToBuffer(&VoltBuffer, pRecordItem->iVoltage);
	AddDataToBuffer(&CurrBuffer, (INT)(pRecordItem->dblCurrent));
	AddDataToBuffer(&TempBuffer, pRecordItem->iTemperature);

	UINT uiVoltBufferLength = GetBufferLength(&VoltBuffer);
	if (0 != uiVoltBufferLength)
	{
		if (AnaylysisData(&VoltBuffer, &iVoltSlop, &iVoltIntercept, &iVoltRMSE, &uiVoltAnalyseWindow))
		{
			pRecordItem->iVoltRate = iVoltSlop;
			pRecordItem->iVoltRMSE = iVoltRMSE;
			if (MINIMUM_ANALYSE_WINDOW_LENGTH < uiVoltBufferLength)
				pRecordItem->iVoltWindow = uiVoltAnalyseWindow;
			else
				pRecordItem->iVoltWindow = uiVoltBufferLength;

			if (iVoltRMSE <= TRUST_THRESHOLD || BAD_RATE_DATA == iVoltRMSE)    //RMSE should be the smaller, the better
			{
				if (uiVoltAnalyseWindow < MAX_BUFFER_SIZE && (uiVoltBufferLength >= MINIMUM_ANALYSE_WINDOW_LENGTH))
				{
					uiVoltAnalyseWindow++;
				}
			}
			else
			{
				if (uiVoltAnalyseWindow > MINIMUM_ANALYSE_WINDOW_LENGTH)
				{
					uiVoltAnalyseWindow--;
				}
			}
#ifdef RELEASE_DEBUG
			char szOutputString[MAX_BUFFER_SIZE] = { 0 };
			sprintf(szOutputString, "BMW: Volt Analyse Window: %d", uiVoltAnalyseWindow);
			OutputDebugString(szOutputString);
#endif // RELEASE_DEBUG
		}
	}

	UINT uiCurrBufferLength = GetBufferLength(&CurrBuffer);
	if (0 != uiCurrBufferLength)
	{
		if (AnaylysisData(&CurrBuffer, &iCurrSlop, &iCurrIntercept, &iCurrRMSE, &uiCurrAnalyseWindow))
		{
			pRecordItem->iCurrRate = iCurrSlop;
			pRecordItem->iCurrRMSE = iCurrRMSE;
			if (MINIMUM_ANALYSE_WINDOW_LENGTH < uiCurrBufferLength)
				pRecordItem->iCurrWindow = uiCurrAnalyseWindow;
			else
				pRecordItem->iCurrWindow = uiCurrBufferLength;

			if (iCurrRMSE <= TRUST_THRESHOLD || BAD_RATE_DATA == iCurrRMSE)
			{
				if (uiCurrAnalyseWindow < MAX_BUFFER_SIZE && (uiCurrBufferLength >= MINIMUM_ANALYSE_WINDOW_LENGTH))
					uiCurrAnalyseWindow++;
			}
			else
			{
				if (uiCurrAnalyseWindow > MINIMUM_ANALYSE_WINDOW_LENGTH)
						uiCurrAnalyseWindow--;
			}
		}
	}

	UINT uiTempBufferLength = GetBufferLength(&TempBuffer);
	if (0 != uiTempBufferLength)
	{
		if (AnaylysisData(&TempBuffer, &iTempSlop, &iTempIntercept, &iTempRMSE, &uiTempAnalyseWindow))
		{
			pRecordItem->iTempRate = iTempSlop;
			pRecordItem->iTempRMSE = iTempRMSE;
			if (MINIMUM_ANALYSE_WINDOW_LENGTH < uiTempBufferLength)
				pRecordItem->iTempWindow = uiTempAnalyseWindow;
			else
				pRecordItem->iTempWindow = uiTempBufferLength;

			if (iTempRMSE <= TRUST_THRESHOLD || BAD_RATE_DATA == iTempRMSE)
			{
				if (uiTempAnalyseWindow < MAX_BUFFER_SIZE && (uiTempBufferLength >= MINIMUM_ANALYSE_WINDOW_LENGTH))
					uiTempAnalyseWindow++;
			}
			else
			{
				if (uiTempAnalyseWindow > MINIMUM_ANALYSE_WINDOW_LENGTH)
					uiTempAnalyseWindow--;
			}
		}
	}

		//Below calculate the passed charge
	if (pRecordItem->bPassedChargeOn)
	{
		pRecordItem->uiPassedChargeCounter++;
		//pRecordItem->llPassedCharge += (pRecordItem->uiPassedChargeCounter == 1) ? pRecordItem->iCurrent / 2 : pRecordItem->iCurrent;
		//pRecordItem->llPresentPassedCharge = (pRecordItem->uiPassedChargeCounter > 1) ? pRecordItem->llPassedCharge : pRecordItem->llPresentPassedCharge - pRecordItem->iCurrent / 2;
		pRecordItem->dblPassedCharge += pRecordItem->dblCurrent * pRecordItem->uiLogInterval;	//added for very low capacity cell, the current can be less than 1mA
		pRecordItem->dblPresentPassedCharge = pRecordItem->dblPassedCharge /3600.0;
		pRecordItem->llPassedCharge = (INT)(pRecordItem->dblPassedCharge);
		pRecordItem->llPresentPassedCharge = (INT)pRecordItem->dblPresentPassedCharge;
		//pRecordItem->llPresentPassedCharge = (pRecordItem->llPresentPassedCharge >= 3600) ? pRecordItem->llPresentPassedCharge / 3600, 0;
	}
//	else
//	{
			//clear passed charge;
//		pRecordItem->uiPassedChargeCounter = 0;
//		pRecordItem->llPassedCharge = 0;
//		pRecordItem->llPresentPassedCharge = 0;
//	}
	//}

	return TRUE;
}

void UpdateRecordItemDisplay(BOOL bAllInstrumentsOpened, HWND EditDateHwnd, HWND EditTimeHwnd, HWND EditTimerHwnd, HWND EditVoltHwnd, HWND EditCurrHwnd, HWND EditTempHwnd, REC_ITEM *pRecordItem)
{
	char szTempString[20] = { 0 };

	if (pRecordItem->uiHighFrequenceyReadCounter)
		return;

	SetWindowText(EditDateHwnd, pRecordItem->szDate);

	SetWindowText(EditTimeHwnd, pRecordItem->szTime);

	sprintf(szTempString, "%d", pRecordItem->uiLogTimer);
	SetWindowText(EditTimerHwnd, szTempString);

	if (bAllInstrumentsOpened)
	{
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW:Updating Record Items");
#endif
		sprintf(szTempString, "%d", pRecordItem->iVoltage);
		SetWindowText(EditVoltHwnd, szTempString);

		sprintf(szTempString, "%.3lf", pRecordItem->dblCurrent);
		SetWindowText(EditCurrHwnd, szTempString);

		sprintf(szTempString, "%5.1f", pRecordItem->iTemperature / 10.0);
		SetWindowText(EditTempHwnd, szTempString);
	}
	else
	{
		SetWindowText(EditVoltHwnd, "");
		SetWindowText(EditCurrHwnd, "");
		SetWindowText(EditTempHwnd, "");
	}
}

HRESULT OpenGPIBDeviceInCombo(HWND hCombo, ViSession DefaultRM, ViPSession pVi, LPTSTR pInstrIDString)
{
	HRESULT Result = ERROR_SUCCESS, ItemIndex, TempResult = 0;
	ViStatus status;
	char szDevString[MAX_STRING_LENGTH] = { 0 };

	if (NULL == hCombo)
		return ERROR_INVALID_PARAMETER;

	if (VI_NULL != *pVi)
		return ERROR_SUCCESS;

	ItemIndex = SendMessage(hCombo, CB_GETCURSEL, (WPARAM)NULL, (LPARAM)NULL);
	if (CB_ERR == ItemIndex)
	{
		MessageBox(NULL, "Instrument has not been selected", NULL, MB_OK);
		Result |= GET_COMBO_INDEX_FAIL;
	}
	else
	{
		TempResult |= SendMessage(hCombo, CB_GETLBTEXT, (WPARAM)ItemIndex, (LPARAM)szDevString);
		if (CB_ERR != TempResult)
		{
			status = viOpen(DefaultRM, szDevString, (ViAccessMode)NULL, (ViUInt32)NULL, pVi);
			if (VI_SUCCESS != status)
			{
				MessageBox(NULL, "Can not Open Instrument", NULL, MB_OK);
				Result |= OPEN_DEVICE_FAIL;
			}
			else
			{
				UINT uiCount;
				status = viWrite(*pVi, "*CLS\n", 5, &uiCount);
				status = viWrite(*pVi, "*RST\n", 5, &uiCount);
				status = viWrite(*pVi, "*IDN?\n", 6, &uiCount);
				status = viRead(*pVi, pInstrIDString, 200, &uiCount);

				if (strstr(pInstrIDString, "34401") || strstr(pInstrIDString, "34410"))
				{
					//status = viWrite(*pVi, "VOLT:RANGE:AUTO OFF\n", sizeof("VOLT:RANGE:AUTO OFF\n"), &uiCount);
					status = viWrite(*pVi, "ZERO:AUTO OFF\n", strlen("ZERO:AUTO OFF\n"), &uiCount);
					status = viWrite(*pVi, "CALC:STAT OFF\n", strlen("CALC:STAT OFF\n"), &uiCount);
					status = viWrite(*pVi, "TRIG:SOUR IMM\n", strlen("TRIG:SOUR IMM\n"), &uiCount);
				}

				EnableWindow(hCombo, FALSE);
			}
		}
		else
		{
			MessageBox(NULL, "Can not get ComboBox text", NULL, MB_OK);
			Result |= GET_COMBO_TEXT_FAIL;
		}
	}

	return Result;
}

HRESULT OpenComDeviceInCombo(HWND ComboCOMCtrlHwnd, HANDLE *phCom, DWORD dwFlagsAndAttributes)
{
	HRESULT Result = ERROR_SUCCESS, ItemIndex, TempResult = 0;
	char szDevString[32] = { "\\\\.\\" };

	if (NULL == ComboCOMCtrlHwnd)
		return ERROR_INVALID_PARAMETER;

	if (NULL != *phCom)
		return ERROR_SUCCESS;

	ItemIndex = SendMessage(ComboCOMCtrlHwnd, CB_GETCURSEL, (WPARAM)NULL, (LPARAM)NULL);
	if (CB_ERR == ItemIndex)
	{
		MessageBox(NULL, "No COM Port Selected", NULL, MB_OK);
		Result |= GET_COMBO_INDEX_FAIL;
	}
	else
	{
		TempResult |= SendMessage(ComboCOMCtrlHwnd, CB_GETLBTEXT, ItemIndex, (LPARAM)&(szDevString[4]));
		if (CB_ERR != TempResult)
		{
			*phCom = CreateFile(szDevString, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, dwFlagsAndAttributes, 0);	//FILE_FLAG_OVERLAPPED是指用异步方式打开串口
			DWORD dwError = GetLastError();
			if (INVALID_HANDLE_VALUE != *phCom)
			{
				PurgeComm(*phCom, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
				EnableWindow(ComboCOMCtrlHwnd, FALSE);
			}
			else
			{
				MessageBox(NULL, "Open COM Failed", NULL, MB_OK);
				Result |= OPEN_DEVICE_FAIL;
			}
		}
		else
		{
			MessageBox(NULL, "Can not get ComboBox text", NULL, MB_OK);
			Result |= GET_COMBO_TEXT_FAIL;
		}
	}

	return Result;
}

HRESULT OpenTempChamberDeviceInCombo(HWND ComboTempChamberHwnd, HANDLE *phTempChamber)
{
	HRESULT Result = ERROR_SUCCESS, ItemIndex, TempResult = 0;
	char szDevString[32] = { "\\\\.\\" };

	if (NULL == ComboTempChamberHwnd)
		return ERROR_INVALID_PARAMETER;

	if (NULL != *phTempChamber)
		return ERROR_SUCCESS;

	ItemIndex = SendMessage(ComboTempChamberHwnd, CB_GETCURSEL, (WPARAM)NULL, (LPARAM)NULL);
	if (CB_ERR == ItemIndex)
	{
		MessageBox(NULL, "No COM Port Selected", NULL, MB_OK);
		Result |= GET_COMBO_INDEX_FAIL;
	}
	else
	{
		TempResult |= SendMessage(ComboTempChamberHwnd, CB_GETLBTEXT, ItemIndex, (LPARAM)&(szDevString[4]));
		if (CB_ERR != TempResult)
		{
			*phTempChamber = DllOpenTempChamber(szDevString, szYiHengModelString);	
			if (INVALID_HANDLE_VALUE != *phTempChamber)
			{
				//PurgeComm(*phCom, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
				EnableWindow(ComboTempChamberHwnd, FALSE);
			}
			else
			{
				MessageBox(NULL, "Open Temp Chamber Failed", NULL, MB_OK);
				Result |= OPEN_DEVICE_FAIL;
			}
		}
		else
		{
			MessageBox(NULL, "Can not get ComboBox text", NULL, MB_OK);
			Result |= GET_COMBO_TEXT_FAIL;
		}
	}

	return Result;
}

BOOL SaveStepToFile(HANDLE hStepFile, TEST_SCHEDULE_PTR pTestSchedule, TEST_PREFERENCE *pTestPreference, LOG_OPTION *pLogOption, BOOL bHideCond)
{
	TEST_STEP *pTestStep = NULL;
	UINT uiWritenBytes = 0, uiFileSize, uiPointer;
	BOOL bResult = TRUE;
	INT iCheckSum = 0;

	pTestStep = pTestSchedule;
	if (NULL == hStepFile || NULL == pTestStep)
		return FALSE;

	if (NULL == pTestStep->pNextStep)
		return FALSE;

	while (bResult && NULL != pTestStep)
	{
		bResult = WriteFile(hStepFile, pTestStep, sizeof(TEST_STEP), &uiWritenBytes, NULL);
		bResult &= SaveConditionToFile(hStepFile, pTestStep->pTerminateCondition);
		pTestStep = pTestStep->pNextStep;
	}

	bResult = WriteFile(hStepFile, pTestPreference, sizeof(TEST_PREFERENCE), &uiWritenBytes, NULL);
	bResult = WriteFile(hStepFile, pLogOption, sizeof(LOG_OPTION), &uiWritenBytes, NULL);

	uiPointer = SetFilePointer(hStepFile, 0, NULL, FILE_BEGIN);
	uiFileSize = GetFileSize(hStepFile, NULL);

	unsigned char ucData;
	UINT uiNumberOfBytes;
	for (UINT i = 0; i < uiFileSize; i++)
	{
		bResult = ReadFile(hStepFile, &ucData, 1, &uiNumberOfBytes, NULL);
		iCheckSum += ucData;
	}

//	uiPointer = SetFilePointer(hStepFile, 0, NULL, FILE_CURRENT);
//	uiPointer = SetFilePointer(hStepFile, uiFileSize - 1, NULL, FILE_BEGIN);
#ifdef _DEBUG
	//bResult = ReadFile(hStepFile, &ucData, 1, &uiWritenBytes, NULL);
#endif // _DEBUG

	if (bHideCond)
		iCheckSum = -iCheckSum;

	bResult = WriteFile(hStepFile, &iCheckSum, sizeof(INT), &uiWritenBytes, NULL);

#ifdef _DEBUG
	DWORD dwError = GetLastError();
#endif

	return bResult;
}

BOOL SaveConditionToFile(HANDLE hStepFile, TERM_COND *pCondition)
{
	if (NULL == hStepFile)
		return FALSE;

	if (NULL == pCondition)
		return TRUE;

	return(RecursiveSaveCondition(hStepFile, pCondition));
}

BOOL RecursiveSaveCondition(HANDLE hStepFile, TERM_COND *pCondition)
{
	UINT uiNumberOfBytes;
	BOOL bResult;

	bResult = WriteFile(hStepFile, pCondition, sizeof(TERM_COND), &uiNumberOfBytes, NULL);

	if (!bResult)
		return bResult;

	if (pCondition->pLeftCondition)
		bResult = RecursiveSaveCondition(hStepFile, pCondition->pLeftCondition);

	if (!bResult)
		return bResult;

	if (pCondition->pRightCondition)
		bResult = RecursiveSaveCondition(hStepFile, pCondition->pRightCondition);

	return bResult;
}

BOOL LoadConditionFromFile(HANDLE hStepFile, TERM_COND **ppCondition)
{
	BOOL bResult = FALSE;

	if (NULL == hStepFile)
		return FALSE;

	if (NULL == *ppCondition)
		return TRUE;

	return(RecursiveLoadCondition(hStepFile, ppCondition));
}

BOOL RecursiveLoadCondition(HANDLE hStepFile, TERM_COND **ppCondition)
{
	UINT uiNumberOfBytes;
	BOOL bResult = TRUE;

	*ppCondition = Malloc(sizeof(TERM_COND));

	bResult = ReadFile(hStepFile, *ppCondition, sizeof(TERM_COND), &uiNumberOfBytes, NULL);

	if (!bResult)
		return FALSE;

	if (NULL != (*ppCondition)->pLeftCondition)
		bResult = RecursiveLoadCondition(hStepFile, &((*ppCondition)->pLeftCondition));

	if (!bResult)
		return FALSE;

	if (NULL != (*ppCondition)->pRightCondition)
		bResult = RecursiveLoadCondition(hStepFile, &((*ppCondition)->pRightCondition));

	return bResult;
}

BOOL LoadStepFromFile(HANDLE hStepFile, TEST_SCHEDULE_PTR *ppTestSchedule, TEST_PREFERENCE *pTestPreference, LOG_OPTION *pLogOption, BOOL *pbHideCondition)
{
	BOOL bResult;
	TEST_STEP *pTestStep;
	UINT uiNumberOfBytes;
	INT iCheckSumRead, iCheckSumCalc = 0;

	if (NULL == hStepFile)
		return FALSE;

	if (NULL != *ppTestSchedule)
		return FALSE;

	*ppTestSchedule = Malloc(sizeof(TEST_STEP));
	pTestStep = *ppTestSchedule;

	do
	{
		bResult = ReadFile(hStepFile, pTestStep, sizeof(TEST_STEP), &uiNumberOfBytes, NULL);

		if (!bResult)
			break;

		bResult = LoadConditionFromFile(hStepFile, &(pTestStep->pTerminateCondition));
		if (!bResult)
			break;

		if (NULL != pTestStep->pNextStep)
			pTestStep->pNextStep = Malloc(sizeof(TEST_STEP));

		pTestStep = pTestStep->pNextStep;
	} while(NULL != pTestStep);

	bResult = ReadFile(hStepFile, pTestPreference, sizeof(TEST_PREFERENCE), &uiNumberOfBytes, NULL);
	bResult = ReadFile(hStepFile, pLogOption, sizeof(LOG_OPTION), &uiNumberOfBytes, NULL);
	bResult = ReadFile(hStepFile, &iCheckSumRead, sizeof(INT), &uiNumberOfBytes, NULL);

	SetFilePointer(hStepFile, 0, NULL, FILE_BEGIN);
	UINT uiFileSize = GetFileSize(hStepFile, NULL);

	for (UINT i = 0; i < uiFileSize - sizeof(INT); i++)
	{
		unsigned char ucData;
		bResult = ReadFile(hStepFile, &ucData, 1, &uiNumberOfBytes, NULL);
		iCheckSumCalc += ucData;
	}

	if (iCheckSumRead == iCheckSumCalc && pbHideCondition)
		*pbHideCondition = FALSE;
	else
		*pbHideCondition = TRUE;

	return bResult;
}

void UpdateTestStatus(TEST_STEP *pStep, LPTSTR szTestStatusString, REC_ITEM *pRecItem)
{
	LPTSTR pStrPointer;
	UINT uiMaxConditionID, uiConditionID, i;
	TERM_COND *pCondition = NULL;

	if (NULL == szTestStatusString)
		return;

	pStrPointer = szTestStatusString;

	sprintf(pStrPointer, "STEP %d: ", pStep->uiStepNumber);
	pStrPointer += strlen(pStrPointer);

	uiMaxConditionID = GetMaxConditionID(pStep->pTerminateCondition);

	if (bDisplayCounter)
	{
		sprintf(pStrPointer, "Counter = %d;	", pRecItem->iCounter);
		pStrPointer += strlen(pStrPointer);
	}

	for (uiConditionID = 0; uiConditionID <= uiMaxConditionID; uiConditionID++)
	{
		pCondition = GetConditionPointerWithID(pStep->pTerminateCondition, uiConditionID);

		if (NULL == pCondition)
			continue;

		for (i = 0; i < uiConditionID; i++)
		{
			TERM_COND *pPrvConditon = GetConditionPointerWithID(pStep->pTerminateCondition, i);
			if (pPrvConditon->ConditionType == pCondition->ConditionType)
				break;
		}

		if (i < uiConditionID)
			continue;					//i <= uiConditionID indicates the condition with same condition type has been processed.

		if (strlen(szTestStatusString) >= MAX_TEST_STATUS_STRING_LENGTH - 50)
			break;

		if (Bifurcation != pCondition->ConditionType)
		{
			if (Counter != pCondition->ConditionType)
				sprintf(pStrPointer, "%s = ", szConditionType[pCondition->ConditionType]);
			pStrPointer += strlen(pStrPointer);
			switch (pCondition->ConditionType)
			{
			case Voltage:
				sprintf(pStrPointer, "%dmV;	", pRecItem->iVoltage);
				pStrPointer += strlen(pStrPointer);
				break;
			case Current:
				sprintf(pStrPointer, "%.3lfmA;	", pRecItem->dblCurrent);
				pStrPointer += strlen(pStrPointer);
				break;
			case Temperature:
				sprintf(pStrPointer, "%5.1f   ", pRecItem->iTemperature / 10.0 + 0.05);
				pStrPointer += strlen(pStrPointer);
				break;
			case VoltageRate:
				sprintf(pStrPointer, "%dμV/S;	WinLength %d;	VoltRMSE%d;	", pRecItem->iVoltRate, pRecItem->iVoltWindow, pRecItem->iVoltRMSE);
				pStrPointer += strlen(pStrPointer);
				break;
			case CurrentRate:
				sprintf(pStrPointer, "%dμA/S;	WinLength %d;	CurrRMSE%d;	", pRecItem->iCurrRate, pRecItem->iCurrWindow, pRecItem->iCurrRMSE);
				pStrPointer += strlen(pStrPointer);
				break;
			case AvgVoltage:
				sprintf(pStrPointer, "%dmV;	", pRecItem->iAvgVoltage);
				pStrPointer += strlen(pStrPointer);
				break;
			case AvgCurrent:
				sprintf(pStrPointer, "%dmA;	", pRecItem->iAvgCurrent);
				pStrPointer += strlen(pStrPointer);
				break;
			case AllTime:
				sprintf(pStrPointer, "%dS;	", pRecItem->uiStepTimer);
				pStrPointer += strlen(pStrPointer);
				break;
			case ConditionTime:
				sprintf(pStrPointer, "%dS;	", pCondition->uiConditionTimer);
				pStrPointer += strlen(pStrPointer);
				break;
			case PassedCharge:
				sprintf(pStrPointer, "%.5lgmAh;	", pRecItem->dblPresentPassedCharge);
				pStrPointer += strlen(pStrPointer);
				break;
			case Counter:
			//	sprintf(pStrPointer, "%d;	", pRecItem->iCounter);
			//	pStrPointer += strlen(pStrPointer);
				break;
			default:
				break;
			}
		}
	}
}

int AverageOnTimeWindow(INT iInputData, INT iPreAveragedData, UINT *puiAverageCounter, UINT uiWindowCounter)
{
	INT AverageData = 0;
	INT AverageData1;

	*puiAverageCounter = *puiAverageCounter + (1 - *puiAverageCounter / uiWindowCounter);
	AverageData1 = iPreAveragedData * (*puiAverageCounter - 1) + iInputData;
	AverageData = (INT)(AverageData1/ (float)(*puiAverageCounter) + 0.5);

	return AverageData;

}

BOOL AddDataToBuffer(DATA_BUFFER *pBuffer, INT iData)
{
	if (NULL == pBuffer)
		return FALSE;

	if (pBuffer->uiHeadIndex >= MAX_BUFFER_SIZE || pBuffer->uiTailIndex >= MAX_BUFFER_SIZE)
		return FALSE;

	if ((pBuffer->uiHeadIndex + 1) % MAX_BUFFER_SIZE == pBuffer->uiTailIndex)		//如果队列头加1要碰到队列尾，就把队列尾加1，这样就相当于把原来的队尾数据丢弃了
	{
		pBuffer->uiTailIndex = (pBuffer->uiTailIndex + 1) % MAX_BUFFER_SIZE;
	}

	pBuffer->uiHeadIndex = (pBuffer->uiHeadIndex + 1) % MAX_BUFFER_SIZE;
	pBuffer->iDataItem[pBuffer->uiHeadIndex] = iData;
	return TRUE;

}

BOOL AddDoubleToBuffer(FLOAT_BUFFER *pBuffer, double dblData)
{
	if (NULL == pBuffer)
		return FALSE;

	if (pBuffer->uiHeadIndex >= MAX_BUFFER_SIZE || pBuffer->uiTailIndex >= MAX_BUFFER_SIZE)
		return FALSE;

	if ((pBuffer->uiHeadIndex + 1) % MAX_BUFFER_SIZE == pBuffer->uiTailIndex)		//如果队列头加1要碰到队列尾，就把队列尾加1，这样就相当于把原来的队尾数据丢弃了
	{
		pBuffer->uiTailIndex = (pBuffer->uiTailIndex + 1) % MAX_BUFFER_SIZE;
	}

	pBuffer->uiHeadIndex = (pBuffer->uiHeadIndex + 1) % MAX_BUFFER_SIZE;
	pBuffer->dblDataItem[pBuffer->uiHeadIndex] = dblData;
	return TRUE;

}

UINT GetBufferLength(DATA_BUFFER *pBuffer)
{
	if (pBuffer->uiHeadIndex > pBuffer->uiTailIndex)
		return (pBuffer->uiHeadIndex - pBuffer->uiTailIndex);
	else
		return MAX_BUFFER_SIZE;
}

INT ReadDataFromBuffer(DATA_BUFFER *pBuffer, UINT uiIndexFromHead)		//this index count from 0
{
	UINT uiIndex;

	uiIndex = ((INT)(pBuffer->uiHeadIndex - uiIndexFromHead) >= 0) ? pBuffer->uiHeadIndex - uiIndexFromHead : MAX_BUFFER_SIZE - uiIndexFromHead + pBuffer->uiHeadIndex;

	return (pBuffer->iDataItem[uiIndex]);
}

BOOL AnaylysisData(DATA_BUFFER *pBuffer, INT *piSlop, INT *piIntercept, INT *piRMSE, UINT *puiDataLength)
{
	INT64 i = 0, Y = 0, Yi = 0, iSumX = 0, iSumY = 0, iSumX2 = 0, iSumY2 = 0, iSumXY = 0, iSumYi = 0, iSumYi2 = 0, iSumYi_Y_2 = 0;
	UINT X = 0, uiAvailableLength = 0;
	
	uiAvailableLength = GetBufferLength(pBuffer);

	if (NULL == pBuffer \
		|| *puiDataLength > MAX_BUFFER_SIZE 
		|| pBuffer->uiHeadIndex > MAX_BUFFER_SIZE \
		|| pBuffer->uiTailIndex > MAX_BUFFER_SIZE\
		|| *puiDataLength < MINIMUM_ANALYSE_WINDOW_LENGTH)
		return FALSE;

	if (2 >= uiAvailableLength)
	{
		*piSlop = *piIntercept = 0;
		*piRMSE = BAD_RATE_DATA;
		return TRUE;
	}

	if (*puiDataLength > uiAvailableLength)
		*puiDataLength = uiAvailableLength;

	for (X = 0; X < *puiDataLength; X++)
	{
		Y = ReadDataFromBuffer(pBuffer, *puiDataLength - X - 1);
		iSumX += X;
		iSumY += Y;
		iSumX2 += X*X;
		iSumY2 += Y*Y;
		iSumXY += X*Y;
	}

	INT iSign = ((INT)((INT64)(*puiDataLength*iSumXY - iSumX*iSumY) / (double)(*puiDataLength*iSumX2 - iSumX*iSumX) * 1000)) >> 31;
	double fSlop = (INT64)(*puiDataLength*iSumXY - iSumX*iSumY) / (double)(*puiDataLength*iSumX2 - iSumX*iSumX) * 1000 + (iSign);
	*piSlop =(fSlop > 0.00000)?(INT)(fSlop + 0.5) : (INT)(fSlop - 0.5);
	iSign = (INT)((INT64)((INT64)iSumY - (*piSlop / 1000.0)*iSumX) / (double)(*puiDataLength)) >> 31;
	double fIntercept = ((INT64)iSumY - (*piSlop / 1000.0)*iSumX) / (double)(*puiDataLength) + (iSign);
	*piIntercept = (fIntercept > 0.0000)?(INT)(fIntercept + 0.5) : (INT)(fIntercept - 0.5);

	for (X = 0; X < *puiDataLength; X++)
	{
		Y = ReadDataFromBuffer(pBuffer, *puiDataLength - X - 1);
		iSign = (*piSlop*(INT)X) >> 31;
		Yi = (INT)(*piSlop / 1000.0*(INT)X + (*piSlop*(INT)X >> 31) + 0.5) + (INT)(*piIntercept);
		iSumYi_Y_2 += (Y-Yi)*(Y-Yi);
	}
	

	if (*puiDataLength > 2)
		*piRMSE = (INT)(sqrt(iSumYi_Y_2 / (float)(*puiDataLength - 2))*1000);
	else
		*piRMSE = 0x80000000;
		//(INT)(1000.0*(iSumYiY - (INT)((iSumY*iSumYi) / (float)(*puiDataLength)))/ sqrt((iSumY2 - (INT)(iSumY*iSumY / (float)(*puiDataLength)))*(iSumYi2 - (INT)(iSumYi*iSumYi / (float)(*puiDataLength)))));
	return TRUE;
	//[∑XiYi - m （∑Xi / m）（∑Yi / m)] / SQR{ [∑Xi2 - m （∑Xi / m)2][∑Yi2 - m （∑Yi / m)2] }
}


BOOL IsNumerical(LPTSTR szInputString)
{
	char *pCharactor = NULL;

	pCharactor = szInputString;

	if (NULL == pCharactor || 0 == *pCharactor)
		return FALSE;

	while (*pCharactor)
	{
		if (((*pCharactor) < '0' || (*pCharactor) > '9') && (pCharactor != szInputString))
			return FALSE;
		else
			if (pCharactor == szInputString && *pCharactor != '+' && *pCharactor != '-' && *pCharactor != '.' && *pCharactor < '0' && *pCharactor > '9')
				return FALSE;
		pCharactor++;
	}

	return TRUE;
}

BOOL IsFloat(LPTSTR szInputString)
{
	char *pCharactor = NULL;

	pCharactor = szInputString;

	if (NULL == pCharactor || 0 == *pCharactor)
		return FALSE;

	while (*pCharactor)
	{
		if (((*pCharactor) < '0' || (*pCharactor) > '9') && *pCharactor != '.' && *pCharactor != '-' && *pCharactor != '+')
			return FALSE;

		pCharactor++;
	}

	return TRUE;
}

BOOL IsFraction(LPTSTR szInputString)
{
	char *pCharactor = NULL;
	UINT uiSlashCounter = 0;

	pCharactor = szInputString;

	if (NULL == pCharactor || 0 == *pCharactor)
		return FALSE;

	while (*pCharactor)
	{
		if (((*pCharactor) < '0' || (*pCharactor) > '9') && *pCharactor != '/')
			return FALSE;

		if (*pCharactor == '/' && pCharactor == szInputString)
			return FALSE;

		if (*pCharactor == '/')
			uiSlashCounter++;

		pCharactor++;
	}

	pCharactor--;
	if ('/' == *pCharactor)
		return FALSE;

	if (uiSlashCounter != 1)
		return FALSE;

	return TRUE;
}

void *Malloc(size_t size)
{
	void *vPointer;
#ifdef RELEASE_DEBUG
//	CHAR pBuffer[1024] = { 0 };

//	sprintf(pBuffer, "BMW: %d bytes Memory Allocated", size);
	OutputDebugString("Memory Allocated");

#endif
	vPointer = malloc(size);

	return vPointer;
}

PENCDATA CreateEncryptData(LPBYTE pData, DWORD dwEncByteLength)
{
	PENCDATA pEncData = Malloc(dwEncByteLength + ENCBYTEOFFSET);
	pEncData->dwEncByteLength = dwEncByteLength;
	pEncData->dwEncByteIndex = 0;
	if (!pData)
		memset(pEncData->chEncByte, 0, dwEncByteLength);
	else
		strncpy(pEncData->chEncByte, pData, dwEncByteLength);

	return pEncData;
}

HRESULT SetEncryptData(PENCDATA pEncData, LPBYTE pData, DWORD dwEncByteLength)
{
	if (dwEncByteLength > pEncData->dwEncByteLength || !pData || !pEncData)
		return ERROR_INVALID_PARAMETER;

	strncpy(pEncData->chEncByte, pData, dwEncByteLength);
	pEncData->dwEncByteIndex = 0;

	return S_OK;
}

HRESULT ClearEncyptData(PENCDATA pEncData)
{
	if (!pEncData)
		return ERROR_INVALID_PARAMETER;

	memset(pEncData->chEncByte, 0, pEncData->dwEncByteLength);
	pEncData->dwEncByteIndex = 0;

	return 0;
}

void DeleteEncData(PENCDATA pEncData)
{
	if (!pEncData)
		free(pEncData);
}

BOOL WriteEncFile(HANDLE handle, LPCVOID lpBuffer, DWORD dwNumOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverLapped, PENCDATA pEncData)
{
#ifdef  RELEASE_DEBUG
	OutputDebugString("BMW In WriteEncFile");
	char szBuffer[MAX_STRING_LENGTH] = { 0 };
	sprintf(szBuffer, "BMW: Index: %d, Length: %d, EncByte1: %02X, EncByte2: %02X", pEncData->dwEncByteIndex, pEncData->dwEncByteLength, pEncData->chEncByte[0], pEncData->chEncByte[1]);
	OutputDebugString(szBuffer);
#endif //  RELEASE_DEBUG

	if (pEncData)
	{
		if (pEncData->dwEncByteIndex >= pEncData->dwEncByteLength)
			pEncData->dwEncByteIndex = 0;

		for (UINT i = 0; i < dwNumOfBytesToWrite; i++)
		{
			((LPBYTE)(lpBuffer))[i] ^= pEncData->chEncByte[(pEncData->dwEncByteIndex)];
			pEncData->dwEncByteIndex++;
			pEncData->dwEncByteIndex %= pEncData->dwEncByteLength;
		}
	}

	WriteFile(handle, lpBuffer, dwNumOfBytesToWrite, lpNumberOfBytesWritten, lpOverLapped);

	return TRUE;
}


