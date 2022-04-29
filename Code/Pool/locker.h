/********************************************************************
@FileName:locker.h
@Version: 1.0
@Notes:   线程同步机制封装类（互斥锁，条件变量）
@Author:  XiaoDexin
@Email:   xiaodexin0701@163.com
@Date:    2022/04/28 22:06:32
********************************************************************/
#ifndef _LOCKER_H_
#define _LOCKER_H_
#include<pthread.h>
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








#endif
