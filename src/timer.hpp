#pragma once

#include <sys/time.h>

namespace loom
{

typedef uint64_t usec_t;

inline double get_time_sec (const timeval & t)
{
    return t.tv_sec + 1e-6 * t.tv_usec;
}

inline usec_t get_time_usec (timeval & t)
{
    return 1000000UL * t.tv_sec + t.tv_usec;
}

inline usec_t current_time_usec ()
{
    timeval t;
    gettimeofday(&t, NULL);
    return get_time_usec(t);
}

class TimedScope
{
    usec_t & time_;

public:

    TimedScope  (usec_t & time) :
        time_(time)
    {
        time_ -= current_time_usec();
    }

    ~TimedScope ()
    {
        time_ += current_time_usec();
    }
};

} // namespace loom
