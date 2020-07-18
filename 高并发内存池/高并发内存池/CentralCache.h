#pragma once

#include"Common.h"

class CentralCache  // 中央缓冲
{
public:

	// 从中心缓存获取一定数量的对象给thread cache
	// 是thread cache自己告诉我，它需要多少对象
	// start和end传的是引用，因为里面改变了，外面就可以拿到这个东西
	size_t FetchRangeObj(void*& start, void*& end, size_t num, size_t size);

	// 将一定数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t size);

	// 从spanlist 或者 page cache获取一个span
	Span* GetOneSpan(size_t size);

	// 单例模式   饿汉模式
	static CentralCache& GetInsatnce()
	{
		static CentralCache inst;
		return inst;
	}


private:

	CentralCache()
	{}

	CentralCache(const CentralCache&) = delete;

	SpanList _spanLists[NFREE_LIST];  //中心缓存自由链表

};
