#pragma once

#include "Voltek.Detours.Config.h"

#include <stdint.h>

#include <vector>
#include <initializer_list>

namespace voltek
{
	VOLTEK_DETOURS_API uintptr_t find_pattern(uintptr_t start_address, uintptr_t max_size, const char* mask);
	VOLTEK_DETOURS_API std::vector<uintptr_t> find_patterns(uintptr_t start_address, uintptr_t max_size, const char* mask);

	VOLTEK_DETOURS_API bool get_pe_section_range(uintptr_t module_base, const char* section, uintptr_t* start, uintptr_t* end);

	VOLTEK_DETOURS_API void detours_patch_memory(uintptr_t address, uint8_t* data, size_t size);
	VOLTEK_DETOURS_API void detours_patch_memory(uintptr_t address, std::initializer_list<uint8_t> data);
	VOLTEK_DETOURS_API void detours_patch_memory_nop(uintptr_t address, size_t size);

	VOLTEK_DETOURS_API uint32_t detours_unlock_protected(uintptr_t address, size_t size);
	VOLTEK_DETOURS_API void detours_lock_protected(uintptr_t address, size_t size, uint32_t old_flags);

	// _unsafe functions are called between detours_unlock_protected and detours_lock_protected functions

	VOLTEK_DETOURS_API void detours_patch_memory_unsafe(uintptr_t address, uint8_t* data, size_t size);
	VOLTEK_DETOURS_API void detours_patch_memory_unsafe(uintptr_t address, std::initializer_list<uint8_t> data);
	VOLTEK_DETOURS_API void detours_patch_memory_nop_unsafe(uintptr_t address, size_t size);

	class protect_guard
	{
	public:
		protect_guard(uintptr_t address, size_t size) noexcept : _begin(0), _size(0), _old_flags(0)
		{
			_old_flags = detours_unlock_protected(address, size);
			if (_old_flags != 0)
			{
				_begin = address;
				_size = size;
			}
		}

		~protect_guard() noexcept
		{
			if (_old_flags)
			{
				detours_lock_protected(_begin, _size, _old_flags);

				_begin = 0;
				_size = 0;
				_old_flags = 0;
			}
		}
	private:
		uintptr_t _begin;
		size_t _size;
		uint32_t _old_flags;
	};

	VOLTEK_DETOURS_API uintptr_t detours_jump(uintptr_t target, uintptr_t destination);
	VOLTEK_DETOURS_API uintptr_t detours_call(uintptr_t target, uintptr_t destination);
	VOLTEK_DETOURS_API uintptr_t detours_vtable(uintptr_t target, uintptr_t detour, uint32_t index);

	template<typename T>
	uintptr_t detours_function_class_jump(uintptr_t target, T detour)
	{
		return detours_jump(target, *(uintptr_t*)&detour);
	}

	template<typename T>
	uintptr_t detours_function_class_call(uintptr_t target, T detour)
	{
		return detours_call(target, *(uintptr_t*)&detour);
	}

	template<typename T>
	uintptr_t detours_function_class_vtable(uintptr_t target, T detour, uint32_t index)
	{
		return detours_vtable(target, *(uintptr_t*)&detour, index);
	}

	VOLTEK_DETOURS_API uintptr_t detours_patch_iat(uintptr_t module, const char* import_module, const char* api, uintptr_t detour);
	VOLTEK_DETOURS_API uintptr_t detours_patch_iat_delayed(uintptr_t module, const char* import_module, const char* api, uintptr_t detour);
}