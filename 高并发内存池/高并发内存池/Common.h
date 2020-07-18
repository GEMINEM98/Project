#pragma once

// �ڴ�й©�ᵼ����Щҳ�ϲ���ȥ����������������������������������������������������������������������

#include<iostream>
#include<stdlib.h>
#include<assert.h>
#include<map>
#include<unordered_map>
#include<thread>
#include<mutex>

#ifdef _WIN32
#include<Windows.h>
#endif //_WIN32


//using namespace std;

using std::cout;
using std::endl;


const size_t MAX_SIZE = 64 * 1024;  //  �ڴ�صĴ�С     C++��const��ö�������  
const size_t NFREE_LIST = MAX_SIZE / 8;  // ÿ����������֮��ļ�����൱���ڴ�ؽ��ļ��
const size_t MAX_PAGES = 129;  // ֱ��ӳ��ģ��±���0���ԣ�ʣ�µ�һ��һ������128ҳ=500��kb����ֱ����ϵͳҪ��
const size_t PAGE_SHIFT = 12;  // 4kΪҳ��λ  1<<12 = 2^12 = 4K


inline void*& NextObj(void* obj)  // ��һ�������������ĵģ�Ƶ������ ���������inline
{
	return *((void**)obj);    //ȡ��һ������
}


class FreeList   //����������
{
public:
	void Push(void* obj)  //������������ͷ�����
	{
		// ͷ��
		NextObj(obj) = _freelist;
		_freelist = obj;
		++_num;
	}

	void PushRange(void* head, void* tail, size_t num)  // ���������19���ڴ�鶼push������������
	{
		NextObj(tail) = _freelist;
		_freelist = head;
		_num += num;
	}

	void* Pop()  // �����������ж��������£�ȡһ������
	{
		// ͷɾ
		void* obj = _freelist;
		_freelist = NextObj(obj); // �ѵڶ������ĵ�ַ����Ŀ�������ָ��
		--_num;
		return obj;
	}

	size_t PopRange(void*& start, void*& end, size_t num)  // ����_freelist��һ��
	{
		size_t actualNum = 0;
		void* prev = nullptr;
		void* cur = _freelist;
		for (; actualNum < num && cur != nullptr; ++actualNum) // ��prev��cur�ҳ�Ҫ������һ�ν��Ľ���λ�á�
		{     // ʵ�ʵ�  < ��Ҫ��
			prev = cur;
			cur = NextObj(cur);
		}

		start = _freelist;  // ������һ�ε���ʼ(��һ�����)
		end = prev;         // ������һ�εĽ���(���һ�����)
		_freelist = cur;    // ��_freelistָ��ָ��ʣ�ಿ�ֵ���ʼλ�á�(����_freelistָ��)

		_num -= actualNum;

		return actualNum;
	}

	size_t Num()
	{
		return _num;
	}

	bool Empty()
	{
		return _freelist == nullptr;
	}

	void Clear()
	{
		_freelist = nullptr;
		_num = 0;
	}

private:
	void* _freelist = nullptr; //���ָ�������ԹҲ�ͬ��С�Ķ������ֻ�����������Ƕ��壬û�п��ռ�
	// û����ʾʵ�ֹ��캯������������Ĭ�����ɹ��캯������ȱʡֵ���г�ʼ��

	size_t _num = 0;

};


class SizeClass   // ר�������ݴ�С����
{
public:
	//����static����Ϊû�� ��Ա��������Щ�ӿڶ������װ����


	// �Ż�ǰ��            ***************************************************************************** һ��Ĵ���
	//static size_t ListIndex(size_t size)  // ����������Ķ�Ӧ�ڴ���±�
	//{
	//	if (size % 8 == 0)
	//	{
	//		return size / 8 - 1;
	//	}
	//	else
	//	{
	//		return size / 8;
	//	}
	//}

	//static size_t RoundUp(size_t size)  // ���϶��룬��Ҫ����5b���Ҹ����8b
	//{
	//	if (size % 8 != 0)
	//	{
	//		return (size / 8 + 1) * 8;
	//	}
	//	else
	//	{
	//		return size;
	//	}
	//}


	// �Ż���            ***************************************************************************** �߼��Ĵ���


	// ������[1%��11%]���ҵ�����Ƭ�˷ѣ���һ���Ƚ����⣬�����Ķ����������Χ��
	// 1k = 1024b
	//                                                                 Ҫ128b���ڴ棬�˷�7b��         Ҫ8b���ڴ棬�˷�7b
	// [1,128]          2^3=8byte����         freelist[0,16)    min�˷ѣ�7 / 128 = 5.4687%    max�˷ѣ�7 / (0+8) = 87%  ������˷����ģ�û�취����
	// [129,1024]        2^4=16byte����       freelist[16,72)           15 / 1024 = 1.4648%           15 / (128+16) = 10%
	// [1025,8*1024]      2^7=128byte����     freelist[72,128)         127 / (8*1024)=1.5686%        127 / (1024+128) = 11.0%
	// [8*1024+1,64*1024]  2^10=1024byte����  freelist[128,184)       1023 / (64*1024)=1.5609%      1023 / (8*1024+1024) = 11.1%

	static size_t _RoundUp(size_t size, size_t alignment)  // �����ڴ��С�����϶��룬alignment�Ƕ����������뵽������alignment(8��16��32...)��λ�á���Ҫ����5b���Ҹ����8b
	{
		return (size + alignment - 1) & (~(alignment - 1));  // +  &  ~ �� /  % ��
		// [9-16] + 7 = [16-23] -> ����λ��16 8 4 2 1�У�16λ��Ϊ1�����ϵ���λΪ0,����λΪ1��Ҳ��������~7
		// [17-32] + 15 = [32-47] -> ����λ��32 16 8 4 2 1�У�32λ��Ϊ1�����ϵ���λΪ0,����λΪ1��Ҳ��������~15

	}
	static inline size_t RoundUp(size_t size)  //��thread������central���������ڴ�ʱʹ��-------------------------------�����thread cache�����ڴ�ʱ�ĵ�λ��С
	{                                                                                                                        // �����ڴ���Ƭ

		// ����һ�㷽���ĳ�����ȡģ����ΪЧ�ʵ�

		assert(size <= MAX_SIZE);

		// if�����еķ�Χ��������Ҳ���� 8 �������
		if (size <= 128) {
			return _RoundUp(size, 8); // Ҫ���� 128byte ֮�ڵ��ڴ棬���� 8byte ����
		}
		else if (size <= (1 * 1024)) {
			return _RoundUp(size, 16); // Ҫ���� 1024byte ֮�ڵ��ڴ棬���� 16byte ����
		}
		else if (size <= (8 * 1024)) {
			return _RoundUp(size, 128); // Ҫ���루8*1024��byte֮�ڵ��ڴ棬���� 128byte ����
		}
		else if (size <= (64 * 1024)) {
			return _RoundUp(size, 1024); // Ҫ���루64*1024��byte֮�ڵ��ڴ棬���� 1024byte ����
		}

		return -1;
	}

	/////////////////////////////////////////////////

	// [9-16] + 7 = [16-23]
	// [9-16]��Ӧ���±���1��λ�ã�

	static size_t _ListIndex(size_t size, size_t align_shift)      //  -----------------------------------------------------�����ڵڼ�����������  
	{

		//       size����������          align_shift�Ǵ���   
		// 	�����size����8���룬��align_shift���� 2^3 �е� ����3		
		//	�����size����16���룬��align_shift���� 2^4 �е� ����4
		//	�����size����32���룬��align_shift���� 2^5 �е� ����5
		//  eg:(( 9   + (1 <<     3      ) - 1) >>       3    ) - 1   
		return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
		// [9-16]
		//        9                3       - 1  >>      3       - 1
		//        9           8            - 1  >>      3       - 1
		//              9 + 7                   >>      3       - 1
		//                16                    /       8       - 1
		//                                2                     - 1  
		//                                            1   �±�
	}


	//                                          ��Χ�ڵ��ֽ���/������
	// [1,128]          2^3=8byte����                (128-1)/8=15           15+1=16         freelist[0,16)    
	// [129,1024]        2^4=16byte����             (1024-128)/16=56        56+16=72        freelist[16,72)        
	// [1025,8*1024]      2^7=128byte����        (8*1024-1024)/128=56       56+72=128       freelist[72,128)     
	// [8*1024+1,64*1024]  2^10=1024byte����  (64*1024-8*1024)/1024=56      56+128=184      freelist[128,184) 

	// ���ȫ����8�ֽڶ��룬��ô��1��ǰ���˷Ѻܶ࣬�����˷Ѻ��٣�2����ϣӳ�����������ܳ�����ǧ�����

	// �ܽ᣺����֮������Ƭ������11%���ң���������ĳ���Ҳ���ޣ�184���������� ������ ����ѧ


	// �����ڵڼ�����������
	static size_t ListIndex(size_t size)
	{
		assert(size <= MAX_SIZE);

		static int group_array[4] = { 16, 56, 56, 56 };  // ��ʾÿһ����Χ�ܻ��ּ�����Ӧ���ȵ����򣬾����ܹҼ�����������
		if (size <= 128) {
			return _ListIndex(size, 3);
		}
		else if (size <= (1 * 1024)) {   // (size-128)��ʾ��128֮ǰ���Ѿ�����ˣ�128֮���Ҫ��16����

			return _ListIndex(size - 128, 4) + group_array[0];  // ����ӵ�����Ԫ�أ���ʾ���Ƕ������У����ǵڼ�����������
		}
		else if (size <= (8 * 1024)) {
			return _ListIndex(size - (1 * 1024), 7) + group_array[0] + group_array[1];
		}
		else if (size <= (64 * 1024)) {
			return _ListIndex(size - (8 * 1024), 10) + group_array[0] + group_array[1] + group_array[2];
		}

		return -1;
	}


	// [2, 512] ֮��
	static size_t NumMoveSize(size_t size)  // -----------------------------�����thread cache��central cache���룬һ��������ٸ�
	{
		if (size == 0)
			return 0;

		int num = MAX_SIZE / size;
		if (num < 2)   // ����һ��ȡ����
			num = 2;

		if (num > 512)  // ������512��Ҳ�������ĳ�1024 ���������ŵ�����
			num = 512;  // 512��֤�ˣ������8b����������1ҳ4k (4*1024/8)
		return num;
	}

	static size_t NumMovePage(size_t size)     // -----------------------------�����page cache�����ڴ�ʱ�ĵ�λ��С
	{
		size_t num = NumMoveSize(size);// ���ĳһ��size��������ҪŲ���ٸ�ҳ
		size_t npage = num * size;  // 512*8=4096����1ҳ

		npage >>= 12;      // 4096/4k=1����1ҳ
		if (npage == 0)
			npage = 1;

		return npage;  // ����Ҫ����1ҳ��span
	}


	// **********************************************************************************************************************



};



// span ���  ����ҳΪ��λ���ڴ���󣬱����Ƿ������ϲ�������ڴ���Ƭ
// 2^32 / 2^12 == 2^20
// 2^64 / 2^12 == 2^52

// ���windows  ��������
#ifdef _WIN32
typedef unsigned int PAGE_ID;  // û�и�������unsigned
#else
typedef unsigned long long PAGE_ID;
#endif 

struct Span  // һ��span���Ƕ�Ӧ��һ������ҳ�г��������п�
{                    //       32λ             2^20
	PAGE_ID _pageid = 0; // ҳ��  64λ�����ַ�ռ� 2^52 ��int�ͱ�ʾ����
	PAGE_ID _pagesize = 0;   // ҳ������

	FreeList _freeList; // ������������
	int _usecount = 0;  // �ڴ�����ʹ�ü���

	size_t _objSize = 0;  // �����С

	Span* _next = nullptr;   // ˫������
	Span* _prev = nullptr;

};


class SpanList  //˫���ͷѭ������  
{
public:
	SpanList()  // ���캯��
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}

	void PushFront(Span* newspan)
	{
		Insert(_head->_next, newspan);
	}

	void PopFront()
	{
		Erase(_head->_next);
	}

	void PushBack(Span* newspan)
	{
		Insert(_head, newspan);
	}

	void PopBack()
	{
		Erase(_head->_prev);
	}


	void Insert(Span* pos, Span* newspan)  // central cache�в���һ��span
	{
		// ���newspan���������Լ�new�����ģ��Ǵ�page cache�и���ģ�
		// ��ֻ�ǰ������뵽central cache�Ľṹ��ȥ����

		Span* prev = pos->_prev;

		// prev newspan pos
		prev->_next = newspan;
		newspan->_next = pos;
		pos->_prev = newspan;
		newspan->_prev = prev;
	}

	void Erase(Span* pos)    // central cache��ɾ��һ��span
	{
		// ɾ��һ��span�����ǰ�span delete�������ǰ�������page cache��
		// ��Ϊ���Ǵ�page cache�������

		assert(pos != _head); // ���ܰ�ͷɾ��

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;

		// ����delete����ΪҪ������ʱ�ϲ���
	}

	bool Empty()
	{
		return Begin() == End();
	}

	void Lock()
	{
		_mtx.lock();
	}

	void Unlock()
	{
		_mtx.unlock();
	}



private:
	Span* _head; // ͷָ��
	std::mutex _mtx;


};


// ��ϵͳ����num_pageҳ�ڴ�ҵ���������
inline static void* SystemAlloc(size_t num_page)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, num_page*(1 << PAGE_SHIFT),
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);   // VirtualAlloc ����ҳΪ��λ�����ڴ档
#else
	// Linux�£�brk mmap��
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();    // ����ʧ�ܣ����쳣�����������ķ��ء�

	return ptr;
}



inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);  // ֱ�ӻ���ϵͳ
#else
#endif
}

