#include"http_conn.h"

//静态成员变量初始化
int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

// 定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

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
    m_write_idx = 0;
    m_method = GET;         // 默认请求方式为GET
    m_check_state = CHECK_STATE_REQUESTLINE;    //初始化状态为 当前正在解析请求首行
    m_checked_index = 0;
    m_start_line = 0;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
    m_linger = false;
    m_content_length = 0;
    bzero(m_read_buf,READ_BUFFER_SIZE);
    bzero(m_write_buf, READ_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
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

//非阻塞的分散写
//写HTTP响应到客户端，此函数在main中被调用
bool http_conn::write()
{
    std::cout<<"一次性写完数据"<<std::endl;
    int temp= 0;
    int bytes_have_send = 0; //已经发送的字节
    int bytes_to_send = m_write_idx;//将要发送的字节（m_write_idx）写缓冲区中待发送的字节数

    if(bytes_to_send == 0){
        //将要发送的字节数为0，这一次响应结束
        modfd(m_epollfd, m_sockfd, EPOLLIN);//由于用了EPOLLONESHOT，所以每次读写结束都要重新modfd
        init();
        return true;
    }

    //轮询写
    while(1){
        //分散写
        temp = Writev(m_sockfd, m_iv, m_iv_count);
        if ( temp <= -1 ) {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if( errno == EAGAIN ) {
                modfd( m_epollfd, m_sockfd, EPOLLOUT );
                return true;
            }
            unmap();//否则说明发送失败，先关闭mmap映射，然后return false
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if ( bytes_to_send <= bytes_have_send ) {
            // 发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接
            unmap();
            if(m_linger) {
                init();
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return true;
            } else {
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return false;
            } 
        }
    }

    return true;
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
//映射到内存地址m_file_address中，并告诉调用者获取文件成功
http_conn::HTTP_CODE http_conn::do_request(){
    // "/home/xiaodexin/桌面/MyProject2_WebServer"
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len -1);
    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if(Stat(m_real_file, &m_file_stat) < 0){
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
    int fd = Open(m_real_file, O_RDONLY);
    //创建内存映射
    m_file_address = (char*)Mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);    //mmap:使一个磁盘文件与存储空间中的一个缓冲区相映射
    Close(fd);
    return FILE_REQUEST;

}

//根据服务器处理HTTP请求的结果，决定返回给客户端的内容
//这个函数其实是生成对应的响应，真正的写回客户端是在write()函数中实现的，该函数在main中被调用
bool http_conn::process_write(HTTP_CODE read_ret){
    switch(read_ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form)){//出错的话响应体也发错误信息
                return false;
            }
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form)){
                return false;
            }
            break;
        }
        case NO_RESOURCE:
        {
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) {
                return false;
            }
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200, ok_200_title );//把响应首行加入m_write_buf
            add_headers(m_file_stat.st_size);//把响应头加入m_write_buf
            m_iv[ 0 ].iov_base = m_write_buf;//要发的响应行和响应头的内存块m_write_buf
            m_iv[ 0 ].iov_len = m_write_idx;
            m_iv[ 1 ].iov_base = m_file_address;//要发的响应体的内存块
            m_iv[ 1 ].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            return true;
        }
        default:
            return false;
    }
    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

//向写缓冲区m_write_buf中添加一行数据
//format：格式，...：可变参数
bool http_conn::add_response( const char* format, ... )
{
    if( m_write_idx >= WRITE_BUFFER_SIZE){
        return false;   //写缓冲区写满
    }
    va_list arg_list;   //解析可变参数的参数指针
    va_start( arg_list, format );// va_start使用第一个可选参数的位置来初始化arg_list参数指针,该宏的第二个参数必须是该函数最后一个有名称参数的名称(即feomat)
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE-1-m_write_idx, format, arg_list);
    /*  int _vsnprintf(char* str, size_t size, const char* format, va_list ap); 
        函数功能：将可变参数格式化输出到一个字符数组
        参数说明：
            1. char *str [out],把生成的格式化的字符串存放在这里.
            2. size_t size [in], str可接受的最大字符数 [1]  (非字节数，UNICODE一个字符两个字节),防止产生数组越界.
            3. const char *format [in], 指定输出格式的字符串，它决定了你需要提供的可变参数的类型、个数和顺序。
            4. va_list ap [in], va_list变量. va:variable-argument:可变参数
        返回值：执行成功，返回最终生成字符串的长度，若生成字符串的长度大于size，则将字符串的前size个字符复制到str，同时将原串的长度返回（不包含终止符）；执行失败，返回负值，并置errno  
    */
    if( len >= (WRITE_BUFFER_SIZE-1-m_write_idx)){
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );//当不再需要使用参数指针时，必须调用宏 va_end
    return true;
}

//对内存映射区执行munmap操作
void http_conn::unmap()
{
    if(m_file_address){
        Munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

//添加响应状态行（响应首行）
//status: 状态码  title：状态信息描述
bool http_conn::add_status_line(int status, const char* title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

//添加响应头
//参数content_length：请求体长度。参数实际简化了，因为我们只实现了GET请求的响应，用不了那么多函数
bool http_conn::add_headers( int content_length )
{
    add_content_length(content_length);
    add_content_type();
    add_linger();
    add_blank_line();
}

//添加响应体
bool http_conn::add_content( const char* content )
{
    return add_response( "%s", content );
}

//添加响应类型
//此处做了简化，实际上应该根据客户端不同的请求进行识别
bool http_conn::add_content_type()
{
    return add_response( "Content-Type:%s\r\n", "text/html");
}

//添加响应体长度
bool http_conn::add_content_length( int content_length )
{
    return add_response("Content-Length: %d\r\n", content_length);
}

//添加响应是否保持连接
bool http_conn::add_linger()
{
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}

//添加响应空行
bool http_conn::add_blank_line()
{
    return add_response( "%s", "\r\n" );
}



//处理客户端的请求(线程池中的工作线程即子线程执行的代码)
void http_conn::process()
{
    //解析HTTP请求
    //有限状态机
    std::cout<<"process_read解析请求......"<<std::endl;
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST){
        //请求不完整，需要继续读客户端，要重置一下事件（因为使用了EPOLLONESHOT)
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    
    //生成响应
    //根据解析结果来响应
    std::cout<<"process_write生成响应..."<<std::endl;
    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    std::cout<<"修改fd为EPOLLOUT，监听客户端是否可写..."<<std::endl;
    modfd(m_epollfd, m_sockfd, EPOLLOUT);

}





