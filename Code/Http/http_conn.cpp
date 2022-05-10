#include"http_conn.h"

//静态成员变量初始化
int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

//网站的根目录
const char* doc_root = "/home/xiaodexin/桌面/MyProject2_WebServer/Resources";

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
    m_content_length = 0;

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
    //遇到空行，表示头部解析完成
    if(text[0] == '\n'){
        //若HTTP请求有消息体，则还需要读取m_content_length字节的消息体
        //状态机转移到CHECK_STATE_CONTENT状态
        if( m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    } else if(strncasecmp(text, "Connection:", 11) == 0){
        //处理Connection 头部字段 Connection: keep-alive
        text += 11;
        text += strspn(text, " \t");    //strspn返回字符串中第一个不在指定字符串中出现的字符下标,即若text开头有\t，则跳过
        if(strcasecmp(text, "keep-alive") == 0){
            m_linger = true;
        }
    } else if(strncasecmp(text, "Content-Length:", 15) == 0){
        //处理Content-Length字段
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text); //char转为long int
    } else if(strncasecmp(text, "Host:", 5) == 0){
        //处理Host头部字段
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    } else{
        std::cout<<"oop! 其他字段，无法识别: "<<text<<std::endl;
    }
    return NO_REQUEST;
}

//解析HTTP请求体
//其实并没有真正的去解析请求体，只是判断它是否被完整的读入了
http_conn::HTTP_CODE http_conn::prase_request_content(char * text)
{
    //一行一行的读取请求体，直到读到请求体最后一行（当m_read_idx >= (m_content_length + m_checked_index)时就是最后一行）
    //此时说明请求体已全部读到，return GET_REQUEST
    if( m_read_idx >= (m_content_length + m_checked_index)){
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
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
            return LINE_BAD;//其余情况出错（上一个字符不是\r）
        }
        return LINE_OPEN;   //？？？？存疑：为啥不是\r\n就要return LINEOPEN，不应该是循环直到有\r或者\n吗
                            //已搞清：是上面的if语句中的return都没有执行到的话才会执行到这，返回一个LINE_OPEN，即还没有解析到/r/n，数据还不完整
    }
    return LINE_OK;    //这里也不明白为啥return LINE OK，正常是不会到这的，即使因为m_checked_index > m_read_idx执行到这了，也不应该return LINE OK
                       //已搞懂：若因为m_checked_index > m_read_idx执行到这了，此时m_checked_index是指向m_read_idx的下一位的，但m_start_line还是指向m_read_buf中的最后一行数据的行首，此时还是LINE_OK的
}

//当得到一个完整的、正确的HTTP请求时，我们就分析目标文件的属性，
//如果目标文件存在，对所有用户可读，且不是目录，则使用mmap将其
//映射到内存地址m_file_address出，并告诉调用者获取文件成功
http_conn::HTTP_CODE http_conn::do_request(){
    // "/home/xiaodexin/桌面/MyProject2_WebServer"
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len -1);
    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if(stat(m_real_file, &m_file_stat) < 0){
        return NO_REQUEST;
    }

    //判断访问权限
    if(!(m_file_stat.st_mode & S_IROTH)){
        return FORBIDDEN_REQUEST;
    }

    //判断是否是目录
    if(S_ISDIR(m_file_stat.st_mode)){
        return BAD_REQUEST;
    }

    //以只读方式打开文件
    int fd = open(m_real_file, O_RDONLY);
    //创建内存映射
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;

}


http_conn::HTTP_CODE http_conn::process_write(HTTP_CODE read_ret){

}

//处理客户端的请求(线程池中的工作线程即子线程执行的代码)
void http_conn::process()
{
    //解析HTTP请求
    //有限状态机
    std::cout<<"process_read解析请求......"<<std::endl;
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST){
        //请求不完整，需要继续读客户端，要重置一下事件
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    
    //生成响应
    std::cout<<"process_write生成响应..."<<std::endl;
    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);

}





