#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <visa.h>
#include "TempChamberDriver.h"
#include "UARTTransceiver.h"
#include "BatCycleController.h"
#include "BasicFunction.h"
#include "resource.h"
#include "UARTProcess.h"
#include "AddTestStep.h"
#include "DeleteTestStep.h"
#include "EditTestStep.h"
#include "TestExecute.h"
#include "SetTestPreference.h"
#include "SaveData.h"
#include <InstrDrv.h>

ViSession DefaultRM;
ElementString InstrDescList = NULL, PortList = NULL, COMList = NULL;
//HANDLE hTempMeter = NULL;
TEST_SCHEDULE_PTR pTestSchedule = NULL;
TEST_STEP *pPresentStep = NULL;
TEST_PREFERENCE TestPreference = {mA, 3000, 3000};
extern BOOL bTestThreadMsgQueueCreated;
BOOL bDisplayCounter = FALSE;
BOOL bHideCondition = FALSE;

extern HICON hAppIcon;

char *szOperation[] = { "NOP", "CHARGE", "DISCHARGE", "REST", "GOTO", "INCREMENT COUNTER", "DECREMENT COUNTER", "CLEAR COUNTER", "PASSEDCHG_ON", "PASSEDCHG_OFF", "PASSEDCHG_CLR", "START_LOG", "STOP_LOG", "SHTDN_CHMBR", "SET_TEMP"};
char *szConditionType[] = { "Bifurcation", "Voltage", "Current", "Temperature", "AvgVoltage", "AvgCurrent","ΔV/Δt", "ΔI/Δt", "StepTime", "PassedCharge", "Counter", "ConditionTime" };
char *szComparasonType[] = {"", ">", "=", "<", "<=", ">="};
char *szLogicalRel[] = {"AND", "OR"};
char szConditionExpression[MAX_CONDITION_EXP_LENGTH] = { 0 };
extern char *szLoadUnit[];
char szPowerIDString[MAX_INSTRUMENT_ID_LENGHT] = { 0 };
char szLoadIDString[MAX_INSTRUMENT_ID_LENGHT] = { 0 };
char szVMeterIDString[MAX_INSTRUMENT_ID_LENGHT] = { 0 };
char szIMeterIDString[MAX_INSTRUMENT_ID_LENGHT] = { 0 };

HWND hComboVoltSrc, hComboCurrSrc, hComboVoltMeter, hComboCurrMeter, hComboCOMCtrl, hComboTempMeter, hComboTempChamber;
HWND hEditDate, hEditTime, hEditTimer, hEditVolt, hEditCurr, hEditTemp;
HWND hButtonStartLog, hButtonAbortLog, hButtonFileOpen, hButtonSaveClose, hStaticFileName, hButtonLogConfig;
HWND hTestScheduleListView, hStaticTestStatus;

HWND hButtonOpenEquipments;
HWND hButtonCloseEquipments;

HWND hButtonAddStep, hButtonEditStep, hButtonDeleteStep, hButtonSaveSteps, hButtonLoadSteps, hButtonStartTest, hButtonStopTest, hButtonSelectStartStep, hButtonTestPreference;
//HANDLE hLogFile = NULL;
LOG_OPTION LogOption = { 1, LOG_OPTION_D_STATE_MARK};

static HANDLE hTempReadThread;
static HANDLE hTestExecuteThread;

static INST_HANDLE_SET InstHandleSet = { 0 };

const char str4SearchEn[] = { "Physical Address. . . . . . . . . : " };
const char str4SearchCh[] = { "物理地址. . . . . . . . . . . . . : " };

PENCDATA pEncData = NULL;
const WORD wEncWord = 0xAA55;

BOOL WINAPI BatCycleController(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	static HANDLE hMutexTemp = NULL, hEventTemp = NULL, hStepFile = NULL;
	HRESULT Result, ResultSummary;
	char VoltSrcDevString[MAX_STRING_LENGTH] = { 0 }, CurrSrcDevString[MAX_STRING_LENGTH] = { 0 }, VoltMeterDevString[MAX_STRING_LENGTH] = { 0 }, CurrMeterDevString[MAX_STRING_LENGTH] = { 0 }, COMDevString[MAX_STRING_LENGTH] = { 0 }, TempMeterDevString[MAX_STRING_LENGTH] = { 0 }, TempChamberDevString[MAX_STRING_LENGTH] = { 0 };
	char VoltSrcSrchString[MAX_STRING_LENGTH] = { 0 }, CurrSrcSrchString[MAX_STRING_LENGTH] = { 0 }, VoltMeterSrchString[MAX_STRING_LENGTH] = { 0 }, CurrMeterSrchString[MAX_STRING_LENGTH] = { 0 }, COMSrchString[MAX_STRING_LENGTH] = { 0 }, TempMeterSrchString[MAX_STRING_LENGTH] = { 0 }, TempChamberSrchString[MAX_STRING_LENGTH] = { 0 };
	char szTestStatusString[MAX_TEST_STATUS_STRING_LENGTH] = { 0 };
	HKEY hKeyRoot = HKEY_CURRENT_USER;
	LPTSTR lpDevSubKey = TEXT("Software\\BatCycleController\\DevAssignment");
	LPTSTR lpFileNameSubKey = TEXT("Software\\BatCycleController\\FileName");
	UINT StringLength = MAX_STRING_LENGTH, ItemIndex = 0;
	static UINT uiPresentStepNumber = 0, uiSelectedStartStepNumber = 0;

	static HKEY hKeyDev, hKeyFileName;

	static REC_ITEM RecordItem = { 0 };

	COMMTIMEOUTS TimeOuts;
	DCB DeviceControlBlock;

	static TAB TempAccessBlock = { 0 };

	static char szDate[11] = { 0 };
	static char szTime[9] = { 0 };
	static char szTestTimer[12] = { 0 };
	static char szVoltage[12] = { 0 };
	static char szCurrent[12] = { 0 };
	static char szTemperature[20] = { 0 };

	//static BOOL bLogStarted = FALSE;
	static BOOL bAllInstrumentsOpened = FALSE;

	BOOL bResult = FALSE;

	static OPENFILENAME ofn;
	static char szLogFileName[MAX_FILE_NAME_LENGTH] = { 0 };
	static char szStepFileName[MAX_FILE_NAME_LENGTH] = { 0 };

	DWORD dErrorCode;
	static DWORD dwTestThreadID;
	static BOOL bHighlight = 0;
	static UINT uiTempLogTimerMilliSec = 0;
	DWORD dwWaitState;
	LRESULT lr;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		lr = SendMessage(hWnd, WM_SETICON, ICON_BIG, (long)hAppIcon);
		
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: in WM_INITDIALOG 1");
#endif
		Result = FindInstruments(&DefaultRM, &InstrDescList);
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: after FindInstruments");
#endif
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: before FindCom");
#endif
		Result |= FindCOM(&PortList, &COMList);
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: after FindCom");
#endif

		hComboVoltSrc = GetDlgItem(hWnd, IDC_COMBO_VOLT_SOURCE);
		UpdateComboBoxWithDevNameList(hComboVoltSrc, InstrDescList);

		hComboCurrSrc = GetDlgItem(hWnd, IDC_COMBO_CURR_SOURCE);
		UpdateComboBoxWithDevNameList(hComboCurrSrc, InstrDescList);

		hComboVoltMeter = GetDlgItem(hWnd, IDC_COMBO_VOLT_METER);
		UpdateComboBoxWithDevNameList(hComboVoltMeter, InstrDescList);

		hComboCurrMeter = GetDlgItem(hWnd, IDC_COMBO_CURR_METER);
		UpdateComboBoxWithDevNameList(hComboCurrMeter, InstrDescList);

		hComboCOMCtrl = GetDlgItem(hWnd, IDC_COMBO_COM);
		UpdateComboBoxWithDevNameList(hComboCOMCtrl, COMList);

		hComboTempMeter = GetDlgItem(hWnd, IDC_COMBO_TEMP_METER);
		UpdateComboBoxWithDevNameList(hComboTempMeter, COMList);

		hComboTempChamber = GetDlgItem(hWnd, IDC_COMBO_TEMP_CHAMBER);
		UpdateComboBoxWithDevNameList(hComboTempChamber, COMList);

		Result = RegOpenKeyEx(hKeyRoot, lpDevSubKey, 0, KEY_ALL_ACCESS, &hKeyDev);
#ifdef RELEASE_DEBUG
		{
			char szDebugMessage[256] = { 0 };
			sprintf(szDebugMessage, "BMW: after RegOpenKeyEx %d", Result);
			OutputDebugString(szDebugMessage);
		}
#endif
		if (ERROR_SUCCESS != Result)
			RegCreateKeyEx(hKeyRoot, lpDevSubKey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKeyDev, NULL);
		else
		{
#ifdef RELEASE_DEBUG
			OutputDebugString("BMW: before reading registry for instrument strings");
#endif
			Result = RegGetValue(hKeyDev, "", "VoltSrc", RRF_RT_ANY, NULL, VoltSrcSrchString, &StringLength);
#ifdef RELEASE_DEBUG
			OutputDebugString("BMW: after reading VoltSrc registry for instrument strings");
#endif
			StringLength = MAX_STRING_LENGTH;
			Result = RegGetValue(hKeyDev, "", "CurrSrc", RRF_RT_ANY, NULL, CurrSrcSrchString, &StringLength);
			StringLength = MAX_STRING_LENGTH;
			Result = RegGetValue(hKeyDev, "", "VoltMeter", RRF_RT_ANY, NULL, VoltMeterSrchString, &StringLength);
			StringLength = MAX_STRING_LENGTH;
			Result = RegGetValue(hKeyDev, "", "CurrMeter", RRF_RT_ANY, NULL, CurrMeterSrchString, &StringLength);
			StringLength = MAX_STRING_LENGTH;
			Result = RegGetValue(hKeyDev, "", "TempMeter", RRF_RT_ANY, NULL, TempMeterSrchString, &StringLength);
			StringLength = MAX_STRING_LENGTH;
			Result = RegGetValue(hKeyDev, "", "COMCtrl", RRF_RT_ANY, NULL, COMSrchString, &StringLength);
			StringLength = MAX_STRING_LENGTH;
			Result = RegGetValue(hKeyDev, "", "TempChamber", RRF_RT_ANY, NULL, TempChamberSrchString, &StringLength);


#ifdef RELEASE_DEBUG
			OutputDebugString("BMW: UpdateComboBoxWithDevNameList");
#endif
			ItemIndex = SendMessage(hComboVoltSrc, CB_FINDSTRING, (WPARAM)(-1), (LPARAM)VoltSrcSrchString);
			if (CB_ERR != ItemIndex)
				SendMessage(hComboVoltSrc, CB_SETCURSEL, (WPARAM)ItemIndex, (LPARAM)NULL);

#ifdef RELEASE_DEBUG
			OutputDebugString("BMW: SetVoltSrc done");
#endif
			ItemIndex = SendMessage(hComboCurrSrc, CB_FINDSTRING, (WPARAM)(-1), (LPARAM)CurrSrcSrchString);
			if (CB_ERR != ItemIndex)
				SendMessage(hComboCurrSrc, CB_SETCURSEL, (WPARAM)ItemIndex, (LPARAM)NULL);

			ItemIndex = SendMessage(hComboVoltMeter, CB_FINDSTRING, (WPARAM)(-1), (LPARAM)VoltMeterSrchString);
			if (CB_ERR != ItemIndex)
				SendMessage(hComboVoltMeter, CB_SETCURSEL, (WPARAM)ItemIndex, (LPARAM)NULL);
#ifdef RELEASE_DEBUG
			OutputDebugString("BMW: Set Volte Meter Done");
#endif
			ItemIndex = SendMessage(hComboCurrMeter, CB_FINDSTRING, (WPARAM)(-1), (LPARAM)CurrMeterSrchString);
			if (CB_ERR != ItemIndex)
				SendMessage(hComboCurrMeter, CB_SETCURSEL, (WPARAM)ItemIndex, (LPARAM)NULL);

			ItemIndex = SendMessage(hComboTempMeter, CB_FINDSTRING, (WPARAM)(-1), (LPARAM)TempMeterSrchString);
			if (CB_ERR != ItemIndex)
				SendMessage(hComboTempMeter, CB_SETCURSEL, (WPARAM)ItemIndex, (LPARAM)NULL);
#ifdef RELEASE_DEBUG
			OutputDebugString("BMW: Set Temp Meter Done");
#endif
			ItemIndex = SendMessage(hComboCOMCtrl, CB_FINDSTRING, (WPARAM)(-1), (LPARAM)COMSrchString);
			if (CB_ERR != ItemIndex)
				SendMessage(hComboCOMCtrl, CB_SETCURSEL, (WPARAM)ItemIndex, (LPARAM)NULL);

			ItemIndex = SendMessage(hComboTempChamber, CB_FINDSTRING, (WPARAM)(-1), (LPARAM)TempChamberSrchString);
			if (CB_ERR != ItemIndex)
				SendMessage(hComboTempChamber, CB_SETCURSEL, (WPARAM)ItemIndex, (LPARAM)NULL);

		}
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: All UpdateComboBoxWithDevNameList");
#endif
		hButtonOpenEquipments = GetDlgItem(hWnd, IDC_OPEN_EQUIPMENT);
		hButtonCloseEquipments = GetDlgItem(hWnd, IDC_CLOSE_EQUIPMENT);

		hEditDate = GetDlgItem(hWnd, IDC_EDIT_DATE);
		hEditTime = GetDlgItem(hWnd, IDC_EDIT_TIME);
		hEditTimer = GetDlgItem(hWnd, IDC_EDIT_TIMER);
		hEditVolt = GetDlgItem(hWnd, IDC_EDIT_VOLT);
		hEditCurr = GetDlgItem(hWnd, IDC_EDIT_CURR);
		hEditTemp = GetDlgItem(hWnd, IDC_EDIT_TEMP);

		hButtonStartLog = GetDlgItem(hWnd, IDC_BUTTON_START_LOGGING);
		hButtonAbortLog = GetDlgItem(hWnd, IDC_BUTTON_ABORT_LOGGING);

		hButtonFileOpen = GetDlgItem(hWnd, IDC_BUTTON_FILE_OPEN);
		hButtonSaveClose = GetDlgItem(hWnd, IDC_BUTTON_SAVE_CLOSE);

		hButtonLogConfig = GetDlgItem(hWnd, IDC_BUTTON_LOG_CONFIG);

		Result = RegOpenKeyEx(hKeyRoot, lpFileNameSubKey, 0, KEY_ALL_ACCESS, &hKeyFileName);
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: after 2nd RegOpenKeyEx");
#endif
		if (ERROR_SUCCESS != Result)
		{
			RegCreateKeyEx(hKeyRoot, lpFileNameSubKey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKeyFileName, NULL);
			strcpy(szLogFileName, "");
		}
		else
		{
			StringLength = MAX_FILE_NAME_LENGTH;
			Result = RegGetValue(hKeyFileName, "", "LogFileName", RRF_RT_ANY, NULL, szLogFileName, &StringLength);
		}
		hStaticFileName = GetDlgItem(hWnd, IDC_STATIC_FILE_NAME);
		SetWindowText(hStaticFileName, szLogFileName);

		hStaticTestStatus = GetDlgItem(hWnd, IDC_STATIC_TEST_STATUS);
		SetWindowText(hStaticTestStatus, "IDLE");

		hMutexTemp = CreateMutex(NULL, FALSE, NULL);
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: CreateMutex");
#endif

		if (NULL == hMutexTemp)
		{
			MessageBox(NULL, "Can not create the Mutex", NULL, MB_OK);
		}

		hEventTemp = CreateEvent(NULL, TRUE, FALSE, NULL);

		TempAccessBlock.phEventTemp = &hEventTemp;
		TempAccessBlock.phCom = &(InstHandleSet.hTempMeter);
		TempAccessBlock.bCloseCOMHandle = FALSE;
		TempAccessBlock.phTempMutex = &hMutexTemp;

		hTempReadThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TempReadProcess, &TempAccessBlock, 0, NULL);
		InstHandleSet.hWnd = hWnd;
		hTestExecuteThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TestExecuteThread, &InstHandleSet, 0, &dwTestThreadID);

		//InstHandleSet.hEventStartCurrReading = CreateEvent(NULL, FALSE, FALSE, NULL);
		//InstHandleSet.hEventStartVoltReading = CreateEvent(NULL, FALSE, FALSE, NULL);
		//InstHandleSet.hEventVoltReadDone = CreateEvent(NULL, TRUE, FALSE, NULL);
		InstHandleSet.hEventDMMReadDone = CreateEvent(NULL, FALSE, TRUE, NULL);

		InstHandleSet.pRecordItem = &RecordItem;

		//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReadVoltage, &InstHandleSet, 0, NULL);
		//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReadCurrent, &InstHandleSet, 0, NULL);

		hTestScheduleListView = GetDlgItem(hWnd, IDC_TEST_SCHEDULE_LIST);
		InitListView(hTestScheduleListView);

		InitTestSchedule(&pTestSchedule);

		pPresentStep = pTestSchedule;				//initialise pPresentStep at the head of the TestSchedule of the step list
#ifdef RELEASE_DEBUG
		{
			CHAR szPresentStep[1024] = { 0 };
			sprintf(szPresentStep, "BMW:pPresentStep is 0X%08X", pPresentStep);
			OutputDebugString(szPresentStep);
		}
#endif // RELEASE_DEBUG


		hButtonAddStep = GetDlgItem(hWnd, IDC_BUTTON_ADD_STEP);
		hButtonEditStep = GetDlgItem(hWnd, IDC_BUTTON_EDIT_STEP);
		hButtonDeleteStep = GetDlgItem(hWnd, IDC_BUTTON_DELETE_STEP);
		hButtonSaveSteps = GetDlgItem(hWnd, IDC_BUTTON_SAVE_STEPS);
		hButtonLoadSteps = GetDlgItem(hWnd, IDC_BUTTON_LOAD_STEPS);
		hButtonStartTest = GetDlgItem(hWnd, IDC_BUTTON_START_TEST);
		hButtonStopTest = GetDlgItem(hWnd, IDC_BUTTON_STOP_TEST);
		hButtonSelectStartStep = GetDlgItem(hWnd, IDC_BUTTON_SELECT_START_STEP);
		hButtonTestPreference = GetDlgItem(hWnd, IDC_BUTTON_PREFRENCE);

		InitRecordItem(RecordItem);
		RecordItem.hWnd = hWnd;
		RecordItem.uiLogInterval = LogOption.uiLogInterval;
		RecordItem.bHighFrequencyDataNeeded = LogOption.dwOptions & LOG_OPTION_HI_FREQ_DATA_NEEDED;
		RecordItem.bMarkDsgState = LogOption.dwOptions & LOG_OPTION_D_STATE_MARK;
#ifdef RELEASE_DEBUG
		OutputDebugString("BMW: in WM_INITDIALOG 6");
#endif

		pEncData = CreateEncryptData(NULL, sizeof(WORD));
		SetTimer(hWnd, ID_TIMER1, 1000, NULL);
	break;
		/*
		case WM_NOTIFY:
		if (IDC_TEST_SCHEDULE_LIST == wParam)
		{
		if (NM_CLICK == ((NMHDR *)lParam)->code || NM_DBLCLK == ((NMHDR *)lParam)->code)
		{
		if(uiPresentStepNumber)
		ListView_SetItemState(hTestScheduleListView, -1, 0, LVIS_FOCUSED | LVIS_SELECTED);
		}
		}
		break;

		*/
	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_OPEN_EQUIPMENT:
			ResultSummary = ERROR_SUCCESS;

			ResultSummary |= OpenGPIBDeviceInCombo(hComboVoltSrc, DefaultRM, &(InstHandleSet.viVoltSrc), szPowerIDString);

			ResultSummary |= OpenGPIBDeviceInCombo(hComboCurrSrc, DefaultRM, &(InstHandleSet.viLoad), szLoadIDString);

			ResultSummary |= OpenGPIBDeviceInCombo(hComboVoltMeter, DefaultRM, &(InstHandleSet.viVoltMeter), szVMeterIDString);

			DWORD dwRetCount;
			ViStatus viStatus = viWrite(InstHandleSet.viVoltMeter, "CONF:VOLT:DC\n", strlen("CONF:VOLT:DC\n"), &dwRetCount);
			//viStatus = viWrite(InstHandleSet.viVoltMeter, "VOLT:RANGE:AUTO OFF\n", strlen("VOLT:RANGE:AUTO OFF\n"), &dwRetCount);
			viStatus = viWrite(InstHandleSet.viVoltMeter, "TRIG:DEL 0\n", strlen("TRIG:DEL 0\n"), &dwRetCount);

			ResultSummary |= OpenGPIBDeviceInCombo(hComboCurrMeter, DefaultRM, &(InstHandleSet.viCurrMeter), szIMeterIDString);

			viStatus = viWrite(InstHandleSet.viCurrMeter, "CONF:CURR:DC MAX, 0.00001\n", strlen("CONF:CURR:DC MAX, 0.00001\n"), &dwRetCount);
			viStatus = viWrite(InstHandleSet.viCurrMeter, "CURR:RANGE:AUTO OFF\n", strlen("CURR:RANGE:AUTO OFF\n"), &dwRetCount);
			viStatus = viWrite(InstHandleSet.viCurrMeter, "TRIG:DEL 0\n", strlen("TRIG:DEL 0\n"), &dwRetCount);

			ResultSummary |= OpenComDeviceInCombo(hComboCOMCtrl, &(InstHandleSet.hCOMCtrl), FILE_FLAG_OVERLAPPED);

			GetCommTimeouts(InstHandleSet.hCOMCtrl, &TimeOuts);
			TimeOuts.ReadTotalTimeoutConstant = 3000;
			TimeOuts.ReadIntervalTimeout = 1000;
			TimeOuts.ReadTotalTimeoutMultiplier = 1000;
			SetCommTimeouts(InstHandleSet.hCOMCtrl, &TimeOuts);
			GetCommState(InstHandleSet.hCOMCtrl, &DeviceControlBlock);
			DeviceControlBlock.BaudRate = CBR_9600;
			DeviceControlBlock.ByteSize = 8;
			DeviceControlBlock.fParity = FALSE;
			DeviceControlBlock.Parity = NOPARITY;
			DeviceControlBlock.StopBits = ONESTOPBIT;
			SetCommState(InstHandleSet.hCOMCtrl, &DeviceControlBlock);

			ResultSummary |= OpenComDeviceInCombo(hComboTempMeter, &(InstHandleSet.hTempMeter), FILE_FLAG_OVERLAPPED);

			GetCommTimeouts(InstHandleSet.hTempMeter, &TimeOuts);
			TimeOuts.ReadTotalTimeoutConstant = 3000;
			TimeOuts.ReadIntervalTimeout = 1000;
			TimeOuts.ReadTotalTimeoutMultiplier = 1000;
			SetCommTimeouts(InstHandleSet.hTempMeter, &TimeOuts);
			GetCommState(InstHandleSet.hTempMeter, &DeviceControlBlock);
			DeviceControlBlock.BaudRate = CBR_9600;
			DeviceControlBlock.ByteSize = 8;
			DeviceControlBlock.fParity = FALSE;
			DeviceControlBlock.Parity = NOPARITY;
			DeviceControlBlock.StopBits = ONESTOPBIT;
			SetCommState(InstHandleSet.hTempMeter, &DeviceControlBlock);

			if (ERROR_SUCCESS == ResultSummary)
			{
				ViStatus viStatus;
				SetEvent(hEventTemp);
				bAllInstrumentsOpened = TRUE;
				DisconnectCharge(InstHandleSet.hCOMCtrl);
				DisconnectLoad(InstHandleSet.hCOMCtrl);
				RecordItem.bHighFrequencyDataNeeded = LogOption.dwOptions & LOG_OPTION_HI_FREQ_DATA_NEEDED;
				RecordItem.bMarkDsgState = LogOption.dwOptions & LOG_OPTION_D_STATE_MARK;
//				RecordItem.hEventDMMReadDone = CreateEvent(NULL, FALSE, TRUE, NULL);

//				InitializeCriticalSection(&(RecordItem.criticSection));
//				viStatus = viInstallHandler(InstHandleSet.viVoltMeter, VI_EVENT_SERVICE_REQ, DllReadVoltageCallback, (ViAddr)(&RecordItem));
//				viStatus = viInstallHandler(InstHandleSet.viVoltMeter, VI_EVENT_IO_COMPLETION, DllReadVoltageCompletion, (ViAddr)(&RecordItem));
//				viStatus = viInstallHandler(InstHandleSet.viCurrMeter, VI_EVENT_SERVICE_REQ, DllReadCurrentCallback, (ViAddr)(&RecordItem));
//				viStatus = viInstallHandler(InstHandleSet.viCurrMeter, VI_EVENT_IO_COMPLETION, DllReadCurrentCompletion, (ViAddr)(&RecordItem));
//				viStatus = viEnableEvent(InstHandleSet.viVoltMeter, VI_EVENT_SERVICE_REQ, VI_HNDLR, VI_NULL);
				viStatus = viEnableEvent(InstHandleSet.viVoltMeter, VI_EVENT_IO_COMPLETION, VI_QUEUE, VI_NULL);
//				viStatus = viEnableEvent(InstHandleSet.viCurrMeter, VI_EVENT_SERVICE_REQ, VI_HNDLR, VI_NULL);
				viStatus = viEnableEvent(InstHandleSet.viCurrMeter, VI_EVENT_IO_COMPLETION, VI_QUEUE, VI_NULL);
//				DllEnableMeterInt(InstHandleSet.viCurrMeter, szIMeterIDString);
//				DllEnableMeterInt(InstHandleSet.viVoltMeter, szVMeterIDString);
			}
			else
			{
				MessageBox(NULL, "Instruments not opened successfully", NULL, MB_OK);
			}

			ResultSummary |= OpenTempChamberDeviceInCombo(hComboTempChamber, &(InstHandleSet.hTempChamber));

			break;
		case IDC_CLOSE_EQUIPMENT:
			if (0 != uiPresentStepNumber || 0 != pPresentStep->uiStepNumber)
			{
				MessageBox(NULL, "Please Stop Test First!", NULL, MB_OK);
				break;
			}
			if (VI_NULL != InstHandleSet.viVoltSrc)
			{
				viPrintf(InstHandleSet.viVoltSrc, "*RST\n");
				viClose(InstHandleSet.viVoltSrc);
				InstHandleSet.viVoltSrc = VI_NULL;
			}

			if (VI_NULL != InstHandleSet.viLoad)
			{
				viPrintf(InstHandleSet.viLoad, "*RST\n");
				viClose(InstHandleSet.viLoad);
				InstHandleSet.viLoad = VI_NULL;
			}

			if (VI_NULL != InstHandleSet.viVoltMeter)
			{
				viDiscardEvents(InstHandleSet.viVoltMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
				viPrintf(InstHandleSet.viVoltMeter, "*RST\n");
				viDisableEvent(InstHandleSet.viVoltMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
				//viUninstallHandler(InstHandleSet.viVoltMeter, VI_EVENT_SERVICE_REQ, DllReadVoltageCallback, (ViAddr)(&RecordItem));
				//viUninstallHandler(InstHandleSet.viVoltMeter, VI_EVENT_IO_COMPLETION, DllReadVoltageCompletion, (ViAddr)(&RecordItem));
				viClose(InstHandleSet.viVoltMeter);
				InstHandleSet.viVoltMeter = VI_NULL;
			}

			if (VI_NULL != InstHandleSet.viCurrMeter)
			{
				viDiscardEvents(InstHandleSet.viCurrMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
				viPrintf(InstHandleSet.viCurrMeter, "*RST\n");
				viDisableEvent(InstHandleSet.viCurrMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
				//viUninstallHandler(InstHandleSet.viCurrMeter, VI_EVENT_SERVICE_REQ, DllReadCurrentCallback, (ViAddr)(&RecordItem));
				//viUninstallHandler(InstHandleSet.viCurrMeter, VI_EVENT_IO_COMPLETION, DllReadCurrentCompletion, (ViAddr)(&RecordItem));
				ViStatus viStatus = viClose(InstHandleSet.viCurrMeter);
				InstHandleSet.viCurrMeter = VI_NULL;
			}

			DeleteCriticalSection(&(RecordItem.criticSection));

			if (NULL != InstHandleSet.hTempMeter)
			{
				WaitForSingleObject(hMutexTemp, INFINITE);
				TempAccessBlock.bCloseCOMHandle = TRUE;
				EnforceReleaseMutex(hMutexTemp);
			}

			if (NULL != InstHandleSet.hCOMCtrl)
			{
				CloseHandle(InstHandleSet.hCOMCtrl);
				InstHandleSet.hCOMCtrl = NULL;
			}
			
			if (NULL != InstHandleSet.hTempChamber)
			{
				DllCloseTempChamber(InstHandleSet.hTempChamber);
				InstHandleSet.hTempChamber = NULL;
			}
			bAllInstrumentsOpened = FALSE;

			break;
		case IDC_BUTTON_START_LOGGING:

			RecordItem.uiLogTimer = 0;
			RecordItem.bLogStarted = TRUE;

			break;
		case IDC_BUTTON_ABORT_LOGGING:

			SetFilePointer(RecordItem.hLogFile, 0, 0, 0);		//clear the file
			SetEndOfFile(RecordItem.hLogFile);
			if (NULL != RecordItem.hLogFile)
			{
				CloseHandle(RecordItem.hLogFile);
				RecordItem.hLogFile = NULL;
			}
			RecordItem.bLogStarted = FALSE;

			break;
		case IDC_BUTTON_FILE_OPEN:

			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFile = szLogFileName;
			ofn.nMaxFile = MAX_FILE_NAME_LENGTH;
			ofn.lpstrFilter = "LOG(*.log)\0*.log\0All(*.*)\0*.*\0\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = NULL;
			ofn.lpstrDefExt = "log";
			ofn.Flags = OFN_CREATEPROMPT;

			if (TRUE == GetOpenFileName(&ofn))
			{
				RecordItem.hLogFile = CreateFile(ofn.lpstrFile, \
					GENERIC_READ | GENERIC_WRITE, \
					FILE_SHARE_READ, \
					(LPSECURITY_ATTRIBUTES)NULL, \
					CREATE_ALWAYS, \
					FILE_ATTRIBUTE_NORMAL, \
					(HANDLE)NULL);

				if (INVALID_HANDLE_VALUE != RecordItem.hLogFile)
					SetWindowText(hStaticFileName, ofn.lpstrFile);
				else
				{
					DWORD dwError;
					dwError = GetLastError();
					MessageBox(NULL, "File Open Failed!", NULL, MB_OK);
					SetWindowText(hStaticFileName, "File Not Open!");
				}
				//DWORD dwShareMode = (bHideCondition) ? 0 : FILE_SHARE_READ;		 
				if (bHideCondition)
				{
					SetEncryptData(pEncData, (LPBYTE)&wEncWord, sizeof(WORD));
				}
				else
				{
					ClearEncyptData(pEncData);
				}
			}
			else
				MessageBox(NULL, "File Name Not Specified", NULL, MB_OK);

			break;
		case IDC_BUTTON_SAVE_CLOSE:

			if (NULL != RecordItem.hLogFile)
			{
				//CloseHandle(RecordItem.hLogFile);
				//SaveCloseLogFile(&(RecordItem.hLogFile), bHideCondition);
				//RecordItem.hLogFile = NULL;
				//if (!bHideCondition)
				//{
					CloseHandle(RecordItem.hLogFile);
					RecordItem.hLogFile = NULL;
				//}
				//else
					//DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_SAVE_LOG_DATA), hWnd, SaveCloseLogFile, (LPARAM)(&RecordItem));
			}
			RecordItem.bLogStarted = FALSE;

			break;
		case IDC_BUTTON_LOG_CONFIG:
			Result = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_LOG_CONFIG), hWnd, SetLogInterval, (LPARAM)(&LogOption));
			if (ERROR_FAIL != Result)
			{
				RecordItem.uiLogInterval = LogOption.uiLogInterval;
				RecordItem.bHighFrequencyDataNeeded = LogOption.dwOptions & LOG_OPTION_HI_FREQ_DATA_NEEDED;
				RecordItem.bMarkDsgState = LogOption.dwOptions & LOG_OPTION_D_STATE_MARK;
			}
			break;
		case IDC_BUTTON_ADD_STEP:
			Result = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_ADD_STEP), hWnd, AddStepToSchedule, (LPARAM)(&pTestSchedule));
			if (ERROR_FAIL != Result)
				UpdateStepListView(pTestSchedule, hTestScheduleListView);
			break;
		case IDC_BUTTON_DELETE_STEP:
			Result = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_DELETE_STEP), hWnd, DeleteStepFromSchedule, (LPARAM)(&pTestSchedule));
			if (ERROR_FAIL != Result)
				UpdateStepListView(pTestSchedule, hTestScheduleListView);
			break;
		case IDC_BUTTON_EDIT_STEP:
			Result = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_EDIT_STEP), hWnd, EditTestStep, (LPARAM)(&pTestSchedule));
			if (ERROR_FAIL != Result)
				UpdateStepListView(pTestSchedule, hTestScheduleListView);
			break;
		case IDC_BUTTON_SAVE_STEPS:
			if (FindMaxStepNumber(pTestSchedule) == 0)
			{
				MessageBox(NULL, "No Step has been configured!", NULL, MB_OK);
				break;
			}

			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFile = szStepFileName;
			ofn.nMaxFile = MAX_FILE_NAME_LENGTH;
			ofn.lpstrFilter = "STEP(*.STP)\0*.STP\0All(*.*)\0*.*\0\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = NULL;
			ofn.lpstrDefExt = "STP";
			ofn.Flags = OFN_CREATEPROMPT;

			if (TRUE == GetSaveFileName(&ofn))
			{
				hStepFile = CreateFile(ofn.lpstrFile, \
					GENERIC_READ | GENERIC_WRITE, \
					0, \
					(LPSECURITY_ATTRIBUTES)NULL, \
					CREATE_ALWAYS, \
					FILE_ATTRIBUTE_NORMAL, \
					(HANDLE)NULL);
			}
			else
			{
				MessageBox(NULL, "File Name Not Specified", NULL, MB_OK);
				break;
			}
			SaveStepToFile(hStepFile, pTestSchedule, &TestPreference, &LogOption, bHideCondition);
			CloseHandle(hStepFile);
			hStepFile = NULL;
			break;
		case IDC_BUTTON_LOAD_STEPS:

			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFile = szStepFileName;
			ofn.nMaxFile = MAX_FILE_NAME_LENGTH;
			ofn.lpstrFilter = "STEP(*.STP)\0*.STP\0All(*.*)\0*.*\0\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = NULL;
			ofn.lpstrDefExt = "STP";
			ofn.Flags = OFN_CREATEPROMPT;

			if (TRUE == GetOpenFileName(&ofn))
			{
				hStepFile = CreateFile(ofn.lpstrFile, \
					GENERIC_READ | GENERIC_WRITE, \
					0, \
					(LPSECURITY_ATTRIBUTES)NULL, \
					OPEN_EXISTING, \
					FILE_ATTRIBUTE_NORMAL, \
					(HANDLE)NULL);
			}
			else
			{
				MessageBox(NULL, "File Name Not Specified", NULL, MB_OK);
				break;
			}

			if (NULL == hStepFile)
			{
				dErrorCode = GetLastError();
				if (ERROR_FILE_NOT_FOUND == dErrorCode)
					MessageBox(NULL, "File Not Found", NULL, MB_OK);
				else
					MessageBox(NULL, "File Open Failed", NULL, MB_OK);
			}

			bResult = FALSE;
			if (NULL != hStepFile)
			{
				bResult = ReleaseScheudleMem(&pTestSchedule);
				bResult &= LoadStepFromFile(hStepFile, &pTestSchedule, &TestPreference, &LogOption, &bHideCondition);
				RecordItem.bHighFrequencyDataNeeded = LogOption.dwOptions & LOG_OPTION_HI_FREQ_DATA_NEEDED;
				RecordItem.bMarkDsgState = LogOption.dwOptions & LOG_OPTION_D_STATE_MARK;
				RecordItem.uiHighFrequenceyReadCounter = 0;
				pPresentStep = pTestSchedule;
				uiPresentStepNumber = uiSelectedStartStepNumber = 0;
			}
			
			if (bResult)
				UpdateStepListView(pTestSchedule, hTestScheduleListView);

			if (NULL != hStepFile)
			{
				CloseHandle(hStepFile);
				hStepFile = NULL;
			}

			if (bHideCondition)
			{
				SetEncryptData(pEncData, (LPBYTE)&wEncWord, sizeof(WORD));
			}
			else
			{
				ClearEncyptData(pEncData);
			}

			break;

		case IDC_BUTTON_START_TEST:
			if (NULL == pTestSchedule->pNextStep)
			{
				MessageBox(NULL, "Test Schedule Not Created!", NULL, MB_OK);
				break;
			}

			if (0 != uiSelectedStartStepNumber)
			{
				uiPresentStepNumber = uiSelectedStartStepNumber;
				uiSelectedStartStepNumber = 0;
			}
			else
			{
				if (0 == uiPresentStepNumber)
				{
					uiPresentStepNumber = 1;
				}
				else
				{
					MessageBox(NULL, "Impossible uiPresentStepNumber", NULL, MB_OK);
				}
			}

			RecordItem.iCounter = 0;
			bDisplayCounter = HasCounterOpertion(pTestSchedule);
			break;

		case IDC_BUTTON_STOP_TEST:
			if (NULL == pTestSchedule->pNextStep)
				MessageBox(NULL, "Test Schedule Not Created!", NULL, MB_OK);
			else
			{
				uiPresentStepNumber = 0;
				DllInitTempChamber(InstHandleSet.hTempChamber);
				DllShutdownTempChmaber(InstHandleSet.hTempChamber);
			}
			break;

		case IDC_BUTTON_SELECT_START_STEP:
			if (NULL == pTestSchedule->pNextStep)
				MessageBox(NULL, "Test Schedule Not Created!", NULL, MB_OK);
			else
				uiSelectedStartStepNumber = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_SELECT_START_STEP), \
					hWnd, SelectStartStep, (LPARAM)FindMaxStepNumber(pTestSchedule));
			break;

		case IDC_BUTTON_PREFRENCE:
			Result = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_PREFERENCE), hWnd, SetTestPreference, (LPARAM)(&TestPreference));
			if (ERROR_FAIL != Result)
			{
				if (C_Rate == TestPreference.lUnit && TestPreference.uBatteryCapaicty != TestPreference.uFormerBatteryCapacity)
				{
					UpdateCurrentToCRate(pTestSchedule, &TestPreference);
				}
				UpdateStepListView(pTestSchedule, hTestScheduleListView);
			}
			break;
		default:
			break;
		}
		break;

	case WM_TIMER:
#ifdef RELEASE_DEBUG
		//OutputDebugString("BMW: In WM_TIMER branch");
#endif
#ifdef RELEASE_DEBUG
		{
			//CHAR szPresentStep[1024] = { 0 };
			//sprintf(szPresentStep, "BMW: pPresentStep is 0X%08X", pPresentStep);
			//OutputDebugString(szPresentStep);
		}

#endif // RELEASE_DEBUG
		dwWaitState = WAIT_OBJECT_0;

		if (bAllInstrumentsOpened)
		{
			dwWaitState = WaitForSingleObject(InstHandleSet.hEventDMMReadDone, 1);		//this is for debug purpose, in debug, the timer keeps on triggerring though the program is pended at the break point
		}
		if (WAIT_OBJECT_0 != dwWaitState)
		{
			break;
		}

		if (bAllInstrumentsOpened)
		{
			if (!(InstHandleSet.hCOMCtrl) || !(InstHandleSet.hTempMeter)\
				|| !(InstHandleSet.viCurrMeter) || !(InstHandleSet.viLoad & InstHandleSet.viVoltMeter) || !(InstHandleSet.viVoltSrc))
			{
				uiPresentStepNumber = 0;			//if something wrong with any instruments, stop the test.
			}
		}
		DllConfigVoltMeter(bAllInstrumentsOpened, InstHandleSet.viVoltMeter, &RecordItem, szVMeterIDString);
		DllConfigAmpMeter(bAllInstrumentsOpened, InstHandleSet.viCurrMeter, &RecordItem, szIMeterIDString);
		DllInitiateAndTriggerVAMeters(bAllInstrumentsOpened, InstHandleSet.viVoltMeter, InstHandleSet.viCurrMeter, &RecordItem, szVMeterIDString, szIMeterIDString);

		if (!bAllInstrumentsOpened)
		{
			GetSystemDataString(szDate);
			GetSystemTimeString(szTime);
			strcpy(RecordItem.szDate, szDate);
			strcpy(RecordItem.szTime, szTime);
		}

		UpdateRecordItemDisplay(bAllInstrumentsOpened, \
			hEditDate, \
			hEditTime, \
			hEditTimer, \
			hEditVolt, \
			hEditCurr, \
			hEditTemp, \
			&RecordItem);

		//WriteRecordItem(RecordItem.hLogFile, RecordItem.bLogStarted, TRUE, RecordItem);
#ifdef RELEASE_DEBUG
		{
			//CHAR szPresentStep[1024] = { 0 };
			//sprintf(szPresentStep, "BMW: pPresentStep is uiPresentStepNumber = %d, pPresentStep->uiStepNumber = %d", uiPresentStepNumber, pPresentStep->uiStepNumber);
			//OutputDebugString(szPresentStep);
		}

#endif // RELEASE_DEBUG

		if (bTestThreadMsgQueueCreated && 0 != uiPresentStepNumber)
		{
			ListView_SetItemState(hTestScheduleListView, -1, 0, LVIS_FOCUSED | LVIS_SELECTED);
			if (uiPresentStepNumber != pPresentStep->uiStepNumber)													//这一步在切换测试步骤时已经做了一遍，这里应该不会再做了
			{
				ListView_SetItemState(hTestScheduleListView, pPresentStep->uiStepNumber - 1, 0, LVIS_DROPHILITED);
				pPresentStep = LocateStepWithTestNumber(pTestSchedule, uiPresentStepNumber);
				memset(szTestStatusString, 0, MAX_TEST_STATUS_STRING_LENGTH);
				if (bHideCondition)
					strcpy(szTestStatusString, "Testing...");
				else
					UpdateTestStatus(pPresentStep, szTestStatusString, &RecordItem);									//this must be done before post message to the thread, otherwise the diplay might be stagnant
				SetWindowText(hStaticTestStatus, szTestStatusString);

				PostThreadMessage(dwTestThreadID, START_TEST_STEP, (WPARAM)(&RecordItem), (LPARAM)(&pPresentStep));
			}
			else
			{
				memset(szTestStatusString, 0, MAX_TEST_STATUS_STRING_LENGTH);										//this is for the successive condition check of the present step
				if (bHideCondition)
					strcpy(szTestStatusString, "Testing...");
				else
					UpdateTestStatus(pPresentStep, szTestStatusString, &RecordItem);										//this must be done before post message to the thread, otherwise the diplay might be stagnant
				SetWindowText(hStaticTestStatus, szTestStatusString);
				bHighlight ^= 1;
				if (bHighlight)
					ListView_SetItemState(hTestScheduleListView, pPresentStep->uiStepNumber - 1, LVIS_DROPHILITED, LVIS_DROPHILITED)
				else
					ListView_SetItemState(hTestScheduleListView, pPresentStep->uiStepNumber - 1, 0, LVIS_DROPHILITED);

				PostThreadMessage(dwTestThreadID, CHECK_TEST_STEP_1S, (WPARAM)(&RecordItem), (LPARAM)(&pPresentStep));
			}
		}

		if (bTestThreadMsgQueueCreated && 0 == uiPresentStepNumber && 0 != pPresentStep->uiStepNumber)
		{
			memset(szTestStatusString, 0, MAX_TEST_STATUS_STRING_LENGTH);										//this is for completing the test
			if (bHideCondition)
				strcpy(szTestStatusString, "Testing...");
			else
				UpdateTestStatus(pPresentStep, szTestStatusString, &RecordItem);									//this must be done before post message to the thread, otherwise the diplay might be stagnant
			SetWindowText(hStaticTestStatus, szTestStatusString);

			pPresentStep = pTestSchedule;

			for (UINT i = 0; i < FindMaxStepNumber(pTestSchedule); i++)
				ListView_SetItemState(hTestScheduleListView, i, 0, LVIS_DROPHILITED);
		}

		if (0 == uiPresentStepNumber && 0 == pPresentStep->uiStepNumber)
		{
			if (bAllInstrumentsOpened)
			{
				DisconnectCharge(InstHandleSet.hCOMCtrl);
				DisconnectLoad(InstHandleSet.hCOMCtrl);
			}
			SetWindowText(hStaticTestStatus, "IDLE");

			//below code shutdown all instruments if any of the inst handle is NULL
			if (!(InstHandleSet.hCOMCtrl) || !(InstHandleSet.hTempMeter)\
				|| !(InstHandleSet.viCurrMeter) || !(InstHandleSet.viLoad & InstHandleSet.viVoltMeter) || !(InstHandleSet.viVoltSrc))
			{
				if (VI_NULL != InstHandleSet.viVoltSrc)
				{
					viPrintf(InstHandleSet.viVoltSrc, "*RST\n");
					viClose(InstHandleSet.viVoltSrc);
					InstHandleSet.viVoltSrc = VI_NULL;
				}

				if (VI_NULL != InstHandleSet.viLoad)
				{
					viPrintf(InstHandleSet.viLoad, "*RST\n");
					viClose(InstHandleSet.viLoad);
					InstHandleSet.viLoad = VI_NULL;
				}

				if (VI_NULL != InstHandleSet.viVoltMeter)
				{
					viDiscardEvents(InstHandleSet.viVoltMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
					viPrintf(InstHandleSet.viVoltMeter, "*RST\n");
					viDisableEvent(InstHandleSet.viVoltMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
					//viUninstallHandler(InstHandleSet.viVoltMeter, VI_EVENT_SERVICE_REQ, DllReadVoltageCallback, (ViAddr)(&RecordItem));
					//viUninstallHandler(InstHandleSet.viVoltMeter, VI_EVENT_IO_COMPLETION, DllReadVoltageCompletion, (ViAddr)(&RecordItem));
					viClose(InstHandleSet.viVoltMeter);
					InstHandleSet.viVoltMeter = VI_NULL;
				}

				if (VI_NULL != InstHandleSet.viCurrMeter)
				{
					viDiscardEvents(InstHandleSet.viCurrMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
					viPrintf(InstHandleSet.viCurrMeter, "*RST\n");
					viDisableEvent(InstHandleSet.viCurrMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
					//viUninstallHandler(InstHandleSet.viCurrMeter, VI_EVENT_SERVICE_REQ, DllReadCurrentCallback, (ViAddr)(&RecordItem));
					//viUninstallHandler(InstHandleSet.viCurrMeter, VI_EVENT_IO_COMPLETION, DllReadCurrentCompletion, (ViAddr)(&RecordItem));
					viClose(InstHandleSet.viCurrMeter);
					InstHandleSet.viCurrMeter = VI_NULL;
				}

				DeleteCriticalSection(&(RecordItem.criticSection));

				if (NULL != InstHandleSet.hTempMeter)
				{
					WaitForSingleObject(hMutexTemp, INFINITE);
					TempAccessBlock.bCloseCOMHandle = TRUE;
					EnforceReleaseMutex(hMutexTemp);
				}

				if (NULL != InstHandleSet.hCOMCtrl)
				{
					CloseHandle(InstHandleSet.hCOMCtrl);
					InstHandleSet.hCOMCtrl = NULL;
				}

				if (NULL != InstHandleSet.hTempChamber)
				{
					DllShutdownTempChmaber(InstHandleSet.hTempChamber);
					DllCloseTempChamber(InstHandleSet.hTempChamber);
				}

				bAllInstrumentsOpened = FALSE;

			}
		}

		UpdateControlStatus(bAllInstrumentsOpened, &RecordItem);
		if (bAllInstrumentsOpened)
		{
			ViEventType etype;
			//ViEvent eventContext;
			ViStatus viStatus;

			if (!RecordItem.uiHighFrequenceyReadCounter)
			{
				viStatus = viWaitOnEvent(InstHandleSet.viCurrMeter, VI_EVENT_IO_COMPLETION, VI_TMO_1000MS, &etype, NULL);
				if (viStatus < VI_SUCCESS)
				{
					ViStatus viStatus;
					//ViUInt16 vuiData;
					//DWORD dwRetCount;
					//ViJobId jobID;

					viStatus = viDiscardEvents(InstHandleSet.viCurrMeter, VI_EVENT_IO_COMPLETION, VI_ALL_MECH);
				}
			}

			viStatus = viWaitOnEvent(InstHandleSet.viVoltMeter, VI_EVENT_IO_COMPLETION, VI_TMO_1000MS, &etype, NULL);
			if (viStatus < VI_SUCCESS)
			{
				ViStatus viStatus;
				//ViUInt16 vuiData;
				//DWORD dwRetCount;
				//ViJobId jobID;

				viStatus = viDiscardEvents(InstHandleSet.viVoltMeter, VI_EVENT_IO_COMPLETION, VI_ALL_MECH);
			}
		}

		GetSystemDataString(szDate);
		GetSystemTimeString(szTime);

		UpdateRecordItem(bAllInstrumentsOpened, RecordItem.bLogStarted, &TempAccessBlock, &RecordItem, szDate, szTime);
		WriteRecordItem(RecordItem.hLogFile, RecordItem.bLogStarted, TRUE, RecordItem);	
		
		if (RecordItem.uiHighFrequenceyReadCounter)
		{
			RecordItem.uiHighFrequenceyReadCounter--;
		}
		SetEvent(InstHandleSet.hEventDMMReadDone);

		break;

	case WM_TEST_STEP_FINISHED:
		
		uiPresentStepNumber = wParam;

		ListView_SetItemState(hTestScheduleListView, pPresentStep->uiStepNumber - 1, 0, LVIS_DROPHILITED);
		pPresentStep = LocateStepWithTestNumber(pTestSchedule, uiPresentStepNumber);

		if (!pPresentStep)
		{
			PostMessage(hWnd, WM_TEST_SCHEDULE_DONE, 0, 0);				//this is for the scenario when last step is GOTO with a conditioned bifurcation
			break;
		}

		if (pPresentStep->StepOperation == REST && (RecordItem.dblCurrent != 0) && (RecordItem.bHighFrequencyDataNeeded == TRUE))
		{
			RecordItem.uiHighFrequenceyReadCounter = HIGH_FREQ_READ_COUNT;
//			KillTimer(hWnd, ID_TIMER1);
//			PostMessage(hWnd, WM_TIMER_HI_FREQ, 0, 0);
		}

		memset(szTestStatusString, 0, MAX_TEST_STATUS_STRING_LENGTH);
		if (bHideCondition)
			strcpy(szTestStatusString, "Testing...");
		else
			UpdateTestStatus(pPresentStep, szTestStatusString, &RecordItem);									//this must be done before post message to the thread, otherwise the diplay might be stagnant
		SetWindowText(hStaticTestStatus, szTestStatusString);

		PostThreadMessage(dwTestThreadID, START_TEST_STEP, (WPARAM)(&RecordItem), (LPARAM)(&pPresentStep));

		break;

	case WM_TEST_SCHEDULE_DONE:
		uiPresentStepNumber = 0;
		pPresentStep = pTestSchedule;
		DllInitTempChamber(InstHandleSet.hTempChamber);
		DllShutdownTempChmaber(InstHandleSet.hTempChamber);

		break;

	case WM_CLOSE:

		if (0 != uiPresentStepNumber || 0 != pPresentStep->uiStepNumber)
		{
			MessageBox(NULL, "Please Stop Test First!", NULL, MB_OK);
			break;
		}
		if (VI_NULL != InstHandleSet.viVoltSrc)
		{
			viPrintf(InstHandleSet.viVoltSrc, "*RST\n");
			viClose(InstHandleSet.viVoltSrc);
			InstHandleSet.viVoltSrc = VI_NULL;
		}

		if (VI_NULL != InstHandleSet.viLoad)
		{
			viPrintf(InstHandleSet.viLoad, "*RST\n");
			viClose(InstHandleSet.viLoad);
			InstHandleSet.viLoad = VI_NULL;
		}

		if (VI_NULL != InstHandleSet.viVoltMeter)
		{
			viDiscardEvents(InstHandleSet.viVoltMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
			viPrintf(InstHandleSet.viVoltMeter, "*RST\n");
			viDisableEvent(InstHandleSet.viVoltMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
			//viUninstallHandler(InstHandleSet.viVoltMeter, VI_EVENT_SERVICE_REQ, DllReadVoltageCallback, (ViAddr)(&RecordItem));
			//viUninstallHandler(InstHandleSet.viVoltMeter, VI_EVENT_IO_COMPLETION, DllReadVoltageCompletion, (ViAddr)(&RecordItem));
			
			ViStatus viStatus = viClose(InstHandleSet.viVoltMeter);
			InstHandleSet.viVoltMeter = VI_NULL;
		}

		if (VI_NULL != InstHandleSet.viCurrMeter)
		{
			viDiscardEvents(InstHandleSet.viCurrMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
			viPrintf(InstHandleSet.viCurrMeter, "*RST\n");
			viDisableEvent(InstHandleSet.viCurrMeter, VI_ALL_ENABLED_EVENTS, VI_ALL_MECH);
			//viUninstallHandler(InstHandleSet.viCurrMeter, VI_EVENT_SERVICE_REQ, DllReadCurrentCallback, (ViAddr)(&RecordItem));
			//viUninstallHandler(InstHandleSet.viCurrMeter, VI_EVENT_IO_COMPLETION, DllReadCurrentCompletion, (ViAddr)(&RecordItem));
			
			ViStatus viStatus = viClose(InstHandleSet.viCurrMeter);
			InstHandleSet.viCurrMeter = VI_NULL;
		}

		DeleteCriticalSection(&(RecordItem.criticSection));

		if (NULL != InstHandleSet.hTempMeter)
		{
			WaitForSingleObject(hMutexTemp, INFINITE);
			TempAccessBlock.bCloseCOMHandle = TRUE;
			EnforceReleaseMutex(hMutexTemp);
		}

		if (NULL != InstHandleSet.hCOMCtrl)
		{
			CloseHandle(InstHandleSet.hCOMCtrl);
			InstHandleSet.hCOMCtrl = NULL;
		}

		if (NULL != InstHandleSet.hTempChamber)
		{
			DllCloseTempChamber(InstHandleSet.hTempChamber);
			InstHandleSet.hTempChamber = NULL;
		}

		if (NULL != hKeyDev)
		{
			Result = GetComboString(hComboVoltSrc, VoltSrcDevString);
			if (ERROR_SUCCESS == Result)
				Result = RegSetKeyValue(hKeyDev, "", "VoltSrc", REG_SZ, VoltSrcDevString, strlen(VoltSrcDevString));

			Result = GetComboString(hComboCurrSrc, CurrSrcDevString);
			if (ERROR_SUCCESS == Result)
				Result = RegSetKeyValue(hKeyDev, "", "CurrSrc", REG_SZ, CurrSrcDevString, strlen(CurrSrcDevString));

			Result = GetComboString(hComboVoltMeter, VoltMeterDevString);
			if (ERROR_SUCCESS == Result)
				Result = RegSetKeyValue(hKeyDev, "", "VoltMeter", REG_SZ, VoltMeterDevString, strlen(VoltMeterDevString));

			Result = GetComboString(hComboCurrMeter, CurrMeterDevString);
			if (ERROR_SUCCESS == Result)
				Result = RegSetKeyValue(hKeyDev, "", "CurrMeter", REG_SZ, CurrMeterDevString, strlen(CurrMeterDevString));

			Result = GetComboString(hComboTempMeter, TempMeterDevString);
			if (ERROR_SUCCESS == Result)
				Result = RegSetKeyValue(hKeyDev, "", "TempMeter", REG_SZ, TempMeterDevString, strlen(TempMeterDevString));

			Result = GetComboString(hComboCOMCtrl, COMDevString);
			if (ERROR_SUCCESS == Result)
				Result = RegSetKeyValue(hKeyDev, "", "COMCtrl", REG_SZ, COMDevString, strlen(COMDevString));
			
			Result = GetComboString(hComboTempChamber, TempChamberDevString);
			if (ERROR_SUCCESS == Result)
				Result = RegSetKeyValue(hKeyDev, "", "TempChamber", REG_SZ, TempChamberDevString, strlen(TempChamberDevString));

			RegCloseKey(hKeyDev);
			hKeyDev = NULL;
		}

		if (NULL != hKeyFileName && INVALID_HANDLE_VALUE != RecordItem.hLogFile)
		{
			Result = GetWindowText(hStaticFileName, szLogFileName, MAX_FILE_NAME_LENGTH);
			if (strlen(szLogFileName) == Result)
				Result = RegSetKeyValue(hKeyFileName, "", "LogFileName", REG_SZ, szLogFileName, strlen(szLogFileName));
		}

		RegCloseKey(hKeyFileName);
		hKeyFileName = NULL;

		if (NULL != RecordItem.hLogFile)
		{
			//if (!bHideCondition)
			//{
				CloseHandle(RecordItem.hLogFile);
				RecordItem.hLogFile = NULL;
			//}
			//else
			//	DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_SAVE_LOG_DATA), hWnd, SaveCloseLogFile, (LPARAM)(&RecordItem));
				//SaveCloseLogFile(&(RecordItem.hLogFile), bHideCondition);
				//RecordItem.hLogFile = NULL;
		}

		if (DefaultRM != VI_NULL)
		{
			viClose(DefaultRM);
			DefaultRM = VI_NULL;
		}

		if (hTempReadThread)
			CloseHandle(hTempReadThread);

		if (hTestExecuteThread)
			CloseHandle(hTestExecuteThread);

		if (pEncData)
		{
			DeleteEncData(pEncData);
			pEncData = NULL;
		}

		KillTimer(hWnd, ID_TIMER1);

		ReleaseScheudleMem(&pTestSchedule);

		DeleteObject(hAppIcon);

		DestroyWindow(hWnd);
		break;
	}
	return FALSE;
}

BOOL WINAPI SetLogInterval(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hEditLogInterval, hChkBoxHighFreqNeeded, hChkMarkDsgState;
	UINT uiLogInterval = 0;
	BOOL bHighFrequencyDataNeeded = FALSE, bMarkDsgState = TRUE;
	char szLogInterval[MAX_STRING_LENGTH] = { 0 }, *szEndOfString = NULL;
	static LOG_OPTION *pLogOption;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		pLogOption = (LOG_OPTION *)lParam;
		hEditLogInterval = GetDlgItem(hWnd, IDC_EDIT_LOG_INTERVAL);
		hChkBoxHighFreqNeeded = GetDlgItem(hWnd, IDC_CHECK_HIGH_FREQ_NEEDED);
		hChkMarkDsgState = GetDlgItem(hWnd, IDC_CHECK_MARK_DSG_STATE);
		uiLogInterval = pLogOption->uiLogInterval;
		bHighFrequencyDataNeeded = pLogOption->dwOptions & LOG_OPTION_HI_FREQ_DATA_NEEDED;
		bMarkDsgState = pLogOption->dwOptions & LOG_OPTION_D_STATE_MARK;
		sprintf(szLogInterval, "%d", uiLogInterval);
		SetWindowText(hEditLogInterval, szLogInterval);
		Button_SetCheck(hChkBoxHighFreqNeeded, bHighFrequencyDataNeeded);
		Button_SetCheck(hChkMarkDsgState, bMarkDsgState);
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			GetWindowText(hEditLogInterval, szLogInterval, MAX_STRING_LENGTH);
			uiLogInterval = strtol(szLogInterval, &szEndOfString, 10);
			bHighFrequencyDataNeeded = Button_GetCheck(hChkBoxHighFreqNeeded);
			bMarkDsgState = Button_GetCheck(hChkMarkDsgState);

			if (*szEndOfString)
			{
				MessageBox(NULL, "Invalid Input", NULL, MB_OK);
				break;
			}
			else
			{
				if (uiLogInterval < 1)
				{
					MessageBox(NULL, "Data Out Of Range", NULL, MB_OK);
					break;
				}
				else
				{
					pLogOption->uiLogInterval = uiLogInterval;
					pLogOption->dwOptions = bHighFrequencyDataNeeded * LOG_OPTION_HI_FREQ_DATA_NEEDED + bMarkDsgState* LOG_OPTION_D_STATE_MARK;
					EndDialog(hWnd, S_OK);
					return TRUE;
				}
			}
			break;

		case IDCANCEL:
			EndDialog(hWnd, ERROR_FAIL);
			break;

		default:
			break;

		}
		break;
		/*
		case WM_CLOSE:
		Sleep(1);
		break;
		*/
	default:
		break;
	}

	return FALSE;
}

void InitListView(HWND hTestScheduleList)
{
	LV_ITEM ListRow;
	LV_COLUMN ListColumn;
	RECT ViewRect = { 0 };

	ZeroMemory(&ListRow, sizeof(LV_ITEM));
	ZeroMemory(&ListColumn, sizeof(LV_COLUMN));

	ListColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT; // 风格
	ListColumn.fmt = LVCFMT_CENTER;
	ListColumn.cx = STEP_WIDTH;
	ListColumn.pszText = "Step"; // 文字
	SendMessage(hTestScheduleList, LVM_INSERTCOLUMN, STEP_COLUMN + 1, (LPARAM)&ListColumn);
	ListColumn.cx = OPERATION_WIDTH;
	ListColumn.pszText = "               Operation";
	ListColumn.fmt = LVCFMT_LEFT;
	SendMessage(hTestScheduleList, LVM_INSERTCOLUMN, OPERATION_COLUMN + 1, (LPARAM)&ListColumn);
	ListColumn.cx = VOLT_WIDTH;
	ListColumn.pszText = "Voltage      ";
	ListColumn.fmt = LVCFMT_RIGHT;
	SendMessage(hTestScheduleList, LVM_INSERTCOLUMN, VOLT_COLUMN + 1, (LPARAM)&ListColumn);
	ListColumn.cx = CURR_WIDTH;
	ListColumn.pszText = "Current      ";
	ListColumn.fmt = LVCFMT_RIGHT;
	SendMessage(hTestScheduleList, LVM_INSERTCOLUMN, CURR_COLUMN + 1, (LPARAM)&ListColumn);
	ListColumn.cx = TEMP_WIDTH;
	ListColumn.pszText = "Temperature  ";
	ListColumn.fmt = LVCFMT_RIGHT;
	SendMessage(hTestScheduleList, LVM_INSERTCOLUMN, CURR_COLUMN + 1, (LPARAM)&ListColumn);
	GetClientRect(hTestScheduleList, &ViewRect);
	ListColumn.cx = (ViewRect.right - ViewRect.left) - (STEP_WIDTH + OPERATION_WIDTH + VOLT_WIDTH + CURR_WIDTH + TEMP_WIDTH);
	ListColumn.pszText = "                                                          Terminate(Or Jump)Condition";
	ListColumn.fmt = LVCFMT_LEFT;
	SendMessage(hTestScheduleList, LVM_INSERTCOLUMN, COND_COLUMN + 1, (LPARAM)&ListColumn);

	ListView_SetExtendedListViewStyle(hTestScheduleList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_AUTOCHECKSELECT);

}

void UpdateStepListView(TEST_SCHEDULE_PTR pTestSchedule, HWND hTestScheduleListView)
{
	LV_ITEM ListRow;
	LV_COLUMN ListColumn;
	TEST_STEP *pTestStep = NULL;
	char szStepNumber[MAX_STRING_LENGTH] = { 0 }, szOpType[MAX_STRING_LENGTH] = { 0 }, szVoltage[MAX_STRING_LENGTH] = { 0 }, szCurrent[MAX_STRING_LENGTH] = { 0 }, szTermCond[MAX_STRING_LENGTH] = { 0 }, szTemp[MAX_STRING_LENGTH] = { 0 };

	ZeroMemory(&ListRow, sizeof(LV_ITEM));
	ZeroMemory(&ListColumn, sizeof(LV_COLUMN));

	ListView_DeleteAllItems(hTestScheduleListView);

	pTestStep = pTestSchedule;

	ListRow.mask = LVIF_TEXT;       // 文字
	ListRow.cchTextMax = MAX_PATH;       // 文字长度
		
	while (NULL != pTestStep->pNextStep)
	{
		pTestStep = pTestStep->pNextStep;
		sprintf(szStepNumber, "%d", pTestStep->uiStepNumber);
		ListRow.iItem = pTestStep->uiStepNumber - 1;
		ListRow.iSubItem = STEP_COLUMN;
		ListRow.pszText = szStepNumber;
		ListView_InsertItem(hTestScheduleListView, &ListRow);

		switch (pTestStep->StepOperation)
		{
			/*
			case NOP:
			case REST:
			ListRow.iSubItem = OPERATION_COLUMN;
			strcpy(szOpType, szOperation[pTestStep->StepOperation]);
			ListRow.pszText = szOpType;
			ListView_SetItem(hTestScheduleListView, &ListRow);
			if (!bHideCondition)
			{
			ListRow.iSubItem = VOLT_COLUMN;
			ListRow.pszText = "";
			ListView_SetItem(hTestScheduleListView, &ListRow);

			ListRow.iSubItem = CURR_COLUMN;
			ListRow.pszText = "";
			ListView_SetItem(hTestScheduleListView, &ListRow);

			ListRow.iSubItem = TEMP_COLUMN;
			ListRow.pszText = "";
			ListView_SetItem(hTestScheduleListView, &ListRow);
			}
			break;

			*/
		case SET_TEMP:
			ListRow.iSubItem = OPERATION_COLUMN;
			strcpy(szOpType, szOperation[pTestStep->StepOperation]);
			ListRow.pszText = szOpType;
			ListView_SetItem(hTestScheduleListView, &ListRow);

			if (!bHideCondition)
			{
				ListRow.iSubItem = VOLT_COLUMN;
				ListRow.pszText = "";
				ListView_SetItem(hTestScheduleListView, &ListRow);

				ListRow.iSubItem = CURR_COLUMN;
				ListRow.pszText = "";
				ListView_SetItem(hTestScheduleListView, &ListRow);

				ListRow.iSubItem = TEMP_COLUMN;
				sprintf(szTemp, "%d°C", pTestStep->iTemperature);
				ListRow.pszText = szTemp;
				ListView_SetItem(hTestScheduleListView, &ListRow);
			}
			break;
		case CHARGE:
			ListRow.iSubItem = OPERATION_COLUMN;
			strcpy(szOpType, szOperation[pTestStep->StepOperation]);
			ListRow.pszText = szOpType;
			ListView_SetItem(hTestScheduleListView, &ListRow);

			if (!bHideCondition)
			{
				ListRow.iSubItem = VOLT_COLUMN;
				sprintf(szVoltage, "%dmV", pTestStep->iVoltage);
				ListRow.pszText = szVoltage;
				ListView_SetItem(hTestScheduleListView, &ListRow);

				ListRow.iSubItem = CURR_COLUMN;

				if (C_Rate == TestPreference.lUnit)
					sprintf(szCurrent, "%5.3lfC", pTestStep->dblCurrent / (float)TestPreference.uBatteryCapaicty);
				else
					sprintf(szCurrent, "%5.3lfmA", pTestStep->dblCurrent);

				ListRow.pszText = szCurrent;
				ListView_SetItem(hTestScheduleListView, &ListRow);

				ListRow.iSubItem = TEMP_COLUMN;
				ListRow.pszText = "";
				ListView_SetItem(hTestScheduleListView, &ListRow);
			}
			break;
		case DISCHARGE:
			ListRow.iSubItem = OPERATION_COLUMN;
			strcpy(szOpType, szOperation[pTestStep->StepOperation]);
			ListRow.pszText = szOpType;
			ListView_SetItem(hTestScheduleListView, &ListRow);

			if (!bHideCondition)
			{
				ListRow.iSubItem = VOLT_COLUMN;
				ListRow.pszText = "";
				ListView_SetItem(hTestScheduleListView, &ListRow);

				ListRow.iSubItem = CURR_COLUMN;

				if (C_Rate == TestPreference.lUnit)
					sprintf(szCurrent, "%5.3lfC", pTestStep->dblCurrent / (float)TestPreference.uBatteryCapaicty);
				else
					sprintf(szCurrent, "%5.3lfmA", pTestStep->dblCurrent);

				ListRow.pszText = szCurrent;
				ListView_SetItem(hTestScheduleListView, &ListRow);

				ListRow.iSubItem = TEMP_COLUMN;
				ListRow.pszText = "";
				ListView_SetItem(hTestScheduleListView, &ListRow);
			}
			break;
		case GOTO:
			ListRow.iSubItem = OPERATION_COLUMN;
			sprintf(szOpType, "%s %d", szOperation[pTestStep->StepOperation], pTestStep->iJumpStepNumber);
			ListRow.pszText = szOpType;
			ListView_SetItem(hTestScheduleListView, &ListRow);

			ListRow.iSubItem = VOLT_COLUMN;
			ListRow.pszText = "";
			ListView_SetItem(hTestScheduleListView, &ListRow);

			ListRow.iSubItem = CURR_COLUMN;
			ListRow.pszText = "";
			ListView_SetItem(hTestScheduleListView, &ListRow);

			ListRow.iSubItem = TEMP_COLUMN;
			ListRow.pszText = "";
			ListView_SetItem(hTestScheduleListView, &ListRow);
			break;
		case NOP:
		case REST:
		case CNTR_INC:
		case CNTR_DEC:
		case CNTR_CLR:
		case PASSEDCHARGE_ON:
		case PASSEDCHARGE_OFF:
		case PASSEDCHARGE_CLEAR:
		case START_LOG:
		case STOP_LOG:
		case SHTDN_TCHMBR:
			ListRow.iSubItem = OPERATION_COLUMN;
			sprintf(szOpType, "%s", szOperation[pTestStep->StepOperation]);
			ListRow.pszText = szOpType;
			ListView_SetItem(hTestScheduleListView, &ListRow);
			break;
		default:
			break;
		}

		if (!bHideCondition)
		{
			memset(szConditionExpression, 0, MAX_CONDITION_EXP_LENGTH);
			ListRow.iSubItem = COND_COLUMN;
			PrintConditionString(pTestStep->pTerminateCondition, szConditionExpression);
			ListRow.pszText = szConditionExpression;
			ListView_SetItem(hTestScheduleListView, &ListRow);
		}
	}
}

void FlashStepListView(UINT StepNumber, HWND hListView, BOOL bHighlight)
{
	UINT uiTotalStepNumber;
	RECT ItemRect;
	HDC hDC;
	HBRUSH hBR;
	COLORREF clr;

	uiTotalStepNumber = ListView_GetItemCount(hListView);
	if (0 == StepNumber || uiTotalStepNumber == StepNumber)
		return;

	ListView_GetItemRect(hListView, StepNumber - 1, &ItemRect, LVIR_BOUNDS);
	hDC = GetDC(hListView);
	
	if (bHighlight)
		clr = RGB(0, 128, 128);
	else
		clr = GetDCBrushColor(hDC);
	
	hBR = CreateSolidBrush(clr);
	FillRect(hDC, &ItemRect, hBR);
}

BOOL PrintConditionString(TERM_COND *pCondition, LPTSTR szExpression)
{
	if (NULL == pCondition || NULL == szExpression)
		return FALSE;

	RecursiveParseAndPrintCondition(pCondition, &szExpression);

	return TRUE;
}

void RecursiveParseAndPrintCondition(TERM_COND *pCondition, LPTSTR *pszExpression)
{
	if (NULL != pCondition->pLeftCondition && Bifurcation == pCondition->ConditionType)
	{
		if (0 != pCondition->uiConditionID && \
			NULL != pCondition->pRightCondition
			)
		{
			if (!(NULL == pCondition->pRightCondition->pLeftCondition && \
				NULL == pCondition->pRightCondition->pRightCondition && \
				Bifurcation == pCondition->pRightCondition->ConditionType) && \
				!(NULL == pCondition->pLeftCondition->pLeftCondition &&\
					NULL == pCondition->pLeftCondition->pRightCondition &&\
					Bifurcation == pCondition->pLeftCondition->ConditionType))
			{
				**pszExpression = '(';
				(*pszExpression)++;
			}
		}
		RecursiveParseAndPrintCondition(pCondition->pLeftCondition, pszExpression);

		if (NULL != pCondition->pRightCondition && NULL != pCondition->pLeftCondition)
		{
			if (!(NULL == pCondition->pRightCondition->pLeftCondition && \
				NULL == pCondition->pRightCondition->pRightCondition && \
				Bifurcation == pCondition->pRightCondition->ConditionType) && \
				!(NULL == pCondition->pLeftCondition->pLeftCondition &&\
					NULL == pCondition->pLeftCondition->pRightCondition &&\
					Bifurcation == pCondition->pLeftCondition->ConditionType))
			{
				(*pszExpression)++;
				sprintf(*pszExpression, " %s ", szLogicalRel[pCondition->RelationShip]);
				*pszExpression += strlen(szLogicalRel[pCondition->RelationShip]) + 2;
			}
		}
	}

	if (NULL != pCondition->pRightCondition && Bifurcation == pCondition->ConditionType)
	{
		RecursiveParseAndPrintCondition(pCondition->pRightCondition, pszExpression);
		if (0 != pCondition->uiConditionID &&\
			NULL != pCondition->pLeftCondition
			)
		{
			if (!(NULL == pCondition->pRightCondition->pLeftCondition && \
				NULL == pCondition->pRightCondition->pRightCondition && \
				Bifurcation == pCondition->pRightCondition->ConditionType) &&\
				!(NULL == pCondition->pLeftCondition->pLeftCondition &&\
					NULL == pCondition->pLeftCondition->pRightCondition &&\
					Bifurcation == pCondition->pLeftCondition->ConditionType))
			{
				(*pszExpression)++;
				**pszExpression = ')';
			}
		}
	}

	if (NULL == pCondition->pLeftCondition && NULL == pCondition->pRightCondition && Bifurcation != pCondition->ConditionType)
	{
		switch (pCondition->ConditionType)
		{
		case Voltage:
		case AvgVoltage:
			sprintf(*pszExpression, "%s %s %dmV", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold);
			break;
		case Current:
		case AvgCurrent:
			if(C_Rate == TestPreference.lUnit)
				sprintf(*pszExpression, "%s %s %5.3fC", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold / (float)TestPreference.uBatteryCapaicty);
			else
				sprintf(*pszExpression, "%s %s %dmA", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold);
			break;
		case VoltageRate:
			sprintf(*pszExpression, "%s %s %dμV/S", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold);
			break;
		case CurrentRate:
			sprintf(*pszExpression, "%s %s %dμA/S", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold);
			break;
		case Temperature:
			sprintf(*pszExpression, "%s %s %d°C", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold);
			break;
		case AllTime:
		case ConditionTime:
			sprintf(*pszExpression, "%s %s %dS", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold);
			break;
		case PassedCharge:
			if (C_Rate == TestPreference.lUnit)
				sprintf(*pszExpression, "%s %s %5.3fDC", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold / (float)TestPreference.uBatteryCapaicty);
			else
				sprintf(*pszExpression, "%s %s %dmAh", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold);
			break;
		case Counter:
			sprintf(*pszExpression, "%s %s %d", szConditionType[pCondition->ConditionType], szComparasonType[pCondition->ComparisonType], pCondition->iThreshold);
			break;
		default:
			break;
		}
		*pszExpression += (strlen(*pszExpression) - 1);
	}
}

BOOL ReleaseScheudleMem(TEST_SCHEDULE_PTR *ppTestSchedule)
{
	UINT i, uiMaxStepNumber = 0;

	uiMaxStepNumber = FindMaxStepNumber(*ppTestSchedule);
	for (i = uiMaxStepNumber; i > 0; i--)
	{
		RemoveStepFromList(*ppTestSchedule, i);					//must remove from the tail of the step list, as this function does not redefine the number of each step
	}

	if (NULL == (*ppTestSchedule)->pNextStep && 0 == (*ppTestSchedule)->uiStepNumber)
	{
		free(*ppTestSchedule);
		*ppTestSchedule = NULL;
		return TRUE;
	}
	else
		return FALSE;
}

void UpdateControlStatus(BOOL bAllIntrumentOpened, REC_ITEM *pRecordItem)
{
	BOOL bTestStarted = FALSE, bTestStepsAvailable = FALSE, bLogFileOpened;

	bTestStarted = (BOOL)(pPresentStep->uiStepNumber);
	bTestStepsAvailable = (BOOL)(pTestSchedule->pNextStep);
	bLogFileOpened = (NULL != pRecordItem->hLogFile);

	EnableWindow(hComboVoltSrc, !bAllIntrumentOpened);
	EnableWindow(hComboCurrSrc, !bAllIntrumentOpened);
	EnableWindow(hComboVoltMeter, !bAllIntrumentOpened);
	EnableWindow(hComboCurrMeter, !bAllIntrumentOpened);
	EnableWindow(hComboTempMeter, !bAllIntrumentOpened);
	EnableWindow(hComboCOMCtrl, !bAllIntrumentOpened);
	EnableWindow(hComboTempChamber, !bAllIntrumentOpened);

	EnableWindow(hButtonStartLog, !pRecordItem->bLogStarted && bAllIntrumentOpened && bLogFileOpened);
	EnableWindow(hButtonAbortLog, pRecordItem->bLogStarted && !bTestStarted);
	EnableWindow(hButtonCloseEquipments, bAllIntrumentOpened && !bTestStarted && !pRecordItem->bLogStarted);
	EnableWindow(hButtonOpenEquipments, !bAllIntrumentOpened);
	EnableWindow(hButtonFileOpen, !bLogFileOpened);
	EnableWindow(hButtonSaveClose, pRecordItem->bLogStarted && !bTestStarted);
	EnableWindow(hButtonLogConfig, !pRecordItem->bLogStarted);

	EnableWindow(hButtonAddStep, !bTestStarted && !bHideCondition);
	EnableWindow(hButtonDeleteStep, !bTestStarted && !bHideCondition);
	EnableWindow(hButtonEditStep, !bTestStarted && !bHideCondition);
	EnableWindow(hButtonSaveSteps, (BOOL)(bTestStepsAvailable) && !bTestStarted);
	EnableWindow(hButtonLoadSteps, !bTestStarted);

	EnableWindow(hButtonStartTest, bTestStepsAvailable &&\
		!bTestStarted && \
		bAllIntrumentOpened && \
		(!HasAutoLog(pTestSchedule) || (HasAutoLog(pTestSchedule) && bLogFileOpened)) && \
		(!HasTempCtrl(pTestSchedule) || (HasTempCtrl(pTestSchedule) && (DllGetChamberStatus(InstHandleSet.hTempChamber) & TC_STATUS_STARTED))));
	EnableWindow(hButtonSelectStartStep, bTestStepsAvailable && !bTestStarted);
	EnableWindow(hButtonStopTest, bTestStarted);

	EnableWindow(hButtonTestPreference, !bTestStarted);
}

BOOL WINAPI SelectStartStep(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UINT uiMaxStepNumber, uiStepNumber, i, uiIndex;
	char szComboString[MAX_STRING_LENGTH] = { 0 };
	static HWND hComboStep;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		hComboStep = GetDlgItem(hWnd, IDC_COMBO_START_STEP);
		uiMaxStepNumber = (UINT)lParam;
		for (i = 1; i <= uiMaxStepNumber; i++)
		{
			sprintf(szComboString, "%d", i);
			SendMessage(hComboStep, CB_ADDSTRING, 0, (LPARAM)szComboString);
		}
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			uiIndex = ComboBox_GetCurSel(hComboStep);
			ComboBox_GetLBText(hComboStep, uiIndex, szComboString);
			sscanf(szComboString, "%d", &uiStepNumber);

			if (CB_ERR == uiStepNumber)
				MessageBox(NULL, "Start Step Not Selected!", NULL, MB_OK);
			else
			{
//				uiStepNumber += 1;
				EndDialog(hWnd, uiStepNumber);
				return TRUE;
			}
			break;
		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

void TraverseConditionTreeForCurrentToCRate(TERM_COND *pCondition, TEST_PREFERENCE *pTestPreference)
{
	if (Current == pCondition->ConditionType || AvgCurrent == pCondition->ConditionType || PassedCharge == pCondition->ConditionType)
	{
		float fCurrentRatio = (float)pTestPreference->uBatteryCapaicty / (float)pTestPreference->uFormerBatteryCapacity;
		pCondition->iThreshold = (INT)(fCurrentRatio * pCondition->iThreshold + 0.5);
	}

	if (pCondition->pLeftCondition != NULL)
		TraverseConditionTreeForCurrentToCRate(pCondition->pLeftCondition, pTestPreference);

	if (pCondition->pRightCondition != NULL)
		TraverseConditionTreeForCurrentToCRate(pCondition->pRightCondition, pTestPreference);

	if (NULL == pCondition->pLeftCondition && NULL == pCondition->pRightCondition)
		return;
}

void UpdateCurrentToCRate(TEST_SCHEDULE_PTR pTestSchedule, TEST_PREFERENCE *pTestPreference)
{
	TEST_STEP *pTestStep;
	
	if (NULL == pTestSchedule || NULL == pTestPreference)
		return;

	pTestStep = pTestSchedule->pNextStep;

	while (NULL != pTestStep)
	{
		if (CHARGE == pTestStep->StepOperation || DISCHARGE == pTestStep->StepOperation)
		{
			float fCurrentRatio = (float)pTestPreference->uBatteryCapaicty / (float)pTestPreference->uFormerBatteryCapacity;
			pTestStep->dblCurrent = fCurrentRatio*pTestStep->dblCurrent;
		}

		TraverseConditionTreeForCurrentToCRate(pTestStep->pTerminateCondition, pTestPreference);
		pTestStep = pTestStep->pNextStep;
	}

	pTestPreference->uFormerBatteryCapacity = pTestPreference->uBatteryCapaicty;
}

BOOL HasAutoLog(TEST_SCHEDULE_PTR pTestSchedule)
{
	TEST_STEP *pStep;

	if (NULL == pTestSchedule)
		return FALSE;

	pStep = pTestSchedule;

	while (NULL != pStep)
	{
		if (START_LOG == pStep->StepOperation || STOP_LOG == pStep->StepOperation)
		{
			return TRUE;
		}
		pStep = pStep->pNextStep;
	}

	return FALSE;
}

BOOL HasTempCtrl(TEST_SCHEDULE_PTR pTestSchedule)
{
	TEST_STEP *pStep;

	if (NULL == pTestSchedule)
		return FALSE;

	pStep = pTestSchedule;

	while (NULL != pStep)
	{
		if (SET_TEMP == pStep->StepOperation || SHTDN_TCHMBR == pStep->StepOperation)
		{
			return TRUE;
		}
		pStep = pStep->pNextStep;
	}

	return FALSE;

}

BOOL HasCounterOpertion(TEST_SCHEDULE_PTR pTestSchedule)
{
	TEST_STEP *pTestStep;

	if (NULL == pTestSchedule || NULL == pTestSchedule->pNextStep)
		return FALSE;

	pTestStep = pTestSchedule;

	while (NULL != pTestStep->pNextStep)
	{
		pTestStep = pTestStep->pNextStep;

		if (pTestStep->StepOperation == CNTR_CLR || pTestStep->StepOperation == CNTR_DEC || pTestStep->StepOperation == CNTR_INC)
			return TRUE;
	}
	return FALSE;
}


BOOL WINAPI ReadVoltage(LPARAM lParam)
{
	INST_HANDLE_SET *pInstHandleSet;
	ViJobId jobID;
	ViEventType etype;
	ViEvent eventContext;
	ViStatus viStatus;
	DWORD dwRetCount;

	pInstHandleSet = (INST_HANDLE_SET *)lParam;
	while (TRUE)
	{
		WaitForSingleObject(pInstHandleSet->hEventStartVoltReading, INFINITE);
		viStatus = viWrite(pInstHandleSet->viVoltMeter, "READ?\n", strlen("READ?\n"), &dwRetCount);
		viStatus = viReadAsync(pInstHandleSet->viVoltMeter, pInstHandleSet->pRecordItem->VoltReading, pInstHandleSet->pRecordItem->dwVoltReadingCount * ELEMENT_LENGTH, &jobID);

		viStatus = viWaitOnEvent(pInstHandleSet->viVoltMeter, VI_EVENT_IO_COMPLETION, VI_INFINITE, &etype, &eventContext);
		SetEvent(pInstHandleSet->hEventVoltReadDone);
	}
	return ERROR_SUCCESS;
}

BOOL WINAPI ReadCurrent(LPARAM lParam)
{
	INST_HANDLE_SET *pInstHandleSet;
	ViJobId jobID;
	ViEventType etype;
	ViEvent eventContext;
	ViStatus viStatus;
	DWORD dwRetCount;
	pInstHandleSet = (INST_HANDLE_SET *)lParam;

	while (TRUE)
	{
		WaitForSingleObject(pInstHandleSet->hEventStartCurrReading, INFINITE);
		viStatus = viWrite(pInstHandleSet->viCurrMeter, "READ?\n", strlen("READ?\n"), &dwRetCount);
		viStatus = viReadAsync(pInstHandleSet->viCurrMeter, pInstHandleSet->pRecordItem->CurrReading, pInstHandleSet->pRecordItem->dwCurrReadingCount * ELEMENT_LENGTH, &jobID);

		viStatus = viWaitOnEvent(pInstHandleSet->viCurrMeter, VI_EVENT_IO_COMPLETION, VI_INFINITE, &etype, &eventContext);
		WaitForSingleObject(pInstHandleSet->hEventVoltReadDone, INFINITE);
		SetEvent(pInstHandleSet->hEventDMMReadDone);
	}

	return ERROR_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////
// 函数名： GetMacByCmd(char *lpszMac)
// 参数：
//      输入： void
//      输出： lpszMac,返回的MAC地址串
// 返回值：
//      TRUE:  获得MAC地址。
//      FALSE: 获取MAC地址失败。
// 过程：
//      1. 创建一个无名管道。
//      2. 创建一个IPCONFIG 的进程，并将输出重定向到管道。
//      3. 从管道获取命令行返回的所有信息放入缓冲区lpszBuffer。
//      4. 从缓冲区lpszBuffer中获得抽取出MAC串。
//
//  提示：可以方便的由此程序获得IP地址等其他信息。
//        对于其他的可以通过其他命令方式得到的信息只需改变strFetCmd 和 
//        str4Search的内容即可。
///////////////////////////////////////////////////////////////////////////

BOOL GetMacByCmd(STRING *pstrMacAddr, int iMacIndex)
{
	//char lpszMac[2048];
	//初始化返回MAC地址缓冲区
	char szFetCmd[] = "ipconfig /all";
	LPSTR lpszTempString = NULL;
	int iIndex = 0;
	BOOL bret;
	SECURITY_ATTRIBUTES sa;
	HANDLE hReadPipe, hWritePipe;

	if (pstrMacAddr->uiLength < MAC_ADDR_LENGTH)
		return FALSE;

	memset(pstrMacAddr->lpszBuffer, 0x00, pstrMacAddr->uiLength);
	
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	//创建管道
	bret = CreatePipe(&hReadPipe, &hWritePipe, &sa, MAX_COMMAND_SIZE + 1);
	if (!bret)
	{
		return FALSE;
	}

	//控制命令行窗口信息
	STARTUPINFO si;
	//返回进程信息
	PROCESS_INFORMATION pi;

	si.cb = sizeof(STARTUPINFO);
	GetStartupInfo(&si);
	si.hStdError = hWritePipe;
	si.hStdOutput = hWritePipe;
	si.wShowWindow = SW_HIDE; //隐藏命令行窗口
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

	//创建获取命令行进程
	bret = CreateProcess(NULL, szFetCmd, NULL, NULL, TRUE, 0, NULL,
		NULL, &si, &pi);

	char szBuffer[MAX_COMMAND_SIZE + 1]; //放置命令行输出缓冲区

	if (bret)
	{
		WaitForSingleObject(pi.hProcess, 3000);
		unsigned long count;
		//CloseHandle(hWritePipe);

		memset(szBuffer, 0x00, sizeof(szBuffer));
		lpszTempString = szBuffer;
		bret = ReadFile(hReadPipe, szBuffer, MAX_COMMAND_SIZE, &count, 0);
		if (!bret)
		{
			//关闭所有的句柄
			CloseHandle(hWritePipe);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			CloseHandle(hReadPipe);
			return FALSE;
		}
		else
		{
			while(iIndex <= iMacIndex)
			{ 
				lpszTempString = strstr(lpszTempString, str4SearchEn);
				if (NULL != lpszTempString)
				{
					lpszTempString += strlen(str4SearchEn);//提取MAC地址串
					iIndex++;
				}
				else
					break;
						
			}

			if (NULL == lpszTempString)			//找不到用中文再找一遍
			{
				lpszTempString = szBuffer;
				iIndex = 0;
				while (iIndex <= iMacIndex)
				{
					lpszTempString = strstr(lpszTempString, str4SearchCh);
					if (NULL != lpszTempString)
					{
						lpszTempString += strlen(str4SearchCh);//提取MAC地址串
						iIndex++;
					}
					else
						break;
				}
			}


			//strBuffer = strBuffer.Mid(ipos + str4Search.GetLength(), 17);
			// ipos = strBuffer.Find("\n");
			// strBuffer = strBuffer.Mid(0, ipos);
		}

	}

	if (!lpszTempString)
		return FALSE;

	//memset(szBuffer, 0x00, sizeof(szBuffer));
	//strncpy(strMacAddr->lpszBuffer, lpszTempString, RAW_MAC_ADDR_LENGTH);

	//去掉中间的“00-50-EB-0F-27-82”中间的'-'得到0050EB0F2782
	int j = 0;
	for (int i = 0; i < RAW_MAC_ADDR_LENGTH; i++)
	{
		if (lpszTempString[i] != '-')
		{
			pstrMacAddr->lpszBuffer[j] = lpszTempString[i];
			j++;
		}
	}

	//mac_addr.Insert(0, lpszMac);
	//关闭所有的句柄
	if (hWritePipe != NULL)
		CloseHandle(hWritePipe);
	if (pi.hProcess != NULL)
		CloseHandle(pi.hProcess);
	if (pi.hThread != NULL)
		CloseHandle(pi.hThread);
	if (hReadPipe)
		CloseHandle(hReadPipe);
	return TRUE;

}
