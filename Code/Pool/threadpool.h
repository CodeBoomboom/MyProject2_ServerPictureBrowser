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
    m_stop(false), m_thread(NULL){
    
    if((thread_number <= 0) || (max_requests) <= 0){
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }

    //创建thread_number个线程，并将它们设置为线程分离
    for(int i = 0; i<thread_number; i++){
        printf("create the %dth thread\n", i);

        if(pthread_create(m_threads + i, NULL, worker, NULL) != 0){   //C++中worker必须是一个静态函数
            delete [] m_threads;
            throw std::exception();
        }

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
@Retuval:None
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
    m_workqueue.unlock();
    m_queuestat.post();
    return true;
}

template<class T>
void* threadpool<T>::worker(void* arg){
    //注意：在静态函数里不能访问非静态成员
}


#endif
