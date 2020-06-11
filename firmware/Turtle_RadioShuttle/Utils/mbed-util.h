/*
 * Copyright (c) 2019 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 * Licensed under the Apache License, Version 2.0);
 */


#ifndef __MBED_UTIL_H__
#define __MBED_UTIL_H__


extern int CPUID(uint8_t *buf, int maxSize, uint32_t value);
extern float BatteryVoltage(void);
extern void OTPWrite(uint8_t *address, const void *d, size_t length);




#endif // __MBED_UTIL_H__
