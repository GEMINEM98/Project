#pragma once

#include"ThreadCache.h"
#include"PageCache.h"


static void* ConcurrentMalloc(size_t size)   // ------申请
{

	// 下面申请内存，大小分三个等级：thread cache / page cache / 系统

	if (size <= MAX_SIZE)// 如果小于等于64KB  [1Byte, 64KB]
	{
		if (pThreadCache == nullptr)  // pThreadCache是每个线程独有的
		{
			pThreadCache = new ThreadCache;
			//cout << std::this_thread::get_id() << "->" << pThreadCache << endl;

		}
		return pThreadCache->Allocte(size);
	}
	else if (size <= ((MAX_PAGES - 1) << PAGE_SHIFT))  // (64KB, 128*4KB]
	{
		size_t align_size = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT); // 对齐的size
		size_t pagenum = (align_size >> PAGE_SHIFT);   // 要多少页
		Span* span = PageCache::GetInstance().NewSpan(pagenum);   // 在这个区间，找page cache要，此时有锁
		span->_objSize = align_size;    // 申请的大小就是对齐的大小
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT); // 通过页号算出起始地址，返回起始地址
		return ptr;
	}
	else   // [128*4KB, ~]
	{
		size_t align_size = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		size_t pagenum = (align_size >> PAGE_SHIFT);
		return SystemAlloc(pagenum);
	}
}

static void ConcurrentFree(void* ptr)     // ------释放
{
	size_t pageid = (PAGE_ID)ptr >> PAGE_SHIFT;  // 通过指针算页号
	Span* span = PageCache::GetInstance().GetIdToSpan(pageid); // 通过页号算span，通过span可以算单个对象的大小

	//找系统申请的话，这个span可能为空。
	if (span == nullptr)  // [128*4KB, ~]
	{
		SystemFree(ptr);
		return;
	}

	size_t size = span->_objSize;  // 通过span算单个对象的大小

	// 下面释放内存，大小分三个等级：
	if (size <= MAX_SIZE)// 如果小于等于64KB  [1Byte, 64KB]
	{
		pThreadCache->Deallocte(ptr, size);
	}
	else if (size <= ((MAX_PAGES - 1) << PAGE_SHIFT))  // (64KB, 128*4KB] 释放给page cache
	{
		PageCache::GetInstance().ReleaseSpanToPageCache(span);
	}
}