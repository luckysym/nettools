#include <sym/odbc.h>
#include <assert.h>
#include <iostream>

namespace db = sym::odbc;
using namespace std;

int main(int argc, char **argv)
{
    db::SQLError err;
    
    // 初始化ODBC环境
    db::SQLEnvironment env;
    bool isok = env.init(&err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<err.str()<<endl;
        return -1;
    }
    
    // 初始化数据库连接
    db::SQLConnection conn;
    isok = conn.init(&env, &err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }

    // 打开连接
    isok = conn.open("LOCAL_3301_SHI", "shi", "shi", &err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }

    // 创建并初始化Statement
    db::SQLStatement stmt1(&conn, &err);
    if ( err.rcode() == SQL_ERROR ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }
    // 先尝试删除测试表
    isok = stmt1.exec("drop table test1", &err);
    if ( !isok ) {
        // 表可能不存在，因此这里报错不返回
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
    }

    // 创建测试表
    const char * sqltext1 = "create table test1 ( id varchar(64) primary key )";
    isok = stmt1.exec(sqltext1, &err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }

    // 使用prepare/execute插入一条固定语句的数据
    sqltext1 = "insert into test1 (id) values ( 'hello' )";
    isok = stmt1.prepare(sqltext1, &err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }
    isok = stmt1.execute(&err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }

    // 执行一条参数化插入语句
    sqltext1 = "insert into test1 (id) values ( ? )";
    isok = stmt1.prepare(sqltext1, &err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }
    db::SQLStringParameter param1("world");
    isok = param1.bind(&stmt1, 1, &err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }
    isok = stmt1.execute(&err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }

    // 查询数据集
    sqltext1 = "select * from test1";
    isok = stmt1.exec(sqltext1, &err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }
    db::SQLResultSet rs1;
    isok = rs1.bind(&stmt1, &err);
    isok = stmt1.exec(sqltext1, &err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }
    while ( rs1.next(&err) ) {
        cout<<rs1.getString(1)<<endl;
    }

    // 关闭连接
    isok = conn.close(&err);
    if ( !isok ) {
        cout<<__FILE__<<':'<<__LINE__<<' '<<err.str()<<endl;
        return -1;
    }

    cout<<"ok"<<endl;
    return 0;
}