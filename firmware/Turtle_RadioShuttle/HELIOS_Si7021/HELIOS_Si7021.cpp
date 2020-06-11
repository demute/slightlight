/*
 * Copyright (c) 2019 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 * Licensed under the Apache License, Version 2.0);
 */

#include "main.h"

#ifdef FEATURE_SI7021

#include <HELIOS_Si7021.h>


#define SI7021_MEASRH_HOLD_CMD		0xE5 // Measure Relative Humidity, Hold Master Mode
#define SI7021_MEASRH_NOHOLD_CMD	0xF5 // Measure Relative Humidity, No Hold Master Mode
#define SI7021_MEASTEMP_HOLD_CMD	0xE3 // Measure Temperature, Hold Master Mode
#define SI7021_MEASTEMP_NOHOLD_CMD	0xF3 // Measure Temperature, No Hold Master Mode
#define SI7021_READPREVTEMP_CMD		0xE0 // Read Temperature Value from Previous RH Measurement
#define SI7021_RESET_CMD			0xFE
#define SI7021_WRITERHT_REG_CMD		0xE6 // Write RH/T User Register 1
#define SI7021_READRHT_REG_CMD		0xE7 // Read RH/T User Register 1
#define SI7021_WRITEHEATER_REG_CMD	0x51 // Write Heater Control Register
#define SI7021_READHEATER_REG_CMD	0x11 // Read Heater Control Register
#define SI7021_ID1_CMD				0xFA0F // Read Electronic ID 1st Byte
#define SI7021_ID2_CMD				0xFCC9 // Read Electronic ID 2nd Byte
#define SI7021_FIRMVERS_CMD			0x84B8 // Read Firmware Revision

#define SI7021_REV_1				0xff
#define SI7021_REV_2				0x20



/**************************************************************************/

HELIOS_Si7021::HELIOS_Si7021(PinName sda, PinName scl)
{
	sernum_a = sernum_b = 0;
	_model = SI_7021;
	_revision = 0;
	_foundDevice = false;
	_i2c = new I2C(sda, scl);
	
	reset();
	
	_data[0] = SI7021_READRHT_REG_CMD;
	_i2c->write(_i2caddr, _data, 1);
	_i2c->read(_i2caddr, _data, 1);
	
	if (_data[0] != 0x3A)
		return;
	_foundDevice = true;
	
	readSerialNumber();
	_readRevision();
}


HELIOS_Si7021::~HELIOS_Si7021(void)
{
	if (_i2c)
		delete _i2c;
}


void HELIOS_Si7021::reset(void)
{

	_data[0] = SI7021_RESET_CMD;
	_i2c->write(_i2caddr, _data, 1);
	 wait_ms(50);
}


void HELIOS_Si7021::readSerialNumber(void)
{

	if (!_foundDevice)
		return;

	_data[0] = SI7021_ID1_CMD >> 8;
	_data[1] = SI7021_ID1_CMD & 0xFF;
	_i2c->write(_i2caddr, _data, 2);

	_i2c->read(_i2caddr, _data, 8);
	sernum_a = _data[0];
	sernum_a <<= 8;
	sernum_a |= _data[2];
	sernum_a <<= 8;
	sernum_a |= _data[4];
	sernum_a <<= 8;
	sernum_a |= _data[6];
	


	_data[0] = SI7021_ID2_CMD >> 8;
	_data[1] = SI7021_ID2_CMD & 0xFF;
	_i2c->write(_i2caddr, _data, 2);

	_i2c->read(_i2caddr, _data, 8);
	sernum_b = _data[0];
	sernum_b <<= 8;
	sernum_b |= _data[2];
	sernum_b <<= 8;
	sernum_b |= _data[4];
	sernum_b <<= 8;
	sernum_b |= _data[6];
	
	
	switch(sernum_b >> 24) {
		case 0:
		case 0xff:
			_model = SI_Engineering_Samples;
			break;
		case 0x0D:
			_model = SI_7013;
			break;
		case 0x14:
			_model = SI_7020;
			break;
		case 0x15:
			_model = SI_7021;
			break;
		default:
			_model = SI_unkown;
	}
}


void HELIOS_Si7021::_readRevision(void)
{
	if (!_foundDevice)
		return;

	_revision = 0;

	_data[0] = SI7021_FIRMVERS_CMD >> 8;
	_data[1] = SI7021_FIRMVERS_CMD & 0xFF;
	_i2c->write(_i2caddr, _data, 2);

	_i2c->read(_i2caddr, _data, 2);
	if (_data[0] == SI7021_REV_1) {
          _revision = 1;
	} else if (_data[0] == SI7021_REV_2) {
          _revision = 2;
	}
}


float HELIOS_Si7021::readTemperature(void) {
	if (!_foundDevice)
		return NAN;

	_data[0] = SI7021_MEASTEMP_NOHOLD_CMD;
	_i2c->write(_i2caddr, _data, 1);

	Timer t;
	t.start();
	while(_i2c->read(_i2caddr, _data, 3) != 0) {
		if (t.read_ms() > _TRANSACTION_TIMEOUT)
			return NAN;
		wait_ms(6); // 1/2 typical sample processing time
	}

	float temperature = _data[0] << 8 | _data[1];
	temperature *= 175.72;
	temperature /= 65536;
	temperature -= 46.85;
	
	return temperature;
}


float HELIOS_Si7021::readHumidity(void)
{
	if (!_foundDevice)
		return NAN;

	_data[0] = SI7021_MEASRH_NOHOLD_CMD;
	_i2c->write(_i2caddr, _data, 1);
	
	Timer t;
	t.start();
	while(_i2c->read(_i2caddr, _data, 3) != 0) {
		if (t.read_ms() > _TRANSACTION_TIMEOUT)
			return NAN;
		wait_ms(6); // 1/2 typical sample processing time
	}

	float humidity = (_data[0] << 8 | _data[1]) * 125;
	humidity /= 65536;
	humidity -= 6;

	return humidity;
}


const char *HELIOS_Si7021::getModelName(void)
{
	switch(_model) {
		case SI_Engineering_Samples:
			return "SI engineering samples";
		case SI_7013:
			return "Si7013";
		case SI_7020:
			return "Si7020";
		case SI_7021:
			return "Si7021";
		case SI_unkown:
		default:
			return "unknown";
	}
}


HELIOS_Si7021::sensorType HELIOS_Si7021::getModel(void)
{
	return _model;
}

#endif // FEATURE_SI7021

