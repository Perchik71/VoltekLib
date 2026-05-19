#include "pattern.h"
#include <Zydis/DecoderTypes.h>
#include <Zydis/Formatter.h>

extern "C" int zydis_decoder_init();
extern "C" void* zydis_create_instruction();
extern "C" int zydis_release_instruction(void* instruction);
extern "C" int zydis_init_instruction(void* instruction, void* raw);
extern "C" int zydis_instruction_length(void* instruction);
extern "C" int zydis_instruction_has_address(void* instruction);
extern "C" int zydis_instruction_opcode(void* instruction);
extern "C" unsigned __int64 zydis_instruction_attributes(void* instruction);

#include <iostream>

static int zydis_instruction_arg_width(void* instruction) noexcept
{
	auto ins = reinterpret_cast<ZydisDecodedInstruction*>(instruction);

	/*ins->length;


	ZYAN_BITS_TO_REPRESENT(ins->mnemonic)

	
	ins->address_width;*/


	auto flags = zydis_instruction_attributes(instruction);

	switch ((uint8_t)zydis_instruction_opcode(instruction))
	{
	case 0x81:
		return 4;
	case 0x83:
		return 1;
	case 0xC6: // mov
		return 1;
	case 0xC7: // mov
		return flags & (1llu << 34) ? 2 : 4;
	default:
		return 0;
	}
}

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

std::string pattern::transform(std::string raw, uint32_t rva) const
{
	std::string mask = "";
	if (raw.empty()) return mask;

	auto def_mask_coded = [](const pattern* sender, std::string& raw, std::string& mask, size_t& endPattern, size_t len) {
		if (len == 1)
		{
			auto ch = ((uint8_t*)raw.data() + endPattern)[0];
			if ((ch == 0x90) || (ch == 0xCC)) return true;
		}

		if (raw.length() > (endPattern + len))
			mask.append(sender->hex2str((uint8_t*)raw.data() + endPattern, (size_t)len));
		else
			mask.append(sender->hex2str((uint8_t*)raw.data() + endPattern, raw.length() - endPattern));

		return false;
		};

	size_t endPattern = 0;
	for (; raw.length() > endPattern; )
	{
		if (zydis_init_instruction(_instruction, raw.data() + endPattern))
			break;

		auto ins = reinterpret_cast<ZydisDecodedInstruction*>(_instruction);
		size_t len = zydis_instruction_length(_instruction);

		if ((ins->mnemonic >= ZYDIS_MNEMONIC_JB) && (ins->mnemonic <= ZYDIS_MNEMONIC_JZ) || 
			(ins->mnemonic == ZYDIS_MNEMONIC_CALL))
		{
			if (ins->opcode == 0xFF)
			{
				auto off = ins->raw.disp.offset;
				if (!off)
					def_mask_coded(this, raw, mask, endPattern, len);
				else
				{
					mask.append(hex2str((uint8_t*)raw.data() + endPattern, (size_t)off));
					for (size_t i = 0; i < ins->raw.disp.size / 8; i++)
						mask.append("??");
				}
			}
			else
			{
				auto off = ins->raw.imm[0].offset;
				if (!off)
					def_mask_coded(this, raw, mask, endPattern, len);
				else
				{
					mask.append(hex2str((uint8_t*)raw.data() + endPattern, (size_t)off));
					for (size_t i = 0; i < ins->raw.imm[0].size / 8; i++)
						mask.append("??");
				}
			}
		}
		else if (ins->raw.disp.offset)
		{
			mask.append(hex2str((uint8_t*)raw.data() + endPattern, (size_t)ins->raw.disp.offset));
			for (size_t i = 0; i < ins->raw.disp.size / 8; i++)
				mask.append("??");
		}
		else
			mask.append(hex2str((uint8_t*)raw.data() + endPattern, (size_t)len));

		if (len == 1)
		{
			auto ch = ((uint8_t*)raw.data() + endPattern)[0];
			if ((ch == 0x90) || (ch == 0xCC)) break;
		}

		endPattern += len;
	}

	if ((raw.length() * 2) < mask.length())
		mask.resize(raw.length() * 2);

	//printf("%s\n", mask.c_str());

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