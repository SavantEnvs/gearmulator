// synthLib::midi::readSmf: Standard MIDI File reader.
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

	if (!synthLib::midi::readSmf(file, events, error))
		return 0;

	double last = 0;
	size_t bytes = 0;
	for (const auto& e : events)
	{
		last += e.seconds;
		bytes += e.bytes.size();
	}
	volatile size_t sink = bytes + static_cast<size_t>(last);
	(void)sink;
	return 0;
}
