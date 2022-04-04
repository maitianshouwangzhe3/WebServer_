/*
 *@http对象方法实现
 *@2022-3-19
*/
#include "httpconn.h"


const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;   //连接上来的用户数量
bool HttpConn::isET;
//构造函数
HttpConn::HttpConn(){
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

//析构函数
HttpConn::~HttpConn(){
    Close();
}


//初始化连接对象
void HttpConn::init(int sockFd, const sockaddr_in& addr){
    assert(sockFd > 0);
    userCount++;
    addr_ = addr;
    fd_ = sockFd;
    isClose_ = false;
    readBuff_.RetrieveAll();            //读缓冲区清零
    writeBuff_.RetrieveAll();           //写缓冲区清零
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);

}

//读数据
ssize_t HttpConn::read(int* saveErrno){
    ssize_t len = -1;
    do{
        len = readBuff_.ReadFd(fd_, saveErrno);
        if(len <= 0){
            break;
        }
    }while(isET);   //边缘触发一次读完，do..while循环保证至少能读一次
    return len;
}

//写数据
ssize_t HttpConn::write(int* saveErrno){
    ssize_t len = -1;
    do{
        len = writev(fd_, iov_, iovCnt_);
        if(len < 0){
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len == 0){         //传输结束
            break;
        }
        else if(static_cast<size_t>(len) > iov_[0].iov_len){
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len){
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else{
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);
        }
    }while(isET || ToWriteBytes() > 10240);
    return len;
}

//关闭连接
void HttpConn::Close(){
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

//返回客户端文件描述符（socket套接字）
int HttpConn::GetFd() const{
    return fd_;
}

//返回客户端端口
int HttpConn::GetPort() const{
    return addr_.sin_port;
}

//返回客户端地址信息
sockaddr_in HttpConn::GetAddr() const{
    return addr_;
}

//转换成本机字节序返回客户端IP
const char* HttpConn::GetIP() const{
    return inet_ntoa(addr_.sin_addr);
}

//解析请求报文并生成响应报文
bool HttpConn::process(){
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0){             //读缓冲区没有数据直接返回false
        return false;
    }
    else if(request_.parse(readBuff_)){             //解析请求报文入口
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    }
    else{
        response_.Init(srcDir, request_.path(), false, 400);
    }

    response_.MakeResponse(writeBuff_);
    //响应头
    iov_[0].iov_base = response_.File();
    iov_[0].iov_len = writeBuff_.WritableBytes();
    iovCnt_ = 1;

    //文件
    if(response_.FileLen() > 0 && response_.File()){
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    return true;
}