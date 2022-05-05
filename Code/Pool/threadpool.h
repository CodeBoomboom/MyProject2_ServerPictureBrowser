/********************************************************************
@FileName:threadpool.h
@Version: 1.0
@Notes:   线程池类
@Author:  XiaoDexin
@Email:   xiaodexin0701@163.com
@Date:    2022/05/03 13:48:06
********************************************************************/
#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include<pthread.h>
#include<list>
#include<exception>
#include<cstdio>
#include"locker.h"

//线程池类
template<class T>       //定义成模板是为了代码复用，模板参数T是任务类
class threadpool
{
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);
private:
    //线程数量
    int m_thread_number;
    
    //线程池数组，大小为线程数量
    pthread_t * m_threads;

    //请求队列中最多允许的等待处理的请求数量
    int m_max_requests;

    //请求队列
    std::list< T*> m_workqueue;

    //互斥锁
    locker m_queuelocker;

    //信号量用来判断是否有任务需要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;
private:
    //子线程处理函数
    static void* worker(void* arg);
    void run();
};

/********************************************************************
@FunName:threadpool(int thread_number, int max_requests)
@Input:  thread_number：线程池线程数量
         max_requests：请求队列中最多允许的等待处理的请求数量
@Output: None
@Retuval:None
@Notes:  构造函数，对线程池进行初始化
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/03 14:45:28
********************************************************************/
template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests):
    m_thread_number(thread_number),m_max_requests(max_requests),
    m_stop(false), m_threads(NULL){
    
    if((thread_number <= 0) || (max_requests) <= 0){
        throw std::exception();
    }

    //创建线程池，内有m_thread_number个线程
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }

    //创建thread_number个线程，并将它们设置为线程分离
    for(int i = 0; i<thread_number; i++){
        printf("create the %dth thread\n", i);

        //通过第一个参数m_threads + i就将每个子线程按顺序创建出来了
        if(pthread_create(m_threads + i, NULL, worker, (void*)this) != 0){   //C++中worker必须是一个静态函数，无法访问非静态成员，所以只能通过将this指针传给worker来实现对当前对象的非静态成员的访问
            delete [] m_threads;
            throw std::exception();
        }
        //线程分离
        if(pthread_detach(m_threads[i]) != 0){
            delete [] m_threads;
            throw std::exception();
        }    
    }
}

/********************************************************************
@FunName:~threadpool()
@Input:  None
@Output: None
@Retuval:None
@Notes:  析构函数，关闭线程池
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/03 14:47:02
********************************************************************/
template<typename T>
threadpool<T>::~threadpool(){
    delete [] m_threads;
    m_stop = true;
}


/********************************************************************
@FunName:append(T* request)
@Input:  T* request:任务队列
@Output: None
@Retuval:true：添加成功。false：添加失败
@Notes:  向任务队列中添加任务
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/03 14:48:46
********************************************************************/
template<typename T>
bool threadpool<T>::append(T* request){

    m_queuelocker.lock();   //上锁，线程同步
    if(m_workqueue.size() > m_max_requests){
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

/********************************************************************
@FunName:worker(void* arg)
@Input:  arg:当前对象的this指针(需强转)
@Output: None
@Retuval:None
@Notes:  子线程处理函数，当有任务添加到任务队列时，调用子线程处理
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/03 17:16:24
********************************************************************/
template<class T>
void* threadpool<T>::worker(void* arg){
    //注意：在静态函数里不能访问非静态成员变量/函数，只能通过传this指针来实现对当前对象的非静态成员的访问
    threadpool * pool = (threadpool*)arg;
    pool->run();   
}

/********************************************************************
@FunName:void run()
@Input:  None
@Output: None
@Retuval:None
@Notes:  子线程运行函数，由子线程处理函数worker调用。从任务队列中取一个任务
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/03 17:20:10
********************************************************************/
template<class T>
void threadpool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();//若信号量>0,则-1，否则阻塞在此
        m_queuelocker.lock();
        if(m_workqueue.empty()){//没数据
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if(!request){//若没有获取到则continue
            continue;
        }

        request->process();//process：任务函数。因为用的是proactor模式，所以到这一步的时候数据已经获取到了
    }
}








#endif

