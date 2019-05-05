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


class SendCallback
{
private:
    nio::SimpleSocketServer & m_server;
public:
    SendCallback(nio::SimpleSocketServer & server) : m_server(server) {}
    void operator()(int fd, int status, io::ConstBuffer & buffer);
};

class RecvCallback
{
private:
    nio::SimpleSocketServer & m_server;
public:
    RecvCallback(nio::SimpleSocketServer & server) : m_server(server) {}
    void operator()(int fd, int status, io::MutableBuffer & buffer);
};

class CloseCallback
{
private:
    nio::SimpleSocketServer & m_server;
public:
    CloseCallback(nio::SimpleSocketServer & server) : m_server(server) {}
    void operator()(int fd);
};

int main(int argc, char **argv)
{
    err::Error e;
    nio::SimpleSocketServer server;

    net::Location loc("0.0.0.0", 8899, &e);

    ListenerCallback lcb(server);
    int listenerId = server.addListener(loc, lcb, &e);

    while (1) {
        int r = server.run(&e);
    }

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
}

