// Copyright © 2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: MIT

#pragma once

#include "Voltek.RelocationDatabase.Config.h"

#include <stdio.h>
#include <stdint.h>

namespace voltek
{
	struct reldb_signature
	{
		uint32_t rva;
		char* pattern;				// You need to free up your memory yourself
		uint32_t pattern_len;
	};

	typedef void (*reldb_stream_msg_event)(const char* msg);
	typedef void* (*reldb_allocate_func)(size_t size);
	typedef void (*reldb_deallocate_func)(void* pointer);
	
	struct read_reldb_item_stream
	{
		FILE* stream;
		uint32_t version;
		char name[60];
		uint32_t sign_total;
		reldb_signature* signs;
		reldb_stream_msg_event msg_handler;
		reldb_allocate_func allocate_handler;
		reldb_deallocate_func deallocate_handler;
	};

	VOLTEK_RELDB_API bool load_reldb_item_stream(read_reldb_item_stream* stream);
	VOLTEK_RELDB_API bool save_reldb_item_stream(read_reldb_item_stream* stream);
	VOLTEK_RELDB_API bool load_reldb_item_stream_dev(read_reldb_item_stream* stream);
	VOLTEK_RELDB_API bool save_reldb_item_stream_dev(read_reldb_item_stream* stream);

	struct read_reldb_stream
	{
		FILE* stream;
		uint32_t patch_total;
		reldb_stream_msg_event msg_handler;
	};

	VOLTEK_RELDB_API bool create_reldb_stream(read_reldb_stream* stream);
	VOLTEK_RELDB_API bool open_reldb_stream(read_reldb_stream* stream);
	VOLTEK_RELDB_API bool update_reldb_stream(read_reldb_stream* stream);
}