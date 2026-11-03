// Known-answer tests for the parsers the fuzz targets cover: inputs built byte by byte here, outputs asserted
// value by value. Prints "<failed> of <checks> checks failed" like upstream's own test programs; mayhem/test.sh
// parses that line. Built with normal flags (no sanitizers) by mayhem/build.sh.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "baseLib/logging.h"
#include "synthLib/frontPanelSysex.h"
#include "synthLib/midi/midiFile.h"
#include "synthLib/midiToSysex.h"
#include "synthLib/midiTranslator.h"
#include "synthLib/midiTypes.h"
#include "synthLib/sysexToMidi.h"
#include "synthLib/vstpreset.h"
#include "synthLib/wavReader.h"

namespace
{
	int g_checks = 0;
	int g_failures = 0;

	void check(const bool _condition, const char* _test, const char* _what)
	{
		++g_checks;
		if (_condition)
			return;
		++g_failures;
		printf("FAILED %s: %s\n", _test, _what);
	}

	#define CHECK(_test, _condition) check((_condition), _test, #_condition)

	using Bytes = std::vector<uint8_t>;

	void put(Bytes& _b, const std::string& _s) { _b.insert(_b.end(), _s.begin(), _s.end()); }
	void be16(Bytes& _b, const uint16_t _v) { _b.push_back(_v >> 8); _b.push_back(_v & 0xff); }
	void be32(Bytes& _b, const uint32_t _v) { be16(_b, _v >> 16); be16(_b, _v & 0xffff); }
	void le16(Bytes& _b, const uint16_t _v) { _b.push_back(_v & 0xff); _b.push_back(_v >> 8); }
	void le32(Bytes& _b, const uint32_t _v) { le16(_b, _v & 0xffff); le16(_b, _v >> 16); }
	void le64(Bytes& _b, const uint64_t _v) { le32(_b, static_cast<uint32_t>(_v)); le32(_b, static_cast<uint32_t>(_v >> 32)); }
	void append(Bytes& _a, const Bytes& _b) { _a.insert(_a.end(), _b.begin(), _b.end()); }

	// ---------------------------------------------------------------- standard MIDI file
	Bytes smf(const Bytes& _track, const uint16_t _division = 480)
	{
		Bytes f;
		put(f, "MThd");
		be32(f, 6);
		be16(f, 0);
		be16(f, 1);
		be16(f, _division);
		put(f, "MTrk");
		be32(f, static_cast<uint32_t>(_track.size()));
		append(f, _track);
		return f;
	}

	void testSmf()
	{
		// 500000 us per quarter note at 480 ticks per quarter note: 480 ticks are half a second
		const Bytes track = {
			0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20,
			0x00, 0x90, 60, 100,
			0x83, 0x60, 0x80, 60, 0,	// delta 480
			0x00, 0xff, 0x2f, 0x00};

		std::vector<synthLib::midi::Event> events;
		std::string error;
		CHECK("smf parses", synthLib::midi::readSmf(smf(track), events, error));

		const synthLib::midi::Event* on = nullptr;
		const synthLib::midi::Event* off = nullptr;
		for (const auto& e : events)
		{
			if (!e.bytes.empty() && e.bytes[0] == 0x90)
				on = &e;
			if (!e.bytes.empty() && e.bytes[0] == 0x80)
				off = &e;
		}
		CHECK("smf note on found", on != nullptr);
		CHECK("smf note off found", off != nullptr);
		if (on && off)
		{
			CHECK("smf note on bytes", on->bytes == Bytes({0x90, 60, 100}));
			CHECK("smf note off bytes", off->bytes == Bytes({0x80, 60, 0}));
			CHECK("smf note on time", std::fabs(on->seconds) < 1e-6);
			CHECK("smf note off time", std::fabs(off->seconds - 0.5) < 1e-6);
		}

		const Bytes sysexTrack = {0x00, 0xf0, 0x05, 0x7e, 0x7f, 0x09, 0x01, 0xf7, 0x00, 0xff, 0x2f, 0x00};
		events.clear();
		CHECK("smf sysex parses", synthLib::midi::readSmf(smf(sysexTrack), events, error));
		bool foundSysex = false;
		for (const auto& e : events)
			foundSysex |= e.bytes == Bytes({0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7});
		CHECK("smf sysex event", foundSysex);

		events.clear();
		CHECK("smf rejects text", !synthLib::midi::readSmf(Bytes({'h', 'e', 'l', 'l', 'o'}), events, error));
		CHECK("smf rejects empty", !synthLib::midi::readSmf(Bytes(), events, error));
		CHECK("smf rejects truncated header", !synthLib::midi::readSmf(Bytes({'M', 'T', 'h', 'd', 0, 0, 0, 6}), events, error));
	}

	// ---------------------------------------------------------------- RCP
	void testRcp()
	{
		Bytes f(0x586, 0);
		memcpy(f.data(), "RCM-PC98V2.0(C)COME ON MUSIC", 28);
		f[0x1c0] = 48;		// time base
		f[0x1c1] = 120;		// tempo
		f[0x1c2] = 4;
		f[0x1c3] = 4;
		f[0x1e6] = 1;		// one track

		Bytes track(0x2c, 0);
		const Bytes events = {60, 48, 40, 100,  0xfe, 0, 0, 0};
		const uint16_t length = static_cast<uint16_t>(0x2c + events.size());
		track[0] = length & 0xff;
		track[1] = length >> 8;
		track[4] = 0;		// channel 1
		append(f, track);
		append(f, events);

		CHECK("rcp is v2", synthLib::midi::isRcpV2(f));
		CHECK("empty is not rcp", !synthLib::midi::isRcpV2(Bytes()));

		std::vector<synthLib::midi::Event> out;
		std::string error;
		CHECK("rcp parses", synthLib::midi::readRcp(f, out, error));

		bool noteOn = false;
		for (const auto& e : out)
			noteOn |= e.bytes.size() == 3 && (e.bytes[0] & 0xf0) == 0x90 && e.bytes[1] == 60;
		CHECK("rcp note on", noteOn);

		out.clear();
		CHECK("rcp rejects smf", !synthLib::midi::readRcp(smf(Bytes({0x00, 0xff, 0x2f, 0x00})), out, error));
		CHECK("rcp rejects truncated header", !synthLib::midi::readRcp(Bytes(f.begin(), f.begin() + 0x100), out, error));
	}

	// ---------------------------------------------------------------- VST presets
	Bytes fxpOpaque(const Bytes& _chunk)
	{
		Bytes body;
		put(body, "FPCh");
		be32(body, 1);
		put(body, "GEAR");
		be32(body, 1);
		be32(body, 0);
		Bytes name(28, 0);
		memcpy(name.data(), "Init", 4);
		append(body, name);
		be32(body, static_cast<uint32_t>(_chunk.size()));
		append(body, _chunk);

		Bytes f;
		put(f, "CcnK");
		be32(f, static_cast<uint32_t>(body.size()));
		append(f, body);
		return f;
	}

	void testVstPreset()
	{
		Bytes payload;
		for (uint8_t i = 0; i < 40; ++i)
			payload.push_back(static_cast<uint8_t>(i * 3));

		const auto fxp = synthLib::VstPreset::read(fxpOpaque(payload));
		CHECK("fxp parses", fxp.has_value());
		if (fxp)
		{
			CHECK("fxp one chunk", fxp->size() == 1);
			if (!fxp->empty())
				CHECK("fxp chunk data", fxp->front().data == payload);
		}

		// a bank of two opaque chunks
		Bytes bank;
		put(bank, "FBCh");
		be32(bank, 2);
		put(bank, "GEAR");
		be32(bank, 1);
		be32(bank, 2);
		be32(bank, 0);
		bank.insert(bank.end(), 124, 0);
		be32(bank, 3);
		put(bank, "abc");
		be32(bank, 2);
		put(bank, "yz");
		Bytes fxb;
		put(fxb, "CcnK");
		be32(fxb, static_cast<uint32_t>(bank.size()));
		append(fxb, bank);

		const auto banks = synthLib::VstPreset::read(fxb);
		CHECK("fxb parses", banks.has_value());
		if (banks && banks->size() == 2)
		{
			CHECK("fxb first chunk", (*banks)[0].data == Bytes({'a', 'b', 'c'}));
			CHECK("fxb second chunk", (*banks)[1].data == Bytes({'y', 'z'}));
		}
		else
			CHECK("fxb two chunks", false);

		// VST3: header, class id, offset of the chunk list, data, "List" with one "Comp" entry
		Bytes v3;
		put(v3, "VST3");
		le32(v3, 1);
		put(v3, std::string(32, 'A'));
		const uint64_t dataStart = v3.size() + 8;
		le64(v3, dataStart + payload.size());
		append(v3, payload);
		put(v3, "List");
		le32(v3, 1);
		put(v3, "Comp");
		le64(v3, dataStart);
		le64(v3, payload.size());

		const auto vst3 = synthLib::VstPreset::read(v3);
		CHECK("vst3 parses", vst3.has_value());
		if (vst3 && vst3->size() == 1)
		{
			CHECK("vst3 chunk id", std::string((*vst3)[0].id.data()) == "Comp");
			CHECK("vst3 chunk data", (*vst3)[0].data == payload);
		}
		else
			CHECK("vst3 one chunk", false);

		CHECK("preset rejects text", !synthLib::VstPreset::read(Bytes({'n', 'o', 'p', 'e', '!'})).has_value());
		CHECK("preset rejects empty", !synthLib::VstPreset::read(Bytes()).has_value());
		const Bytes whole = fxpOpaque(payload);
		CHECK("preset rejects truncated fxp", !synthLib::VstPreset::read(Bytes(whole.begin(), whole.begin() + 40)).has_value());
	}

	// ---------------------------------------------------------------- WAV
	Bytes wav(const uint16_t _tag, const uint16_t _channels, const uint32_t _rate, const uint16_t _bits, const Bytes& _data, const Bytes& _extra = {})
	{
		Bytes body;
		put(body, "WAVE");
		put(body, "fmt ");
		le32(body, 16);
		le16(body, _tag);
		le16(body, _channels);
		le32(body, _rate);
		le32(body, _rate * _channels * _bits / 8);
		le16(body, static_cast<uint16_t>(_channels * _bits / 8));
		le16(body, _bits);
		append(body, _extra);
		put(body, "data");
		le32(body, static_cast<uint32_t>(_data.size()));
		append(body, _data);

		Bytes f;
		put(f, "RIFF");
		le32(f, static_cast<uint32_t>(body.size()));
		append(f, body);
		return f;
	}

	void testWav()
	{
		Bytes pcm;
		for (uint8_t i = 0; i < 16; ++i)
			pcm.push_back(static_cast<uint8_t>(0x10 + i));

		const Bytes f = wav(1, 2, 48000, 16, pcm);
		synthLib::Data d;
		std::vector<synthLib::CuePoint> cues;
		CHECK("wav parses", synthLib::WavReader::load(d, &cues, f.data(), f.size()));
		CHECK("wav sample rate", d.samplerate == 48000);
		CHECK("wav channels", d.channels == 2);
		CHECK("wav bits", d.bitsPerSample == 16);
		CHECK("wav not float", !d.isFloat);
		CHECK("wav data size", d.dataByteSize == pcm.size());
		CHECK("wav data pointer", d.data && memcmp(d.data, pcm.data(), pcm.size()) == 0);
		CHECK("wav no cues", cues.empty());

		const Bytes g = wav(3, 1, 96000, 32, Bytes(32, 0x3f));
		synthLib::Data fd;
		CHECK("float wav parses", synthLib::WavReader::load(fd, nullptr, g.data(), g.size()));
		CHECK("float wav is float", fd.isFloat);
		CHECK("float wav rate", fd.samplerate == 96000);
		CHECK("float wav bits", fd.bitsPerSample == 32);
		CHECK("float wav data size", fd.dataByteSize == 32);

		// a cue point at play position 4, labelled "loop"
		Bytes cue;
		le32(cue, 1);
		le32(cue, 1);		// id
		le32(cue, 4);		// play order position
		put(cue, "data");
		le32(cue, 0);
		le32(cue, 0);
		le32(cue, 4);
		Bytes extra;
		put(extra, "cue ");
		le32(extra, static_cast<uint32_t>(cue.size()));
		append(extra, cue);
		Bytes list;
		put(list, "adtl");
		put(list, "labl");
		le32(list, 4 + 5);
		le32(list, 1);
		put(list, std::string("loop\0", 5));
		list.push_back(0);	// pad to even
		put(extra, "LIST");
		le32(extra, static_cast<uint32_t>(list.size()));
		append(extra, list);

		const Bytes h = wav(1, 1, 22050, 16, pcm, extra);
		synthLib::Data cd;
		std::vector<synthLib::CuePoint> cuePoints;
		CHECK("cue wav parses", synthLib::WavReader::load(cd, &cuePoints, h.data(), h.size()));
		CHECK("cue wav data size", cd.dataByteSize == pcm.size());
		CHECK("cue count", cuePoints.size() == 1);
		if (cuePoints.size() == 1)
		{
			CHECK("cue offset", cuePoints[0].sampleOffset == 4);
			CHECK("cue label", cuePoints[0].name == "loop");
		}

		const Bytes adpcm = wav(2, 1, 8000, 4, Bytes(8, 0));
		synthLib::Data ad;
		CHECK("wav rejects adpcm", !synthLib::WavReader::load(ad, nullptr, adpcm.data(), adpcm.size()));
		const Bytes junk = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'A', 'V', 'I', ' '};
		CHECK("wav rejects avi", !synthLib::WavReader::load(ad, nullptr, junk.data(), junk.size()));
		CHECK("wav rejects tiny", !synthLib::WavReader::load(ad, nullptr, junk.data(), 3));
	}

	// ---------------------------------------------------------------- sysex
	synthLib::SysexBuffer sx(const Bytes& _b) { return synthLib::SysexBuffer(_b.begin(), _b.end()); }

	void testSysex()
	{
		const Bytes a = {0xf0, 0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00, 0xf7};
		const Bytes b = {0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7};
		Bytes both = a;
		append(both, b);

		synthLib::SysexBufferList messages;
		CHECK("extract two", synthLib::MidiToSysex::extractSysexFromData(messages, sx(both)));
		CHECK("extract count", messages.size() == 2);
		if (messages.size() == 2)
		{
			CHECK("extract first", messages[0] == sx(a));
			CHECK("extract second", messages[1] == sx(b));
		}

		synthLib::SysexBufferList split;
		synthLib::MidiToSysex::splitMultipleSysex(split, sx(both));
		CHECK("split count", split.size() == 2);
		if (split.size() == 2)
			CHECK("split second", split[1] == sx(b));

		Bytes doubled = {0xf0};
		append(doubled, a);
		doubled.push_back(0xf7);
		synthLib::SysexBuffer framed = sx(doubled);
		synthLib::MidiToSysex::removeDuplicateFraming(framed);
		CHECK("duplicate framing removed", framed == sx(a));

		// written as a MIDI file and read back
		std::ostringstream out;
		synthLib::SysexToMidi::write(out, messages);
		const std::string file = out.str();
		CHECK("midi file written", file.size() > 14 && file.compare(0, 4, "MThd") == 0);

		synthLib::SysexBufferList readBack;
		const bool ok = synthLib::MidiToSysex::extractSysexFromData(readBack, synthLib::SysexBuffer(file.begin(), file.end()));
		CHECK("midi file read back", ok);
		CHECK("midi file round trip", readBack == messages);

		synthLib::SysexBufferList none;
		CHECK("no sysex in notes", !synthLib::MidiToSysex::extractSysexFromData(none, sx({0x90, 0x3c, 0x40})));
	}

	// ---------------------------------------------------------------- MIDI translator
	void testTranslator()
	{
		using namespace synthLib;
		MidiTranslator t;
		std::vector<SMidiEvent> out;

		SMidiEvent note(MidiEventSource::Host, 0x90, 60, 100);
		t.process(out, note);
		CHECK("translator passes a note", out.size() == 1 && out[0].a == 0x90 && out[0].b == 60);

		out.clear();
		t.clear();
		t.addTargetChannel(0, 5);
		t.process(out, note);
		CHECK("translator retargets channel", out.size() == 1 && out[0].a == 0x95);

		out.clear();
		t.addTargetChannel(0, 7);
		t.process(out, note);
		CHECK("translator fans out", out.size() == 2);
	}

	// ---------------------------------------------------------------- front panel
	struct PanelLog
	{
		bool lcd = false;
		uint32_t cols = 0, rows = 0;
		std::vector<uint8_t> cells;
		std::vector<bool> leds;
		uint32_t buttonIndex = 0;
		bool pressed = false;
		bool button = false;
		uint32_t encoderIndex = 0;
		int32_t delta = 0;
		uint8_t page = 0, index = 0;
		int32_t value = 0;
		bool param = false;
		uint8_t mask = 0xff;
		bool request = false;
	};

	void testFrontPanel()
	{
		using namespace synthLib;
		FrontPanelTransport t;
		PanelLog log;
		t.evLcdText.addListener([&](uint32_t c, uint32_t r, const std::vector<uint8_t>& cells) { log.lcd = true; log.cols = c; log.rows = r; log.cells = cells; });
		t.evLedMask.addListener([&](const std::vector<bool>& leds) { log.leds = leds; });
		t.evButton.addListener([&](uint32_t i, bool p) { log.button = true; log.buttonIndex = i; log.pressed = p; });
		t.evEncoder.addListener([&](uint32_t i, int32_t d) { log.encoderIndex = i; log.delta = d; });
		t.evParam.addListener([&](uint8_t p, uint8_t i, int32_t v) { log.param = true; log.page = p; log.index = i; log.value = v; });
		t.evRequestState.addListener([&](uint8_t m) { log.request = true; log.mask = m; });

		std::vector<SMidiEvent> frames;
		std::vector<uint8_t> cells(32);
		for (size_t i = 0; i < cells.size(); ++i)
			cells[i] = static_cast<uint8_t>(0x20 + i * 3);
		fp::sendLcdText(frames, 16, 2, cells.data());
		fp::sendLedMask(frames, std::vector<bool>{true, false, true, true, false, false, true, true, true, false});
		fp::sendButton(frames, 5, true);
		fp::sendEncoder(frames, 3, -7);
		fp::sendParam(frames, 2, 17, -123456);
		fp::sendRequestState(frames, 0x05);

		bool allFrontPanel = true;
		for (const auto& f : frames)
			allFrontPanel &= fp::isFrontPanel(f.sysex) && t.receive(f.sysex);
		CHECK("every frame is recognised", allFrontPanel && frames.size() == 6);

		CHECK("lcd size", log.lcd && log.cols == 16 && log.rows == 2);
		CHECK("lcd cells", log.cells == cells);
		CHECK("led mask", log.leds == std::vector<bool>({true, false, true, true, false, false, true, true, true, false}));
		CHECK("button", log.button && log.buttonIndex == 5 && log.pressed);
		CHECK("encoder", log.encoderIndex == 3 && log.delta == -7);
		CHECK("param", log.param && log.page == 2 && log.index == 17 && log.value == -123456);
		CHECK("request state", log.request && log.mask == 0x05);

		CHECK("other sysex is not front panel", !t.receive(sx({0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7})));
		CHECK("unknown type is swallowed", t.receive(sx({0xf0, 0x7d, 0x54, 0x75, 0x53, 0x5f, 0x01, 0xf7})));
		CHECK("short frame is swallowed", t.receive(sx({0xf0, 0x7d, 0x54, 0x75, 0x53, 0x04, 0x01, 0xf7})));

		// pack7/unpack7: 7 bytes become 8
		const Bytes raw = {0xff, 0x00, 0x80, 0x7f, 0x81, 0xaa, 0x55, 0xc3, 0x01};
		SysexBuffer packed;
		fp::pack7(packed, raw.data(), raw.size());
		CHECK("pack7 size", packed.size() == fp::packedSize(raw.size()) && packed.size() == 16);	// the last group is padded to 8
		bool sevenBit = true;
		for (const auto b : packed)
			sevenBit &= b < 0x80;
		CHECK("pack7 is 7-bit clean", sevenBit);
		std::vector<uint8_t> unpacked;
		fp::unpack7(unpacked, packed.data(), raw.size());
		CHECK("pack7 round trip", unpacked == raw);
	}
}

int main()
{
	baseLib::logging::setLogFunc([](const std::string&) {});

	testSmf();
	testRcp();
	testVstPreset();
	testWav();
	testSysex();
	testTranslator();
	testFrontPanel();

	printf("%d of %d checks failed\n", g_failures, g_checks);
	return g_failures ? 1 : 0;
}
