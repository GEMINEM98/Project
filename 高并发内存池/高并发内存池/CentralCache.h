#pragma once

#include"Common.h"

class CentralCache  // ���뻺��
{
public:

	// �����Ļ����ȡһ�������Ķ����thread cache
	// ��thread cache�Լ������ң�����Ҫ���ٶ���
	// start��end���������ã���Ϊ����ı��ˣ�����Ϳ����õ��������
	size_t FetchRangeObj(void*& start, void*& end, size_t num, size_t size);

	// ��һ�������Ķ����ͷŵ�span���
	void ReleaseListToSpans(void* start, size_t size);

	// ��spanlist ���� page cache��ȡһ��span
	Span* GetOneSpan(size_t size);

	// ����ģʽ   ����ģʽ
	static CentralCache& GetInsatnce()
	{
		static CentralCache inst;
		return inst;
	}


private:

	CentralCache()
	{}

	CentralCache(const CentralCache&) = delete;

	SpanList _spanLists[NFREE_LIST];  //���Ļ�����������

};
