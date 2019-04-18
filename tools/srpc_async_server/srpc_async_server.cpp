#include <sym/srpc.h>
#include <assert.h>

#define LOCAL_URL "0.0.0.0:40001"

int main(int argc, char **argv)
{
    using namespace srpc;

    const char * local = LOCAL_URL;
    if ( argc > 1 ) local = argv[1];

    SRPC_AsyncServer server;
    bool isok = server.add_listener(local);
    assert(isok);
    
    server.run();

    return 0;
}