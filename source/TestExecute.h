#pragma once

#define START_TEST_STEP WM_APP + 1
#define CHECK_TEST_STEP_1S WM_APP + 2
#define WM_TEST_STEP_FINISHED WM_APP + 3
#define STOP_TEST_STEP	WM_APP + 4
#define WM_TEST_SCHEDULE_DONE	WM_APP + 5
#define WM_DMM_DATA_READY	WM_APP + 6
#define WM_TIMER_HI_FREQ		WM_APP + 7
#define WM_UPDATE_PROGRESS_BAR  WM_APP + 8

#define MIN_TIME_RATE_PRODUCT 500

typedef BOOL REL_STATUS;
#define ON	TRUE
#define OFF	FALSE

DWORD WINAPI TestExecuteThread(LPARAM);
BOOL TurnOnChargePath(HANDLE hComCtrl);
BOOL TurnOnDischargePath(HANDLE hComCtrl);
BOOL SetCharger(ViSession viCharger, TEST_STEP *pTestStep);
BOOL SetLoad(ViSession viLoad, TEST_STEP *pTestStep);
BOOL CheckConditionTrueOrFalse(TERM_COND *pCondition, REC_ITEM *pRecordItem);
BOOL RecursiveCheckConditionTrueOrFalse(TERM_COND *pCondition, REC_ITEM *pRecordItem, BOOL bAdjacnetLeftResult);
BOOL CheckNodeCondition(TERM_COND *pCondition, REC_ITEM *pRecordItemBOOL, BOOL bAdjacentLeftResult);
BOOL DisconnectCharge(HANDLE hComCtrl);
BOOL DisconnectLoad(HANDLE hComCtrl);
BOOL GetRelayStatus(HANDLE hComCtrl, REL_STATUS *Chg_Rel_Status, REL_STATUS *Dsg_Rel_Status);
BOOL ShutdownCharger(ViSession viCharger);
BOOL ShutdownLoad(ViSession viLoad);