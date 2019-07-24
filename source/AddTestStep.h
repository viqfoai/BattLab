#pragma once

#include "UARTTransceiver.h"

#define DEFAULT_LOG_INTERVAL	1

#define ADD_BEFORE				0
#define ADD_AFTER				1
#define OVERRIDE_PRESENT		2

#define CBN_STEP_NUMBER_SELECTED	(CBN_SELCHANGE << 16) + IDC_COMBO_STEP_NUMBER
#define CBN_STEP_OPERATION_SELECTED	(CBN_SELCHANGE << 16) + IDC_COMBO_OPERATION_TYPE
#define CBN_CONDITION_ID_CHANGE		(CBN_SELCHANGE << 16) + IDC_COMBO_CONDITION_ID
#define CBN_UP_CONDITION_TYPE_CHANGE	(CBN_SELCHANGE << 16) + IDC_COMBO_CONDITION_TYPE_UP
#define CBN_DOWN_CONDITION_TYPE_CHANGE	(CBN_SELCHANGE << 16) + IDC_COMBO_CONDITION_TYPE_DOWN
#define EN_VOLTAGE_UPDATED			(EN_KILLFOCUS << 16) + IDC_EDIT_VOLT_ADD_STEP
#define EN_CURRENT_UPDATED			(EN_KILLFOCUS << 16) + IDC_EDIT_CURR_ADD_STEP
#define DLGN_ADD_STEP_INIT			(WM_INITDIALOG << 16) + IDD_DIALOG_ADD_STEP
#define DLGN_EDIT_STEP_INIT			(WM_INITDIALOG << 16) + IDD_DIALOG_EDIT_STEP

#define WM_UPDATE_STEP_DIALOG	0x8000

#define ERROR_FAIL -1

#define ERROR_CONDITION_INCORRECT_COMPARATOR	1
#define ERROR_CONDITION_UNINITIALIZED_TREE		2
#define ERROR_STEP_NUMBER_NOT_ASSIGNED			3
#define ERROR_VOLTAGE_INPUT						4
#define ERROR_CURRENT_INPUT						5
#define ERROR_INCORRECT_OPERATION_TYPE			6
#define ERROR_CONDITION_INCORRECT_TYPE			7
#define ERROR_STEP_NUMBER						8

typedef struct _CONTROL_HANDLE_SET
{
	HWND hTestScheduleListView, hParentDialog, hComboStepNumber, hComboOperationType;
	HWND hComboConditonID, hComboConditonIDUp, hComboConditionIDDown, hStaticConditionIDUp, hStaticConditionIDDown, hStaticCurrUnitAddStep, hStaticCurrUnitEditStep;
	HWND hComboConditionUp, hComboConditionDown, hComboComparatorUp, hComboComparatorDown, hEditThresholdUp, hEditThresholdDown;
	HWND hComboRelationShip, hButtonAddUp, hButtonDeleteUp, hButtonAddDown, hButtonDeleteDown;
	HWND hEditVoltage, hEditCurrent, hEditTemperature, hEditMinTime, hEditJumpStepNumber, hStaticThresholdUnitUp, hStaticThresholdUnitDown;
}CONTROL_HANDLE_SET;

BOOL WINAPI AddStepToSchedule(HWND, UINT, WPARAM, LPARAM);
void EditStep(HWND, TEST_STEP **);
void InitAddStepDialog(HWND, CONTROL_HANDLE_SET *, TEST_SCHEDULE_PTR);
void InitTestSchedule(TEST_SCHEDULE_PTR *);
TEST_STEP *LocateStepWithTestNumber(TEST_SCHEDULE_PTR, UINT);
UINT FindMaxStepNumber(TEST_SCHEDULE_PTR pTestSchedule);
void UpdateStepDialog(CONTROL_HANDLE_SET, TEST_STEP *, TEST_SCHEDULE_PTR *, WPARAM ReasonToChange);
void UpdateTestStep(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep, DWORD ReasonToChange);
void InitCondition(TERM_COND **ppTermCondition);
BOOL InitTestStep(TEST_STEP **ppTestStep, UINT uiLastStepIndex);
UINT GetMaxConditionID(TERM_COND *pTermCondition);
void TraverseConditionTree(TERM_COND *pTermCond, TERM_COND **ppTermCond, UINT *pIndex);
UINT GetAllConditionPointer(TERM_COND *pTermCondition);
void AddBifurcationToCondtionTree(TERM_COND **ppTermCondition, TEST_STEP *pTestStep);
void TraverseConditionTreeForMaxID(TERM_COND *pTermCond, UINT *uiMaxID);
void SetNumericalComboRange(HWND hCombo, UINT uiItemCount, UINT uiBase);
TERM_COND *GetConditionPointerWithID(TERM_COND *pTopCondition, UINT uiConditionID);
void TraverseConditionTreeForMatchedID(TERM_COND *pTermCond, UINT uiConditionID, TERM_COND **ppMatchedTermCondition);
void UpdateConditionDisplay(CONTROL_HANDLE_SET HandleSet, TERM_COND *pTermCondition, WPARAM UpdateReason);
void UpdateTestOperationDisplay(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep, UINT UpdateReason);
void RecursiveDeleteCondition(TERM_COND **ppTermCondition);
HRESULT ReleaseConditionMem(TERM_COND **ppTermCondition);
void UpdateConditionIDCombo(HWND hCombo, TERM_COND *pTermCondition);
UINT GetIDFromConditionCombo(HWND hCombo);
BOOL AddConditionToTree(CONTROL_HANDLE_SET, TEST_STEP *, UINT ReasonToChange);
BOOL DeleteConditionFromTree(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep, UINT ReasonToChange);
UINT GetMinimumAvailableConditionID(TERM_COND *pTermCondition);
HRESULT CheckConditionTree(TERM_COND *pConditionTree);
HRESULT CheckSingleCondition(TERM_COND *pCondition, CONDITION_NODE_POSITION enPosition);
HRESULT RecursiveCheckConditionTree(TERM_COND *pCondtion, CONDITION_NODE_POSITION enPosition);
HRESULT CheckAndSaveOperationInfo(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep);
void RestoreWindowText(HWND hEdit);
void AddTestStepToScheduleTree(TEST_SCHEDULE_PTR pTestSchedule, TEST_STEP *pTestStep);
