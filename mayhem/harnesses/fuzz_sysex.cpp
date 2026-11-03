// synthLib::MidiToSysex: sysex extraction from raw dumps and MIDI file data, framing cleanup, and the
// SysexToMidi writer that serialises the extracted messages again.
#include <cstddef>
#include <cstdint>
#include <sstream>

#include "synthLib/midiToSysex.h"
#include "synthLib/sysexToMidi.h"
#include "quiet.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* _data, size_t _size)
{
	if (!_size)
		return -1;

	const synthLib::SysexBuffer buf(_data, _data + _size);

	synthLib::SysexBufferList messages;
	const bool extracted = synthLib::MidiToSysex::extractSysexFromData(messages, buf);

	synthLib::SysexBufferList rawSplit;
	synthLib::MidiToSysex::splitMultipleSysex(rawSplit, buf, false);

	synthLib::SysexBufferList fileSplit;
	synthLib::MidiToSysex::splitMultipleSysex(fileSplit, buf, true);

	synthLib::SysexBuffer framed(buf);
	synthLib::MidiToSysex::removeDuplicateFraming(framed);

	if (extracted)
	{
		std::ostringstream out;
		synthLib::SysexToMidi::write(out, messages);
		volatile size_t sink = out.str().size();
		(void)sink;
	}
	return 0;
}
