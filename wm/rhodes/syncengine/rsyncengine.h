#pragma once
#include "../stdafx.h"
#include "../atlutil.h" //Using local copy, as 'standard' atlutil has errors

#include "shttpd.h"
#include "sqlite3.h"

class CSyncEngine : public IWorkerThreadClient
{
  HANDLE m_hEvent;
  HANDLE m_hDoSyncEvent;
  CRITICAL_SECTION m_critical_section;

public:
  static CSyncEngine* Create();
  static CSyncEngine* Instance();
  virtual ~CSyncEngine(void);

  CWorkerThread<DefaultThreadTraits> m_thread;

	void ResumeThread();
	void FreezeThread();

  void SetMainWindow(HWND hWnd) { m_hMainWindow = hWnd; }

  void Lock();
  void Unlock();
  void TriggerSync();
  void StartSync();

private:
  CSyncEngine(void);

private:
  HWND m_hMainWindow;

  HRESULT Execute(DWORD_PTR dwParam, HANDLE hObject);
	HRESULT CloseHandle(HANDLE hHandle);

  sqlite3 *m_database;
  int	m_delaySync;
  bool  m_bSyncInitialized;
  bool  StartSyncEngine();
  bool  PerformSync();
  bool  StopSyncEngine();
};

#ifdef __cplusplus
extern "C" {
#endif

char* fetch_remote_data(char* url);
int push_remote_data(char* url, char* data, size_t data_size);
void start_sync();

#ifdef __cplusplus
} //extern "C" {
#endif
