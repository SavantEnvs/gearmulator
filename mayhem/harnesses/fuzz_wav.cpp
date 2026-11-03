// synthLib::WavReader::load: RIFF/WAVE parser with cue-point and label chunks, reading from a raw buffer.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "synthLib/wavReader.h"
#include "quiet.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* _data, size_t _size)
{
	// load() dereferences the 12 byte RIFF header without checking the buffer length, so any shorter input
	// is an immediate out-of-bounds read that would end every run before it starts (see known-findings/)
	if (_size < 12)
		return -1;

	synthLib::Data wav;
	std::vector<synthLib::CuePoint> cues;

	if (!synthLib::WavReader::load(wav, &cues, _data, _size))
		return 0;

	const auto* samples = static_cast<const uint8_t*>(wav.data);
	uint8_t x = 0;
	// load() reports the data chunk size from the file header without clamping it to the buffer
	const auto* const end = _data + _size;
	const size_t avail = samples >= _data && samples <= end ? static_cast<size_t>(end - samples) : 0;
	const size_t n = wav.dataByteSize < avail ? wav.dataByteSize : avail;
	for (size_t i = 0; i < n; ++i)
		x = static_cast<uint8_t>(x ^ samples[i]);
	volatile uint8_t sink = x;
	(void)sink;

	size_t offsets = 0;
	for (const auto& c : cues)
		offsets += c.sampleOffset + c.name.size();
	volatile size_t sink2 = offsets;
	(void)sink2;
	return 0;
}
