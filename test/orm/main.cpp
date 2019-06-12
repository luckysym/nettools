# include <sym/orm.h>
# include <string>
# include <stdint.h>
# include <stdarg.h>

class Value 
{
public:
    typedef void (Value::*SetterFn)(void *);
    typedef void (Value::*GetterFn)() const;

    typedef void (Value::*SetStrFn)(const std::string &);
    typedef std::string (Value::*GetStrFn)() const; 

    enum EnumType 
    {
        vtVoid,
        vtString,
        vtCString,
        vtInt16,
        vtInt32,
        vtInt64,
        vtFloat,
        vtDouble
    };

public:
    Value(int vt, SetterFn setter, GetterFn getter); 
    Value(int vt, intptr_t offset); 

    int type() const;
    int length() const;
    intptr_t  offset() const;
    
    template<class T>
    T * ptr(void * obj) { return nullptr; }

    template<class T>
    const T * ptr(void * obj) const  { return nullptr; }

    SetterFn * setter();
    GetterFn * getter();
}; // end class FieldValue


class Field
{
public:
    enum EnumDbType 
    {
        dbNone,
        dbVarchar,
        dbChar,
        dbDecimal
    };

    enum EnumFlags
    {
        fPrimaryKey = 1,
    };
public:
    Field(const char * name, int type, int length, int flags, const Value & value);

    const char *       name() const;
    int                type() const;
    int                length() const;

    const Value *      value() const;
    Value *            value();
}; // end class Field

class EntityMeta
{
public:
    EntityMeta(const char * tableName, ...);

    const char *  tableName() const;
    int           fieldCount() const;
    Field *       fields(int index);
    const Field * fields(int index) const;
}; // class EntiryMeta


struct User
{
    static EntityMeta  s_meta;
    static EntityMeta & meta() { return s_meta; }

    std::string m_uid;
    std::string m_user;
    std::string m_pwd;
    
    std::string toString() const {
        std::string str;
        str.reserve(128);
        return str.append(m_uid).append("/").append(m_user).append("/").append(m_pwd);
    }
}; // end class User

EntityMeta User::s_meta("au_users",
    Field("uid", Field::dbVarchar, 64, Field::fPrimaryKey, Value(Value::vtString, 0)),
    Field("username", Field::dbVarchar, 64, 0, Value(Value::vtString, sizeof(std::string))),
    Field("passwd", Field::dbVarchar, 64, 0, Value(Value::vtString, sizeof(std::string)))
);

#include <iostream>

int main(int argc, char **argv)
{
    using namespace std;
    User user;
    EntityMeta & meta = User::meta();

    Field * field0 = meta.fields(0);
    Value * value0 = field0->value();
    if ( field0->type() == Field::dbVarchar) {
        if ( value0->type() == Value::vtString ) {
            if ( value0->offset() >= 0 ) {
                *value0->ptr<std::string>(&user) = "123456";
            }
        }
    }

    Field * field1 = meta.fields(1);
    Value * value1 = field1->value();
    if ( field1->type() == Field::dbVarchar) {
        if ( value1->type() == Value::vtString ) {
            if ( value1->offset() >= 0 ) {
                *value1->ptr<std::string>(&user) = "shi";
            }
        }
    }

    Field * field2 = meta.fields(2);
    Value * value2 = field2->value();
    if ( field2->type() == Field::dbVarchar) {
        if ( value2->type() == Value::vtString ) {
            if ( value2->offset() >= 0 ) {
                *value2->ptr<std::string>(&user) = "shi1234";
            }
        }
    }
    
    cout<<user.toString()<<endl;
    return 0;
}