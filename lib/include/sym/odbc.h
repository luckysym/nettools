#pragma once

# include <sym/symdef.h>
# include <sym/utilities.h>
# include <sym/error.h>

# include <assert.h>
# include <string.h>
# include <stdint.h>

# include <sqltypes.h>
# include <sql.h>
# include <sqlext.h>

# include <sstream>
# include <string>
# include <vector>

# include <sym/odbc/sym_odbc_def.h>
# include <sym/odbc/sql_error.h>
# include <sym/odbc/sql_handle.h>
# include <sym/odbc/sql_environment.h>
# include <sym/odbc/sql_connection.h>
# include <sym/odbc/sql_statement.h>

BEGIN_SYM_NAMESPACE

/// odbc名称空间，包含对ODBC操作的封装。
namespace odbc
{
    class SQLResultSetImpl
    {
    public:
        struct ColumnDesc {
            std::string colname;
            SQLSMALLINT dbtype; 
            SQLSMALLINT colsize;
            SQLSMALLINT ctype;
            SQLSMALLINT csize; 
            SQLSMALLINT decdigits;
            SQLSMALLINT nullable;
            size_t      valueoffet;   // offset to column value pointer
        };

        struct ColumnValue
        {
            int64_t lenOrInd;
            char    data[0];
        };
    public:
        SQLStatement * m_stmt;
        std::vector<ColumnDesc> m_cols;
        std::vector<char>       m_record;   // record buffer
        bool                    m_wasNull;  // 最后一个获取的列是否是NULL；

    public:
        SQLResultSetImpl() : m_stmt(nullptr) {}

        size_t addColumn(
            SQLCHAR *colname, 
            SQLSMALLINT namelen, 
            SQLSMALLINT dbtype, 
            SQLSMALLINT colsize, 
            SQLSMALLINT decdigits,
            SQLSMALLINT nullable)
        {
            if ( m_cols.capacity() == m_cols.size() ) {
                m_cols.reserve(m_cols.size()==0?128:(m_cols.size()*2));
            }

            m_cols.push_back(ColumnDesc());
            ColumnDesc & col = m_cols.back();
            col.colname.assign((const char *)colname, namelen);
            col.dbtype = dbtype;
            col.ctype = this->dbTypeToCType(dbtype);
            col.csize = this->dbSizeToCSize(dbtype, colsize);
            col.colsize = colsize;
            col.decdigits = decdigits;
            col.nullable = nullable;
            col.valueoffet = m_record.size();

            // 计算当前字段值缓存长度（按8字节对齐）
            size_t size = col.csize + 8; // 加上LenOrInd字段
            size_t newsize = size + m_record.size();
            if ( newsize <= m_record.capacity()) {
                m_record.resize(newsize);
            } else {
                m_record.reserve( (newsize + 127) & ~127); // 一次分配长度按128为对其单位
                m_record.resize(newsize);
            }

            return m_cols.size();
        }

        size_t dbSizeToCSize(SQLSMALLINT dbtype, SQLSMALLINT colsize)
        {
            if ( SYM_VALUE_IN_LIST(dbtype, charTypes(), SQL_UNKNOWN_TYPE ) ) return SYM_PADDING(colsize, 8);
            else if ( SYM_VALUE_IN_LIST(dbtype, intTypes(), SQL_UNKNOWN_TYPE) ) return sizeof(int64_t);
            else {
                SYM_TRACE_VA("*** unsupported type: %d", dbtype);
                assert("UNSUPPORT DBTYPE " == nullptr);
            }
            
        }

        SQLSMALLINT dbTypeToCType(SQLSMALLINT dbtype ) 
        {
            if ( SYM_VALUE_IN_LIST(dbtype, charTypes(), SQL_UNKNOWN_TYPE)) return SQL_C_CHAR;
            else if (SYM_VALUE_IN_LIST(dbtype, intTypes(), SQL_UNKNOWN_TYPE)) return SQL_C_SBIGINT;
            else {
                SYM_TRACE_VA("*** unsupported type: %d", dbtype);
                assert("UNSUPPORT DBTYPE " == nullptr);
            }
        }

        const SQLSMALLINT * charTypes() {
            static const SQLSMALLINT types[] = 
                {
                    SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR, SQL_WCHAR, SQL_WVARCHAR,
                    SQL_UNKNOWN_TYPE
                };
            return types;
        }

        const SQLSMALLINT * intTypes() {
            static const SQLSMALLINT types[] = 
                {
                    SQL_INTEGER, SQL_SMALLINT, SQL_BIGINT, SQL_TINYINT, SQL_BIT,
                    SQL_UNKNOWN_TYPE
                };
            return types;
        }
    }; // end class SQLResultSetImpl

    class SQLResultSet
    {
    private:
        SYM_NONCOPYABLE(SQLResultSet);
        SQLResultSetImpl * m_impl;
    public:

        SQLResultSet();
        SQLResultSet(SQLStatement * stmt, SQLError *e);
        ~SQLResultSet();

        bool init(SQLStatement * stmt, SQLError *e);
        bool next(SQLError * e);

        int         getInt(int col) const;
        std::string getString(int col) const;
    }; // end class SQLResultSet

    class SQLParameter
    {
    private:
        util::Array<char>   m_buffer;
        size_t              m_bufsize;
        SQLLEN              m_lenOrInd;
        SQLSMALLINT         m_inout;
        SQLSMALLINT         m_ctype;
        SQLSMALLINT         m_dbtype;
        SQLULEN             m_colsize;
        SQLSMALLINT         m_decdigits;

    public:
        SQLParameter(SQLSMALLINT inOrOut, SQLSMALLINT ctype, SQLSMALLINT dbtype, SQLULEN colsize, SQLSMALLINT decdigits);
        virtual ~SQLParameter() {}

        bool bind(SQLStatement * stmt, int paramNum, SQLError * e);

    protected:
        void setValue(const void * ptr, size_t len);
    }; // end class SQLParameter

    class SQLStringParameter : public SQLParameter
    {
    public:
        SQLStringParameter(const char * value);
        virtual ~SQLStringParameter() {}
    }; // end class SQLStringParameter

    class SQLIntParameter : public SQLParameter
    {
    public:
        SQLIntParameter(int value);
        virtual ~SQLIntParameter() {}
    }; // end class SQLIntParameter

} // end namespace odbc

namespace odbc
{
    SQLIntParameter::SQLIntParameter(int value)
        : SQLParameter(SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, SQL_NTS, 0)
    {
        SQLParameter::setValue(&value, sizeof(int));
    }

} // end namespace SQLIntParameter

namespace odbc
{
    SYM_INLINE
    SQLResultSet::SQLResultSet() : m_impl(nullptr) {}

    SYM_INLINE
    SQLResultSet::SQLResultSet(SQLStatement * stmt, SQLError *e)
        : m_impl(nullptr)
    {
        this->init(stmt, e);
    }

    SYM_INLINE
    SQLResultSet::~SQLResultSet() {
        if ( m_impl ) {
            delete m_impl;
            m_impl = nullptr;
        }
    }

    SYM_INLINE
    int SQLResultSet::getInt(int col) const 
    {
        assert( col <= m_impl->m_cols.size() );
        SQLResultSetImpl::ColumnDesc & desc = m_impl->m_cols[col-1];
        SQLResultSetImpl::ColumnValue * ptr = (SQLResultSetImpl::ColumnValue *)&m_impl->m_record[desc.valueoffet];
        if ( ptr->lenOrInd == SQL_NULL_DATA ) {
            m_impl->m_wasNull = true;
            return 0;
        } else {
            m_impl->m_wasNull = false;
            return (int)(*(int64_t*)ptr->data);
        }
    }

    SYM_INLINE
    std::string SQLResultSet::getString(int col) const
    {
        assert( col <= m_impl->m_cols.size());
        SQLResultSetImpl::ColumnDesc & desc = m_impl->m_cols[col-1];
        SQLResultSetImpl::ColumnValue * ptr = (SQLResultSetImpl::ColumnValue *)&m_impl->m_record[desc.valueoffet];
        // SYM_TRACE_VA("GETSTRING offset: %ld", desc.valueoffet);
        if ( ptr->lenOrInd == SQL_NULL_DATA ) {
            m_impl->m_wasNull = true;
            return std::string();
        } else {
            m_impl->m_wasNull = false;
            SQLLEN len = *(SQLLEN*)&ptr->lenOrInd;
            return std::string(ptr->data, len);
        }
    }

    SYM_INLINE
    bool SQLResultSet::next(SQLError * e) 
    {
        SQLRETURN r = SQLFetch(m_impl->m_stmt->handle());
        // SYM_TRACE_VA("SQLFetch: %d", r);
        if ( r == SQL_NO_DATA) {
            if ( e ) *e = SQLError(SQL_SUCCESS);
            return false;  
        }
        return SYM_ODBC_MAKE_RETURN("SQLFetch", r, e, SQL_HANDLE_STMT, m_impl->m_stmt->handle());
    }

    SYM_INLINE
    bool SQLResultSet::init(SQLStatement * stmt, SQLError * e)
    {
        if ( m_impl == nullptr ) m_impl = new SQLResultSetImpl();
        m_impl->m_stmt = stmt;

        // 获取结果集列数
        SQLSMALLINT cols;
        SQLRETURN r = SQLNumResultCols(stmt->handle(), &cols);
        if ( r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) {
            return SYM_ODBC_MAKE_RETURN("SQLNumResultCols", r, e, SQL_HANDLE_STMT, stmt->handle());
        }

        SQLCHAR     colname[128];
        SQLSMALLINT namelen;
        SQLSMALLINT dbtype;
        SQLULEN     colsize;
        SQLSMALLINT decdigits;
        SQLSMALLINT nullable;

        size_t totalRecordSize = 0;

        // 获取记录每列描述信息，并分配记录缓存
        for(int i = 0; i < cols; ++i ) {
            r = SQLDescribeCol(stmt->handle(), i + 1, colname, 128, 
                &namelen, &dbtype, &colsize, &decdigits, &nullable);
            if ( r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) {
                return SYM_ODBC_MAKE_RETURN("SQLDescribeCol", r, e, SQL_HANDLE_STMT, stmt->handle());
            }
            // SYM_TRACE_VA("%s, %d, %d", colname, dbtype, colsize);
            m_impl->addColumn(colname, namelen, dbtype, colsize, decdigits, nullable);
        }

        for ( int i = 0; i < cols; ++i ) {
            SQLResultSetImpl::ColumnDesc & col = m_impl->m_cols[i];
            SQLResultSetImpl::ColumnValue * ptr = (SQLResultSetImpl::ColumnValue *)&m_impl->m_record[col.valueoffet];

            r = SQLBindCol(stmt->handle(), i + 1, col.ctype, ptr->data, col.csize, (SQLLEN*)&ptr->lenOrInd);
            if ( r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) {
                return SYM_ODBC_MAKE_RETURN("SQLBindCol", r, e, SQL_HANDLE_STMT, stmt->handle());
            }
        }
        return true;
    }

} // end namespace odbc


namespace odbc 
{
    SYM_INLINE
    SQLParameter::SQLParameter(SQLSMALLINT inOrOut, SQLSMALLINT ctype, SQLSMALLINT dbtype, SQLULEN colsize, SQLSMALLINT decdigits)
        : m_inout(inOrOut), m_ctype(ctype), m_dbtype(dbtype), m_colsize(colsize), m_decdigits(decdigits)
    { }

    SYM_INLINE
    void SQLParameter::setValue(const void * ptr, size_t len)
    {
        util::Array<char> newArray(len);
        newArray.resize(len);
        m_buffer = std::move(newArray);
        if ( len <= 16 ) for(int i = 0; i < len; ++i) m_buffer[i] = reinterpret_cast<const char*>(ptr)[i];
        else memcpy(m_buffer.data(), ptr, len);
        m_lenOrInd = len;
    }

    SYM_INLINE
    bool SQLParameter::bind(SQLStatement * stmt, int paramNum, SQLError * e)
    {
        SQLRETURN r = SQLBindParameter(
            stmt->handle(),
            paramNum, 
            m_inout,
            m_ctype,
            m_dbtype,
            m_colsize,
            m_decdigits,
            (SQLPOINTER)m_buffer.data(),
            m_bufsize,
            &m_lenOrInd
        );
        return SYM_ODBC_MAKE_RETURN("SQLBindParameter", r, e, SQL_HANDLE_STMT, stmt->handle());
    }

} // end namespace odbc

namespace odbc 
{
    SYM_INLINE
    SQLStringParameter::SQLStringParameter(const char * value) 
        : SQLParameter(SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, SQL_NTS, 0)
    {
        SQLParameter::setValue(value, strlen(value));
    }

} // end namespace odbc



END_SYM_NAMESPACE
