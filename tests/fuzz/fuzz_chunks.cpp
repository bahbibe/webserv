// libFuzzer harness for the chunked-transfer-encoding body parser
// (src/Request/Chunks.cpp) - a hand-written byte-level state machine
// parsing untrusted client input, the highest-risk parsing surface
// in the request path. Build: cmake -DWEBSERV_FUZZ=ON (needs Clang).
// Run: ./build/fuzz_chunks -max_total_time=60 corpus/

#include "Chunks.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Chunks chunks;
    // NULL outfile: parser writes nowhere, so this only exercises
    // chunk-framing state, never touches the filesystem.
    chunks.setChunks(NULL, "", 1024 * 1024);

    size_t offset = 0;
    int nextSize = BUFFER_SIZE;
    // Real callers stop as soon as parse() throws (success or a
    // real error). Cap iterations so a pathological input can't look
    // like a hang to the fuzzer - this loop has no equivalent in
    // production code, it's purely a fuzz-harness safety net.
    for (int i = 0; i < 10000 && offset < size && nextSize > 0; i++)
    {
        size_t take = std::min(static_cast<size_t>(nextSize), size - offset);
        string piece(reinterpret_cast<const char *>(data + offset), take);
        try
        {
            nextSize = chunks.parse(piece, static_cast<int>(take));
        }
        catch (int)
        {
            break;
        }
        offset += take;
    }
    return 0;
}
