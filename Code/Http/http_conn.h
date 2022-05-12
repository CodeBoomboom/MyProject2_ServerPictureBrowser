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
#include<string.h>
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
    static const int FILENAME_LEN = 200;        // 文件名的最大长度
    static const int READ_BUFFER_SIZE = 2048;   //读缓冲大小
    static const int WRITE_BUFFER_SIZE = 1024;  //写缓冲大小

    //HTTP请求方法，但我们只支持GET
    enum METHOD{
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT
    };

    /*解析客户端请求时，主状态机的状态
    CHECK_STATE_REQUESTLINE:当前正在分析请求行
    CHECK_STATE_HEADER:当前正在分析请求头
    CHECK_STATE_CONTENT:当前正在解析请求体*/
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    /*从状态机的三种可能状态，即行的读取状态，分别表示
    1.读取到一个完整的行  2.行出错    3. 行数据尚且不完整*/
    enum LINE_STATUS{
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

    /*服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完整的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESCOURCE        :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求，获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已关闭连接
    */
    enum HTTP_CODE{
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESCOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    http_conn(){}
    ~http_conn(){}

    void process(); //处理客户端的请求(线程池的工作线程即子线程执行的代码)
    void init(int sockfd, sockaddr_in &addr);   //初始化新接收的连接（客户端）
    void init();            //初始化连接其余的信息
    
    void close_conn();  //关闭连接
    bool read();        //非阻塞的读
    bool write();       //非阻塞的写



    HTTP_CODE process_read();       //解析HTTP请求（解析m_read_buf中的数据）
    HTTP_CODE prase_request_line(char * text); //解析HTTP请求首行
    HTTP_CODE prase_request_head(char * text); //解析HTTP请求头
    HTTP_CODE prase_request_content(char * text); //解析HTTP请求体
    LINE_STATUS parse_line();    //解析一行(获取一行），根据\r\n来
    inline char * get_line() { return m_read_buf + m_start_line;} //获取一行数据，m_read_buf+m_start_line就是该行数据\
                                                                (在函数parse_line中已经将m_read_buf中的数据按字符串结束符\0分隔开了，所以此时获取m_read_buf+m_start_line获取到的就是该行数据）\
                                                                函数体较少，使用内联函数
    HTTP_CODE do_request(); //具体的解析处理
    void unmap();  //对内存映射区执行munmap操作
    HTTP_CODE process_write(HTTP_CODE read_ret);       //生成HTTP响应

private:
    int m_sockfd;           //该HTTP连接的socket
    sockaddr_in m_address;  //通信的socket地址

    char m_read_buf[READ_BUFFER_SIZE];  //读缓冲区
    int m_read_idx;         //标识读缓冲区中已经读入的客户端数据的最后一个字节的下一个位置
    int m_checked_index;    //当前正在解析的字符在读缓冲区的位置
    int m_start_line;       //当前正在解析的行的起始位置

    char m_real_file[FILENAME_LEN];  //客户请求的目标文件的完整路径，其内容等于doc_root + m_url，doc_root是网站根目录
    char * m_url;           //请求目标文件的文件名
    char * m_version;       //协议版本，支持HTTP1.1
    METHOD m_method;        //请求方法
    char * m_host;          //主机名
    bool m_linger;          //HTTP请求是否要保持连接
    int m_content_length;   //请求体（消息体）长度
    CHECK_STATE m_check_state;  //主状态机当前所处的状态

    char m_write_buf[ WRITE_BUFFER_SIZE ];  // 写缓冲区
    int m_write_idx;                        // 写缓冲区中待发送的字节数
    char* m_file_address;                   // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;                // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];                   // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
    int m_iv_count;

};





#endif
