/*
INTEL CONFIDENTIAL
Copyright 2017 Intel Corporation

The source code contained or described herein and all documents related to the
source code ("Material") are owned by Intel Corporation or its suppliers or
licensors. Title to the Material remains with Intel Corporation or its
suppliers and licensors. The Material contains trade secrets and proprietary
and confidential information of Intel or its suppliers and licensors. The
Material is protected by worldwide copyright and trade secret laws and treaty
provisions. No part of the Material may be used, copied, reproduced, modified,
published, uploaded, posted, transmitted, distributed, or disclosed in any way
without Intel's prior express written permission.

No license under any patent, copyright, trade secret or other intellectual
property right is granted to or conferred upon you by disclosure or delivery of
the Materials, either expressly, by implication, inducement, estoppel or
otherwise. Any license under such intellectual property rights must be express
and approved by Intel in writing.
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
