#include <Windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include "resource.h"
#include "TempChamberDriver.h"
#include "BatCycleController.h"
#include "BasicFunction.h"
#include "AddTestStep.h"
#include "DeleteTestStep.h"



BOOL WINAPI DeleteStepFromSchedule(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static TEST_STEP *pTestScheduleList = NULL, *pTestStep = NULL;
	static HWND hComboStepList = NULL, hButtonDelete = NULL, hButtonCancelDelete = NULL, hParentWindow = NULL, hStepListView = NULL;
	static UINT uiStepIndex = 0, uiTotalSteps = 0, i;
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
		hButtonDelete = GetDlgItem(hWnd, IDC_BUTTON_DELETE_STEP);
		hButtonCancelDelete = GetDlgItem(hWnd, IDC_BUTTON_CANCEL_DELETE);

		hParentWindow = GetParent(hWnd);
		hStepListView = GetDlgItem(hParentWindow, IDC_TEST_SCHEDULE_LIST);

		hComboStepList = GetDlgItem(hWnd, IDC_COMBO_STEP_LIST);
		uiTotalSteps = FindMaxStepNumber(pTestScheduleList);
		if (uiTotalSteps >= 1)
		{
			SetNumericalComboRange(hComboStepList, uiTotalSteps, 1);
			uiStepIndex = ListView_GetSelectionMark(hStepListView);
			if(-1 == uiStepIndex)
				ComboBox_SetCurSel(hComboStepList, 0);
			else
				ComboBox_SetCurSel(hComboStepList, uiStepIndex);

			EnableWindow(hButtonDelete, TRUE);
			EnableWindow(hButtonCancelDelete, FALSE);
		}
		else
		{
			EnableWindow(hButtonDelete, FALSE);
			EnableWindow(hButtonCancelDelete, FALSE);
		}
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_BUTTON_DELETE_STEP:
			uiStepIndex = ComboBox_GetCurSel(hComboStepList);
			if (IsJumpTarget(pTestScheduleList, uiStepIndex + 1))
			{
				MessageBox(NULL, "This step is a target of another GOTO step\n You must delete the GOTO step targetting at this step first!", NULL, MB_OK);
				break;
			}
			szTempStr[0] = '*';
			ComboBox_GetLBText(hComboStepList, uiStepIndex, szTempStr + 1);
			ComboBox_InsertString(hComboStepList, uiStepIndex, szTempStr);
			ComboBox_DeleteString(hComboStepList, uiStepIndex + 1);
			ComboBox_SetCurSel(hComboStepList, uiStepIndex);
			EnableWindow(hButtonDelete, FALSE);
			EnableWindow(hButtonCancelDelete, TRUE);
			break;
		case IDC_BUTTON_CANCEL_DELETE:
			uiStepIndex = ComboBox_GetCurSel(hComboStepList);
			sprintf(szTempStr, "%d", uiStepIndex + 1);
			ComboBox_InsertString(hComboStepList, uiStepIndex, szTempStr);
			ComboBox_DeleteString(hComboStepList, uiStepIndex + 1);
			ComboBox_SetCurSel(hComboStepList, uiStepIndex);
			EnableWindow(hButtonDelete, TRUE);
			EnableWindow(hButtonCancelDelete, FALSE);
			break;
		case CBN_DELETE_STEP_SELECTED:
			uiStepIndex = ComboBox_GetCurSel(hComboStepList);
			ComboBox_GetLBText(hComboStepList, uiStepIndex, szTempStr);
			if ('*' == szTempStr[0])
			{
				EnableWindow(hButtonDelete, FALSE);
				EnableWindow(hButtonCancelDelete, TRUE);
			}
			else
			{
				EnableWindow(hButtonDelete, TRUE);
				EnableWindow(hButtonCancelDelete, FALSE);
			}
			break;
		case IDOK:
			uiTotalSteps = ComboBox_GetCount(hComboStepList);
			for (i = 0; i < uiTotalSteps; i++)
			{
				ComboBox_GetLBText(hComboStepList, i, szTempStr);
				if ('*' == szTempStr[0])
					RemoveStepFromList(pTestScheduleList, i + 1);
			}
			SmoothNumberInStepList(pTestScheduleList);
			EndDialog(hWnd, ERROR_SUCCESS);
			break;
		case IDCANCEL:					//this message is equivalent to WM_CLOSE
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

BOOL RemoveStepFromList(TEST_SCHEDULE_PTR pSchedule, UINT uiStepIndex)
{
	TEST_STEP *pTestStep = NULL, *pPrevStep = NULL;
	HRESULT Result;

	pPrevStep = pSchedule;
	pTestStep = pPrevStep->pNextStep;	

	while (uiStepIndex != pTestStep->uiStepNumber)
	{
		pPrevStep = pTestStep;
		pTestStep = pTestStep->pNextStep;
		if (NULL == pTestStep)
			return FALSE;
	}

	Result = ReleaseConditionMem(&(pTestStep->pTerminateCondition));
	if (ERROR_FAIL == Result)
		return FALSE;
	else
	{
		pPrevStep->pNextStep = pTestStep->pNextStep;
		free(pTestStep);
		return TRUE;
	}
}

void SmoothNumberInStepList(TEST_SCHEDULE_PTR pSchedule)
{
	TEST_STEP *pTestStep;
	UINT uiNewStepNumber = 0;

	pTestStep = pSchedule;

	while (NULL != pTestStep)
	{
		if (IsJumpTarget(pSchedule, pTestStep->uiStepNumber))
		{
			RefreshJumpTarget(pSchedule, pTestStep->uiStepNumber, uiNewStepNumber);
		}
		pTestStep->uiStepNumber = uiNewStepNumber;
		uiNewStepNumber++;
		pTestStep = pTestStep->pNextStep;
	}
}

BOOL IsJumpTarget(TEST_SCHEDULE_PTR pSchedule, UINT uiNumber)
{
	TEST_STEP *pTestStep;

	pTestStep = pSchedule;
	while(NULL != pTestStep)
	{
		if (GOTO == pTestStep->StepOperation && uiNumber == pTestStep->iJumpStepNumber)
		{
			return TRUE;
		}

		pTestStep = pTestStep->pNextStep;
	}

	return FALSE;
}

void RefreshJumpTarget(TEST_SCHEDULE_PTR pSchedule, UINT uiOldNumber, UINT uiNewNumber)
{
	TEST_STEP *pTestStep;

	pTestStep = pSchedule;
	while (NULL != pTestStep)
	{
		if (GOTO == pTestStep->StepOperation && uiOldNumber == pTestStep->iJumpStepNumber)
		{
			pTestStep->iJumpStepNumber = uiNewNumber;
		}

		pTestStep = pTestStep->pNextStep;
	}
}

void RecoverJumpNumberOffset(TEST_SCHEDULE_PTR pSchedule)
{
	TEST_STEP *pTestStep;

	pTestStep = pSchedule;
	while (NULL != pTestStep)
	{
		if (GOTO == pTestStep->StepOperation && pTestStep->iJumpStepNumber >= MINLONG)
		{
			pTestStep->iJumpStepNumber &= ~MINLONG;
		}

		pTestStep = pTestStep->pNextStep;
	}
}