/********************************************************************
@FileName:locker.h
@Version: 1.0
@Notes:   线程同步机制封装类（互斥锁，条件变量，信号量）,这个文件是用自己写的，实际上C++有成熟的封装库
          其实现源码也是像本文件一样，最终都是调C库的pthread.h
          #include<thread>
@Author:  XiaoDexin
@Email:   xiaodexin0701@163.com
@Date:    2022/04/28 22:06:32
********************************************************************/
#ifndef _LOCKER_H_
#define _LOCKER_H_
#include<pthread.h>
#include<semaphore.h>
#include<exception> //异常相关头文件

//线程同步机制封装类

//互斥锁类
class locker {
public:
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL) != 0){
            throw std::exception();
        }
    }

    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t* get(){
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

//条件变量类
class cond {
public:
    //构造函数，初始化条件变量
    cond(){
        if(pthread_cond_init(&m_cond, NULL) != 0){
            throw std::exception();
        }
    }
    //析构函数，销毁条件变量
    ~cond(){
        pthread_cond_destroy(&m_cond);
    }
    //等待，阻塞等待某一条件变量满足
    bool wait(pthread_mutex_t* mutex){
       return  pthread_cond_wait(&m_cond, mutex) == 0;
    }
    //等待，等待某一条件满足，设置超时时间
    bool timewait(pthread_mutex_t* mutex, struct timespec t){
       return  pthread_cond_timedwait(&m_cond, mutex,&t) == 0;
    }
    //一次唤醒阻塞在条件变量上的（至少）一个线程
    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;
    }
    //一次唤醒阻塞在条件变量上的所有线程
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }
    
private:
    pthread_cond_t m_cond;
};


//信号量类
class sem{
public:
    sem(){
        if(sem_init(&m_sem, 0, 0) != 0){
            throw std::exception();
        }
    }
    //num:同时访问的线程数
    sem(int num){
        if(sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }

    ~sem(){
        sem_destroy(&m_sem);
    }

    //等待信号量
    bool wait(){
        return sem_wait(&m_sem) == 0;
    }

    //增加信号量
    bool post(){
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};






#endif
