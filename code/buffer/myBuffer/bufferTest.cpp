#include "myBuffer.h"
#include <iostream>
#include <fcntl.h>
using namespace std;

//轮回测试
void recur_read_test(){
    Buffer buff(9);
    int tmp=10;
    int p;
    for(int i=0;i<10;i++){
        buff.Append((const void*)&tmp, 4);
        buff.ReadToDst((char*)&p,4);
        cout<<p<<endl;
        tmp+=10;
    }
}

//扩容测试
void capicity_extenstion_test(){    
    Buffer buff(11);
    int tmp1=10;
    int tmp2=20;
    int tmp3=30;
    buff.Append((const void*)&tmp1, 4);
    buff.Append((const void*)&tmp2, 4);
    buff.Append((const void*)&tmp3, 4);
    int p1,p2,p3;
    buff.ReadToDst((char*)&p1,4);
    buff.ReadToDst((char*)&p2,4);
    buff.ReadToDst((char*)&p3,4);
    cout<<p1<<endl;
    cout<<p2<<endl;
    cout<<p3<<endl;
    cout<<buff.WritableBytes()<<endl;
}

//fd测试
void Fd_test(){
    Buffer buff(9);
    int tmp=10;
    int p;
    int fd=open("test.txt",O_RDWR, O_CREAT);
    int err;
    for(int i=0;i<10;i++){
        buff.Append((const void*)&tmp, 4);
        if(i%2==0) buff.Append((const void*)&tmp, 4);
        buff.WriteFd(fd,&err);
        tmp+=10;
    }
    close(fd);
    fd=open("test.txt",O_RDONLY);
    buff.ReadFd(fd,&err);
    while(buff.ReadableBytes()>0){
        buff.ReadToDst((char*)&tmp,4);
        cout<<tmp<<endl;
    }
    cout<<"capacity="<<buff.capacity()<<endl;
}

//toStr测试
void ret_to_str_test(){
    Buffer buff(9);
    for(int i=0;i<1000;i++){
        string s;
        if(i%2==0){
            s="HTTP/1.1 " + to_string(15) + " " + "fuck" + "\r\n";
        }else{
            s="Ever_";
        }
        s+=to_string(i);
        buff.Append(s);
        string t=buff.RetrieveAllToStr();
        cout<<t<<endl;
    }
}

int main(){
    recur_read_test();
    capicity_extenstion_test();
    Fd_test();
    ret_to_str_test();
}