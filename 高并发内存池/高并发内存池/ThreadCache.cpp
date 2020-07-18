#define _CRT_SECURE_NO_WARNINGS 1

#include"ThreadCache.h"
#include"CentralCache.h"

void* ThreadCache::Allocte(size_t size) // 申请几个字节的内存
{
	size_t index = SizeClass::ListIndex(size); // 将需要的内存规格化

	FreeList& freeList = _freeLists[index];    // 自定义的数组会调默认的成员函数进行初始化

	if (!freeList.Empty())
	{
		return freeList.Pop();// 链表不为空，从目标链表中返回第一个结点

	}
	else // 如果目标链表为空，那么不会去找目标链表的下一个链表，而是直接，向上层缓存申请
	{
		return FetchFromCentralCache(SizeClass::RoundUp(size)); // 从CentralCache中获取对齐后的内存
	   // CentralCache是做资源均衡的
	   // 拿走要的内存块，把剩下的都挂在自由链表下
	}
}

void ThreadCache::Deallocte(void* ptr, size_t size) //释放空间，挂到自由链表里
{
	size_t index = SizeClass::ListIndex(size); // 将你需要的内存规格化

	FreeList& freeList = _freeLists[index];    // 自定义的数组会调默认的成员函数进行初始化

	freeList.Push(ptr);   // 将目标对象头插到目标链表中

	// 对象个数满足一定条件 | 内存大小
	size_t num = SizeClass::NumMoveSize(size);
	if (freeList.Num() >= num) //如果回来的内存超过一次批量申请的内存，就释放一个批量的内存。
	{
		// 如果这个自由链表里面的结点太多了，那么就把内存还给内存池，接口如下：
		ListTooLong(freeList, num, size);
	}
}

// 如果自由链表中对象超过一定长度就要释放给central cache
void ThreadCache::ListTooLong(FreeList& freeList, size_t num, size_t size)
{
	void *start = nullptr;
	void *end = nullptr;
	freeList.PopRange(start, end, num);  // 拿走了这一区间的内存块

	NextObj(end) = nullptr;
	CentralCache::GetInsatnce().ReleaseListToSpans(start, size);// 将start指向的内存块释放给central cache
}

void* ThreadCache::FetchFromCentralCache(size_t size)   // 从中心cache中获取内存
{
	size_t num = SizeClass::NumMoveSize(size); // thread cache向central cache要的内存大小经过NumMoveSize，给的是central cache分配内存时分配的单位

	void* start = nullptr, *end = nullptr;
	// 从中心cache中获取内存
	size_t actualNum = CentralCache::GetInsatnce().FetchRangeObj(start, end, num, size); //centralCacheInst是一个在central cache.h文件中定义的静态对象
	// num是你想要多少个，actualNum是实际给你多少个

	// 上面失败的话，会抛异常；至少获取1个对象
	if (actualNum == 1) // 返回start 和 end 指向的那一个
	{
		return start;
	}
	else // 获取了多个对象，将多余的对象挂到thread cache下
	{
		size_t index = SizeClass::ListIndex(size); // 用size算的是挂到哪个自由链表低下；
		FreeList& list = _freeLists[index]; // 取别名，这是thread cache的自由链表，要往它底下挂多余的内存对象
		list.PushRange(NextObj(start), end, actualNum - 1); // start的下一个对象--->end，之间的多余的对象都挂起来

		return start;
	}
}
