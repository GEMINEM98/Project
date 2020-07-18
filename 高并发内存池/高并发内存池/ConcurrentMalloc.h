#pragma once

#include"ThreadCache.h"
#include"PageCache.h"


static void* ConcurrentMalloc(size_t size)   // ------����
{

	// ���������ڴ棬��С�������ȼ���thread cache / page cache / ϵͳ

	if (size <= MAX_SIZE)// ���С�ڵ���64KB  [1Byte, 64KB]
	{
		if (pThreadCache == nullptr)  // pThreadCache��ÿ���̶߳��е�
		{
			pThreadCache = new ThreadCache;
			//cout << std::this_thread::get_id() << "->" << pThreadCache << endl;

		}
		return pThreadCache->Allocte(size);
	}
	else if (size <= ((MAX_PAGES - 1) << PAGE_SHIFT))  // (64KB, 128*4KB]
	{
		size_t align_size = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT); // �����size
		size_t pagenum = (align_size >> PAGE_SHIFT);   // Ҫ����ҳ
		Span* span = PageCache::GetInstance().NewSpan(pagenum);   // ��������䣬��page cacheҪ����ʱ����
		span->_objSize = align_size;    // ����Ĵ�С���Ƕ���Ĵ�С
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT); // ͨ��ҳ�������ʼ��ַ��������ʼ��ַ
		return ptr;
	}
	else   // [128*4KB, ~]
	{
		size_t align_size = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		size_t pagenum = (align_size >> PAGE_SHIFT);
		return SystemAlloc(pagenum);
	}
}

static void ConcurrentFree(void* ptr)     // ------�ͷ�
{
	size_t pageid = (PAGE_ID)ptr >> PAGE_SHIFT;  // ͨ��ָ����ҳ��
	Span* span = PageCache::GetInstance().GetIdToSpan(pageid); // ͨ��ҳ����span��ͨ��span�����㵥������Ĵ�С

	//��ϵͳ����Ļ������span����Ϊ�ա�
	if (span == nullptr)  // [128*4KB, ~]
	{
		SystemFree(ptr);
		return;
	}

	size_t size = span->_objSize;  // ͨ��span�㵥������Ĵ�С

	// �����ͷ��ڴ棬��С�������ȼ���
	if (size <= MAX_SIZE)// ���С�ڵ���64KB  [1Byte, 64KB]
	{
		pThreadCache->Deallocte(ptr, size);
	}
	else if (size <= ((MAX_PAGES - 1) << PAGE_SHIFT))  // (64KB, 128*4KB] �ͷŸ�page cache
	{
		PageCache::GetInstance().ReleaseSpanToPageCache(span);
	}
}