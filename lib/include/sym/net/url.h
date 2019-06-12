# pragma once 

# include <sym/symdef.h>
# include <string>

BEGIN_SYM_NAMESPACE

namespace net
{
    /// \brief URL对象， [schema://][user[:passwd]@][host[:port]][/path][?query][#label]
    class URL {
    public:
        std::string m_schema;
        std::string m_user;
        std::string m_password;
        std::string m_host;
        std::string m_port;
        std::string m_path;
        std::string m_query;

        void clear() {
            m_schema.clear();
            m_user.clear();
            m_password.clear();
            m_host.clear();
            m_port.clear();
            m_path.clear();
            m_query.clear();
        }

        std::string  toString() const;
    }; // end class URL

    /// \brief URL解析器
    class URLParser
    {
    public:
        bool parse(URL * url, const char * str, err::Error * e = nullptr);
    protected:
        const char * parseSchema(URL * url, const char * str, int len, err::Error * e = nullptr);
        const char * parseUserPass(URL * url, const char * str, int len, err::Error * e = nullptr);
        const char * parseHostPort(URL * url, const char * str, int len, err::Error * e = nullptr);
        const char * parsePath(URL * url, const char * str, int len, err::Error * e = nullptr);
        const char * parseQuery(URL * url, const char * str, int len, err::Error * e = nullptr);
    private:
        const char * find(const char * str, int len1, const char chars[], int len2);
        const char * rfind(const char * str, int len1, const char chars[], int len2);
    }; // end class URLParser

} // end namespace net


namespace net
{
    bool URLParser::parse(URL * url, const char * str, err::Error * e)
    {
        int len = strlen(str);
        const char * p1 = this->parseSchema(url, str, len, e);
        if ( !p1 ) return false;
        
        len = len - (p1 - str);
        const char *p2 = this->parseUserPass(url, p1, len, e);
        if ( !p2 ) return false;

        len = len - (p2 - p1);
        const char * p3 = this->parseHostPort(url, p2, len, e);
        if ( !p3 ) return false;

        len = len - ( p3 - p2 );
        const char *p4 = this->parsePath(url, p3, len, e);
        if ( !p4 ) return false;

        len = len - ( p4 - p3 );
        const char * p5 = this->parseQuery(url, p4, len, e);
        if ( !p5 ) return false; 

        return true;
    } // end URLParser::parse

    const char * URLParser::parseSchema(URL * url, const char * str, int len, err::Error * e)
    {
        const char * p = this->find(str, len, ":/", 2);
        if ( p == nullptr ) return str;   // not found

        if ( p[0] == ':' && p[1] == '/' && p[2] == '/' ) {
            url->m_schema.assign(str, p - str);
            return p + 3;
        }  else {
            return str;
        }
    }
    
    const char * URLParser::parseUserPass(URL * url, const char * str, int len, err::Error * e)
    {
        const char * p0 = this->find(str, len, "@/", 2 );
        if ( !p0 || p0[0] != '@' ) return str;  // 没有user部分，直接返回str
        
        // 截取出user:pass部分, 再解析user和pass
        int len1 = p0 - str;
        const char * p1 = this->find(str, len1, ":", 1);
        if ( p1 && p1[0] == ':') {
            url->m_user.assign(str, p1 - str);
            url->m_password.assign(p1 + 1, p0 - (p1 + 1));
        } else {
            url->m_user.assign(str, p0 - str);
            url->m_password = "";
        }
        return p0 + 1;
    }

    const char * URLParser::parseHostPort(URL * url, const char * str, int len, err::Error * e)
    {
        const char *p0 = this->find(str, len, "/?#", 1);
        if ( p0 && p0[0] != '/' ) return nullptr;   // 找到，但不是/，是为格式错误
        if ( !p0 ) p0 = str + len;
        
        int len1 = p0 - str;  // 截取host:port部分
        int len2 = len1;      // host部分
        const char * p1 = this->rfind(str, len1, ":].", 3);
        if ( p1[0] == ':' ) {
            url->m_port.assign(p1 + 1, p0 - (p1 + 1));
            len2 = len1 - (p0 - p1);
        }
        else p1 = p0;  // 没有port，重新指向结尾

        // 解析host部分
        url->m_host.assign(str, len2);
        return p0;
    }
    const char * URLParser::parsePath(URL * url, const char * str, int len, err::Error * e)
    {
        const char * p0 = this->find(str, len, "#?", 2);
        if ( !p0 ) p0 = str + len;

        const char * p1 = this->find(str, len, "/", 1);
        if ( p1 ) {
            url->m_path.assign(p1, p0 - p1);
        } else {
            url->m_path.assign(str, p0 - str);
        }

        return p0;
    }

    const char * URLParser::parseQuery(URL * url, const char * str, int len, err::Error * e)
    {
        const char * p0 = this->find(str, len, "?", 2);
        if ( p0 ) {
            url->m_query.assign(p0 + 1);
            return str + len;
        } else {
            return str;
        }
    }

    const char * URLParser::find(const char * str, int len1, const char chars[], int len2)
    {
        for( int i = 0; i < len1; ++i ) {
            for (int t = 0; t < len2; ++t) {
                if ( str[i] == chars[t]) return str + i;
            }
        }
        return nullptr;
    }

    const char * URLParser::rfind(const char * str, int len1, const char chars[], int len2)
    {
        // 反向查找
        for( int i = len1 - 1; i >= 0; --i ) {
            for (int t = 0; t < len2; ++t) {
                if ( str[i] == chars[t]) return str + i;
            }
        }
        return nullptr;
    }

    std::string  URL::toString() const {
        std::string str;
        str.reserve(1024);
        if ( !m_schema.empty() ) str.append(m_schema).append("://");
        if ( !m_host.empty() ) {
            if ( !m_user.empty() ) {
                str.append(m_user);
                if ( !m_password.empty() ) str.append(1, ':').append(m_password);
                str.append(1, '@');
            }
            str.append(m_host);
            if ( !m_port.empty() ) str.append(1, ':').append(m_port);
        }
        if ( !m_path.empty() ) {
            if ( m_path[0] != '/') str.append(1, '/');
            str.append(m_path);
        }
        if ( !m_query.empty() ) {
            if ( m_path.empty() ) str.append(1, '/');
            str.append(m_query);
        }
        return str;
    }

} // end namespace net

END_SYM_NAMESPACE
