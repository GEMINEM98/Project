#define _CRT_SECURE_NO_WARNINGS 1

#include"ThreadCache.h"
#include"CentralCache.h"

void* ThreadCache::Allocte(size_t size) // ���뼸���ֽڵ��ڴ�
{
	size_t index = SizeClass::ListIndex(size); // ����Ҫ���ڴ���

	FreeList& freeList = _freeLists[index];    // �Զ����������Ĭ�ϵĳ�Ա�������г�ʼ��

	if (!freeList.Empty())
	{
		return freeList.Pop();// ����Ϊ�գ���Ŀ�������з��ص�һ�����

	}
	else // ���Ŀ������Ϊ�գ���ô����ȥ��Ŀ���������һ����������ֱ�ӣ����ϲ㻺������
	{
		return FetchFromCentralCache(SizeClass::RoundUp(size)); // ��CentralCache�л�ȡ�������ڴ�
	   // CentralCache������Դ�����
	   // ����Ҫ���ڴ�飬��ʣ�µĶ���������������
	}
}

void ThreadCache::Deallocte(void* ptr, size_t size) //�ͷſռ䣬�ҵ�����������
{
	size_t index = SizeClass::ListIndex(size); // ������Ҫ���ڴ���

	FreeList& freeList = _freeLists[index];    // �Զ����������Ĭ�ϵĳ�Ա�������г�ʼ��

	freeList.Push(ptr);   // ��Ŀ�����ͷ�嵽Ŀ��������

	// �����������һ������ | �ڴ��С
	size_t num = SizeClass::NumMoveSize(size);
	if (freeList.Num() >= num) //����������ڴ泬��һ������������ڴ棬���ͷ�һ���������ڴ档
	{
		// ������������������Ľ��̫���ˣ���ô�Ͱ��ڴ滹���ڴ�أ��ӿ����£�
		ListTooLong(freeList, num, size);
	}
}

// ������������ж��󳬹�һ�����Ⱦ�Ҫ�ͷŸ�central cache
void ThreadCache::ListTooLong(FreeList& freeList, size_t num, size_t size)
{
	void *start = nullptr;
	void *end = nullptr;
	freeList.PopRange(start, end, num);  // ��������һ������ڴ��

	NextObj(end) = nullptr;
	CentralCache::GetInsatnce().ReleaseListToSpans(start, size);// ��startָ����ڴ���ͷŸ�central cache
}

void* ThreadCache::FetchFromCentralCache(size_t size)   // ������cache�л�ȡ�ڴ�
{
	size_t num = SizeClass::NumMoveSize(size); // thread cache��central cacheҪ���ڴ��С����NumMoveSize��������central cache�����ڴ�ʱ����ĵ�λ

	void* start = nullptr, *end = nullptr;
	// ������cache�л�ȡ�ڴ�
	size_t actualNum = CentralCache::GetInsatnce().FetchRangeObj(start, end, num, size); //centralCacheInst��һ����central cache.h�ļ��ж���ľ�̬����
	// num������Ҫ���ٸ���actualNum��ʵ�ʸ�����ٸ�

	// ����ʧ�ܵĻ��������쳣�����ٻ�ȡ1������
	if (actualNum == 1) // ����start �� end ָ�����һ��
	{
		return start;
	}
	else // ��ȡ�˶�����󣬽�����Ķ���ҵ�thread cache��
	{
		size_t index = SizeClass::ListIndex(size); // ��size����ǹҵ��ĸ�����������£�
		FreeList& list = _freeLists[index]; // ȡ����������thread cache����������Ҫ�������¹Ҷ�����ڴ����
		list.PushRange(NextObj(start), end, actualNum - 1); // start����һ������--->end��֮��Ķ���Ķ��󶼹�����

		return start;
	}
}
