#include "myBuffer.h"
#include <iostream>
using namespace std;

//3、混合测试
void mixed_test(){
    
}

//2、轮回测试
void recur_read_test(){
    Buffer buff(9);
    int tmp=10;
    int p;
    for(int i=0;i<10;i++){
        buff.Append((const void*)&tmp, 4);
        buff.ReadToDst(4,(char*)&p);
        buff.Retrieve(4);
        cout<<p<<endl;
        tmp+=10;
    }
}

//1、扩容测试
void capicity_extenstion_test(){    
    Buffer buff(11);
    int tmp1=10;
    int tmp2=20;
    int tmp3=30;
    buff.Append((const void*)&tmp1, 4);
    buff.Append((const void*)&tmp2, 4);
    buff.Append((const void*)&tmp3, 4);
    int p1,p2,p3;
    memcpy((void*)&p1,buff.Peek(),4);
    buff.Retrieve(4);
    memcpy((void*)&p2,buff.Peek(),4);
    buff.Retrieve(4);
    memcpy((void*)&p3,buff.Peek(),4);
    buff.Retrieve(4);
    cout<<p1<<endl;
    cout<<p2<<endl;
    cout<<p3<<endl;
    cout<<buff.WritableBytes()<<endl;
}

int main(){
    recur_read_test();
}