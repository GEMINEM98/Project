#pragma once

#include"Common.h"

class ThreadCache   //�̳߳�
{
public:

	//�����ڴ���ͷ��ڴ�
	void* Allocte(size_t size); // ���ڴ棬���ڴ�ؿ��ռ�
	void Deallocte(void* ptr, size_t size); //�ͷſռ䣬�ŵ�����������

	//�����Ļ����ȡ������ΪҪ������һ��ȡ�������ڴ棬�Ա��´β��ü�����ֱ��������������ȡ
	void* FetchFromCentralCache(size_t index);

	// ������������ж��󳬹�һ�����Ⱦ�Ҫ�ͷŸ�central cache
	void ListTooLong(FreeList& freeList, size_t num, size_t size);


private:

	FreeList _freeLists[NFREE_LIST]; //�������������

};

//�߳�TLS(�̱߳��ش洢): ������������һ������ִ�е�ָ���̹߳���������
//����ʵ�֣������Ҫ��һ���߳��ڲ��ĸ����������ö��ܷ��ʣ��������̲߳��ܷ��ʵı���������ҪTLS��
//������һ��ȫ�ֱ�ͨ����ǰ�߳�IDȥ��ѯ��Ӧ�����ݣ���Ϊ�����̵߳�id��ͬ���鵽��������ȻҲ��ͬ��
//TLS�ֶ�̬�;�̬��Windows����TlsAlloc������

//�þ�̬�ģ����Ӽ򵥣�
//static ThreadCache* pThreadCache = nullptr; ���Ƕ�����һ��ȫ�ֱ���������һ����������������̵߳ġ�
//ǰ����� _declspec (thread) ����ʾÿ���̶߳����

#ifdef _WIN32
_declspec (thread) static ThreadCache* pThreadCache = nullptr;  // ÿ���̶߳����Լ���TLS�������Ϳ��������Ļ�ȡ�Լ���thread cache
#else
//linux tls
#endif

