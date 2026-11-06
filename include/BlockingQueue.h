#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <utility>
//模板类定义和实现不能分开
template <typename T>
class BlockingQueue
{
private:
    std::queue<T> Tasks_q;  //创建任务队列
    std::mutex mtx;
    std::condition_variable T_cv;
    bool stopped=false;
public:
    //防御式编程:
    //(1).后续就算手动写了其他构造函数，还是能确保无参创建的默认构造存在，从而避免出错
    BlockingQueue()=default;
    //(2).禁用拷贝构造函数和拷贝赋值操作符，锁在操作系统的底层，对应的是 CPU 的硬件原语和内核资源，是绝对不能拷贝复制的，防止后续代码修改出错
    //拷贝构造：BlockingQueue<int> q2 = q1;或BlockingQueue<int> q3(q1);    用一个已经存在的对象，去创建并初始化一个全新的对象
    //拷贝赋值操作符：q2 = q1;          两个对象都已经存在了，把其中一个强行盖在另一个身上
    BlockingQueue(const BlockingQueue&)=delete;   //相当于：BlockingQueue(const BlockingQueue& other)=delete;编译器在头文件里只关心参数类型，不关心参数名字，所以名字省略
    BlockingQueue& operator=(const BlockingQueue&)=delete;


    //提供给外界的接口
    //1.push()接口，生产者推入数据
    void push(T value)
    {
        {
            std::unique_lock<std::mutex> lcok(mtx);
            //stopped判断
            if(stopped)
            {
                return;
            }
            Tasks_q.push(value);
        }
        T_cv.notify_one();
    }

    bool try_push(T value, size_t limit)
    {
        { std::lock_guard<std::mutex> lock(mtx); if(stopped || Tasks_q.size()>=limit) return false; Tasks_q.push(std::move(value)); }
        T_cv.notify_one(); return true;
    }

    //2.pop()接口，消费者取数据
    bool pop(T &value)        //传递一个参数来接收取出的值，要引用传递不能值传递
    {
        {
            std::unique_lock<std::mutex> lock(mtx);
            //捕获方式：this表示捕获this指针
            T_cv.wait(lock,[this]()
            {
                return !Tasks_q.empty()||stopped;
            });
            //判断是否无法取出数据
            if(stopped&&Tasks_q.empty())
            {
                return false;
            }
        
            //成功获取数据
            value=Tasks_q.front();
            Tasks_q.pop();
            return true;
        }
    }
    //3.stop()接口，停止生产者填入数据，唤醒所有进程
    void stop()
    {
        {
            std::unique_lock<std::mutex> lock(mtx);
            stopped=true;
        }
        T_cv.notify_all();
    }
    //4.检查任务队列是否为空
    bool empty()
    {
        {
            std::unique_lock<std::mutex>lock(mtx);
            return Tasks_q.empty();
        }
    }
    //5.获取任务队列的任务数量
    size_t size()
    {
        {
            std::unique_lock<std::mutex>lock(mtx);
            return Tasks_q.size();
        }
    }
};