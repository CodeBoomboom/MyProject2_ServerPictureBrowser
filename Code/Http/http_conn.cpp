#include"http_conn.h"


int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

/********************************************************************
@FunName:void setnonblocking(int fd)
@Input:  fd :要设置的文件描述符
@Output: None
@Retuval:None
@Notes:  设置指定文件描述符为非阻塞
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/04 18:32:32
********************************************************************/
void setnonblocking(int fd){
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}


/********************************************************************
@FunName:int addfd(int epollfd, int fd)
@Input:  epollfd:epoll句柄
         fd：要添加的fd
@Output: None
@Retuval:None
@Notes:  添加文件描述符到epoll中
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/04 16:06:04
********************************************************************/
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    //event.events = EPOLLIN | EPOLLRDHUP;//EPOLLRDHUP是内核2.6.17后才有的，该事件作用是若对端连接断开时，触发此事件，在底层对对端断开进行处理（之前是在上层通过Recv函数返回值判断）
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;//边沿触发
    if(one_shot){
        event.events | EPOLLONESHOT;
    }
    Epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    
    //设置文件描述符为非阻塞
    setnonblocking(fd);
}


/********************************************************************
@FunName:int removefd(int epollfd, int fd)
@Input:  epollfd:epoll句柄
         fd：要删除的fd
@Output: None
@Retuval:None
@Notes:  从epoll中删除文件描述符
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/04 16:06:51
********************************************************************/
void removefd(int epollfd, int fd)
{
    Epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    Close(fd);

}

/********************************************************************
@FunName:void modfd(int epollfd, int fd, int ev)
@Input:  epollfd:epoll句柄
         fd：要修改的fd
         ev：要修改的event
@Output: None
@Retuval:None
@Notes:  修改文件epoll上的描述符,重置socket上的EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件可以触发
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/05/04 16:36:34
********************************************************************/
void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    Epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//初始化连接
void http_conn::init(int sockfd, sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;

    //端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //添加到epoll红黑树中
    addfd(m_epollfd, m_sockfd, true);   //connfd需要有onshot事件
    m_user_count++; //总用户数（客户端数）+1
}

//关闭连接
void http_conn::close_conn(){
    if(m_sockfd != -1){
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;//客户端总数减1
    }
}


//非阻塞的读
//循环读取客户数据，直到无数据可读或者对方关闭连接
bool http_conn::read()
{
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }

    //读取到的字节
    int bytes_read = 0;
    while(true){
        bytes_read = Recv(m_sockfd, m_read_buf+m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);//前面可能已经有数据读到缓冲区了，所以应该保存到缓冲区的m_read_buf+m_read_idx位置，缓冲区的剩余大小也就为READ_BUFFER_SIZE-m_read_idx
        if(bytes_read == -1){
		    if(errno == EAGAIN || errno == EWOULDBLOCK){
			    //没有数据/读完，跳出循环
                break;
		    }else{
                //实际上在Recv中已经封装了出错处理，若真出错了不会走到这
                return false;
            }            
        }else if(bytes_read == 0){
            //对方关闭连接
            return false;
        }
        //读到数据
        //更新m_read_idx
        m_read_idx += bytes_read;
    }
    //若读到了数据，打印一下
    if(m_read_buf){
        std::cout<<"读到了数据:"<<std::endl<<m_read_buf<<std::endl;
    }else{
        std::cout<<"没有数据"<<std::endl;
    }
    return true;
}



//解析HTTP请求（解析m_read_buf中的数据）
HTTP_CODE http_conn::process_read()
{

}

//解析HTTP请求首行
HTTP_CODE http_conn::prase_request_line(char * text)
{

}

//解析HTTP请求头
HTTP_CODE http_conn::prase_request_line(char * text)
{

}

//解析HTTP请求体
HTTP_CODE http_conn::prase_request_content(char * text)
{

}

//解析一行(获取一行），根据\r\n来
LINE_STATUS http_conn::parse_line(char * text)
{

}



//非阻塞的写
bool http_conn::write()
{
    std::cout<<"一次性写完数据"<<std::endl;
    return true;//还没完成，先return true
}

//处理客户端的请求(线程池中的工作线程即子线程执行的代码)
void http_conn::process()
{
    //解析HTTP请求
    //有限状态机
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST){
        //请求不完整，需要继续读客户端，要重置一下事件
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    

    std::cout<<"process解析请求，生成响应"<<std::endl;


    //生成响应

}





