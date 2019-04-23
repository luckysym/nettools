#include <sym/nio.h>

namespace nio
{
    bool channel_recvsome_async(channel_t *ch, char * buf, int len, int64_t exp, err::error_t * err) 
    {
        // try to recv first
        int rr = 0;
        int total  = 0;

        do {
            rr = net::socket_recv(ch->fd, buf + total, len - total, err );
            if ( rr > 0 ) total += rr;
        } while ( rr > 0 && total < len );

        SYM_TRACE_VA("channel_recvsome_async, %d / %d sent", total, len);

        // readsome只要收到数据就返回，一个没收到就selecting
        // 所以只要收到数据或者出错，直接回调；没有收到数据，就申请select
        if ( total > 0 || rr < 0 ) {
            io::buffer_t ibuf;
            ibuf.data = (char *)buf;
            ibuf.begin = 0;
            ibuf.end = total;
            ibuf.size = len;

            int event = channel_event_received;
            if ( rr < 0 ) event |= channel_event_error;
            ch->iocb(ch, event, &ibuf, ch->arg);

            return true;
        } else {
            // create a buffer node and push to channel read buffer queue
            auto * pn = (detail::nio_oper_node_t*)malloc(sizeof(detail::nio_oper_node_t));
            io::buffer_init(&pn->value.buf);
            pn->prev = pn->next = nullptr;

            pn->value.buf.data = (char *)buf;
            pn->value.buf.size = len;
            pn->value.buf.begin = total;
            pn->value.buf.end  =  0;
            pn->value.flags = 0;
            pn->value.exp = exp;
        
            alg::dlinklist_push_back(&ch->rdops, pn);
            bool isok = selector_request(ch->sel, ch->fd, select_read, exp, err);
            return isok;
        }
    } // end channel_recvsome_async

    inline 
    bool channel_recvn_async(channel_t *ch, char * buf, int len, int64_t exp, err::error_t * err)
    {
        // try to recv first
        // try to recv first
        int rr = 0;
        int total  = 0;

        do {
            rr = net::socket_recv(ch->fd, buf + total, len - total, err );
            if ( rr > 0 ) total += rr;
        } while ( rr > 0 && total < len );
        
        if ( rr < 0 ) {
            // 发送失败，recv error 回调
            io::buffer_t ibuf;
            ibuf.data = (char *)buf;
            ibuf.begin = 0;
            ibuf.end = total;
            ibuf.size = len;

            int event = channel_event_received;
            if ( rr < 0 ) event |= channel_event_error;
            ch->iocb(ch, event, &ibuf, ch->arg);

            return true;
        } else if ( total == len ) {   // rr >= 0 && total == len
            // 收到全部的消息，received 回调
            io::buffer_t ibuf;
            ibuf.data = (char *)buf;
            ibuf.begin = 0;
            ibuf.end = total;
            ibuf.size = len;

            ch->iocb(ch, channel_event_received, &ibuf, ch->arg);
            return true;
        } else {      // rr >= 0 && total < len
            // 没有全收到, async request
            auto * pn = (detail::nio_oper_node_t*)malloc(sizeof(detail::nio_oper_node_t));
            io::buffer_init(&pn->value.buf);
            pn->prev = pn->next = nullptr;

            pn->value.buf.data = (char *)buf;
            pn->value.buf.size = len;
            pn->value.buf.begin = 0;
            pn->value.buf.end   = total;
            pn->value.flags = nio_flag_exact_size;
            pn->value.exp   = exp;
        
            alg::dlinklist_push_back(&ch->rdops, pn);
            bool isok = selector_request(ch->sel, ch->fd, select_read, exp, err);
            return isok;
        }
    } // end channel_recvb_async
} // end namespace nio