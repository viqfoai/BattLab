#pragma once

#define CBN_DELETE_STEP_SELECTED	(CBN_SELCHANGE << 16) + IDC_COMBO_STEP_LIST

BOOL WINAPI DeleteStepFromSchedule(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL RemoveStepFromList(TEST_SCHEDULE_PTR pSchedule, UINT uiStepIndex);
void SmoothNumberInStepList(TEST_SCHEDULE_PTR pSchedule);
BOOL IsJumpTarget(TEST_SCHEDULE_PTR pSchedule, UINT uiNumber);
void RefreshJumpTarget(TEST_SCHEDULE_PTR pSchedule, UINT uiOldNumber, UINT uiNewNumber);
void RecoverJumpNumberOffset(TEST_SCHEDULE_PTR pSchedule);