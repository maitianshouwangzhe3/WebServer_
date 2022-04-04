/*
 *@服务端主体类方法实现
 *@2022-4-4
 */

#include "webserver.h"


using namespace std;

//类构造函数
WebServer::WebServer( int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize) :
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    LOG_ERROR("========== Server init error!==========");
    //获取当前工作目录
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);

    //给工作目录追加resourecs/ 资源目录
    strncat(srcDir_, "/resources/", 16);

    //设置用户数量为0
    HttpConn::userCount = 0;

    //设置web网站根目录
    HttpConn::srcDir = srcDir_;

    //初始化数据库连接池
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    //设置模式(默认边缘触发)
    InitEventMode_(trigMode);

    //初始化socket
    if(!InitSocket_()){
        isClose_ = true;
    }

    //初始化日志系统
    if(openLog){
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { 
            LOG_ERROR("========== Server init error!=========="); 
            }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

//析构函数
WebServer::~WebServer(){
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

//业务逻辑处理函数
void WebServer::Start(){
    int timeMs = -1;            //epoll wait timeout == -1 没有事件将阻塞
    if(!isClose_){
        LOG_INFO("============= Server Start ============");
    }
    while(!isClose_){
        //超时机制
        if(timeoutMS_ > 0){
            timeMs = timer_->GetNextTick();
        }

        //获取事件
        int eventCnt = epoller_->wait(timeMs);

        //处理事件
        for(int i = 0; i < eventCnt; ++i){
            int fd = epoller_->GetEventFd(i);
            uint32_t event = epoller_->GetEvents(i);

            if(fd == listenFd_){
                DealListen_();
            }
            else if(event & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            else if(event & EPOLLIN){
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if(event & EPOLLOUT){
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            }
            else{
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

//初始化套接字
bool WebServer::InitSocket_(){
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024){
        LOG_ERROR("Port:%d error", port_);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    struct linger optLinger = {0};
    if(openLinger_){
        //所有数据发送完或者超时时优雅关闭
        optLinger.l_linger = 1;
        optLinger.l_onoff = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0){
        close(listenFd_);
        LOG_ERROR("Init socket error!");
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0){
        close(listenFd_);
        LOG_ERROR("Init linger error");
        return false;
    }

    int optval = 1;
    //端口复用
    //只有一个socket能正常接受数据
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR,(const void*)&optval, sizeof(int));
    if( ret < 0){
        close(listenFd_);
        LOG_ERROR("set socket setsockopt error");
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0 ){
        close(listenFd_);
        LOG_ERROR("Bind Port:%d error!", port_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if(ret < 0){
        close(listenFd_);
        LOG_ERROR("Listen port:%d error!", port_);
        return false;
    }

    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if(ret < 0){
        close(listenFd_);
        LOG_ERROR("Add listen error!");
    }

    SetFdNonblock(listenFd_);
    LOG_ERROR("Server port:%d", port_);
    return true;
}

//设置模式
void WebServer::InitEventMode_(int trigMode){
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch(trigMode){
        case 0:
        break;
        case 1:
        connEvent_ |= EPOLLET;
        break;
        case 2:
        listenEvent_ |= EPOLLET;
        break;
        case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
        default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

//添加客户端信息
void WebServer::AddClient_(int fd, sockaddr_in addr){
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0){
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

//连接新进来的客户端
void WebServer::DealListen_(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do{
        int fd = accept(listenFd_, (struct sockaddr*)&addr, &len);
        if(fd < 0){
            return;
        }
        else if(HttpConn::userCount >= MAX_FD){
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    }while(listenEvent_ & EPOLLET);
}

//给线程池中线程拿到写业务代码
void WebServer::DealWrite_(HttpConn* client){
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}
//给线程池中线程拿到读业务代码
void WebServer::DealRead_(HttpConn* client){
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

//发送错误信息
void WebServer::SendError_(int fd, const char*info){
    assert(fd > 0);
    int ret = send(fd, info, sizeof(info), 0);
    if(ret < 0){
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

//调整客户端连接在小根堆里的位置
void WebServer::ExtentTime_(HttpConn* client){
    assert(client);
    if(timeoutMS_ > 0){
        timer_->adjust(client->GetFd(), timeoutMS_);
    }
}

//关闭指定客户端连接
void WebServer::CloseConn_(HttpConn* client){
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

//读方法
void WebServer::OnRead_(HttpConn* client){
    assert(client);
    int ret = 1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if(ret < 0){
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

//写方法
void WebServer::OnWrite_(HttpConn* client){
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0){
        //传输完成
        if(client->IsKeepAlive()){
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0){
        if(writeErrno == EAGAIN){
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

//解析请求报文
void WebServer::OnProcess(HttpConn* client){
    if(client->process()){
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    }
    else{
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

//设置非阻塞
int WebServer::SetFdNonblock(int fd){
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}