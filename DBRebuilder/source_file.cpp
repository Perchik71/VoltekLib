#include "source_file.h"

#include <algorithm>

#include "pattern.h"

#define START_PROCESS_ADDR 0x1000
#define NEED_PATTERN_SIZE 64

source_file::source_file() : _begin(0) 
{
}

source_file::source_file(const char* fname) : _begin(0) 
{
	open(fname);
}

source_file::~source_file()
{ 
}

bool source_file::open(const char* fname)
{
	auto _f = _fsopen(fname, "rb", _SH_DENYWR);
	if (_f)
	{
		fseek(_f, 0, SEEK_END);
		data.resize(ftell(_f));
		fseek(_f, 0, SEEK_SET);
		fread(data.data(), 1, data.size(), _f);		
		fclose(_f);

		uint32_t start, end;
		// get begin for rva (default 0x400)
		get_pe_section_range(nullptr, &start, &_begin);
		// get range code in file
		get_pe_section_range(".text", &_code_start, &_code_end);
		/*if (get_pe_section_range(".textbss", &start, &end))
		{
			_code_start = std::min(_code_start, start);
			_code_end = std::max(_code_end, end);
		}*/
		//// correction
		//if (_begin < _code_start)
		//{
		//	auto diff = _code_start - _begin;
		//	_code_start = _begin;
		//	_code_end -= diff;
		//}

		return true;
	}

	return false;
}

void source_file::get_code_range(uint32_t* start, uint32_t* end) const
{
	*start = _code_start;
	*end = _code_end;
}

uint32_t source_file::off2rva(uint32_t target) const
{
	return (target - _begin) + _code_start;
}

uint32_t source_file::rva2off(uint32_t target) const
{
	return (target - _code_start) + _begin;
}

bool source_file::is_rva_in_code_range(uint32_t rva) const
{
	return (rva >= _code_start) && (rva < _code_end);
}

bool source_file::get_raw(uint32_t rva, std::string& raw) const
{
	if (!data.size() || !is_rva_in_code_range(rva)) return false;

	auto locate = rva2off(rva);
	if (locate >= data.size())
		return false;

	raw.resize(NEED_PATTERN_SIZE);

	uint32_t reads = NEED_PATTERN_SIZE;
	if (reads > (uint32_t)(data.size() - locate))
		reads = (uint32_t)(data.size() - locate);

	memcpy(const_cast<char*>(raw.data()), data.data() + locate, reads);
	raw.resize(reads);

	return true;
}

uint32_t source_file::find_pattern(std::string mask)
{
	char opcode[3] = { 0 };
	std::vector<std::pair<uint8_t, bool>> pattern;

	for (size_t i = 0; i < mask.length(); i += 2)
	{
		if (mask[i] != '?')
		{
			memcpy(opcode, &mask[i], 2);
			pattern.emplace_back((uint8_t)strtoul(opcode, nullptr, 16), false);
		}
		else
		{
			if (mask[i + 1] != '?')
				return 0;
			
			pattern.emplace_back(0x00, true);
		}
	}

	auto ret = std::search(data.begin(), data.end(), pattern.begin(), pattern.end(),
		[](uint8_t CurrentByte, std::pair<uint8_t, bool>& Pattern) {
			return Pattern.second || (CurrentByte == Pattern.first);
		});

	if (ret == data.end())
		return 0;

	return off2rva((uint32_t)std::distance(data.begin(), ret));
}

std::vector<uint32_t> source_file::find_patterns(std::string mask)
{
	char opcode[3] = { 0 };
	std::vector<uint32_t> results;
	std::vector<std::pair<uint8_t, bool>> pattern;

	for (size_t i = 0; i < mask.length(); i += 2)
	{
		if (mask[i] != '?')
		{
			memcpy(opcode, &mask[i], 2);
			pattern.emplace_back((uint8_t)strtoul(opcode, nullptr, 16), false);
		}
		else
		{
			if (mask[i + 1] != '?')
				return results;

			pattern.emplace_back(0x00, true);
		}
	}

	for (auto i = data.begin();;)
	{
		auto ret = std::search(i, data.end(), pattern.begin(), pattern.end(),
			[](uint8_t CurrentByte, std::pair<uint8_t, bool>& Pattern) {
				return Pattern.second || (CurrentByte == Pattern.first);
			});

		if (ret == data.end())
			break;

		results.push_back(off2rva((uint32_t)std::distance(data.begin(), ret)));
		i = ++ret;
	}

	return results;
}

uint32_t source_file::search(std::string mask, int32_t offset, char check_char)
{
	auto patterns = find_patterns(mask);
	if (!patterns.size()) return 0;

	for (auto it = patterns.begin(); it != patterns.end(); it++)
	{
		if (data[(size_t)rva2off((ptrdiff_t)*it + offset)] == check_char)
			return (uint32_t)((ptrdiff_t)*it + offset);
	}

	return 0;
}

bool source_file::get_pe_section_range(const char* section, uint32_t* start, uint32_t* end)
{
	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(data.data() + ((PIMAGE_DOS_HEADER)data.data())->e_lfanew);
	PIMAGE_SECTION_HEADER cur_section = IMAGE_FIRST_SECTION(ntHeaders);

	if (!section || strlen(section) <= 0)
	{
		if (start)
			*start = 0;

		if (end)
			*end = ntHeaders->OptionalHeader.SizeOfHeaders;

		return true;
	}

	for (uint32_t i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++, cur_section++)
	{
		char sectionName[sizeof(IMAGE_SECTION_HEADER::Name) + 1] = { };
		memcpy(sectionName, cur_section->Name, sizeof(IMAGE_SECTION_HEADER::Name));

		if (!strcmp(sectionName, section))
		{
			if (start)
				*start = cur_section->VirtualAddress;

			if (end)
				*end = cur_section->VirtualAddress + cur_section->Misc.VirtualSize;

			return true;
		}
	}

	return false;
}

bool source_file::can_mask_by_raw64(uint32_t rva) const
{
	std::string raw;
	if (!get_raw(rva, raw))
		return false;

	pattern p;
	return !p.failed_beg(raw);
}

bool source_file::get_mask_by_raw64(uint32_t rva, std::string& mask) const
{
	std::string raw;
	if (!get_raw(rva, raw))
		return false;

	pattern p;
	mask = p.transform(raw, rva);

	return true;
}

bool source_file::get_mask_by_raw64_ex(uint32_t rva, std::string& mask) const
{
	bool can = false;
	uint32_t nearest_rva = rva;
	for (int32_t i = 0; i < 15; i++)
	{
		can = can_mask_by_raw64(nearest_rva - i);
		if (can)
		{
			get_mask_by_raw64(nearest_rva - i, mask);
			break;
		}
	}

	return can;
}