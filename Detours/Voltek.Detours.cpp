#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>

#include "Detours/detours/Detours.h"
#include "include/Voltek.Detours.h"

namespace voltek
{
	VOLTEK_DETOURS_API uintptr_t find_pattern(uintptr_t start_address, uintptr_t max_size, const char* mask)
	{
		std::vector<std::pair<uint8_t, bool>> pattern;

		for (size_t i = 0; i < strlen(mask);) 
		{
			if (mask[i] != '?') 
			{
				pattern.emplace_back((uint8_t)strtoul(&mask[i], nullptr, 16), false);
				i += 3;
			}
			else 
			{
				pattern.emplace_back(0x00, true);
				i += 2;
			}
		}

		const uint8_t* dataStart = (uint8_t*)start_address;
		const uint8_t* dataEnd = (uint8_t*)start_address + max_size + 1;

		auto ret = std::search(dataStart, dataEnd, pattern.begin(), pattern.end(),
			[](uint8_t CurrentByte, std::pair<uint8_t, bool>& Pattern) {
				return Pattern.second || (CurrentByte == Pattern.first);
			});

		if (ret == dataEnd)
			return 0;

		return std::distance(dataStart, ret) + start_address;
	}

	VOLTEK_DETOURS_API std::vector<uintptr_t> find_patterns(uintptr_t start_address, uintptr_t max_size, const char* mask)
	{
		std::vector<uintptr_t> results;
		std::vector<std::pair<uint8_t, bool>> pattern;

		for (size_t i = 0; i < strlen(mask);) 
		{
			if (mask[i] != '?') 
			{
				pattern.emplace_back((uint8_t)strtoul(&mask[i], nullptr, 16), false);
				i += 3;
			}
			else 
			{
				pattern.emplace_back(0x00, true);
				i += 2;
			}
		}

		const uint8_t* dataStart = (uint8_t*)start_address;
		const uint8_t* dataEnd = (uint8_t*)start_address + max_size + 1;

		for (const uint8_t* i = dataStart;;) 
		{
			auto ret = std::search(i, dataEnd, pattern.begin(), pattern.end(),
				[](uint8_t CurrentByte, std::pair<uint8_t, bool>& Pattern) {
					return Pattern.second || (CurrentByte == Pattern.first);
				});

			if (ret == dataEnd)
				break;

			uintptr_t addr = std::distance(dataStart, ret) + start_address;
			results.push_back(addr);

			i = (uint8_t*)(addr + 1);
		}

		return results;
	}

	VOLTEK_DETOURS_API bool get_pe_section_range(uintptr_t module_base, const char* section, uintptr_t* start, uintptr_t* end)
	{
		PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(module_base + ((PIMAGE_DOS_HEADER)module_base)->e_lfanew);
		PIMAGE_SECTION_HEADER cur_section = IMAGE_FIRST_SECTION(ntHeaders);

		if (!section || strlen(section) <= 0) 
		{
			if (start)
				*start = module_base;

			if (end)
				*end = module_base + ntHeaders->OptionalHeader.SizeOfHeaders;

			return true;
		}

		for (uint32_t i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++, cur_section++) 
		{
			char sectionName[sizeof(IMAGE_SECTION_HEADER::Name) + 1] = { };
			memcpy(sectionName, cur_section->Name, sizeof(IMAGE_SECTION_HEADER::Name));

			if (!strcmp(sectionName, section)) 
			{
				if (start)
					*start = module_base + cur_section->VirtualAddress;

				if (end)
					*end = module_base + cur_section->VirtualAddress + cur_section->Misc.VirtualSize;

				return true;
			}
		}

		return false;
	}

	VOLTEK_DETOURS_API void detours_patch_memory(uintptr_t address, uint8_t* data, size_t size)
	{
		DWORD d = 0;
		VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &d);

		for (uintptr_t i = address; i < (address + size); i++)
			*(volatile uint8_t*)i = *data++;

		VirtualProtect((LPVOID)address, size, d, &d);
		FlushInstructionCache(GetCurrentProcess(), (LPVOID)address, size);
	}

	VOLTEK_DETOURS_API void detours_patch_memory(uintptr_t address, std::initializer_list<uint8_t> data)
	{
		DWORD d = 0;
		VirtualProtect((LPVOID)address, data.size(), PAGE_EXECUTE_READWRITE, &d);

		uintptr_t i = address;
		for (auto value : data)
			*(volatile uint8_t*)i++ = value;

		VirtualProtect((LPVOID)address, data.size(), d, &d);
		FlushInstructionCache(GetCurrentProcess(), (LPVOID)address, data.size());
	}

	VOLTEK_DETOURS_API void detours_patch_memory_nop(uintptr_t address, size_t size)
	{
		DWORD d = 0;
		VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &d);

		for (uintptr_t i = address; i < (address + size); i++)
			*(volatile uint8_t*)i = 0x90;

		VirtualProtect((LPVOID)address, size, d, &d);
		FlushInstructionCache(GetCurrentProcess(), (LPVOID)address, size);
	}

	VOLTEK_DETOURS_API uint32_t detours_unlock_protected(uintptr_t address, size_t size)
	{
		DWORD OldFlag = 0;
		if (VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &OldFlag))
			return OldFlag;
		return 0;
	}

	VOLTEK_DETOURS_API void detours_lock_protected(uintptr_t address, size_t size, uint32_t old_flags)
	{
		DWORD OldFlag = 0;
		VirtualProtect((LPVOID)address, size, old_flags, &OldFlag);
		FlushInstructionCache(GetCurrentProcess(), (LPVOID)address, size);
	}

	VOLTEK_DETOURS_API void detours_patch_memory_unsafe(uintptr_t address, uint8_t* data, size_t size)
	{
		for (uintptr_t i = address; i < (address + size); i++)
			*(volatile uint8_t*)i = *data++;
	}

	VOLTEK_DETOURS_API void detours_patch_memory_unsafe(uintptr_t address, std::initializer_list<uint8_t> data)
	{
		uintptr_t i = address;
		for (auto value : data)
			*(volatile uint8_t*)i++ = value;
	}

	VOLTEK_DETOURS_API void detours_patch_memory_nop_unsafe(uintptr_t address, size_t size)
	{
		for (uintptr_t i = address; i < (address + size); i++)
			*(volatile uint8_t*)i = 0x90;
	}

	VOLTEK_DETOURS_API uintptr_t detours_jump(uintptr_t target, uintptr_t destination)
	{
		return Detours::X64::DetourFunction(target, destination, Detours::X64Option::USE_REL32_JUMP);
	}

	VOLTEK_DETOURS_API uintptr_t detours_call(uintptr_t target, uintptr_t destination)
	{
		return Detours::X64::DetourFunction(target, destination, Detours::X64Option::USE_REL32_CALL);
	}

	VOLTEK_DETOURS_API uintptr_t detours_vtable(uintptr_t target, uintptr_t detour, uint32_t index)
	{
		return Detours::X64::DetourVTable(target, detour, index);
	}

	VOLTEK_DETOURS_API uintptr_t detours_patch_iat(uintptr_t module, const char* import_module, const char* api, uintptr_t detour)
	{
		return Detours::IATHook(module, import_module, api, detour);
	}

	VOLTEK_DETOURS_API uintptr_t detours_patch_iat_delayed(uintptr_t module, const char* import_module, const char* api, uintptr_t detour)
	{
		return Detours::IATDelayedHook(module, import_module, api, detour);
	}
}