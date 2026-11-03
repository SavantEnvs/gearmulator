// Writes the starter seeds of fuzz_frontpanel: one file per front panel frame type, produced by the library's own
// frame writers. Not part of the build; compile it against the objects in build-fuzz/obj and run it as
// `gen_frontpanel <dir>` (see gen_seeds.py).
#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "synthLib/frontPanelSysex.h"
#include "synthLib/midiTypes.h"

namespace
{
	int g_n = 0;

	void dump(const char* _dir, const char* _name, const std::vector<synthLib::SMidiEvent>& _events)
	{
		for (const auto& e : _events)
		{
			const std::string path = std::string(_dir) + "/" + _name + std::to_string(g_n++) + ".syx";
			FILE* f = fopen(path.c_str(), "wb");
			fwrite(e.sysex.data(), 1, e.sysex.size(), f);
			fclose(f);
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
		return 1;
	const char* dir = argv[1];

	using namespace synthLib;
	std::vector<SMidiEvent> ev;

	std::vector<uint8_t> cells(16 * 2);
	for (size_t i = 0; i < cells.size(); ++i)
		cells[i] = static_cast<uint8_t>('A' + i);
	fp::sendLcdText(ev, 16, 2, cells.data());
	dump(dir, "lcdtext", ev); ev.clear();

	std::array<uint8_t, fp::g_cgRamSize> cg{};
	for (size_t i = 0; i < cg.size(); ++i)
		cg[i] = static_cast<uint8_t>(i & 0x1f);
	fp::sendLcdCgRam(ev, cg);
	dump(dir, "cgram", ev); ev.clear();

	std::vector<uint8_t> px(fp::bitmapStride(20) * 8, 0xa5);
	fp::sendLcdBitmap(ev, 20, 8, px.data());
	dump(dir, "bitmap", ev); ev.clear();

	fp::sendLedMask(ev, std::vector<bool>{true, false, true, true, false, false, true, true, true, false});
	dump(dir, "ledmask", ev); ev.clear();

	fp::sendButton(ev, 5, true);
	fp::sendEncoder(ev, 3, -7);
	fp::sendParam(ev, 2, 17, -123456);
	fp::sendRequestState(ev, 0x05);
	dump(dir, "input", ev); ev.clear();

	// an unknown framework type and a product type
	const std::vector<uint8_t> unknown = {0xf0, 0x7d, 0x54, 0x75, 0x53, 0x5f, 0x01, 0x02, 0xf7};
	const std::vector<uint8_t> product = {0xf0, 0x7d, 0x54, 0x75, 0x53, 0x61, 0x10, 0x20, 0xf7};
	for (const auto* v : {&unknown, &product})
	{
		const std::string path = std::string(dir) + "/type" + std::to_string(g_n++) + ".syx";
		FILE* f = fopen(path.c_str(), "wb");
		fwrite(v->data(), 1, v->size(), f);
		fclose(f);
	}
	return 0;
}
