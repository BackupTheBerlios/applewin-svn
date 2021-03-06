#pragma once

enum {NOT_ASCII=0, ASCII};

// 3D Border
#define  VIEWPORTX   5
#define  VIEWPORTY   5

// Win32
extern HWND       g_hFrameWindow;
extern HDC        g_hFrameDC;

extern BOOL       fullscreen;

void    FrameCreateWindow ();
HDC     FrameGetDC ();
HDC     FrameGetVideoDC (LPBYTE *,LONG *);
void    FrameRefreshStatus (int);
void    FrameRegisterClass ();
void    FrameReleaseDC ();
void    FrameReleaseVideoDC ();

LRESULT CALLBACK FrameWndProc (
	HWND   window,
	UINT   message,
	WPARAM wparam,
	LPARAM lparam );
