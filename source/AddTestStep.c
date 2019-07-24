#include <Windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <math.h>
#include "resource.h"
#include "TempChamberDriver.h"
#include "BatCycleController.h"
#include "BasicFunction.h"
#include "AddTestStep.h"
#include "DeleteTestStep.h"
#include "SetTestPreference.h"

extern char *szOperation[];
extern char *szConditionType[];
extern char *szComparasonType[];
extern char *szLogicalRel[];
extern TEST_PREFERENCE TestPreference;

BOOL WINAPI AddStepToSchedule(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static TEST_STEP *pTestScheduleTree = NULL, *pTestStep = NULL;
	static HWND hTestScheduleListView = NULL, hParentDialog = NULL;
	static CONTROL_HANDLE_SET HandleSet = { 0 };
	static UINT uiStepIndex = 0;
	char szTempStr[MAX_STRING_LENGTH] = { 0 };
	HRESULT Result = 0;

	switch (uMsg)
	{
	case WM_INITDIALOG:

		pTestScheduleTree = *(TEST_SCHEDULE_PTR *)lParam;
		if (NULL == pTestScheduleTree)
		{
			MessageBox(NULL, "Test Schedule Not Initialised", NULL, MB_OK);
		}

		hParentDialog = GetParent(hWnd);
		hTestScheduleListView = GetDlgItem(hParentDialog, IDC_TEST_SCHEDULE_LIST);
		uiStepIndex = ListView_GetSelectionMark(hTestScheduleListView);

		if (-1 == uiStepIndex)
			uiStepIndex = FindMaxStepNumber(pTestScheduleTree);

		InitTestStep(&pTestStep, uiStepIndex);		//set the test step number to the end of the test step list or the highlighted step in the list view.

		InitAddStepDialog(hWnd, &HandleSet, pTestScheduleTree);

		ComboBox_SetCurSel(HandleSet.hComboStepNumber, pTestStep->uiStepNumber - 1);
		ComboBox_SetCurSel(HandleSet.hComboOperationType, pTestStep->StepOperation);

		UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleTree, (WPARAM)WM_INITDIALOG);
		//PostMessage(hWnd, WM_UPDATE_STEP_DIALOG, WM_INITDIALOG, 0);
		break;

	case WM_UPDATE_STEP_DIALOG:
		UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleTree, (WPARAM)wParam);
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case CBN_STEP_NUMBER_SELECTED:
			UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleTree, CBN_STEP_NUMBER_SELECTED);
			//PostMessage(hWnd, WM_UPDATE_STEP_DIALOG, CBN_STEP_NUMBER_SELECTED, 0);
			break;
		case CBN_STEP_OPERATION_SELECTED:
			UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleTree, CBN_STEP_OPERATION_SELECTED);
			//PostMessage(hWnd, WM_UPDATE_STEP_DIALOG, CBN_STEP_OPERATION_SELECTED, 0);
			break;
		case CBN_CONDITION_ID_CHANGE:
			UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleTree, CBN_CONDITION_ID_CHANGE);
			//PostMessage(hWnd, WM_UPDATE_STEP_DIALOG, CBN_CONDITION_ID_CHANGE, 0);
			break;
		case CBN_UP_CONDITION_TYPE_CHANGE:
			UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleTree, CBN_UP_CONDITION_TYPE_CHANGE);
			break;
		case CBN_DOWN_CONDITION_TYPE_CHANGE:
			UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleTree, CBN_DOWN_CONDITION_TYPE_CHANGE);
			break;
		case IDC_BUTTON_ADD_CONDITION_UP:
			AddConditionToTree(HandleSet, pTestStep, IDC_BUTTON_ADD_CONDITION_UP);
			PostMessage(hWnd, WM_UPDATE_STEP_DIALOG, IDC_BUTTON_ADD_CONDITION_UP, 0);
			break;
		case IDC_BUTTON_ADD_CONDITION_DOWN:
			AddConditionToTree(HandleSet, pTestStep, IDC_BUTTON_ADD_CONDITION_DOWN);
			PostMessage(hWnd, WM_UPDATE_STEP_DIALOG, IDC_BUTTON_ADD_CONDITION_DOWN, 0);
			break;
		case IDC_BUTTON_DELETE_CONDITION_UP:
			DeleteConditionFromTree(HandleSet, pTestStep, IDC_BUTTON_DELETE_CONDITION_UP);
			PostMessage(hWnd, WM_UPDATE_STEP_DIALOG, IDC_BUTTON_DELETE_CONDITION_UP, 0);
			break;
		case IDC_BUTTON_DELETE_CONDITION_DOWN:
			DeleteConditionFromTree(HandleSet, pTestStep, IDC_BUTTON_DELETE_CONDITION_DOWN);
			PostMessage(hWnd, WM_UPDATE_STEP_DIALOG, IDC_BUTTON_DELETE_CONDITION_DOWN, 0);
			break;
		case IDOK:
			Result = CheckConditionTree(pTestStep->pTerminateCondition);
			Result |= CheckAndSaveOperationInfo(HandleSet, pTestStep);

			if (ERROR_SUCCESS == Result)
			{
				AddTestStepToScheduleTree(pTestScheduleTree, pTestStep);
				EndDialog(hWnd, ERROR_SUCCESS);
			}
			else
			{
				MessageBox(NULL, "Condition Error, Please Check!", NULL, MB_OK);
			}
			break;
		case IDCANCEL:					//this message is equivalent to WM_CLOSE
			if (pTestStep->pTerminateCondition != NULL)
				Result = ReleaseConditionMem(&(pTestStep->pTerminateCondition));

			if (ERROR != ERROR_SUCCESS)
			{
				MessageBox(NULL, "Memory Release for Condition failed", NULL, MB_OK);
			}
			else
			{
				free(pTestStep);
				pTestStep = NULL;
			}
			EndDialog(hWnd, ERROR_FAIL);
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}

	return FALSE;
}

void InitTestSchedule(TEST_SCHEDULE_PTR *ppTestSchedule)			//第0个只是起点，没有测试内容
{
	TEST_SCHEDULE_PTR pTestSchedule = NULL;
	
	pTestSchedule = *ppTestSchedule;

	if (NULL == pTestSchedule)
	{

		pTestSchedule = (TEST_SCHEDULE_PTR)Malloc(sizeof(TEST_STEP));

		memset(pTestSchedule, 0, sizeof(TEST_STEP));

		pTestSchedule->StepOperation = NOP;
		pTestSchedule->uiStepNumber = 0;
		pTestSchedule->pNextStep = NULL;
	}

	*ppTestSchedule = pTestSchedule;
}

void InitAddStepDialog(HWND hWnd, CONTROL_HANDLE_SET *pHandleSet, TEST_SCHEDULE_PTR pTestSchedule)
{
	HWND hParentDialog = NULL, hTestScheduleListView = NULL;
	UINT uiItemCount = 0, uiStepIndex = 0;
	char szComboString[10] = { 0 };
	TEST_STEP *pTestStep = NULL;
	COND_TYPE enConditionType;
	OPERATION enOperation;
	COMP_TYPE enCompType;

	if (NULL == pHandleSet)
		return;

	pHandleSet->hComboStepNumber = GetDlgItem(hWnd, IDC_COMBO_STEP_NUMBER);
	pTestStep = pTestSchedule;

	while (NULL != pTestStep)
	{
		if (pTestStep->uiStepNumber)
		{
			sprintf(szComboString, "%d", pTestStep->uiStepNumber);
			ComboBox_InsertString(pHandleSet->hComboStepNumber, -1, szComboString);
			memset(szComboString, 0, strlen(szComboString));
		}
		pTestStep = pTestStep->pNextStep;
	}

	uiItemCount = ComboBox_GetCount(pHandleSet->hComboStepNumber);
	sprintf(szComboString, "%d", uiItemCount + 1);

	ComboBox_InsertString(pHandleSet->hComboStepNumber, -1, szComboString);
	/*
	hParentDialog = GetParent(hWnd);
	hTestScheduleListView = GetDlgItem(hParentDialog, IDC_TEST_SCHEDULE_LIST);
	uiStepIndex = ListView_GetSelectionMark(hTestScheduleListView);

	if (-1 == uiStepIndex)
		uiStepIndex = uiItemCount;

	ComboBox_SetCurSel(pHandleSet->hComboStepNumber, uiStepIndex);
	*/
	pHandleSet->hComboOperationType = GetDlgItem(hWnd, IDC_COMBO_OPERATION_TYPE);
	for (enOperation = NOP; enOperation <= SET_TEMP; enOperation++)
	{
		SendMessage(pHandleSet->hComboOperationType, CB_INSERTSTRING, -1, (LPARAM)szOperation[enOperation]);
	}
	SendMessage(pHandleSet->hComboOperationType, CB_SETCURSEL, (WPARAM)NOP, 0);
	
	pHandleSet->hEditVoltage = GetDlgItem(hWnd, IDC_EDIT_VOLT_ADD_STEP);
	pHandleSet->hEditCurrent = GetDlgItem(hWnd, IDC_EDIT_CURR_ADD_STEP);
	pHandleSet->hEditTemperature = GetDlgItem(hWnd, IDC_EDIT_SET_TEMP);
	pHandleSet->hEditMinTime = GetDlgItem(hWnd, IDC_EDIT_MIN_TIME);
	pHandleSet->hStaticCurrUnitAddStep = GetDlgItem(hWnd, IDC_STATIC_CURR_ADD_STEP);
	SetWindowText(pHandleSet->hStaticCurrUnitAddStep, GetLoadUnit(&TestPreference));
	pHandleSet->hEditJumpStepNumber = GetDlgItem(hWnd, IDC_EDIT_JUMP_STEP_NUMBER);

	pHandleSet->hComboConditonID = GetDlgItem(hWnd, IDC_COMBO_CONDITION_ID);
	SetNumericalComboRange(pHandleSet->hComboConditonID, 0, 0);
	EnableWindow(pHandleSet->hComboConditonID, TRUE);

	pHandleSet->hStaticConditionIDUp = GetDlgItem(hWnd, IDC_STATIC_CONDITON_ID_UP);

	pHandleSet->hComboConditionUp = GetDlgItem(hWnd, IDC_COMBO_CONDITION_TYPE_UP);
	for (enConditionType = Bifurcation; enConditionType < ConditionTime; enConditionType++)
	{
		SendMessage(pHandleSet->hComboConditionUp, CB_INSERTSTRING, -1, (LPARAM)szConditionType[enConditionType]);
	}
	SendMessage(pHandleSet->hComboConditionUp, CB_SETCURSEL, -1, 0);

	pHandleSet->hComboComparatorUp = GetDlgItem(hWnd, IDC_COMBO_COMPARATOR_UP);
	for (enCompType = NOCOMP; enCompType <= NoLessThan; enCompType++)
	{
		SendMessage(pHandleSet->hComboComparatorUp, CB_INSERTSTRING, -1, (LPARAM)szComparasonType[enCompType]);
	}
	SendMessage(pHandleSet->hComboComparatorUp, CB_SETCURSEL, -1, 0);

	pHandleSet->hEditThresholdUp = GetDlgItem(hWnd, IDC_EDIT_CONDITION_THRESHOLD_UP);

	pHandleSet->hStaticThresholdUnitUp = GetDlgItem(hWnd, IDC_STATIC_THRESHOLD_UNIT_UP);

	pHandleSet->hButtonAddUp = GetDlgItem(hWnd, IDC_BUTTON_ADD_CONDITION_UP);
	pHandleSet->hButtonDeleteUp = GetDlgItem(hWnd, IDC_BUTTON_DELETE_CONDITION_UP);

	pHandleSet->hComboRelationShip = GetDlgItem(hWnd, IDC_COMBO_RELATIONSHIP);
	SendMessage(pHandleSet->hComboRelationShip, CB_INSERTSTRING, -1, (LPARAM)szLogicalRel[AND]);
	SendMessage(pHandleSet->hComboRelationShip, CB_INSERTSTRING, -1, (LPARAM)szLogicalRel[OR]);

	pHandleSet->hStaticConditionIDDown = GetDlgItem(hWnd, IDC_STATIC_CONDITON_ID_DOWN);

	pHandleSet->hComboConditionDown = GetDlgItem(hWnd, IDC_COMBO_CONDITION_TYPE_DOWN);
	for (enConditionType = Bifurcation; enConditionType <= ConditionTime; enConditionType++)
	{
		SendMessage(pHandleSet->hComboConditionDown, CB_INSERTSTRING, -1, (LPARAM)szConditionType[enConditionType]);
	}
	SendMessage(pHandleSet->hComboConditionDown, CB_SETCURSEL, -1, 0);

	pHandleSet->hComboComparatorDown = GetDlgItem(hWnd, IDC_COMBO_COMPARATOR_DOWN);
	for (enCompType = NOCOMP; enCompType <= NoLessThan; enCompType++)
	{
		SendMessage(pHandleSet->hComboComparatorDown, CB_INSERTSTRING, -1, (LPARAM)szComparasonType[enCompType]);
	}
	SendMessage(pHandleSet->hComboComparatorDown, CB_SETCURSEL, -1, 0);

	pHandleSet->hEditThresholdDown = GetDlgItem(hWnd, IDC_EDIT_CONDITION_THRESHOLD_DOWN);
	pHandleSet->hButtonAddDown = GetDlgItem(hWnd, IDC_BUTTON_ADD_CONDITION_DOWN);
	pHandleSet->hButtonDeleteDown = GetDlgItem(hWnd, IDC_BUTTON_DELETE_CONDITION_DOWN);

	pHandleSet->hStaticThresholdUnitDown = GetDlgItem(hWnd, IDC_STATIC_THRESHOLD_UNIT_DOWN);

	SetWindowText(pHandleSet->hStaticConditionIDUp, "");
	ComboBox_SetCurSel(pHandleSet->hComboConditionUp, (UINT)Bifurcation);
	ComboBox_SetCurSel(pHandleSet->hComboComparatorUp, (UINT)NOCOMP);
	SetWindowText(pHandleSet->hEditThresholdUp, "");
	SetWindowText(pHandleSet->hStaticThresholdUnitUp, "");
	ComboBox_SetCurSel(pHandleSet->hComboRelationShip, (UINT)AND);
	EnableWindow(pHandleSet->hButtonAddUp, TRUE);
	EnableWindow(pHandleSet->hButtonDeleteUp, FALSE);

	SetWindowText(pHandleSet->hStaticConditionIDDown, "");
	ComboBox_SetCurSel(pHandleSet->hComboConditionDown, (UINT)Bifurcation);
	ComboBox_SetCurSel(pHandleSet->hComboComparatorDown, (UINT)NOCOMP);
	SetWindowText(pHandleSet->hEditThresholdDown, "");
	SetWindowText(pHandleSet->hStaticThresholdUnitDown, "");
	EnableWindow(pHandleSet->hButtonAddDown, TRUE);
	EnableWindow(pHandleSet->hButtonDeleteDown, FALSE);

}

TEST_STEP *LocateStepWithTestNumber(TEST_SCHEDULE_PTR pTestSchedule, UINT uiStepIndex)
{
	TEST_STEP *pTestStep = NULL;

	if (NULL == pTestSchedule || 0 == uiStepIndex)		//return NULL if the test schedule is not initialised or initialised but is empty
		return NULL;

	pTestStep = pTestSchedule;

	while (pTestStep->uiStepNumber != uiStepIndex && pTestStep->pNextStep != NULL)
	{
		pTestStep = pTestStep->pNextStep;
	}

	if (uiStepIndex == pTestStep->uiStepNumber)
		return pTestStep;
	else
		return NULL;
}

UINT FindMaxStepNumber(TEST_SCHEDULE_PTR pTestSchedule)
{
	TEST_STEP *pTestStep = NULL;
	UINT uiMaxStepNumber = 0;

	pTestStep = pTestSchedule;

	if (NULL == pTestStep)
		return 0;

	while (NULL != pTestStep->pNextStep)
	{
		pTestStep = pTestStep->pNextStep;
	}

	return pTestStep->uiStepNumber;
}

void UpdateStepDialog(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep, TEST_SCHEDULE_PTR *ppTestSchedule, WPARAM ReasonToChange)
{
	char szTempString[MAX_STRING_LENGTH] = { 0 };
	TERM_COND *pTermCondition = NULL;

	if (NULL == pTestStep || pTestStep->uiStepNumber == 0)
		return;

	UpdateTestOperationDisplay(HandleSet, pTestStep, ReasonToChange);

	UpdateConditionDisplay(HandleSet, pTestStep->pTerminateCondition, ReasonToChange);
}

void InitCondition(TERM_COND **ppTermCondition)
{
	TERM_COND *pTermCondition = NULL;

	pTermCondition = *ppTermCondition;

	if (NULL != pTermCondition)
		return;

	pTermCondition = (TERM_COND *)Malloc(sizeof(TERM_COND));

	memset(pTermCondition, 0, sizeof(TERM_COND));

	pTermCondition->uiConditionID = 0;
	pTermCondition->ConditionType = Bifurcation;
	pTermCondition->pLeftCondition = NULL;
	pTermCondition->pRightCondition = NULL;
	pTermCondition->RelationShip = AND;

	*ppTermCondition = pTermCondition;
}

void AddBifurcationToCondtionTree(TERM_COND **ppTermCondition, TEST_STEP *pTestStep)
{
	TERM_COND *pTermCondition = NULL;
	UINT uiConditionID;

	pTermCondition = *ppTermCondition;

	if (NULL != pTermCondition)
		return;

	pTermCondition = (TERM_COND *)Malloc(sizeof(TERM_COND));

	memset(pTermCondition, 0, sizeof(TERM_COND));

	uiConditionID = GetMaxConditionID(pTestStep->pTerminateCondition);
	pTermCondition->uiConditionID = uiConditionID + 1;
	pTermCondition->ConditionType = Bifurcation;
	pTermCondition->pLeftCondition = NULL;
	pTermCondition->pRightCondition = NULL;
	pTermCondition->RelationShip = AND;

	*ppTermCondition = pTermCondition;
}

void UpdateTestStep(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep, DWORD ReasonToChange)
{
	UINT uiTempVar = 0;
	TERM_COND *pTermCondition = NULL;

	switch (ReasonToChange)
	{
	case CBN_STEP_NUMBER_SELECTED:
		pTestStep->uiStepNumber = ComboBox_GetCurSel(HandleSet.hComboStepNumber) + 1;
		break;
	case CBN_STEP_OPERATION_SELECTED:
		uiTempVar = ComboBox_GetCurSel(HandleSet.hComboOperationType);
		pTestStep->StepOperation = (OPERATION)uiTempVar;
		break;
	default:
		break;
	}
}

BOOL InitTestStep(TEST_STEP **ppTestStep, UINT uiLastStepIndex)
{
	TEST_STEP *pTestStep = NULL;

	
	pTestStep = *ppTestStep;

	pTestStep = (TEST_STEP *)Malloc(sizeof(TEST_STEP));   //init the new step data
	if (NULL == pTestStep)
		return FALSE;

	memset(pTestStep, 0, sizeof(TEST_STEP));

	pTestStep->uiStepNumber = uiLastStepIndex + 1;
	pTestStep->pNextStep = NULL;
	pTestStep->pTerminateCondition = NULL;
	InitCondition(&(pTestStep->pTerminateCondition));

	pTestStep->StepOperation = NOP;

	*ppTestStep = pTestStep;

	return TRUE;
}

UINT GetAllConditionPointer(TERM_COND *pTermCondition)
{
	TERM_COND *ppTermCond[20] = { 0 };
	UINT ConditionIndex = 0;

	TraverseConditionTree(pTermCondition, ppTermCond, &ConditionIndex);
	
	return ConditionIndex;
}

UINT GetMaxConditionID(TERM_COND *pTermCondition)
{
	UINT uiMaxID = 0;

	TraverseConditionTreeForMaxID(pTermCondition, &uiMaxID);

	return uiMaxID;
}

UINT GetMinimumAvailableConditionID(TERM_COND *pTermCondition)
{
	UINT uiMaxID = 0, i;
	TERM_COND *pTermCond = NULL;

	uiMaxID = GetMaxConditionID(pTermCondition);

	for (i = 0; i <= uiMaxID; i++)
	{
		pTermCond = NULL;
		TraverseConditionTreeForMatchedID(pTermCondition, i, &pTermCond);

		if (NULL == pTermCond)
			break;
	}

	return i;
}

void TraverseConditionTreeForMaxID(TERM_COND *pTermCond, UINT *uiMaxID)
{
	if (pTermCond->uiConditionID > *uiMaxID)
	{
		*uiMaxID = pTermCond->uiConditionID;
	}

	if (pTermCond->pLeftCondition)
		TraverseConditionTreeForMaxID(pTermCond->pLeftCondition, uiMaxID);


	if (pTermCond->pRightCondition)
		TraverseConditionTreeForMaxID(pTermCond->pRightCondition, uiMaxID);

	return;
}

void TraverseConditionTree(TERM_COND *pTermCond, TERM_COND **ppTermCond, UINT *pIndex)
{
	if (pTermCond && ppTermCond)
	{
		ppTermCond[*pIndex] = pTermCond;
		(*pIndex)++;
	}

	if (pTermCond->pLeftCondition)
		TraverseConditionTree(pTermCond->pLeftCondition, ppTermCond, pIndex);


	if (pTermCond->pRightCondition)
		TraverseConditionTree(pTermCond->pRightCondition, ppTermCond, pIndex);
	
		return;
}

void SetNumericalComboRange(HWND hCombo, UINT uiItemCount, UINT uiBase)
{
	char szComboString[MAX_STRING_LENGTH] = { 0 };
	UINT i;

	if (uiBase > uiItemCount)
		return;

	SendMessage(hCombo, CB_RESETCONTENT, 0, 0);

	for (i = uiBase; i < uiItemCount + 1; i++)
	{
		sprintf(szComboString, "%d", i);
		SendMessage(hCombo, CB_INSERTSTRING, -1, (LPARAM)szComboString);
	}
}

void UpdateConditionIDCombo(HWND hCombo, TERM_COND *pTermCondition)
{
	char szCombString[MAX_STRING_LENGTH] = { 0 };
	UINT uiMaxConditionNumber = 0, i;
	TERM_COND *pTemp;

	uiMaxConditionNumber = GetMaxConditionID(pTermCondition);

	for (i = 0; i <= uiMaxConditionNumber; i++)
	{
		pTemp = GetConditionPointerWithID(pTermCondition, i);
		if (NULL == pTemp)
			continue;
		sprintf(szCombString, "%d", pTemp->uiConditionID);
		ComboBox_AddString(hCombo, szCombString);
	}
}

UINT GetIDFromConditionCombo(HWND hCombo)
{
	char szTempStr[MAX_STRING_LENGTH] = { 0 };
	INT uiConditionID = ERROR_FAIL, uiTemp;

	uiTemp = ComboBox_GetCurSel(hCombo);
	if (CB_ERR == uiTemp)
		uiTemp = 0;

	ComboBox_GetLBText(hCombo, uiTemp, szTempStr);
	sscanf(szTempStr, "%d", &uiConditionID);
	return uiConditionID;
}

TERM_COND *GetConditionPointerWithID(TERM_COND *pConditionTree, UINT uiConditionID)
{
	TERM_COND *pMatchedTermCondition = NULL;

	TraverseConditionTreeForMatchedID(pConditionTree, uiConditionID, &pMatchedTermCondition);

	return pMatchedTermCondition;
}

void TraverseConditionTreeForMatchedID(TERM_COND *pTermCond, UINT uiConditionID, TERM_COND **ppMatchedTermCondition)
{
	if (pTermCond->uiConditionID == uiConditionID)
	{
		*ppMatchedTermCondition = pTermCond;
		return;
	}

	if (pTermCond->pLeftCondition)
		TraverseConditionTreeForMatchedID(pTermCond->pLeftCondition, uiConditionID, ppMatchedTermCondition);


	if (pTermCond->pRightCondition)
		TraverseConditionTreeForMatchedID(pTermCond->pRightCondition, uiConditionID, ppMatchedTermCondition);

	return;
}

void UpdateConditionDisplay(CONTROL_HANDLE_SET HandleSet, TERM_COND *pTermCondition, WPARAM UpdateReason)
{
	UINT uiComboCurSel = 0, uiConditionID = 0;
	char szTempString[MAX_STRING_LENGTH] = { 0 };
	COND_TYPE ConditionType = Voltage;
	if (CBN_STEP_NUMBER_SELECTED != UpdateReason)											//不是由于步骤号变化引起的更新
	{
		uiComboCurSel = ComboBox_GetCurSel(HandleSet.hComboConditonID);
		ComboBox_GetLBText(HandleSet.hComboConditonID, uiComboCurSel, szTempString);		//读出当前的ID号，保存在szTempString中

		ComboBox_ResetContent(HandleSet.hComboConditonID);
		UpdateConditionIDCombo(HandleSet.hComboConditonID, pTermCondition);					//用当前的条件树更新条件编号显示

		sscanf(szTempString, "%d", &uiConditionID);
		uiComboCurSel = ComboBox_FindString(HandleSet.hComboConditonID, 0, szTempString);  //找出原来的ID号对应的序号
		if (CB_ERR == uiComboCurSel)
			uiComboCurSel = 0;
	}
	else
	{
		ComboBox_ResetContent(HandleSet.hComboConditonID);
		UpdateConditionIDCombo(HandleSet.hComboConditonID, pTermCondition);
		uiComboCurSel = uiConditionID = 0;
	}

	ComboBox_SetCurSel(HandleSet.hComboConditonID, uiComboCurSel);							//用条件序号更新条件ID
	
	pTermCondition = GetConditionPointerWithID(pTermCondition, uiConditionID);

	if (CBN_CONDITION_ID_CHANGE == UpdateReason \
		|| CBN_UP_CONDITION_TYPE_CHANGE == UpdateReason \
		|| IDC_BUTTON_ADD_CONDITION_UP == UpdateReason\
		|| IDC_BUTTON_DELETE_CONDITION_UP == UpdateReason\
		|| DLGN_ADD_STEP_INIT == UpdateReason\
		|| DLGN_EDIT_STEP_INIT == UpdateReason\
		|| CBN_STEP_NUMBER_SELECTED == UpdateReason)
	{
		if (NULL == pTermCondition->pLeftCondition)
		{
			if (CBN_CONDITION_ID_CHANGE == UpdateReason || IDC_BUTTON_DELETE_CONDITION_UP == UpdateReason || CBN_STEP_NUMBER_SELECTED == UpdateReason)
				ComboBox_SetCurSel(HandleSet.hComboConditionUp, Bifurcation);

			ConditionType = (OPERATION)ComboBox_GetCurSel(HandleSet.hComboConditionUp);
			SetWindowText(HandleSet.hStaticConditionIDUp, "");
			EnableWindow(HandleSet.hComboConditionUp, TRUE);
			EnableWindow(HandleSet.hButtonAddUp, (Bifurcation == pTermCondition->ConditionType));
			EnableWindow(HandleSet.hButtonDeleteUp, FALSE);
			switch (ConditionType)
			{
			case Voltage:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "mV");
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case Current:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, GetLoadUnit(&TestPreference));
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case Temperature:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "°C");
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case AvgVoltage:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "mV");
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case AvgCurrent:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, GetLoadUnit(&TestPreference));
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case VoltageRate:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "μV/S");
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case CurrentRate:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "μA/S");
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case AllTime:
			case ConditionTime:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "S");
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case Bifurcation:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "");
				EnableWindow(HandleSet.hComboComparatorUp, FALSE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, FALSE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case PassedCharge:
				if (C_Rate == TestPreference.lUnit)
					SetWindowText(HandleSet.hStaticThresholdUnitUp, "DC");
				else
					SetWindowText(HandleSet.hStaticThresholdUnitUp, "mAh");
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			case Counter:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "");
				EnableWindow(HandleSet.hComboComparatorUp, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorUp, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdUp, TRUE);
				SetWindowText(HandleSet.hEditThresholdUp, "");
				break;
			default:
				break;
			}
		}
		else
		{
			sprintf(szTempString, "%d", pTermCondition->pLeftCondition->uiConditionID);
			SetWindowText(HandleSet.hStaticConditionIDUp, szTempString);

			ComboBox_SetCurSel(HandleSet.hComboConditionUp, pTermCondition->pLeftCondition->ConditionType);
			ComboBox_SetCurSel(HandleSet.hComboComparatorUp, pTermCondition->pLeftCondition->ComparisonType);
			if (C_Rate == TestPreference.lUnit &&\
				(pTermCondition->pLeftCondition->ConditionType == Current \
					|| pTermCondition->pLeftCondition->ConditionType == AvgCurrent\
					|| pTermCondition->pLeftCondition->ConditionType == PassedCharge))
				sprintf(szTempString, "%5.3f", pTermCondition->pLeftCondition->iThreshold / (float)TestPreference.uBatteryCapaicty);
			else
				sprintf(szTempString, "%d", pTermCondition->pLeftCondition->iThreshold);
			SetWindowText(HandleSet.hEditThresholdUp, szTempString);

			EnableWindow(HandleSet.hComboConditionUp, FALSE);
			EnableWindow(HandleSet.hComboComparatorUp, FALSE);
			EnableWindow(HandleSet.hEditThresholdUp, FALSE);
			EnableWindow(HandleSet.hButtonAddUp, FALSE);
			EnableWindow(HandleSet.hButtonDeleteUp, TRUE);

			switch (pTermCondition->pLeftCondition->ConditionType)
			{
			case Voltage:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "mV");
				break;
			case Current:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, GetLoadUnit(&TestPreference));
				break;
			case Temperature:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "°C");
				break;
			case AvgVoltage:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "mV");
				break;
			case AvgCurrent:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, GetLoadUnit(&TestPreference));
				break;
			case VoltageRate:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "μV/S");
				break;
			case CurrentRate:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "μA/S");
				break;
			case AllTime:
			case ConditionTime:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "S");
				break;
			case Bifurcation:
				SetWindowText(HandleSet.hEditThresholdUp, "");
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "");
				break;
			case PassedCharge:
				if (C_Rate == TestPreference.lUnit)
					SetWindowText(HandleSet.hStaticThresholdUnitUp, "DC");
				else
					SetWindowText(HandleSet.hStaticThresholdUnitUp, "mAh");
				break;
			case Counter:
				SetWindowText(HandleSet.hStaticThresholdUnitUp, "");
				break;
			default:
				break;
			}
		}
	}
	
	if (CBN_CONDITION_ID_CHANGE == UpdateReason \
		|| CBN_DOWN_CONDITION_TYPE_CHANGE == UpdateReason \
		|| IDC_BUTTON_ADD_CONDITION_DOWN == UpdateReason\
		|| IDC_BUTTON_DELETE_CONDITION_DOWN == UpdateReason\
		|| DLGN_ADD_STEP_INIT == UpdateReason\
		|| DLGN_EDIT_STEP_INIT == UpdateReason\
		|| CBN_STEP_NUMBER_SELECTED == UpdateReason)
	{
		if (NULL == pTermCondition->pRightCondition)
		{
			if (CBN_CONDITION_ID_CHANGE == UpdateReason || IDC_BUTTON_DELETE_CONDITION_DOWN == UpdateReason || CBN_STEP_NUMBER_SELECTED == UpdateReason)
				ComboBox_SetCurSel(HandleSet.hComboConditionDown, Bifurcation);

			ConditionType = (OPERATION)ComboBox_GetCurSel(HandleSet.hComboConditionDown);
			SetWindowText(HandleSet.hStaticConditionIDDown, "");
			EnableWindow(HandleSet.hComboConditionDown, TRUE);
			EnableWindow(HandleSet.hButtonAddDown, (Bifurcation == pTermCondition->ConditionType));
			EnableWindow(HandleSet.hButtonDeleteDown, FALSE);

			switch (ConditionType)
			{
			case Voltage:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "mV");
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case Current:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, GetLoadUnit(&TestPreference));
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case Temperature:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "°C");
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case AvgVoltage:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "mV");
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case AvgCurrent:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, GetLoadUnit(&TestPreference));
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case VoltageRate:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "μV/S");
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case CurrentRate:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "μA/S");
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case AllTime:
			case ConditionTime:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "S");
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case Bifurcation:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "");
				EnableWindow(HandleSet.hComboComparatorDown, FALSE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, FALSE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case PassedCharge:
				if (C_Rate == TestPreference.lUnit)
					SetWindowText(HandleSet.hStaticThresholdUnitDown, "DC");
				else
					SetWindowText(HandleSet.hStaticThresholdUnitDown, "mAh");
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			case Counter:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "");
				EnableWindow(HandleSet.hComboComparatorDown, TRUE);
				ComboBox_SetCurSel(HandleSet.hComboComparatorDown, NOCOMP);
				EnableWindow(HandleSet.hEditThresholdDown, TRUE);
				SetWindowText(HandleSet.hEditThresholdDown, "");
				break;
			default:
				break;
			}
		}
		else
		{
			sprintf(szTempString, "%d", pTermCondition->pRightCondition->uiConditionID);
			SetWindowText(HandleSet.hStaticConditionIDDown, szTempString);

			ComboBox_SetCurSel(HandleSet.hComboConditionDown, pTermCondition->pRightCondition->ConditionType);
			ComboBox_SetCurSel(HandleSet.hComboComparatorDown, pTermCondition->pRightCondition->ComparisonType);

			if (C_Rate == TestPreference.lUnit &&\
				(pTermCondition->pRightCondition->ConditionType == Current \
					|| pTermCondition->pRightCondition->ConditionType == AvgCurrent \
					|| pTermCondition->pRightCondition->ConditionType == PassedCharge))
				sprintf(szTempString, "%5.3f", pTermCondition->pRightCondition->iThreshold / (float)TestPreference.uBatteryCapaicty);
			else
				sprintf(szTempString, "%d", pTermCondition->pRightCondition->iThreshold);

			SetWindowText(HandleSet.hEditThresholdDown, szTempString);

			EnableWindow(HandleSet.hComboConditionDown, FALSE);
			EnableWindow(HandleSet.hComboComparatorDown, FALSE);
			EnableWindow(HandleSet.hEditThresholdDown, FALSE);
			EnableWindow(HandleSet.hButtonAddDown, FALSE);
			EnableWindow(HandleSet.hButtonDeleteDown, TRUE);

			switch (pTermCondition->pRightCondition->ConditionType)
			{
			case Voltage:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "mV");
				break;
			case Current:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, GetLoadUnit(&TestPreference));
				break;
			case Temperature:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "°C");
				break;
			case AvgVoltage:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "mV");
				break;
			case AvgCurrent:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, GetLoadUnit(&TestPreference));
				break;
			case VoltageRate:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "μV/S");
				break;
			case CurrentRate:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "μA/S");
				break;
			case AllTime:
			case ConditionTime:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "S");
				break;
			case Bifurcation:
				SetWindowText(HandleSet.hEditThresholdDown, "");
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "");
				break;
			case PassedCharge:
				if (C_Rate == TestPreference.lUnit)
					SetWindowText(HandleSet.hStaticThresholdUnitUp, "DC");
				else
					SetWindowText(HandleSet.hStaticThresholdUnitUp, "mAh");
				break;
			case Counter:
				SetWindowText(HandleSet.hStaticThresholdUnitDown, "");
				break;
			default:
				break;
			}
		}
	}

	if(CBN_CONDITION_ID_CHANGE == UpdateReason || DLGN_EDIT_STEP_INIT == UpdateReason || CBN_STEP_NUMBER_SELECTED == UpdateReason)
		ComboBox_SetCurSel(HandleSet.hComboRelationShip, pTermCondition->RelationShip);

	if ((NULL != pTermCondition->pLeftCondition) && (NULL != pTermCondition->pRightCondition))
	{
		EnableWindow(HandleSet.hComboRelationShip, FALSE);
	}
	else
	{
		EnableWindow(HandleSet.hComboRelationShip, TRUE);
	}
}

void UpdateTestOperationDisplay(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep, UINT UpdateReason)
{
	OPERATION Operation;
	char szTempString[MAX_STRING_LENGTH] = { 0 };

	if (CBN_STEP_NUMBER_SELECTED == UpdateReason)
		pTestStep->uiStepNumber = ComboBox_GetCurSel(HandleSet.hComboStepNumber) + 1;

	Operation = ComboBox_GetCurSel(HandleSet.hComboOperationType);

	switch (Operation)
	{
		/*
		case NOP:
		EnableWindow(HandleSet.hEditVoltage, FALSE);
		EnableWindow(HandleSet.hEditCurrent, FALSE);
		EnableWindow(HandleSet.hEditTemperature, FALSE);
		EnableWindow(HandleSet.hEditMinTime, FALSE);
		EnableWindow(HandleSet.hEditJumpStepNumber, FALSE);
		SetWindowText(HandleSet.hEditVoltage, "");
		SetWindowText(HandleSet.hEditCurrent, "");
		SetWindowText(HandleSet.hEditTemperature, "");
		SetWindowText(HandleSet.hEditMinTime, "");
		SetWindowText(HandleSet.hEditJumpStepNumber, "");
		break;

		*/
	case SET_TEMP:
		EnableWindow(HandleSet.hEditVoltage, FALSE);
		EnableWindow(HandleSet.hEditCurrent, FALSE);
		EnableWindow(HandleSet.hEditTemperature, TRUE);
		EnableWindow(HandleSet.hEditMinTime, TRUE);
		EnableWindow(HandleSet.hEditJumpStepNumber, FALSE);
		if (CBN_STEP_OPERATION_SELECTED == UpdateReason \
			|| CBN_STEP_NUMBER_SELECTED == UpdateReason \
			|| DLGN_EDIT_STEP_INIT == UpdateReason)
			if (SET_TEMP == pTestStep->StepOperation)
			{
				SetWindowText(HandleSet.hEditVoltage, "");
				SetWindowText(HandleSet.hEditCurrent, "");
				sprintf(szTempString, "%d", pTestStep->iTemperature);
				SetWindowText(HandleSet.hEditTemperature, szTempString);
				memset(szTempString, 0, strlen(szTempString));
				sprintf(szTempString, "%d", pTestStep->iMinTempTime);
				SetWindowText(HandleSet.hEditMinTime, szTempString);
				memset(szTempString, 0, strlen(szTempString));
				SetWindowText(HandleSet.hEditJumpStepNumber, "");
			}
			else
			{
				SetWindowText(HandleSet.hEditVoltage, "");
				SetWindowText(HandleSet.hEditCurrent, "");
				SetWindowText(HandleSet.hEditTemperature, "");
				SetWindowText(HandleSet.hEditMinTime, "");
				SetWindowText(HandleSet.hEditJumpStepNumber, "");
			}
		else
		{
			RestoreWindowText(HandleSet.hEditVoltage);
			RestoreWindowText(HandleSet.hEditCurrent);
			RestoreWindowText(HandleSet.hEditTemperature);
			RestoreWindowText(HandleSet.hEditMinTime);
			RestoreWindowText(HandleSet.hEditJumpStepNumber);
		}

		break;
	case CHARGE:
		EnableWindow(HandleSet.hEditVoltage, TRUE);
		EnableWindow(HandleSet.hEditCurrent, TRUE);
		EnableWindow(HandleSet.hEditTemperature, FALSE);
		EnableWindow(HandleSet.hEditMinTime, FALSE);
		EnableWindow(HandleSet.hEditJumpStepNumber, FALSE);
		if (CBN_STEP_OPERATION_SELECTED == UpdateReason \
			|| CBN_STEP_NUMBER_SELECTED == UpdateReason \
			|| DLGN_EDIT_STEP_INIT == UpdateReason)
			if (CHARGE == pTestStep->StepOperation)
			{
				sprintf(szTempString, "%d", pTestStep->iVoltage);
				SetWindowText(HandleSet.hEditVoltage, szTempString);
				memset(szTempString, 0, strlen(szTempString));

				if (C_Rate == TestPreference.lUnit)
					sprintf(szTempString, "%5.3lf", pTestStep->dblCurrent / (float)TestPreference.uBatteryCapaicty);
				else
					sprintf(szTempString, "%5.3lf", pTestStep->dblCurrent);

				SetWindowText(HandleSet.hEditCurrent, szTempString);
				memset(szTempString, 0, strlen(szTempString));

				SetWindowText(HandleSet.hEditTemperature, "");
				SetWindowText(HandleSet.hEditMinTime, "");

				SetWindowText(HandleSet.hEditJumpStepNumber, "");
			}
			else
			{
				SetWindowText(HandleSet.hEditVoltage, "");
				SetWindowText(HandleSet.hEditCurrent, "");
				SetWindowText(HandleSet.hEditTemperature, "");
				SetWindowText(HandleSet.hEditMinTime, "");
				SetWindowText(HandleSet.hEditJumpStepNumber, "");
			}
		else
		{
			RestoreWindowText(HandleSet.hEditVoltage);
			RestoreWindowText(HandleSet.hEditCurrent);
			RestoreWindowText(HandleSet.hEditTemperature);
			RestoreWindowText(HandleSet.hEditMinTime);
			RestoreWindowText(HandleSet.hEditJumpStepNumber);
		}
		break;
	case DISCHARGE:
		EnableWindow(HandleSet.hEditVoltage, FALSE);
		EnableWindow(HandleSet.hEditCurrent, TRUE);
		EnableWindow(HandleSet.hEditTemperature, FALSE);
		EnableWindow(HandleSet.hEditMinTime, FALSE);
		EnableWindow(HandleSet.hEditJumpStepNumber, FALSE);
		if (CBN_STEP_OPERATION_SELECTED == UpdateReason \
			|| CBN_STEP_NUMBER_SELECTED == UpdateReason\
			|| DLGN_EDIT_STEP_INIT == UpdateReason)
			if (DISCHARGE == pTestStep->StepOperation)
			{
				SetWindowText(HandleSet.hEditVoltage, "");

				if (C_Rate == TestPreference.lUnit)
					sprintf(szTempString, "%5.3lf", pTestStep->dblCurrent / (float)TestPreference.uBatteryCapaicty);
				else
					sprintf(szTempString, "%5.3lf", pTestStep->dblCurrent);

				SetWindowText(HandleSet.hEditCurrent, szTempString);
				memset(szTempString, 0, strlen(szTempString));

				SetWindowText(HandleSet.hEditTemperature, "");
				SetWindowText(HandleSet.hEditMinTime, "");

				SetWindowText(HandleSet.hEditJumpStepNumber, "");
			}
			else
			{
				SetWindowText(HandleSet.hEditVoltage, "");
				SetWindowText(HandleSet.hEditCurrent, "");
				SetWindowText(HandleSet.hEditTemperature, "");
				SetWindowText(HandleSet.hEditMinTime, "");
				SetWindowText(HandleSet.hEditJumpStepNumber, "");
			}
		else
		{
			RestoreWindowText(HandleSet.hEditVoltage);
			RestoreWindowText(HandleSet.hEditCurrent);
			RestoreWindowText(HandleSet.hEditTemperature);
			RestoreWindowText(HandleSet.hEditMinTime);
			RestoreWindowText(HandleSet.hEditJumpStepNumber);
		}
		break;
	case PASSEDCHARGE_OFF:
	case PASSEDCHARGE_ON:
	case PASSEDCHARGE_CLEAR:
	case START_LOG:
	case STOP_LOG:
	case CNTR_CLR:
	case CNTR_INC:
	case CNTR_DEC:
	case REST:
	case SHTDN_TCHMBR:
	case NOP:
		EnableWindow(HandleSet.hEditVoltage, FALSE);
		EnableWindow(HandleSet.hEditCurrent, FALSE);
		EnableWindow(HandleSet.hEditTemperature, FALSE);
		EnableWindow(HandleSet.hEditMinTime, FALSE);
		EnableWindow(HandleSet.hEditJumpStepNumber, FALSE);
		SetWindowText(HandleSet.hEditVoltage, "");
		SetWindowText(HandleSet.hEditCurrent, "");
		SetWindowText(HandleSet.hEditTemperature, "");
		SetWindowText(HandleSet.hEditMinTime, "");
		SetWindowText(HandleSet.hEditJumpStepNumber, "");
		break;
	case GOTO:
		EnableWindow(HandleSet.hEditVoltage, FALSE);
		EnableWindow(HandleSet.hEditCurrent, FALSE);
		EnableWindow(HandleSet.hEditTemperature, FALSE);
		EnableWindow(HandleSet.hEditMinTime, FALSE);
		EnableWindow(HandleSet.hEditJumpStepNumber, TRUE);
		if (CBN_STEP_OPERATION_SELECTED == UpdateReason \
			|| CBN_STEP_NUMBER_SELECTED == UpdateReason\
			||  DLGN_EDIT_STEP_INIT == UpdateReason)
			if (GOTO == pTestStep->StepOperation)
			{
				SetWindowText(HandleSet.hEditVoltage, "");
				SetWindowText(HandleSet.hEditCurrent, "");
				SetWindowText(HandleSet.hEditTemperature, "");
				SetWindowText(HandleSet.hEditMinTime, "");
				sprintf(szTempString, "%d", pTestStep->iJumpStepNumber);
				SetWindowText(HandleSet.hEditJumpStepNumber, szTempString);
				memset(szTempString, 0, strlen(szTempString));
			}
			else
			{
				SetWindowText(HandleSet.hEditVoltage, "");
				SetWindowText(HandleSet.hEditCurrent, "");
				SetWindowText(HandleSet.hEditTemperature, "");
				SetWindowText(HandleSet.hEditMinTime, "");
				SetWindowText(HandleSet.hEditJumpStepNumber, "");
			}
		else
		{
			RestoreWindowText(HandleSet.hEditVoltage);
			RestoreWindowText(HandleSet.hEditCurrent);
			RestoreWindowText(HandleSet.hEditTemperature);
			RestoreWindowText(HandleSet.hEditMinTime);
			RestoreWindowText(HandleSet.hEditJumpStepNumber);
		}
		break;
	default:
		break;
	}
}

HRESULT ReleaseConditionMem(TERM_COND **ppTermCondition)
{

	RecursiveDeleteCondition(ppTermCondition);

	if (NULL == *ppTermCondition)
		return ERROR_SUCCESS;
	else
		return ERROR_FAIL;
}

void RecursiveDeleteCondition(TERM_COND **ppTermCondition)
{
	if (NULL == *ppTermCondition)
		return;

	if ((*ppTermCondition)->pLeftCondition)
	{
		RecursiveDeleteCondition(&((*ppTermCondition)->pLeftCondition));
	}

	if ((*ppTermCondition)->pRightCondition)
	{
		RecursiveDeleteCondition(&((*ppTermCondition)->pRightCondition));
	}

	if ((NULL == (*ppTermCondition)->pLeftCondition) && (NULL == (*ppTermCondition)->pRightCondition))
	{
		free((void *)(*ppTermCondition));
		*ppTermCondition = NULL;
	}
}

BOOL AddConditionToTree(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep, UINT ReasonToChange)
{
	UINT uiConditionID, uiNextUsableConditionID = 0, uiTempVar;
	TERM_COND *pPresentTermCondition = NULL, *pNewTermCondition = NULL;
	char szTempString[MAX_STRING_LENGTH] = { 0 };

	if (NULL == pTestStep)
		return FALSE;

	uiConditionID = GetIDFromConditionCombo(HandleSet.hComboConditonID);
	pPresentTermCondition = GetConditionPointerWithID(pTestStep->pTerminateCondition, uiConditionID);

	if (NULL == pPresentTermCondition)
		return FALSE;

	pNewTermCondition = Malloc(sizeof(TERM_COND));
	if (NULL == pPresentTermCondition)
		return FALSE;

	memset(pNewTermCondition, 0, sizeof(TERM_COND));

	switch (ReasonToChange)
	{
	case IDC_BUTTON_ADD_CONDITION_UP:
		if (NULL != pPresentTermCondition->pLeftCondition)
			return FALSE;

		pPresentTermCondition->RelationShip = ComboBox_GetCurSel(HandleSet.hComboRelationShip);
		uiNextUsableConditionID = GetMinimumAvailableConditionID(pTestStep->pTerminateCondition);

		pNewTermCondition->uiConditionID = uiNextUsableConditionID;
		pNewTermCondition->ConditionType = ComboBox_GetCurSel(HandleSet.hComboConditionUp);
		pNewTermCondition->ComparisonType = ComboBox_GetCurSel(HandleSet.hComboComparatorUp);
		pNewTermCondition->pLeftCondition = pNewTermCondition->pRightCondition = NULL;
		GetWindowText(HandleSet.hEditThresholdUp, szTempString, MAX_STRING_LENGTH);
		if (szTempString[0] != 0)
		{
			if (TestPreference.lUnit == C_Rate && \
				(pNewTermCondition->ConditionType == Current || pNewTermCondition->ConditionType == AvgCurrent || pNewTermCondition->ConditionType == PassedCharge))
			{
				if (!IsFraction(szTempString))
				{
					float fTempVar;
					if (!IsFloat(szTempString))
					{
						MessageBox(NULL, "The value of current should be a float", NULL, MB_OK);
						fTempVar = 0;
					}
					else
					{
						sscanf(szTempString, "%f", &fTempVar);
						uiTempVar = (UINT)(fTempVar * TestPreference.uBatteryCapaicty);
					}
				}
				else
				{
					INT iNumerator, iDenumerator;
					sscanf(szTempString, "%d/%d", &iNumerator, &iDenumerator);
					uiTempVar = (UINT)((float)iNumerator / iDenumerator * TestPreference.uBatteryCapaicty);
				}
			}
			else
			{
				if (!IsNumerical(szTempString))							//the imput current should be at minimum resolution of 1mA
				{
					MessageBox(NULL, "The current for unit mA should be Integer", NULL, MB_OK);
				}
				else
				{
					sscanf(szTempString, "%d", &uiTempVar);
				}
			}
		}
		else
			uiTempVar = 0;

		pNewTermCondition->iThreshold = uiTempVar;

		pPresentTermCondition->pLeftCondition = pNewTermCondition;
		break;
	case IDC_BUTTON_ADD_CONDITION_DOWN:
		if (NULL != pPresentTermCondition->pRightCondition)
			return FALSE;

		pPresentTermCondition->RelationShip = ComboBox_GetCurSel(HandleSet.hComboRelationShip);
		uiNextUsableConditionID = GetMinimumAvailableConditionID(pTestStep->pTerminateCondition);

		pNewTermCondition->uiConditionID = uiNextUsableConditionID;
		pNewTermCondition->ConditionType = ComboBox_GetCurSel(HandleSet.hComboConditionDown);
		pNewTermCondition->ComparisonType = ComboBox_GetCurSel(HandleSet.hComboComparatorDown);
		pNewTermCondition->pLeftCondition = pNewTermCondition->pRightCondition = NULL;
		GetWindowText(HandleSet.hEditThresholdDown, szTempString, MAX_STRING_LENGTH);
		if (szTempString[0] != 0)
		{
			if (TestPreference.lUnit == C_Rate && \
				(pNewTermCondition->ConditionType == Current || pNewTermCondition->ConditionType == AvgCurrent || pNewTermCondition->ConditionType == PassedCharge))
			{
				if (!IsFraction(szTempString))
				{
					float fTempVar;
					if (!IsFloat(szTempString))
					{
						MessageBox(NULL, "The value of current should be a float", NULL, MB_OK);
						fTempVar = 0;
					}
					else
					{
						sscanf(szTempString, "%f", &fTempVar);
						uiTempVar = (UINT)(fTempVar * TestPreference.uBatteryCapaicty);
					}
				}
				else
				{
					INT iNumerator, iDenumerator;
					sscanf(szTempString, "%d/%d", &iNumerator, &iDenumerator);
					uiTempVar = (UINT)((float)iNumerator / iDenumerator * TestPreference.uBatteryCapaicty);
				}
			}
			else
			{
				if (!IsNumerical(szTempString))				//the input current with unit mA should be at minimum resolution of 1mA
				{
					MessageBox(NULL, "The current for unit mA should be integer", NULL, MB_OK);
				}
				else
				{
					sscanf(szTempString, "%d", &uiTempVar);
				}
			}
		}
		else
			uiTempVar = 0;

		pNewTermCondition->iThreshold = uiTempVar;

		pPresentTermCondition->pRightCondition = pNewTermCondition;
		break;
	default:
		break;
	}

	return TRUE;
}

BOOL DeleteConditionFromTree(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep, UINT ReasonToChange)
{
	UINT uiConditionID, uiPrevConditionID;
	char szTempString[MAX_STRING_LENGTH] = { 0 };
	TERM_COND *pTermCond = NULL, *pPrevTermCond = NULL;

	switch (ReasonToChange)
	{
	case IDC_BUTTON_DELETE_CONDITION_UP:

		GetWindowText(HandleSet.hStaticConditionIDUp, szTempString, MAX_STRING_LENGTH);
		if (szTempString[0] != 0)
			sscanf(szTempString, "%d", &uiConditionID);
		else
			return FALSE;

		pTermCond = GetConditionPointerWithID(pTestStep->pTerminateCondition, uiConditionID);

		if (NULL == pTermCond)
			return FALSE;

		uiPrevConditionID = GetIDFromConditionCombo(HandleSet.hComboConditonID);

		if (-1 == uiPrevConditionID)
			return FALSE;

		pPrevTermCond = GetConditionPointerWithID(pTestStep->pTerminateCondition, uiPrevConditionID);

		if (pPrevTermCond->pLeftCondition != pTermCond)
			return FALSE;

		ReleaseConditionMem(&pTermCond);
		pPrevTermCond->pLeftCondition = NULL;

		break;
	case IDC_BUTTON_DELETE_CONDITION_DOWN:

		GetWindowText(HandleSet.hStaticConditionIDDown, szTempString, MAX_STRING_LENGTH);
		if (szTempString[0] != 0)
			sscanf(szTempString, "%d", &uiConditionID);
		else
			return FALSE;

		pTermCond = GetConditionPointerWithID(pTestStep->pTerminateCondition, uiConditionID);

		if (NULL == pTermCond)
			return FALSE;

		uiPrevConditionID = GetIDFromConditionCombo(HandleSet.hComboConditonID);

		if (-1 == uiPrevConditionID)
			return FALSE;

		pPrevTermCond = GetConditionPointerWithID(pTestStep->pTerminateCondition, uiPrevConditionID);

		if (pPrevTermCond->pRightCondition != pTermCond)
			return FALSE;

		ReleaseConditionMem(&pTermCond);
		pPrevTermCond->pRightCondition = NULL;

		ReleaseConditionMem(&pTermCond);

		break;
	default:
		break;
	}

	return TRUE;
}

HRESULT CheckConditionTree(TERM_COND *pConditionTree)
{
	HRESULT Result = ERROR_SUCCESS;

	if (NULL == pConditionTree)
		return ERROR_CONDITION_UNINITIALIZED_TREE;

	Result = RecursiveCheckConditionTree(pConditionTree, CENTER);

	return Result;
}

HRESULT RecursiveCheckConditionTree(TERM_COND *pCondition, CONDITION_NODE_POSITION enPosition)
{
	HRESULT ResultRight = ERROR_SUCCESS, ResultLeft = ERROR_SUCCESS, Result = ERROR_SUCCESS;
	
	if (NULL != pCondition->pLeftCondition)
		ResultLeft = RecursiveCheckConditionTree(pCondition->pLeftCondition, LEFT);

	if (ERROR_SUCCESS != ResultLeft)
		return ResultLeft;

	if (NULL != pCondition->pRightCondition)
		ResultRight = RecursiveCheckConditionTree(pCondition->pRightCondition, RIGHT);

	if (ERROR_SUCCESS != ResultRight)
		return ResultRight;

	if ((NULL == pCondition->pLeftCondition) && (NULL == pCondition->pRightCondition))
		Result = CheckSingleCondition(pCondition, enPosition);

		return Result;

}

HRESULT CheckSingleCondition(TERM_COND *pCondition, CONDITION_NODE_POSITION enPosition)
{
	HRESULT Result = ERROR_SUCCESS;

	switch (pCondition->ConditionType)
	{
	case Voltage:
	case Current:
	case VoltageRate:
	case CurrentRate:
	case AllTime:
	case PassedCharge:
	case Counter:
		if (NOCOMP == pCondition->ComparisonType)
			Result = ERROR_CONDITION_INCORRECT_COMPARATOR;
		break;
	case ConditionTime:
		if (LEFT == enPosition)
		{
			Result = ERROR_CONDITION_INCORRECT_TYPE;
			break;
		}

		if (NOCOMP == pCondition->ComparisonType)
			Result = ERROR_CONDITION_INCORRECT_COMPARATOR;
		break;
	case Bifurcation:
		if (NOCOMP != pCondition->ComparisonType)
			Result = ERROR_CONDITION_INCORRECT_COMPARATOR;
		break;
	default:
		break;
	}

	if (ERROR_SUCCESS != Result)
	{
		Result = pCondition->uiConditionID << 16;
	}

	return Result;
}

HRESULT CheckAndSaveOperationInfo(CONTROL_HANDLE_SET HandleSet, TEST_STEP *pTestStep)
{
	HRESULT	Result = ERROR_SUCCESS;
	int TempResult = 0;
	char szTempString[MAX_TEMP_STRING_LENGTH] = { 0 };

	GetWindowText(HandleSet.hComboStepNumber, szTempString, MAX_STRING_LENGTH);

	if (0 == szTempString[0])
	{
		MessageBox(NULL, "Step Number not assigned", NULL, MB_OK);
		return ERROR_STEP_NUMBER_NOT_ASSIGNED;
	}
	
	TempResult = sscanf(szTempString, "%d", &(pTestStep->uiStepNumber));

	pTestStep->StepOperation = ComboBox_GetCurSel(HandleSet.hComboOperationType);

	switch (pTestStep->StepOperation)
	{
	case CB_ERR:
		MessageBox(NULL, "Incorrect Operation Type!", NULL, MB_OK);
		Result = ERROR_INCORRECT_OPERATION_TYPE;
		break;
	case NOP:
		break;
	case CHARGE:
		GetWindowText(HandleSet.hEditVoltage, szTempString, MAX_STRING_LENGTH);
		if (!IsNumerical(szTempString))
		{
			MessageBox(NULL, "Error Inputs in Voltage Box", NULL, MB_OK);
			Result = ERROR_VOLTAGE_INPUT;
		}
		else
		{
			sscanf(szTempString, "%d", &(pTestStep->iVoltage));
		}

		GetWindowText(HandleSet.hEditCurrent, szTempString, MAX_STRING_LENGTH);
		if (!IsNumerical(szTempString) && !IsFloat(szTempString) && !IsFraction(szTempString))
		{
			MessageBox(NULL, "Error Inputs in Current Box", NULL, MB_OK);
			Result = ERROR_CURRENT_INPUT;
		}
		else
		{
			if (C_Rate == TestPreference.lUnit)
			{
				float fTempCRateCurrent;
				if(!IsFraction(szTempString))
					sscanf(szTempString, "%f", &fTempCRateCurrent);
				else
				{
					INT iNumerator, iDenumerator;
					sscanf(szTempString, "%d/%d", &iNumerator, &iDenumerator);
					fTempCRateCurrent = (float)iNumerator / iDenumerator;
				}
				pTestStep->dblCurrent = TestPreference.uBatteryCapaicty * fTempCRateCurrent;
			}
			else
			{
				if (!IsFloat(szTempString))
				{
					MessageBox(NULL, "The current for unit mA should be a float", NULL, MB_OK);
					Result = ERROR_CURRENT_INPUT;
				}
				sscanf(szTempString, "%lf", &(pTestStep->dblCurrent));
			}
		}
		break;
	case DISCHARGE:
		GetWindowText(HandleSet.hEditCurrent, szTempString, MAX_STRING_LENGTH);
		if (!IsNumerical(szTempString) && !IsFloat(szTempString) && !IsFraction(szTempString))
		{
			MessageBox(NULL, "Error Inputs in Current Box", NULL, MB_OK);
			Result = ERROR_CURRENT_INPUT;
		}
		else
		{
			if (C_Rate == TestPreference.lUnit)
			{
				float fTempCRateCurrent;
				if (!IsFraction(szTempString))
					sscanf(szTempString, "%f", &fTempCRateCurrent);
				else
				{
					INT iNumerator, iDenumerator;
					sscanf(szTempString, "%d/%d", &iNumerator, &iDenumerator);
					fTempCRateCurrent = (float)iNumerator / iDenumerator;
				}
				pTestStep->dblCurrent = TestPreference.uBatteryCapaicty * fTempCRateCurrent;
			}
			else
			{
				if (!IsNumerical(szTempString))
				{
					MessageBox(NULL, "The current for unit mA should be float", NULL, MB_OK);
					Result = ERROR_CURRENT_INPUT;
				}
				sscanf(szTempString, "%lf", &(pTestStep->dblCurrent));
			}
		}
		break;
	case SET_TEMP:
		GetWindowText(HandleSet.hEditTemperature, szTempString, MAX_STRING_LENGTH);

		if (!IsNumerical(szTempString))
		{
			MessageBox(NULL, "Error Inputs in Temperature Box", NULL, MB_OK);
			Result = ERROR_CURRENT_INPUT;
		}
		else
		{
			sscanf(szTempString, "%d", &(pTestStep->iTemperature));
		}

		GetWindowText(HandleSet.hEditMinTime, szTempString, MAX_STRING_LENGTH);

		if (!IsNumerical(szTempString))
		{
			MessageBox(NULL, "Error Inputs in Temperature Box", NULL, MB_OK);
			Result = ERROR_CURRENT_INPUT;
		}
		else
		{
			sscanf(szTempString, "%d", &(pTestStep->iMinTempTime));
		}

		break;
	case REST:
		break;
	case GOTO:
		GetWindowText(HandleSet.hEditJumpStepNumber, szTempString, MAX_STRING_LENGTH);
		if (!IsNumerical(szTempString))
		{
			MessageBox(NULL, "Error Inputs in Jump Step Number Box", NULL, MB_OK);
			Result = ERROR_STEP_NUMBER;
		}
		else
		{
			sscanf(szTempString, "%d", &(pTestStep->iJumpStepNumber));
		}
		break;
	default:
		break;
	}

	return Result;
}

void RestoreWindowText(HWND hEdit)
{
	char szTempString[MAX_STRING_LENGTH] = { 0 };

	GetWindowText(hEdit, szTempString, MAX_STRING_LENGTH);
	SetWindowText(hEdit, szTempString);

}

void AddTestStepToScheduleTree(TEST_SCHEDULE_PTR pTestSchedule, TEST_STEP *pTestStep)
{
	TEST_STEP *pStep = NULL;

	if (NULL == pTestSchedule)
		return;

	pStep = pTestSchedule;

	while ((pStep->pNextStep) && (pTestStep->uiStepNumber - 1 != pStep->uiStepNumber))
	{
		pStep = pStep->pNextStep;
	}

	pTestStep->pNextStep = pStep->pNextStep;
	pStep->pNextStep = pTestStep;

	if (NULL == pTestStep->pNextStep)
		return;

	pStep = pTestStep->pNextStep;

	if (pTestStep->uiStepNumber == pStep->uiStepNumber)
	{
		for (; pStep != NULL; pStep = pStep->pNextStep)
		{
			if (IsJumpTarget(pTestSchedule, pStep->uiStepNumber))
			{
				RefreshJumpTarget(pTestSchedule, pStep->uiStepNumber, pStep->uiStepNumber + 1 + MINLONG);
			}
			pStep->uiStepNumber++;
		}
		RecoverJumpNumberOffset(pTestSchedule);
	}
}