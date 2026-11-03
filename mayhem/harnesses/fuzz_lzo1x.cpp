// baseLib::lzo1xDecompress: the LZO1X decoder used for compressed ROM/firmware blobs.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "baseLib/lzo1x.h"
#include "quiet.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* _data, size_t _size)
{
	if (!_size)
		return -1;

	const std::vector<uint8_t> src(_data, _data + _size);
	std::vector<uint8_t> dst;

	if (!baseLib::lzo1xDecompress(src, dst))
		return 0;

	volatile uint8_t sink = 0;
	for (const auto b : dst)
		sink = static_cast<uint8_t>(sink ^ b);
	return 0;
}
