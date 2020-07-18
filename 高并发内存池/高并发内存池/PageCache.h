#pragma once
#include "Common.h"


class PageCache    // 以页为单位，
{
public:
	// 从链表/系统返回一个span
	Span* _NewSpan(size_t numpage);
	Span* NewSpan(size_t numpage);

	// 将一定数量的对象释放到span跨度
	void ReleaseSpanToPageCache(Span* span);

	// 你给我id，我给你对应的span
	Span* GetIdToSpan(PAGE_ID id);

	static PageCache& GetInstance() // 饿汉模式，拷贝构造不能用，所以加&
	{
		static PageCache inst;
		return inst;
	}


private:

	PageCache()  // 构造函数私有化
	{ }
	PageCache(const PageCache&) = delete; // 别人创建不了对象


	SpanList _spanLists[MAX_PAGES];
	std::unordered_map<PAGE_ID, Span*> _idSpanMap; //映射，用于合并内存

	std::mutex _mtx;
};
