/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#pragma once

#include <uavcan_stm32/build_config.hpp>
#include <uavcan/driver/system_clock.hpp>

namespace uavcan_stm32
{

namespace clock
{
/**
 * Starts the clock.
 * Can be called multiple times, only the first call will be effective.
 */
void init();

/**
 * Returns current monotonic time passed since the moment when clock::init() was called.
 * Note that both monotonic and UTC clocks are implemented using SysTick timer.
 */
uavcan::MonotonicTime getMonotonic();

/**
 * Returns UTC time if it has been set, otherwise returns zero time.
 * Note that both monotonic and UTC clocks are implemented using SysTick timer.
 */
uavcan::UtcTime getUtc();

/**
 * Performs UTC time adjustment.
 * The UTC time will be zero until first adjustment has been performed.
 */
void adjustUtc(uavcan::UtcDuration adjustment);

/**
 * Returns clock error sampled at previous UTC adjustment.
 * Positive if the hardware timer is slower than reference time.
 */
uavcan::UtcDuration getPrevUtcAdjustment();

}

/**
 * Adapter for uavcan::ISystemClock.
 */
class SystemClock : public uavcan::ISystemClock, uavcan::Noncopyable
{
    static SystemClock self;

    SystemClock() { }

    virtual uavcan::MonotonicTime getMonotonic()     const { return clock::getMonotonic(); }
    virtual uavcan::UtcTime getUtc()                 const { return clock::getUtc(); }
    virtual void adjustUtc(uavcan::UtcDuration adjustment) { clock::adjustUtc(adjustment); }

public:
    /**
     * Calls clock::init() as needed.
     */
    static SystemClock& instance();
};

}
