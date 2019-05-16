#include <sym/srpc.h>
#include <assert.h>
#include <map>

#define LOCAL_URL "0.0.0.0:40001"

class ListenerCallback
{
private:
    nio::SimpleSocketServer & m_server;
public:
    ListenerCallback(nio::SimpleSocketServer & server) : m_server(server) {}
    void operator()(int sfd, int cfd, const net::Location * remote);
};

class RecvCallback
{
private:
    nio::SimpleSocketServer & m_server;
    int  m_sendTimeout;

public:
    RecvCallback(nio::SimpleSocketServer & server, int sendtimeout = 5000) 
        : m_server(server) , m_sendTimeout(sendtimeout) {}

    void sendResponse(int fd, const char * data, size_t len);
    void operator()(int fd, int status, io::MutableBuffer & buffer);
};

class SendCallback
{
private:
    nio::SimpleSocketServer & m_server;
public:
    SendCallback(nio::SimpleSocketServer & server) : m_server(server) {}
    void operator()(int fd, int status, io::ConstBuffer & buffer);
};

class CloseCallback
{
private:
    nio::SimpleSocketServer & m_server;
public:
    CloseCallback(nio::SimpleSocketServer & server) : m_server(server) {}
    void operator()(int fd);
};

class TimerCallback
{
private:
    nio::SimpleSocketServer & m_server;
public:
    TimerCallback(nio::SimpleSocketServer & server) : m_server(server) {}
    
    bool operator()(int fd) {
        SYM_TRACE_VA("[trace] TimerCallback, timer %d", fd);
    }
};

class ServerCallback 
{
public:
    void operator()(int status) {
        SYM_TRACE_VA("[trace] ServerCallback, status %d", status);
    }
};

int main(int argc, char **argv)
{
    err::Error e;
    nio::SimpleSocketServer server;

    server.setServerCallback(ServerCallback());
    server.setIdleInterval(10);    // 10s空闲回调。
    net::Location loc("0.0.0.0", 8899, &e);

    int listenerId = server.addListener(loc, ListenerCallback(server), &e);

    // server.addTimer(1000, TimerCallback( server ), &e); 
    server.run(&e);

    return 0;
}

void ListenerCallback::operator()(int sfd, int cfd, const net::Location * remote)
{
    if ( cfd == -1 )  {
        SYM_TRACE_VA("[error] listener error, fd: %d", sfd);
        m_server.exitLoop();   // 获取连接失败，退出server循环
        return;
    }

    SYM_TRACE_VA("[info] accept new channel, fd: %d", cfd);
    err::Error error;
    m_server.acceptChannel(cfd, RecvCallback(m_server), SendCallback(m_server), CloseCallback(m_server), &error);
    
    // 开始接收消息
    io::MutableBuffer buffer(new char[1024], 0, 1024);
    buffer.limit(sizeof(srpc::srpc_message_header));
    m_server.beginReceive(cfd, buffer);
}

void RecvCallback::operator()(int fd, int status, io::MutableBuffer & buffer) 
{
    if ( status != 0 ) {
        // 读消息失败，channel关闭
        SYM_TRACE_VA("[error] channel read error, fd: %d", fd);
        if ( buffer.data()) free(buffer.detach());
        m_server.shutdownChannel(fd, nio::SimpleSocketServer::shutdownRead);
        // return false;  // 回调后不再接收消息, b不需要返回
    }

    // 一般首次读消息时，缓存还没分配，先分配缓存
    if ( buffer.data() == nullptr ) {
        char * p = (char *)malloc(1024);
        buffer.attach(p, 1024);   // 缓存挂到buffer
        buffer.resize(0);         // 有效数据为0
        buffer.limit(sizeof(srpc::message_header_t));  // 先接收报文头
        // return true;   // 回调后自动继续接收 不需要返回
    }

    // 后续缓存已经分配，就检查收到的数据
    srpc::message_t * msg = (srpc::message_t*)buffer.data();
    bool isok = message_check_magic(msg);
    if ( !isok ) {
        // 消息头magic不正确，连接需要关闭
        SYM_TRACE_VA("[error] channel message error, fd: %d", fd);
        free(buffer.detach());
        m_server.shutdownChannel(fd, nio::SimpleSocketServer::shutdownRead);
        // return false;
    }

    // 检查收到的消息长度，是否接收完整。
    size_t rsize = buffer.size();
    size_t msize = io::btoh(msg->header.length);
    if ( rsize == msize ) {
        // 消息总长等于收到的长度，当前消息接收完成
        SYM_TRACE_VA("[info] message received, fd: %d, timestamp: %lld， len: %d", 
            fd, io::btoh(msg->header.timestamp), (int)msize);
        
        // 将消息发回
        this->sendResponse(fd, buffer.data(), buffer.size());
        
        buffer.reset();  // 缓存倒回, limit = cap, size = 0;
        buffer.limit(sizeof(srpc::message_header_t));
        // return true;  // 接续接收
    }

    // 消息长度和当前收到的不一致，通常是收到消息头，需要扩充内存，继续接收
    assert( msize > rsize);   // 这里必然 msize > rsize, 收到的比消息总长还长，就有问题了
    if ( rsize == sizeof(srpc::message_header_t) ) {

        // 收到了包头，但还有包体要收，如果缓存不够，就扩充
        // 包总长等于包头长度，则是个空包，按以完成处理。
        
        if ( msize > buffer.capacity()) {
            char * p = (char *)realloc(buffer.data(), msize);
            buffer.attach(p, msize);
            buffer.resize(rsize);
        }

        buffer.limit(msize);
        // return true; // 继续收包体
    } else {
        // 收到了一半消息体或消息头，一般不会有这种问题。
        assert("bad message length" == nullptr);
    }
}

void RecvCallback::sendResponse(int fd, const char * data, size_t len) 
{
    char * outdata = (char *)malloc(len);
    memcpy(outdata, data, len);

    err::Error e;
    io::ConstBuffer buffer(outdata, len);
    bool isok = m_server.send(fd, buffer, &e);
    assert( isok );
    return ;
}

void SendCallback::operator()(int fd, int status, io::ConstBuffer & buffer)
{
    // 发送回复数据完成，释放缓存，关闭连接。
    const srpc::message_t * msg = (const srpc::message_t*)buffer.data();
    if ( status == 0 ) {
        SYM_TRACE_VA("[info] message response sent, fd: %d, timestamp: %lld", 
            fd, io::btoh(msg->header.timestamp));
    } else {
        SYM_TRACE_VA("[error] message response send failed, fd: %d, timestamp: %lld", 
            fd, io::btoh(msg->header.timestamp));
    }

    free((void *)buffer.detach());

    m_server.shutdownChannel(fd, nio::SimpleSocketServer::shutdownBoth);
}

void CloseCallback::operator()(int fd)
{
    SYM_TRACE_VA("[info] channel closed, fd: %d", fd);
    // 该回调说明channel已经关闭，fd相关资源不再可用
}