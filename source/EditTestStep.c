#include <Windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include "resource.h"
#include "TempChamberDriver.h"
#include "BatCycleController.h"
#include "BasicFunction.h"
#include "AddTestStep.h"
#include "DeleteTestStep.h"
#include "EditTestStep.h"
#include "SetTestPreference.h"

extern char *szOperation[];
extern char *szConditionType[];
extern char *szComparasonType[];
extern char *szLogicalRel[];
extern TEST_PREFERENCE TestPreference;

BOOL WINAPI EditTestStep(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static TEST_STEP *pTestScheduleList = NULL, *pTestStep = NULL, *pOrgTestStep = NULL;
	static HWND hTestScheduleListView = NULL, hParentDialog = NULL;
	static CONTROL_HANDLE_SET HandleSet = { 0 };
	static UINT uiStepIndex = 0;
	char szTempStr[MAX_STRING_LENGTH] = { 0 };
	HRESULT Result = 0;

	switch (uMsg)
	{
	case WM_INITDIALOG:

		pTestScheduleList = *(TEST_SCHEDULE_PTR *)lParam;
		if (NULL == pTestScheduleList || NULL == pTestScheduleList->pNextStep)
		{
			MessageBox(NULL, "Test Schedule Not Initialised", NULL, MB_OK);
			EndDialog(hWnd, -1);
			break;
		}

		hParentDialog = GetParent(hWnd);
		hTestScheduleListView = GetDlgItem(hParentDialog, IDC_TEST_SCHEDULE_LIST);
		uiStepIndex = ListView_GetSelectionMark(hTestScheduleListView);

		if (-1 != uiStepIndex)
			pOrgTestStep = LocateStepWithTestNumber(pTestScheduleList, uiStepIndex + 1);
		else
			pOrgTestStep = pTestScheduleList->pNextStep;
		
		ReCreateTestStep(&pTestStep, pOrgTestStep);

		InitEditStepDialog(hWnd, &HandleSet, pTestScheduleList);

		ComboBox_SetCurSel(HandleSet.hComboStepNumber, pTestStep->uiStepNumber - 1);
		ComboBox_SetCurSel(HandleSet.hComboOperationType, pTestStep->StepOperation);

		UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleList, (WPARAM)DLGN_EDIT_STEP_INIT);

		break;

	case WM_UPDATE_STEP_DIALOG:
		UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleList, (WPARAM)wParam);
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case CBN_STEP_NUMBER_SELECTED:
			if (!LitterTestStep(&pTestStep))
			{
				MessageBox(NULL, "Littering Test Step Failed!", NULL, MB_OK);
				EndDialog(hWnd, -1);
				break;
			}
			else
			{
				uiStepIndex = ComboBox_GetCurSel(HandleSet.hComboStepNumber);
				ReCreateTestStepWithStepNumber(&pTestStep, pTestScheduleList, uiStepIndex + 1);
			}

			ComboBox_SetCurSel(HandleSet.hComboStepNumber, pTestStep->uiStepNumber - 1);
			ComboBox_SetCurSel(HandleSet.hComboOperationType, pTestStep->StepOperation);

			//RefreshEditStepDialog(hWnd, &HandleSet, pTestStep);
			PostMessage(hWnd, WM_UPDATE_STEP_DIALOG, CBN_STEP_NUMBER_SELECTED, 0);
			break;
		case CBN_STEP_OPERATION_SELECTED:
			UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleList, CBN_STEP_OPERATION_SELECTED);
			break;
		case CBN_CONDITION_ID_CHANGE:
			UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleList, CBN_CONDITION_ID_CHANGE);
			break;
		case CBN_UP_CONDITION_TYPE_CHANGE:
			UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleList, CBN_UP_CONDITION_TYPE_CHANGE);
			break;
		case CBN_DOWN_CONDITION_TYPE_CHANGE:
			UpdateStepDialog(HandleSet, pTestStep, &pTestScheduleList, CBN_DOWN_CONDITION_TYPE_CHANGE);
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
				SubstituteTestStepInScheduleTree(pTestScheduleList, pTestStep);
				pTestStep = NULL;
				EndDialog(hWnd, ERROR_SUCCESS);
			}
			else
			{
				MessageBox(NULL, "Condition Error, Please Check", NULL, MB_OK);
			}
			break;
		case IDCANCEL:					//this message is equivalent to WM_CLOSE
			if (!LitterTestStep(&pTestStep))
			{
				MessageBox(NULL, "Littering Test Step Failed!", NULL, MB_OK);
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

BOOL ReCreateTestStep(TEST_STEP **ppDestTestStep, TEST_STEP *pSourceStep)
{
	if (NULL != *ppDestTestStep)
		return FALSE;

	*ppDestTestStep = Malloc(sizeof(TEST_STEP));

	if (NULL == *ppDestTestStep)
		return FALSE;

	memcpy(*ppDestTestStep, pSourceStep, sizeof(TEST_STEP));

	return (ReCreateCondition(&((*ppDestTestStep)->pTerminateCondition), pSourceStep->pTerminateCondition));
}

BOOL ReCreateTestStepWithStepNumber(TEST_STEP **ppDestTestStep, TEST_SCHEDULE_PTR pSourceSchedule, UINT uiStepNumber)
{
	TEST_STEP *pSourceStep = NULL;

	pSourceStep = LocateStepWithTestNumber(pSourceSchedule, uiStepNumber);
	if (pSourceStep)
		return (ReCreateTestStep(ppDestTestStep, pSourceStep));
	else
		return FALSE;
}

BOOL ReCreateCondition(TERM_COND **ppDestTermCond, TERM_COND *pSourcTermCond)
{
	RecursiveReCreateCondition(ppDestTermCond, pSourcTermCond);

	if (NULL == *ppDestTermCond)
		return FALSE;
	else
		return TRUE;
}

void RecursiveReCreateCondition(TERM_COND **ppDestTermCond, TERM_COND *pSourcTermCond)
{
	if (NULL != pSourcTermCond)
	{
		*ppDestTermCond = Malloc(sizeof(TERM_COND));
		memcpy(*ppDestTermCond, pSourcTermCond, sizeof(TERM_COND));
	}

	if (NULL != pSourcTermCond->pLeftCondition)
		RecursiveReCreateCondition(&((*ppDestTermCond)->pLeftCondition), pSourcTermCond->pLeftCondition);

	if (NULL != pSourcTermCond->pRightCondition)
		RecursiveReCreateCondition(&((*ppDestTermCond)->pRightCondition), pSourcTermCond->pRightCondition);

	if (NULL == pSourcTermCond->pLeftCondition)
		(*ppDestTermCond)->pLeftCondition = NULL;

	if (NULL == pSourcTermCond->pRightCondition)
		(*ppDestTermCond)->pRightCondition = NULL;
}

void InitEditStepDialog(HWND hWnd, CONTROL_HANDLE_SET *pHandleSet, TEST_SCHEDULE_PTR pTestSchedule)
{
	HWND hParentDialog = NULL, hTestScheduleListView = NULL;
	UINT uiItemCount = 0;
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
	pTestStep = pTestSchedule->pNextStep;

	ComboBox_SetCurSel(pHandleSet->hComboStepNumber, 0);

	pHandleSet->hComboOperationType = GetDlgItem(hWnd, IDC_COMBO_OPERATION_TYPE);
	for (enOperation = NOP; enOperation <= SET_TEMP; enOperation++)
	{
		SendMessage(pHandleSet->hComboOperationType, CB_INSERTSTRING, -1, (LPARAM)szOperation[enOperation]);
	}
	SendMessage(pHandleSet->hComboOperationType, CB_SETCURSEL, (WPARAM)(pTestStep->StepOperation), 0);

	pHandleSet->hEditVoltage = GetDlgItem(hWnd, IDC_EDIT_VOLT_ADD_STEP);
	pHandleSet->hEditCurrent = GetDlgItem(hWnd, IDC_EDIT_CURR_ADD_STEP);
	pHandleSet->hEditTemperature = GetDlgItem(hWnd, IDC_EDIT_SET_TEMP);
	pHandleSet->hEditMinTime = GetDlgItem(hWnd, IDC_EDIT_MIN_TIME);
	pHandleSet->hStaticCurrUnitAddStep = GetDlgItem(hWnd, IDC_STATIC_CURR_EDIT_STEP);
	SetWindowText(pHandleSet->hStaticCurrUnitAddStep, GetLoadUnit(&TestPreference));
	pHandleSet->hEditJumpStepNumber = GetDlgItem(hWnd, IDC_EDIT_JUMP_STEP_NUMBER);

	switch (pTestStep->StepOperation)
	{
	case CHARGE:
		sprintf(szComboString, "%d", pTestStep->iVoltage);
		SetWindowText(pHandleSet->hEditVoltage, szComboString);
		memset(szComboString, 0, strlen(szComboString));

		if (C_Rate == TestPreference.lUnit)
			sprintf(szComboString, "%5.3f", pTestStep->dblCurrent / (float)TestPreference.uBatteryCapaicty);
		else
			sprintf(szComboString, "%5.3lf", pTestStep->dblCurrent);

		SetWindowText(pHandleSet->hEditCurrent, szComboString);
		memset(szComboString, 0, strlen(szComboString));
		break;
	case SET_TEMP:
		sprintf(szComboString, "%d", pTestStep->iTemperature);
		SetWindowText(pHandleSet->hEditTemperature, szComboString);

		memset(szComboString, 0, strlen(szComboString));
		sprintf(szComboString, "%d", pTestStep->iMinTempTime);
		SetWindowText(pHandleSet->hEditMinTime, szComboString);

		break;
	case DISCHARGE:

		if (C_Rate == TestPreference.lUnit)
			sprintf(szComboString, "%5.3f", pTestStep->dblCurrent / (float)TestPreference.uBatteryCapaicty);
		else
			sprintf(szComboString, "%5.3lf", pTestStep->dblCurrent);

		SetWindowText(pHandleSet->hEditCurrent, szComboString);
		memset(szComboString, 0, strlen(szComboString));
		break;
	case GOTO:
		sprintf(szComboString, "%d", pTestStep->iJumpStepNumber);
		SetWindowText(pHandleSet->hEditJumpStepNumber, szComboString);
		memset(szComboString, 0, strlen(szComboString));
		break;
	default:
		break;
	}

	pHandleSet->hComboConditonID = GetDlgItem(hWnd, IDC_COMBO_CONDITION_ID);
	UpdateConditionIDCombo(pHandleSet->hComboConditonID, pTestStep->pTerminateCondition);
	EnableWindow(pHandleSet->hComboConditonID, TRUE);

	pHandleSet->hStaticConditionIDUp = GetDlgItem(hWnd, IDC_STATIC_CONDITON_ID_UP);
	if (NULL != pTestStep->pTerminateCondition->pLeftCondition)
	{
		sprintf(szComboString, "%d", pTestStep->pTerminateCondition->pLeftCondition->uiConditionID);
		SetWindowText(pHandleSet->hStaticConditionIDUp, szComboString);
		memset(szComboString, 0, strlen(szComboString));
	}

	pHandleSet->hComboConditionUp = GetDlgItem(hWnd, IDC_COMBO_CONDITION_TYPE_UP);
	for (enConditionType = Bifurcation; enConditionType < ConditionTime; enConditionType++)
	{
		SendMessage(pHandleSet->hComboConditionUp, CB_INSERTSTRING, -1, (LPARAM)szConditionType[enConditionType]);
	}
	if (NULL != pTestStep->pTerminateCondition->pLeftCondition)
		SendMessage(pHandleSet->hComboConditionUp, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->pLeftCondition->ConditionType), 0);
	else
		SendMessage(pHandleSet->hComboConditionUp, CB_SETCURSEL, Bifurcation, 0);

	pHandleSet->hComboComparatorUp = GetDlgItem(hWnd, IDC_COMBO_COMPARATOR_UP);
	for (enCompType = NOCOMP; enCompType <= NoLessThan; enCompType++)
	{
		SendMessage(pHandleSet->hComboComparatorUp, CB_INSERTSTRING, -1, (LPARAM)szComparasonType[enCompType]);
	}
	if (NULL != pTestStep->pTerminateCondition->pLeftCondition)
		SendMessage(pHandleSet->hComboComparatorUp, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->pLeftCondition->ComparisonType), 0);
	else
		SendMessage(pHandleSet->hComboComparatorUp, CB_SETCURSEL, (WPARAM)NOCOMP, 0);

	pHandleSet->hEditThresholdUp = GetDlgItem(hWnd, IDC_EDIT_CONDITION_THRESHOLD_UP);
	if ((NULL != pTestStep->pTerminateCondition->pLeftCondition) && (NOCOMP != pTestStep->pTerminateCondition->pLeftCondition->ComparisonType))
	{
		if (C_Rate == TestPreference.lUnit &&\
			(pTestStep->pTerminateCondition->pLeftCondition->ConditionType == Current \
				|| pTestStep->pTerminateCondition->pLeftCondition->ConditionType == AvgCurrent\
				|| pTestStep->pTerminateCondition->pLeftCondition->ConditionType == PassedCharge))
			sprintf(szComboString, "%5.3f", pTestStep->pTerminateCondition->pLeftCondition->iThreshold / (float)TestPreference.uBatteryCapaicty);
		else
			sprintf(szComboString, "%d", pTestStep->pTerminateCondition->pLeftCondition->iThreshold);

		SetWindowText(pHandleSet->hEditThresholdUp, szComboString);
		memset(szComboString, 0, sizeof(szComboString));
	}
	else
		SetWindowText(pHandleSet->hEditThresholdUp, "");

	pHandleSet->hStaticThresholdUnitUp = GetDlgItem(hWnd, IDC_STATIC_THRESHOLD_UNIT_UP);		//this item will be updated in the UpdateStepDialog function

	pHandleSet->hButtonAddUp = GetDlgItem(hWnd, IDC_BUTTON_ADD_CONDITION_UP);					//this item will be updated in the UpdateStepDialog function
	pHandleSet->hButtonDeleteUp = GetDlgItem(hWnd, IDC_BUTTON_DELETE_CONDITION_UP);				//this item will be updated in the UpdateStepDialog function

	pHandleSet->hComboRelationShip = GetDlgItem(hWnd, IDC_COMBO_RELATIONSHIP);
	SendMessage(pHandleSet->hComboRelationShip, CB_INSERTSTRING, -1, (LPARAM)szLogicalRel[AND]);
	SendMessage(pHandleSet->hComboRelationShip, CB_INSERTSTRING, -1, (LPARAM)szLogicalRel[OR]);
	if (NULL != pTestStep->pTerminateCondition)
		SendMessage(pHandleSet->hComboRelationShip, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->RelationShip), 0);
	else
		SendMessage(pHandleSet->hComboComparatorUp, CB_SETCURSEL, (WPARAM)AND, 0);

	pHandleSet->hStaticConditionIDDown = GetDlgItem(hWnd, IDC_STATIC_CONDITON_ID_DOWN);
	if (NULL != pTestStep->pTerminateCondition->pRightCondition)
	{
		sprintf(szComboString, "%d", pTestStep->pTerminateCondition->pRightCondition->uiConditionID);
		SetWindowText(pHandleSet->hStaticConditionIDDown, szComboString);
		memset(szComboString, 0, strlen(szComboString));
	}

	pHandleSet->hComboConditionDown = GetDlgItem(hWnd, IDC_COMBO_CONDITION_TYPE_DOWN);
	for (enConditionType = Bifurcation; enConditionType <= ConditionTime; enConditionType++)
	{
		SendMessage(pHandleSet->hComboConditionDown, CB_INSERTSTRING, -1, (LPARAM)szConditionType[enConditionType]);
	}
	if (NULL != pTestStep->pTerminateCondition->pRightCondition)
		SendMessage(pHandleSet->hComboConditionDown, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->pRightCondition->ConditionType), 0);
	else
		SendMessage(pHandleSet->hComboConditionDown, CB_SETCURSEL, (WPARAM)Bifurcation, 0);
	
	pHandleSet->hComboComparatorDown = GetDlgItem(hWnd, IDC_COMBO_COMPARATOR_DOWN);
	for (enCompType = NOCOMP; enCompType <= NoLessThan; enCompType++)
	{
		SendMessage(pHandleSet->hComboComparatorDown, CB_INSERTSTRING, -1, (LPARAM)szComparasonType[enCompType]);
	}
	if (NULL != pTestStep->pTerminateCondition->pRightCondition)
		SendMessage(pHandleSet->hComboComparatorDown, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->pRightCondition->ComparisonType), 0);
	else
		SendMessage(pHandleSet->hComboComparatorDown, CB_SETCURSEL, (WPARAM)NOCOMP, 0);
	
	pHandleSet->hEditThresholdDown = GetDlgItem(hWnd, IDC_EDIT_CONDITION_THRESHOLD_DOWN);
	if ((NULL != pTestStep->pTerminateCondition->pRightCondition) && (NOCOMP != pTestStep->pTerminateCondition->pRightCondition->ComparisonType))
	{
		if (C_Rate == TestPreference.lUnit &&\
			(pTestStep->pTerminateCondition->pRightCondition->ConditionType == Current \
				|| pTestStep->pTerminateCondition->pRightCondition->ConditionType == AvgCurrent\
				|| pTestStep->pTerminateCondition->pRightCondition->ConditionType == PassedCharge))
			sprintf(szComboString, "%5.3f", pTestStep->pTerminateCondition->pRightCondition->iThreshold / (float)TestPreference.uBatteryCapaicty);
		else
			sprintf(szComboString, "%d", pTestStep->pTerminateCondition->pRightCondition->iThreshold);
		SetWindowText(pHandleSet->hEditThresholdDown, szComboString);
		memset(szComboString, 0, sizeof(szComboString));
	}
	else
		SetWindowText(pHandleSet->hEditThresholdDown, "");

	pHandleSet->hButtonAddDown = GetDlgItem(hWnd, IDC_BUTTON_ADD_CONDITION_DOWN);
	pHandleSet->hButtonDeleteDown = GetDlgItem(hWnd, IDC_BUTTON_DELETE_CONDITION_DOWN);

	pHandleSet->hStaticThresholdUnitDown = GetDlgItem(hWnd, IDC_STATIC_THRESHOLD_UNIT_DOWN);
}

void SubstituteTestStepInScheduleTree(TEST_SCHEDULE_PTR pTestSchedule, TEST_STEP *pTestStep)
{
	TEST_STEP *pStep = NULL;

	if (NULL == pTestSchedule)
		return;

	pStep = pTestSchedule;

	while ((pStep->pNextStep) && (pTestStep->uiStepNumber != pStep->pNextStep->uiStepNumber))
	{
		pStep = pStep->pNextStep;
	}

	pTestStep->pNextStep = pStep->pNextStep->pNextStep;
	RemoveStepFromList(pTestSchedule, pTestStep->uiStepNumber);
	pStep->pNextStep = pTestStep;
}

BOOL LitterTestStep(TEST_STEP **pStep)
{
	TEST_STEP *pTestStep = *pStep;
	HRESULT Result = ERROR_SUCCESS;

	if (NULL == *pStep)
		return TRUE;

	if (pTestStep->pTerminateCondition != NULL)
		Result = ReleaseConditionMem(&(pTestStep->pTerminateCondition));

	if (ERROR != ERROR_SUCCESS)
	{
		return FALSE;
	}
	else
	{
		free(pTestStep);
		*pStep = NULL;
		return TRUE;
	}
}

void RefreshEditStepDialog(HWND hWnd, CONTROL_HANDLE_SET *pHandleSet, TEST_STEP *pTestStep)
{
	HWND hParentDialog = NULL, hTestScheduleListView = NULL;
	UINT uiItemCount = 0;
	char szComboString[10] = { 0 };

	if (NULL == pHandleSet)
		return;

	ComboBox_SetCurSel(pHandleSet->hComboStepNumber, pTestStep->uiStepNumber - 1);

	SendMessage(pHandleSet->hComboOperationType, CB_SETCURSEL, (WPARAM)(pTestStep->StepOperation), 0);
	/*
	switch (pTestStep->StepOperation)
	{
	case CHARGE:
		sprintf(szComboString, "%d", pTestStep->iVoltage);
		SetWindowText(pHandleSet->hEditVoltage, szComboString);
		memset(szComboString, 0, strlen(szComboString));

		sprintf(szComboString, "%d", pTestStep->iCurrent);
		SetWindowText(pHandleSet->hEditCurrent, szComboString);
		memset(szComboString, 0, strlen(szComboString));

		SetWindowText(pHandleSet->hEditJumpStepNumber, "");
		break;
	case DISCHARGE:
		SetWindowText(pHandleSet->hEditVoltage, "");

		sprintf(szComboString, "%d", pTestStep->iCurrent);
		SetWindowText(pHandleSet->hEditCurrent, szComboString);
		memset(szComboString, 0, strlen(szComboString));

		SetWindowText(pHandleSet->hEditJumpStepNumber, "");
		break;
	case GOTO:
		SetWindowText(pHandleSet->hEditVoltage, "");

		SetWindowText(pHandleSet->hEditCurrent, "");
		sprintf(szComboString, "%d", pTestStep->iJumpStepNumber);
		SetWindowText(pHandleSet->hEditJumpStepNumber, szComboString);
		memset(szComboString, 0, strlen(szComboString));
		break;
	case REST:
		SetWindowText(pHandleSet->hEditVoltage, "");
		SetWindowText(pHandleSet->hEditCurrent, "");
		SetWindowText(pHandleSet->hEditJumpStepNumber, "");
		break;
	default:
		break;
	}*/
	/*
	UpdateConditionIDCombo(pHandleSet->hComboConditonID, pTestStep->pTerminateCondition);
	EnableWindow(pHandleSet->hComboConditonID, TRUE);

	if (NULL != pTestStep->pTerminateCondition->pLeftCondition)
	{
		sprintf(szComboString, "%d", pTestStep->pTerminateCondition->pLeftCondition->uiConditionID);
		SetWindowText(pHandleSet->hStaticConditionIDUp, szComboString);
		memset(szComboString, 0, strlen(szComboString));
	}

	if (NULL != pTestStep->pTerminateCondition->pLeftCondition)
		SendMessage(pHandleSet->hComboConditionUp, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->pLeftCondition->ConditionType), 0);
	else
		SendMessage(pHandleSet->hComboConditionUp, CB_SETCURSEL, Bifurcation, 0);

	if (NULL != pTestStep->pTerminateCondition->pLeftCondition)
		SendMessage(pHandleSet->hComboComparatorUp, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->pLeftCondition->ComparisonType), 0);
	else
		SendMessage(pHandleSet->hComboComparatorUp, CB_SETCURSEL, (WPARAM)NOCOMP, 0);

	if ((NULL != pTestStep->pTerminateCondition->pLeftCondition) && (NOCOMP != pTestStep->pTerminateCondition->pLeftCondition->ComparisonType))
	{
		sprintf(szComboString, "%d", pTestStep->pTerminateCondition->pLeftCondition->iThreshold);
		SetWindowText(pHandleSet->hEditThresholdUp, szComboString);
		memset(szComboString, 0, sizeof(szComboString));
	}
	else
		SetWindowText(pHandleSet->hEditThresholdUp, "");

	if (NULL != pTestStep->pTerminateCondition)
		SendMessage(pHandleSet->hComboRelationShip, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->RelationShip), 0);
	else
		SendMessage(pHandleSet->hComboComparatorUp, CB_SETCURSEL, (WPARAM)AND, 0);

	if (NULL != pTestStep->pTerminateCondition->pRightCondition)
	{
		sprintf(szComboString, "%d", pTestStep->pTerminateCondition->pRightCondition->uiConditionID);
		SetWindowText(pHandleSet->hStaticConditionIDDown, szComboString);
		memset(szComboString, 0, strlen(szComboString));
	}

	if (NULL != pTestStep->pTerminateCondition->pRightCondition)
		SendMessage(pHandleSet->hComboConditionDown, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->pRightCondition->ConditionType), 0);
	else
		SendMessage(pHandleSet->hComboConditionDown, CB_SETCURSEL, (WPARAM)Bifurcation, 0);

	if (NULL != pTestStep->pTerminateCondition->pRightCondition)
		SendMessage(pHandleSet->hComboComparatorDown, CB_SETCURSEL, (WPARAM)(pTestStep->pTerminateCondition->pRightCondition->ComparisonType), 0);
	else
		SendMessage(pHandleSet->hComboComparatorDown, CB_SETCURSEL, (WPARAM)NOCOMP, 0);

	if ((NULL != pTestStep->pTerminateCondition->pRightCondition) && (NOCOMP != pTestStep->pTerminateCondition->pRightCondition->ComparisonType))
	{
		sprintf(szComboString, "%d", pTestStep->pTerminateCondition->pRightCondition->iThreshold);
		SetWindowText(pHandleSet->hEditThresholdDown, szComboString);
		memset(szComboString, 0, sizeof(szComboString));
	}
	else
		SetWindowText(pHandleSet->hEditThresholdDown, "");*/
}