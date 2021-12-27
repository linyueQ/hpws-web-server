#include <iostream>
#include <time.h>
#include <unistd.h>
#include"mythreadpool.h"
using namespace std;

void funX(int slp)
{
	printf("  hello, fun1 !  %d\n", std::this_thread::get_id());
	if (slp > 0) {
		printf(" ======= funX sleep %d  =========  %d\n", slp, std::this_thread::get_id());
		std::this_thread::sleep_for(std::chrono::milliseconds(slp));
	}
}

struct gfun {
	int operator()(int n) {
		printf("%d  hello, gfun !  %d\n", n, std::this_thread::get_id());
		return 42;
	}
};

class ZZZ {
public:
	static int Afun(int n = 0) {   //函数必须是static的才能直接使用线程池
		std::cout << n << "  hello, Afun !  " << std::this_thread::get_id() << std::endl;
		return n;
	}

	static std::string Bfun(int n, std::string str, char c) {
		std::cout << n << "  hello, Bfun !  " << str.c_str() << "  " << (int)c << "  " << std::this_thread::get_id() << std::endl;
		return str;
	}
};

// 测试能否进行扩容
// void ThreadLogTask(int i, int cnt) {
// 	for (int j = 0; j < 10; j++) {
// 		cout << "PID:[%04d]======= %05d ========= " << endl;
// 		Sleep(1000);//1 sec
// 	}
// }

// void TestThreadPool() {
// 	ThreadPool threadpool(8);
// 	for (int i = 0; i < 1800; i++) {
// 		threadpool.AddTask(ThreadLogTask, i % 4, i * 10000);
// 	}
// 	getchar();
// }

#if 0
int main() {
	//TestThreadPool();
	try {
		MyThreadPool executor{ 8 };
		ZZZ a;
		std::future<void> ff = executor.AddTask(funX, 0);
		std::future<int> fg = executor.AddTask(gfun{}, 0);
		std::future<int> gg = executor.AddTask(a.Afun, 9999); //IDE��ʾ����,�����Ա�������
		std::future<std::string> gh = executor.AddTask(ZZZ::Bfun, 9998, "mult args", 123);
		std::future<std::string> fh = executor.AddTask([]()->std::string { std::cout << "hello, fh !  " << std::this_thread::get_id() << std::endl; return "hello,fh ret !"; });
		
		std::cout << " =======  sleep ========= " << std::this_thread::get_id() << std::endl;
		std::this_thread::sleep_for(std::chrono::microseconds(900));
		
		for (int i = 0; i < 50; i++) {
			executor.AddTask(funX, i * 100);
		}
		std::cout << " =======  AddTask all1 ========= "  << std::endl;
		
		std::cout << " =======  sleep ========= " << std::this_thread::get_id() << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		
		ff.get(); //调用.get()获取返回值会等待线程执行完,获取返回值
		std::cout << fg.get() << "  " << fh.get().c_str() << "  " << std::this_thread::get_id() << std::endl;
		
		std::cout << " =======  sleep ========= " << std::this_thread::get_id() << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		
		std::cout << " =======  fun1,55 ========= " << std::this_thread::get_id() << std::endl;
		executor.AddTask(funX, 55).get(); //调用.get()获取返回值会等待线程执行完
		
		std::cout << "end... " << std::this_thread::get_id() << std::endl;
		
		
		MyThreadPool pool(4);
		std::vector< std::future<int> > results;
		
		for (int i = 0; i < 8; ++i) {
			results.emplace_back(
				pool.AddTask([i] {
				std::cout << "hello " << i << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
				std::cout << "world " << i << std::endl;
				return i * i;
			})
			);
		}
		std::cout << " =======  AddTask all2 ========= " << std::this_thread::get_id() << std::endl;
		
		for (auto && result : results)
			std::cout << result.get() << ' ';
		std::cout << std::endl;
	}
	catch (std::exception& e) {
		std::cout << "some unhappy happened...  " << std::this_thread::get_id() << e.what() << std::endl;
	}
	getchar();
}
#endif