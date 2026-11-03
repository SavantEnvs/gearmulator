// synthLib::MidiBufferParser (raw MIDI byte stream to events, with running status and sysex reassembly) and
// MidiTranslator (channel translation plus its F0 60 remote-control sysex commands) on the parsed events.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "synthLib/midiBufferParser.h"
#include "synthLib/midiTranslator.h"
#include "synthLib/midiTypes.h"
#include "quiet.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* _data, size_t _size)
{
	if (!_size)
		return -1;

	const std::vector<uint8_t> bytes(_data, _data + _size);

	synthLib::MidiBufferParser whole(synthLib::MidiEventSource::Host);
	whole.write(bytes);
	std::vector<synthLib::SMidiEvent> events;
	whole.getEvents(events);

	synthLib::MidiBufferParser split(synthLib::MidiEventSource::Host);
	std::vector<synthLib::SMidiEvent> splitEvents;
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		split.write(bytes[i]);
		if ((i & 7) == 7)
			split.getEvents(splitEvents);
	}
	split.getEvents(splitEvents);

	synthLib::MidiTranslator translator;
	std::vector<synthLib::SMidiEvent> translated;
	for (const auto& e : events)
		translator.process(translated, e);

	volatile size_t sink = translated.size() + splitEvents.size();
	(void)sink;
	return 0;
}
