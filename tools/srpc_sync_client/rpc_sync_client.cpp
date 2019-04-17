#include <sym/srpc.h>
#include <assert.h>

#define REMOTE_URL "127.0.0.1:40001"

int main( int argc, char **argv)
{
    using namespace srpc;

    SRPC_SyncConnection connect;

    const char * remote = REMOTE_URL;
    if ( argc > 1 ) remote = argv[1];

    bool isok = connect.open(remote);
    assert(isok);

    isok = connect.close();
    assert(isok);
    return 0;
}

