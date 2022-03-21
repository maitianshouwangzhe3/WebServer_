/*
 * @http对象
 * @2022-3-19
 * @
 */ 

#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      

#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);     //初始化

    ssize_t read(int* saveErrno);                   //读数据

    ssize_t write(int* saveErrno);                  //写数据

    void Close();                                   //关闭

    int GetFd() const;                              //获取客户端通信的文件描述符

    int GetPort() const;                            //获取端口

    const char* GetIP() const;                      //返回客户端IP
    
    sockaddr_in GetAddr() const;                    //返回客户端地址
    
    bool process();                                 //工作线程

    int ToWriteBytes() {                            //可以写的字节
        return iov_[0].iov_len + iov_[1].iov_len; 
    }

    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    static bool isET;                           //是否是边缘触发
    static const char* srcDir;                  //访问目录
    static std::atomic<int> userCount;          //用户数量
    
private:
   
    int fd_;                            //客户端文件描述符
    struct  sockaddr_in addr_;          //保存客户端信息

    bool isClose_;              //是否关闭
    
    int iovCnt_;
    struct iovec iov_[2];           
    
    Buffer readBuff_; // 读缓冲区
    Buffer writeBuff_; // 写缓冲区

    HttpRequest request_;           //请求对象
    HttpResponse response_;         //响应对象
};


#endif //HTTP_CONN_H