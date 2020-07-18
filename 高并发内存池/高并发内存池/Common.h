#pragma once

// 内存泄漏会导致有些页合不回去！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！

#include<iostream>
#include<stdlib.h>
#include<assert.h>
#include<map>
#include<unordered_map>
#include<thread>
#include<mutex>

#ifdef _WIN32
#include<Windows.h>
#endif //_WIN32


//using namespace std;

using std::cout;
using std::endl;


const size_t MAX_SIZE = 64 * 1024;  //  内存池的大小     C++用const、枚举替代宏  
const size_t NFREE_LIST = MAX_SIZE / 8;  // 每个自由链表之间的间隔，相当于内存池结点的间隔
const size_t MAX_PAGES = 129;  // 直接映射的，下标是0忽略，剩下的一对一；超过128页=500多kb，就直接向系统要。
const size_t PAGE_SHIFT = 12;  // 4k为页移位  1<<12 = 2^12 = 4K


inline void*& NextObj(void* obj)  // 调一个函数是有消耗的，频繁调用 定义成内联inline
{
	return *((void**)obj);    //取下一个对象
}


class FreeList   //自由链表类
{
public:
	void Push(void* obj)  //给自由链表中头插对象
	{
		// 头插
		NextObj(obj) = _freelist;
		_freelist = obj;
		++_num;
	}

	void PushRange(void* head, void* tail, size_t num)  // 将新申请的19个内存块都push到自由链表中
	{
		NextObj(tail) = _freelist;
		_freelist = head;
		_num += num;
	}

	void* Pop()  // 在自由链表有对象的情况下，取一个对象
	{
		// 头删
		void* obj = _freelist;
		_freelist = NextObj(obj); // 把第二个结点的地址赋给目标链表的指针
		--_num;
		return obj;
	}

	size_t PopRange(void*& start, void*& end, size_t num)  // 拿走_freelist的一段
	{
		size_t actualNum = 0;
		void* prev = nullptr;
		void* cur = _freelist;
		for (; actualNum < num && cur != nullptr; ++actualNum) // 用prev和cur找出要拿走那一段结点的结束位置。
		{     // 实际的  < 想要的
			prev = cur;
			cur = NextObj(cur);
		}

		start = _freelist;  // 被拿走一段的起始(第一个结点)
		end = prev;         // 被拿走一段的结束(最后一个结点)
		_freelist = cur;    // 将_freelist指针指向剩余部分的起始位置。(重置_freelist指针)

		_num -= actualNum;

		return actualNum;
	}

	size_t Num()
	{
		return _num;
	}

	bool Empty()
	{
		return _freelist == nullptr;
	}

	void Clear()
	{
		_freelist = nullptr;
		_num = 0;
	}

private:
	void* _freelist = nullptr; //这个指针后面可以挂不同大小的对象，这个只是声明，不是定义，没有开空间
	// 没有显示实现构造函数，编译器会默认生成构造函数，用缺省值进行初始化

	size_t _num = 0;

};


class SizeClass   // 专门算数据大小的类
{
public:
	//都是static，因为没有 成员变量，这些接口都用类封装起来


	// 优化前：            ***************************************************************************** 一般的代码
	//static size_t ListIndex(size_t size)  // 找自由链表的对应内存块下标
	//{
	//	if (size % 8 == 0)
	//	{
	//		return size / 8 - 1;
	//	}
	//	else
	//	{
	//		return size / 8;
	//	}
	//}

	//static size_t RoundUp(size_t size)  // 向上对齐，你要申请5b，我给你个8b
	//{
	//	if (size % 8 != 0)
	//	{
	//		return (size / 8 + 1) * 8;
	//	}
	//	else
	//	{
	//		return size;
	//	}
	//}


	// 优化后：            ***************************************************************************** 高级的代码


	// 控制在[1%，11%]左右的内碎片浪费（第一个比较特殊，其他的都满足这个范围）
	// 1k = 1024b
	//                                                                 要128b的内存，浪费7b；         要8b的内存，浪费7b
	// [1,128]          2^3=8byte对齐         freelist[0,16)    min浪费：7 / 128 = 5.4687%    max浪费：7 / (0+8) = 87%  这个是浪费最大的，没办法控制
	// [129,1024]        2^4=16byte对齐       freelist[16,72)           15 / 1024 = 1.4648%           15 / (128+16) = 10%
	// [1025,8*1024]      2^7=128byte对齐     freelist[72,128)         127 / (8*1024)=1.5686%        127 / (1024+128) = 11.0%
	// [8*1024+1,64*1024]  2^10=1024byte对齐  freelist[128,184)       1023 / (64*1024)=1.5609%      1023 / (8*1024+1024) = 11.1%

	static size_t _RoundUp(size_t size, size_t alignment)  // 申请内存大小的向上对齐，alignment是对齐数，对齐到对齐数alignment(8、16、32...)的位置。你要申请5b，我给你个8b
	{
		return (size + alignment - 1) & (~(alignment - 1));  // +  &  ~ 比 /  % 快
		// [9-16] + 7 = [16-23] -> 比特位：16 8 4 2 1中，16位必为1，与上低三位为0,其他位为1，也就是与上~7
		// [17-32] + 15 = [32-47] -> 比特位：32 16 8 4 2 1中，32位必为1，与上低四位为0,其他位为1，也就是与上~15

	}
	static inline size_t RoundUp(size_t size)  //在thread缓存向central缓存申请内存时使用-------------------------------算的是thread cache申请内存时的单位大小
	{                                                                                                                        // 控制内存碎片

		// 不用一般方法的除法和取模，因为效率低

		assert(size <= MAX_SIZE);

		// if条件中的范围，基本上也是以 8 倍扩大的
		if (size <= 128) {
			return _RoundUp(size, 8); // 要申请 128byte 之内的内存，就以 8byte 对齐
		}
		else if (size <= (1 * 1024)) {
			return _RoundUp(size, 16); // 要申请 1024byte 之内的内存，就以 16byte 对齐
		}
		else if (size <= (8 * 1024)) {
			return _RoundUp(size, 128); // 要申请（8*1024）byte之内的内存，就以 128byte 对齐
		}
		else if (size <= (64 * 1024)) {
			return _RoundUp(size, 1024); // 要申请（64*1024）byte之内的内存，就以 1024byte 对齐
		}

		return -1;
	}

	/////////////////////////////////////////////////

	// [9-16] + 7 = [16-23]
	// [9-16]对应到下标是1的位置；

	static size_t _ListIndex(size_t size, size_t align_shift)      //  -----------------------------------------------------算属于第几个自由链表  
	{

		//       size是区间的左端          align_shift是次数   
		// 	如果，size按照8对齐，而align_shift就是 2^3 中的 次数3		
		//	如果，size按照16对齐，而align_shift就是 2^4 中的 次数4
		//	如果，size按照32对齐，而align_shift就是 2^5 中的 次数5
		//  eg:(( 9   + (1 <<     3      ) - 1) >>       3    ) - 1   
		return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
		// [9-16]
		//        9                3       - 1  >>      3       - 1
		//        9           8            - 1  >>      3       - 1
		//              9 + 7                   >>      3       - 1
		//                16                    /       8       - 1
		//                                2                     - 1  
		//                                            1   下标
	}


	//                                          范围内的字节数/对齐数
	// [1,128]          2^3=8byte对齐                (128-1)/8=15           15+1=16         freelist[0,16)    
	// [129,1024]        2^4=16byte对齐             (1024-128)/16=56        56+16=72        freelist[16,72)        
	// [1025,8*1024]      2^7=128byte对齐        (8*1024-1024)/128=56       56+72=128       freelist[72,128)     
	// [8*1024+1,64*1024]  2^10=1024byte对齐  (64*1024-8*1024)/1024=56      56+128=184      freelist[128,184) 

	// 如果全部以8字节对齐，那么：1、前面浪费很多，后面浪费很少；2、哈希映射的自由链表很长，八千多个；

	// 总结：控制之后，内碎片问题在11%左右，自由链表的长度也有限，184条，更均匀 更集中 更科学


	// 算属于第几个自由链表
	static size_t ListIndex(size_t size)
	{
		assert(size <= MAX_SIZE);

		static int group_array[4] = { 16, 56, 56, 56 };  // 表示每一个范围能划分几个对应长度的区域，就是能挂几个自由链表
		if (size <= 128) {
			return _ListIndex(size, 3);
		}
		else if (size <= (1 * 1024)) {   // (size-128)表示在128之前的已经算过了，128之后的要以16对齐

			return _ListIndex(size - 128, 4) + group_array[0];  // 后面加的数组元素，表示在那段区间中，你是第几个自由链表
		}
		else if (size <= (8 * 1024)) {
			return _ListIndex(size - (1 * 1024), 7) + group_array[0] + group_array[1];
		}
		else if (size <= (64 * 1024)) {
			return _ListIndex(size - (8 * 1024), 10) + group_array[0] + group_array[1] + group_array[2];
		}

		return -1;
	}


	// [2, 512] 之间
	static size_t NumMoveSize(size_t size)  // -----------------------------算的是thread cache向central cache申请，一次申请多少个
	{
		if (size == 0)
			return 0;

		int num = MAX_SIZE / size;
		if (num < 2)   // 最少一次取两个
			num = 2;

		if (num > 512)  // 上限是512，也可以随便改成1024 ，参数调优的问题
			num = 512;  // 512保证了：如果是8b，就正好是1页4k (4*1024/8)
		return num;
	}

	static size_t NumMovePage(size_t size)     // -----------------------------算的是page cache分配内存时的单位大小
	{
		size_t num = NumMoveSize(size);// 针对某一个size，到底需要挪多少个页
		size_t npage = num * size;  // 512*8=4096，是1页

		npage >>= 12;      // 4096/4k=1，是1页
		if (npage == 0)
			npage = 1;

		return npage;  // 最少要返回1页的span
	}


	// **********************************************************************************************************************



};



// span 跨度  管理页为单位的内存对象，本质是方便做合并，解决内存碎片
// 2^32 / 2^12 == 2^20
// 2^64 / 2^12 == 2^52

// 针对windows  条件编译
#ifdef _WIN32
typedef unsigned int PAGE_ID;  // 没有负数，用unsigned
#else
typedef unsigned long long PAGE_ID;
#endif 

struct Span  // 一个span就是对应的一个或多个页切出来的所有块
{                    //       32位             2^20
	PAGE_ID _pageid = 0; // 页号  64位虚拟地址空间 2^52 ，int型表示不下
	PAGE_ID _pagesize = 0;   // 页的数量

	FreeList _freeList; // 对象自由链表
	int _usecount = 0;  // 内存块对象使用计数

	size_t _objSize = 0;  // 对象大小

	Span* _next = nullptr;   // 双向链表
	Span* _prev = nullptr;

};


class SpanList  //双向带头循环链表  
{
public:
	SpanList()  // 构造函数
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}

	void PushFront(Span* newspan)
	{
		Insert(_head->_next, newspan);
	}

	void PopFront()
	{
		Erase(_head->_next);
	}

	void PushBack(Span* newspan)
	{
		Insert(_head, newspan);
	}

	void PopBack()
	{
		Erase(_head->_prev);
	}


	void Insert(Span* pos, Span* newspan)  // central cache中插入一个span
	{
		// 这个newspan不是我们自己new出来的，是从page cache中给你的，
		// 你只是把它插入到central cache的结构中去而已

		Span* prev = pos->_prev;

		// prev newspan pos
		prev->_next = newspan;
		newspan->_next = pos;
		pos->_prev = newspan;
		newspan->_prev = prev;
	}

	void Erase(Span* pos)    // central cache中删除一个span
	{
		// 删除一个span，不是把span delete掉，而是把它还给page cache，
		// 因为它是从page cache中申请的

		assert(pos != _head); // 不能把头删了

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;

		// 不能delete，因为要最后回收时合并。
	}

	bool Empty()
	{
		return Begin() == End();
	}

	void Lock()
	{
		_mtx.lock();
	}

	void Unlock()
	{
		_mtx.unlock();
	}



private:
	Span* _head; // 头指针
	std::mutex _mtx;


};


// 向系统申请num_page页内存挂到自由链表
inline static void* SystemAlloc(size_t num_page)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, num_page*(1 << PAGE_SHIFT),
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);   // VirtualAlloc 是以页为单位申请内存。
#else
	// Linux下：brk mmap等
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();    // 申请失败，抛异常，不是正常的返回。

	return ptr;
}



inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);  // 直接还给系统
#else
#endif
}

