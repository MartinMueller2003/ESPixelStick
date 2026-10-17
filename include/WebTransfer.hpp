#pragma once

#include <stddef.h>

// A response may outlive a config reload which shrinks the output buffer.
inline size_t DiagnosticChunkSize(size_t requested, size_t responseSize, size_t currentSize, size_t index)
{
    const size_t available = responseSize < currentSize ? responseSize : currentSize;
    if (index >= available) { return 0; }
    const size_t remaining = available - index;
    return requested < remaining ? requested : remaining;
}
