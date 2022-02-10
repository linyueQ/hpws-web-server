#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

// 初始化请求对象信息
void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE; 
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

// 解析HTTP请求的数据
bool HttpRequest::parse(Buffer& buff) {
    printf("parsing Data\n");
    const char CRLF[] = "\r\n"; // 行结束符
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    // buff中有数据可读，并且状态没有到FINISH，就一直解析
    while(buff.ReadableBytes() && state_ != FINISH) {
        // 获取一行数据，找到缓冲区当前第一个\r\n为结束标志
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd);//特殊的string构造形式
        switch(state_)
        {
            case REQUEST_LINE:
                // 解析请求首行
                if(!ParseRequestLine_(line)) {
                    return false;
                }
                // 解析出请求资源路径
                ParsePath_();
                break;
            case HEADERS:
                // 解析请求头
                ParseHeader_(line);
                // 这里是为了处理只有“请求行”和“HEADER”的报文，如果没有BODY的数据，
                //  则readbuff剩下\r\n
                if(buff.ReadableBytes() <= 2) {
                    state_ = FINISH;
                }
                break;
            case BODY:
                // 解析请求体
                ParseBody_(line);
                break;
            default:
                break;
        }
        if(lineEnd == buff.BeginWrite()) { break; }//说明没数据可读，因为lineEnd找不到数据了
        buff.RetrieveUntil(lineEnd + 2);//剩下2个字符 (\r\n)，所以+2
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    printf("parse Data Finish\n");
    return true;
}

void HttpRequest::ParsePath_() {
    // 如果访问根目录，默认表示访问index.html
    // 例如 http://192.168.110.111:10000/
    if(path_ == "/") {
        path_ = "/index.html"; 
    }
    else {
        // 其他默认的一些页面
        // 例如 http://192.168.110.111:10000/regist
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line) {
    // GET / HTTP/1.1
    //[^ ]*表示除了空格以外的任意多个字符
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
// Connection: keep-alive
void HttpRequest::ParseHeader_(const string& line) {
    // 使用正则表达式分离出key:value这种配对
    // 第一个^表示字符串开头，()表示子表达式的开始结束位置，$表示字符串结尾
    //  第二个^表示取反，?表示匹配前面的子表达式零次或一次，.表示任意字符
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    //注意header会有多行
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {//这里是不匹配的情况，解析到header和body之间的\r\n会出现这种情况
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

// 将十六进制的字符，转换成十进制的整数
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        // 解析表单信息
        ParseFromUrlencoded_();
        // 检查请求路径是否为register.html和login.html中的一个
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                //根据tag判断表单信息是否来自login的页面
                bool isLogin = (tag == 1);
                //根据结果返回html
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }   
}

void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }
    // 这里的解析格式其实是由Content-Type决定的，前端html页面以表单的形式提交数据，所以
    //  Content-Type为application/x-www-form-urlencoded，其格式为：key1=value1&key2=value2&...
    //  eg. username=zhangsan&password=123
    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;
    
    //继续使用switch进行解析
    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            //获取key值
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            //该content-type会在传输数据时把空格替换成加号，所以现在要替换回来
            body_[i] = ' ';
            break;
        case '%':
            // 简单的加密操作，存进数据库里面，比如名字为中文的时候，传输过来的数据就是一串
            //  带%的码，eg.username=%E9%AB%98%E8&password=123
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            //获取value值
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

// 用户验证（整合了登录和注册的验证）
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    assert(sql);//这里太暴力了，正常来说应该进行重试，到达重试次数以后发送一个数据库繁忙的http响应
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };        //存放sql命令
    MYSQL_FIELD *fields = nullptr;  //存放结果集的列信息（行数列数啥的）
    MYSQL_RES *res = nullptr;       //存放结果集
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码（正常来说不应该这样写死，容易被SQL注入攻击） */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) {       //执行mysql命令
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);      //将数据库中查询(mysql_query)得到的结果(集合)存放在MYSQL_RES结构中
    j = mysql_num_fields(res);          //返回结果集中的列的数目
    fields = mysql_fetch_fields(res);   //返回结果集中的列信息

    //MYSQL_ROW是数组，返回结果集(MYSQL_RES)的当前行的结果（逐行取用），结果不为空则进入判断逻辑
    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 登录行为 and 用户名未被使用*/
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } else { /* 注册行为，用户名已存在*/
            flag = false; 
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 and 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}