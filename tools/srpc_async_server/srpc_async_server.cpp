#include <sym/srpc.h>
#include <assert.h>

#define LOCAL_URL "0.0.0.0:40001"

int main(int argc, char **argv)
{
    using namespace srpc;

    const char * local_url = LOCAL_URL;
    if ( argc > 1 ) local_url = argv[1];

    err::error_t err;
    err::init_error_info(&err);

    srpc::async_server_t server;
    bool isok = srpc::async_server_init(&server, &err);
    assert(isok);

    nio::listener_t * lis = srpc::async_server_add_listener(&server, local_url, &err);
    assert(lis);

    isok = srpc::async_server_run(&server, &err);
    assert(isok);

    isok = srpc::async_server_close(&server, &err);
    assert(isok);
    return 0;
}
