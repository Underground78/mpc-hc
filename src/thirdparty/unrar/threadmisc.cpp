// Typically we use the same global thread pool for all RAR modules.
static ThreadPool *GlobalPool=NULL;
static uint GlobalPoolUseCount=0;

ThreadPool* CreateThreadPool()
{
  if (GlobalPoolUseCount++ == 0)
    GlobalPool=new ThreadPool(MaxPoolThreads);
  return GlobalPool;
}


void DestroyThreadPool(ThreadPool *Pool)
{
  if (Pool!=NULL && Pool==GlobalPool && GlobalPoolUseCount > 0 && --GlobalPoolUseCount == 0)
    delete GlobalPool;
}


static THREAD_HANDLE ThreadCreate(PTHREAD_PROC Proc,void *Data)
{
#ifdef _UNIX
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  pthread_t pt;
  int Code=pthread_create(&pt,&attr,Proc,(void*)Data);
  if (Code!=0)
  {
    wchar Msg[100];
    swprintf(Msg,ASIZE(Msg),L"\npthread_create failed, code %d\n",Code);
    ErrHandler.GeneralErrMsg(Msg);
    ErrHandler.SysErrMsg();
    ErrHandler.Exit(RARX_FATAL);
  }
  return pt;
#else
  DWORD ThreadId;
  HANDLE hThread=CreateThread(NULL,0x10000,(LPTHREAD_START_ROUTINE)Proc,Data,0,&ThreadId);
  if (hThread==NULL)
  {
    ErrHandler.GeneralErrMsg(L"CreateThread failed");
    ErrHandler.SysErrMsg();
    ErrHandler.Exit(RARX_FATAL);
  }
  return hThread;
#endif
}


static void ThreadClose(THREAD_HANDLE hThread)
{
#ifdef _UNIX
  pthread_join(hThread,NULL);
#else
  CloseHandle(hThread);
#endif
}


static inline void CriticalSectionStart(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  EnterCriticalSection(CritSection);
#elif defined(_UNIX)
  pthread_mutex_lock(CritSection);
#endif
}


static inline void CriticalSectionEnd(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  LeaveCriticalSection(CritSection);
#elif defined(_UNIX)
  pthread_mutex_unlock(CritSection);
#endif
}


#ifdef _WIN_ALL
static void CWaitForSingleObject(HANDLE hHandle)
{
  DWORD rc=WaitForSingleObject(hHandle,INFINITE);
  if (rc==WAIT_FAILED)
  {
    ErrHandler.GeneralErrMsg(L"\nWaitForMultipleObjects error %d, GetLastError %d",rc,GetLastError());
    ErrHandler.Exit(RARX_FATAL);
  }
}
#endif


#ifdef _UNIX
static void cpthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  int rc=pthread_cond_wait(cond,mutex);
  if (rc!=0)
  {
    ErrHandler.GeneralErrMsg(L"\npthread_cond_wait error %d",rc);
    ErrHandler.Exit(RARX_FATAL);
  }
}
#endif


uint GetNumberOfCPU()
{
#ifndef RAR_SMP
  return 1;
#else
#ifdef _UNIX
#ifdef _SC_NPROCESSORS_ONLN
  uint Count=(uint)sysconf(_SC_NPROCESSORS_ONLN);
  return Count<1 ? 1:Count;
#elif defined(_APPLE)
  uint Count;
  size_t Size=sizeof(Count);
  return sysctlbyname("hw.ncpu",&Count,&Size,NULL,0)==0 ? Count:1;
#endif
#else // !_UNIX
  DWORD_PTR ProcessMask;
  DWORD_PTR SystemMask;

  if (!GetProcessAffinityMask(GetCurrentProcess(),&ProcessMask,&SystemMask))
    return 1;
  uint Count=0;
  for (DWORD_PTR Mask=1;Mask!=0;Mask<<=1)
    if ((ProcessMask & Mask)!=0)
      Count++;
  return Count<1 ? 1:Count;
#endif

#endif // RAR_SMP
}


uint GetNumberOfThreads()
{
  uint NumCPU=GetNumberOfCPU();
  if (NumCPU<1)
    return 1;
  if (NumCPU>MaxPoolThreads)
    return MaxPoolThreads;
  return NumCPU;
}

