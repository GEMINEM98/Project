#pragma once

#include"Common.h"

class ThreadCache   //线程池
{
public:

	//申请内存和释放内存
	void* Allocte(size_t size); // 开内存，从内存池开空间
	void Deallocte(void* ptr, size_t size); //释放空间，放到自由链表中

	//从中心缓存获取对象，因为要加锁，一次取批量的内存，以便下次不用加锁，直接在自由链表中取
	void* FetchFromCentralCache(size_t index);

	// 如果自由链表中对象超过一定长度就要释放给central cache
	void ListTooLong(FreeList& freeList, size_t num, size_t size);


private:

	FreeList _freeLists[NFREE_LIST]; //自由链表的数组

};

//线程TLS(线程本地存储): 用来将数据与一个正在执行的指定线程关联起来。
//用来实现：如果需要在一个线程内部的各个函数调用都能访问，但其他线程不能访问的变量，就需要TLS。
//建立把一个全局表，通过当前线程ID去查询相应的数据，因为各个线程的id不同，查到的数据自然也不同。
//TLS分动态和静态，Windows下用TlsAlloc函数。

//用静态的，更加简单：
//static ThreadCache* pThreadCache = nullptr; 这是定义了一个全局变量，属于一个进程里面的所有线程的。
//前面加上 _declspec (thread) ，表示每个线程独享的

#ifdef _WIN32
_declspec (thread) static ThreadCache* pThreadCache = nullptr;  // 每个线程都有自己的TLS，这样就可以无锁的获取自己的thread cache
#else
//linux tls
#endif

