#include "signmaker.h"
#include "pluginmain.h"

#include <algorithm>
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

xCoffee::TSignMaker::TSignMaker()
{
	if (!zydis_decoder_init())
		handle = zydis_create_instruction();
	
	mem = std::make_unique<uint8_t[]>((SIZE_SIG * 2) + 1);
}

xCoffee::TSignMaker::~TSignMaker()
{
	if (handle)
	{
		zydis_release_instruction(handle);
		handle = nullptr;
	}
}

std::string xCoffee::TSignMaker::CreateSign(size_t a_addr, size_t a_size) noexcept
{
	if (!a_size || !a_addr) return "";
	
	auto readSize = std::min(a_size, SIZE_SIG * 4);
	if (!DbgMemRead(a_addr, mem.get(), readSize))
		return "";

	auto def_mask_coded = [&a_size](const uint8_t* raw, std::string& mask, size_t& endPattern, size_t len) {
		if (len == 1)
		{
			auto ch = (raw + endPattern)[0];
			if ((ch == 0x90) || (ch == 0xCC)) return true;
		}

		if (a_size > (endPattern + len))
			mask.append(Hex2Str(raw + endPattern, (size_t)len));
		else
			mask.append(Hex2Str(raw + endPattern, a_size - endPattern));

		return false;
		};

	std::string mask{};
	size_t endPattern = 0;
	for (; a_size > endPattern; )
	{
		if (zydis_init_instruction(handle, mem.get() + endPattern))
			break;

		auto ins = reinterpret_cast<ZydisDecodedInstruction*>(handle);
		size_t len = zydis_instruction_length(handle);

		if ((ins->mnemonic >= ZYDIS_MNEMONIC_JB) && (ins->mnemonic <= ZYDIS_MNEMONIC_JZ) ||
			(ins->mnemonic == ZYDIS_MNEMONIC_CALL))
		{
			if (ins->opcode == 0xFF)
			{
				auto off = ins->raw.disp.offset;
				if (!off)
					def_mask_coded(mem.get(), mask, endPattern, len);
				else
				{
					mask.append(Hex2Str(mem.get() + endPattern, (size_t)off));
					for (size_t i = 0; i < ins->raw.disp.size / 8; i++)
						mask.append("??");
				}
			}
			else
			{
				auto off = ins->raw.imm[0].offset;
				if (!off)
					def_mask_coded(mem.get(), mask, endPattern, len);
				else
				{
					mask.append(Hex2Str(mem.get() + endPattern, (size_t)off));
					for (size_t i = 0; i < ins->raw.imm[0].size / 8; i++)
						mask.append("??");
				}
			}
		}
		else if (ins->raw.disp.offset)
		{
			mask.append(Hex2Str(mem.get() + endPattern, (size_t)ins->raw.disp.offset));
			for (size_t i = 0; i < ins->raw.disp.size / 8; i++)
				mask.append("??");
			auto posend = (size_t)ins->raw.disp.offset + ins->raw.disp.size / 8;
			if (posend < len)
				mask.append(Hex2Str(mem.get() + endPattern + posend, len - posend));
		}
		else if (len != 1)
			mask.append(Hex2Str(mem.get() + endPattern, (size_t)len));

		if (len == 1)
		{
			auto ch = (mem.get() + endPattern)[0];
			if ((ch == 0x90) || (ch == 0xCC)) break;
			mask.append(Hex2Str(mem.get() + endPattern, (size_t)len));
		}

		endPattern += len;
	}

	if ((a_size * 2) < mask.length())
		mask.resize(a_size * 2);

	return mask;
}

std::string xCoffee::TSignMaker::Hex2Str(const uint8_t* raw, size_t len) noexcept
{
	std::string s{};
	auto l = std::min(len, SIZE_SIG);
	char xchar[SIZE_SIG] = { 0 };

	for (size_t i = 0; i < len; i++)
	{
		sprintf_s(xchar, "%02X", raw[i]);
		s += xchar;
	}

	return s;
}