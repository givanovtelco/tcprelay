#include "threadpool.hpp"


ThreadPool::ThreadPool(int n_threads)
        : m_threads(std::vector<std::thread>(n_threads)),
          m_shutdown(false) {
}

void ThreadPool::init() {
    std::size_t sz = m_threads.size();
    for (int i = 0; i < sz; ++i) {
        m_threads[i] = std::thread(ThreadWorker(this, i));
        cpu_set_t cpuset;
    	CPU_ZERO(&cpuset);
    	CPU_SET(i, &cpuset);
    	int rc = pthread_setaffinity_np(m_threads[i].native_handle(),
                                    sizeof(cpu_set_t), &cpuset);
    }
}

void ThreadPool::shutdown() {

    m_shutdown = true;
    m_conditional_lock.notify_all();
    
    for (auto& thr : m_threads)
        if( thr.joinable() ) 
            thr.join();
}


