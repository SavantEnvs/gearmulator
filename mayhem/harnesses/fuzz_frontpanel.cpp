// synthLib::FrontPanelTransport::receive: the F0 7D 54 75 53 front panel sysex frames (LCD text/bitmap,
// LEDs, buttons, encoders, parameters). Every event is listened to so the decoded payloads are consumed.
#include <cstddef>
#include <cstdint>

#include "synthLib/frontPanelSysex.h"
#include "synthLib/midiTypes.h"
#include "quiet.h"

namespace
{
	volatile size_t g_sink;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* _data, size_t _size)
{
	if (!_size)
		return -1;

	synthLib::FrontPanelTransport t;

	t.evLcdText.addListener([](uint32_t _c, uint32_t _r, const std::vector<uint8_t>& _cells)
	{
		size_t s = _c + _r;
		for (const auto b : _cells)
			s += b;
		g_sink = s;
	});
	t.evLcdCgRam.addListener([](const std::array<uint8_t, synthLib::fp::g_cgRamSize>& _a)
	{
		size_t s = 0;
		for (const auto b : _a)
			s += b;
		g_sink = s;
	});
	t.evLcdBitmap.addListener([](uint32_t _w, uint32_t _h, const std::vector<uint8_t>& _px)
	{
		size_t s = _w + _h;
		for (const auto b : _px)
			s += b;
		g_sink = s;
	});
	t.evLedMask.addListener([](const std::vector<bool>& _leds)
	{
		size_t s = 0;
		for (const bool b : _leds)
			s += b ? 1 : 0;
		g_sink = s;
	});
	t.evButton.addListener([](uint32_t _i, bool _p) { g_sink = _i + (_p ? 1 : 0); });
	t.evEncoder.addListener([](uint32_t _i, int32_t _d) { g_sink = _i + static_cast<size_t>(_d); });
	t.evParam.addListener([](uint8_t _p, uint8_t _i, int32_t _v) { g_sink = _p + _i + static_cast<size_t>(_v); });
	t.evRequestState.addListener([](uint8_t _m) { g_sink = _m; });
	t.evProduct.addListener([](uint8_t _t, const synthLib::SysexBuffer& _f) { g_sink = _t + _f.size(); });

	const synthLib::SysexBuffer frame(_data, _data + _size);
	(void)t.receive(frame);
	return 0;
}
