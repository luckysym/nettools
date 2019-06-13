# pragma once

# include <sym/symdef.h>

# include <string>
# include <vector>
# include <functional>
# include <stdint.h>
# include <assert.h>

BEGIN_SYM_NAMESPACE

namespace orm {

enum EnumVarType 
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


class Value 
{
public:
    typedef void (Value::*SetterFn)(void *);
    typedef void (Value::*GetterFn)() const;

    typedef void (Value::*SetStrFn)(const char * str, int len);
    typedef std::string (Value::*GetStrFn)() const; 

private:
    int        m_type;
    int        m_len;
    intptr_t   m_offset;
    SetterFn   m_setter;
    GetterFn   m_getter;
    
public:
    Value(int vt, SetterFn setter, GetterFn getter)
        : m_type(vt), m_len(0), m_offset(-1), m_setter(setter), m_getter(getter) {}

    Value(int vt, intptr_t offset)
        : m_type(vt), m_len(0), m_offset(offset), m_setter(nullptr), m_getter(nullptr) {}

    int type() const {  return m_type; }
    int length() const { return m_len; }
    intptr_t  offset() const { return m_offset; }
    
    template<class T>
    T * ptr(void * obj) const { return (T *)((char *)obj + m_offset); }

    SetterFn setter() { return m_setter; }
    GetterFn getter() { return m_getter; }

    void setData(void * obj, const void * data, int len) {
        if ( m_type == vtString ) {
            if ( m_offset >= 0 ) {
                this->ptr<std::string>(obj)->assign((const char *)data, len);
            } else {
                SetStrFn setstr = reinterpret_cast<SetStrFn>(m_setter);
                (reinterpret_cast<Value*>(obj)->*setstr)((const char *) data, len);
            }
        } else {
            assert(false);
        }
    }

    int  get(void * data, int len);

}; // end class FieldValue

class Field
{
public:

private:
    std::string m_name;
    int         m_type;
    int         m_len;
    int         m_flags;
    Value       m_value;
public:
    Field(const char * name, int type, int length, int flags, const Value & value)
        : m_name(name), m_type(type), m_len(length), m_flags(flags), m_value(value)
    {}

    const char * name() const   { return m_name.c_str(); }
    int          type() const   { return m_type; }
    int          length() const { return m_len;  }

    const Value *      value() const { return &m_value; }
    Value *            value() { return &m_value; }
}; // end class Field

class EntityMeta
{
private:
    std::string        m_table;
    std::vector<Field> m_fields;

public:
    EntityMeta(const char * tableName, std::function<void (EntityMeta *)> initfn) {
        m_table = tableName;
        initfn(this);
    }

    const char *  tableName() const  { return m_table.c_str();  }
    int           fieldCount() const { return m_fields.size();  }
    Field *       fields(int index)  { return &m_fields[index]; }
    const Field * fields(int index) const { return &m_fields[index]; }

    Field * addField(const Field & field) {
        m_fields.push_back(field);
        return &m_fields.back();
    }
}; // class EntityMeta

} // end namespace orm

# define SYM_ORM_ENTITY_DECL \
    static NAMESPACE_SYM::orm::EntityMeta  s_entityMeta; \
    static NAMESPACE_SYM::orm::EntityMeta & meta() { return s_entityMeta; }

# define SYM_ORM_ENTITY_IMPL_BEGIN(clazz, table) \
    NAMESPACE_SYM::orm::EntityMeta  \
    clazz::s_entityMeta(table, [](NAMESPACE_SYM::orm::EntityMeta * meta) { \
        clazz * ptr = nullptr;

# define SYM_ORM_ENTITY_IMPL_END  });

# define SYM_ORM_FIELD_VAR(colname, coltype, colsize, fieldvar, vartype, flags) \
    { \
        intptr_t offset = reinterpret_cast<intptr_t>(&ptr->fieldvar); \
        Field field(colname, coltype, colsize, flags, Value(vartype, offset)); \
        meta->addField(field); \
    } \

# define SYM_ORM_FIELD_MTD(colname, coltype, colsize, varname, vartype, flags) \
    {  \
        Value value(vartype, (Value::SetterFn)&User2::set##varname, (Value::GetterFn)&User2::get##varname); \
        Field field(colname, coltype, colsize, fPrimaryKey, value); \
        meta->addField(field); \
    } \


END_SYM_NAMESPACE 
