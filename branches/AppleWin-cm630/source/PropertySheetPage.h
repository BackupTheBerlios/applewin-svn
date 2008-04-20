#pragma once

void    PSP_Init();
DWORD   PSP_GetVolumeMax();
bool    PSP_SaveStateSelectImage(HWND hWindow, bool bSave);
void ui_tfe_settings_dialog(HWND hwnd);
void * get_tfe_interface(void);
void get_tfe_enabled(int *tfe_enabled);
//void BrowseToCiderPress ();
//static int BrowseToCiderPress (HWND hWindow, TCHAR* pszTitle);
string BrowseToCiderPress (HWND hWindow, TCHAR* pszTitle);
UINT RestartOnNewComputerType(DWORD NewApple2Type, HWND window, UINT afterclose);

extern UINT g_uScrollLockToggle;
extern UINT g_uMouseInSlot4;
extern UINT g_uTheFreezesF8Rom;
extern DWORD g_uCloneType;
extern HWND hwConfigTab;
extern HWND hwAdvancedTab;
