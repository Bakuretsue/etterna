#include "global.h"
#include "CrashHandlerInternal.h"
#include "Crash.h"

#include <windows.h>
#include "archutils/Win32/ddk/dbghelp.h"
#include <io.h>
#include <fcntl.h>

#include "archutils/Win32/WindowsResources.h"
#include "archutils/Win32/DialogUtil.h"
#include "archutils/Win32/GotoURL.h"
#include "archutils/Win32/RestartProgram.h"
#include "ProductInfo.h"
#include "RageUtil.h"

#if defined(_MSC_VER)
#pragma comment(lib, "archutils/Win32/ddk/dbghelp.lib")
#endif

extern unsigned long version_num;
extern const char *version_time;

// VDI symbol lookup:
namespace VDDebugInfo
{
	struct Context
	{
		Context() { pRVAHeap=NULL; }
		bool Loaded() const { return pRVAHeap != NULL; }
		void *pRawBlock;

		int nBuildNumber;

		const unsigned char *pRVAHeap;
		unsigned nFirstRVA;

		const char *pFuncNameHeap;
		const unsigned long (*pSegments)[2];
		int nSegments;
		char sFilename[1024];
		char szError[1024];
	};

	static void GetVDIPath( char *buf, int bufsiz )
	{
		GetModuleFileName( NULL, buf, bufsiz );
		buf[bufsiz-5] = 0;
		char *p = strrchr( buf, '.' );
		if( p )
			strcpy( p, ".vdi" );
		else
			strcat( buf, ".vdi" );
	}

	bool VDDebugInfoInitFromMemory( Context *pctx, const void *src_ )
	{
		const unsigned char *src = (const unsigned char *)src_;

		pctx->pRVAHeap = NULL;

		static const char *header = "symbolic debug information";
		if( memcmp(src, header, strlen(header)) )
		{
			strcpy( pctx->szError, "header doesn't match" );
			return false;
		}

		// Extract fields

		src += 64;

		pctx->nBuildNumber		= *(int *)src;
		pctx->pRVAHeap			= (const unsigned char *)(src + 20);
		pctx->nFirstRVA			= *(const long *)(src + 16);
		pctx->pFuncNameHeap		= (const char *)pctx->pRVAHeap - 4 + *(const long *)(src + 4);
		pctx->pSegments			= (unsigned long (*)[2])(pctx->pFuncNameHeap + *(const long *)(src + 8));
		pctx->nSegments			= *(const long *)(src + 12);

		return true;
	}

	void VDDebugInfoDeinit( Context *pctx )
	{
		if( pctx->pRawBlock )
		{
			VirtualFree(pctx->pRawBlock, 0, MEM_RELEASE);
			pctx->pRawBlock = NULL;
		}
	}

	bool VDDebugInfoInitFromFile( Context *pctx )
	{
		if( pctx->Loaded() )
			return true;

		pctx->pRawBlock = NULL;
		pctx->pRVAHeap = NULL;
		GetVDIPath( pctx->sFilename, ARRAYSIZE(pctx->sFilename) );
		pctx->szError[0] = 0;

		HANDLE h = CreateFile("StepMania.vdi", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if( h == INVALID_HANDLE_VALUE )
		{
			strcpy( pctx->szError, "CreateFile failed" );
			return false;
		}

		do {
			DWORD dwFileSize = GetFileSize( h, NULL );
			if( dwFileSize == INVALID_FILE_SIZE )
				break;

			pctx->pRawBlock = VirtualAlloc( NULL, dwFileSize, MEM_COMMIT, PAGE_READWRITE );
			if( !pctx->pRawBlock )
				break;

			DWORD dwActual;
			if( !ReadFile(h, pctx->pRawBlock, dwFileSize, &dwActual, NULL) || dwActual != dwFileSize )
				break;

			if( VDDebugInfoInitFromMemory(pctx, pctx->pRawBlock) )
			{
				CloseHandle(h);
				return true;
			}

			VirtualFree( pctx->pRawBlock, 0, MEM_RELEASE );
		} while(0);

		VDDebugInfoDeinit(pctx);
		CloseHandle(h);
		return false;
	}

	static bool PointerIsInAnySegment( const Context *pctx, unsigned rva )
	{
		for( int i=0; i<pctx->nSegments; ++i )
		{
			if (rva >= pctx->pSegments[i][0] && rva < pctx->pSegments[i][0] + pctx->pSegments[i][1])
				return true;
		}

		return false;
	}

	static const char *GetNameFromHeap(const char *heap, int idx)
	{
		while(idx--)
			while(*heap++);

		return heap;
	}

	long VDDebugInfoLookupRVA( Context *pctx, unsigned rva, char *buf, int buflen )
	{
		if( !PointerIsInAnySegment(pctx, rva) )
			return -1;

		const unsigned char *pr = pctx->pRVAHeap;
		const unsigned char *pr_limit = (const unsigned char *)pctx->pFuncNameHeap;
		int idx = 0;

		// Linearly unpack RVA deltas and find lower_bound
		rva -= pctx->nFirstRVA;

		if( (signed)rva < 0 )
			return -1;

		while( pr < pr_limit )
		{
			unsigned char c;
			unsigned diff = 0;

			do
			{
				c = *pr++;

				diff = (diff << 7) | (c & 0x7f);
			} while(c & 0x80);

			rva -= diff;

			if ((signed)rva < 0) {
				rva += diff;
				break;
			}

			++idx;
		}
		if( pr >= pr_limit )
			return -1;

		// Decompress name for RVA
		const char *fn_name = GetNameFromHeap(pctx->pFuncNameHeap, idx);

		if( !*fn_name )
			fn_name = "(special)";

		strncpy( buf, fn_name, buflen );
		buf[buflen-1] = 0;

		return rva;
	}
}

bool ReadFromParent( int fd, void *p, int size )
{
	char *buf = (char *) p;
	int got = 0;
	while( got < size )
	{
		int ret = read( fd, buf+got, size-got );
		if( ret == -1 )
		{
			if( errno == EINTR )
				continue;
			fprintf( stderr, "Crash handler: error communicating with parent: %s\n", strerror(errno) );
			return false;
		}

		if( ret == 0 )
		{
			fprintf( stderr, "Crash handler: EOF communicating with parent.\n" );
			return false;
		}

		got += ret;
	}

	return true;
}


// General symbol lookup; uses VDDebugInfo for detailed information within the
// process, and DbgHelp for simpler information about loaded DLLs.
namespace SymbolLookup
{
	HANDLE g_hParent;

	bool InitDbghelp()
	{
		static bool bInitted = false;
		if( !bInitted )
		{
			SymSetOptions( SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS );

			if( !SymInitialize(g_hParent, NULL, TRUE) )
				return false;

			bInitted = true;
		}

		return true;
	}

	SYMBOL_INFO *GetSym( unsigned long ptr, DWORD64 &disp )
	{
		InitDbghelp();

		static BYTE buffer[1024];
		SYMBOL_INFO *pSymbol = (PSYMBOL_INFO)buffer;

		pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSymbol->MaxNameLen = sizeof(buffer) - sizeof(SYMBOL_INFO) + 1;

		if( !SymFromAddr(g_hParent, ptr, &disp, pSymbol) )
			return NULL;

		return pSymbol;
	}

	const char *Demangle( const char *buf )
	{
		if( !InitDbghelp() )
			return buf;

		static char obuf[1024];
		if( !UnDecorateSymbolName(buf, obuf, sizeof(obuf),
			UNDNAME_COMPLETE
			| UNDNAME_NO_CV_THISTYPE
			| UNDNAME_NO_ALLOCATION_MODEL
			| UNDNAME_NO_ACCESS_SPECIFIERS // no public:
			| UNDNAME_NO_MS_KEYWORDS // no __cdecl 
			) )
		{
			return buf;
		}

		if( obuf[0] == '_' )
		{
			strcat( obuf, "()" ); /* _main -> _main() */
			return obuf+1; /* _main -> main */
		}

		return obuf;
	}

	RString CrashChildGetModuleBaseName( HMODULE hMod )
	{
		int iCmd = 1;
		write( _fileno(stdout), &iCmd, sizeof(iCmd) );
		write( _fileno(stdout), &hMod,  sizeof(hMod) );

		int iFD = fileno(stdin);
		int iSize;
		if( !ReadFromParent(iFD, &iSize, sizeof(iSize)) )
			return "???";
		RString sName;
		char *pBuf = sName.GetBuffer( iSize );
		if( !ReadFromParent(iFD, pBuf, iSize) )
			return "???";
		sName.ReleaseBuffer( iSize );
		return sName;
	}

	void SymLookup( VDDebugInfo::Context *pctx, const void *ptr, char *buf )
	{
		if( !pctx->Loaded() )
		{
			strcpy( buf, "error" );
			return;
		}

		MEMORY_BASIC_INFORMATION meminfo;
		VirtualQueryEx( g_hParent, ptr, &meminfo, sizeof meminfo );

		char tmp[512];
		if( VDDebugInfoLookupRVA(pctx, (unsigned int)ptr, tmp, sizeof(tmp)) >= 0 )
		{
			wsprintf( buf, "%08x: %s", ptr, Demangle(tmp) );
			return;
		}

		
		RString sName = CrashChildGetModuleBaseName( (HMODULE)meminfo.AllocationBase );

		DWORD64 disp;
		SYMBOL_INFO *pSymbol = GetSym( (unsigned int)ptr, disp );

		if( pSymbol )
		{
			wsprintf( buf, "%08lx: %s!%s [%08lx+%lx+%lx]",
				(unsigned long) ptr, sName.c_str(), pSymbol->Name,
				(unsigned long) meminfo.AllocationBase,
				(unsigned long) (pSymbol->Address) - (unsigned long) (meminfo.AllocationBase),
				(unsigned long) disp);
			return;
		}

		wsprintf( buf, "%08lx: %s!%08lx",
			(unsigned long) ptr, sName.c_str(), 
			(unsigned long) meminfo.AllocationBase );
	}
}

namespace
{

RString SpliceProgramPath( RString fn )
{
	char szBuf[MAX_PATH];
	GetModuleFileName( NULL, szBuf, sizeof(szBuf) );

	char szModName[MAX_PATH];
	char *pszFile;
	GetFullPathName( szBuf, sizeof(szModName), szModName, &pszFile );
	strcpy( pszFile, fn );

	return szModName;
}

namespace
{
	VDDebugInfo::Context g_debugInfo;

	RString ReportCallStack( const void * const *Backtrace )
	{
		if( !g_debugInfo.Loaded() )
			return ssprintf( "debug resource file '%s': %s.\n", g_debugInfo.sFilename, g_debugInfo.szError );

		if( g_debugInfo.nBuildNumber != int(version_num) )
		{
			return ssprintf( "Incorrect %s file (build %d, expected %d) for this version of " PRODUCT_NAME " -- call stack unavailable.\n",
				g_debugInfo.sFilename, g_debugInfo.nBuildNumber, int(version_num) );
		}

		RString sRet;
		for( int i = 0; Backtrace[i]; ++i )
		{
			char buf[10240];
			SymbolLookup::SymLookup( &g_debugInfo, Backtrace[i], buf );
			sRet += ssprintf( "%s\n", buf );
		}

		return sRet;
	}
}

struct CompleteCrashData
{
	CrashInfo m_CrashInfo;
	RString m_sInfo;
	RString m_sAdditionalLog;
	RString m_sCrashedThread;
	vector<RString> m_asRecent;
	vector<RString> m_asCheckpoints;
};

static void MakeCrashReport( const CompleteCrashData &Data, RString &sOut )
{
	sOut += ssprintf(
			"%s crash report (build %d, %s)\n"
			"--------------------------------------\n\n",
			PRODUCT_NAME_VER, version_num, version_time );

	sOut += ssprintf( "%s\n", Data.m_CrashInfo.m_CrashReason );
	sOut += ssprintf( "\n" );

	// Dump thread stacks
	static char buf[1024*32];
	sOut += ssprintf( "%s\n", join("\n", Data.m_asCheckpoints).c_str() );
	
	sOut += ReportCallStack( Data.m_CrashInfo.m_BacktracePointers );
	sOut += ssprintf( "\n" );

	if( Data.m_CrashInfo.m_AlternateThreadBacktrace[0] )
	{
		for( int i = 0; i < CrashInfo::MAX_BACKTRACE_THREADS; ++i )
		{
			if( !Data.m_CrashInfo.m_AlternateThreadBacktrace[i][0] )
				continue;

			sOut += ssprintf( "Thread %s:\n", Data.m_CrashInfo.m_AlternateThreadName[i] );
			sOut += ssprintf( "\n" );
			sOut += ReportCallStack( Data.m_CrashInfo.m_AlternateThreadBacktrace[i] );
			sOut += ssprintf( "" );
		}
	}

	sOut += ssprintf( "Static log:\n" );
	sOut += ssprintf( "%s\n", Data.m_sInfo.c_str() );
	sOut += ssprintf( "%s\n", Data.m_sAdditionalLog.c_str() );
	sOut += ssprintf( "\n" );

	sOut += ssprintf( "Partial log:\n" );
	for( size_t  i = 0; i < Data.m_asRecent.size(); ++i )
		sOut += ssprintf( "%s\n", Data.m_asRecent[i].c_str() );
	sOut += ssprintf( "\n" );

	sOut += ssprintf( "-- End of report\n" );
}

static void DoSave( const CompleteCrashData &Data )
{
	RString sReport;
	MakeCrashReport( Data, sReport );

	RString sName = SpliceProgramPath( "../crashinfo.txt" );

	SetFileAttributes( sName, FILE_ATTRIBUTE_NORMAL );
	FILE *pFile = fopen( sName, "w+" );
	if( pFile == NULL )
		return;
	fprintf( pFile, "%s", sReport.c_str() );

	fclose( pFile );

	/* Discourage changing crashinfo.txt. */
	SetFileAttributes( sName, FILE_ATTRIBUTE_READONLY );
}

void ViewWithNotepad(const char *str)
{
	char buf[256] = "";
	strcat( buf, "notepad.exe " );
	strcat( buf, str );

	RString cwd = SpliceProgramPath( "" );

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	ZeroMemory( &si, sizeof(si) );

	CreateProcess(
		NULL,		// pointer to name of executable module
		buf,		// pointer to command line string
		NULL,  // process security attributes
		NULL,   // thread security attributes
		false,  // handle inheritance flag
		0, // creation flags
		NULL,  // pointer to new environment block
		cwd,   // pointer to current directory name
		&si,  // pointer to STARTUPINFO
		&pi  // pointer to PROCESS_INFORMATION
	);
}


bool ReadCrashDataFromParent( int iFD, CompleteCrashData &Data )
{
	_setmode( _fileno(stdin), O_BINARY );

	/* 0. Read the parent handle. */
	if( !ReadFromParent(iFD, &SymbolLookup::g_hParent, sizeof(SymbolLookup::g_hParent)) )
		return false;

	/* 1. Read the CrashData. */
	if( !ReadFromParent(iFD, &Data.m_CrashInfo, sizeof(Data.m_CrashInfo)) )
		return false;

	/* 2. Read info. */
	int iSize;
	if( !ReadFromParent(iFD, &iSize, sizeof(iSize)) )
		return false;

	char *pBuf = Data.m_sInfo.GetBuffer( iSize );
	if( !ReadFromParent(iFD, pBuf, iSize) )
		return false;
	Data.m_sInfo.ReleaseBuffer( iSize );

	/* 3. Read AdditionalLog. */
	if( !ReadFromParent(iFD, &iSize, sizeof(iSize)) )
		return false;

	pBuf = Data.m_sAdditionalLog.GetBuffer( iSize );
	if( !ReadFromParent(iFD, pBuf, iSize) )
		return false;
	Data.m_sAdditionalLog.ReleaseBuffer( iSize );

	/* 4. Read RecentLogs. */
	int iCnt = 0;
	if( !ReadFromParent(iFD, &iCnt, sizeof(iCnt)) )
		return false;
	for( int i = 0; i < iCnt; ++i )
	{
		if( !ReadFromParent(iFD, &iSize, sizeof(iSize)) )
			return false;
		RString sBuf;
		pBuf = sBuf.GetBuffer( iSize );
		if( !ReadFromParent(iFD, pBuf, iSize) )
			return false;
		Data.m_asRecent.push_back( sBuf );
		sBuf.ReleaseBuffer( iSize );
	}

	/* 5. Read CHECKPOINTs. */
	if( !ReadFromParent(iFD, &iSize, sizeof(iSize)) )
		return false;

	RString sBuf;
	pBuf = sBuf.GetBuffer( iSize );
	if( !ReadFromParent(iFD, pBuf, iSize) )
		return false;

	split( sBuf, "$$", Data.m_asCheckpoints );
	sBuf.ReleaseBuffer( iSize );

	/* 6. Read the crashed thread's name. */
	if( !ReadFromParent(iFD, &iSize, sizeof(iSize)) )
		return false;
	pBuf = Data.m_sCrashedThread.GetBuffer( iSize );
	if( !ReadFromParent(iFD, pBuf, iSize) )
		return false;
	Data.m_sCrashedThread.ReleaseBuffer();

	return true;
}

BOOL APIENTRY CrashDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		DialogUtil::SetHeaderFont( hDlg, IDC_STATIC_HEADER_TEXT );
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_BUTTON_CLOSE:
			EndDialog(hDlg, FALSE);
			return TRUE;
		case IDOK:
			// EndDialog(hDlg, TRUE); /* don't always exit on ENTER */
			return TRUE;
		case IDC_VIEW_LOG:
			ViewWithNotepad("../log.txt");
			break;
		case IDC_CRASH_SAVE:
			ViewWithNotepad("../crashinfo.txt");
			return TRUE;
		case IDC_BUTTON_RESTART:
			Win32RestartProgram();
			EndDialog( hDlg, FALSE );
			break;
		case IDC_BUTTON_REPORT:
			GotoURL( REPORT_BUG_URL );
			break;
		}
		break;
	}

	return FALSE;
}

void ChildProcess()
{
	/* Read the crash data from the crashed parent. */
	CompleteCrashData Data;
	ReadCrashDataFromParent( fileno(stdin), Data );

	VDDebugInfoInitFromFile( &g_debugInfo );
	DoSave( Data );
	VDDebugInfoDeinit( &g_debugInfo );

	/* Tell the crashing process that it can exit.  Be sure to write crashinfo.txt first. */
	int iCmd = 0;
	write( _fileno(stdout), &iCmd, sizeof(iCmd) );

	/* Now that we've done that, the process is gone.  Don't use g_hParent. */
	CloseHandle( SymbolLookup::g_hParent );
	SymbolLookup::g_hParent = NULL;

	/* Little trick to get an HINSTANCE of ourself without having access to the hwnd ... */
	{
		TCHAR szFullAppPath[MAX_PATH];
		GetModuleFileName( NULL, szFullAppPath, MAX_PATH );
		HINSTANCE hHandle = LoadLibrary( szFullAppPath );

		DialogBoxParam( hHandle, MAKEINTRESOURCE(IDD_DISASM_CRASH), NULL, CrashDlgProc, NULL );
	}
}

}

void CrashHandler::CrashHandlerHandleArgs( int argc, char* argv[] )
{
	if( argc == 2 && !strcmp(argv[1], CHILD_MAGIC_PARAMETER) )
	{
		ChildProcess();
		exit(0);
	}
}

/*
 * (c) 2003-2006 Glenn Maynard
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
