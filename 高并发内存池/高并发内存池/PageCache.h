#pragma once
#include "Common.h"


class PageCache    // ��ҳΪ��λ��
{
public:
	// ������/ϵͳ����һ��span
	Span* _NewSpan(size_t numpage);
	Span* NewSpan(size_t numpage);

	// ��һ�������Ķ����ͷŵ�span���
	void ReleaseSpanToPageCache(Span* span);

	// �����id���Ҹ����Ӧ��span
	Span* GetIdToSpan(PAGE_ID id);

	static PageCache& GetInstance() // ����ģʽ���������첻���ã����Լ�&
	{
		static PageCache inst;
		return inst;
	}


private:

	PageCache()  // ���캯��˽�л�
	{ }
	PageCache(const PageCache&) = delete; // ���˴������˶���


	SpanList _spanLists[MAX_PAGES];
	std::unordered_map<PAGE_ID, Span*> _idSpanMap; //ӳ�䣬���ںϲ��ڴ�

	std::mutex _mtx;
};
