#pragma once

BOOL    RegLoadString (LPCTSTR,LPCTSTR,BOOL,LPTSTR,DWORD);
BOOL    RegLoadValue (LPCTSTR,LPCTSTR,BOOL,DWORD *);
void    RegSaveString (LPCTSTR,LPCTSTR,BOOL,LPCTSTR);
void    RegSaveValue (LPCTSTR,LPCTSTR,BOOL,DWORD);
