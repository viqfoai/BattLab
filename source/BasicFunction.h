#ifndef BASIC_FUNCTION
#define BASIC_FUNCTION

#include <windows.h>
#include <stdio.h>
#include <visa.h>
#include "UARTTransceiver.h"
#include "UARTProcess.h"


#define MAX_INSTR_NUM	256

#define GET_COMBO_INDEX_FAIL	1
#define OPEN_DEVICE_FAIL		2
#define GET_COMBO_TEXT_FAIL		4

#define MAX_INSTRUMENT_ID_LENGHT	256
#define MAX_CONDITION_EXP_LENGTH	256
#define MAX_STRING_LENGTH		1024
#define MAX_TEST_STATUS_STRING_LENGTH	160
#define VOLT_AVERAGE_WINDOW_LENGTH		100
#define CURR_AVERAGE_WINDOW_LENGTH		100
#define TEMP_AVERAGE_WINDOW_LENGTH		100
#define VOLT_RATE_AVERAGE_WINDOW_LENGTH	100
#define CURR_RATE_AVERAGE_WINDOW_LENGTH	100
#define TEMP_RATE_AVERAGE_WINDOW_LENGTH	100
#define DELTA_TIME_FOR_DELTA_VOLT_CURR	100
#define MAX_BUFFER_SIZE	1000
#define TRUST_THRESHOLD	1000
#define START_ANALYSE_WINDOW 100
#define MINIMUM_ANALYSE_WINDOW_LENGTH 3
#define BAD_RATE_DATA					0x80000000

#define EnforceReleaseMutex(Handle)  		while(ReleaseMutex(Handle)){}

typedef struct
{
	UINT uiHeadIndex;
	UINT uiTailIndex;
	INT  iDataItem[MAX_BUFFER_SIZE];
}DATA_BUFFER;

typedef struct
{
	UINT uiHeadIndex;
	UINT uiTailIndex;
	double  dblDataItem[MAX_BUFFER_SIZE];
}FLOAT_BUFFER;

#define ENCBYTEOFFSET 8
typedef struct _ENC_DATA			//this data is used to encrypt the log file information
{
	DWORD dwEncByteLength;
	DWORD dwEncByteIndex;
	BYTE  chEncByte[0];
}ENCDATA, *PENCDATA;

void ClearList(Element **StrList);
void AddStringToList(Element **StrList, char *NameString);
char *FindStringInList(Element *ListHeader, UINT AppointedPostionInList, UINT *ActualPositionInList);
HRESULT FindInstruments(ViSession *RM, ElementString *InstrDesclist);
HRESULT FindCOM(ElementString *PortList, ElementString *COMList);
void UpdateComboBoxWithDevNameList(HWND hCombo, Element *DevNameList);
HRESULT GetComboString(HWND hwnd, char *CombString);
HRESULT OpenGPIBDeviceInCombo(HWND, ViSession, ViPSession, LPTSTR pInstrIDString);
HRESULT OpenComDeviceInCombo(HWND, HANDLE *, DWORD);

void GetSystemDataString(char *);
void GetSystemTimeString(char *);
void GetTimer(BOOL, UINT *);
void WriteRecordItem(HANDLE hFile, BOOL bTestStarted, BOOL bSaveDate, REC_ITEM RecordItem);
BOOL UpdateVoltage(BOOL, ViSession, char *);
BOOL UpdateCurrent(BOOL, ViSession, char *);
BOOL UpdateTemperature(BOOL, TAB *, char *);
BOOL UpdateRecordItem(BOOL bAllIntrumentsOpened, BOOL bLogStarted, TAB *pTempAccessBlock, REC_ITEM *pRecordItem, char *szDate, char *szTime);
void UpdateRecordItemDisplay(BOOL AllInstrumentsOpened, HWND EditDateHwnd, HWND EditTimeHwnd, HWND EditTimerHwnd, HWND EditVoltHwnd, HWND EditCurrHwnd, HWND EditTempHwnd, REC_ITEM *RecordItem);
BOOL SaveStepToFile(HANDLE hFile, TEST_SCHEDULE_PTR pTestSchedule, TEST_PREFERENCE *pTestPreference, LOG_OPTION *pLogOption, BOOL bHideCond);
BOOL LoadStepFromFile(HANDLE hStepFile, TEST_SCHEDULE_PTR *ppTestSchedule, TEST_PREFERENCE *pTestPreference, LOG_OPTION *pLogOption, BOOL *pbHideCondition);
BOOL RecursiveLoadCondition(HANDLE hStepFile, TERM_COND **ppCondition);
BOOL LoadConditionFromFile(HANDLE hStepFile, TERM_COND **ppCondition);
BOOL RecursiveSaveCondition(HANDLE hStepFile, TERM_COND *pCondition);
BOOL SaveConditionToFile(HANDLE hStepFile, TERM_COND *pCondition);
void UpdateTestStatus(TEST_STEP *pPresentStep, LPTSTR szTestStatusString, REC_ITEM *pRecItem);
int AverageOnTimeWindow(INT iInputData, INT uiPreAveragedData, UINT *pAverageCounter, UINT uiWindow);
BOOL AddDataToBuffer(DATA_BUFFER *pBuffer, INT iData);
UINT GetBufferLength(DATA_BUFFER *pBuffer);
INT ReadDataFromBuffer(DATA_BUFFER *pBuffer, UINT uiIndexFromHead);
BOOL AnaylysisData(DATA_BUFFER *pBuffer, INT *puiSlop, INT *puiIntercept, INT *puiCorrelation, UINT *puiDataLength);
BOOL IsNumerical(LPTSTR szInputString);
BOOL IsFloat(LPTSTR szInputString);
BOOL IsFraction(LPTSTR szInputString);
void *Malloc(size_t size);
void QueryKey(HKEY hKey);

PENCDATA CreateEncryptData(LPBYTE pData, DWORD dwEncByteLength);
HRESULT SetEncryptData(PENCDATA pEncData, LPBYTE pData, DWORD dwEncByteLength);
HRESULT ClearEncyptData(PENCDATA pEncData);
void DeleteEncData(PENCDATA pEncData);
BOOL WriteEncFile(HANDLE handle, LPCVOID lpBuffer, DWORD dwNumOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverLapped, PENCDATA pEncData);
HRESULT OpenTempChamberDeviceInCombo(HWND ComboTempChamberHwnd, HANDLE *phTempChamber);

#endif // !BASIC_FUNCTION
