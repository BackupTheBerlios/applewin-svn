#pragma once

#define  TRACKS      35
#define  IMAGETYPES  7
#define  NIBBLES     6656

BOOL    ImageBoot (HIMAGE);
void    ImageClose (HIMAGE);
void    ImageDestroy ();
void    ImageInitialize ();

enum ImageError_e
{
	IMAGE_ERROR_BAD_POINTER    =-1,
	IMAGE_ERROR_NONE           = 0,
	IMAGE_ERROR_UNABLE_TO_OPEN = 1,
	IMAGE_ERROR_BAD_SIZE       = 2
};

//int     ImageOpen (LPCTSTR,HIMAGE *,BOOL *,BOOL);
int ImageOpen (LPCTSTR imagefilename, HIMAGE *hDiskImage_, BOOL *pWriteProtected_, BOOL bCreateIfNecessary );

void    ImageReadTrack (HIMAGE,int,int,LPBYTE,int *);
void    ImageWriteTrack (HIMAGE,int,int,LPBYTE,int);
