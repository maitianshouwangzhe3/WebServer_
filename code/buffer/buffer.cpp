/*
 *buffer类方法实现
 *2022-3-16
*/

#include "buffer.h"

//构造函数
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0){

}

//可以写的字节
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

//可以读的字节
size_t Buffer::ReadableBytes() const {
    return writePos_ -  readPos_;
}

//读了多少字节
size_t Buffer::PrependableBytes()  const {
    return readPos_;
}

//读到的位置
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

//检查缓冲区中可以写的字节
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len){
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

//调整writePos_位置
void Buffer::HasWritten(size_t len){
    writePos_ += len;
}

//调整readPos_位置
void Buffer::Retrieve(size_t len){
    assert(len <= ReadableBytes());
    readPos_ += len;
}

void Buffer::RetrieveUntil(const char* end){
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

//清空缓冲区
void Buffer::RetrieveAll(){
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr(){
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

//写到的位置
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite(){
    return BeginPtr_() + writePos_;
}


void Buffer::Append(const std::string& str){
    Append(str.data(), str.size());
}

void Buffer::Append(const void* data, size_t len){
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len){
    assert(str);
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const Buffer& buff){
    Append(buff.Peek(), ReadableBytes());
}

//调整内容分布
void Buffer::MakeSpace_(size_t len){
    if(WritableBytes() + PrependableBytes() < len){
        buffer_.resize(writePos_ + len + 1);
    }
    else{
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readable + readPos_;
        assert(readable == ReadableBytes()); 
    }
}

ssize_t Buffer::ReadFd(int fd, int* Errno){
    char buff[65535];
    struct iovec iov[2];
    const size_t writeable = WritableBytes();

    //分散读，确保读完
    iov[0].iov_base =  BeginPtr_() + writePos_;
    iov[0].iov_len = writeable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0){
        *Errno = errno;
    }
    else if(static_cast<size_t>(len) <= writeable){
        writePos_ += len;
    }
    else{
        writePos_ = buffer_.size();
        Append(buff, len - writeable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* Errno){
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len <= 0){
        *Errno = errno;
        return len;
    }
    readPos_ += len;
    return len;
}

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}