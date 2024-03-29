/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL Multi-Threading, and process handling portability functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cpl_multiproc.h"
#include "cpl_conv.h"

#if !defined(WIN32CE)
#  include <time.h>
#else
#  include <wce_time.h>
#endif

CPL_CVSID("$Id$");

#if defined(CPL_MULTIPROC_STUB) && !defined(DEBUG)
#  define MUTEX_NONE
#endif

/* We don't want it to be publicly used since it solves rather tricky issues */
/* that are better to remain hidden... */
void CPLFinalizeTLS();

/************************************************************************/
/*                           CPLMutexHolder()                           */
/************************************************************************/

CPLMutexHolder::CPLMutexHolder( void **phMutex, double dfWaitInSeconds,
                                const char *pszFileIn, 
                                int nLineIn )

{
#ifndef MUTEX_NONE
    pszFile = pszFileIn;
    nLine = nLineIn;

#ifdef DEBUG_MUTEX
    /*
     * XXX: There is no way to use CPLDebug() here because it works with
     * mutexes itself so we will fall in infinite recursion. Good old
     * fprintf() will do the job right.
     */
    fprintf( stderr,
             "CPLMutexHolder: Request %p for pid %ld at %d/%s.\n", 
             *phMutex, (long) CPLGetPID(), nLine, pszFile );
#endif

    if( !CPLCreateOrAcquireMutex( phMutex, dfWaitInSeconds ) )
    {
        fprintf( stderr, "CPLMutexHolder: Failed to acquire mutex!\n" );
        hMutex = NULL;
    }
    else
    {
#ifdef DEBUG_MUTEX
        fprintf( stderr,
                 "CPLMutexHolder: Acquired %p for pid %ld at %d/%s.\n", 
                 *phMutex, (long) CPLGetPID(), nLine, pszFile );
#endif

        hMutex = *phMutex;
    }
#endif /* ndef MUTEX_NONE */
}

/************************************************************************/
/*                           CPLMutexHolder()                           */
/************************************************************************/

CPLMutexHolder::CPLMutexHolder( void *hMutexIn, double dfWaitInSeconds,
                                const char *pszFileIn, 
                                int nLineIn )

{
#ifndef MUTEX_NONE
    pszFile = pszFileIn;
    nLine = nLineIn;
    hMutex = hMutexIn;

    if( hMutex != NULL &&
        !CPLAcquireMutex( hMutex, dfWaitInSeconds ) )
    {
        fprintf( stderr, "CPLMutexHolder: Failed to acquire mutex!\n" );
        hMutex = NULL;
    }
#endif /* ndef MUTEX_NONE */
}

/************************************************************************/
/*                          ~CPLMutexHolder()                           */
/************************************************************************/

CPLMutexHolder::~CPLMutexHolder()

{
#ifndef MUTEX_NONE
    if( hMutex != NULL )
    {
#ifdef DEBUG_MUTEX
        fprintf( stderr,
                 "~CPLMutexHolder: Release %p for pid %ld at %d/%s.\n", 
                 hMutex, (long) CPLGetPID(), nLine, pszFile );
#endif
        CPLReleaseMutex( hMutex );
    }
#endif /* ndef MUTEX_NONE */
}


/************************************************************************/
/*                      CPLCreateOrAcquireMutex()                       */
/************************************************************************/

#ifndef CPL_MULTIPROC_PTHREAD

#ifndef MUTEX_NONE
static void *hCOAMutex = NULL;
#endif

int CPLCreateOrAcquireMutex( void **phMutex, double dfWaitInSeconds )

{
    int bSuccess = FALSE;

#ifndef MUTEX_NONE

    /*
    ** ironically, creation of this initial mutex is not threadsafe
    ** even though we use it to ensure that creation of other mutexes
    ** is threadsafe. 
    */
    if( hCOAMutex == NULL )
    {
        hCOAMutex = CPLCreateMutex();
        if (hCOAMutex == NULL)
        {
            *phMutex = NULL;
            return FALSE;
        }
    }
    else
    {
        CPLAcquireMutex( hCOAMutex, dfWaitInSeconds );
    }

    if( *phMutex == NULL )
    {
        *phMutex = CPLCreateMutex();
        bSuccess = *phMutex != NULL;
        CPLReleaseMutex( hCOAMutex );
    }
    else
    {
        CPLReleaseMutex( hCOAMutex );

        bSuccess = CPLAcquireMutex( *phMutex, dfWaitInSeconds );
    }
#endif /* ndef MUTEX_NONE */

    return bSuccess;
}
#endif

/************************************************************************/ 
/*                      CPLCleanupMasterMutex()                         */ 
/************************************************************************/ 
 
void CPLCleanupMasterMutex() 
{ 
#ifndef CPL_MULTIPROC_PTHREAD 
#ifndef MUTEX_NONE 
    if( hCOAMutex != NULL ) 
    { 
        CPLDestroyMutex( hCOAMutex ); 
        hCOAMutex = NULL; 
    } 
#endif 
#endif 
} 

/************************************************************************/
/*                        CPLCleanupTLSList()                           */
/*                                                                      */
/*      Free resources associated with a TLS vector (implementation     */
/*      independent).                                                   */
/************************************************************************/

static void CPLCleanupTLSList( void **papTLSList )

{
    int i;

//    printf( "CPLCleanupTLSList(%p)\n", papTLSList );
    
    if( papTLSList == NULL )
        return;

    for( i = 0; i < CTLS_MAX; i++ )
    {
        if( papTLSList[i] != NULL && papTLSList[i+CTLS_MAX] != NULL )
        {
            CPLTLSFreeFunc pfnFree = (CPLTLSFreeFunc) papTLSList[i + CTLS_MAX];
            pfnFree( papTLSList[i] );
            papTLSList[i] = NULL;
        }
    }

    CPLFree( papTLSList );
}

#if defined(CPL_MULTIPROC_STUB)
/************************************************************************/
/* ==================================================================== */
/*                        CPL_MULTIPROC_STUB                            */
/*                                                                      */
/*      Stub implementation.  Mutexes don't provide exclusion, file     */
/*      locking is achieved with extra "lock files", and thread         */
/*      creation doesn't work.  The PID is always just one.             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             CPLGetNumCPUs()                          */
/************************************************************************/

int CPLGetNumCPUs()
{
    return 1;
}

/************************************************************************/
/*                        CPLGetThreadingModel()                        */
/************************************************************************/

const char *CPLGetThreadingModel()

{
    return "stub";
}

/************************************************************************/
/*                           CPLCreateMutex()                           */
/************************************************************************/

void *CPLCreateMutex()

{
#ifndef MUTEX_NONE
    unsigned char *pabyMutex = (unsigned char *) malloc( 4 );

    pabyMutex[0] = 1;
    pabyMutex[1] = 'r';
    pabyMutex[2] = 'e';
    pabyMutex[3] = 'd';

    return (void *) pabyMutex;
#else
    return (void *) 0xdeadbeef;
#endif 
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

int CPLAcquireMutex( void *hMutex, double dfWaitInSeconds )

{
#ifndef MUTEX_NONE
    unsigned char *pabyMutex = (unsigned char *) hMutex;

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e' 
               && pabyMutex[3] == 'd' );

    pabyMutex[0] += 1;

    return TRUE;
#else
    return TRUE;
#endif
}

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

void CPLReleaseMutex( void *hMutex )

{
#ifndef MUTEX_NONE
    unsigned char *pabyMutex = (unsigned char *) hMutex;

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e' 
               && pabyMutex[3] == 'd' );

    if( pabyMutex[0] < 1 )
        CPLDebug( "CPLMultiProc", 
                  "CPLReleaseMutex() called on mutex with %d as ref count!",
                  pabyMutex[0] );

    pabyMutex[0] -= 1;
#endif
}

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

void CPLDestroyMutex( void *hMutex )

{
#ifndef MUTEX_NONE
    unsigned char *pabyMutex = (unsigned char *) hMutex;

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e' 
               && pabyMutex[3] == 'd' );

    free( pabyMutex );
#endif
}

/************************************************************************/
/*                            CPLCreateCond()                           */
/************************************************************************/

void  *CPLCreateCond()
{
    return NULL;
}

/************************************************************************/
/*                            CPLCondWait()                             */
/************************************************************************/

void  CPLCondWait( void *hCond, void* hMutex )
{
}

/************************************************************************/
/*                            CPLCondSignal()                           */
/************************************************************************/

void  CPLCondSignal( void *hCond )
{
}

/************************************************************************/
/*                           CPLCondBroadcast()                         */
/************************************************************************/

void  CPLCondBroadcast( void *hCond )
{
}

/************************************************************************/
/*                            CPLDestroyCond()                          */
/************************************************************************/

void  CPLDestroyCond( void *hCond )
{
}

/************************************************************************/
/*                            CPLLockFile()                             */
/*                                                                      */
/*      Lock a file.  This implementation has a terrible race           */
/*      condition.  If we don't succeed in opening the lock file, we    */
/*      assume we can create one and own the target file, but other     */
/*      processes might easily try creating the target file at the      */
/*      same time, overlapping us.  Death!  Mayhem!  The traditional    */
/*      solution is to use open() with _O_CREAT|_O_EXCL but this        */
/*      function and these arguments aren't trivially portable.         */
/*      Also, this still leaves a race condition on NFS drivers         */
/*      (apparently).                                                   */
/************************************************************************/

void *CPLLockFile( const char *pszPath, double dfWaitInSeconds )

{
    FILE      *fpLock;
    char      *pszLockFilename;
    
/* -------------------------------------------------------------------- */
/*      We use a lock file with a name derived from the file we want    */
/*      to lock to represent the file being locked.  Note that for      */
/*      the stub implementation the target file does not even need      */
/*      to exist to be locked.                                          */
/* -------------------------------------------------------------------- */
    pszLockFilename = (char *) CPLMalloc(strlen(pszPath) + 30);
    sprintf( pszLockFilename, "%s.lock", pszPath );

    fpLock = fopen( pszLockFilename, "r" );
    while( fpLock != NULL && dfWaitInSeconds > 0.0 )
    {
        fclose( fpLock );
        CPLSleep( MIN(dfWaitInSeconds,0.5) );
        dfWaitInSeconds -= 0.5;

        fpLock = fopen( pszLockFilename, "r" );
    }
        
    if( fpLock != NULL )
    {
        fclose( fpLock );
        CPLFree( pszLockFilename );
        return NULL;
    }

    fpLock = fopen( pszLockFilename, "w" );

    if( fpLock == NULL )
    {
        CPLFree( pszLockFilename );
        return NULL;
    }

    fwrite( "held\n", 1, 5, fpLock );
    fclose( fpLock );

    return pszLockFilename;
}

/************************************************************************/
/*                           CPLUnlockFile()                            */
/************************************************************************/

void CPLUnlockFile( void *hLock )

{
    char *pszLockFilename = (char *) hLock;

    if( hLock == NULL )
        return;
    
    VSIUnlink( pszLockFilename );
    
    CPLFree( pszLockFilename );
}

/************************************************************************/
/*                             CPLGetPID()                              */
/************************************************************************/

GIntBig CPLGetPID()

{
    return 1;
}

/************************************************************************/
/*                          CPLCreateThread();                          */
/************************************************************************/

int CPLCreateThread( CPLThreadFunc pfnMain, void *pArg )

{
    CPLDebug( "CPLCreateThread", "Fails to dummy implementation" );

    return -1;
}

/************************************************************************/
/*                      CPLCreateJoinableThread()                       */
/************************************************************************/

void* CPLCreateJoinableThread( CPLThreadFunc pfnMain, void *pThreadArg )

{
    CPLDebug( "CPLCreateJoinableThread", "Fails to dummy implementation" );

    return NULL;
}

/************************************************************************/
/*                          CPLJoinThread()                             */
/************************************************************************/

void CPLJoinThread(void* hJoinableThread)
{
}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )

{
    time_t  ltime;
    time_t  ttime;

    time( &ltime );
    ttime = ltime + (int) (dfWaitInSeconds+0.5);

    for( ; ltime < ttime; time(&ltime) )
    {
        /* currently we just busy wait.  Perhaps we could at least block on 
           io? */
    }
}

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **papTLSList = NULL;

static void **CPLGetTLSList()

{
    if( papTLSList == NULL )
    {
        papTLSList = (void **) VSICalloc(sizeof(void*),CTLS_MAX*2);
        if( papTLSList == NULL )
            CPLEmergencyError("CPLGetTLSList() failed to allocate TLS list!");
    }

    return papTLSList;
}

/************************************************************************/
/*                             CPLFinalizeTLS()                         */
/************************************************************************/

void CPLFinalizeTLS()
{
    CPLCleanupTLS();
}

/************************************************************************/
/*                           CPLCleanupTLS()                            */
/************************************************************************/

void CPLCleanupTLS()

{
    CPLCleanupTLSList( papTLSList );
    papTLSList = NULL;
}

/* endif CPL_MULTIPROC_STUB */

#elif defined(CPL_MULTIPROC_WIN32)


  /************************************************************************/
  /* ==================================================================== */
  /*                        CPL_MULTIPROC_WIN32                           */
  /*                                                                      */
  /*    WIN32 Implementation of multiprocessing functions.                */
  /* ==================================================================== */
  /************************************************************************/

/* InitializeCriticalSectionAndSpinCount requires _WIN32_WINNT >= 0x403 */
#define _WIN32_WINNT 0x0500

#include <windows.h>

/* windows.h header must be included above following lines. */
#if defined(WIN32CE)
#  include "cpl_win32ce_api.h"
#  define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
#endif

/************************************************************************/
/*                             CPLGetNumCPUs()                          */
/************************************************************************/

int CPLGetNumCPUs()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    DWORD dwNum = info.dwNumberOfProcessors;
    if( dwNum < 1 )
        return 1;
    return (int)dwNum;
}

/************************************************************************/
/*                        CPLGetThreadingModel()                        */
/************************************************************************/

const char *CPLGetThreadingModel()

{
    return "win32";
}

/************************************************************************/
/*                           CPLCreateMutex()                           */
/************************************************************************/

void *CPLCreateMutex()

{
#ifdef USE_WIN32_MUTEX
    HANDLE hMutex;

    hMutex = CreateMutex( NULL, TRUE, NULL );

    return (void *) hMutex;
#else
    CRITICAL_SECTION *pcs;

	/* Do not use CPLMalloc() since its debugging infrastructure */
	/* can call the CPL*Mutex functions... */
    pcs = (CRITICAL_SECTION *)malloc(sizeof(*pcs));
    if( pcs )
    {
      InitializeCriticalSectionAndSpinCount(pcs, 4000);
      EnterCriticalSection(pcs);
    }

    return (void *) pcs;
#endif
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

int CPLAcquireMutex( void *hMutexIn, double dfWaitInSeconds )

{
#ifdef USE_WIN32_MUTEX
    HANDLE hMutex = (HANDLE) hMutexIn;
    DWORD  hr;

    hr = WaitForSingleObject( hMutex, (int) (dfWaitInSeconds * 1000) );
    
    return hr != WAIT_TIMEOUT;
#else
    CRITICAL_SECTION *pcs = (CRITICAL_SECTION *)hMutexIn;
    BOOL ret;

    while( (ret = TryEnterCriticalSection(pcs)) == 0 && dfWaitInSeconds > 0.0 )
    {
        CPLSleep( MIN(dfWaitInSeconds,0.125) );
        dfWaitInSeconds -= 0.125;
    }
    
    return ret;
#endif
}

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

void CPLReleaseMutex( void *hMutexIn )

{
#ifdef USE_WIN32_MUTEX
    HANDLE hMutex = (HANDLE) hMutexIn;

    ReleaseMutex( hMutex );
#else
    CRITICAL_SECTION *pcs = (CRITICAL_SECTION *)hMutexIn;

    LeaveCriticalSection(pcs);
#endif
}

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

void CPLDestroyMutex( void *hMutexIn )

{
#ifdef USE_WIN32_MUTEX
    HANDLE hMutex = (HANDLE) hMutexIn;

    CloseHandle( hMutex );
#else
    CRITICAL_SECTION *pcs = (CRITICAL_SECTION *)hMutexIn;

    DeleteCriticalSection( pcs );
    free( pcs );
#endif
}

/************************************************************************/
/*                            CPLCreateCond()                           */
/************************************************************************/

struct _WaiterItem
{
    HANDLE hEvent;
    struct _WaiterItem* psNext;
};
typedef struct _WaiterItem WaiterItem;

typedef struct
{
    void        *hInternalMutex;
    WaiterItem  *psWaiterList;
} Win32Cond;

void  *CPLCreateCond()
{
    Win32Cond* psCond = (Win32Cond*) malloc(sizeof(Win32Cond));
    if (psCond == NULL)
        return NULL;
    psCond->hInternalMutex = CPLCreateMutex();
    if (psCond->hInternalMutex == NULL)
    {
        free(psCond);
        return NULL;
    }
    CPLReleaseMutex(psCond->hInternalMutex);
    psCond->psWaiterList = NULL;
    return psCond;
}

/************************************************************************/
/*                            CPLCondWait()                             */
/************************************************************************/

static void CPLTLSFreeEvent(void* pData)
{
    CloseHandle((HANDLE)pData);
}

void  CPLCondWait( void *hCond, void* hClientMutex )
{
    Win32Cond* psCond = (Win32Cond*) hCond;

    HANDLE hEvent = (HANDLE) CPLGetTLS(CTLS_WIN32_COND);
    if (hEvent == NULL)
    {
        hEvent = CreateEvent(NULL, /* security attributes */
                             0,    /* manual reset = no */
                             0,    /* initial state = unsignaled */
                             NULL  /* no name */);
        CPLAssert(hEvent != NULL);

        CPLSetTLSWithFreeFunc(CTLS_WIN32_COND, hEvent, CPLTLSFreeEvent);
    }

    /* Insert the waiter into the waiter list of the condition */
    CPLAcquireMutex(psCond->hInternalMutex, 1000.0);

    WaiterItem* psItem = (WaiterItem*)malloc(sizeof(WaiterItem));
    CPLAssert(psItem != NULL);

    psItem->hEvent = hEvent;
    psItem->psNext = psCond->psWaiterList;

    psCond->psWaiterList = psItem;

    CPLReleaseMutex(psCond->hInternalMutex);

    /* Release the client mutex before waiting for the event being signaled */
    CPLReleaseMutex(hClientMutex);

    // Ideally we would check that we do not get WAIT_FAILED but it is hard 
    // to report a failure.
    WaitForSingleObject(hEvent, INFINITE);

    /* Reacquire the client mutex */
    CPLAcquireMutex(hClientMutex, 1000.0);
}

/************************************************************************/
/*                            CPLCondSignal()                           */
/************************************************************************/

void  CPLCondSignal( void *hCond )
{
    Win32Cond* psCond = (Win32Cond*) hCond;

    /* Signal the first registered event, and remove it from the list */
    CPLAcquireMutex(psCond->hInternalMutex, 1000.0);

    WaiterItem* psIter = psCond->psWaiterList;
    if (psIter != NULL)
    {
        SetEvent(psIter->hEvent);
        psCond->psWaiterList = psIter->psNext;
        free(psIter);
    }

    CPLReleaseMutex(psCond->hInternalMutex);
}

/************************************************************************/
/*                           CPLCondBroadcast()                         */
/************************************************************************/

void  CPLCondBroadcast( void *hCond )
{
    Win32Cond* psCond = (Win32Cond*) hCond;

    /* Signal all the registered events, and remove them from the list */
    CPLAcquireMutex(psCond->hInternalMutex, 1000.0);

    WaiterItem* psIter = psCond->psWaiterList;
    while (psIter != NULL)
    {
        WaiterItem* psNext = psIter->psNext;
        SetEvent(psIter->hEvent);
        free(psIter);
        psIter = psNext;
    }
    psCond->psWaiterList = NULL;

    CPLReleaseMutex(psCond->hInternalMutex);
}

/************************************************************************/
/*                            CPLDestroyCond()                          */
/************************************************************************/

void  CPLDestroyCond( void *hCond )
{
    Win32Cond* psCond = (Win32Cond*) hCond;
    CPLDestroyMutex(psCond->hInternalMutex);
    psCond->hInternalMutex = NULL;
    CPLAssert(psCond->psWaiterList == NULL);
    free(psCond);
}

/************************************************************************/
/*                            CPLLockFile()                             */
/************************************************************************/

void *CPLLockFile( const char *pszPath, double dfWaitInSeconds )

{
    char      *pszLockFilename;
    HANDLE    hLockFile;
    
    pszLockFilename = (char *) CPLMalloc(strlen(pszPath) + 30);
    sprintf( pszLockFilename, "%s.lock", pszPath );

    hLockFile = 
        CreateFile( pszLockFilename, GENERIC_WRITE, 0, NULL,CREATE_NEW, 
                    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE, NULL );

    while( GetLastError() == ERROR_ALREADY_EXISTS
           && dfWaitInSeconds > 0.0 )
    {
        CloseHandle( hLockFile );
        CPLSleep( MIN(dfWaitInSeconds,0.125) );
        dfWaitInSeconds -= 0.125;

        hLockFile = 
            CreateFile( pszLockFilename, GENERIC_WRITE, 0, NULL, CREATE_NEW, 
                        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE, 
                        NULL );
    }

    CPLFree( pszLockFilename );

    if( hLockFile == INVALID_HANDLE_VALUE )
        return NULL;

    if( GetLastError() == ERROR_ALREADY_EXISTS )
    {
        CloseHandle( hLockFile );
        return NULL;
    }

    return (void *) hLockFile;
}

/************************************************************************/
/*                           CPLUnlockFile()                            */
/************************************************************************/

void CPLUnlockFile( void *hLock )

{
    HANDLE    hLockFile = (HANDLE) hLock;

    CloseHandle( hLockFile );
}

/************************************************************************/
/*                             CPLGetPID()                              */
/************************************************************************/

GIntBig CPLGetPID()

{
    return (GIntBig) GetCurrentThreadId();
}

/************************************************************************/
/*                       CPLStdCallThreadJacket()                       */
/************************************************************************/

typedef struct {
    void *pAppData;
    CPLThreadFunc pfnMain;
    HANDLE hThread;
} CPLStdCallThreadInfo;

static DWORD WINAPI CPLStdCallThreadJacket( void *pData )

{
    CPLStdCallThreadInfo *psInfo = (CPLStdCallThreadInfo *) pData;

    psInfo->pfnMain( psInfo->pAppData );

    if (psInfo->hThread == NULL)
        CPLFree( psInfo ); /* Only for detached threads */

    CPLCleanupTLS();

    return 0;
}

/************************************************************************/
/*                          CPLCreateThread()                           */
/*                                                                      */
/*      The WIN32 CreateThread() call requires an entry point that      */
/*      has __stdcall conventions, so we provide a jacket function      */
/*      to supply that.                                                 */
/************************************************************************/

int CPLCreateThread( CPLThreadFunc pfnMain, void *pThreadArg )

{
    HANDLE hThread;
    DWORD  nThreadId;
    CPLStdCallThreadInfo *psInfo;

    psInfo = (CPLStdCallThreadInfo*) CPLCalloc(sizeof(CPLStdCallThreadInfo),1);
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;
    psInfo->hThread = NULL;

    hThread = CreateThread( NULL, 0, CPLStdCallThreadJacket, psInfo, 
                            0, &nThreadId );

    if( hThread == NULL )
        return -1;

    CloseHandle( hThread );

    return nThreadId;
}

/************************************************************************/
/*                      CPLCreateJoinableThread()                       */
/************************************************************************/

void* CPLCreateJoinableThread( CPLThreadFunc pfnMain, void *pThreadArg )

{
    HANDLE hThread;
    DWORD  nThreadId;
    CPLStdCallThreadInfo *psInfo;

    psInfo = (CPLStdCallThreadInfo*) CPLCalloc(sizeof(CPLStdCallThreadInfo),1);
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;

    hThread = CreateThread( NULL, 0, CPLStdCallThreadJacket, psInfo, 
                            0, &nThreadId );

    if( hThread == NULL )
        return NULL;
        
    psInfo->hThread = hThread;
    return psInfo;
}

/************************************************************************/
/*                          CPLJoinThread()                             */
/************************************************************************/

void CPLJoinThread(void* hJoinableThread)
{
    CPLStdCallThreadInfo *psInfo = (CPLStdCallThreadInfo *) hJoinableThread;
    
    WaitForSingleObject(psInfo->hThread, INFINITE);
    CloseHandle( psInfo->hThread );
    CPLFree( psInfo );
}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )

{
    Sleep( (DWORD) (dfWaitInSeconds * 1000.0) );
}

static int           bTLSKeySetup = FALSE;
static DWORD         nTLSKey;

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **CPLGetTLSList()

{
    void **papTLSList;

    if( !bTLSKeySetup )
    {
        nTLSKey = TlsAlloc();
        if( nTLSKey == TLS_OUT_OF_INDEXES )
        {
            CPLEmergencyError( "CPLGetTLSList(): TlsAlloc() failed!" );
        }
        bTLSKeySetup = TRUE;
    }

    papTLSList = (void **) TlsGetValue( nTLSKey );
    if( papTLSList == NULL )
    {
        papTLSList = (void **) VSICalloc(sizeof(void*),CTLS_MAX*2);
        if( papTLSList == NULL )
            CPLEmergencyError("CPLGetTLSList() failed to allocate TLS list!");
        if( TlsSetValue( nTLSKey, papTLSList ) == 0 )
        {
            CPLEmergencyError( "CPLGetTLSList(): TlsSetValue() failed!" );
        }
    }

    return papTLSList;
}

/************************************************************************/
/*                             CPLFinalizeTLS()                         */
/************************************************************************/

void CPLFinalizeTLS()
{
    CPLCleanupTLS();
}

/************************************************************************/
/*                           CPLCleanupTLS()                            */
/************************************************************************/

void CPLCleanupTLS()

{
    void **papTLSList;

    if( !bTLSKeySetup )
        return;

    papTLSList = (void **) TlsGetValue( nTLSKey );
    if( papTLSList == NULL )
        return;

    TlsSetValue( nTLSKey, NULL );

    CPLCleanupTLSList( papTLSList );
}

/* endif CPL_MULTIPROC_WIN32 */

#elif defined(CPL_MULTIPROC_PTHREAD)

#include <pthread.h>
#include <time.h>
#include <unistd.h>

  /************************************************************************/
  /* ==================================================================== */
  /*                        CPL_MULTIPROC_PTHREAD                         */
  /*                                                                      */
  /*    PTHREAD Implementation of multiprocessing functions.              */
  /* ==================================================================== */
  /************************************************************************/

/************************************************************************/
/*                             CPLGetNumCPUs()                          */
/************************************************************************/

int CPLGetNumCPUs()
{
#ifdef _SC_NPROCESSORS_ONLN
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
    return 1;
#endif
}

/************************************************************************/
/*                      CPLCreateOrAcquireMutex()                       */
/************************************************************************/

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
static void *CPLCreateMutexInternal(int bAlreadyInGlobalLock);

int CPLCreateOrAcquireMutex( void **phMutex, double dfWaitInSeconds )

{
    int bSuccess = FALSE;

    pthread_mutex_lock(&global_mutex);
    if( *phMutex == NULL )
    {
        *phMutex = CPLCreateMutexInternal(TRUE);
        bSuccess = *phMutex != NULL;
        pthread_mutex_unlock(&global_mutex);
    }
    else
    {
        pthread_mutex_unlock(&global_mutex);

        bSuccess = CPLAcquireMutex( *phMutex, dfWaitInSeconds );
    }

    return bSuccess;
}

/************************************************************************/
/*                        CPLGetThreadingModel()                        */
/************************************************************************/

const char *CPLGetThreadingModel()

{
    return "pthread";
}

/************************************************************************/
/*                           CPLCreateMutex()                           */
/************************************************************************/

typedef struct _MutexLinkedElt MutexLinkedElt;
struct _MutexLinkedElt
{
    pthread_mutex_t   sMutex;
    _MutexLinkedElt  *psPrev;
    _MutexLinkedElt  *psNext;
};
static MutexLinkedElt* psMutexList = NULL;

static void CPLInitMutex(MutexLinkedElt* psItem)
{
#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(HAVE_PTHREAD_MUTEX_RECURSIVE)
    {
        pthread_mutexattr_t  attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
        pthread_mutex_init( &(psItem->sMutex), &attr );
    }
/* BSDs have PTHREAD_MUTEX_RECURSIVE as an enum, not a define. */
/* But they have #define MUTEX_TYPE_COUNTING_FAST   PTHREAD_MUTEX_RECURSIVE */
#elif defined(MUTEX_TYPE_COUNTING_FAST)
    {
        pthread_mutexattr_t  attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, MUTEX_TYPE_COUNTING_FAST );
        pthread_mutex_init( &(psItem->sMutex), &attr );
    }
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
    pthread_mutex_t tmp_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    psItem->sMutex = tmp_mutex;
#else
#error "Recursive mutexes apparently unsupported, configure --without-threads" 
#endif
}

static void *CPLCreateMutexInternal(int bAlreadyInGlobalLock)
{
    MutexLinkedElt* psItem = (MutexLinkedElt *) malloc(sizeof(MutexLinkedElt));
    if (psItem == NULL)
        return NULL;

    if( !bAlreadyInGlobalLock )
        pthread_mutex_lock(&global_mutex);
    psItem->psPrev = NULL;
    psItem->psNext = psMutexList;
    if( psMutexList )
        psMutexList->psPrev = psItem;
    psMutexList = psItem;
    if( !bAlreadyInGlobalLock )
        pthread_mutex_unlock(&global_mutex);

    CPLInitMutex(psItem);

    // mutexes are implicitly acquired when created.
    CPLAcquireMutex( &(psItem->sMutex), 0.0 );

    return psItem;
}

void *CPLCreateMutex()
{
    return CPLCreateMutexInternal(FALSE);
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

int CPLAcquireMutex( void *hMutexIn, CPL_UNUSED double dfWaitInSeconds )
{
    int err;

    /* we need to add timeout support */
    MutexLinkedElt* psItem = (MutexLinkedElt *) hMutexIn;
    err =  pthread_mutex_lock( &(psItem->sMutex) );
    
    if( err != 0 )
    {
        if( err == EDEADLK )
            fprintf(stderr, "CPLAcquireMutex: Error = %d/EDEADLK", err );
        else
            fprintf(stderr, "CPLAcquireMutex: Error = %d", err );

        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

void CPLReleaseMutex( void *hMutexIn )

{
    MutexLinkedElt* psItem = (MutexLinkedElt *) hMutexIn;
    pthread_mutex_unlock( &(psItem->sMutex) );
}

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

void CPLDestroyMutex( void *hMutexIn )

{
    MutexLinkedElt* psItem = (MutexLinkedElt *) hMutexIn;
    pthread_mutex_destroy( &(psItem->sMutex) );
    pthread_mutex_lock(&global_mutex);
    if( psItem->psPrev )
        psItem->psPrev->psNext = psItem->psNext;
    if( psItem->psNext )
        psItem->psNext->psPrev = psItem->psPrev;
    if( psItem == psMutexList )
        psMutexList = psItem->psNext;
    pthread_mutex_unlock(&global_mutex);
    free( hMutexIn );
}

/************************************************************************/
/*                          CPLReinitAllMutex()                         */
/************************************************************************/

/* Used by gdalclientserver.cpp just after forking, to avoid */
/* deadlocks while mixing threads with fork */
void CPLReinitAllMutex();
void CPLReinitAllMutex()
{
    MutexLinkedElt* psItem = psMutexList;
    while(psItem != NULL )
    {
        CPLInitMutex(psItem);
        psItem = psItem->psNext;
    }
    pthread_mutex_t tmp_global_mutex = PTHREAD_MUTEX_INITIALIZER;
    global_mutex = tmp_global_mutex;
}

/************************************************************************/
/*                            CPLCreateCond()                           */
/************************************************************************/

void  *CPLCreateCond()
{
    pthread_cond_t* pCond = (pthread_cond_t* )malloc(sizeof(pthread_cond_t));
    if (pCond)
        pthread_cond_init(pCond, NULL);
    return pCond;
}

/************************************************************************/
/*                            CPLCondWait()                             */
/************************************************************************/

void  CPLCondWait( void *hCond, void* hMutex )
{
    pthread_cond_t* pCond = (pthread_cond_t* )hCond;
    pthread_mutex_t * pMutex = (pthread_mutex_t *)hMutex;
    pthread_cond_wait(pCond,  pMutex);
}

/************************************************************************/
/*                            CPLCondSignal()                           */
/************************************************************************/

void  CPLCondSignal( void *hCond )
{
    pthread_cond_t* pCond = (pthread_cond_t* )hCond;
    pthread_cond_signal(pCond);
}

/************************************************************************/
/*                           CPLCondBroadcast()                         */
/************************************************************************/

void  CPLCondBroadcast( void *hCond )
{
    pthread_cond_t* pCond = (pthread_cond_t* )hCond;
    pthread_cond_broadcast(pCond);
}

/************************************************************************/
/*                            CPLDestroyCond()                          */
/************************************************************************/

void  CPLDestroyCond( void *hCond )
{
    pthread_cond_t* pCond = (pthread_cond_t* )hCond;
    pthread_cond_destroy(pCond);
    free(hCond);
}

/************************************************************************/
/*                            CPLLockFile()                             */
/*                                                                      */
/*      This is really a stub implementation, see first                 */
/*      CPLLockFile() for caveats.                                      */
/************************************************************************/

void *CPLLockFile( const char *pszPath, double dfWaitInSeconds )

{
    FILE      *fpLock;
    char      *pszLockFilename;
    
/* -------------------------------------------------------------------- */
/*      We use a lock file with a name derived from the file we want    */
/*      to lock to represent the file being locked.  Note that for      */
/*      the stub implementation the target file does not even need      */
/*      to exist to be locked.                                          */
/* -------------------------------------------------------------------- */
    pszLockFilename = (char *) CPLMalloc(strlen(pszPath) + 30);
    sprintf( pszLockFilename, "%s.lock", pszPath );

    fpLock = fopen( pszLockFilename, "r" );
    while( fpLock != NULL && dfWaitInSeconds > 0.0 )
    {
        fclose( fpLock );
        CPLSleep( MIN(dfWaitInSeconds,0.5) );
        dfWaitInSeconds -= 0.5;

        fpLock = fopen( pszLockFilename, "r" );
    }
        
    if( fpLock != NULL )
    {
        fclose( fpLock );
        CPLFree( pszLockFilename );
        return NULL;
    }

    fpLock = fopen( pszLockFilename, "w" );

    if( fpLock == NULL )
    {
        CPLFree( pszLockFilename );
        return NULL;
    }

    fwrite( "held\n", 1, 5, fpLock );
    fclose( fpLock );

    return pszLockFilename;
}

/************************************************************************/
/*                           CPLUnlockFile()                            */
/************************************************************************/

void CPLUnlockFile( void *hLock )

{
    char *pszLockFilename = (char *) hLock;

    if( hLock == NULL )
        return;
    
    VSIUnlink( pszLockFilename );
    
    CPLFree( pszLockFilename );
}

/************************************************************************/
/*                             CPLGetPID()                              */
/************************************************************************/

GIntBig CPLGetPID()

{
    return (GIntBig) pthread_self();
}

/************************************************************************/
/*                       CPLStdCallThreadJacket()                       */
/************************************************************************/

typedef struct {
    void *pAppData;
    CPLThreadFunc pfnMain;
    pthread_t hThread;
    int bJoinable;
} CPLStdCallThreadInfo;

static void *CPLStdCallThreadJacket( void *pData )

{
    CPLStdCallThreadInfo *psInfo = (CPLStdCallThreadInfo *) pData;

    psInfo->pfnMain( psInfo->pAppData );

    if (!psInfo->bJoinable)
        CPLFree( psInfo );

    return NULL;
}

/************************************************************************/
/*                          CPLCreateThread()                           */
/*                                                                      */
/*      The WIN32 CreateThread() call requires an entry point that      */
/*      has __stdcall conventions, so we provide a jacket function      */
/*      to supply that.                                                 */
/************************************************************************/

int CPLCreateThread( CPLThreadFunc pfnMain, void *pThreadArg )

{

    CPLStdCallThreadInfo *psInfo;
    pthread_attr_t hThreadAttr;

    psInfo = (CPLStdCallThreadInfo*) CPLCalloc(sizeof(CPLStdCallThreadInfo),1);
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;
    psInfo->bJoinable = FALSE;

    pthread_attr_init( &hThreadAttr );
    pthread_attr_setdetachstate( &hThreadAttr, PTHREAD_CREATE_DETACHED );
    if( pthread_create( &(psInfo->hThread), &hThreadAttr, 
                        CPLStdCallThreadJacket, (void *) psInfo ) != 0 )
    {
        CPLFree( psInfo );
        return -1;
    }

    return 1; /* can we return the actual thread pid? */
}

/************************************************************************/
/*                      CPLCreateJoinableThread()                       */
/************************************************************************/

void* CPLCreateJoinableThread( CPLThreadFunc pfnMain, void *pThreadArg )

{
    CPLStdCallThreadInfo *psInfo;
    pthread_attr_t hThreadAttr;

    psInfo = (CPLStdCallThreadInfo*) CPLCalloc(sizeof(CPLStdCallThreadInfo),1);
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;
    psInfo->bJoinable = TRUE;

    pthread_attr_init( &hThreadAttr );
    pthread_attr_setdetachstate( &hThreadAttr, PTHREAD_CREATE_JOINABLE );
    if( pthread_create( &(psInfo->hThread), &hThreadAttr,
                        CPLStdCallThreadJacket, (void *) psInfo ) != 0 )
    {
        CPLFree( psInfo );
        return NULL;
    }

    return psInfo;
}

/************************************************************************/
/*                          CPLJoinThread()                             */
/************************************************************************/

void CPLJoinThread(void* hJoinableThread)
{
    CPLStdCallThreadInfo *psInfo = (CPLStdCallThreadInfo*) hJoinableThread;

    void* status;
    pthread_join( psInfo->hThread, &status);

    CPLFree(psInfo);
}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )

{
    struct timespec sRequest, sRemain;

    sRequest.tv_sec = (int) floor(dfWaitInSeconds);
    sRequest.tv_nsec = (int) ((dfWaitInSeconds - sRequest.tv_sec)*1000000000);
    nanosleep( &sRequest, &sRemain );
}

static pthread_key_t  oTLSKey;
static pthread_once_t oTLSKeySetup = PTHREAD_ONCE_INIT;

/************************************************************************/
/*                             CPLMake_key()                            */
/************************************************************************/

static void CPLMake_key()

{
    if( pthread_key_create( &oTLSKey, (void (*)(void*)) CPLCleanupTLSList ) != 0 )
    {
        CPLError( CE_Fatal, CPLE_AppDefined, "pthread_key_create() failed!" );
    }
}

/************************************************************************/
/*                             CPLFinalizeTLS()                         */
/************************************************************************/

void CPLFinalizeTLS()
{
    CPLCleanupTLS();
    /* See #5509 for the explanation why this may be needed */
    pthread_key_delete(oTLSKey);
}

/************************************************************************/
/*                             CPLCleanupTLS()                          */
/************************************************************************/

void CPLCleanupTLS()

{
    void **papTLSList;

    papTLSList = (void **) pthread_getspecific( oTLSKey );
    if( papTLSList == NULL )
        return;

    pthread_setspecific( oTLSKey, NULL );

    CPLCleanupTLSList( papTLSList );
}

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **CPLGetTLSList()

{
    void **papTLSList;

    if ( pthread_once(&oTLSKeySetup, CPLMake_key) != 0 )
    {
        CPLEmergencyError( "CPLGetTLSList(): pthread_once() failed!" );
    }

    papTLSList = (void **) pthread_getspecific( oTLSKey );
    if( papTLSList == NULL )
    {
        papTLSList = (void **) VSICalloc(sizeof(void*),CTLS_MAX*2);
        if( papTLSList == NULL )
            CPLEmergencyError("CPLGetTLSList() failed to allocate TLS list!");
        if( pthread_setspecific( oTLSKey, papTLSList ) != 0 )
        {
            CPLEmergencyError( 
                "CPLGetTLSList(): pthread_setspecific() failed!" );
        }
    }

    return papTLSList;
}

#endif /* def CPL_MULTIPROC_PTHREAD */

/************************************************************************/
/*                             CPLGetTLS()                              */
/************************************************************************/

void *CPLGetTLS( int nIndex )

{
    void** papTLSList = CPLGetTLSList();

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    return papTLSList[nIndex];
}

/************************************************************************/
/*                             CPLSetTLS()                              */
/************************************************************************/

void CPLSetTLS( int nIndex, void *pData, int bFreeOnExit )

{
    CPLSetTLSWithFreeFunc(nIndex, pData, (bFreeOnExit) ? CPLFree : NULL);
}

/************************************************************************/
/*                      CPLSetTLSWithFreeFunc()                         */
/************************************************************************/

/* Warning : the CPLTLSFreeFunc must not in any case directly or indirectly */
/* use or fetch any TLS data, or a terminating thread will hang ! */
void CPLSetTLSWithFreeFunc( int nIndex, void *pData, CPLTLSFreeFunc pfnFree )

{
    void **papTLSList = CPLGetTLSList();

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    papTLSList[nIndex] = pData;
    papTLSList[CTLS_MAX + nIndex] = (void*) pfnFree;
}
