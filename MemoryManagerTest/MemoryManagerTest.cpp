// MemoryManagerTest.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <chrono>
#include <iostream>
#include <forward_list>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

//#include "rpmalloc.h"
#include "..\\Memory Manager\include\Voltek.MemoryManager.h"

//#pragma comment(lib, "rpmalloc.lib")
#pragma comment(lib, "..\\x64\\Release\\VoltekLib.MemoryManager.lib")

#define NOMINMAX
#include <Windows.h>

class TMemoryManager;
class TGarbageCollector
{
public:
	struct DATA
	{
		std::thread* gc_thread{ nullptr };
		std::atomic_bool gc_terminated{ false };
		std::forward_list<void*> list_garbage{};
		std::mutex locker{};
	};
	friend class TMemoryManager;
private:
	DATA* data{ nullptr };
	
	TGarbageCollector(TGarbageCollector&&) = delete;
	TGarbageCollector(const TGarbageCollector&) = delete;
	TGarbageCollector operator=(TGarbageCollector&&) = delete;
	TGarbageCollector operator=(const TGarbageCollector&) = delete;

	static void TryFreeBlock(void* a_block);
public:
	TGarbageCollector();
	~TGarbageCollector();
};

void TGarbageCollector::TryFreeBlock(void* a_block)
{
	__try
	{
		if (a_block)
		{
			//std::cout << "sdfsdfsdf " << (uintptr_t)a_block << "\n";
			voltek::scalable_free(a_block);
		}
	}
	__except (1)
	{}
}

TGarbageCollector::TGarbageCollector()
{
	data = new DATA;
	if (data)
	{
		data->gc_thread = new std::thread([](DATA* a_data) {
			using namespace std::chrono_literals;

			std::forward_list<void*> list_for_free{};

			while (!(a_data->gc_terminated.load()))
			{
				// for locker
				{
					std::lock_guard guard(a_data->locker);
					list_for_free.assign(a_data->list_garbage.begin(), a_data->list_garbage.end());
					a_data->list_garbage.clear();
				}
	
				for (auto item : list_for_free)
					TryFreeBlock(item);

				list_for_free.clear();
				std::this_thread::sleep_for(500ms);
			}

			delete a_data->gc_thread;
			delete a_data;
			}, data);
		if (data->gc_thread)
			data->gc_thread->detach();
	}
}

TGarbageCollector::~TGarbageCollector()
{
	if (data)
		data->gc_terminated = true;
}

class IMemoryManager
{
	IMemoryManager(IMemoryManager&&) = delete;
	IMemoryManager(const IMemoryManager&) = delete;
	IMemoryManager operator=(IMemoryManager&&) = delete;
	IMemoryManager operator=(const IMemoryManager&) = delete;
public:
	IMemoryManager() noexcept = default;
	virtual ~IMemoryManager() noexcept = default;

	[[nodiscard]] virtual void* alloc(size_t a_size) noexcept = 0;
	[[nodiscard]] virtual void* aligned_alloc(size_t a_alignment, size_t a_size) noexcept = 0;
	[[nodiscard]] virtual void* realloc(void* a_block, size_t a_size) noexcept = 0;
	[[nodiscard]] virtual void* aligned_realloc(void* a_block, size_t a_alignment, size_t a_size) noexcept = 0;
	virtual void dealloc(void* a_block, bool a_align = false) noexcept = 0;
	[[nodiscard]] virtual size_t size(void* a_block) const noexcept = 0;
	[[nodiscard]] virtual const char* name() const noexcept = 0;
};

class TFatherMemoryManager :
	public IMemoryManager
{
	using FuncAlloc = void* (uint32_t);
	using FuncDealloc = void (void*);

	std::function<FuncAlloc> Malloc{};
	std::function<FuncDealloc> MFree{};
	std::function<FuncAlloc> MallocAligned{};
	std::function<FuncDealloc> MFreeAligned{};

	HMODULE hDll;
public:
	TFatherMemoryManager() noexcept
	{
		hDll = LoadLibraryA("Malloc.dll");
		if (!hDll)
			std::cout << "Fatal load library \"Malloc.dll\"" << std::endl;
		else
		{
			Malloc = reinterpret_cast<FuncAlloc*>(GetProcAddress(hDll, "MAlloc"));
			MFree = reinterpret_cast<FuncDealloc*>(GetProcAddress(hDll, "MFree"));
			MallocAligned = reinterpret_cast<FuncAlloc*>(GetProcAddress(hDll, "MAllocAligned"));
			MFreeAligned = reinterpret_cast<FuncDealloc*>(GetProcAddress(hDll, "MFreeAligned"));
		}
	}

	virtual ~TFatherMemoryManager() noexcept
	{
		if (hDll)
		{
			FreeLibrary(hDll);
			hDll = nullptr;
		}
	}

	[[nodiscard]] void* alloc(size_t a_size) noexcept override
	{
		return Malloc(static_cast<uint32_t>(a_size));
	}

	[[nodiscard]] void* aligned_alloc([[maybe_unused]] size_t a_alignment, size_t a_size) noexcept override
	{
		return MallocAligned(static_cast<uint32_t>(a_size));
	}

	[[nodiscard]] void* realloc([[maybe_unused]] void* a_block, [[maybe_unused]] size_t a_size) noexcept override
	{
		return nullptr;
	}

	[[nodiscard]] void* aligned_realloc([[maybe_unused]] void* a_block, [[maybe_unused]] size_t a_alignment,
		[[maybe_unused]] size_t a_size) noexcept override
	{
		return nullptr;
	}

	void dealloc(void* a_block, bool a_align) noexcept override
	{
		if (a_align)
			MFreeAligned(a_block);
		else
			MFree(a_block);
	}

	[[nodiscard]] size_t size([[maybe_unused]] void* a_block) const noexcept override
	{
		return 0;
	}

	[[nodiscard]] const char* name() const noexcept override
	{
		return "Father";
	}
};

class TVoltekMemoryManager :
	public IMemoryManager
{
public:
	TVoltekMemoryManager()
	{
		voltek::scalable_memory_manager_initialize();
	}

	~TVoltekMemoryManager()
	{
		voltek::scalable_memory_manager_shutdown();
	}

	[[nodiscard]] void* alloc(size_t a_size) noexcept override
	{
		return voltek::scalable_alloc(a_size);
	}

	[[nodiscard]] void* aligned_alloc([[maybe_unused]] size_t a_alignment, size_t a_size) noexcept override
	{
		return voltek::scalable_alloc(a_size);
	}

	[[nodiscard]] void* realloc(void* a_block, size_t a_size) noexcept override
	{
		if (!a_size)
		{
			dealloc(a_block);
			return nullptr;
		}

		void* newPtr = alloc(a_size);

		if (a_block)
		{
			memcpy(newPtr, a_block, std::min(size(a_block), a_size));
			dealloc(a_block);
		}

		return newPtr;
	}

	[[nodiscard]] void* aligned_realloc(void* a_block, size_t a_alignment, size_t a_size) noexcept override
	{
		if (!a_size)
		{
			dealloc(a_block, true);
			return nullptr;
		}

		void* newPtr = aligned_alloc(a_alignment, a_size);

		if (a_block)
		{
			memcpy(newPtr, a_block, std::min(size(a_block), a_size));
			dealloc(a_block, true);
		}

		return newPtr;
	}

	void dealloc(void* a_block, [[maybe_unused]] bool a_align = false) noexcept override
	{
		voltek::scalable_free(a_block);
	}

	[[nodiscard]] size_t size(void* a_block) const noexcept override
	{
		return voltek::scalable_msize(a_block);
	}

	[[nodiscard]] const char* name() const noexcept override
	{
		return "Voltek";
	}
};

static TFatherMemoryManager f_memmgr{};
static TVoltekMemoryManager v_memmgr{};


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

static void test_03(IMemoryManager& mmgr, size_t block_size, size_t count_total)
{
	size_t MAX = count_total / 10;
	double common = 0.0;
	auto test_arch = new char* [MAX];
	size_t bsize = block_size;
	std::chrono::duration<double, std::milli> ms_double;

	double a_times[10]{};
	double d_times[10]{};

	for (size_t j = 0; j < 10; j++)
	{
		auto t1 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
			test_arch[i] = (char*)mmgr.alloc(bsize);

		auto t2 = std::chrono::high_resolution_clock::now();
		ms_double = t2 - t1;
		a_times[j] = ms_double.count();

		auto t3 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
			if (test_arch[i])
				mmgr.dealloc(test_arch[i]);

		auto t4 = std::chrono::high_resolution_clock::now();
		ms_double = t4 - t3;
		d_times[j] = ms_double.count();
	}

	for (size_t j = 0; j < 10; j++)
		common += a_times[j];
	common /= 10.0f;

	std::cout << mmgr.name() << " allocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	for (size_t j = 0; j < 10; j++)
		common += d_times[j];
	common /= 10.0f;

	std::cout << mmgr.name() << " deallocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	////////////////////////////////////////

	for (size_t j = 0; j < 10; j++)
	{
		auto t1 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
			test_arch[i] = (char*)mmgr.aligned_alloc(0x10, bsize);

		auto t2 = std::chrono::high_resolution_clock::now();
		ms_double = t2 - t1;
		a_times[j] = ms_double.count();

		auto t3 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
			if (test_arch[i])
				mmgr.dealloc(test_arch[i], true);

		auto t4 = std::chrono::high_resolution_clock::now();
		ms_double = t4 - t3;
		d_times[j] = ms_double.count();
	}

	for (size_t j = 0; j < 10; j++)
		common += a_times[j];
	common /= 10.0f;

	std::cout << mmgr.name() << " allocate align " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	for (size_t j = 0; j < 10; j++)
		common += d_times[j];
	common /= 10.0f;

	std::cout << mmgr.name() << " deallocate align " << bsize << " bytes\n";
	std::cout << common << "ms\n\n\n";

	common = 0.0;
}

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

	std::cout << "rp allocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	for (size_t j = 0; j < 10; j++)
		common += d_times[j];
	common /= 10.0f;

	std::cout << "rp deallocate " << bsize << " bytes\n";
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

	std::cout << "rp alligned allocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	for (size_t j = 0; j < 10; j++)
		common += d_times[j];
	common /= 10.0f;

	std::cout << "rp alligned deallocate " << bsize << " bytes\n";
	std::cout << common << "ms\n";

	common = 0.0;

	for (size_t j = 0; j < 10; j++)
	{
		auto t1 = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < MAX; i++)
		{
			test_arch[i] = (char*)voltek::scalable_alloc(bsize);
			strcpy_s(test_arch[i], bsize, "hi");
			*test_arch[i] = '\xC';
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
	constexpr int a = 1024;
	constexpr int aaa = 64 * 1024 * 10;

	test_03(f_memmgr, a, aaa);
	test_03(v_memmgr, a, aaa);


	//{
	//	guard_ptr<void> ptr((size_t)(4) * 1024 * 1024 * 1024);
	//	std::cout << "Alloc ptr more 4GB [" << (!ptr.empty() ? "SUCCESS" : "FAILED") << "]\n";
	//	if (!ptr.empty()) std::cout << "Size ptr [" << ptr.size() << "] " << std::hex << uintptr_t(ptr.get()) << std::dec << "\n";
	//}

	//{
	//	guard_ptr<void> ptr(((size_t)(4) * 1024 * 1024 * 1024) - 1);
	//	std::cout << "Alloc ptr more 4GB-1 [" << (!ptr.empty() ? "SUCCESS" : "FAILED") << "]\n";
	//	if (!ptr.empty()) std::cout << "Size ptr [" << ptr.size() << "] " << std::hex << uintptr_t(ptr.get()) << std::dec << "\n";
	//}

	//std::cout << "\n\n";
	//std::cout << "===============================\n\n";
	//test_02(16384);

	//std::cout << "\n\n";
	//system("pause");

	//size_t sizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024, 4096, 8192,/* 16384, 32768, 65536 */ };

	//for (size_t i = 0; i < ARRAYSIZE(sizes); i++)
	//{
	//	std::cout << "===============================\n\n";
	//	test_01(sizes[i]);
	//}

	system("pause");
}