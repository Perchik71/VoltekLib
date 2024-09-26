// MemoryManagerTest.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <chrono>
#include <iostream>

#pragma comment(lib, "..\\x64\\Release\\VoltekLib.MemoryManager.lib")
#include "..\\Memory Manager\\include\\Voltek.MemoryManager.h"

template<typename T>
class guard_ptr
{
public:
	guard_ptr(size_t size) : ptr(nullptr) { ptr = (T*)voltek::scalable_alloc(size); }
	~guard_ptr() { if(ptr) voltek::scalable_free(ptr); }

	bool empty() const { return !ptr; }
	size_t size() const { return voltek::scalable_msize(ptr); }
	const T* get() const { return ptr; }
private:
	guard_ptr(const guard_ptr& ptr) = default;

	T* ptr;
};

void test_02(size_t block_size)
{
	size_t MAX = 1000;
	double common = 0.0;
	char** test_arch = new char* [MAX];
	size_t bsize = block_size;
	std::chrono::duration<double, std::milli> ms_double;

	double a_times[10];
	double d_times[10];

	for (size_t j = 0; j < 10; j++)
	{
		auto t1 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
		{
			test_arch[i] = (char*)voltek::scalable_alloc(bsize);
		}

		auto t2 = std::chrono::high_resolution_clock::now();
		ms_double = t2 - t1;
		a_times[j] = ms_double.count();

		auto t3 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
		{
			if (test_arch[i])
				voltek::scalable_free(test_arch[i]);
		}

		auto t4 = std::chrono::high_resolution_clock::now();
		ms_double = t4 - t3;
		d_times[j] = ms_double.count();
	}

	for (size_t j = 0; j < 10; j++)
		common += a_times[j];
	common /= 10.0f;

	std::cout << "voltek allocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	for (size_t j = 0; j < 10; j++)
		common += d_times[j];
	common /= 10.0f;

	std::cout << "voltek deallocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	common = 0.0;
}

void test_01(size_t block_size)
{
	size_t MAX = 100000;
	double common = 0.0;
	char** test_arch = new char* [MAX];
	size_t bsize = block_size;
	std::chrono::duration<double, std::milli> ms_double;

	double a_times[10];
	double d_times[10];

	for (size_t j = 0; j < 10; j++)
	{
		auto t1 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
		{
			test_arch[i] = (char*)malloc(bsize);
		}

		auto t2 = std::chrono::high_resolution_clock::now();
		ms_double = t2 - t1;
		a_times[j] = ms_double.count();

		auto t3 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
		{
			if (test_arch[i])
				free(test_arch[i]);
		}

		auto t4 = std::chrono::high_resolution_clock::now();
		ms_double = t4 - t3;
		d_times[j] = ms_double.count();
	}

	for (size_t j = 0; j < 10; j++)
		common += a_times[j];
	common /= 10.0f;

	std::cout << "default allocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	for (size_t j = 0; j < 10; j++)
		common += d_times[j];
	common /= 10.0f;

	std::cout << "default deallocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	common = 0.0;

	for (size_t j = 0; j < 10; j++)
	{
		auto t1 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
		{
			test_arch[i] = (char*)_aligned_malloc(bsize, 0x10);
		}

		auto t2 = std::chrono::high_resolution_clock::now();
		ms_double = t2 - t1;
		a_times[j] = ms_double.count();

		auto t3 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
		{
			if (test_arch[i])
				_aligned_free(test_arch[i]);
		}

		auto t4 = std::chrono::high_resolution_clock::now();
		ms_double = t4 - t3;
		d_times[j] = ms_double.count();
	}

	for (size_t j = 0; j < 10; j++)
		common += a_times[j];
	common /= 10.0f;

	std::cout << "default alligned allocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	for (size_t j = 0; j < 10; j++)
		common += d_times[j];
	common /= 10.0f;

	std::cout << "default alligned deallocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	common = 0.0;

	for (size_t j = 0; j < 10; j++)
	{
		auto t1 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
		{
			test_arch[i] = (char*)voltek::scalable_alloc(bsize);
		}

		auto t2 = std::chrono::high_resolution_clock::now();
		ms_double = t2 - t1;
		a_times[j] = ms_double.count();

		auto t3 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
		{
			if (test_arch[i])
				voltek::scalable_free(test_arch[i]);
		}

		auto t4 = std::chrono::high_resolution_clock::now();
		ms_double = t4 - t3;
		d_times[j] = ms_double.count();
	}

	for (size_t j = 0; j < 10; j++)
		common += a_times[j];
	common /= 10.0f;

	std::cout << "voltek allocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	for (size_t j = 0; j < 10; j++)
		common += d_times[j];
	common /= 10.0f;

	std::cout << "voltek deallocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	delete[] test_arch;
}

#define ARRAYSIZE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
  static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

int main()
{
	{
		guard_ptr<void> ptr((size_t)(4) * 1024 * 1024 * 1024);
		std::cout << "Alloc ptr more 4GB [" << (!ptr.empty() ? "SUCCESS" : "FAILED") << "]\n";
		if (!ptr.empty()) std::cout << "Size ptr [" << ptr.size() << "] " << std::hex << uintptr_t(ptr.get()) << std::dec << "\n";
	}

	{
		guard_ptr<void> ptr(((size_t)(4) * 1024 * 1024 * 1024) - 1);
		std::cout << "Alloc ptr more 4GB-1 [" << (!ptr.empty() ? "SUCCESS" : "FAILED") << "]\n";
		if (!ptr.empty()) std::cout << "Size ptr [" << ptr.size() << "] " << std::hex << uintptr_t(ptr.get()) << std::dec << "\n";
	}

	std::cout << "\n\n";
	std::cout << "===============================\n\n";
	test_02(16384);

	std::cout << "\n\n";
	system("pause");

	size_t sizes[] = { 8, 16, 32, 64, 128, /*256, 512, 1024, 4096, 8192, 16384, 32768,*/ 65536 };

	for (size_t i = 0; i < ARRAYSIZE(sizes); i++)
	{
		std::cout << "===============================\n\n";
		test_01(sizes[i]);
	}

	system("pause");
}