#include "ThreadPool.h"

#include <stdexcept>

#include <signal.h>
#include <unistd.h>

ThreadPool::ThreadPool(size_t threads) : mCancelling(false) {
	Lock lock(mCond);
	
	if (threads == 0)
		threads = systemCPUs();
	mThreads.resize(threads);
	for (size_t i = 0; i < threads; ++i) {
		mThreads.push_back(ThreadInfo(this, i));
		ThreadInfo& info = mThreads.back();
		pthread_create(&info.pthread, 0, &threadFunc, &info);
	}
}

void *ThreadPool::threadFunc(void *val) {
	// Don't run signal handlers on worker threads
	sigset_t allsig;
	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	ThreadInfo *info = reinterpret_cast<ThreadInfo*>(val);
	
	while (true) {
		Job *job = info->pool->nextJob();
		try {
			if (!job)
				pthread_exit(0); // we're being cancelled
			(*job)();
		} catch (...) {
			if (job)
				delete job;
			throw;
		}
		delete job;
	}
}

ThreadPool::~ThreadPool() {
	{
		Lock lock(mCond);
		mCancelling = true;
		
		// delete any jobs left in the queue
		while (!mJobs.empty()) {
			delete mJobs.front();
			mJobs.pop();
		}
		
		for (size_t i = 0; i < mThreads.size(); ++i)
			mJobs.push(0); // request cancellation
		mCond.broadcast();
	}
	
	for (ThreadList::iterator i = mThreads.begin(); i < mThreads.end(); ++i)
		pthread_join(i->pthread, 0);
}

size_t ThreadPool::systemCPUs() const {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

ThreadPool::Job* ThreadPool::nextJob() {
	Lock lock(mCond);
	while (mJobs.empty())
		mCond.wait();
	Job *job = mJobs.front();
	mJobs.pop();
	return job;
}

void ThreadPool::enqueue(Job *job) {
	Lock lock(mCond);
	if (mCancelling)
		throw std::runtime_error("Can't add jobs while cancelling");
	
	mJobs.push(job);
	mCond.signal();
}
