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
    static int i = 0;
    SYM_TRACE_VA("on recv: %d", len);
    // strcpy(cmd, "+OK\r\n");
    // strcpy(cmd, "-Error message\r\n");
    /*
    if ( i == 0 ) {
        strcpy(cmd, "-Error ");
        i = 1;
    } else {
        strcpy(cmd, "Message\r\n");
    }
    */
    
    // strcpy(cmd, ":1234567890\r\n");
    // strcpy(cmd, "$10\r\n123456\r\n78\r\n");
    // strcpy(cmd, "*-1\r\n");
    // strcpy(cmd, "*1\r\n+OK\r\n");
    // strcpy(cmd, "*2\r\n+OK\r\n:987654321\r\n");
    strcpy(cmd, "*5\r\n"
                  "+OK\r\n"
                  ":987654321\r\n"
                  "$5\r\nhello\r\n"
                  "*2\r\n"
                    "$6\r\nworld!\r\n"
                    ":55667788\r\n"
                  "$-1\r\n");

    return strlen(cmd);
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

    SYM_TRACE("channel open ok");

    redis::Value result;
    redis::Command command(on_send, on_recv);
    command.setTimeout(1000);
    command.setText("set a 100");
    command.execute(&result, &e);
    
    printf("type of result: %d\n", result.type());
    print_redis_value(&result);
    sleep(1);

    channel.close();

    return 0;
}
