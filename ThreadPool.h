#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "lzopfs.h"

#include <queue>

#include <pthread.h>

class Mutex {
protected:
	pthread_mutex_t mMutex;
	
public:
	Mutex() { pthread_mutex_init(&mMutex, 0); }
	~Mutex() { pthread_mutex_destroy(&mMutex); }
	void lock() { pthread_mutex_lock(&mMutex); }
	void unlock() { pthread_mutex_unlock(&mMutex); };
};

class Lock {
	Mutex& mMutex;
	
public:
	Lock(Mutex& m) : mMutex(m) { mMutex.lock(); }
	~Lock() { mMutex.unlock(); }
};

class ConditionVariable : public Mutex {
	pthread_cond_t mCond;
	
public:
	ConditionVariable() { pthread_cond_init(&mCond, 0); }
	~ConditionVariable() { pthread_cond_destroy(&mCond); }
	
	void wait() { pthread_cond_wait(&mCond, &mMutex); }
	void signal() { pthread_cond_signal(&mCond); }
	void broadcast() { pthread_cond_broadcast(&mCond); }
};


class ThreadPool {
public:
	struct Job {
		virtual void operator()() = 0;
		virtual ~Job() { }
	};

protected:
	struct ThreadInfo {
		ThreadPool *pool;
		pthread_t pthread;
		size_t num;
		ThreadInfo(ThreadPool *p = 0, size_t n = 0) : pool(p), num(n) { }
	};
	typedef std::vector<ThreadInfo> ThreadList;
	ThreadList mThreads;
	ConditionVariable mCond;
	
	typedef std::queue<Job*> JobQ;
	JobQ mJobs;
	bool mCancelling;
	
	
	size_t systemCPUs() const;
	Job *nextJob();
	
	static void *threadFunc(void *val);

public:	
	ThreadPool(size_t threads = 0);
	~ThreadPool();
	
	void enqueue(Job* job);
};

#endif // THREADPOOL_H
