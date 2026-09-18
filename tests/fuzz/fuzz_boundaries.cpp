// libFuzzer harness for the multipart/form-data body parser
// (src/Request/Boundaries.cpp) - the same class of hand-written,
// byte-level state machine as Chunks.cpp, parsing untrusted client
// input directly. Unlike Chunks, Boundaries has no discard/dry-run
// mode (no equivalent of Chunks::discardFromNowOn()), so this writes
// real (small) files under a scratch directory for the run's
// lifetime - point it at a tmpfs and clean up after a long local run.
// Build: cmake -DWEBSERV_FUZZ=ON (needs Clang).
// Run: ./build/fuzz_boundaries -max_total_time=60 corpus/

#include "Boundaries.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/stat.h>

namespace
{
    const string kBoundary = "----WebKitFormBoundaryFUZZ";
    string scratchDir;
}

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    scratchDir = "/tmp/fuzz_boundaries_scratch/";
    mkdir(scratchDir.c_str(), 0700);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Boundaries boundaries;
    boundaries.setBoundaries(kBoundary, scratchDir, 1u << 30);

    size_t offset = 0;
    // Real callers stop as soon as parseBoundary()'s callee throws
    // (success or a real error). Boundaries has no equivalent of
    // Chunks::parse()'s "next suggested read size" return value, so
    // this just walks the input in fixed BUFFER_SIZE slices like a
    // normal socket read would deliver.
    while (offset < size)
    {
        size_t take = std::min(static_cast<size_t>(BUFFER_SIZE), size - offset);
        string piece(reinterpret_cast<const char *>(data + offset), take);
        try
        {
            boundaries.parseBoundary(piece, static_cast<int>(take));
        }
        catch (int)
        {
            break;
        }
        offset += take;
    }
    return 0;
}
