/********************************************************************
@FileName:http_conn.h
@Version: 1.0
@Notes:   http任务类。本项目采用的是Peoactor模式，服务器主线程（main）接收到数据后，
将其读出来封装成任务类（本文件），交给子线程（线程池）处理。
@Author:  XiaoDexin
@Email:   xiaodexin0701@163.com
@Date:    2022/05/04 13:57:54
********************************************************************/
#ifndef _HTTP_CONN_H_
#define _HTTP_CONN_H_

#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<errno.h>
#include<stdarg.h>
#include<sys/epoll.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/uio.h>
#include"../Pool/locker.h"
#include"../Wrap/wrap.h"

//任务类
class http_conn{
public:

    static int m_epollfd;       //epollfd是所有的http_conn对象（任务对象）所共享的———所有的socket上的事件都被注册到一个epoll对象中（挂到一棵以epoll为根的红黑树上）
    static int m_user_count;    //统计用户数量


    http_conn();
    ~http_conn();

    void process(); //处理客户端的请求(线程池的工作线程即子线程执行的代码)
    void init(int sockfd, sockaddr_in &addr);   //初始化新接收的连接（客户端）
    void close_conn();  //关闭连接
    bool read();        //非阻塞的读
    bool write();       //非阻塞的写


private:
    int m_sockfd;           //该HTTP连接的socket
    sockaddr_in m_address;  //通信的socket地址


};





#endif
