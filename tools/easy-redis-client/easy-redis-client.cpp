#include <sym/nio.h>
#include <sym/network.h>
#include <sym/io.h>
#include <sym/redis.h>

volatile int G_stop = 0;

int G_seq = 0;

int on_send(const char * cmd, int len, int timeout, err::Error *e)
{
    SYM_TRACE_VA("on send: %d", len);
    return len;    
}

int on_recv(char * cmd, int len, int timeout, err::Error *e)
{
    SYM_TRACE_VA("on recv: %d", len);
    strcpy(cmd, "+OK\r\n");
    return strlen(cmd);    
}


int main(int argc, char **argv)
{
    err::Error e;
    net::Address remote("127.0.0.1", 6379, &e);
    if ( e ) {
        SYM_TRACE_VA("init remote addr error, %s", e.message());
        return -1;
    }
    
    nio::SocketChannel channel;
    bool isok = channel.open(remote, 5000, &e);
    if ( !isok ) {
        SYM_TRACE_VA("channel open error, %s", e.message());
        return -1;
    }

    SYM_TRACE("channel open ok");

    redis::Value result;
    redis::Command command(on_send, on_recv);
    command.setText("set a 100");
    command.execute(&result, 1000, &e);
    
    SYM_TRACE_VA("command result: %s", result.getString().c_str() );
    sleep(30);

    channel.close();

    return 0;
}
