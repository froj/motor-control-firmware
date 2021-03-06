/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#include <uavcan_stm32/clock.hpp>
#include <uavcan_stm32/thread.hpp>
#include "internal.hpp"
#include <ch.h>

namespace uavcan_stm32
{
namespace clock
{
namespace
{

bool initialized = false;
bool utc_set = false;

int32_t utc_correction_usec_per_overflow_x16 = 0;
int64_t prev_adjustment = 0;

uint64_t time_mono = 0;
uint64_t time_utc = 0;

/**
 * If this value is too large for the given core clock, reload value will be out of the 24-bit integer range.
 * This will be detected at run time during timer initialization - refer to SysTick_Config().
 */
const uint32_t USecPerOverflow = 65536 * 2;
const int32_t MaxUtcSpeedCorrectionX16 = 100 * 16;

}

static void fail()
{
    chSysHalt("uavcan timer fail");
}

void init()
{
    CriticalSectionLocker lock;
    if (!initialized)
    {
        initialized = true;

        if ((STM32_SYSCLK % 1000000) != 0)  // Core clock frequency validation
        {
            fail();
        }

        if (SysTick_Config((STM32_SYSCLK / 1000000) * USecPerOverflow) != 0)
        {
            fail();
        }
    }
}

static uint64_t sampleFromCriticalSection(const volatile uint64_t* const value)
{
    const uint32_t reload = SysTick->LOAD + 1;  // SysTick counts downwards, hence the value subtracted from reload

    volatile uint64_t time = *value;
    volatile uint32_t cycles = reload - SysTick->VAL;

    if ((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == SCB_ICSR_PENDSTSET_Msk)
    {
        cycles = reload - SysTick->VAL;
        time += USecPerOverflow;
    }
    const uint32_t cycles_per_usec = STM32_SYSCLK / 1000000;
    return time + (cycles / cycles_per_usec);
}

uint64_t getUtcUSecFromCanInterrupt()
{
    return utc_set ? sampleFromCriticalSection(&time_utc) : 0;
}

uavcan::MonotonicTime getMonotonic()
{
    uint64_t usec = 0;
    {
        CriticalSectionLocker locker;
        usec = sampleFromCriticalSection(&time_mono);
    }
    return uavcan::MonotonicTime::fromUSec(usec);
}

uavcan::UtcTime getUtc()
{
    uint64_t usec = 0;
    if (utc_set)
    {
        CriticalSectionLocker locker;
        usec = sampleFromCriticalSection(&time_utc);
    }
    return uavcan::UtcTime::fromUSec(usec);
}

uavcan::UtcDuration getPrevUtcAdjustment()
{
    return uavcan::UtcDuration::fromUSec(prev_adjustment);
}

void adjustUtc(uavcan::UtcDuration adjustment)
{
    const int64_t adj_delta = adjustment.toUSec() - prev_adjustment;  // This is the P term
    prev_adjustment = adjustment.toUSec();

    utc_correction_usec_per_overflow_x16 += adjustment.isPositive() ? 1 : -1; // I
    utc_correction_usec_per_overflow_x16 += (adj_delta > 0) ? 1 : -1;         // P

    utc_correction_usec_per_overflow_x16 =
        uavcan::max(utc_correction_usec_per_overflow_x16, -MaxUtcSpeedCorrectionX16);
    utc_correction_usec_per_overflow_x16 =
        uavcan::min(utc_correction_usec_per_overflow_x16,  MaxUtcSpeedCorrectionX16);

    if (adjustment.getAbs().toMSec() > 9 || !utc_set)
    {
        const int64_t adj_usec = adjustment.toUSec();
        {
            CriticalSectionLocker locker;
            if ((adj_usec < 0) && uint64_t(-adj_usec) > time_utc)
            {
                time_utc = 1;
            }
            else
            {
                time_utc = uint64_t(int64_t(time_utc) + adj_usec);
            }
        }
        if (!utc_set)
        {
            utc_set = true;
            utc_correction_usec_per_overflow_x16 = 0;
        }
    }
}

} // namespace clock

SystemClock SystemClock::self;

SystemClock& SystemClock::instance()
{
    clock::init();
    return self;
}

}

/*
 * Timer interrupt handler
 */
extern "C"
{

CH_IRQ_HANDLER(SysTick_Handler)
{
    CH_IRQ_PROLOGUE();
    using namespace uavcan_stm32::clock;
    if (initialized)
    {
        time_mono += USecPerOverflow;
        if (utc_set)
        {
            // Values below 16 are ignored
            time_utc += uint64_t(int32_t(USecPerOverflow) + (utc_correction_usec_per_overflow_x16 / 16));
        }
    }
    else
    {
        fail();
    }
    CH_IRQ_EPILOGUE();
}

} // namespace uavcan_stm32
