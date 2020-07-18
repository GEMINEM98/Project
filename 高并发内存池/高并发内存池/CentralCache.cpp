#define _CRT_SECURE_NO_WARNINGS 1

#include"CentralCache.h"
#include "PageCache.h"

Span* CentralCache::GetOneSpan(size_t size)   // ���span�еĻ���ֱ�Ӹ��㣻û�о���page cacheҪ��
{
	size_t index = SizeClass::ListIndex(size);  // �����ĸ�spanlist
	SpanList& spanlist = _spanLists[index];  // ��central cache���ҵ���Ӧ����һ��_spanLists[index]��Ȼ�����������µ�����span������û�в��յ�span��û�о���page cache����
	Span* it = spanlist.Begin();   // ������
	while (it != spanlist.End())
	{
		if (!it->_freeList.Empty())
		{
			return it;
		}
		else
		{
			it = it->_next;    // ����Ҳ�Ϊ�յ�_freeList
		}
	}

	// ��page cache ��ȡһ��span
	size_t numpage = SizeClass::NumMovePage(size);   // ����Ҫ��ҳ��span
	Span* span = PageCache::GetInstance().NewSpan(numpage);


	//��Ϊ�кò��ܹҽ�ȥ���������У�
	// ��span�����гɶ�Ӧ��С���ҵ�span��freelist��

	char *start = (char *)(span->_pageid << 12);  // <<12 == id*4k������ʼ��ַ(ֽ�������ڴ���������ͼ)
	// Ϊʲô��char*����Ϊ�������Ǻ����1�����Ǽ�1�ֽ�
	char *end = start + (span->_pagesize << 12);
	while (start < end)
	{
		char *obj = start;
		start += size;

		span->_freeList.Push(obj);
	}
	span->_objSize = size; // �����������Ĵ�С
	spanlist.PushFront(span);   // ������Ĳ��ֹҽ�ȥspanlist��


	return span;
}

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t size) // thread cache��central cacheҪ���ٶ���
																					  //start�Ǹ���һ�ζ����ͷָ�룬end��βָ��
{
	size_t index = SizeClass::ListIndex(size);
	SpanList& spanlist = _spanLists[index];
	spanlist.Lock();

	Span* span = GetOneSpan(size); //��ȡһ��span�����ֿ��ܣ���span�������ȡ��������page cache���룻��������Ͳ�֪��span��û�б��У�������GetOneSpan���С�

	FreeList& freelist = span->_freeList;   // ֻҪ�ǲ�ͬ��span���Ͳ���Ҫ�������Ǵ�ģ��㲻�ܱ�֤ÿ���̶߳��ǲ�ͬ��span��
											// ����Ҫ��GetOneSpan�������

	size_t actualNum = freelist.PopRange(start, end, num); // ���thread cache��central cacheҪ���ٶ���
	span->_usecount += actualNum; // central cache��space�ķ����thread cache��ҳ���������ںϲ�span��


	spanlist.Unlock();

	return actualNum;
}


// ��һ�������Ķ����ͷŵ�span���
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	//ͨ��size��ȡ����һ��spanlist��������spanlist�мӵġ�
	size_t index = SizeClass::ListIndex(size); // ����ͨ��index��ȡsize
	SpanList& spanlist = _spanLists[index];  // ͨ��index�õ�span

	spanlist.Lock();                         // �õ�span�ͼ���

	while (start)
	{
		// Ҫע�⣬�ж������ĸ�span����ַ��ҳ�ţ�

		void *next = NextObj(start);

		// ��ַ��ҳ�ţ�
		PAGE_ID id = (PAGE_ID)start >> PAGE_SHIFT; // ֻ�����Ͳ�����λ��

		Span* span = PageCache::GetInstance().GetIdToSpan(id);  // �����ж�spanΪ��Ϊ�գ���Ϊ������ڶ���û�ж�Ӧ��span�������Ǵ�span���г����ġ�

		span->_freeList.Push(start); //����һ��ͷ��
		span->_usecount--;
		if (span->_usecount == 0)  // ��ʾ��ǰspan�г�ȥ�Ķ���ȫ�����أ����Խ�span����page cache�����кϲ��������ڴ���Ƭ���⡣
		{
			// �����ĸ�spanList
			size_t index = SizeClass::ListIndex(span->_objSize);
			_spanLists[index].Erase(span); // ��central cache��ȫ��������span����page cache

			span->_freeList.Clear();    // ����һ�£����_freeList�Ѿ�û�����ˣ������������ô����֮��page cache�����span����һЩ�ڴ�(_freeList�Ľ��)��
										// Ȼ���ֱ��г�ȥ����ôcentral cache�ж�Ӧ��span�е�_freeList�л��Ǻ�page cache�е�һ������ô��Щ�ڴ�ͱ��������Ρ�

			PageCache::GetInstance().ReleaseSpanToPageCache(span); // �ͷŸ�page cache ������page cache������
		}
		start = next;
	}

	spanlist.Unlock();      // ����
}
