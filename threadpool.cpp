#include "threadpool.h"


ThreadPool::ThreadPool(int n_threads)
        : _threads(std::vector<std::thread>(n_threads)),
          _shutdown(false) {
}

void ThreadPool::init() {
    std::size_t sz = _threads.size();
    for (int i = 0; i < sz; ++i) {
        _threads[i] = std::thread(ThreadWorker(this, i));
        cpu_set_t cpuset;
    	CPU_ZERO(&cpuset);
    	CPU_SET(i, &cpuset);
    	int rc = pthread_setaffinity_np(_threads[i].native_handle(),
                                    sizeof(cpu_set_t), &cpuset);
    }
}

void ThreadPool::shutdown() {

    _shutdown = true;
    _conditional_lock.notify_all();
    
    for (auto& thr : _threads)
        if( thr.joinable() ) 
            thr.join();
}


