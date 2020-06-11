/*
 * Copyright (c) 2019 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 * Licensed under the Apache License, Version 2.0);
 */

#ifndef __HELIOS_Si7021_H__
#define __HELIOS_Si7021_H__

class HELIOS_Si7021 {
public:
	HELIOS_Si7021(PinName sda, PinName scl);
	~HELIOS_Si7021();

	void reset(void);
	void readSerialNumber(void);
	uint8_t getRevision(void) { return _revision; };
	bool hasSensor(void) { return _foundDevice; };
	
	float readTemperature(void);
	float readHumidity(void);
	
	enum sensorType {
		SI_Engineering_Samples,
		SI_7013,
		SI_7020,
		SI_7021,
		SI_unkown,
	};
	const char *getModelName(void);
	sensorType getModel(void);
	
private:
    I2C *_i2c;
	char _data[8];

	bool _foundDevice;
	void _readRevision(void);
	uint32_t sernum_a, sernum_b;
	sensorType _model;
	uint8_t _revision;
	uint8_t _readRegister8(uint8_t reg);
	uint16_t _readRegister16(uint8_t reg);
	void _writeRegister8(uint8_t reg, uint8_t value);
	const static uint8_t _i2caddr = 0x40 << 1; // convert from 7 to 8 bit.
	const static int _TRANSACTION_TIMEOUT = 100; // Wire NAK/Busy timeout in ms
};

/**************************************************************************/

#endif // __HELIOS_Si7021_H__

