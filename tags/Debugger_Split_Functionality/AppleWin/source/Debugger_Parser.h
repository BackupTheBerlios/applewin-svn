#ifndef DEBUGGER_PARSER_H
#define DEBUGGER_PARSER_H

	#define CHAR_LF     '\x0D'
	#define CHAR_CR     '\x0A'
	#define CHAR_SPACE  ' '
	#define CHAR_TAB    '\t'
	#define CHAR_QUOTED '"' 
	#define CHAR_ESCAPE '\x1B'

// Globals __________________________________________________________________

	extern	int   g_nArgRaw;
	extern	Arg_t g_aArgRaw[ MAX_ARGS ]; // pre-processing
	extern	Arg_t g_aArgs  [ MAX_ARGS ]; // post-processing

	extern	const TCHAR * g_pConsoleFirstArg; //    = 0; // points to first arg

	extern	const TokenTable_t g_aTokens[ NUM_TOKENS ];

	extern	const TCHAR TCHAR_LF    ;//= 0x0D;
	extern	const TCHAR TCHAR_CR    ;//= 0x0A;
	extern	const TCHAR TCHAR_SPACE ;//= TEXT(' ');
	extern	const TCHAR TCHAR_TAB   ;//= TEXT('\t');
	extern	const TCHAR TCHAR_QUOTED;//= TEXT('"' );

// Prototypes _______________________________________________________________

// Arg - Command Processing
	Update_t Help_Arg_1( int iCommandHelp );
	int _Arg_1     ( int nValue );
	int _Arg_1     ( LPTSTR pName );
	int _Arg_Shift ( int iSrc, int iEnd, int iDst = 0 );
	void ArgsClear ();

	bool ArgsGetValue ( Arg_t *pArg, WORD * pAddressValue_, const int nBase = 16 );
	bool ArgsGetImmediateValue ( Arg_t *pArg, WORD * pAddressValue_ );
	int	 ArgsGet ( TCHAR * pInput );
	bool ArgsGetRegisterValue ( Arg_t *pArg, WORD * pAddressValue_ );
	void ArgsRawParse ( void );
	int  ArgsCook ( const int nArgs ); // ArgsRawCook

// Token
	const char * ParserFindToken( const char *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e * pToken_ );

// Text Util
/*
inline	const char* SkipEOL                    ( const char *pSrc )
	{
		while (pSrc && ((*pSrc == CHAR_LF) || (*pSrc == CHAR_CR)))
		if (pSrc)
		{
				pSrc++;
		}
		return pSrc;
	}
*/
inline	const char* EatEOL                    ( const char *pSrc )
	{
		if (pSrc)
		{
			if (*pSrc == CHAR_LF)
				pSrc++;
				
			if (*pSrc == CHAR_CR)
				pSrc++;
		}
		return pSrc;
	}
inline	const char* SkipWhiteSpace             ( const char *pSrc )
	{
		while (pSrc && ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB)))
		{
			pSrc++;
		}
		return pSrc;
	}
inline	const char* SkipWhiteSpaceReverse      ( const char *pSrc, const char *pStart )
	{
		while (pSrc && ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB)) && (pSrc > pStart))
		{
			pSrc--;
		}
		return pSrc;
	}


inline	const char* SkipUntilChar              ( const char *pSrc, const char nDelim )
{
	while (pSrc && (*pSrc))
	{
		if (*pSrc == nDelim)
			break;
		pSrc++;
	}
	return pSrc;
}
inline	const char* SkipUntilEOL               ( const char *pSrc )
	{
		// EOL delims: NULL, LF, CR
		while (pSrc && (*pSrc))
		{
			if ((*pSrc == CHAR_LF) || (*pSrc == CHAR_CR))
			{
				break;
			}
			pSrc++;
		}
		return pSrc;
	}

inline	const char* SkipUntilTab               ( const char *pSrc)
	{
		while (pSrc && (*pSrc))
		{
			if (*pSrc == CHAR_TAB)
			{
				break;
			}
			pSrc++;
		}
		return pSrc;
	}

inline	const char* SkipUntilToken             ( const char *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e *pToken_ )
{
	if ( pToken_)
		*pToken_ = NO_TOKEN;

	while (pSrc && (*pSrc))
	{
		if (ParserFindToken( pSrc, aTokens, nTokens, pToken_ ))
			return pSrc;

		pSrc++;
	}
	return pSrc;
}
inline	const char* SkipUntilWhiteSpace        ( const char *pSrc )
	{
		while (pSrc && (*pSrc))
		{
			if ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB))
			{
				break;
			}
			pSrc++;
		}
		return pSrc;
	}

inline	const char* SkipUntilWhiteSpaceReverse ( const char *pSrc, const char *pStart )
	{
		while (pSrc && (pSrc > pStart))
		{
			if ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB))
			{
				break;		
			}
			pSrc--;
		}
		return pSrc;
	}


/*
	const TCHAR* SkipEOL                    ( const TCHAR *pSrc );
	const TCHAR* SkipWhiteSpace             ( const TCHAR *pSrc );
	const TCHAR* SkipWhiteSpaceReverse      ( const TCHAR *pSrc, const TCHAR *pStart );
	const TCHAR* SkipUntilChar              ( const TCHAR *pSrc, const TCHAR nDelim );
	const TCHAR* SkipUntilEOL               ( const TCHAR *pSrc );
	const TCHAR* SkipUntilToken             ( const TCHAR *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e *pToken_ );
	const TCHAR* SkipUntilWhiteSpace        ( const TCHAR *pSrc );
	const TCHAR* SkipUntilWhiteSpaceReverse ( const TCHAR *pSrc, const TCHAR *pStart );
	const TCHAR* SkipUntilTab               ( const TCHAR *pSrc);
*/

	const TCHAR * FindTokenOrAlphaNumeric ( const TCHAR *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e * pToken_ );
// TextRemoveWhiteSpaceReverse
	int          RemoveWhiteSpaceReverse    (       char *pSrc );
// TextExpandTabsToSpaces
	void TextConvertTabsToSpaces( TCHAR *pDeTabified_, LPCTSTR pText, const int nDstSize, int nTabStop = 0 );

#endif
