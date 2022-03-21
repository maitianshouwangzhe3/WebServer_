/*
 * @http响应报文类
 * @2022-3-21
 * @
 */ 
#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    //初始化响应报文
    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);

    //判断请求的资源
    void MakeResponse(Buffer& buff);

    
    void UnmapFile();
    char* File();
    size_t FileLen() const;
    void ErrorContent(Buffer& buff, std::string message);
    int Code() const { return code_; }

private:
    void AddStateLine_(Buffer &buff);       //添加响应首行
    void AddHeader_(Buffer &buff);          //添加响应头部
    void AddContent_(Buffer &buff);         //添加响应内容

    void ErrorHtml_();
    std::string GetFileType_();

    int code_;
    bool isKeepAlive_;

    std::string path_;              
    std::string srcDir_;                    
    
    char* mmFile_; 
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};


#endif //HTTP_RESPONSE_H