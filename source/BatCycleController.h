#pragma once

#include <visa.h>
#include "UARTTransceiver.h"
#include "SetTestPreference.h"

#define LOG_OPTION_HI_FREQ_DATA_NEEDED	1
#define LOG_OPTION_D_STATE_MARK			2

#define STEP_WIDTH		40
#define OPERATION_WIDTH 150
#define VOLT_WIDTH		80
#define CURR_WIDTH		80
#define TEMP_WIDTH		80
#define STEP_COLUMN		0
#define OPERATION_COLUMN	1
#define VOLT_COLUMN		2
#define CURR_COLUMN		3
#define TEMP_COLUMN		4
#define COND_COLUMN		5
#define HIGH_FREQ_READ_COUNT 2
#define MAC_ADDR_LENGTH	13
#define RAW_MAC_ADDR_LENGTH	MAC_ADDR_LENGTH + 4
#define MAC_OFFSET	17
#define MAX_COMMAND_SIZE	10000
#define MAX_FILE_NAME_LENGTH	1024
#define InitRecordItem(RecordItem)			(RecordItem).iAvgVoltage = 0;\
											(RecordItem).iAvgCurrent = 0;\
											(RecordItem).iAvgTemperature = 0;\
											(RecordItem).iVoltRate = 0;\
											(RecordItem).iCurrRate = 0;\
											(RecordItem).iTempRate = 0;\
											(RecordItem).iCounter = 0;\
											(RecordItem).llPassedCharge = 0;\
											(RecordItem).llPresentPassedCharge = 0;\
											(RecordItem).bPassedChargeOn = FALSE;\
											(RecordItem).uiPassedChargeCounter = 0;\
											(RecordItem).uiAverageCurrentCounter = 0;\
											(RecordItem).uiAverageVoltageCounter = 0;\
											(RecordItem).uiAverageTempCounter = 0;\
											(RecordItem).uiCurrentAverageWindowLength = CURR_AVERAGE_WINDOW_LENGTH;\
											(RecordItem).uiVoltageAverageWindowLength = VOLT_AVERAGE_WINDOW_LENGTH;\
											(RecordItem).uiTempAverageWindowLength = TEMP_AVERAGE_WINDOW_LENGTH;\
											

typedef enum { NOP, CHARGE, DISCHARGE, REST, GOTO, CNTR_INC, CNTR_DEC, CNTR_CLR, PASSEDCHARGE_ON, PASSEDCHARGE_OFF, PASSEDCHARGE_CLEAR, START_LOG, STOP_LOG, SHTDN_TCHMBR, SET_TEMP}OPERATION;
typedef enum { Bifurcation, Voltage, Current, Temperature, AvgVoltage, AvgCurrent, VoltageRate, CurrentRate, AllTime, PassedCharge, Counter, ConditionTime }COND_TYPE;
typedef enum { NOCOMP, GreaterThan, EqualTo, LessThan, NoGreaterThan, NoLessThan }COMP_TYPE;
typedef enum { AND, OR }LOGICAL_RELATIONSHIP;
typedef enum { CENTER, LEFT, RIGHT }CONDITION_NODE_POSITION;

typedef struct _TERM_COND *pCond;
typedef struct _TERM_COND
{
	UINT uiConditionID;
	COND_TYPE ConditionType;
	COMP_TYPE ComparisonType;
	INT iThreshold;
	UINT uiConditionTimer;
	pCond pRightCondition;
	pCond pLeftCondition;
	LOGICAL_RELATIONSHIP RelationShip;
}TERM_COND;

typedef struct _STRING
{
	UINT uiLength;
	LPSTR lpszBuffer;
}STRING;

typedef struct _TEST_STEP *pNext;
typedef struct _TEST_STEP
{
	UINT uiStepNumber;
	OPERATION StepOperation;
	double dblCurrent;
	INT iVoltage;
	INT iTemperature;
	INT iMinTempTime;
	INT iJumpStepNumber;
	INT(*pCurrent)();
	INT(*pVoltage)();
	TERM_COND *pTerminateCondition;
	pNext pNextStep;
}TEST_STEP, *TEST_SCHEDULE_PTR;



#define MAX_CNT	1000
#define ELEMENT_LENGTH	16
#define VI_TMO_1000MS	1000

typedef struct
{
	char szDate[11];
	char szTime[9];
	UINT uiLogTimer;
	UINT uiLogTimerMilliSec;
	UINT uiTempLogTimerMilliSec;
	UINT uiBatDisconnectTimer;
	UINT uiInstOnTimer;
	UINT uiStepTimer;
	UINT uiStepTimerStart;
	UINT uiConditionTimerStart;
	INT  iVoltage;
	double  dblCurrent;
	INT  iTemperature;
	INT  iAvgVoltage;
	INT  iAvgCurrent;
	INT  iAvgTemperature;
	INT  iVoltRate;
	INT  iVoltRMSE;
	INT  iVoltWindow;
	INT  iCurrRate;
	INT  iCurrRMSE;
	INT  iCurrWindow;
	INT  iTempRate;
	INT  iTempRMSE;
	INT  iTempWindow;
	BOOL bPassedChargeOn;
	UINT uiPassedChargeCounter;
	LONGLONG llPresentPassedCharge;
	DOUBLE dblPresentPassedCharge;		//add this two items for low cap cell test.
	LONGLONG  llPassedCharge;
	DOUBLE dblPassedCharge;				//add this two items for low cap cell test.
	UINT uiAverageVoltageCounter;
	UINT uiAverageCurrentCounter;
	UINT uiAverageTempCounter;
	UINT uiVoltageAverageWindowLength;
	UINT uiCurrentAverageWindowLength;
	UINT uiTempAverageWindowLength;
	INT  iCounter;
	/*
	UINT iAvgVoltRate;
	UINT iAvgCurrRate;
	UINT iAvgTempRate;


	UINT uiAverageVoltageRateCounter;
	UINT uiAverageCurerntRateCounter;
	UINT uiAverageTempRateCounter;
	UINT uiVoltageRateAverageWindowLength;
	UINT uiCurrentRateAverageWindowLength;
	UINT uiTempRateAverageWindowLength;
	UINT uiDeltaTimeForDeltaVoltage;
	UINT uiDeltaTimeForDeltaCurrent;
	UINT uiDeltaTimeForDeltaTemp;
	*/
	UINT uiLogInterval;
	BOOL bTimerCountOnConditionCheck;
	HANDLE hLogFile;
	HANDLE hFakeLogFile;
	BOOL bLogStarted;
	BOOL bHighFrequencyDataNeeded;
	BOOL bMarkDsgState;
	UINT uiHighFrequenceyReadCounter;				//定义在每次电流为零后需要读几遍高频数据
	DWORD dwVoltReadingCount;
	char  VoltReading[ELEMENT_LENGTH * MAX_CNT];
	DWORD dwCurrReadingCount;
	char CurrReading[ELEMENT_LENGTH * MAX_CNT];
	HWND hWnd;
	ViJobId JobIdV;
	ViJobId JobIdI;
	//HANDLE hEventTimerProcessDone;
	CRITICAL_SECTION criticSection;
	BOOL bChargerConnected;
	BOOL bLoadConnected;
}REC_ITEM;

typedef struct _INST_HANDLE_SET
{
	ViSession viVoltSrc;
	ViSession viLoad;
	ViSession viVoltMeter;
	ViSession viCurrMeter;
	HANDLE hCOMCtrl;
	HANDLE hTempMeter;
	HANDLE hTempChamber;
	HWND hWnd;
	HANDLE hEventStartVoltReading;
	HANDLE hEventStartCurrReading;
	HANDLE hEventVoltReadDone;
	HANDLE hEventDMMReadDone;
	REC_ITEM *pRecordItem;
}INST_HANDLE_SET;

typedef struct _LOG_OPTION
{
	UINT uiLogInterval;
	//BOOL bHighFrequencyDataNeeded;
	DWORD dwOptions;
}LOG_OPTION;

void InitListView(HWND);
BOOL WINAPI BatCycleController(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI SetLogInterval(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateStepListView(TEST_SCHEDULE_PTR pTestSchedule, HWND hTestScheduleListView);
BOOL PrintConditionString(TERM_COND *pCondition, LPTSTR szExpression);
void RecursiveParseAndPrintCondition(TERM_COND *pCondition, LPTSTR *szExpression);
BOOL ReleaseScheudleMem(TEST_SCHEDULE_PTR *ppTestSchedule);
void FlashStepListView(UINT StepNumber, HWND hListView, BOOL bHighlight);
void UpdateControlStatus(BOOL bAllIntrumentOpened, REC_ITEM *pRecordItem);
BOOL WINAPI SelectStartStep(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateCurrentToCRate(TEST_SCHEDULE_PTR pTestSchedule, TEST_PREFERENCE *pTestPreference);
void TraverseConditionTreeForCurrentToCRate(TERM_COND *, TEST_PREFERENCE *);
BOOL HasAutoLog(TEST_SCHEDULE_PTR pTestSchedule);
BOOL HasCounterOpertion(TEST_SCHEDULE_PTR pTestSchedule);
BOOL WINAPI ReadVoltage(LPARAM lParam);
BOOL WINAPI ReadCurrent(LPARAM lParam);
BOOL GetMacByCmd(STRING *strMacAddr, int iMaxIndex);
BOOL HasTempCtrl(TEST_SCHEDULE_PTR pTestSchedule);