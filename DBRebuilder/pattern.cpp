#include "pattern.h"

extern "C" int zydis_decoder_init();
extern "C" void* zydis_create_instruction();
extern "C" int zydis_release_instruction(void* instruction);
extern "C" int zydis_init_instruction(void* instruction, void* raw);
extern "C" int zydis_instruction_length(void* instruction);
extern "C" int zydis_instruction_has_address(void* instruction);

#include <iostream>

pattern::pattern()
{
	init();
}

pattern::~pattern()
{
	release();
}

bool pattern::failed_beg(std::string raw) const
{
	if (raw.empty()) return true;
	return zydis_init_instruction(_instruction, raw.data()) != 0;
}

std::string pattern::transform(std::string raw) const
{
	std::string mask = "";
	if (raw.empty()) return mask;

	size_t endPattern = 0;
	for (; raw.length() > endPattern; )
	{
		if (zydis_init_instruction(_instruction, raw.data() + endPattern))
			break;

		auto len = zydis_instruction_length(_instruction);
		if (!zydis_instruction_has_address(_instruction))
		{
			if (len < 4)
			{
				if (len != 2)
					break;
				
				mask.append(hex2str((uint8_t*)raw.data() + endPattern, (size_t)len - 1));
				mask.append("??");
			}
			else
			{
				mask.append(hex2str((uint8_t*)raw.data() + endPattern, (size_t)len - 4));
				mask.append("????????");
			}
		}
		else
		{
			if (len == 1)
			{
				auto ch = ((uint8_t*)raw.data() + endPattern)[0];
				if ((ch == 0x90) || (ch == 0xCC)) break;
			}

			if (raw.length() > (endPattern + len))
				mask.append(hex2str((uint8_t*)raw.data() + endPattern, (size_t)len));
			else
				mask.append(hex2str((uint8_t*)raw.data() + endPattern, raw.length() - endPattern));
		}

		endPattern += len;
	}

	return mask;
}

bool pattern::init()
{
	if (!zydis_decoder_init())
	{
		_instruction = zydis_create_instruction();
		return _instruction != nullptr;
	}

	return false;
}

void pattern::release()
{
	if (_instruction)
	{
		zydis_release_instruction(_instruction);
		_instruction = nullptr;
	}
}

std::string pattern::hex2str(const uint8_t* raw, size_t len) const
{
	std::string s;

	char xchar[64] = { 0 };
	for (size_t i = 0; i < len; i++)
	{
		sprintf_s(xchar, "%02X", raw[i]);
		s += xchar;
	}

	return s;
}