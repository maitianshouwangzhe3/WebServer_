/*
 *@数据库连接池类方法实现
 *@2022-3-13
 */
#include "sqlconnpool.h"

SqlConnPool::SqlConnPool(){
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool::~SqlConnPool(){
    ClosePool();
}

//单例设计模式，创建一个静态的数据库连接池
SqlConnPool* SqlConnPool::Instance(){
    static SqlConnPool connPool;
    return &connPool;
}

//将使用完的数据库连接放进数据库连接池
void SqlConnPool::FreeConn(MYSQL* conn){
    //assert(sql);
    std::lock_guard<std::mutex> locker(mtx_);         //共享资源操作前加锁
    connQue_.push(conn);                              //放进连接池
    sem_post(&semId_);                                //信号量加一
}

//获取一个空闲的数据库连接
MYSQL* SqlConnPool::GetConn(){
    //assert(connQue_)
    MYSQL*  tmp = nullptr;
    if(connQue_.empty()){
        //这里写日志
        return nullptr;
    }

    sem_wait(&semId_);           //连接池中有空闲则向下运行，没有则阻塞
    {
        std::lock_guard<std::mutex> locker(mtx_);
        tmp = connQue_.front();
        connQue_.pop();
    }
    return tmp;
}

//返回数据库连接池中可以使用的连接数量
int SqlConnPool::GetFreeConnCount(){
    std::lock_guard<std::mutex> locker(mtx_);           //加锁
    return connQue_.size();
}

//释放数据库连接池中的资源
void SqlConnPool::ClosePool(){
    std::lock_guard<std::mutex> locker(mtx_);
    while(!connQue_.empty()){
        auto tmp = connQue_.front();
        connQue_.pop();
        mysql_close(tmp);
    }
    mysql_library_end();
}

//初始化数据库连接池
void SqlConnPool::Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize){
                  //assert(connSize > 0);
                  for(int i = 0; i < connSize; ++i){
                      MYSQL* sql = nullptr;
                      mysql_init(sql);
                      if(!sql){//安全检查
                          //这里写日志 mysql init error
                          //assert(sql);
                      }

                      sql = mysql_real_connect(sql, host, user, pwd, dbName,  port, nullptr, 0);

                      if(!sql){//安全检查
                        //这里写日志 mysql connect error
                        //assert(sql);
                      }

                      connQue_.push(sql);
                  }

                  MAX_CONN_ = connSize;
                  sem_init(&semId_, 0, connSize);
              }

