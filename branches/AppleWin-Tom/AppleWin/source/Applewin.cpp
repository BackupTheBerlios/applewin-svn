/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2009, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: main
 *
 * Author: Various
 */

#include "StdAfx.h"
#pragma hdrstop			// Normally would have stdafx.cpp (being the file with /Yc) and wouldn't bother with #pragma hdrstop

#include "DiskImage.h"
#include "Harddisk.h"

//#include <objbase.h>	// Updated[TC]: Removed, as not needed
#include "MouseInterface.h"
#include "Echo.h"
#ifdef USE_SPEECH_API
#include "Speech.h"
#endif

char VERSIONSTRING[16] = "xx.yy.zz.ww";

TCHAR *g_pAppTitle = TITLE_APPLE_2E_ENHANCED;

eApple2Type	g_Apple2Type	= A2TYPE_APPLE2EEHANCED;

BOOL      behind            = 0;			// Redundant
DWORD     cumulativecycles  = 0;			// Wraps after ~1hr 9mins
DWORD     cyclenum          = 0;			// Used by SpkrToggle() for non-wave sound
DWORD     emulmsec          = 0;
static DWORD emulmsec_frac  = 0;
bool      g_bFullSpeed      = false;

//Pravets 8A/C variables
bool P8CAPS_ON = false;
bool P8Shift = false;
//=================================================

// Win32
HINSTANCE g_hInstance          = (HINSTANCE)0;

AppMode_e	g_nAppMode = MODE_LOGO;

static int lastmode         = MODE_LOGO;
DWORD     needsprecision    = 0;			// Redundant
TCHAR     g_sProgramDir[MAX_PATH] = TEXT(""); // Directory of where AppleWin executable resides
TCHAR     g_sDebugDir  [MAX_PATH] = TEXT(""); // TODO: Not currently used
TCHAR     g_sScreenShotDir[MAX_PATH] = TEXT(""); // TODO: Not currently used
TCHAR     g_sCurrentDir[MAX_PATH] = TEXT(""); // Also Starting Dir.  Debugger uses this when load/save
bool      g_bResetTiming    = false;			// Redundant
BOOL      restart           = 0;

DWORD		g_dwSpeed		= SPEED_NORMAL;	// Affected by Config dialog's speed slider bar
double		g_fCurrentCLK6502 = CLK_6502;	// Affected by Config dialog's speed slider bar
static double g_fMHz		= 1.0;			// Affected by Config dialog's speed slider bar

int			g_nCpuCyclesFeedback = 0;
DWORD       g_dwCyclesThisFrame = 0;

FILE*		g_fh			= NULL;
bool		g_bDisableDirectSound = false;
bool		g_bDisableDirectSoundMockingboard = false;

CSuperSerialCard	sg_SSC;
CMouseInterface		sg_Mouse;
CEcho*				g_pEcho = NULL;

// TODO: CLEANUP! Move to peripherals.cpp!!!
#ifdef SUPPORT_CPM
UINT		g_Slot4 = CT_Echo;	//CT_Empty;
#else
UINT		g_Slot4 = CT_Mockingboard;		// CT_Mockingboard or CT_MouseInterface
#endif

eCPU		g_ActiveCPU = CPU_6502;

HANDLE		g_hCustomRomF8 = INVALID_HANDLE_VALUE;	// Cmd-line specified custom ROM at $F800..$FFFF
static bool	g_bCustomRomF8Failed = false;			// Set if custom ROM file failed

static bool	g_bEnableSpeech = false;
#ifdef USE_SPEECH_API
CSpeech		g_Speech;
#endif

//===========================================================================

#define DBG_CALC_FREQ 0
#if DBG_CALC_FREQ
const UINT MAX_CNT = 256;
double g_fDbg[MAX_CNT];
UINT g_nIdx = 0;
double g_fMeanPeriod,g_fMeanFreq;
ULONGLONG g_nPerfFreq = 0;
#endif

//---------------------------------------------------------------------------

static bool g_bPriorityNormal = true;

// Make APPLEWIN process higher priority
void SetPriorityAboveNormal()
{
	if (!g_bPriorityNormal)
		return;

	if ( SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS) )
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
		g_bPriorityNormal = false;
	}
}

// Make APPLEWIN process normal priority
void SetPriorityNormal()
{
	if (g_bPriorityNormal)
		return;

	if ( SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS) )
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
		g_bPriorityNormal = true;
	}
}

//---------------------------------------------------------------------------

void ContinueExecution()
{
	static BOOL pageflipping    = 0; //?

	const double fUsecPerSec        = 1.e6;
#if 1
	const UINT nExecutionPeriodUsec = 1000;		// 1.0ms
//	const UINT nExecutionPeriodUsec = 100;		// 0.1ms
	const double fExecutionPeriodClks = g_fCurrentCLK6502 * ((double)nExecutionPeriodUsec / fUsecPerSec);
#else
	const double fExecutionPeriodClks = 1800.0;
	const UINT nExecutionPeriodUsec = (UINT) (fUsecPerSec * (fExecutionPeriodClks / g_fCurrentCLK6502));
#endif

	//

	bool bScrollLock_FullSpeed = g_uScrollLockToggle
									? g_bScrollLock_FullSpeed
									: (GetKeyState(VK_SCROLL) < 0);

	g_bFullSpeed = ( (g_dwSpeed == SPEED_MAX) || 
					 bScrollLock_FullSpeed ||
					 (DiskIsSpinning() && enhancedisk && !Spkr_IsActive() && !MB_IsActive()) );

	if(g_bFullSpeed)
	{
		// Don't call Spkr_Mute() - will get speaker clicks
		MB_Mute();
		SysClk_StopTimer();
#ifdef USE_SPEECH_API
		g_Speech.Reset();			// TODO: Put this on a timer (in emulated cycles)... otherwise CATALOG cuts out
#endif

		g_nCpuCyclesFeedback = 0;	// For the case when this is a big -ve number

		// Switch to normal priority so that APPLEWIN process doesn't hog machine!
		//. EG: No disk in Drive-1, and boot Apple: Windows will start to crawl!
		SetPriorityNormal();
	}
	else
	{
		// Don't call Spkr_Demute()
		MB_Demute();
		SysClk_StartTimerUsec(nExecutionPeriodUsec);

		// Switch to higher priority, eg. for audio (BUG #015394)
		SetPriorityAboveNormal();
	}

	//

	int nCyclesToExecute = (int) fExecutionPeriodClks + g_nCpuCyclesFeedback;
	if(nCyclesToExecute < 0)
		nCyclesToExecute = 0;

	DWORD dwExecutedCycles = CpuExecute(nCyclesToExecute);
	g_dwCyclesThisFrame += dwExecutedCycles;

	//

	cyclenum = dwExecutedCycles;

	DiskUpdatePosition(dwExecutedCycles);
	JoyUpdatePosition();

	SpkrUpdate(cyclenum);
	sg_SSC.CommUpdate(cyclenum);
	PrintUpdate(cyclenum);

	//

	const DWORD CLKS_PER_MS = (DWORD)g_fCurrentCLK6502 / 1000;

	emulmsec_frac += dwExecutedCycles;
	if(emulmsec_frac > CLKS_PER_MS)
	{
		emulmsec += emulmsec_frac / CLKS_PER_MS;
		emulmsec_frac %= CLKS_PER_MS;
	}

	//
	// DETERMINE WHETHER THE SCREEN WAS UPDATED, THE DISK WAS SPINNING,
	// OR THE KEYBOARD I/O PORTS WERE BEING EXCESSIVELY QUERIED THIS CLOCKTICK
	VideoCheckPage(0);
	BOOL screenupdated = VideoHasRefreshed();
	BOOL systemidle    = 0;	//(KeybGetNumQueries() > (clockgran << 2));	//  && (!ranfinegrain);	// TO DO

	if(screenupdated)
		pageflipping = 3;

	//

	if(g_dwCyclesThisFrame >= dwClksPerFrame)
	{
		g_dwCyclesThisFrame -= dwClksPerFrame;

		if(g_nAppMode != MODE_LOGO)
		{
			VideoUpdateFlash();

			static BOOL  anyupdates     = 0;
			static DWORD lastcycles     = 0;
			static BOOL  lastupdates[2] = {0,0};

			anyupdates |= screenupdated;

			//

			lastcycles = cumulativecycles;
			if ((!anyupdates) && (!lastupdates[0]) && (!lastupdates[1]) && VideoApparentlyDirty())
			{
				VideoCheckPage(1);
				static DWORD lasttime = 0;
				DWORD currtime = GetTickCount();
				if ((!g_bFullSpeed) ||
					(currtime-lasttime >= (DWORD)((g_bGraphicsMode || !systemidle) ? 100 : 25)))
				{
					VideoRefreshScreen();
					lasttime = currtime;
				}
				screenupdated = 1;
			}

			lastupdates[1] = lastupdates[0];
			lastupdates[0] = anyupdates;
			anyupdates     = 0;

			if (pageflipping)
				pageflipping--;
		}

		MB_EndOfVideoFrame();
	}

	//

	if(!g_bFullSpeed)
	{
		SysClk_WaitTimer();

#if DBG_CALC_FREQ
		if(g_nPerfFreq)
		{
			QueryPerformanceCounter((LARGE_INTEGER*)&nTime1);
			LONGLONG nTimeDiff = nTime1 - nTime0;
			double fTime = (double)nTimeDiff / (double)(LONGLONG)g_nPerfFreq;

			g_fDbg[g_nIdx] = fTime;
			g_nIdx = (g_nIdx+1) & (MAX_CNT-1);
			g_fMeanPeriod = 0.0;
			for(UINT n=0; n<MAX_CNT; n++)
				g_fMeanPeriod += g_fDbg[n];
			g_fMeanPeriod /= (double)MAX_CNT;
			g_fMeanFreq = 1.0 / g_fMeanPeriod;
		}
#endif
	}
}

//===========================================================================

void SetCurrentCLK6502()
{
	static DWORD dwPrevSpeed = (DWORD) -1;

	if(dwPrevSpeed == g_dwSpeed)
		return;

	dwPrevSpeed = g_dwSpeed;

	// SPEED_MIN    =  0 = 0.50 MHz
	// SPEED_NORMAL = 10 = 1.00 MHz
	//                20 = 2.00 MHz
	// SPEED_MAX-1  = 39 = 3.90 MHz
	// SPEED_MAX    = 40 = ???? MHz (run full-speed, /g_fCurrentCLK6502/ is ignored)

	if(g_dwSpeed < SPEED_NORMAL)
		g_fMHz = 0.5 + (double)g_dwSpeed * 0.05;
	else
		g_fMHz = (double)g_dwSpeed / 10.0;

	g_fCurrentCLK6502 = CLK_6502 * g_fMHz;

	//
	// Now re-init modules that are dependent on /g_fCurrentCLK6502/
	//

	SpkrReinitialize();
	MB_Reinitialize();
}

//===========================================================================
LRESULT CALLBACK DlgProc (HWND   window,
                          UINT   message,
                          WPARAM wparam,
                          LPARAM lparam) {
  if (message == WM_CREATE) {
    RECT rect;
    GetWindowRect(window,&rect);
    SIZE size;
    size.cx = rect.right-rect.left;
    size.cy = rect.bottom-rect.top;
    rect.left   = (GetSystemMetrics(SM_CXSCREEN)-size.cx) >> 1;
    rect.top    = (GetSystemMetrics(SM_CYSCREEN)-size.cy) >> 1;
    rect.right  = rect.left+size.cx;
    rect.bottom = rect.top +size.cy;
    MoveWindow(window,
	       rect.left,
	       rect.top,
	       rect.right-rect.left,
	       rect.bottom-rect.top,
	       0);
    ShowWindow(window,SW_SHOW);
  }
  return DefWindowProc(window,message,wparam,lparam);
}

//===========================================================================
void EnterMessageLoop ()
{
	MSG message;

	PeekMessage(&message, NULL, 0, 0, PM_NOREMOVE);

	while (message.message!=WM_QUIT)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);

			while ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_STEPPING))
			{
				if (PeekMessage(&message,0,0,0,PM_REMOVE))
				{
					if (message.message == WM_QUIT)
						return;

					TranslateMessage(&message);
					DispatchMessage(&message);
				}
				else if (g_nAppMode == MODE_STEPPING)
				{
					DebugContinueStepping();
				}
				else
				{
					ContinueExecution();
					if (g_nAppMode != MODE_DEBUG)
					{
						if (g_bFullSpeed)
							ContinueExecution();
					}
				}
			}
		}
		else
		{
			if (g_nAppMode == MODE_DEBUG)
				DebuggerUpdate();
			else if ((g_nAppMode == MODE_LOGO) || (g_nAppMode == MODE_PAUSED))
				Sleep(100);		// Stop process hogging CPU
		}
	}
}

//===========================================================================
void GetProgramDirectory () {
  GetModuleFileName((HINSTANCE)0,g_sProgramDir,MAX_PATH);
  g_sProgramDir[MAX_PATH-1] = 0;
  int loop = _tcslen(g_sProgramDir);
  while (loop--)
    if ((g_sProgramDir[loop] == TEXT('\\')) ||
        (g_sProgramDir[loop] == TEXT(':'))) {
      g_sProgramDir[loop+1] = 0;
      break;
    }
}

//===========================================================================
//Reads configuration from the registry entries
void LoadConfiguration ()
{
  DWORD dwComputerType;

  if (REGLOAD(TEXT(REGVALUE_APPLE2_TYPE),&dwComputerType))
  {
	REGLOAD(TEXT(REGVALUE_CLONETYPE),&g_uCloneType);
	if ((dwComputerType >= A2TYPE_MAX) || (dwComputerType >= A2TYPE_UNDEFINED && dwComputerType < A2TYPE_CLONE))
		dwComputerType = A2TYPE_APPLE2EEHANCED;

	if (dwComputerType == A2TYPE_CLONE)
	  {
		switch (g_uCloneType)
		{
		case 0:	g_Apple2Type = A2TYPE_PRAVETS82; break;
		case 1:	g_Apple2Type = A2TYPE_PRAVETS8M; break;
		case 2:	g_Apple2Type = A2TYPE_PRAVETS8A; break;
		default:	g_Apple2Type = A2TYPE_APPLE2EEHANCED; break;
		}
	  }	
	else
	{
		g_Apple2Type = (eApple2Type) dwComputerType;
	}
  }
  else	// Support older AppleWin registry entries
  {
	  REGLOAD(TEXT("Computer Emulation"),&dwComputerType);
	  switch (dwComputerType)
	  {
      // NB. No A2TYPE_APPLE2E (this is correct)
	  case 0:	g_Apple2Type = A2TYPE_APPLE2;
	  case 1:	g_Apple2Type = A2TYPE_APPLE2PLUS;
	  case 2:	g_Apple2Type = A2TYPE_APPLE2EEHANCED;
	  default:	g_Apple2Type = A2TYPE_APPLE2EEHANCED;
	  }
  }

	switch (g_Apple2Type) //Sets the character set for the Apple model/clone
	{
	case A2TYPE_APPLE2:			g_nCharsetType  = 0; break; 
	case A2TYPE_APPLE2PLUS:		g_nCharsetType  = 0; break; 
	case A2TYPE_APPLE2E:		g_nCharsetType  = 0; break; 
	case A2TYPE_APPLE2EEHANCED:	g_nCharsetType  = 0; break; 
	case A2TYPE_PRAVETS82:	    g_nCharsetType  = 1; break; 
	case A2TYPE_PRAVETS8A:	    g_nCharsetType  = 2; break; 
	case A2TYPE_PRAVETS8M:	    g_nCharsetType  = 3; break; //This charset has a very small difference with the PRAVETS82 one an probably has some misplaced characters. Still the Pravets82 charset is used, because settiong charset to 3 results in some problems.
	}


  REGLOAD(TEXT("Joystick 0 Emulation"),&joytype[0]);
  REGLOAD(TEXT("Joystick 1 Emulation"),&joytype[1]);
  REGLOAD(TEXT("Sound Emulation")     ,&soundtype);

  char aySerialPortName[ CSuperSerialCard::SIZEOF_SERIALCHOICE_ITEM ];
  if (RegLoadString(	TEXT("Configuration"),
						TEXT(REGVALUE_SERIAL_PORT_NAME),
						TRUE,
						aySerialPortName,
						sizeof(aySerialPortName) ) )
  {
	sg_SSC.SetSerialPortName(aySerialPortName);
  }

  REGLOAD(TEXT("Emulation Speed")   ,&g_dwSpeed);
  REGLOAD(TEXT(REGVALUE_ENHANCE_DISK_SPEED),(DWORD *)&enhancedisk);

  Config_Load_Video();

  REGLOAD(TEXT("Uthernet Active")   ,(DWORD *)&tfe_enabled);

  SetCurrentCLK6502();

  //

  DWORD dwTmp;

  if(REGLOAD(TEXT(REGVALUE_THE_FREEZES_F8_ROM), &dwTmp))
	  g_uTheFreezesF8Rom = dwTmp;

  if(REGLOAD(TEXT(REGVALUE_SPKR_VOLUME), &dwTmp))
      SpkrSetVolume(dwTmp, PSP_GetVolumeMax());

  if(REGLOAD(TEXT(REGVALUE_MB_VOLUME), &dwTmp))
      MB_SetVolume(dwTmp, PSP_GetVolumeMax());

  if(REGLOAD(TEXT(REGVALUE_SOUNDCARD_TYPE), &dwTmp))
	  MB_SetSoundcardType((eSOUNDCARDTYPE)dwTmp);

  if(REGLOAD(TEXT(REGVALUE_SAVE_STATE_ON_EXIT), &dwTmp))
	  g_bSaveStateOnExit = dwTmp ? true : false;


  if(REGLOAD(TEXT(REGVALUE_DUMP_TO_PRINTER), &dwTmp))
	g_bDumpToPrinter = dwTmp ? true : false;

  if(REGLOAD(TEXT(REGVALUE_CONVERT_ENCODING), &dwTmp))
    g_bConvertEncoding = dwTmp ? true : false;

  if(REGLOAD(TEXT(REGVALUE_FILTER_UNPRINTABLE), &dwTmp))
    g_bFilterUnprintable = dwTmp ? true : false;

  if(REGLOAD(TEXT(REGVALUE_PRINTER_APPEND), &dwTmp))
    g_bPrinterAppend = dwTmp ? true : false;


  if(REGLOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp))
	  HD_SetEnabled(dwTmp ? true : false);

  char szHDVPathname[MAX_PATH] = {0};
  if(RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_1), 1, szHDVPathname, sizeof(szHDVPathname)))
	  HD_InsertDisk(HARDDISK_1, szHDVPathname);
  if(RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_2), 1, szHDVPathname, sizeof(szHDVPathname)))
	  HD_InsertDisk(HARDDISK_2, szHDVPathname);

  if(REGLOAD(TEXT(REGVALUE_PDL_XTRIM), &dwTmp))
      JoySetTrim((short)dwTmp, true);
  if(REGLOAD(TEXT(REGVALUE_PDL_YTRIM), &dwTmp))
      JoySetTrim((short)dwTmp, false);

  if(REGLOAD(TEXT(REGVALUE_SCROLLLOCK_TOGGLE), &dwTmp))
	  g_uScrollLockToggle = dwTmp;

  if(REGLOAD(TEXT(REGVALUE_MOUSE_IN_SLOT4), &dwTmp))
	  g_uMouseInSlot4 = dwTmp;
  if(REGLOAD(TEXT(REGVALUE_MOUSE_CROSSHAIR), &dwTmp))
	  g_uMouseShowCrosshair = dwTmp;
  if(REGLOAD(TEXT(REGVALUE_MOUSE_RESTRICT_TO_WINDOW), &dwTmp))
	  g_uMouseRestrictToWindow = dwTmp;

#ifdef SUPPORT_CPM
  if(REGLOAD(TEXT(REGVALUE_Z80_IN_SLOT5), &dwTmp))
	  g_uZ80InSlot5 = dwTmp;

  if (g_uZ80InSlot5)
	  MB_SetSoundcardType(SC_NONE);

//	g_Slot4 = 
//	g_uMouseInSlot4	? CT_MouseInterface
//					: g_uZ80InSlot5	? CT_Empty
//									: CT_Mockingboard;
////									: g_uClockInSlot4	? CT_GenericClock
////														: CT_Mockingboard;
#else
	g_Slot4 = g_uMouseInSlot4
				? CT_MouseInterface
				: CT_Mockingboard;
#endif

  //

	char szFilename[MAX_PATH] = {0};
	RegLoadString(TEXT(REG_CONFIG),TEXT(REGVALUE_SAVESTATE_FILENAME),1,szFilename,sizeof(szFilename));
	Snapshot_SetFilename(szFilename);	// If not in Registry than default will be used

	szFilename[0] = 0;
	RegLoadString(TEXT(REG_CONFIG),TEXT(REGVALUE_PRINTER_FILENAME),1,szFilename,sizeof(szFilename));
	Printer_SetFilename(szFilename);	// If not in Registry than default will be used

	// Current/Starting Dir is the "root" of where the user keeps his disk images
	RegLoadString(TEXT(REG_PREFS),TEXT(REGVALUE_PREF_START_DIR),1,g_sCurrentDir,MAX_PATH);
	SetCurrentImageDir();

	Disk_LoadLastDiskImage(DRIVE_1);
	Disk_LoadLastDiskImage(DRIVE_2);

	dwTmp = 10;
	REGLOAD(TEXT(REGVALUE_PRINTER_IDLE_LIMIT), &dwTmp);
	Printer_SetIdleLimit(dwTmp);

	char szUthernetInt[MAX_PATH] = {0};
	RegLoadString(TEXT(REG_CONFIG),TEXT("Uthernet Interface"),1,szUthernetInt,MAX_PATH);  
	update_tfe_interface(szUthernetInt,NULL);
}

//===========================================================================

void SetCurrentImageDir()
{
	SetCurrentDirectory(g_sCurrentDir);
}

//===========================================================================

// TODO: Added dialog option of which file extensions to registry
bool g_bRegisterFileTypes = true;
//bool g_bRegistryFileBin = false;
bool g_bRegistryFileDo  = true;
bool g_bRegistryFileDsk = true;
bool g_bRegistryFileNib = true;
bool g_bRegistryFilePo  = true;


void RegisterExtensions()
{
	TCHAR szCommandTmp[MAX_PATH];
	GetModuleFileName((HMODULE)0,szCommandTmp,MAX_PATH);

#ifdef TEST_REG_BUG
	TCHAR command[MAX_PATH];
	wsprintf(command, "%s",	szCommandTmp);	// Wrap	path & filename	in quotes &	null terminate

	TCHAR icon[MAX_PATH];
	wsprintf(icon,TEXT("\"%s,1\""),(LPCTSTR)command);
#endif

	TCHAR command[MAX_PATH];
	wsprintf(command, "\"%s\"",	szCommandTmp);	// Wrap	path & filename	in quotes &	null terminate

	TCHAR icon[MAX_PATH];
	wsprintf(icon,TEXT("%s,1"),(LPCTSTR)command);

	_tcscat(command,TEXT(" \"%1\""));			// Append "%1"
//	_tcscat(command,TEXT("-d1 %1\""));			// Append "%1"
//	sprintf(command, "\"%s\" \"-d1 %%1\"", szCommandTmp);	// Wrap	path & filename	in quotes &	null terminate

	// NB. Reflect extensions in DELREG.INF
//	RegSetValue(HKEY_CLASSES_ROOT,".bin",REG_SZ,"DiskImage",10);	// Removed as .bin is too generic
	long Res = RegDeleteValue(HKEY_CLASSES_ROOT, ".bin");			// TODO: This isn't working :-/

	RegSetValue(HKEY_CLASSES_ROOT,".do"	,REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".dsk",REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".nib",REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".po"	,REG_SZ,"DiskImage",10);
//	RegSetValue(HKEY_CLASSES_ROOT,".2mg",REG_SZ,"DiskImage",10);	// Don't grab this, as not all .2mg images are supported (so defer to CiderPress)
//	RegSetValue(HKEY_CLASSES_ROOT,".2img",REG_SZ,"DiskImage",10);	// Don't grab this, as not all .2mg images are supported (so defer to CiderPress)
//	RegSetValue(HKEY_CLASSES_ROOT,".aws",REG_SZ,"DiskImage",10);	// TO DO
//	RegSetValue(HKEY_CLASSES_ROOT,".hdv",REG_SZ,"DiskImage",10);	// TO DO

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage",
				REG_SZ,"Disk Image",21);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\DefaultIcon",
				REG_SZ,icon,_tcslen(icon)+1);

// This key can interfere....
// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExt\.dsk

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\command",
				REG_SZ,command,_tcslen(command)+1);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\ddeexec",
				REG_SZ,"%1",3);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\ddeexec\\application",
				REG_SZ,"applewin",_tcslen("applewin")+1);
//				REG_SZ,szCommandTmp,_tcslen(szCommandTmp)+1);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\ddeexec\\topic",
				REG_SZ,"system",_tcslen("system")+1);
}

//===========================================================================
void AppleWin_RegisterHotKeys()
{
	BOOL bStatus = true;
	
	bStatus &= RegisterHotKey(      
		g_hFrameWindow	, // HWND hWnd
		VK_SNAPSHOT_560	, // int id (user/custom id)
		0					, // UINT fsModifiers
		VK_SNAPSHOT		  // UINT vk = PrintScreen
	);

	bStatus &= RegisterHotKey(      
		g_hFrameWindow	, // HWND hWnd
		VK_SNAPSHOT_280, // int id (user/custom id)
		MOD_SHIFT		, // UINT fsModifiers
		VK_SNAPSHOT		  // UINT vk = PrintScreen
	);

	if (! bStatus)
	{
		MessageBox( g_hFrameWindow, "Unable to capture PrintScreen key", "Warning", MB_OK );
	}
}

//===========================================================================

LPSTR GetCurrArg(LPSTR lpCmdLine)
{
	if(*lpCmdLine == '\"')
		lpCmdLine++;

	return lpCmdLine;
}

LPSTR GetNextArg(LPSTR lpCmdLine)
{
	int bInQuotes = 0;

	while(*lpCmdLine)
	{
		if(*lpCmdLine == '\"')
		{
			bInQuotes ^= 1;
			if(!bInQuotes)
			{
				*lpCmdLine++ = 0x00;	// Assume end-quote is end of this arg
				continue;
			}
		}

		if((*lpCmdLine == ' ') && !bInQuotes)
		{
			*lpCmdLine++ = 0x00;
			break;
		}

		lpCmdLine++;
	}

	return lpCmdLine;
}

//---------------------------------------------------------------------------

static int DoDiskInsert(const int nDrive, LPCSTR szFileName)
{
	string strPathName;

	if (szFileName[0] == '\\' || szFileName[1] == ':')
	{
		// Abs pathname
		strPathName = szFileName;
	}
	else
	{
		// Rel pathname
		char szCWD[_MAX_PATH] = {0};
		if (!GetCurrentDirectory(sizeof(szCWD), szCWD))
			return false;

		strPathName = szCWD;
		strPathName.append("\\");
		strPathName.append(szFileName);
	}

	ImageError_e Error = DiskInsert(nDrive, strPathName.c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
	return Error == eIMAGE_ERROR_NONE;
}

//---------------------------------------------------------------------------

int APIENTRY WinMain (HINSTANCE passinstance, HINSTANCE, LPSTR lpCmdLine, int)
{
	bool bSetFullScreen = false;
	bool bBoot = false;
	LPSTR szImageName_drive1 = NULL;
	LPSTR szImageName_drive2 = NULL;

	while(*lpCmdLine)
	{
		LPSTR lpNextArg = GetNextArg(lpCmdLine);

		if(strcmp(lpCmdLine, "-noreg") == 0)
		{
			g_bRegisterFileTypes = false;
		}
		else if(strcmp(lpCmdLine, "-d1") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			szImageName_drive1 = lpCmdLine;
		}
		else if(strcmp(lpCmdLine, "-d2") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			szImageName_drive2 = lpCmdLine;
		}
		else if(strcmp(lpCmdLine, "-f") == 0)
		{
			bSetFullScreen = true;
		}
		else if(((strcmp(lpCmdLine, "-l") == 0) || (strcmp(lpCmdLine, "-log") == 0)) && (g_fh == NULL))
		{
			g_fh = fopen("AppleWin.log", "a+t");	// Open log file (append & text mode)
			CHAR aDateStr[80], aTimeStr[80];
			GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, (LPTSTR)aDateStr, sizeof(aDateStr));
			GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, (LPTSTR)aTimeStr, sizeof(aTimeStr));
			fprintf(g_fh,"*** Logging started: %s %s\n",aDateStr,aTimeStr);
		}
		else if(strcmp(lpCmdLine, "-m") == 0)
		{
			g_bDisableDirectSound = true;
		}
		else if(strcmp(lpCmdLine, "-no-mb") == 0)
		{
			g_bDisableDirectSoundMockingboard = true;
		}
#ifdef RAMWORKS
		else if(strcmp(lpCmdLine, "-r") == 0)		// RamWorks size [1..127]
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_uMaxExPages = atoi(lpCmdLine);
			if (g_uMaxExPages > 127)
				g_uMaxExPages = 128;
			else if (g_uMaxExPages < 1)
				g_uMaxExPages = 1;
		}
#endif
		else if(strcmp(lpCmdLine, "-f8rom") == 0)		// Use custom 2K ROM at [$F800..$FFFF]
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_hCustomRomF8 = CreateFile(lpCmdLine, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
			if ((g_hCustomRomF8 == INVALID_HANDLE_VALUE) || (GetFileSize(g_hCustomRomF8, NULL) != 0x800))
				g_bCustomRomF8Failed = true;
		}
		else if(strcmp(lpCmdLine, "-printscreen") == 0)		// Turn on display of the last filename print screen was saved to
		{
			g_bDisplayPrintScreenFileName = true;
		}
		else if(strcmp(lpCmdLine, "-spkr-inc") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			const int nErrorInc = atoi(lpCmdLine);
			SoundCore_SetErrorInc( nErrorInc );
		}
		else if(strcmp(lpCmdLine, "-spkr-max") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			const int nErrorMax = atoi(lpCmdLine);
			SoundCore_SetErrorMax( nErrorMax );
		}
		else if(strcmp(lpCmdLine, "-use-real-printer") == 0)	// Enable control in Advanced config to allow dumping to a real printer
		{
			g_bEnableDumpToRealPrinter = true;
		}
		else if(strcmp(lpCmdLine, "-speech") == 0)
		{
			g_bEnableSpeech = true;
		}
		else if(strcmp(lpCmdLine,"-multimon") == 0)
		{
			g_bMultiMon = true;
		}
		lpCmdLine = lpNextArg;
	}

#if 0
#ifdef RIFF_SPKR
	RiffInitWriteFile("Spkr.wav", SPKR_SAMPLE_RATE, 1);
#endif
#ifdef RIFF_MB
	RiffInitWriteFile("Mockingboard.wav", 44100, 2);
#endif
#endif

	//-----

    char szPath[_MAX_PATH];

    if(0 == GetModuleFileName(NULL, szPath, sizeof(szPath)))
    {
        strcpy(szPath, __argv[0]);
    }

    // Extract application version and store in a global variable
    DWORD dwHandle, dwVerInfoSize;

    dwVerInfoSize = GetFileVersionInfoSize(szPath, &dwHandle);

    if(dwVerInfoSize > 0)
    {
        char* pVerInfoBlock = new char[dwVerInfoSize];

        if(GetFileVersionInfo(szPath, NULL, dwVerInfoSize, pVerInfoBlock))
        {
            VS_FIXEDFILEINFO* pFixedFileInfo;
            UINT pFixedFileInfoLen;

            VerQueryValue(pVerInfoBlock, TEXT("\\"), (LPVOID*) &pFixedFileInfo, (PUINT) &pFixedFileInfoLen);

            // Construct version string from fixed file info block

            unsigned long major     = pFixedFileInfo->dwFileVersionMS >> 16;
            unsigned long minor     = pFixedFileInfo->dwFileVersionMS & 0xffff;
            unsigned long fix       = pFixedFileInfo->dwFileVersionLS >> 16;
			unsigned long fix_minor = pFixedFileInfo->dwFileVersionLS & 0xffff;

            sprintf(VERSIONSTRING, "%d.%d.%d.%d", major, minor, fix, fix_minor); // potential buffer overflow
        }
    }

#if DBG_CALC_FREQ
	QueryPerformanceFrequency((LARGE_INTEGER*)&g_nPerfFreq);
	if(g_fh) fprintf(g_fh, "Performance frequency = %d\n",g_nPerfFreq);
#endif

	//-----

	// Initialize COM - so we can use CoCreateInstance
	// . NB. DSInit() & DIMouse::DirectInputInit are done when g_hFrameWindow is created (WM_CREATE)
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	bool bSysClkOK = SysClk_InitTimer();
#ifdef USE_SPEECH_API
	if (g_bEnableSpeech)
	{
		bool bSpeechOK = g_Speech.Init();
	}
#endif

	// DO ONE-TIME INITIALIZATION
	g_hInstance = passinstance;
	MemPreInitialize();		// Call before any of the slot devices are initialized
	GdiSetBatchLimit(512);
	GetProgramDirectory();
	if( g_bRegisterFileTypes )
	{
		RegisterExtensions();
	}
	FrameRegisterClass();
	ImageInitialize();
	DiskInitialize();
	CreateColorMixMap();	// For tv emulation mode
	g_pEcho = new CEcho(SPKR_SAMPLE_RATE);

	int nError = 0;	// TODO: Show error MsgBox if we get a DiskInsert error
	if(szImageName_drive1)
	{
		nError = DoDiskInsert(DRIVE_1, szImageName_drive1);
		FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
		bBoot = true;
	}
	if(szImageName_drive2)
	{
		nError |= DoDiskInsert(DRIVE_2, szImageName_drive2);
	}

	//

	do
	{
		// DO INITIALIZATION THAT MUST BE REPEATED FOR A RESTART
		restart = 0;
		g_nAppMode = MODE_LOGO;
		LoadConfiguration();
		DebugInitialize();
		JoyInitialize();
		MemInitialize();
		VideoInitialize(); // g_pFramebufferinfo been created now
		FrameCreateWindow();
		// PrintScrn support
		AppleWin_RegisterHotKeys(); // needs valid g_hFrameWindow

		// Need to test if it's safe to call ResetMachineState(). In the meantime, just call DiskReset():
		DiskReset();	// Switch from a booting A][+ to a non-autostart A][, so need to turn off floppy motor

		if (!bSysClkOK)
		{
			MessageBox(g_hFrameWindow, "DirectX failed to create SystemClock instance", TEXT("AppleWin Error"), MB_OK);
			PostMessage(g_hFrameWindow, WM_DESTROY, 0, 0);	// Close everything down
		}

		if (g_bCustomRomF8Failed)
		{
			MessageBox(g_hFrameWindow, "Failed to load custom F8 rom (not found or not exactly 2KB)", TEXT("AppleWin Error"), MB_OK);
			PostMessage(g_hFrameWindow, WM_DESTROY, 0, 0);	// Close everything down
		}

		tfe_init();
		Snapshot_Startup();		// Do this after everything has been init'ed

		if(bSetFullScreen)
		{
			PostMessage(g_hFrameWindow, WM_KEYDOWN, VK_F1+BTN_FULLSCR, 0);
			PostMessage(g_hFrameWindow, WM_KEYUP,   VK_F1+BTN_FULLSCR, 0);
			bSetFullScreen = false;
		}

		if(bBoot)
		{
			PostMessage(g_hFrameWindow, WM_USER_BOOT, 0, 0);
			bBoot = false;
		}

		// ENTER THE MAIN MESSAGE LOOP
		EnterMessageLoop();

		MB_Reset();
		sg_Mouse.Uninitialize();	// Maybe restarting due to switching slot-4 card from mouse to MB
	}
	while (restart);
	
	// Release COM
	DSUninit();
	SysClk_UninitTimer();
	CoUninitialize();
	
	tfe_shutdown();
	
	if	(g_fh)
	{
		fprintf(g_fh,"*** Logging ended\n\n");
		fclose(g_fh);
	}

	RiffFinishWriteFile();

	if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
		CloseHandle(g_hCustomRomF8);

	delete g_pEcho;

	return 0;
}
