#include <sym/odbc.h>
#include <assert.h>
#include <iostream>

namespace db = sym::odbc;
using namespace std;

int main(int argc, char **argv)
{
    db::SQLError err;
    db::SQLEnvironment env;
    bool isok = env.init(&err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<err.str()<<endl;
        return -1;
    }
    
    isok = env.SetDefaultVersion(&err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<err.str()<<endl;
        return -1;
    }

    db::SQLConnection conn;
    isok = conn.init(&env, &err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }


    return 0;
}