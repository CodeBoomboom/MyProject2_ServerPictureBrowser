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
#include"./Pool/locker.h"
#include"./Pool/threadpool.h"
#include"signal.h"
#include"./Http/http_conn.h"
#include"./Wrap/wrap.h"

#define MAX_FD  65535   //最大的文件描述符数
#define MAX_EVENT_NUMBER 10000   //监听的最大的事件数量

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

/********************************************************************
@FunName:extern int addfd(int epollfd, int fd)
@Input:  None
@Output: None
@Retuval:None
@Notes:  添加文件描述符到epoll中
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/04 16:08:56
********************************************************************/
extern void addfd(int epollfd, int fd, bool one_shot);

/********************************************************************
@FunName:extern int removefd(int epollfd, int fd);
@Input:  None
@Output: None
@Retuval:None
@Notes:  从epoll中删除文件描述符
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/04 16:09:16
********************************************************************/
extern void removefd(int epollfd, int fd);

/********************************************************************
@FunName:extern void modfd(int epollfd,  int fd, int ev);
@Input:  None
@Output: None
@Retuval:None
@Notes:  修改文件epoll上的描述符
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/04 16:40:00
********************************************************************/
extern void modfd(int epollfd,  int fd, int ev);


int  main(int argc, char* argv[])
{
    if(argc <= 1){
        std::cout<<"按照如下格式运行："<<basename(argv[0])<<"port_number"<<std::endl;   // ./server 端口号
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

    //监听套接字  
    int listenfd = Socket(PF_INET, SOCK_STREAM, 0);
    
    //设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_family = INADDR_ANY;
    address.sin_port = htons(port);
    Bind(listenfd, (struct sockaddr*)&address, sizeof(address));

    //监听
    Listen(listenfd,5);

    //创建epoll对象，事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = Epoll_create(5);


    //将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);    //listenfd不需要添加oneshot
    http_conn::m_epollfd = epollfd;


    while(true){
        int num = Epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);//阻塞监听epoll上的fd

        //循环遍历事件数组
        for(int i = 0; i<num; i++){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                //有新客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = Accept(listenfd, (sockaddr*)&client_address,&client_addrlen);

                if(http_conn::m_user_count >= MAX_FD){
                    //目前连接数满了
                    //*给客户端写一个信息：服务器内部正忙
                    std::cout<<"目前连接数满了"<<std::endl;
                    Close(connfd);
                    continue;
                }
                //将新的客户端的数据初始化，并将此客户端信息加入users数组中
                users[connfd].init(connfd, client_address);       //直接将connfd作为索引，方便之后的操作
            }else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                //对方异常断开或者错误等事件
                users[sockfd].close_conn();
            }else if(events[i].events & EPOLLIN){
                //可读
                if(users[sockfd].read()){//一次性把数据都读完
                    //交给线程池处理
                    pool->append(users + sockfd);   //users + sockfd就是该sockfd的地址，因为sockfd也是users[sockfd]的索引值
                }else{
                    //读失败
                    users[sockfd].close_conn();
                }
            }else if(events[i].events & EPOLLOUT){
                //可写
                if(!users[sockfd].write()){//一次性写完所有数据
                    //写失败
                    users[sockfd].close_conn();
                }
            }
        }
        
    }
    Close(epollfd);
    Close(listenfd);
    delete [] users;
    delete [] pool;
    
    return 0;
}