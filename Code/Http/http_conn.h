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

//任务类
class http_conn{
public:
    http_conn();
    ~http_conn();


private:



};





#endif
