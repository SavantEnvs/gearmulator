// synthLib::midi::readRcp: Recomposer RCP/G36 song reader (v1 and v2 headers).
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "synthLib/midi/midiFile.h"
#include "quiet.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* _data, size_t _size)
{
	if (!_size)
		return -1;

	const std::vector<uint8_t> file(_data, _data + _size);
	std::vector<synthLib::midi::Event> events;
	std::string error;

	(void)synthLib::midi::isRcpV2(file);

	if (!synthLib::midi::readRcp(file, events, error))
		return 0;

	size_t bytes = 0;
	for (const auto& e : events)
		bytes += e.bytes.size();
	volatile size_t sink = bytes;
	(void)sink;
	return 0;
}
