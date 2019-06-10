#include <sym/nio.h>
#include <sym/network.h>
#include <sym/io.h>
#include <sym/redis.h>

volatile int G_stop = 0;

int G_seq = 0;

nio::SocketChannel * G_channel = nullptr;

int on_send(const char * cmd, int len, int timeout, err::Error *e)
{
    nio::SocketChannel * channel = G_channel;

    io::ConstBuffer buffer(cmd, len, len);
    int r = channel->sendN(buffer, timeout, e);
    SYM_TRACE_VA("on send: %d, %s", r, cmd);
    return r;
}

int on_recv(char * cmd, int len, int timeout, err::Error *e)
{
    io::MutableBuffer buffer(cmd, 0, len);
    nio::SocketChannel * channel = G_channel;
    int r = channel->receiveSome(buffer, timeout);
    SYM_TRACE_VA("on recv: %d", r);
    return r;
}

void print_redis_value(redis::Value * value)
{
    if ( value->type() == redis::Value::vtString ) {
        printf("%s\n", value->getString().c_str());
    } 
    else if ( value->type() == redis::Value::vtInteger ) {
        printf("%lld\n", value->getInt());
    } 
    else if ( value->type() == redis::Value::vtList ) {
        auto it = value->list().begin();
        printf("list count: %d\n", (int)value->list().size());
        for( ; it != value->list().end(); ++it) print_redis_value(&(*it));
    }
    else if ( value->type() == redis::Value::vtStatus ) {
        printf("%s\n", value->getString().c_str());
    }
    else if ( value->type() == redis::Value::vtNull ) {
        printf("(NULL)\n");
    }
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
    G_channel = &channel;

    SYM_TRACE("channel open ok");

    redis::Value result;
    redis::Command command(on_send, on_recv);
    command.setTimeout(1000);
    command.execute(&result, "set a abcd", &e);
    
    printf("type of result: %d\n", result.type());
    print_redis_value(&result);
    
    command.assign("get").append("a");
    command.execute(&result, &e);
    printf("type of result: %d\n", result.type());
    print_redis_value(&result);

    sleep(1);

    channel.close();

    return 0;
}
