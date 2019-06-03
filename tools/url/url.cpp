#include <sym/network.h>

int main(int argc, char **argv)
{
    using namespace net;
    bool isok;
    URLParser parser;
    err::Error e;

    URL u0;
    const char * s0 = "127.0.0.1";
    isok = parser.parse(&u0, s0, &e);
    printf("u0 = %s\n", u0.toString().c_str());
    assert(isok);

    URL u1;
    const char * s1 = "http://127.0.0.1";
    isok = parser.parse(&u1, s1, &e);
    printf("u1 = %s\n", u1.toString().c_str());
    assert(isok);

    URL u2;
    const char * s2 = "http://127.0.0.1:9090";
    isok = parser.parse(&u2, s2, &e);
    printf("u2 = %s\n", u2.toString().c_str());
    assert(isok);

    URL u3;
    const char * s3 = "http://127.0.0.1:9090/data01/rel/file.txt";
    isok = parser.parse(&u3, s3, &e);
    printf("u3 = %s\n", u3.toString().c_str());
    assert(isok);

    URL u4;
    const char * s4 = "http://127.0.0.1:9090/data01/rel/file.txt?hello=1234&world=5678";
    isok = parser.parse(&u4, s4, &e);
    printf("u4 = %s\n", u4.toString().c_str());
    assert(isok);

    URL u5;
    const char * s5 = "http://user@127.0.0.1:9090/data01/rel/file.txt?hello=1234&world=5678";
    isok = parser.parse(&u5, s5, &e);
    printf("u5 = %s\n", u5.toString().c_str());
    assert(isok);

    URL u6;
    const char * s6 = "http://user:pass@127.0.0.1:9090/data01/rel/file.txt?hello=1234&world=5678";
    isok = parser.parse(&u6, s6, &e);
    printf("u6 = %s\n", u6.toString().c_str());
    assert(isok);

    URL u7;
    const char * s7 = "http://user:pass@127.0.0.1:9090/";
    isok = parser.parse(&u7, s7, &e);
    printf("u7 = %s \n", u7.toString().c_str());
    assert(isok);

    URL u8;
    const char * s8 = "http://user:pass@[fe80::7541:b25:e9e0:6c66]:9090/";
    isok = parser.parse(&u8, s8, &e);
    printf("u8 = %s\n",  u8.toString().c_str());
    assert(isok);

    URL u9;
    const char * s9 = "user:pass@[fe80::7541:b25:e9e0:6c66]:9090";
    isok = parser.parse(&u9, s9, &e);
    printf("u9 = %s \n", u9.toString().c_str());
    assert(isok);

    URL u10;
    const char * s10 = "www.myhost.com:9090";
    isok = parser.parse(&u10, s10, &e);
    printf("u10 = %s\n", u10.toString().c_str());
    assert(isok);

    return 0;
}