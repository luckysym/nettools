# include <sym/orm.h>

using namespace sym::orm;

struct User
{
    static NAMESPACE_SYM::orm::EntityMeta  s_entityMeta; \
    static NAMESPACE_SYM::orm::EntityMeta & meta() { return s_entityMeta; }

    std::string m_uid;
    std::string m_user;
    std::string m_pwd;
    
    std::string toString() const 
    {
        std::string str;
        str.reserve(128);
        return str.append(m_uid).append("/").append(m_user).append("/").append(m_pwd);
    }
}; // end class User

EntityMeta User::s_entityMeta("au_users", [](EntityMeta * meta) {
        
        User * ptr = nullptr;
        {
            intptr_t offset = reinterpret_cast<intptr_t>(&ptr->m_uid);
            Field field("uid", dbVarchar, 64, fPrimaryKey, Value(vtString, offset));
            meta->addField(field);
        }
        {
            intptr_t offset = reinterpret_cast<intptr_t>(&ptr->m_user);
            Field field("username", dbVarchar, 64, fPrimaryKey, Value(vtString, offset));
            meta->addField(field);
        }
        {
            intptr_t offset = reinterpret_cast<intptr_t>(&ptr->m_pwd);
            Field field("passwd", dbVarchar, 64, fPrimaryKey, Value(vtString, offset));
            meta->addField(field);
        }
    } 
);

class User2
{
public:
    static NAMESPACE_SYM::orm::EntityMeta  s_entityMeta; \
    static NAMESPACE_SYM::orm::EntityMeta & meta() { return s_entityMeta; }

private:
    std::string m_uid;
    std::string m_user;
    std::string m_pwd;
    
public:

    void setUid(const char * str, int len) { m_uid.assign(str, len); }
    std::string getUid() const { return m_uid; }

    void setUser(const char * str, int len) { m_user.assign(str, len); }
    std::string getUser() const { return m_user; }

    void setPassword(const char * str, int len) { m_pwd.assign(str, len); }
    std::string getPassword() const { return m_pwd; }

    std::string toString() const 
    {
        std::string str;
        str.reserve(128);
        return str.append(m_uid).append("/").append(m_user).append("/").append(m_pwd);
    }
}; // end class User

EntityMeta User2::s_entityMeta("au_users", [](EntityMeta * meta) {
        
        User * ptr = nullptr;
        {
            Value value(vtString, (Value::SetterFn)&User2::setUid, (Value::GetterFn)&User2::getUid);
            Field field("uid",dbVarchar, 64, fPrimaryKey, value);
            meta->addField(field);
        }
        {
            Value value(vtString, (Value::SetterFn)&User2::setUser, (Value::GetterFn)&User2::getUser);
            Field field("username",dbVarchar, 64, fPrimaryKey, value);
            meta->addField(field);
        }
        {
            Value value(vtString, (Value::SetterFn)&User2::setPassword, (Value::GetterFn)&User2::getPassword);
            Field field("passwd",dbVarchar, 64, fPrimaryKey, value);
            meta->addField(field);
        }
    } 
);

struct User3 
{
    SYM_ORM_ENTITY_DECL

    std::string m_uid;
    std::string m_user;
    std::string m_pwd;

    std::string toString() const 
    {
        std::string str;
        str.reserve(128);
        return str.append(m_uid).append("/").append(m_user).append("/").append(m_pwd);
    }
};

SYM_ORM_ENTITY_IMPL_BEGIN(User3, "au_users")
    SYM_ORM_FIELD_VAR("uid", sym::orm::dbVarchar, 64, m_uid, sym::orm::vtString, sym::orm::fPrimaryKey)
    SYM_ORM_FIELD_VAR("username", sym::orm::dbVarchar, 64, m_user, sym::orm::vtString, 0)
    SYM_ORM_FIELD_VAR("passwd", sym::orm::dbVarchar, 64, m_pwd, sym::orm::vtString, 0)
SYM_ORM_ENTITY_IMPL_END


class User4
{
public:
    SYM_ORM_ENTITY_DECL
private:
    std::string m_uid;
    std::string m_user;
    std::string m_pwd;
    
public:

    void setUid(const char * str, int len) { m_uid.assign(str, len); }
    std::string getUid() const { return m_uid; }

    void setUser(const char * str, int len) { m_user.assign(str, len); }
    std::string getUser() const { return m_user; }

    void setPassword(const char * str, int len) { m_pwd.assign(str, len); }
    std::string getPassword() const { return m_pwd; }

    std::string toString() const 
    {
        std::string str;
        str.reserve(128);
        return str.append(m_uid).append("/").append(m_user).append("/").append(m_pwd);
    }
}; // end class User4

SYM_ORM_ENTITY_IMPL_BEGIN(User4, "au_users")
    SYM_ORM_FIELD_MTD("uid", sym::orm::dbVarchar, 64, Uid, sym::orm::vtString, sym::orm::fPrimaryKey)
    SYM_ORM_FIELD_MTD("username", sym::orm::dbVarchar, 64, User, sym::orm::vtString, 0)
    SYM_ORM_FIELD_MTD("passwd", sym::orm::dbVarchar, 64, Password, sym::orm::vtString, 0)
SYM_ORM_ENTITY_IMPL_END

#include <iostream>

void feed_by_offset()
{
    using namespace std;
    User user;
    EntityMeta & meta = User::meta();

    Field * field0 = meta.fields(0);
    Value * value0 = field0->value();
    if ( field0->type() == dbVarchar) {
        value0->setData(&user, "123456", 6);
    }

    Field * field1 = meta.fields(1);
    Value * value1 = field1->value();
    if ( field1->type() == dbVarchar) {
        value1->setData(&user, "shi", 3);
    }

    Field * field2 = meta.fields(2);
    Value * value2 = field2->value();
    if ( field2->type() == dbVarchar) {
        value2->setData(&user, "shi1234", 7);
    }
    
    cout<<user.toString()<<endl;
}


void feed_by_setter()
{
    using namespace std;
    User2 user;
    EntityMeta & meta = User2::meta();

    Field * field0 = meta.fields(0);
    Value * value0 = field0->value();
    if ( field0->type() == dbVarchar) {
        value0->setData(&user, "654321", 6);
    }

    Field * field1 = meta.fields(1);
    Value * value1 = field1->value();
    if ( field1->type() == dbVarchar) {
        value1->setData(&user, "hao", 3);
    }

    Field * field2 = meta.fields(2);
    Value * value2 = field2->value();
    if ( field2->type() == dbVarchar) {
        value2->setData(&user, "hao1234", 7);
    }
    
    cout<<user.toString()<<endl;
}

void feed_by_offset_macro()
{
    using namespace std;
    User3 user;
    EntityMeta & meta = User3::meta();

    Field * field0 = meta.fields(0);
    Value * value0 = field0->value();
    if ( field0->type() == dbVarchar) {
        value0->setData(&user, "223344", 6);
    }

    Field * field1 = meta.fields(1);
    Value * value1 = field1->value();
    if ( field1->type() == dbVarchar) {
        value1->setData(&user, "jay", 3);
    }

    Field * field2 = meta.fields(2);
    Value * value2 = field2->value();
    if ( field2->type() == dbVarchar) {
        value2->setData(&user, "jay1234", 7);
    }
    
    cout<<user.toString()<<endl;
}

void feed_by_setter_macro()
{
    using namespace std;
    User4 user;
    EntityMeta & meta = User4::meta();

    Field * field0 = meta.fields(0);
    Value * value0 = field0->value();
    if ( field0->type() == dbVarchar) {
        value0->setData(&user, "567890", 6);
    }

    Field * field1 = meta.fields(1);
    Value * value1 = field1->value();
    if ( field1->type() == dbVarchar) {
        value1->setData(&user, "kkk", 3);
    }

    Field * field2 = meta.fields(2);
    Value * value2 = field2->value();
    if ( field2->type() == dbVarchar) {
        value2->setData(&user, "kkk1234", 7);
    }
    
    cout<<user.toString()<<endl;
}


int main(int argc, char **argv)
{
    feed_by_offset();
    feed_by_setter();
    feed_by_offset_macro();
    feed_by_setter_macro();
    return 0;
}

/*
#include <functional>
#include <string>
#include <iostream>

using namespace std;

class Fake
{
public:
    string mstr;   
public:
    Fake(std::function<void ()> fn) 
    {
        fn();
    }
};

int main(int argc, char **argv)
{
    Fake fake([&fake](){
        cout<<"init class"<<endl;
        fake.mstr = "fake class";
    });

    cout<<fake.mstr<<endl;
    return 0;
}
*/
