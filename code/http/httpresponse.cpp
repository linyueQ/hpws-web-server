#include "httpresponse.h"

using namespace std;

// 文件后缀 对应的 MIME-TYPE类型，用在响应头Content-Type中
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

// 响应状态码对应的描述语
const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

// 响应码对应的资源路径
const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
};

HttpResponse::~HttpResponse() {
    UnmapFile();
}

void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){
    assert(srcDir != "");
    
    if(mmFile_) { UnmapFile(); }

    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
}

void HttpResponse::MakeResponse(Buffer& buff) {
    //Step1：判断请求的资源文件是否存在（路径是否合理）
    // eg.index.html
    //  /home/ljq/WebServer-master/resources/index.html
    //  stat函数获取文件的信息并存放到Buffer里面，然后检查该文件的权限模式，
    //  其实st_mode就是一个unsigned int（linux里面的drwxrwxrwx）
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        //没找到资源
        code_ = 404;
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) {
        //S_IROTH是表示该文件只能由其他用户组读，需要确认该文件是“其他用户组”可读的
        //禁止该用户访问
        code_ = 403;
    }
    else if(code_ == -1) { 
        //数据处理成功
        code_ = 200; 
    }
    ErrorHtml_();
    // 封装http数据
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
    AddEmptyLine_(buff);
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

// 添加响应状态行
void HttpResponse::AddStateLine_(Buffer& buff) {
    string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        //除了本身已经准备好的状态，其余的全部回应Bad_Request
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    //协议版本(HTTP/1.1)-空格-状态码-状态码描述-\r\n
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

// 添加响应头
void HttpResponse::AddHeader_(Buffer& buff) {
    buff.Append("Connection: ");//key
    if(isKeepAlive_) {
        //每个头部都是一行
        buff.Append("keep-alive\r\n");//value
        //这里是写keep-alive的属性，max表示该链接最多还能接收几个http请求就要关闭
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

// 添加响应体
void HttpResponse::AddContent_(Buffer& buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    /* 将文件映射到内存提高文件的访问速度 
        MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    //下次再访问则无需陷入内核态，减少系统调用次数
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    //这里两个\r\n是因为Header结束了
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n");
}

// 添加空行
void HttpResponse::AddEmptyLine_(Buffer& buff) {
    buff.Append("\r\n");
}

// 解除内存映射
void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

string HttpResponse::GetFileType_() {
    /* 判断文件类型 */
    string::size_type idx = path_.find_last_of('.');
    if(idx == string::npos) {
        return "text/plain";
    }
    string suffix = path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer& buff, string message) 
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}
