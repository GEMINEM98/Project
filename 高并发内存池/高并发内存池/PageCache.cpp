#define _CRT_SECURE_NO_WARNINGS 1

#include "PageCache.h"


Span* PageCache::_NewSpan(size_t numpage)  
{

	//_spanLists[numpage].Lock();  // �����ǵݹ�ʵ�ֵģ��������������Ļ���������ɵݹ���(������)��
								   // ������������һ���������ʵ�֣�����������ߣ��ݹ��ʱ�򣬵ڶ��λ����Ͳ�����μ���


	// ������λ�ò��գ���ֱ��ȡ
	if (!_spanLists[numpage].Empty())
	{
		Span* span = _spanLists[numpage].Begin();
		_spanLists[numpage].PopFront();
		return span;
	}
	// ������λ���ǿգ���ô�Һ����span������û���ڴ棬�����и�
	for (size_t i = numpage + 1; i < MAX_PAGES; ++i)   //MAX_PAGES=129��0��λ�ò��ã�����һ��128��λ��
	{
		if (!_spanLists[i].Empty())    // ���ﶼ����Ҫ�и�ģ��ոպ��ʵ��������if���߹��ˡ�
		{
			// ���ѡ��и��Ҫ���صĲ���+ʣ���λ��span

			Span* span = _spanLists[i].Begin();  //����ԭ��������span
			_spanLists[i].PopFront();

			Span* splitspan = new Span;          // �����к�����span

			
			//�ڷ��ѵ�ʱ��ע�⣬������Ǵ�ǰ����и��ôʣ�ಿ��splitspan��ӳ���ϵȫ����Ҫ�ı䣻������ǴӺ���ǰ�и��ôֻ��Ҫ�ı�������Ҫ���ֵ�ӳ���ϵ�Ϳ��ԡ�
			
			//  �Ӻ���ǰ�и
	
			splitspan->_pageid = span->_pageid + span->_pagesize - numpage; // ���ѳ�����ҳ��
			splitspan->_pagesize = numpage;  // ���ѳ����ļ�ҳ��С
			for (PAGE_ID i = 0; i < numpage; ++i)  // ���ѳ����ļ�ҳ���½���ӳ���ϵ
			{
				_idSpanMap[splitspan->_pageid + i] = splitspan;
			}

			span->_pagesize -= numpage;
			_spanLists[span->_pagesize].PushFront(span);// �Ѷ����span��λ��splitspan->_pagesize�Ǽ����͹��ڼ�ҳ��span�±ߡ�

			return splitspan;

		}
	}


	// ������涼û�з��أ�����ϵͳ���룺
	void* ptr = SystemAlloc(MAX_PAGES - 1);

	Span* bigspan = new Span; // �����ö���� ObjectPool<Span> -> spanpool.New();
	bigspan->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT;   // �Ƚ���ת�����ͣ�������
	bigspan->_pagesize = MAX_PAGES - 1;

	// ��ʱ����ӳ���ϵ
	for (PAGE_ID i = 0; i < bigspan->_pagesize; ++i)
	{
		// _idSpanMap.insert(std::make_pair(bigspan->_pageid+i, bigspan)); 
		_idSpanMap[bigspan->_pageid + i] = bigspan;
	}

	_spanLists[bigspan->_pagesize].PushFront(bigspan);


	// �����Ѿ���������ڴ�ҽ�SpanList�У����ԣ��ݹ�ִ������Ĳ��裬�ָ����Ҫ��С���ڴ档
	return _NewSpan(numpage);   

}

Span* PageCache::NewSpan(size_t numpage)  // ��Ҫ����
{
	_mtx.lock();
	Span* span = _NewSpan(numpage);

	_mtx.unlock();

	return span;

}


// ��һ�������Ķ����ͷŵ�span���
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// ҳ����ǰ�ϲ�
	while (1)
	{
		PAGE_ID prevPageId = span->_pageid - 1; // ǰһҳ��ҳ��-1
		auto pit = _idSpanMap.find(prevPageId); // ֻҪ��map�У����������ȥ���ڴ�

		// ǰһҳû�ڣ��Ͳ�������ǰ���ˣ����ܺϲ�
		if (pit == _idSpanMap.end())
		{
			break;
		}

		// ˵��ǰһ��ҳ����ʹ���У����ܺϲ�
		Span* prevSpan = pit->second;
		if (prevSpan->_usecount != 0)
		{
			break;
		}

		// �ϲ�������ǰһҳ�������ҳ�ϲ�
		if (span->_pagesize + prevSpan->_pagesize >= MAX_PAGES)
		{
			break;
		}

		span->_pageid = prevSpan->_pageid;
		span->_pagesize += prevSpan->_pagesize;

		//������ӳ��
		for (PAGE_ID i = 0; i < prevSpan->_pagesize; ++i)
		{
			_idSpanMap[prevSpan->_pageid + i] = span;
		}

		_spanLists[prevSpan->_pagesize].Erase(prevSpan);  // ��ֹ����Ұָ�������

		delete prevSpan;

	}


	// ҳ�����ϲ�

	while (1)
	{
		PAGE_ID nextPageId = span->_pageid + span->_pagesize;
		auto nextIt = _idSpanMap.find(nextPageId);
		// ��һҳ���ڣ�ֱ�ӷ���
		if (nextIt == _idSpanMap.end())
		{
			break;
		}

		Span* nextSpan = nextIt->second;
		// ��һҳ����ʹ���У����ܺϲ�
		if (nextSpan->_usecount != 0)
		{
			break;
		}

		// �ϲ���������ҳ�������ҳ�ϲ�
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

Span* PageCache::GetIdToSpan(PAGE_ID id)  // �����id���Ҹ����Ӧ��span
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
