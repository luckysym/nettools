
#include <sym/srpc.h>
#include <sym/utilities.h>
#include <assert.h>
#include <map>
#include <memory.h>

#define LOCAL_URL "0.0.0.0:8899"
using namespace sym;

class ListenerCallback
{
private:
    nio::SimpleSocketServer & m_server;
public:
    ListenerCallback(nio::SimpleSocketServer & server) : m_server(server) {}
    void operator()(int sfd, int cfd, const net::Address * remote);
};

class RecvCallback
{
private:
    nio::SimpleSocketServer & m_server;
    int  m_sendTimeout;

public:
    RecvCallback(nio::SimpleSocketServer & server, int sendtimeout = 5000) 
        : m_server(server) , m_sendTimeout(sendtimeout) {}

    void sendResponse(int fd, io::ConstBuffer & outbuf);
    void operator()(int fd, int status, io::MutableBuffer & buffer);

    void onMessageReceived(srpc::message_t * in, io::ConstBuffer & out);
protected:
    void onLogonRequestReceived(srpc::logon_request_t *in, io::ConstBuffer & out);
    void onServiceRequestReceived(srpc::service_request_t *in, io::ConstBuffer & out);
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
    net::Address loc("0.0.0.0", 8899, &e);

    int listenerId = server.addListener(loc, ListenerCallback(server), &e);

    // server.addTimer(1000, TimerCallback( server ), &e); 
    server.run(&e);

    return 0;
}

void ListenerCallback::operator()(int sfd, int cfd, const net::Address * remote)
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
    SYM_TRACE_VA("[trace] recv buffer created, ptr: %p, cap: %d", buffer.data(), buffer.capacity());
    m_server.beginReceive(cfd, buffer);
}

void RecvCallback::operator()(int fd, int status, io::MutableBuffer & buffer) 
{
    if ( status != 0 ) {
        // 读消息失败，channel关闭
        SYM_TRACE_VA("[error] channel read error, fd: %d", fd);
        if ( buffer.data()) free(buffer.detach());
        m_server.closeChannel(fd);
        // return false;  // 回调后不再接收消息, b不需要返回
        return;
    }

    // 一般首次读消息时，缓存还没分配，先分配缓存
    // 如果调用的beginReceive已经分配了缓存，这里就不用再分配。
    if ( buffer.data() == nullptr ) {
        SYM_TRACE_VA("[error] ON_RECV, buffer alloc, fd: %d", fd);
        char * p = (char *)malloc(1024);
        buffer.attach(p, 0, 1024);   // 缓存挂到buffer
        buffer.limit(sizeof(srpc::message_header_t));  // 先接收报文头
        return; // 回调后自动继续接收 不需要返回
    }

    // 后续缓存已经分配，就检查收到的数据
    srpc::message_t * msg = (srpc::message_t*)buffer.data();
    bool isok = message_check_magic(msg);
    if ( !isok ) {
        // 消息头magic不正确，连接需要关闭
        SYM_TRACE_VA("[error] channel message magic word invalid, fd: %d", fd);
        free(buffer.detach());
        m_server.closeChannel(fd);
        return;
    }

    // 检查收到的消息长度，是否接收完整。
    size_t rsize = buffer.size();    // 当前已经接收到的数据大小
    size_t msize = io::btoh(msg->header.length);
    if ( rsize == msize ) {
        // 消息总长等于收到的长度，当前消息接收完成
        SYM_TRACE_VA("[info] message received, fd: %d, timestamp: %lld， len: %d", 
            fd, io::btoh(msg->header.timestamp), (int)msize);
        
        io::ConstBuffer out;
        this->onMessageReceived(msg, out);

        // 将消息发回
        this->sendResponse(fd, out);
        
        buffer.reset();  // 缓存倒回, limit = cap, size = 0;
        buffer.limit(sizeof(srpc::message_header_t));
        // return true;  // 接续接收
        return ;
    }

    // 消息长度和当前收到的不一致，通常是收到消息头，需要扩充内存，继续接收
    assert( msize > rsize);   // 这里必然 msize > rsize, 收到的比消息总长还长，就有问题了
    if ( rsize == sizeof(srpc::message_header_t) ) {

        // 收到了包头，但还有包体要收，如果缓存不够，就扩充
        // 包总长等于包头长度，则是个空包，按以完成处理。
        if ( msize > buffer.capacity()) {
            char * p = (char *)realloc(buffer.data(), msize);
            buffer.attach(p, rsize, msize);
        }

        buffer.limit(msize);
        SYM_TRACE_VA("[trace] ON_RECV, buffer realloc, %d", (int)msize);
        return ;
    } else {
        // 收到了一半消息体或消息头，一般不会有这种问题。
        assert("bad message length" == nullptr);
    }
}

void RecvCallback::onMessageReceived(srpc::message_t * in, io::ConstBuffer & out)
{
    int16_t logon_req = io::htob((int16_t)srpc::typeLogonRequest);
    int16_t service_req = io::htob((int16_t)srpc::typeServiceRequest); 

    if ( in->header.body_type == logon_req ) {
        this->onLogonRequestReceived((srpc::logon_request_t*)in, out);
    }
    else if (in->header.body_type == service_req) {
        this->onServiceRequestReceived((srpc::service_request_t*)in, out);
    } 
    else {
        abort();
    }
}

void RecvCallback::onServiceRequestReceived(srpc::service_request_t * in, io::ConstBuffer &out)
{
    const char * replydata = "SDS0{{0x8, \\{\"result\": \"1234567\"\\}}}";
    srpc::service_response_t * resp = (srpc::service_response_t*)malloc(1024);
    
    int64_t sid = io::btoh(in->service.session_id);
    srpc::datablock_t * session = in->data;
    int32_t sessionlen = io::btoh(session->length);
    std::string strsession((char *)session->value, sessionlen);

    const char * p = (const char *)session;
    p += sizeof(int32_t) + sessionlen;

    srpc::datablock_t * stream = (srpc::datablock_t*)(p);
    int32_t streamlen  = io::btoh(stream->length);
    std::string strstream((char *)stream->value, streamlen);

    SYM_TRACE_VA("ON_SERVICE_REQUEST, sid: %lld, sessionlen: %d, session: %s, streamlen: %d, stream: %s",
        sid, sessionlen, strsession.c_str(), streamlen, strstream.c_str());
    
    resp->header = in->header;
    resp->service = in->service;

    resp->header.body_type = io::htob((int16_t)srpc::typeServiceResponse);
    
    srpc::datablock_t * osess = resp->data;
    osess->length = session->length;
    memcpy(osess->value, session->value, sessionlen);

    srpc::datablock_t * ostream = (srpc::datablock_t * )((char *) (&osess->value) + sessionlen);
    int32_t oslen = strlen(replydata);
    ostream->length = io::htob(oslen);
    memcpy(ostream->value, replydata, oslen);

    int32_t svcbodylen = 2 * sizeof(int32_t) + sessionlen + oslen;
    int32_t totallen = sizeof(srpc::message_header_t) + sizeof(srpc::service_request_t)
                     + sessionlen + oslen + 2 * sizeof(int32_t);
    resp->header.length = io::htob(totallen);
    resp->service.rpc_body_len = io::htob(svcbodylen);

    out.attach((const char *)resp, totallen, 1024);
    out.limit(totallen);

    sleep(1);  // 停止几秒模拟运行，以便前端超时测试
    return ;
}

void RecvCallback::onLogonRequestReceived(srpc::logon_request_t *in, io::ConstBuffer & out)
{
        int size = io::btoh(in->client_length) + io::btoh(in->server_length);
        std::string str(in->body, size);
        SYM_TRACE_VA("[trace] ON_LOGON_REQUEST_RECV, %s", str.c_str());

        srpc::logon_reply_t * p  = (srpc::logon_reply_t*)malloc(sizeof(srpc::logon_reply_t));
        
        p->header = in->header;
        p->header.body_type = io::htob((int16_t)srpc::typeLogonResponse);
        p->header.timestamp = io::htob((int64_t)chrono::now());
        p->header.length = io::htob((int32_t)sizeof(srpc::logon_reply_t));

        p->regcode = io::htob(1);
        p->result  = 0;
        p->suspend = 0;
        p->window  = 1;
        
        out.attach((char *)p, sizeof(srpc::logon_reply_t), sizeof(srpc::logon_reply_t));
        out.limit(sizeof(srpc::logon_reply_t));
}

void RecvCallback::sendResponse(int fd, io::ConstBuffer & outbuf) 
{
    SYM_TRACE_VA("SRPC_SEND_RESPONSE, buf: %p, pos: %d, limit: %d", 
        outbuf.data(), outbuf.position(), outbuf.limit());
    
    bool isok = m_server.send(fd, outbuf);
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

    // m_server.closeChannel(fd);
}

void CloseCallback::operator()(int fd)
{
    SYM_TRACE_VA("[info] channel closed, fd: %d", fd);
    // 该回调说明channel已经关闭，fd相关资源不再可用
}