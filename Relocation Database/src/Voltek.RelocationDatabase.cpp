// Copyright © 2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: MIT

#include "..\include\Voltek.RelocationDatabase.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <memory>
#include <vector>
#include <map>

#include <io.h>
#include <errno.h>
#include <string.h>
#include <mmsystem.h>

#define NO_ERR 0
#define ERR_NO_STREAM -1
#define ERR_INCORRECT_FNAME -2
#define ERR_NOACCESS_FILE -3
#define ERR_STDIO_FAILED -4
#define ERR_OUT_OF_MEMORY -5

#define ERR_FILE_NO_DATABASE -6
#define ERR_DATABASE_VERSION_NO_SUPPORTED -7
#define ERR_DATABASE_NO_INFO -8
#define ERR_INCORRECT_CHUNK_SIZE -9
#define ERR_FILE_ID_CORRUPTED -10
#define ERR_PATCH_NO_INFO -11

#define ERR_INDEX_OUT_OF_RANGE -12
#define ERR_INVALID_ARGUMENTS -13
#define ERR_NO_FOUND -14
#define ERR_PATCH_NAME_ALREADY_EXISTS -15
#define ERR_RVA_IN_PATCH_ALREADY_EXISTS -16

#pragma pack(push, 1)

namespace voltek
{
	constexpr static uint32_t RELOCATION_DB_CHUNK_ID = MAKEFOURCC('R', 'E', 'L', 'B');
	constexpr static uint32_t RELOCATION_DB_CHUNK_VERSION = 1;
	constexpr static uint32_t RELOCATION_DB_DATA_CHUNK_ID = MAKEFOURCC('R', 'E', 'L', 'D');
	constexpr static uint32_t RELOCATION_DB_DATA_CHUNK_VERSION = 1;
	constexpr static uint32_t RELOCATION_DB_ITEM_CHUNK_ID = MAKEFOURCC('R', 'L', 'B', 'I');
	constexpr static uint32_t RELOCATION_DB_ITEM_CHUNK_VERSION = 1;
	constexpr static uint32_t RELOCATION_DB_ITEM_DATA_CHUNK_ID = MAKEFOURCC('R', 'L', 'B', 'D');
	constexpr static uint32_t RELOCATION_DB_ITEM_DATA_CHUNK_VERSION = 2;

	constexpr static const char* EXTENDED_FORMAT = "extended";

	struct reldb_chunk
	{
		uint32_t u32Id;
		uint32_t u32Version;
		uint32_t u32Size;
		uint32_t u32Reserved;
	};

	struct reldb_signature
	{
		uint32_t rva;
		std::string pattern;
	};

	struct reldb_patch
	{
		uint32_t version;
		char name[60];
		std::vector<reldb_signature> signs;
		bool has_shared;
	};

	struct reldb_stream
	{
		FILE* stream;
		std::string fname;
		reldb_allocate_func allocate_func;
		reldb_deallocate_func deallocate_func;
		std::map<std::string, std::shared_ptr<reldb_patch>> patches;
	};

	static const char* whitespaceDelimiters = " \t\n\r\f\v";

	std::string& Trim(std::string& str)
	{
		str.erase(str.find_last_not_of(whitespaceDelimiters) + 1);
		str.erase(0, str.find_first_not_of(whitespaceDelimiters));

		return str;
	}

	std::string Trim(const char* s)
	{
		std::string str(s);
		return Trim(str);
	}

	template<typename T>
	bool FileReadBuffer(FILE* fileStream, T* nValue, uint32_t nCount = 1)
	{
		return fread(nValue, sizeof(T), nCount, fileStream) == nCount;
	}

	template<typename T>
	bool FileWriteBuffer(FILE* fileStream, T nValue, uint32_t nCount = 1)
	{
		return fwrite(&nValue, sizeof(T), nCount, fileStream) == nCount;
	}

	bool FileReadString(FILE* fileStream, std::string& sStr)
	{
		uint16_t nLen = 0;
		if (fread(&nLen, sizeof(uint16_t), 1, fileStream) != 1)
			return false;

		sStr.resize((size_t)nLen);
		return fread(sStr.data(), 1, nLen, fileStream) == nLen;
	}

	bool FileWriteString(FILE* fileStream, const std::string& sStr)
	{
		uint16_t nLen = (uint16_t)sStr.length();
		if (fwrite(&nLen, sizeof(uint16_t), 1, fileStream) != 1)
			return false;

		return fwrite(sStr.c_str(), 1, nLen, fileStream) == nLen;
	}

	bool FileGetString(char* buffer, size_t bufferMaxSize, FILE* fileStream)
	{
		if (!fgets(buffer, (int)bufferMaxSize, fileStream))
			return false;

		buffer[bufferMaxSize - 1] = '\0';
		return true;
	}

	class file_unique
	{
	public:
		file_unique(reldb_stream* _stm) : stm(_stm) {}
		~file_unique()
		{
			if (stm->stream)
			{
				fflush(stm->stream);
				fclose(stm->stream);
				stm->stream = nullptr;
			}
		}
	private:
		reldb_stream* stm;
	};

	long __stdcall reldb_patch_open_stream(reldb_stream* stm, std::shared_ptr<reldb_patch>& patch)
	{
		if (!stm)
			return ERR_NO_STREAM;

		reldb_chunk Chunk;
		memset(&Chunk, 0, sizeof(reldb_chunk));

		if (!FileReadBuffer(stm->stream, &Chunk))
			return ERR_STDIO_FAILED;

		if (Chunk.u32Id != RELOCATION_DB_ITEM_CHUNK_ID)
			return ERR_FILE_ID_CORRUPTED;

		if (Chunk.u32Version != RELOCATION_DB_ITEM_CHUNK_VERSION)
			return ERR_DATABASE_VERSION_NO_SUPPORTED;

		if (!Chunk.u32Size)
			return ERR_PATCH_NO_INFO;

		// Версия патча
		if (!FileReadBuffer(stm->stream, &(patch->version)))
			return ERR_STDIO_FAILED;

		std::string _name;

		// Имя патча
		if (!FileReadString(stm->stream, _name))
			return ERR_STDIO_FAILED;

		_name = Trim(_name);
		strncpy_s(patch->name, _name.c_str(), _name.length());

		if (!FileReadBuffer(stm->stream, &Chunk))
			return ERR_STDIO_FAILED;

		if (Chunk.u32Id != RELOCATION_DB_ITEM_DATA_CHUNK_ID)
			return ERR_PATCH_NO_INFO;

		// Чтение данных

		if (Chunk.u32Version == 1)
		{
			auto Count = Chunk.u32Size >> 2;
			auto Pattern = (uint32_t*)stm->allocate_func(Count * sizeof(uint32_t));
			patch->signs.resize(Count);

			if (!Pattern)
			{
				patch->signs.clear();
				return ERR_OUT_OF_MEMORY;
			}

			if (!FileReadBuffer(stm->stream, Pattern, Count))
			{
				stm->deallocate_func(Pattern);
				patch->signs.clear();

				return ERR_STDIO_FAILED;
			}

			for (uint32_t i = 0; i < Count; i++)
				patch->signs[i].rva = Pattern[i];

			stm->deallocate_func(Pattern);
		}
		else if (Chunk.u32Version != RELOCATION_DB_ITEM_DATA_CHUNK_VERSION)
			return ERR_DATABASE_VERSION_NO_SUPPORTED;
		else
		{
			uint32_t Count;
			if (!FileReadBuffer(stm->stream, &Count))
				return ERR_STDIO_FAILED;

			if (!Count)
				return true;

			patch->signs.resize(Count);

			for (uint32_t i = 0; i < Count; i++)
			{
				if (!FileReadBuffer(stm->stream, &(patch->signs[i].rva)))
				{
					patch->signs.clear();
					return ERR_STDIO_FAILED;
				}

				if (!FileReadString(stm->stream, patch->signs[i].pattern))
				{
					patch->signs.clear();
					return ERR_STDIO_FAILED;
				}
			}
		}

		return NO_ERR;
	}

	long __stdcall reldb_patch_save_stream(reldb_stream* stm, std::shared_ptr<reldb_patch>& patch)
	{
		if (!stm)
			return ERR_NO_STREAM;

		auto pos_safe = ftell(stm->stream);

		reldb_chunk Chunk = {
			RELOCATION_DB_ITEM_CHUNK_ID,
			RELOCATION_DB_ITEM_CHUNK_VERSION,
			0,
			0
		};

		if (!FileWriteBuffer(stm->stream, Chunk))
			return ERR_STDIO_FAILED;

		// Версия патча
		if (!FileWriteBuffer(stm->stream, patch->version))
			return ERR_STDIO_FAILED;

		// Имя патча
		if (!FileWriteString(stm->stream, patch->name))
			return ERR_STDIO_FAILED;

		auto pos_safe_2 = ftell(stm->stream);

		Chunk.u32Id = RELOCATION_DB_ITEM_DATA_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_ITEM_DATA_CHUNK_VERSION;
		Chunk.u32Size = 0;

		if (!FileWriteBuffer(stm->stream, Chunk))
			return ERR_STDIO_FAILED;

		if (!FileWriteBuffer(stm->stream, (uint32_t)patch->signs.size()))
			return ERR_STDIO_FAILED;

		// Запись данных
		for (auto it = patch->signs.begin(); it != patch->signs.end(); it++)
		{
			if (!FileWriteBuffer(stm->stream, it->rva))
				return ERR_STDIO_FAILED;

			if (!it->pattern.empty())
			{
				if (!FileWriteString(stm->stream, it->pattern))
					return ERR_STDIO_FAILED;
			}
			else
			{
				if (!FileWriteBuffer(stm->stream, (uint16_t)0))
					return ERR_STDIO_FAILED;
			}
		}

		auto pos_end = ftell(stm->stream);

		fseek(stm->stream, pos_safe, SEEK_SET);

		Chunk.u32Id = RELOCATION_DB_ITEM_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_ITEM_CHUNK_VERSION;
		Chunk.u32Size = pos_end - pos_safe;

		if (!FileWriteBuffer(stm->stream, Chunk))
			return ERR_STDIO_FAILED;

		fseek(stm->stream, pos_safe_2, SEEK_SET);

		Chunk.u32Id = RELOCATION_DB_ITEM_DATA_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_ITEM_DATA_CHUNK_VERSION;
		Chunk.u32Size = pos_end - pos_safe_2;

		if (!FileWriteBuffer(stm->stream, Chunk))
			return ERR_STDIO_FAILED;

		fseek(stm->stream, pos_end, SEEK_SET);

		return NO_ERR;
	}

	VOLTEK_RELDB_API const char* __stdcall reldb_get_error_text(long err)
	{
		switch (err)
		{
		case NO_ERR:
			return "Ok";
		case ERR_NO_STREAM:
			return "Stream is nullptr";
		case ERR_INCORRECT_FNAME:
			return "Incorrect file name";
		case ERR_NOACCESS_FILE:
			return "Permission denied";
		case ERR_STDIO_FAILED:
			return strerror(errno);
		case ERR_OUT_OF_MEMORY:
			return "Out of memory";
		case ERR_FILE_NO_DATABASE:
			return "File is not a database";
		case ERR_DATABASE_VERSION_NO_SUPPORTED:
			return "The file version is not supported";
		case ERR_DATABASE_NO_INFO:
			return "There is no information in the database section";
		case ERR_INCORRECT_CHUNK_SIZE:
			return "Incorrect chunk size";
		case ERR_FILE_ID_CORRUPTED:
			return "File is corrupted";
		case ERR_PATCH_NO_INFO:
			return "There is no information in the patch section";
		case ERR_INDEX_OUT_OF_RANGE:
			return "Index is out of range";
		case ERR_INVALID_ARGUMENTS:
			return "The arguments of the function are set incorrectly";
		case ERR_NO_FOUND:
			return "Not found";
		case ERR_PATCH_NAME_ALREADY_EXISTS:
			return "The patch with that name already exists";
		case ERR_RVA_IN_PATCH_ALREADY_EXISTS:
			return "The specified offset is already available";
		default:
			return "Unknown error";
		}
	}

	VOLTEK_RELDB_API long __stdcall reldb_new_db(reldb_stream** stm, const char* fname,
		reldb_allocate_func allocate_handler, reldb_deallocate_func deallocate_handler)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (!fname)
			return ERR_INCORRECT_FNAME;

		if (!_access(fname, 0) && _access(fname, 6))
			return ERR_NOACCESS_FILE;

		*stm = new reldb_stream;
		if (!(*stm)) return ERR_OUT_OF_MEMORY;

		(*stm)->stream = _fsopen(fname, "wb", _SH_DENYRW);
		if (!(*stm)->stream)
		{
			delete (*stm);
			return ERR_STDIO_FAILED;
		}

		file_unique file(*stm);

		(*stm)->fname = fname;
		(*stm)->allocate_func = allocate_handler ? allocate_handler : malloc;
		(*stm)->deallocate_func = deallocate_handler ? deallocate_handler : free;

		reldb_chunk Chunk = {
			RELOCATION_DB_CHUNK_ID,
			RELOCATION_DB_CHUNK_VERSION,
			sizeof(reldb_chunk) + sizeof(uint32_t),
			0
		};

		if (!FileWriteBuffer((*stm)->stream, Chunk))
		{
			delete (*stm);
			return ERR_STDIO_FAILED;
		}

		Chunk.u32Id = RELOCATION_DB_DATA_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_DATA_CHUNK_VERSION;
		Chunk.u32Size = sizeof(uint32_t);

		if (!FileWriteBuffer((*stm)->stream, Chunk))
		{
			delete (*stm);
			return ERR_STDIO_FAILED;
		}

		if (!FileWriteBuffer((*stm)->stream, (uint32_t)((*stm)->patches.size())))
		{
			delete (*stm);
			return ERR_STDIO_FAILED;
		}

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_open_db(reldb_stream** stm, const char* fname,
		reldb_allocate_func allocate_handler, reldb_deallocate_func deallocate_handler)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (!fname)
			return ERR_INCORRECT_FNAME;

		if (_access(fname, 4))
			return ERR_NOACCESS_FILE;

		*stm = new reldb_stream;
		if (!(*stm)) return ERR_OUT_OF_MEMORY;

		(*stm)->stream = _fsopen(fname, "rb", _SH_DENYWR);
		if (!(*stm)->stream)
		{
			delete (*stm);
			return ERR_STDIO_FAILED;
		}

		file_unique file(*stm);

		(*stm)->fname = fname;
		(*stm)->allocate_func = allocate_handler ? allocate_handler : malloc;
		(*stm)->deallocate_func = deallocate_handler ? deallocate_handler : free;

		reldb_chunk Chunk;
		memset(&Chunk, 0, sizeof(reldb_chunk));

		if (!FileReadBuffer((*stm)->stream, &Chunk))
		{
			delete (*stm);
			return ERR_STDIO_FAILED;
		}

		if (Chunk.u32Id != RELOCATION_DB_CHUNK_ID)
		{
			delete (*stm);
			return ERR_FILE_NO_DATABASE;
		}

		if (Chunk.u32Version != RELOCATION_DB_CHUNK_VERSION)
		{
			delete (*stm);
			return ERR_DATABASE_VERSION_NO_SUPPORTED;
		}

		if (!Chunk.u32Size)
		{
			delete (*stm);
			return ERR_DATABASE_NO_INFO;
		}

		if (!FileReadBuffer((*stm)->stream, &Chunk))
		{
			delete (*stm);
			return ERR_STDIO_FAILED;
		}

		if (Chunk.u32Id != RELOCATION_DB_DATA_CHUNK_ID)
		{
			delete (*stm);
			return ERR_DATABASE_NO_INFO;
		}

		if (Chunk.u32Version != RELOCATION_DB_DATA_CHUNK_VERSION)
		{
			delete (*stm);
			return ERR_DATABASE_VERSION_NO_SUPPORTED;
		}

		if (Chunk.u32Size < sizeof(uint32_t))
		{
			delete (*stm);
			return ERR_INCORRECT_CHUNK_SIZE;
		}

		// Кол-во патчей в базе
		uint32_t count;
		if (!FileReadBuffer((*stm)->stream, &count))
		{
			delete (*stm);
			return ERR_STDIO_FAILED;
		}

		for (uint32_t nId = 0; nId < count; nId++)
		{
			auto patch = std::make_shared<reldb_patch>();
			patch->has_shared = true;

			long result = reldb_patch_open_stream(*stm, patch);
			if (result == NO_ERR)
				(*stm)->patches.emplace(patch->name, patch);
			else
			{
				delete (*stm);
				return result;
			}
		}

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_clear_db(reldb_stream* stm)
	{
		if (!stm)
			return ERR_NO_STREAM;

		stm->patches.clear();

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_save_db(reldb_stream* stm)
	{
		if (!stm)
			return ERR_NO_STREAM;

		stm->stream = _fsopen(stm->fname.c_str(), "wb", _SH_DENYRW);
		if (!stm->stream)
			return ERR_STDIO_FAILED;

		file_unique file(stm);

		reldb_chunk Chunk = {
			RELOCATION_DB_CHUNK_ID,
			RELOCATION_DB_CHUNK_VERSION,
			sizeof(reldb_chunk) + sizeof(uint32_t),
			0
		};

		if (!FileWriteBuffer(stm->stream, Chunk))
			return ERR_STDIO_FAILED;

		Chunk.u32Id = RELOCATION_DB_DATA_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_DATA_CHUNK_VERSION;
		Chunk.u32Size = sizeof(uint32_t);

		if (!FileWriteBuffer(stm->stream, Chunk))
			return ERR_STDIO_FAILED;

		if (!FileWriteBuffer(stm->stream, (uint32_t)(stm->patches.size())))
			return ERR_STDIO_FAILED;

		for (auto it = stm->patches.begin(); it != stm->patches.end(); it++)
		{
			long result = reldb_patch_save_stream(stm, it->second);
			if (result != NO_ERR)
				return result;
		}

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_release_db(reldb_stream* stm)
	{
		if (!stm)
			return ERR_NO_STREAM;

		stm->patches.clear();
		delete stm;

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_count_patches(reldb_stream* stm)
	{
		if (!stm)
			return ERR_NO_STREAM;

		return (long)stm->patches.size();
	}

	VOLTEK_RELDB_API long __stdcall reldb_get_patch_by_id(reldb_stream* stm, reldb_patch** pch, uint32_t index)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (!pch)
			return ERR_INVALID_ARGUMENTS;

		if (index >= (uint32_t)stm->patches.size())
			return ERR_INDEX_OUT_OF_RANGE;

		auto bg = stm->patches.begin();
		std::advance(bg, index);
		*pch = bg->second.get();

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_get_patch_by_name(reldb_stream* stm, reldb_patch** pch, const char* name)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (!name)
			return ERR_INVALID_ARGUMENTS;

		auto bg = stm->patches.find(name);
		if (bg == stm->patches.end())
			return ERR_NO_FOUND;

		*pch = bg->second.get();

		return NO_ERR;
	}

	VOLTEK_RELDB_API bool __stdcall reldb_has_patch(reldb_stream* stm, const char* name)
	{
		reldb_patch* pch = nullptr;
		return reldb_get_patch_by_name(stm, &pch, name) == NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_open_dev_file_patch(reldb_stream* stm, reldb_patch** pch, const char* fname)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (!pch)
			return ERR_INVALID_ARGUMENTS;

		if (!fname)
			return ERR_INCORRECT_FNAME;

		if (_access(fname, 4))
			return ERR_NOACCESS_FILE;

		stm->stream = _fsopen(fname, "rt", _SH_DENYWR);
		if (!stm->stream)
			return ERR_STDIO_FAILED;

		file_unique file(stm);

		*pch = new reldb_patch();
		(*pch)->has_shared = false;

		auto Buffer = std::make_unique<char[]>(1024);
		if (!FileGetString(Buffer.get(), 1024, stm->stream))
			return ERR_STDIO_FAILED;

		auto _name = Trim(Buffer.get());
		strcpy_s((*pch)->name, _name.c_str());

		if (!FileGetString(Buffer.get(), 1024, stm->stream))
			return ERR_STDIO_FAILED;

		char* EndPtr = nullptr;
		(*pch)->version = strtoul(Buffer.get(), &EndPtr, 10);

		auto pos_safe = ftell(stm->stream);
		if (!FileGetString(Buffer.get(), 1024, stm->stream))
			return ERR_STDIO_FAILED;

		if (!_stricmp(Trim(Buffer.get()).c_str(), EXTENDED_FORMAT))
		{
			uint32_t rva;
			auto pattern = std::make_unique<char[]>(256);
			uint32_t pattern_len;

			while (!feof(stm->stream))
			{
				if (!FileGetString(Buffer.get(), 1024, stm->stream))
					return ERR_STDIO_FAILED;

				auto Row = Trim(Buffer.get());
				if (!Row.empty() && (sscanf(Row.c_str(), "%X %u %s", &rva, &pattern_len, pattern.get()) == 3))
				{
					reldb_signature s;
					memset(&s, 0, sizeof(reldb_signature));

					s.rva = rva;
					s.pattern = pattern.get();

					(*pch)->signs.push_back(s);
				}
			}
		}
		else
		{
			fseek(stm->stream, pos_safe, SEEK_SET);
			uint32_t rva;
			while (!feof(stm->stream))
			{
				if (fscanf(stm->stream, "%X", &rva) == 1)
				{
					reldb_signature s;
					memset(&s, 0, sizeof(reldb_signature));

					s.rva = rva;
					(*pch)->signs.push_back(s);
				}
			}
		}

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_save_dev_file_patch(reldb_stream* stm, reldb_patch* pch, const char* fname)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (!pch)
			return ERR_INVALID_ARGUMENTS;

		if (!fname)
			return ERR_INCORRECT_FNAME;

		stm->stream = _fsopen(fname, "wt", _SH_DENYRW);
		if (!stm->stream)
			return ERR_STDIO_FAILED;

		file_unique file(stm);

		// Запись имени патча
		fputs(pch->name, stm->stream);
		fputc('\n', stm->stream);
		// Запись версии патча
		fprintf(stm->stream, "%u\n", pch->version);
		// Расширенный формат
		fputs(EXTENDED_FORMAT, stm->stream);
		fputc('\n', stm->stream);
		// Запись данных
		for (auto it = pch->signs.begin(); it != pch->signs.end(); it++)
			fprintf(stm->stream, "%X %u %s\n", it->rva, (uint32_t)it->pattern.length(), it->pattern.c_str());

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_add_patch(reldb_stream* stm, reldb_patch* pch, bool free_tmp_patch)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (!pch || (pch->has_shared && free_tmp_patch))
			return ERR_INVALID_ARGUMENTS;

		if (reldb_has_patch(stm, pch->name))
			return ERR_PATCH_NAME_ALREADY_EXISTS;

		auto patch = std::make_shared<reldb_patch>();
		patch->has_shared = true;

		strcpy_s(patch->name, pch->name);
		patch->version = pch->version;
		patch->signs.assign(pch->signs.begin(), pch->signs.end());

		stm->patches.emplace(patch->name, patch);

		if (free_tmp_patch)
			delete pch;

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_update_patch(reldb_stream* stm, reldb_patch* pch, bool free_tmp_patch)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (!pch || (pch->has_shared && free_tmp_patch))
			return ERR_INVALID_ARGUMENTS;

		reldb_patch* pch_exists = nullptr;
		auto ret = reldb_get_patch_by_name(stm, &pch_exists, pch->name);

		if (ret == NO_ERR)
		{
			strcpy_s(pch_exists->name, pch->name);
			pch_exists->version = pch->version;
			pch_exists->signs.assign(pch->signs.begin(), pch->signs.end());

			return NO_ERR;
		}

		return reldb_add_patch(stm, pch, free_tmp_patch);
	}

	VOLTEK_RELDB_API long __stdcall reldb_remove_patch_by_id(reldb_stream* stm, uint32_t index)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (index >= (uint32_t)stm->patches.size())
			return ERR_INDEX_OUT_OF_RANGE;

		auto bg = stm->patches.begin();
		std::advance(bg, index);

		stm->patches.erase(bg);

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_remove_patch(reldb_stream* stm, const char* name)
	{
		if (!stm)
			return ERR_NO_STREAM;

		if (!name)
			return ERR_INCORRECT_FNAME;

		auto bg = stm->patches.find(name);
		if (bg == stm->patches.end())
			return ERR_NO_FOUND;

		stm->patches.erase(bg);

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_count_signatures_in_patch(reldb_patch* pch)
	{
		if (!pch)
			return ERR_INVALID_ARGUMENTS;

		return (long)pch->signs.size();
	}

	VOLTEK_RELDB_API long __stdcall reldb_get_version_patch(reldb_patch* pch)
	{
		if (!pch)
			return ERR_INVALID_ARGUMENTS;

		return (long)pch->version;
	}

	VOLTEK_RELDB_API long __stdcall reldb_get_name_patch(reldb_patch* pch, char* name, uint32_t maxsize)
	{
		if (!pch || !name)
			return ERR_INVALID_ARGUMENTS;

		if (!maxsize)
			return NO_ERR;

		strcpy_s(name, maxsize, pch->name);
		name[maxsize] = 0;

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_get_signature_patch(reldb_patch* pch, reldb_signature** sign, uint32_t index)
	{
		if (!pch || !sign)
			return ERR_INVALID_ARGUMENTS;

		if (index >= (uint32_t)pch->signs.size())
			return ERR_INDEX_OUT_OF_RANGE;

		*sign = &(pch->signs[index]);

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_add_signature_to_patch(reldb_patch* pch, uint32_t rva, const char* pattern, bool check_exist)
	{
		if (!pch)
			return ERR_INVALID_ARGUMENTS;

		if (check_exist)
		{
			for (auto it = pch->signs.begin(); it != pch->signs.end(); it++)
			{
				if (it->rva == rva)
					return ERR_RVA_IN_PATCH_ALREADY_EXISTS;
			}
		}

		reldb_signature s;
		s.rva = rva;
		if (pattern) s.pattern = pattern;
		pch->signs.push_back(s);

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_remove_signature_from_patch(reldb_patch* pch, uint32_t index)
	{
		if (!pch)
			return ERR_INVALID_ARGUMENTS;

		if (index >= (uint32_t)pch->signs.size())
			return ERR_INDEX_OUT_OF_RANGE;

		auto bg = pch->signs.begin();
		std::advance(bg, index);
		pch->signs.erase(bg);

		return NO_ERR;
	}

	VOLTEK_RELDB_API long __stdcall reldb_clear_signatures_in_patch(reldb_patch* pch)
	{
		if (!pch)
			return ERR_INVALID_ARGUMENTS;

		pch->signs.clear();

		return NO_ERR;
	}

	VOLTEK_RELDB_API uint32_t __stdcall reldb_get_rva_from_signature(reldb_signature* sign)
	{
		if (!sign) return 0;
		return sign->rva;
	}

	VOLTEK_RELDB_API long __stdcall reldb_get_pattern_from_signature(reldb_signature* sign, char* name, uint32_t maxsize)
	{
		if (!sign || !name || !maxsize)
			return ERR_INVALID_ARGUMENTS;

		memcpy(name, sign->pattern.c_str(), std::min(maxsize, (uint32_t)sign->pattern.length()));

		return NO_ERR;
	}

	VOLTEK_RELDB_API uint32_t __stdcall reldb_get_pattern_length_from_signature(reldb_signature* sign)
	{
		if (!sign) return 0;
		return sign->pattern.empty() ? 0 : (uint32_t)sign->pattern.length();
	}
}

#pragma pack(pop)