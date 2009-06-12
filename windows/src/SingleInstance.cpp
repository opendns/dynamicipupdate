#include "stdafx.h"

// See http://www.codeproject.com/KB/cpp/avoidmultinstance.aspx

#include "SingleInstance.h"
#include <malloc.h>	// We use the _alloca() function

// Creates a "unique" name, where the meaning of "unique" depends on the nMode 
// flag values. Returns pszBuffer
//
// pszGUID:		Copied to the beginning of pszBuffer, should be an GUID
// pszBuffer:	Buffer for unique name. Length (in chars) must be >= MAX_PATH
// nMode:		Information, that should be used to create the unique name.
//				Can be one of the following values:
//
//				SI_SESSION_UNIQUE						- Allow one instance per login session
//				SI_DESKTOP_UNIQUE						- Allow one instance per desktop
//				SI_TRUSTEE_UNIQUE						- Allow one instance per user account
//				SI_SESSION_UNIQUE | SI_DESKTOP_UNIQUE	- Allow one instance per login session,
//														  instances in different login sessions
//														  must also reside on a different desktop
//				SI_TRUSTEE_UNIQUE | SI_DESKTOP_UNIQUE	- Allow one instance per user account,
//														  instances in login sessions running a
//														  different user account must also reside	
//														  on different desktops.
//				SI_SYSTEM_UNIQUE						- Allow only one instance on the whole system	
//
LPTSTR CreateUniqueName( 
		LPCTSTR pszGUID, 
		LPTSTR pszBuffer, 
		int nMode	// = SI_DESKTOP_UNIQUE 
		)
{
	if( pszBuffer == NULL ) {
		SetLastError( ERROR_INVALID_PARAMETER );
		return NULL;
	}

	// First copy GUID to destination buffer
	if( pszGUID )
		_tcscpy( pszBuffer, pszGUID );
	else
		*pszBuffer = 0;

	if( nMode & SI_DESKTOP_UNIQUE ) {
		// Name should be desktop unique, so add current desktop name
		
		_tcscat( pszBuffer, _T("-" ) );
		HDESK hDesk		= GetThreadDesktop( GetCurrentThreadId() );
		ULONG  cchDesk	= MAX_PATH - _tcslen( pszBuffer ) - 1;

		if( !GetUserObjectInformation( hDesk, UOI_NAME, pszBuffer + _tcslen( pszBuffer ), cchDesk, &cchDesk ) )
			// Call will fail on Win9x
			_tcsncat( pszBuffer, _T("Win9x"), cchDesk );
	}
	if( nMode & SI_SESSION_UNIQUE ) {
		// Name should be session unique, so add current session id
		
		HANDLE hToken = NULL;
		// Try to open the token (fails on Win9x) and check necessary buffer size
		if( OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &hToken ) 
			&& ( MAX_PATH - _tcslen( pszBuffer ) > 9 ) ) 
		{
			DWORD cbBytes = 0;

			if( !GetTokenInformation( hToken, TokenStatistics, NULL, cbBytes, &cbBytes ) 
				&& GetLastError() == ERROR_INSUFFICIENT_BUFFER )
			{
				PTOKEN_STATISTICS pTS = (PTOKEN_STATISTICS) _alloca( cbBytes );
				if( GetTokenInformation( hToken, TokenStatistics, (LPVOID) pTS, cbBytes, &cbBytes ) ) {
					wsprintf( 
						pszBuffer + _tcslen( pszBuffer ), 
						_T("-%08x%08x"), 
						pTS->AuthenticationId.HighPart, 
						pTS->AuthenticationId.LowPart 
						);
				}
			}
		}
	}
	if( nMode & SI_TRUSTEE_UNIQUE ) {
		// Name should be unique to the current user

		TCHAR szUser[ 64 ] = {0};
		TCHAR szDomain[ 64 ] = {0};
		DWORD cchUser	= 64;
		DWORD cchDomain	= 64;

		if( GetUserName( szUser, &cchUser ) ) {
			// Since NetApi() calls are quite time consuming
			// we retrieve the domain name from an environment variable
			cchDomain = GetEnvironmentVariable( _T("USERDOMAIN"), szDomain, cchDomain );

			UINT cchUsed = _tcslen( pszBuffer );
			if( MAX_PATH - cchUsed > cchUser + cchDomain + 3 ) {
				wsprintf(
					pszBuffer + cchUsed,
					_T("-%s-%s"),
					szDomain,
					szUser
					);
			}
		}
	}

	return pszBuffer;
}

// Returns TRUE, if there exists, according to the meaning of "unique" passed
// in nMode, another instance of this process.
BOOL IsInstancePresent( 
		LPCTSTR pszGUID, 
		int nMode	// = SI_DESKTOP_UNIQUE 
		)
{
	static HANDLE hMutex = NULL;
	
	if( hMutex == NULL ) {
		TCHAR szName[ MAX_PATH ];
		hMutex = CreateMutex( NULL, FALSE, CreateUniqueName( pszGUID, szName, nMode ) );
		return ( GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_ACCESS_DENIED );
	}
	return FALSE;
}
