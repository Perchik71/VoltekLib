// Copyright © 2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: MIT

#include "..\include\Voltek.RelocationDatabase.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mmsystem.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <memory>
#include <vector>

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

	constexpr static const char* MESSAGE_FAILED_READ2 = "[EEROR] RelocationDatabase: The end of the file was reached unexpectedly";
	constexpr static const char* MESSAGE_FAILED_WRITE2 = "[EEROR] RelocationDatabase: Failed to write to a file";
	constexpr static const char* MESSAGE_FAILED_READ = "[EEROR] RelocationDatabaseItem: The end of the file was reached unexpectedly";
	constexpr static const char* MESSAGE_FAILED_WRITE = "[EEROR] RelocationDatabaseItem: Failed to write to a file";

	constexpr static const char* EXTENDED_FORMAT = "extended";

	struct reldb_chunk
	{
		uint32_t u32Id;
		uint32_t u32Version;
		uint32_t u32Size;
		uint32_t u32Reserved;
	};

	void _MESSAGE(reldb_stream_msg_event handler, const char* msg)
	{
		if (handler) handler(msg);
	}

	void _MESSAGE_VA(reldb_stream_msg_event handler, const char* fmt, ...)
	{
		if (handler) handler(fmt);
	}

	static const char* whitespaceDelimiters = " \t\n\r\f\v";

	inline std::string& Trim(std::string& str)
	{
		str.erase(str.find_last_not_of(whitespaceDelimiters) + 1);
		str.erase(0, str.find_first_not_of(whitespaceDelimiters));

		return str;
	}

	inline std::string Trim(const char* s)
	{
		std::string str(s);
		return Trim(str);
	}

	template<typename T>
	inline bool FileReadBuffer(FILE* fileStream, T* nValue, uint32_t nCount = 1)
	{
		return fread(nValue, sizeof(T), nCount, fileStream) == nCount;
	}

	template<typename T>
	inline bool FileWriteBuffer(FILE* fileStream, T nValue, uint32_t nCount = 1)
	{
		return fwrite(&nValue, sizeof(T), nCount, fileStream) == nCount;
	}

	inline bool FileReadString(FILE* fileStream, std::string& sStr)
	{
		uint16_t nLen = 0;
		if (fread(&nLen, sizeof(uint16_t), 1, fileStream) != 1)
			return false;

		sStr.resize((size_t)nLen + 1);
		sStr[nLen] = '\0';

		return fread(sStr.data(), 1, nLen, fileStream) == nLen;
	}

	inline bool FileWriteString(FILE* fileStream, const std::string& sStr)
	{
		uint16_t nLen = (uint16_t)sStr.length();
		if (fwrite(&nLen, sizeof(uint16_t), 1, fileStream) != 1)
			return false;

		return fwrite(sStr.c_str(), 1, nLen, fileStream) == nLen;
	}

	inline bool FileGetString(char* buffer, size_t bufferMaxSize, FILE* fileStream)
	{
		if (!fgets(buffer, (int)bufferMaxSize, fileStream))
			return false;

		buffer[bufferMaxSize - 1] = '\0';
		return true;
	}

	VOLTEK_RELDB_API bool load_reldb_item_stream(read_reldb_item_stream* stream)
	{
		if (!stream || !stream->stream || !stream->allocate_handler || !stream->deallocate_handler)
			return false;

		reldb_chunk Chunk;
		memset(&Chunk, 0, sizeof(reldb_chunk));

		if (!FileReadBuffer(stream->stream, &Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
			return false;
		}

		if (Chunk.u32Id != RELOCATION_DB_ITEM_CHUNK_ID)
		{
			_MESSAGE(stream->msg_handler, "[EEROR] RelocationDatabaseItem: This is not patch");
			return false;
		}

		if (Chunk.u32Version != RELOCATION_DB_ITEM_CHUNK_VERSION)
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabaseItem: The patch version is not supported, please update the platform");
			return false;
		}

		if (!Chunk.u32Size)
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabaseItem: There is no patch data");
			return false;
		}

		// Версия патча
		if (!FileReadBuffer(stream->stream, &(stream->version)))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
			return false;
		}

		std::string _name;

		// Имя патча
		if (!FileReadString(stream->stream, _name))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
			return false;
		}

		_name = Trim(_name);
		strncpy_s(stream->name, _name.c_str(), _name.length());
		
		if (!FileReadBuffer(stream->stream, &Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
			return false;
		}

		if (Chunk.u32Id != RELOCATION_DB_ITEM_DATA_CHUNK_ID)
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabaseItem: This is not patch data");
			return false;
		}

		// Чтение данных

		if (Chunk.u32Version == 1)
		{
			auto Count = Chunk.u32Size >> 2;
			auto Pattern = (uint32_t*)stream->allocate_handler(Count * sizeof(uint32_t));
			stream->signs = (reldb_signature*)stream->allocate_handler(Count * sizeof(reldb_signature));

			if (!Pattern || !stream->signs)
			{
				if (Pattern) stream->deallocate_handler(Pattern);
				if (stream->signs) stream->deallocate_handler(stream->signs);

				stream->signs = nullptr;

				_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
				return false;
			}

			memset(stream->signs, 0, Count * sizeof(reldb_signature));

			if (!FileReadBuffer(stream->stream, Pattern, Count))
			{
				_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);

				stream->deallocate_handler(stream->signs);
				stream->signs = nullptr;

				return false;
			}

			for (uint32_t i = 0; i < Count; i++)
				stream->signs[i].rva = Pattern[i];
		}
		else if (Chunk.u32Version != RELOCATION_DB_ITEM_DATA_CHUNK_VERSION)
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabaseItem: The patch data version is not supported, please update the platform");
			return false;
		}
		else
		{
			uint32_t Count;
			if (!FileReadBuffer(stream->stream, &Count))
			{
				_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
				return false;
			}

			if (!Count)
				return true;

			stream->signs = (reldb_signature*)stream->allocate_handler(Count * sizeof(reldb_signature));
			if (!stream->signs)
			{
				_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
				return false;
			}

			memset(stream->signs, 0, Count * sizeof(reldb_signature));

			uint32_t rva;
			std::string pattern;

			for (uint32_t i = 0; i < Count; i++)
			{
				if (!FileReadBuffer(stream->stream, &rva))
				{
					_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);

					stream->deallocate_handler(stream->signs);
					stream->signs = nullptr;

					return false;
				}

				if (!FileReadString(stream->stream, pattern))
				{
					_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);

					stream->deallocate_handler(stream->signs);
					stream->signs = nullptr;

					return false;
				}

				stream->signs[i].rva = rva;
				stream->signs[i].pattern_len = (uint32_t)pattern.length();

				if (stream->signs[i].pattern_len)
				{
					stream->signs[i].pattern = (char*)stream->allocate_handler((size_t)stream->signs[i].pattern_len + 1);
					if (!stream->signs[i].pattern)
					{
						_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);

						stream->deallocate_handler(stream->signs);
						stream->signs = nullptr;

						return false;
					}

					strcpy(stream->signs[i].pattern, pattern.c_str());
					stream->signs[i].pattern[stream->signs[i].pattern_len] = 0;
				}
			}
		}

		return true;
	}

	VOLTEK_RELDB_API bool save_reldb_item_stream(read_reldb_item_stream* stream)
	{
		if (!stream || !stream->stream)
			return false;

		auto pos_safe = ftell(stream->stream);

		reldb_chunk Chunk = {
			RELOCATION_DB_ITEM_CHUNK_ID,
			RELOCATION_DB_ITEM_CHUNK_VERSION,
			0,
			0
		};

		if (!FileWriteBuffer(stream->stream, Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
			return false;
		}

		// Версия патча
		if (!FileWriteBuffer(stream->stream, stream->version))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
			return false;
		}

		// Имя патча
		if (!FileWriteString(stream->stream, stream->name))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
			return false;
		}

		auto pos_safe_2 = ftell(stream->stream);

		Chunk.u32Id = RELOCATION_DB_ITEM_DATA_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_ITEM_DATA_CHUNK_VERSION;
		Chunk.u32Size = 0;

		if (!FileWriteBuffer(stream->stream, Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
			return false;
		}

		if (!FileWriteBuffer(stream->stream, stream->sign_total))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
			return false;
		}

		// Запись данных
		for (uint32_t i = 0; i < stream->sign_total; i++)
		{
			if (!FileWriteBuffer(stream->stream, stream->signs[i].rva))
			{
				_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
				return false;
			}

			if ((stream->signs[i].pattern_len > 0) && stream->signs[i].pattern)
			{
				if (!FileWriteString(stream->stream, stream->signs[i].pattern))
				{
					_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
					return false;
				}
			}
			else
			{
				if (!FileWriteBuffer(stream->stream, (uint16_t)0))
				{
					_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
					return false;
				}
			}
		}

		auto pos_end = ftell(stream->stream);

		fseek(stream->stream, pos_safe, SEEK_SET);

		Chunk.u32Id = RELOCATION_DB_ITEM_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_ITEM_CHUNK_VERSION;
		Chunk.u32Size = pos_end - pos_safe;

		if (!FileWriteBuffer(stream->stream, Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
			return false;
		}

		fseek(stream->stream, pos_safe_2, SEEK_SET);

		Chunk.u32Id = RELOCATION_DB_ITEM_DATA_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_ITEM_DATA_CHUNK_VERSION;
		Chunk.u32Size = pos_end - pos_safe_2;

		if (!FileWriteBuffer(stream->stream, Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE);
			return false;
		}

		fseek(stream->stream, pos_end, SEEK_SET);

		return true;
	}

	VOLTEK_RELDB_API bool load_reldb_item_stream_dev(read_reldb_item_stream* stream)
	{
		if (!stream || !stream->stream || !stream->allocate_handler || !stream->deallocate_handler)
			return false;
		
		auto Buffer = std::make_unique<char[]>(1024);
		if (!FileGetString(Buffer.get(), 1024, stream->stream))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
			return false;
		}

		auto _name = Trim(Buffer.get());
		strcpy_s(stream->name, _name.c_str());
		_MESSAGE_VA(stream->msg_handler, "\tThe patch name: \"%s\"", stream->name);

		if (!FileGetString(Buffer.get(), 1024, stream->stream))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
			return false;
		}

		char* EndPtr = nullptr;
		stream->version = strtoul(Buffer.get(), &EndPtr, 10);
		_MESSAGE_VA(stream->msg_handler, "\tThe patch version: %u", stream->version);

		auto pos_safe = ftell(stream->stream);
		if (!FileGetString(Buffer.get(), 1024, stream->stream))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
			return false;
		}

		std::vector<reldb_signature> sings;

		if (!_stricmp(Trim(Buffer.get()).c_str(), EXTENDED_FORMAT))
		{
			_MESSAGE_VA(stream->msg_handler, "\tExtended format");

			uint32_t rva;
			auto pattern = std::make_unique<char[]>(256);
			uint32_t pattern_len;

			while (!feof(stream->stream))
			{
				if (!FileGetString(Buffer.get(), 1024, stream->stream))
				{
					_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
					return false;
				}

				auto Row = Trim(Buffer.get());
				if (!Row.empty() && (sscanf(Row.c_str(), "%X %u %s", &rva, &pattern_len, pattern.get()) == 3))
				{
					reldb_signature s;
					memset(&s, 0, sizeof(reldb_signature));

					s.rva = rva;
					s.pattern_len = (uint32_t)strlen(pattern.get());
					if (s.pattern_len)
					{
						s.pattern = (char*)stream->allocate_handler((size_t)s.pattern_len + 1);
						if (!s.pattern)
						{
							_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);

							stream->deallocate_handler(stream->signs);
							stream->signs = nullptr;

							return false;
						}

						strcpy(s.pattern, pattern.get());
						s.pattern[s.pattern_len] = 0;
					}
					else
						s.pattern = nullptr;

					sings.push_back(s);
				}
			}
		}
		else
		{
			std::vector<reldb_signature> sings;

			fseek(stream->stream, pos_safe, SEEK_SET);
			uint32_t rva;
			while (!feof(stream->stream))
			{
				if (fscanf(stream->stream, "%X", &rva) == 1)
				{
					reldb_signature s;
					memset(&s, 0, sizeof(reldb_signature));

					s.rva = rva;
					sings.push_back(s);
				}
			}
		}

		stream->sign_total = sings.size();
		stream->signs = (reldb_signature*)stream->allocate_handler((size_t)stream->sign_total * sizeof(reldb_signature));
		if (!stream->signs)
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ);
			return false;
		}

		memcpy(stream->signs, sings.data(), (size_t)stream->sign_total * sizeof(reldb_signature));

		return true;
	}

	VOLTEK_RELDB_API bool save_reldb_item_stream_dev(read_reldb_item_stream* stream)
	{
		if (!stream || !stream->stream)
			return false;

		// Запись имени патча
		fputs(stream->name, stream->stream);
		fputc('\n', stream->stream);
		// Запись версии патча
		fprintf(stream->stream, "%u\n", stream->version);
		// Расширенный формат
		fputs(EXTENDED_FORMAT, stream->stream);
		// Запись данных
		for (uint32_t i = 0; i < stream->sign_total; i++)
			fprintf(stream->stream, "%X %u %s\n", stream->signs[i].rva, stream->signs[i].pattern_len, stream->signs[i].pattern);

		return true;
	}

	VOLTEK_RELDB_API bool create_reldb_stream(read_reldb_stream* stream)
	{
		reldb_chunk Chunk = {
			RELOCATION_DB_CHUNK_ID,
			RELOCATION_DB_CHUNK_VERSION,
			sizeof(reldb_chunk) + sizeof(uint32_t),
			0
		};

		if (!FileWriteBuffer(stream->stream, Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE2);
			return false;
		}

		Chunk.u32Id = RELOCATION_DB_DATA_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_DATA_CHUNK_VERSION;
		Chunk.u32Size = sizeof(uint32_t);

		if (!FileWriteBuffer(stream->stream, Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE2);
			return false;
		}

		if (!FileWriteBuffer(stream->stream, stream->patch_total))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE2);
			return false;
		}

		return true;
	}

	VOLTEK_RELDB_API bool open_reldb_stream(read_reldb_stream* stream)
	{
		reldb_chunk Chunk;
		memset(&Chunk, 0, sizeof(reldb_chunk));

		if (!FileReadBuffer(stream->stream, &Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ2);
			return false;
		}

		if (Chunk.u32Id != RELOCATION_DB_CHUNK_ID)
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabase: This is not database");
			return false;
		}

		if (Chunk.u32Version != RELOCATION_DB_CHUNK_VERSION)
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabase: The database version is not supported, please update the platform");
			return false;
		}

		if (!Chunk.u32Size)
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabase: There is no database data");
			return false;
		}

		if (!FileReadBuffer(stream->stream, &Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ2);
			return false;
		}

		if (Chunk.u32Id != RELOCATION_DB_DATA_CHUNK_ID)
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabase: This is not database data");
			return false;
		}

		if (Chunk.u32Version != RELOCATION_DB_DATA_CHUNK_VERSION)
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabase: The database data version is not supported, please update the platform");
			return false;
		}

		if (Chunk.u32Size < sizeof(uint32_t))
		{
			_MESSAGE(stream->msg_handler, "[ERROR] RelocationDatabase: Chunk.u32Size < sizeof(uint32_t)");
			return false;
		}

		// Кол-во патчей в базе
		if (!FileReadBuffer(stream->stream, &stream->patch_total))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_READ2);
			return false;
		}

		return true;
	}

	VOLTEK_RELDB_API bool update_reldb_stream(read_reldb_stream* stream)
	{
		reldb_chunk Chunk = {
			RELOCATION_DB_CHUNK_ID,
			RELOCATION_DB_CHUNK_VERSION,
			sizeof(reldb_chunk) + sizeof(uint32_t),
			0
		};

		if (!FileWriteBuffer(stream->stream, Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE2);
			return false;
		}

		Chunk.u32Id = RELOCATION_DB_DATA_CHUNK_ID;
		Chunk.u32Version = RELOCATION_DB_DATA_CHUNK_VERSION;
		Chunk.u32Size = sizeof(uint32_t);

		if (!FileWriteBuffer(stream->stream, Chunk))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE2);
			return false;
		}

		if (!FileWriteBuffer(stream->stream, stream->patch_total))
		{
			_MESSAGE(stream->msg_handler, MESSAGE_FAILED_WRITE2);
			return false;
		}

		return true;
	}
}