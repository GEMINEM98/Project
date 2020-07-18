#define _CRT_SECURE_NO_WARNINGS 1

#include"CentralCache.h"
#include "PageCache.h"

Span* CentralCache::GetOneSpan(size_t size)   // 如果span有的话，直接给你；没有就向page cache要。
{
	size_t index = SizeClass::ListIndex(size);  // 算是哪个spanlist
	SpanList& spanlist = _spanLists[index];  // 在central cache中找到对应的那一个_spanLists[index]，然后在这个结点下的所有span中找有没有不空的span，没有就向page cache申请
	Span* it = spanlist.Begin();   // 迭代器
	while (it != spanlist.End())
	{
		if (!it->_freeList.Empty())
		{
			return it;
		}
		else
		{
			it = it->_next;    // 向后找不为空的_freeList
		}
	}

	// 从page cache 获取一个span
	size_t numpage = SizeClass::NumMovePage(size);   // 算你要几页的span
	Span* span = PageCache::GetInstance().NewSpan(numpage);


	//因为切好才能挂进去，所以先切：
	// 把span对象切成对应大小，挂到span的freelist中

	char *start = (char *)(span->_pageid << 12);  // <<12 == id*4k就是起始地址(纸上申请内存的最下面的图)
	// 为什么是char*，因为方便我们后面加1，就是加1字节
	char *end = start + (span->_pagesize << 12);
	while (start < end)
	{
		char *obj = start;
		start += size;

		span->_freeList.Push(obj);
	}
	span->_objSize = size; // 自由链表对象的大小
	spanlist.PushFront(span);   // 将多余的部分挂进去spanlist中


	return span;
}

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t size) // thread cache向central cache要多少对象，
																					  //start是给你一段对象的头指针，end是尾指针
{
	size_t index = SizeClass::ListIndex(size);
	SpanList& spanlist = _spanLists[index];
	spanlist.Lock();

	Span* span = GetOneSpan(size); //获取一个span，两种可能：从span链表里获取，或者向page cache申请；所以下面就不知道span有没有被切，所以在GetOneSpan中切。

	FreeList& freelist = span->_freeList;   // 只要是不同的span，就不需要锁，这是错的，你不能保证每个线程都是不同的span。
											// 必须要在GetOneSpan后加锁！

	size_t actualNum = freelist.PopRange(start, end, num); // 算出thread cache向central cache要多少对象
	span->_usecount += actualNum; // central cache中space的分配给thread cache的页数量，用于合并span。


	spanlist.Unlock();

	return actualNum;
}


// 将一定数量的对象释放到span跨度
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	//通过size获取到哪一个spanlist，锁是在spanlist中加的。
	size_t index = SizeClass::ListIndex(size); // 首先通过index获取size
	SpanList& spanlist = _spanLists[index];  // 通过index拿到span

	spanlist.Lock();                         // 拿到span就加锁

	while (start)
	{
		// 要注意，判断属于哪个span：地址算页号；

		void *next = NextObj(start);

		// 地址算页号：
		PAGE_ID id = (PAGE_ID)start >> PAGE_SHIFT; // 只有整型才能移位。

		Span* span = PageCache::GetInstance().GetIdToSpan(id);  // 不用判断span为不为空，因为不会存在对象没有对应的span，对象都是从span中切出来的。

		span->_freeList.Push(start); //这是一个头插
		span->_usecount--;
		if (span->_usecount == 0)  // 表示当前span切出去的对象全部返回，可以将span还给page cache，进行合并，减少内存碎片问题。
		{
			// 算是哪个spanList
			size_t index = SizeClass::ListIndex(span->_objSize);
			_spanLists[index].Erase(span); // 把central cache中全部回来的span还给page cache

			span->_freeList.Clear();    // 清理一下，这个_freeList已经没有用了；如果不清理，那么还了之后，page cache中这个span挂了一些内存(_freeList的结点)，
										// 然后又被切出去，那么central cache中对应的span中的_freeList中还是和page cache中的一样，那么这些内存就被挂了两次。

			PageCache::GetInstance().ReleaseSpanToPageCache(span); // 释放给page cache ，所以page cache来调用
		}
		start = next;
	}

	spanlist.Unlock();      // 解锁
}
