/* This file contains declaration of varius utility functions used in the software */

#ifndef util_h
#define util_h

#include <stdio.h>
#include <time.h>
#include <string.h>

#include "definitions.h"        // Including definition for time-conversion
#include "err_cleanup.h"        // Including definition for error management

unsigned long time_difference_ns(stimespec*, stimespec*);   // Returns the difference of time, in ns, between the two time-stamps
short compare_time(stimespec*, stimespec*);     // Returns -1 if first>second, 0 if first=second, 1 if first<second
short compare_current_time(const stimespec*);     // Returns -1 if current_time>arg, 0 if current_time=arg, 1 if current_time<arg
short nanosleep_w(stimespec*);      // Wrapper for the nanosleep function

#endif  // util_h