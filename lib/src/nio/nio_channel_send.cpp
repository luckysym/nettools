#include <sym/nio.h>

namespace nio
{
    bool channel_sendn_async(channel_t *ch, const char * buf, int len, int64_t exp, err::error_t * err) 
    {
        // try to send first
        int sr = 0;
        int total  = 0;
        do {
            sr = net::socket_send(ch->fd, buf + total, len - total, err );
            if ( sr > 0 ) total += sr;
        } while ( sr > 0 && total < len);

        SYM_TRACE_VA("channel_sendn_async, %d / %d sent", total, len);

        // send ok, do callback, or wait for writable
        if ( total == len ) {
            io::buffer_t obuf;
            obuf.data = (char *)buf;
            obuf.begin = obuf.end = obuf.size = len; 
            ch->iocb(ch, channel_event_sent, &obuf, ch->arg);
            return true;
        } else {

            // create a buffer node and push to channel write buffer queue
            auto * pn = (detail::nio_oper_node_t*)malloc(sizeof(detail::nio_oper_node_t));
            io::buffer_init(&pn->value.buf);
            pn->prev = pn->next = nullptr;

            pn->value.buf.data = (char *)buf;
            pn->value.buf.size = len;
            pn->value.buf.begin = total;
            pn->value.buf.end  = len;

            pn->value.flags = nio_flag_exact_size;
            pn->value.exp = exp;
        
            alg::dlinklist_push_back(&ch->wrops, pn);
            bool isok = selector_request(ch->sel, ch->fd, select_write, exp, err);
            return isok;
        }
    }
} // end namespace nio
