#define _CRT_SECURE_NO_WARNINGS 1

#include"ThreadCache.h"
#include<vector>

void UnitThreadCache1()  // 测试申请内存是否正确
{
	ThreadCache tc; // 定义一个thread cache对象
	std::vector<void*> v;
	for (size_t i = 0; i < 21; ++i)  // 申请了21次内存，放到vector中，方便释放
	{                               
		v.push_back(tc.Allocte(7));
	}

	for (size_t i = 0; i < v.size(); ++i)
	{
		printf("[%d]->%p\n", i, v[i]);
	}

	for (auto ptr : v)
	{
		tc.Deallocte(ptr, 7);
	}
}

void UnitTestSizeClass()  // 测试申请内存大小对齐数 和 内存块在自由链表的下标
{
	//                                          范围内的字节数/对齐数
	// [1,128]          2^3=8byte对齐                (128-1)/8=15           15+1=16         freelist[0,16)    
	// [129,1024]        2^4=16byte对齐             (1024-128)/16=56        56+16=72        freelist[16,72)        
	// [1025,8*1024]      2^7=128byte对齐        (8*1024-1024)/128=56       56+72=128       freelist[72,128)     
	// [8*1024+1,64*1024]  2^10=1024byte对齐  (64*1024-8*1024)/1024=56      56+128=184      freelist[128,184) 

	cout << SizeClass::RoundUp(1) << endl; //8
	cout << SizeClass::RoundUp(127) << endl; //128
	cout << endl;

	cout << SizeClass::RoundUp(129) << endl; //144
	cout << SizeClass::RoundUp(1023) << endl;//1024
	cout << endl;

	cout << SizeClass::RoundUp(1025) << endl;//1152
	cout << SizeClass::RoundUp(8 * 1024 - 1) << endl;//8192
	cout << endl;

	cout << SizeClass::RoundUp(8 * 1024 + 1) << endl;//9216
	cout << SizeClass::RoundUp(64 * 1024 - 1) << endl;//65536
	cout << endl;

	cout << SizeClass::ListIndex(1) << endl; //0
	cout << SizeClass::ListIndex(128) << endl; //15
	cout << endl;

	cout << SizeClass::ListIndex(129) << endl; //16
	cout << SizeClass::ListIndex(1023) << endl;//71
	cout << endl;

	cout << SizeClass::ListIndex(1025) << endl;//72
	cout << SizeClass::ListIndex(8 * 1024 - 1) << endl;//127
	cout << endl;

	cout << SizeClass::ListIndex(8 * 1024 + 1) << endl;//128
	cout << SizeClass::ListIndex(64 * 1024 - 1) << endl;//183
	cout << endl;

}

void UnitTestSystemAlloc()
{
	void* ptr1 = SystemAlloc(1);  // 页的起始地址
	void* ptr2 = SystemAlloc(1);

	PAGE_ID id1 = (PAGE_ID)ptr1 >> PAGE_SHIFT; // 地址算页号：/4K
	PAGE_ID id2 = (PAGE_ID)ptr2 >> PAGE_SHIFT; // 地址算页号：/4K

	void* ptrshift1 = (void*)(id1 << PAGE_SHIFT); // 用页号算指针：*4K
	void* ptrshift2 = (void*)(id2 << PAGE_SHIFT); // 用页号算指针：*4K

	char* obj1 = (char*)ptr1;
	char* obj2 = (char*)ptr1 + 8;
	char* obj3 = (char*)ptr1 + 16;
	PAGE_ID idd1 = (PAGE_ID)obj1 >> PAGE_SHIFT;  // 知道页内地址，用地址 / 4k = 页号
	PAGE_ID idd2 = (PAGE_ID)obj2 >> PAGE_SHIFT;
	PAGE_ID idd3 = (PAGE_ID)obj3 >> PAGE_SHIFT;
}

void UnitThreadCache2()
{
	/*ThreadCache tc;
	void* ptr1 = tc.Allocte(6);
	void* ptr2 = tc.Allocte(6);
	void* ptr3 = tc.Allocte(6);*/


	ThreadCache tc; // 定义一个thread cache对象
	std::vector<void*> v;
	size_t size = 7;
	for (size_t i = 0; i < SizeClass::NumMoveSize(size); ++i)
	{                               
		v.push_back(tc.Allocte(size));
	}

	v.push_back(tc.Allocte(size));

	for (size_t i = 0; i < v.size(); ++i)
	{
		printf("[%d]->%p\n", i, v[i]);
	}

	for (auto ptr : v)
	{
		tc.Deallocte(ptr, 7);
	}

	v.clear();

	v.push_back(tc.Allocte(size));

}


#include"ConcurrentMalloc.h"
void func1()
{
	std::vector<void*> v;
	size_t size = 7;
	for (size_t i = 0; i < 512; ++i) 
	{
		v.push_back(ConcurrentMalloc(size));
	}
	v.push_back(ConcurrentMalloc(size));


	for (size_t i = 0; i < v.size(); ++i)
	{
		printf("[%d]->%p\n", i, v[i]);
	}

	for (auto ptr : v)
	{
		ConcurrentFree(ptr);
	}

	v.clear();
}

void func2()
{
	std::vector<void*> v;
	size_t size = 7;
	for (size_t i = 0; i < SizeClass::NumMoveSize(size) + 1; ++i)  
	{
		v.push_back(ConcurrentMalloc(size));
	}

	for (size_t i = 0; i < v.size(); ++i)
	{
		printf("[%d]->%p\n", i, v[i]);
	}

	for (auto ptr : v)
	{
		ConcurrentFree(ptr);
	}

	v.clear();
}


//int main()
//{
//	//UnitThreadCache1();
//	//UnitTestSizeClass();
//	//UnitTestSystemAlloc();
//	//UnitThreadCache2();
//	//UnitThreadCache();
//
//	func1();
//
//	 
//
//	system("pause");
//	return 0;
//}
