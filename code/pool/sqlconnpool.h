/*
 * @数据库连接池
 * @2022-3-13
 * 
 */ 
#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool *Instance();             //创建一个对象

    MYSQL *GetConn();                           //从数据库连接池中获取一个连接
    void FreeConn(MYSQL * conn);                //放回连接进数据库连接池
    int GetFreeConnCount();                     //获取数据库连接池中可以使用的连接的数量

    

    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);           //初始化
    void ClosePool();                //关闭数据库连接池

private:
    SqlConnPool();                   //构造私有化，单例设计模式
    ~SqlConnPool();                  //

    int MAX_CONN_;                   //最大的连接数
    int useCount_;                   //在使用的
    int freeCount_;                  //可以使用的数量

    std::queue<MYSQL *> connQue_;    //数据库连接池
    std::mutex mtx_;                 //互斥锁
    sem_t semId_;                    //信号量
};


#endif // SQLCONNPOOL_H