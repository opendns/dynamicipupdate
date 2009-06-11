// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHARED_MEM_H__
#define SHARED_MEM_H__

static inline char *str_append(char *dst, const char *src, size_t len)
{
	memcpy(dst, src, len);
	return dst + len;
}

static inline char *str_cat(const char *s1, const char *s2)
{
	size_t s1len = strlen(s1);
	size_t s2len = strlen(s2);
	size_t len = s1len + s2len;
	char *res = (char*)malloc(len+1);
	char *t = str_append(res, s1, s1len);
	t = str_append(t, s2, s2len);
	*t = 0;
	return res;
}

class MutexAutoLock {
public:
	explicit MutexAutoLock(HANDLE  mutex)
	{
		m_mutex = mutex;
		WaitForSingleObject(m_mutex, INFINITE);
	}
 
    ~MutexAutoLock()
    {
		ReleaseMutex(m_mutex);
	}
private:
	HANDLE m_mutex;
};
 
class SharedMem
{
public:
	const char *	m_name;
	int				m_size;
	void *			m_mem;
	HANDLE			m_memHandle;
	HANDLE			m_mutex;
	HANDLE			m_requestEvent;
	HANDLE			m_responseEvent;

	void SetRequestEvent() {
		SetEvent(m_requestEvent);
	}

	void SetResponseEvent() {
		SetEvent(m_responseEvent);
	}

	static SharedMem *Create(const char* name, int size) {
		SharedMem *o = new SharedMem();
		bool ok = o->CreateHelper(name, size);
		if (!ok) {
			delete o;
			return NULL;
		}
		return o;
	}

	static SharedMem *Open(const char* name) {
		SharedMem *o = new SharedMem();
		bool ok = o->OpenHelper(name);
		if (!ok) {
			delete o;
			return NULL;
		}
		return o;
	}

	~SharedMem() {
		if (m_mem)
			UnmapViewOfFile(m_mem);
		if (m_memHandle)
			CloseHandle(m_memHandle);
		if (m_mutex)
			CloseHandle(m_mutex);
		if (m_requestEvent)
			CloseHandle(m_requestEvent);
		if (m_responseEvent)
			CloseHandle(m_responseEvent);
	}

private:
	SharedMem() :
		  m_name(NULL)
		, m_size(0)
		, m_mem(NULL)
		, m_memHandle(NULL)
		, m_mutex(NULL)
		, m_requestEvent(NULL)
		, m_responseEvent(NULL)
	{
	}

    bool CreateSyncObjects()
	{
		char *mutexName = str_cat(m_name, "_mutex");
		m_mutex = CreateMutexA(NULL, FALSE, mutexName);
		free(mutexName);
		if (NULL == m_mutex)
			return false;

		char *requestEventName = str_cat(m_name, "_request_event");
		m_requestEvent = CreateEventA(NULL, FALSE, FALSE, requestEventName);
		free(requestEventName);
		if (NULL == m_requestEvent)
			return false;

		char *responseEventName = str_cat(m_name, "_response_event");
		m_responseEvent = CreateEventA(NULL, FALSE, FALSE, responseEventName);
		free(responseEventName);
		if (NULL == m_responseEvent)
			return false;

		return true;
	}

	bool CreateHelper(const char* name, int size) {
		m_size = size;
		m_name = name;
		m_memHandle = CreateFileMappingA(INVALID_HANDLE_VALUE,
							NULL, PAGE_READWRITE, 0, size, name);
		if (NULL == m_memHandle)
			return false;

		m_mem = MapViewOfFile(m_memHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if (!m_mem)
			return false;
		return CreateSyncObjects();
	}

	bool OpenHelper(const char *name) {
		m_name = name;
		m_memHandle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
		if (NULL == m_memHandle)
			return false;

		m_mem = MapViewOfFile(m_memHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if (!m_mem)
			return false;
		return CreateSyncObjects();
	}
};

template<typename T, typename Name>
class SharedMemT : public SharedMem
{
public:
	static SharedMemT<T,Name> *Create() {
		SharedMem *o = SharedMem::Create(Name::name(), sizeof(T));
		return static_cast<SharedMemT<T,Name> *>(o);
	}

	static SharedMemT<T,Name> *Open() {
		SharedMem *o = SharedMem::Open(Name::name());
		return static_cast<SharedMemT<T,Name> *>(o);
	}

	/*T* operator->() const {
		return static_cast<T*>(m_mem);
	}*/

	T* GetData() const {
		return static_cast<T*>(m_mem);
	}
};

class SharedMemAutoLock {
public:
	explicit SharedMemAutoLock(SharedMem *m)
	{
		m_mutex = m->m_mutex;
		WaitForSingleObject(m_mutex, INFINITE);
	}
 
    ~SharedMemAutoLock()
    {
		ReleaseMutex(m_mutex);
	}
private:
	HANDLE m_mutex;
};
#endif
