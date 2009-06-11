#ifndef __SINGLEINSTANCE_H__
#define __SINGLEINSTANCE_H__

// See http://www.codeproject.com/KB/cpp/avoidmultinstance.aspx

const int SI_SESSION_UNIQUE		= 0x0001;	// Allow only one instance per login session
const int SI_DESKTOP_UNIQUE		= 0x0002;	// Allow only one instance on current desktop
const int SI_TRUSTEE_UNIQUE		= 0x0004;	// Allow only one instance for current user
const int SI_SYSTEM_UNIQUE		= 0x0000;	// Allow only one instance at all (on the whole system)

										// Note: SI_SESSION_UNIQE and SI_TRUSTEE_UNIQUE can
										// be combined with SI_DESKTOP_UNIQUE

LPTSTR CreateUniqueName( LPCTSTR pszGUID, LPTSTR pszBuffer, int nMode = SI_DESKTOP_UNIQUE );
BOOL IsInstancePresent( LPCTSTR pszGUID, int nMode = SI_DESKTOP_UNIQUE );

#endif