#ifndef THREAD_POOL_H__
#define THREAD_POOL_H__

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
        int _id;
        ThreadPool * _pool;
    public:
        ThreadWorker(ThreadPool * pool, const int id)
            : _pool(pool), _id(id) {
        }
        
        void operator()() {
            std::function<void ()> func;
            bool pop;
            while (!_pool->_shutdown) {
                {
                    std::unique_lock<std::mutex> lock(_pool->_conditional_mutex);
                    if (_pool->_queue.empty()) {
                        _pool->_conditional_lock.wait(lock);
                    }

                    // double check in case of shutdown
                    if (!_pool->_queue.empty()) {
                        func = std::move(_pool->_queue.front());
                        _pool->_queue.pop();
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
	std::lock_guard<std::mutex> lock(_jobmtx);
        // Create a function with bounded parameters ready to execute
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        // Encapsulate it into a shared ptr in order to be able to copy construct / assign 
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);
        
        // Wrap packaged task into void function
        std::function<void ()> wrapper_func = [task_ptr]() {
            (*task_ptr)();
        };
        
        _queue.push(std::move(wrapper_func));
        
        // Wake up one thread if its waiting
        _conditional_lock.notify_one();
        
        // Return future from promise
        return task_ptr->get_future();
    }
private: 
    bool _shutdown;
    std::queue<std::function<void ()>> _queue;
    std::vector<std::thread> _threads;
    std::mutex _conditional_mutex;
    std::mutex _jobmtx;
    std::condition_variable _conditional_lock;
};

#endif
