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
    init();
}

//初始化连接其余的信息
void http_conn::init(){ //把两个init分开写的原因是此init在解析的过程中要用到，若两个init写在一起会导致把sockfd也初始化了
    m_read_idx = 0;

    m_check_state = CHECK_STATE_REQUESTLINE;    //初始化状态为 当前正在解析请求首行
    m_checked_index = 0;
    m_start_line = 0;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_linger = false;

    bzero(m_read_buf,READ_BUFFER_SIZE);
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

//非阻塞的写
bool http_conn::write()
{
    std::cout<<"一次性写完数据"<<std::endl;
    return true;//还没完成，先return true
}


//主状态机 解析HTTP请求（解析m_read_buf中的数据）
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;

    char * text = 0; 
    while(((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) \
            || (line_status = parse_line()) == LINE_OK){
        //解析到了一行完整的数据，或者解析到了请求体，也是完整的数据

        //获取一行数据
        text = get_line();

        m_start_line = m_checked_index;//行起始位置更新
        std::cout<<"获取到一行HTTP数据:"<<text<<std::endl;

        switch(m_check_state){
            case CHECK_STATE_REQUESTLINE:
            {
                ret = prase_request_line(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }

            case CHECK_STATE_HEADER:
            {
                ret = prase_request_head(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }else if(ret == GET_REQUEST){
                    return do_request();//解析具体的信息
                }
                break;   
            }

            case CHECK_STATE_CONTENT:
            {
                ret = prase_request_content(text);
                if(ret == GET_REQUEST){ //如果解析完了
                    return do_request();//解析具体的信息
                }
                //否则就是有问题
                line_status = LINE_OPEN;
                break;
            }

            default:
            {
                return INTERNAL_ERROR;
            }
        }
        return NO_REQUEST;//若到此还没获取到信息，则说明请求不完整
    }
}

//解析HTTP请求首行，获得请求方法，目标URL，HTTP版本
http_conn::HTTP_CODE http_conn::prase_request_line(char * text){
    //GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t");//strpbrk:判断空格和\t哪一个在text中先出现，（空格先出现，返回空格的索引）

    //GET\0/index.html HTTP/1.1
    *m_url++ = '\0';

    //GET\0
    char * method = text;
    if(strcasecmp(method, "GET") == 0){ //只判断了GET
        m_method = GET;
    }else{
        return BAD_REQUEST;
    }

    // /index.html HTTP/1.1
    m_version = strpbrk(m_url, " \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    // /index.html\0HTTP/1.1
    *m_version++ = '\0';
    if(strcasecmp(m_version, "HTTP/1.1") != 0){
        return BAD_REQUEST;
    }

    // http://192.168.1.1:10000/index.html
    if(strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7; // 192.168.1.1:10000/index.html 
        m_url = strchr(m_url, '/');// /index.html 
    }
    if(!m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER; //已经解析完请求行，改变主状态机状态为检查请求头

    return NO_REQUEST;  //虽然到此解析完了请求行，但还没有将完整的客户请求解析完，所以还是return NO_REQUEST
}

//解析HTTP请求头
http_conn::HTTP_CODE http_conn::prase_request_head(char * text)
{

}

//解析HTTP请求体
http_conn::HTTP_CODE http_conn::prase_request_content(char * text)
{

}

//解析一行(获取一行），根据\r\n来判断
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for(; m_checked_index < m_read_idx; ++m_checked_index){
        temp = m_read_buf[m_checked_index];
        if(temp == '\r'){
            if((m_checked_index + 1) == m_read_idx){
                //解析的当前字符是\r，且当前读缓冲区没有数据了，则认为是不完整的
                return LINE_OPEN;
            }else if(m_read_buf[m_checked_index+1] == '\n'){
                //说明是'\r\n'，则将 m_read_buf[m_checked_index]以及m_read_buf[m_checked_index+1]置为字符串结束符\0，最后m_checked_index指向下一行数据的第一个元素
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;//其余情况出错
        }else if(temp == '\n'){
            //说明上一次检查最后一个字符为'\r'，再有数据来的时候就是'\n'
            if((m_checked_index >1) && (m_read_buf[m_checked_index - 1] == '\r')){
                m_read_buf[m_checked_index - 1] = '\0';
                m_read_buf[m_checked_index++] = '\0';   //先将\n置为\0，再将m_checked_index+1
                return LINE_OK;
            }
            return LINE_BAD;
        }
        return LINE_OPEN;   //？？？？存疑：为啥不是\r\n就要return LINEOPEN，不应该是循环直到有\r或者\n吗

    }
    return LINE_OK;    //这里也不明白为啥return LINE OK，正常是不会到这的，即使因为m_checked_index > m_read_idx执行到这了，也不应该return LINE OK
}


http_conn::HTTP_CODE http_conn::do_request(){



    return ;

}


http_conn::HTTP_CODE http_conn::process_write(HTTP_CODE read_ret){

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
    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);

}





