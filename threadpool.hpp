#ifndef THREAD_POOL_HPP__
#define THREAD_POOL_HPP__

#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

class ThreadPool {
private:
    class ThreadWorker {
    private:
        int m_id;
        ThreadPool * m_pool;
    public:
        ThreadWorker(ThreadPool * pool, const int id)
            : m_pool(pool), m_id(id) {
        }
        
        void operator()() {
            std::function<void()> func;
            bool pop;
            while (!m_pool->m_shutdown) {
                {
                    std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);
                    if (m_pool->m_queue.empty()) {
                        m_pool->m_conditional_lock.wait(lock);
                    }

                    // double check in case of shutdown
                    if (!m_pool->m_queue.empty()) {
                        func = std::move(m_pool->m_queue.front());
                        m_pool->m_queue.pop();
                        func();
                    }
                }

            }
        }      
    };

public:
    ThreadPool(int n_threads);
    
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;

    ThreadPool & operator=(const ThreadPool &) = delete;
    ThreadPool & operator=(ThreadPool &&) = delete;

    void init();    
    void shutdown();
    
    template<typename F, typename...Args>
    auto push(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
	std::lock_guard<std::mutex> lock(m_jobmtx);
        // Create a function with bounded parameters ready to execute
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        // Encapsulate it into a shared ptr in order to be able to copy construct / assign 
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);
        
        // Wrap packaged task into void function
        std::function<void()> wrapper_func = [task_ptr]() {
            (*task_ptr)();
        };
        
        m_queue.push(std::move(wrapper_func));
        
        // Wake up one thread if its waiting
        m_conditional_lock.notify_one();
        
        // Return future from promise
        return task_ptr->get_future();
    }
private: 
    bool m_shutdown;
    std::queue<std::function<void()>> m_queue;
    std::vector<std::thread> m_threads;
    std::mutex m_conditional_mutex;
    std::mutex m_jobmtx;
    std::condition_variable m_conditional_lock;
};

#endif
