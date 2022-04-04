/*
    线程池头文件
*/

#ifndef PTHREADPOOL_H
#define PTHREADPOOL_H


#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <assert.h>

class ThreadPool{
public:
    explicit ThreadPool(size_t ThreadCount = 8) : pool_(std::make_shared<Pool>()){
        assert(ThreadCount > 0);
        for(size_t i = 0; i < ThreadCount; ++i){
            std::thread([pool = pool_]{
                std::unique_lock<std::mutex> locker(pool->mtx);
                while(true){
                    if(!pool->tasks.empty()){
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    }
                    else if(pool->isClose){
                        break;
                    }
                    else{
                        pool->cond.wait(locker);
                    }
                }
            }).detach();
        }
    }

    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool(){
        if(static_cast<bool>(pool_)){
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClose = true;
            }
            pool_->cond.notify_all();                   //唤醒所有等待的线程
        }
    }
    template<class F>
    void AddTask(F && task){                            //模板参数将到来的任务加进任务队列
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);  //lock_guard<> 封装一个互斥锁并加锁，离开作用域会自动解锁
            pool_->tasks.emplace(std::forward<F>(task));       //forward()完美转发，转进来的值是什么形式传递给下一个函数也是什么形式
        }
        pool_->cond.notify_one();                            //随机唤醒一个等待的线程
    }

private:
    struct Pool{
        std::mutex mtx;                             //互斥锁
        std::condition_variable cond;               //信号量
        bool isClose;                               //是否关闭
        std::queue<std::function<void()>> tasks;    //任务队列
    };

    std::shared_ptr<Pool> pool_;                     //维护任务队列的只能指针
};

#endif
