/********************************************************************
@FileName:wrap.c
@Version: 1.0
@Notes:   封装Socket+epoll相关函数，加入错误提示
@Author:  XiaoDexin
@Email:   xiaodexin0701@163.com
@Date:    2022/04/18 09:55:26
********************************************************************/
#include"wrap.h"

void perr_exit(const char *s)
{
    perror(s);
    exit(-1);
}

/********************************************************************
@FunName:int Socket(int family, int type, int protocol)
@Input:  family:协议类型：AF_INET、AF_INET6、AF_UNIX
         type:数据传输类型：SOCK_STREAM、SOCK_DGRAM
         protocol: 与协议一起使用的特定协议，通常为0 
@Output: None
@Retuval:成功： 新套接字所对应文件描述符
         失败: -1 errno
@Notes:  创建一个网络套接字
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 17:15:11
********************************************************************/
int Socket(int family, int type, int protocol)
{
    int n;
    if((n = socket(family,type,protocol)) < 0)
    {
        perr_exit("socket error");
    }
    return n;
}

/********************************************************************
@FunName:int Bind(int fd, const struct sockaddr *sa, socklen_t salen)   
@Input:  fd:套接字
         *sa:套接字地址
         salen:套接字地址长度,传入参数
@Output: None
@Retuval:成功：0
         失败：-1
@Notes:  将指定网络套接字与指定地址相连接
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 17:22:28
********************************************************************/
int Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
    int n;
    if((n = bind(fd, sa,salen)) < 0)
    {
        perr_exit("bind error");
    }
    return n;
}

/********************************************************************
@FunName:int Listen(int fd, int backlog)
@Input:  fd:套接字
         backlog:同时与服务器建立连接的上限数
@Output: None
@Retuval:成功：0
         失败：-1
@Notes:  设置同时与服务器建立连接的上限数。（同时进行3次握手的客户端数量）
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 17:27:33
********************************************************************/
int Listen(int fd, int backlog)
{
    int n;

	if ((n = listen(fd, backlog)) < 0)
		perr_exit("listen error");

    return n;
}

/********************************************************************
@FunName:int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr)
@Input:  fd:监听套接字
         *sa:传出参数
         addrlen：传入传出。入：传入的addr的大小
@Output: *sa:传出参数，成功与服务器建立连接的那个客户端的地址结构（IP+port）
          addrlen：传入传出。出：客户端addr实际大小。
@Retuval:成功：0
         失败：-1
@Notes:  阻塞等待客户端建立连接，成功的话，返回一个与客户端成功连接的socket文件描述符。
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 17:30:33
********************************************************************/
int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr)
{
    int n;
again:    
    if ((n = accept(fd, sa, salenptr)) < 0) {
        if((errno == ECONNABORTED) || (errno == EINTR)) //出错排除处理：1.软件导致的连接终止 2.中断的系统调用
            goto again;
        else    perr_exit("accept error");
    }
    return n;
}


/********************************************************************
@FunName:int Connect(int fd, const struct sockaddr *sa, socklen_t salen)
@Input:  fd:客户端套接字
         *sa:服务器地址结构
         salen:服务器地址结构长度
@Output: None
@Retuval:成功：0
         失败：-1
@Notes:  使用现有的 socket 与服务器建立连接
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 18:36:25
********************************************************************/
int Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
    int n;

	if ((n = connect(fd, sa, salen)) < 0)
		perr_exit("connect error");

    return n;
}


/********************************************************************
@FunName:ssize_t Read(int fd, void *ptr, size_t nbytes)
@Input:  fd:文件描述符/套接字
         *ptr:缓冲区
         nbytes:缓冲区大小
@Output: None
@Retuval:n>0:读到的字节数
         n=0:读到文件描述符末尾/套接字关闭
         n<0:出错
@Notes:  向文件描述符读数据
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 18:44:07
********************************************************************/
ssize_t Read(int fd, void *ptr, size_t nbytes)
{
	ssize_t n;

again:
	if ( (n = read(fd, ptr, nbytes)) == -1) {
		if (errno == EINTR)
			goto again;
		else
			return -1;
	}
	return n;
}

/********************************************************************
@FunName:ssize_t Write(int fd, const void *ptr, size_t nbytes)
@Input:  fd:文件描述符/套接字
         *ptr:缓冲区
         nbytes:缓冲区大小
@Output: None
@Retuval:成功：写入的字节数
         失败：-1
@Notes:  向文件描述符写数据
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 18:57:15
********************************************************************/
ssize_t Write(int fd, const void *ptr, size_t nbytes)
{
	ssize_t n;

again:
	if ( (n = write(fd, ptr, nbytes)) == -1) {
		if (errno == EINTR)
			goto again;
		else
			return -1;
	}
	return n;
}

/********************************************************************
@FunName:int Close(int fd)
@Input:  fd:文件描述符名
@Output: None
@Retuval:成功：0
         失败：-1
@Notes:  关闭一个文件描述符
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 18:58:59
********************************************************************/
int Close(int fd)
{
    int n;
	if ((n = close(fd)) == -1)
		perr_exit("close error");

    return n;
}

/********************************************************************
@FunName:ssize_t Readn(int fd, void *vptr, size_t n)
@Input:  fd:文件描述符
         *vptr：数据缓缓冲区
         n：应该读取的字节数
@Output: None
@Retuval:成功：实际读到的字节数
         失败：-1
@Notes:  读取指定字节数
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 19:00:56
********************************************************************/
ssize_t Readn(int fd, void *vptr, size_t n)
{
	size_t  nleft;              //usigned int 剩余未读取的字节数
	ssize_t nread;              //int 实际读到的字节数
	char   *ptr;

	ptr = (char*)vptr;
	nleft = n;

	while (nleft > 0) {
		if ((nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;
			else
				return -1;
		} else if (nread == 0)
			break;

		nleft -= nread;
		ptr += nread;
	}
	return n - nleft;
}

/********************************************************************
@FunName:ssize_t Writen(int fd, const void *vptr, size_t n)
@Input:  fd:文件描述符
         *vptr：数据缓缓冲区
         n：应该写入的字节数
@Output: None
@Retuval:成功：实际写入的字节数
         失败：-1
@Notes:  写入制定字节数
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 19:03:22
********************************************************************/
ssize_t Writen(int fd, const void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = (char*)vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;
			else
				return -1;
		}

		nleft -= nwritten;
		ptr += nwritten;
	}
	return n;
}

/********************************************************************
@FunName:static ssize_t my_read(int fd, char *ptr)
@Input:  fd:文件描述符
         *ptr：
@Output: None
@Retuval:None
@Notes:  None
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 19:05:04
********************************************************************/
static ssize_t my_read(int fd, char *ptr)
{
	static int read_cnt;
	static char *read_ptr;
	static char read_buf[100];

	if (read_cnt <= 0) {
again:
		if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
			if (errno == EINTR)
				goto again;
			return -1;
		} else if (read_cnt == 0)
			return 0;
		read_ptr = read_buf;
	}
	read_cnt--;
	*ptr = *read_ptr++;

	return 1;
}

/********************************************************************
@FunName:ssize_t Readline(int fd, void *vptr, size_t maxlen)
@Input:  None
@Output: None
@Retuval:None
@Notes:  读一行数据
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/17 19:07:01
********************************************************************/
ssize_t Readline(int fd, void *vptr, size_t maxlen)
{
	ssize_t n, rc;
	char    c, *ptr;

	ptr = (char*)vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = my_read(fd, &c)) == 1) {
			*ptr++ = c;
			if (c  == '\n')
				break;
		} else if (rc == 0) {
			*ptr = 0;
			return n - 1;
		} else
			return -1;
	}
	*ptr  = 0;

	return n;
}

/********************************************************************
@FunName:int Epoll_create(int size)
@Input:  size:epoll最大容量
@Output: None
@Retuval:成功
		 失败-1
@Notes:  创建监听红黑树。size：创建的红黑树的监听节点数目（仅供内核参考，实际可能比这大——连接的客户端数目比设置的size大时会自动扩容）
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/18 09:50:22
********************************************************************/
int Epoll_create(int size)
{
	int n;
	if((n=epoll_create(size)) <0)
	{
		perr_exit("epoll_create error");
	}
	return n;
}

/********************************************************************
@FunName:int Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
@Input:  epfd：为epoll_creat的句柄（epoll树根节点）
		 op：表示动作，用3个宏来表示：
			EPOLL_CTL_ADD (注册新的fd到监听红黑树)，
			EPOLL_CTL_MOD (修改已经注册的fd在监听红黑树上的监听事件)，
			EPOLL_CTL_DEL (从监听红黑树上删除一个fd，即取消监听)；
		 fd:待监听的fd
		 event：监听事件	本质 struct epoll_event //结构体 地址 
           		events: EPOLLIN/EPOLLOUT/EPOLLERR/EPOLLET
            	data: 联合体
				struct epoll_event {
					__uint32_t events;//Epoll events 
					epoll_data_t data;//User data variable 
				};
				typedef union epoll_data {
					void *ptr;		//epoll反应堆模型（回调函数）:epoll ET模式 + 非阻塞、轮询 + void *ptr
					int fd;			//对应监听事件的fd
					uint32_t u32;	// 不用
					uint64_t u64;	// 不用
				} epoll_data_t;	
				★EPOLLIN ：	表示对应的文件描述符可以读（包括对端SOCKET正常关闭）
				★EPOLLOUT：	表示对应的文件描述符可以写
				★EPOLLERR：	表示对应的文件描述符发生错误
				★EPOLLET： 	将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)而言的
@Output: None
@Retuval:成功
		 失败-1
@Notes:  控制某个epoll监控的文件描述符上的事件：注册、修改、删除。
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/18 10:24:02
********************************************************************/
int Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	int n;
	if((n=epoll_ctl(epfd,op,fd,event)) <0)
	{
		perr_exit("epoll_ctl error");
	}
	return n;
}

/********************************************************************
@FunName:int Epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
@Input:  epfd:epoll树根节点
		 events：用来存内核得到事件的集合，【数组】，传出参数，传出满足监听条件的fd结构体
		 maxevents：数组 元素总个数 1024。告之内核这个events有多大，这个maxevents的值不能大于创建epoll_create()时的size
		 timeout:超时时间
		 		 -1:阻塞
				  0：立即返回，非阻塞
				 >0：指定毫秒
@Output: events：用来存内核得到事件的集合，【数组】，传出参数，传出满足监听条件的fd结构体
@Retuval:>0: 成功返回有多少文件描述符就绪（满足监听的总个数），可以用作循环上限。
          0: 时间到时返回0
         -1: 出错返回-1
@Notes:  None
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/18 10:44:07
********************************************************************/
int Epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	int n;
	if(((n=epoll_wait(epfd,events,maxevents,timeout)) <0) && (errno != EINTR))
	{
		perr_exit("epoll_wait error");
	}
	return n;
}

/********************************************************************
@FunName:ssize_t Recv(int fd, void *buf, size_t n, int flags)
@Input:  fd:要读的文件描述符
		 buf:数据缓冲区
		 n：缓冲区大小
		 flags：一些标志位。具体看manpage，通常为0
@Output: None
@Retuval:实际读到的字节数
@Notes:  网络通信中的read函数（TCP通信）
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/18 14:58:31
********************************************************************/
ssize_t Recv(int fd, void *buf, size_t n, int flags)
{
	ssize_t i;
	if((i = recv(fd,buf,n,flags)) < 0)
	{
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			//没有数据
		}		
		else{
			perr_exit("recv error");
		}
	}
	return i;
}

/********************************************************************
@FunName:ssize_t send (int __fd, const void *__buf, size_t __n, int __flags);
@Input:  fd:发送的对端fd
		 buf：数据缓冲区
		 n:数据缓冲区大小
		 flags：flags：一些标志位。具体看manpage，通常为0
@Output: None
@Retuval:实际发送的字节数
@Notes:  网络通信数据发送函数
@Author: XiaoDexin
@Email:  xiaodexin0701@163.com
@Time:   2022/04/18 15:06:47
********************************************************************/
ssize_t Send(int fd, const void *buf, size_t n, int flags)
{
	ssize_t i;
	if((i = send(fd,buf,n,flags)) < 0)
	{
		perr_exit("send error");
	}
	return i;
}








