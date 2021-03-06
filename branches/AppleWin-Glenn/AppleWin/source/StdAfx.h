//#define WIN32_LEAN_AND_MEAN
//#define _WIN32_WINNT 0x0400	// For CoInitializeEx() to get defined in objbase.h (Updated[TC]: Removed as not needed)

// Mouse Wheel is not supported on Win95.
// If we didn't care about supporting Win95 (compile/run-time errors)
// we would just define the minimum windows version to support.
// #define _WIN32_WINDOWS  0x0401
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif

// Not needed in VC7.1, but needed in VC Express
#include <tchar.h> 

#include <assert.h>
#include <crtdbg.h>
#include <dsound.h>
#include <dshow.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <windows.h>
#include <winuser.h> // WM_MOUSEWHEEL
#include <commctrl.h>
#include <ddraw.h>
#include <htmlhelp.h>
#include <assert.h>

#include <queue>
#include <vector>

#include "zlib.h"
#include "unzip.h"
#include "zip.h"
#include "iowin32.h"

#include "Common.h"
#include "Structs.h"

#include "slot7.h"
#include "AppleSPI.h"
#include "AppleWin.h"
#include "AY8910.h"
#include "CPU.h"
#include "Debug.h"
#include "Disk.h"
#include "Frame.h"
#include "Joystick.h"
#include "Keyboard.h"
#include "Log.h"
#include "Memory.h"
#include "Mockingboard.h"
#include "ParallelPrinter.h"
#include "Peripheral_Clock_Generic.h"
#include "PropertySheetPage.h"
#include "Registry.h"
#include "Riff.h"
#include "SaveState.h"
#include "SerialComms.h"
#include "SoundCore.h"
#include "Speaker.h"
#include "Tape.h"
#include "Tfe/Tfe.h"
#include "Video.h"
