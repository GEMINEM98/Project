#define _CRT_SECURE_NO_WARNINGS 1

#include "PageCache.h"


Span* PageCache::_NewSpan(size_t numpage)  
{

	//_spanLists[numpage].Lock();  // 下面是递归实现的，如果在这里加锁的话，容易造成递归锁(锁中锁)，
								   // 所以下面用另一个函数配合实现，将锁加在外边，递归的时候，第二次回来就不会二次加锁


	// 如果这个位置不空，就直接取
	if (!_spanLists[numpage].Empty())
	{
		Span* span = _spanLists[numpage].Begin();
		_spanLists[numpage].PopFront();
		return span;
	}
	// 如果这个位置是空，那么找后面的span，看有没有内存，并且切割
	for (size_t i = numpage + 1; i < MAX_PAGES; ++i)   //MAX_PAGES=129，0的位置不用，还是一共128个位置
	{
		if (!_spanLists[i].Empty())    // 这里都是需要切割的，刚刚合适的在上面的if中走过了。
		{
			// 分裂、切割，需要返回的部分+剩余归位的span

			Span* span = _spanLists[i].Begin();  //这是原本的完整span
			_spanLists[i].PopFront();

			Span* splitspan = new Span;          // 这是切后多余的span

			
			//在分裂的时候注意，如果我们从前向后切割，那么剩余部分splitspan的映射关系全部都要改变；如果我们从后向前切割，那么只需要改变我们需要部分的映射关系就可以。
			
			//  从后向前切割：
	
			splitspan->_pageid = span->_pageid + span->_pagesize - numpage; // 分裂出来的页号
			splitspan->_pagesize = numpage;  // 分裂出来的几页大小
			for (PAGE_ID i = 0; i < numpage; ++i)  // 分裂出来的几页重新建立映射关系
			{
				_idSpanMap[splitspan->_pageid + i] = splitspan;
			}

			span->_pagesize -= numpage;
			_spanLists[span->_pagesize].PushFront(span);// 把多余的span归位，splitspan->_pagesize是几，就挂在几页的span下边。

			return splitspan;

		}
	}


	// 如果上面都没有返回，就找系统申请：
	void* ptr = SystemAlloc(MAX_PAGES - 1);

	Span* bigspan = new Span; // 可以用对象池 ObjectPool<Span> -> spanpool.New();
	bigspan->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT;   // 先将其转成整型，再右移
	bigspan->_pagesize = MAX_PAGES - 1;

	// 此时进行映射关系
	for (PAGE_ID i = 0; i < bigspan->_pagesize; ++i)
	{
		// _idSpanMap.insert(std::make_pair(bigspan->_pageid+i, bigspan)); 
		_idSpanMap[bigspan->_pageid + i] = bigspan;
	}

	_spanLists[bigspan->_pagesize].PushFront(bigspan);


	// 上面已经将申请的内存挂进SpanList中，所以，递归执行上面的步骤，分割出想要大小的内存。
	return _NewSpan(numpage);   

}

Span* PageCache::NewSpan(size_t numpage)  // 需要加锁
{
	_mtx.lock();
	Span* span = _NewSpan(numpage);

	_mtx.unlock();

	return span;

}


// 将一定数量的对象释放到span跨度
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// 页的向前合并
	while (1)
	{
		PAGE_ID prevPageId = span->_pageid - 1; // 前一页的页号-1
		auto pit = _idSpanMap.find(prevPageId); // 只要在map中，就是申请出去的内存

		// 前一页没在，就不用了向前找了，不能合并
		if (pit == _idSpanMap.end())
		{
			break;
		}

		// 说明前一个页还在使用中，不能合并
		Span* prevSpan = pit->second;
		if (prevSpan->_usecount != 0)
		{
			break;
		}

		// 合并，保留前一页，后面的页合并
		if (span->_pagesize + prevSpan->_pagesize >= MAX_PAGES)
		{
			break;
		}

		span->_pageid = prevSpan->_pageid;
		span->_pagesize += prevSpan->_pagesize;

		//重新做映射
		for (PAGE_ID i = 0; i < prevSpan->_pagesize; ++i)
		{
			_idSpanMap[prevSpan->_pageid + i] = span;
		}

		_spanLists[prevSpan->_pagesize].Erase(prevSpan);  // 防止出现野指针的问题

		delete prevSpan;

	}


	// 页的向后合并

	while (1)
	{
		PAGE_ID nextPageId = span->_pageid + span->_pagesize;
		auto nextIt = _idSpanMap.find(nextPageId);
		// 后一页不在，直接返回
		if (nextIt == _idSpanMap.end())
		{
			break;
		}

		Span* nextSpan = nextIt->second;
		// 这一页还在使用中，不能合并
		if (nextSpan->_usecount != 0)
		{
			break;
		}

		// 合并，保留本页，后面的页合并
		if (span->_pagesize + nextSpan->_pagesize >= MAX_PAGES)
		{
			break;
		}

		span->_pagesize += nextSpan->_pagesize;
		for (PAGE_ID i = 0; i < nextSpan->_pagesize; ++i)
		{
			_idSpanMap[nextSpan->_pageid + i] = span;
		}

		_spanLists[nextSpan->_pagesize].Erase(nextSpan);

		delete nextSpan;
	}

	_spanLists[span->_pagesize].PushFront(span);

}

Span* PageCache::GetIdToSpan(PAGE_ID id)  // 你给我id，我给你对应的span
{
	//std::map<PAGE_ID, Span*>::iterator it = _idSpanMap.find(id);
	auto it = _idSpanMap.find(id);
	if (it != _idSpanMap.end())
	{
		return it->second;
	}
	else
	{
		return nullptr;
	}
}
