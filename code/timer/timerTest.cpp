#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include "rbtimer.h"

using namespace std;

//g++ rbtimer.cpp timerTest.cpp -o test
TimeStamp originTime = Clock::now();

void func(int id){
    cout<<"expire: "<<"id = "<<id<<", timeout = "<<std::chrono::duration_cast<MS>(Clock::now() - originTime).count()<<endl;
}

// int main(){
//     RBTimer *timer_=new RBTimer();
//     int n=100;
//     cout<< endl <<"============================== regist start =============================="<<endl<<endl;
//     for(int i=0;i<n;i++){
//         auto timeoutMS_ = 1000 + rand()%10000;
//         auto expireTime = Clock::now() + (MS)timeoutMS_;
//         cout<<"regist: "<<"id = "<<i<<", timeout = "<< \
//             std::chrono::duration_cast<MS>(expireTime - originTime).count()<<endl;
//         timer_->add(i, timeoutMS_, bind(func, i));
//     }
//     cout<< endl <<"=============================== check map ==============================="<<endl<<endl;
//     for(int i=0;i<n;i++){
//         cout<<"map: "<<"id = "<<i<<", timeout = "<< timer_->getExpire(i).count()<<endl;
//     }
//     cout<< endl <<"============================== expire start =============================="<<endl<<endl;
//     while(1){
//         timer_->getNextTick();
//     }
// }

