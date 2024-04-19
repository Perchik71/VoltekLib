// Copyright © 2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: MIT

#pragma once

#include "Voltek.RelocationDatabase.Config.h"

#include <stdint.h>

namespace voltek
{
	struct reldb_stream;
	struct reldb_patch;
	struct reldb_signature;
	
	typedef void* (*reldb_allocate_func)(size_t size);
	typedef void (*reldb_deallocate_func)(void* pointer);

	VOLTEK_RELDB_API const char* __stdcall reldb_get_error_text(long err);

	VOLTEK_RELDB_API long __stdcall reldb_new_db(reldb_stream** stm, const char* fname,
		reldb_allocate_func allocate_handler = nullptr, reldb_deallocate_func deallocate_handler = nullptr);
	VOLTEK_RELDB_API long __stdcall reldb_open_db(reldb_stream** stm, const char* fname,
		reldb_allocate_func allocate_handler = nullptr, reldb_deallocate_func deallocate_handler = nullptr);
	VOLTEK_RELDB_API long __stdcall reldb_clear_db(reldb_stream* stm);
	VOLTEK_RELDB_API long __stdcall reldb_save_db(reldb_stream* stm);
	VOLTEK_RELDB_API long __stdcall reldb_release_db(reldb_stream* stm);

	VOLTEK_RELDB_API long __stdcall reldb_count_patches(reldb_stream* stm);
	VOLTEK_RELDB_API long __stdcall reldb_get_patch_by_id(reldb_stream* stm, reldb_patch** pch, uint32_t index);
	VOLTEK_RELDB_API long __stdcall reldb_get_patch_by_name(reldb_stream* stm, reldb_patch** pch, const char* name);
	VOLTEK_RELDB_API bool __stdcall reldb_has_patch(reldb_stream* stm, const char* name);

	VOLTEK_RELDB_API long __stdcall reldb_open_dev_file_patch(reldb_stream* stm, reldb_patch** pch, const char* fname);
	VOLTEK_RELDB_API long __stdcall reldb_save_dev_file_patch(reldb_stream* stm, reldb_patch* pch, const char* fname);

	VOLTEK_RELDB_API long __stdcall reldb_add_patch(reldb_stream* stm, reldb_patch* pch, bool free_tmp_patch = true);
	VOLTEK_RELDB_API long __stdcall reldb_update_patch(reldb_stream* stm, reldb_patch* pch, bool free_tmp_patch = true);
	VOLTEK_RELDB_API long __stdcall reldb_remove_patch_by_id(reldb_stream* stm, uint32_t index);
	VOLTEK_RELDB_API long __stdcall reldb_remove_patch(reldb_stream* stm, const char* name);

	VOLTEK_RELDB_API long __stdcall reldb_get_version_patch(reldb_patch* pch);
	VOLTEK_RELDB_API long __stdcall reldb_get_name_patch(reldb_patch* pch, char* name, uint32_t maxsize);
	VOLTEK_RELDB_API long __stdcall reldb_count_signatures_in_patch(reldb_patch* pch);	
	VOLTEK_RELDB_API long __stdcall reldb_get_signature_patch(reldb_patch* pch, reldb_signature** sign, uint32_t index);
	VOLTEK_RELDB_API long __stdcall reldb_add_signature_to_patch(reldb_patch* pch, uint32_t rva, const char* pattern);
	VOLTEK_RELDB_API long __stdcall reldb_remove_signature_from_patch(reldb_patch* pch, uint32_t index);
	VOLTEK_RELDB_API long __stdcall reldb_clear_signatures_in_patch(reldb_patch* pch);

	VOLTEK_RELDB_API uint32_t __stdcall reldb_get_rva_from_signature(reldb_signature* sign);
	VOLTEK_RELDB_API long __stdcall reldb_get_pattern_from_signature(reldb_signature* sign, char* name, uint32_t maxsize);
	VOLTEK_RELDB_API uint32_t __stdcall reldb_get_pattern_length_from_signature(reldb_signature* sign);
}