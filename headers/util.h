/* This file contains declaration of varius utility functions used in the software */

#ifndef util_h
#define util_h

#include <stdio.h>

#include "definitions.h"    // Including definition for time-conversion

unsigned long time_difference_ns(stimespec*, stimespec*);   // Returns the difference of time, in ns, between the two time-stamps
short compare_time(stimespec*, stimespec*);     // Returns -1 if first>second, 0 if first=second, 1 if first<second

#endif  // util_h