/*
Copyright 2017-2018 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <windows.h>

struct PerfTimerFrequency {
    uint64_t Numerator;
    enum { Denominator = 1 };
};

inline uint64_t GetPerfTimerCount()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

inline PerfTimerFrequency GetPerfTimerFrequency()
{
    PerfTimerFrequency f;
    QueryPerformanceFrequency((LARGE_INTEGER*) &f.Numerator);
    return f;
}
#else // ifdef _WIN32
#include <mach/mach_time.h>
#include <stdint.h>

struct PerfTimerFrequency {
    uint64_t Numerator;
    uint32_t Denominator;
};

inline uint64_t GetPerfTimerCount()
{
    return mach_absolute_time();
}

inline PerfTimerFrequency GetPerfTimerFrequency()
{
    mach_timebase_info_data_t i;
    mach_timebase_info(&i);

    PerfTimerFrequency f;
    f.Numerator = 1000000000ull * i.denom;
    f.Denominator = i.numer;
    return f;
}
#endif // ifdef _WIN32
