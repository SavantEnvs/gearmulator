// synthLib::VstPreset::read: VST2 fxp/fxb and VST3 .vstpreset container parsers.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "synthLib/vstpreset.h"
#include "quiet.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* _data, size_t _size)
{
	if (!_size)
		return -1;

	const std::vector<uint8_t> file(_data, _data + _size);
	const auto chunks = synthLib::VstPreset::read(file);

	if (!chunks)
		return 0;

	size_t total = 0;
	for (const auto& c : chunks.value())
		total += c.data.size() + c.id.size();
	volatile size_t sink = total;
	(void)sink;
	return 0;
}
