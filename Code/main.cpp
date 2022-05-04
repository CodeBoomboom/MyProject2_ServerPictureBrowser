#include<iostream>
#include<cstdio>
#include<cstring>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include"Pool/locker.h"
#include"Pool/threadpool.h"
#include"signal.h"
#include"Http/http_conn.h"

#define MAX_FD  65535   //最大的文件描述符数


/********************************************************************
@FunName:void addsig(int sig, void(handler)(int))
@Input:  sig:信号
         void(handler)(int)：回调函数
@Output: None
@Retuval:None
@Notes:  注册信号捕捉
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/04 13:07:24
********************************************************************/
void addsig(int sig, void(handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);   //临时信号mask全部置1，阻塞
    sigaction(sig, &sa, NULL);
}



int  main(int argc, char* argv[])
{
    if(argc <= 1){
        std::cout<<"按照如下格式运行："<<basename(argv[0])<<"port_number"<<std::endl;
        exit(-1);
    }

    //获取端口号
    int port = atoi(argv[1]);   //argv[0]是./server
    
    //对SIGPIE信号做处理，SIGPIPE：向一个没有读端的管道写数据，会触发这个信号，默认为终止进程。
    //此处是网络对端（客户端）关闭时直接忽略
    addsig(SIGPIPE,SIG_IGN);

    //创建线程池，初始化线程池
    threadpool<http_conn> * pool = NULL;
    try{
        pool = new threadpool<http_conn>;
    }catch(...){
        exit(-1);
    }

    //创建一个数组用于保存所有的客户端信息
    http_conn * users = new http_conn[MAX_FD];



    std::cout<<"Webserve Start"<<std::endl;

    return 0;
}