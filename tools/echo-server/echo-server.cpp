
#include <sym/network.h>
#include <sym/nio.h>
#include <sym/io.h>
#include <stdio.h>
#include <assert.h>

void listner_event_proc(int fd, int event, void *arg);
void channel_event_proc(int fd, int event, void *arg);

struct echo_channel 
{
    int               fd;
    net::location_t   remote;
    io::buffer_t      rdbuf;
    nio::selector_t * selector;
};

/**
 * command:  echosvr <remote_url> <message> 
 * example:  echosvr tcp://*:8989
 */
int main(int argc, char **argv)
{
    if ( argc < 2 ) {
        fprintf(stderr, "help: %s <local_url>\n", argv[0]);
        return 0;
    }

    char * remote = argv[1];
    char * message = argv[2];

    // remote url to net::location
    net::location loc;
    net::location_init(&loc);
    net::location * p = net::location_from_url(&loc, remote);
    if ( !p ) {
        fprintf(stderr, "[error] bad url for pasing, %s\n", remote);
        return -1;
    }
    struct err::error_info err;
    err::init_error_info(&err);
    
    // open socket listener
    int opts = net::sockopt_reuseaddr | net::sockopt_nonblocked;
    int sfd = net::socket_open_listener(&loc, opts, &err);
    if ( sfd == -1 ) {
        fprintf(stderr, "[error] failed to open socket listener, %s\n", err.str);
        err::free_error_info(&err);
        return -1;
    }
    sleep(1);

    // create selector 
    nio::selector_t selector ;
    auto sel = nio::selector_init(&selector, 0, &err);
    if ( !sel ) {
        fprintf(stderr, "[error] failed to create selector, %s", err.str);
        err::free_error_info(&err);
        net::socket_close(sfd, nullptr);
        return -1;
    }

    // add listener to selector
    bool isok = nio::selector_add(sel, sfd, listner_event_proc, sel, &err);
    if ( !isok ) {
        fprintf(stderr, "[error] failed to create selector, %s", err.str);
        err::free_error_info(&err);
        nio::selector_destroy(sel, nullptr);
        net::socket_close(sfd, nullptr);
        return -1;
    }
    
    while ( 1) {
        int r = nio::selector_run(sel, &err);    
        if ( r < 0 ) {
            fprintf(stderr, "[error] failed to run selector, %s", err.str);
            err::free_error_info(&err);
            break;
        }
    }

    nio::selector_destroy(sel, nullptr);
    net::socket_close(sfd, nullptr);

    return 0;
}

// 监听socket事件回调
void listner_event_proc(int fd, int event, void *arg)
{
    nio::selector_t *sel = (nio::selector_t*)arg;
    assert(sel);

    if ( event == nio::select_read ) {
        net::location_t remote;
        err::error_t    err;

        fprintf(stderr, "[info] listener acceptable, fd:%d\n", fd);

        net::location_init(&remote);
        err::init_error_info(&err);
        while (1) {
            int cfd = net::socket_accept(fd, net::sockopt_nonblocked | net::sockopt_tcp_nodelay, &remote, &err);
            if ( cfd == -1 && err.str) {
                fprintf(stderr, "[error] failed to accept channel, %s\n", err.str);
                err::free_error_info(&err);
                nio::selector_remove(sel, fd, &err);
            } else if ( cfd == -1 && err.str == nullptr ) {
                // no more channel
                net::location_free(&remote);
                break;
            } else {
                fprintf(stderr, "[info] new client accept, fd:%d, %s:%d\n", cfd, remote.host, remote.port);
                
                echo_channel *ch = new echo_channel;
                memset(ch, 0, sizeof(echo_channel));
                ch->fd = cfd;
                ch->selector = sel;
                net::location_copy(&ch->remote, &remote);
                io::buffer_alloc(&ch->rdbuf, 256);

                bool isok = nio::selector_add(sel, cfd, channel_event_proc, ch, &err);
                assert(isok);
                net::location_free(&remote);
            }
        } // end while  

        // 继续监听accept事件
        bool rok = nio::selector_request(sel, fd, nio::select_read, -1, nullptr);
        assert(rok);
    } else if ( event == nio::select_remove) {
        fprintf(stderr, "[info] listener will be closed, fd:%d\n", fd);
        net::socket_close(fd, nullptr);
    } else if ( event == nio::select_add ) {
        fprintf(stderr, "[trace] listener is added to selector, request for accepting, fd:%d\n", fd);
        // request accept event for no timeout
        err::error_t err;
        err::init_error_info(&err);
        int revents = nio::select_read;
        bool isok = nio::selector_request(sel, fd, revents, -1, &err);
        assert( isok );
    }
    return ;
}

void channel_event_proc(int fd, int event, void *arg)
{
    err::error_t err;
    err::init_error_info(&err);

    echo_channel * ch = (echo_channel *)arg;
    assert(ch);

    nio::selector_t *sel = ch->selector;
    assert(sel);

    bool is_error = false;

    if ( event == nio::select_read ) {
        fprintf(stderr, "[trace] channel_event_proc, read, fd: %d, buf: %d - %d\n", fd, ch->rdbuf.begin, ch->rdbuf.end);
        // 循环读取消息，直到无消息可读或者缓存满

        // 把有数据的缓存移到头部
        io::buffer_pullup(&ch->rdbuf);      
        bool buffer_empty = ch->rdbuf.begin == ch->rdbuf.end;  // 当前缓存是否为空

        while (1) {
            int    remain = 0;
            // 计算剩余空i教案
            remain = ch->rdbuf.size - ch->rdbuf.end;
            
            // 缓存有可用空间，则读取数据
            if ( remain > 0 ) {
                char * ptr = ch->rdbuf.data + ch->rdbuf.end;
                // receive data
                int r = net::socket_recv(ch->fd, ptr, remain, &err);
                if ( r > 0 ) {
                    fprintf(stderr, "[trace] channel_event_proc, data read, fd: %d, size: %d\n", fd, r);
                    
                    // data received
                    ch->rdbuf.end += r;
                } else if ( r == 0 ) {
                    // nothing received
                    break;
                } else {
                    // error, or remote closed
                    fprintf(stderr, "[info] channel recv failed, fd:%d\n", fd);
                    bool sok = nio::selector_remove(sel, fd, &err);
                    assert(sok);
                    is_error = true;
                    break;  // 退出
                }
            } else {  // remain == 0
                // no space for read
                fprintf(stderr, "[trace] channel no data recv, fd:%d, %d\n", fd, remain);
                break;
            }
        } // end while 

        // 打印收到的数据
        fprintf(stderr, "[trace] print message, begin: %d, end: %d \n", ch->rdbuf.begin, ch->rdbuf.end);
        for( int i = ch->rdbuf.begin; i < ch->rdbuf.end; ++i) {
            fputc(ch->rdbuf.data[i], stderr);
        }
        fputc('\n', stderr);
        
        // 如果缓存有数据, 且收消息前缓存空，则通知写
        if ( buffer_empty && !is_error ) {
            bool isok = nio::selector_request(sel, fd, nio::select_write, -1, &err);
            assert(isok);
        }

        // 如果缓存满了，或者出错了，就不请求收了，如果缓存没满，且继续请求读事件
        if ( ch->rdbuf.end < ch->rdbuf.size && !is_error ) {
            bool isok = nio::selector_request(sel, fd, nio::select_read, -1, &err);
            assert(isok);
        }

    } else if ( event == nio::select_write ) {
        while ( 1 ) {
            char * ptr = ch->rdbuf.data + ch->rdbuf.begin;
            int    remain = ch->rdbuf.end - ch->rdbuf.begin;
            
            if ( remain > 0 ) {

                int s = net::socket_send(ch->fd, ptr, remain, &err);
                if ( s > 0 ) {
                    // sent some ok
                    ch->rdbuf.begin += s;
                } else if ( s == 0 ) {
                    // no data sent
                    break;
                } else {
                    // send error
                    fprintf(stderr, "[info] channel send failed, fd:%d\n", fd);
                    bool sok = nio::selector_remove(sel, fd, &err);
                    assert(sok);
                    is_error = true;
                }
            } else {   // remain == 0 , no data to send
                break;
            }
        } // end while

        // 当发送缓存空了，就取申请读新消息,
        // 如果发送缓存没空，就继续申请发送
        if ( ch->rdbuf.begin == ch->rdbuf.end && !is_error ) {
            bool isok = nio::selector_request(sel, fd, nio::select_read, -1, &err);
            assert(isok);
        } else if ( ch->rdbuf.end > ch->rdbuf.begin && !is_error) {
            bool isok = nio::selector_request(sel, fd, nio::select_write, -1, &err);
            assert(isok);
        }

    } else if ( event == nio::select_remove) {

        fprintf(stderr, "[info] channel will be closed, fd:%d\n", fd);

        net::socket_close(ch->fd, nullptr);
        io::buffer_free(&ch->rdbuf);
        net::location_free(&ch->remote);

        ch->fd = -1;
        delete ch;
    } else if ( event == nio::select_add ) {
        
        // channel added, wait for reading
        bool sok = nio::selector_request(sel, fd, nio::select_read, -1, &err);
        assert(sok);
        
    } else if ( event == nio::select_error ) {
        fprintf(stderr, "[info] channel wait error, fd:%d\n", fd);
        bool sok = nio::selector_remove(sel, fd, &err);
        assert(sok);
    } else if ( event & nio::select_timeout ) {
        fprintf(stderr, "[info] channel wait timeout, fd:%d\n", fd);
        assert("[info] channel wait timeout" == nullptr);
    } else {
        assert("bad select event" == nullptr);
    }
}