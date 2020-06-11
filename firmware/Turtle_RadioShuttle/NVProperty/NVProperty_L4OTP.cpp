/*
 * This is an unpublished work copyright
 * (c) 2019 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 *
 *
 * Use is granted to registered RadioShuttle licensees only.
 * Licensees must own a valid serial number and product code.
 * Details see: www.radioshuttle.de
 */

#ifdef TARGET_STM32L4

#include <mbed.h>
#include "main.h"
#include "arch.h"
#include <algorithm>
#include <NVPropertyProviderInterface.h>
#include <NVProperty_L4OTP.h>
#include <NVProperty.h>


#if 0	// sample test code for a main app.
	{
	NVProperty p;
	
	p.OpenPropertyStore(true);
	dprintf("OTP--1: %d", p.GetProperty(p.CPUID, -1));
	p.SetProperty(p.CPUID, p.T_32BIT, 123, p.S_OTP);
	dprintf("OTP123: %d", p.GetProperty(p.CPUID, 0));
	p.SetProperty(p.CPUID, p.T_32BIT, 0x12345678, p.S_OTP);
	dprintf("OTP0x12345678: %x", p.GetProperty(p.CPUID, 0));
	p.EraseProperty(p.CPUID, p.S_OTP);
	dprintf("OTP:-2 %d", p.GetProperty(p.CPUID, -2));
	dprintf("OTP: Host %s", p.GetProperty(p.HOSTNAME, "MyHost"));
	p.SetProperty(p.HOSTNAME, p.T_STR, "Wunstorf", p.S_OTP);
	dprintf("OTP: Host %s", p.GetProperty(p.HOSTNAME, "MyHost"));
	p.SetProperty(p.CPUID, p.T_32BIT, 9876, p.S_OTP);
	dprintf("OTP9876: %d", p.GetProperty(p.CPUID, 0));
	dprintf("OTP: Host %s", p.GetProperty(p.HOSTNAME, "MyHost"));
	
	}
#endif

// #define OTP_TEST_IN_RAM // test OTP in RAM


NVProperty_L4OTP::NVProperty_L4OTP()
{
	_debug = false;
	_propSize = 1 * 1024; 					// no define in HAL laye
	_startAddress = (uint8_t *) 0x1FFF7000; // no define in HAL layer

#ifdef OTP_TEST_IN_RAM
	static uint8_t *savedStart;
	if (!savedStart) {
 		_startAddress = (uint8_t *) malloc(512);
		memset(_startAddress, _flashErasedValue, 512);
		savedStart = _startAddress;
	} else {
		_startAddress = savedStart;
	}
#endif
	
	_endAddress = _startAddress + (_propSize);
	_lastEntry = NULL;
	
	_FlashInititalize();
	_GetFlashEntry(0); // inits the _lastEntry record
}


NVProperty_L4OTP::~NVProperty_L4OTP()
{
#ifdef OTP_TEST_IN_RAM
	_debug = true;
	_DumpAllEntires();
	wait_ms(100);
	dump("_startAddress", _startAddress, 100);
#endif
}


void
NVProperty_L4OTP::_FlashInititalize(void)
{
	_flash_header *fh = (_flash_header *)_startAddress;
	if (fh->magic == FLASH_PROP_MAGIC && fh->version == FLASH_PROP_VERSION && fh->size == _propSize) {
		return;
	}
	
	uint8_t *p = _startAddress;
	for (int i = 0; i < (int)sizeof(_flash_header); i++) {
		if (*p++ != _flashErasedValue)
			return; // invalid data
	}
	
	_flash_header f;
	memset(&f, 0, sizeof(f));
	f.magic = FLASH_PROP_MAGIC;
	f.version = FLASH_PROP_VERSION;
	f.size = _propSize;
	
	_OTPWrite(_startAddress, &f, sizeof(f));
}


int
NVProperty_L4OTP::GetProperty(int key)
{
    return GetProperty64(key);
}


int64_t
NVProperty_L4OTP::GetProperty64(int key)
{
	_flashEntry *p = _GetFlashEntry(key);
	if (!p)
		return NVProperty::NVP_ENOENT;

    int64_t value = 0;
	
    switch(p->t.type) {
		case NVProperty::T_BIT:
			if (p->t.t_bit)
				value = 1;
			else
				value = 0;
			break;
		case NVProperty::T_8BIT:
			value = p->u.v_8bit;
			break;
		case NVProperty::T_16BIT:
			{
				int16_t v;
				memcpy(&v, &p->u.v_16bit, sizeof(p->u.v_16bit));
				value = v;
			}
			break;
		case NVProperty::T_32BIT:
			{
				int32_t v;
				memcpy(&v, &p->data.v_32bit, sizeof(p->data.v_32bit));
				value = v;
			}
			break;
		case NVProperty::T_64BIT:
			memcpy(&value, p->data.v_64bit, sizeof(p->data.v_64bit));
			break;
		case NVProperty::T_STR:
		case NVProperty::T_BLOB:
			value = p->u.option.d_len;
			break;
	}
    return value;
}

const char *
NVProperty_L4OTP::GetPropertyStr(int key)
{
	_flashEntry *p = _GetFlashEntry(key);
	if (!p || p->t.type != NVProperty::T_STR)
		return NULL;
    return strdup(p->data.v_str);
}

int
NVProperty_L4OTP::GetPropertyBlob(int key, const void *blob, int *size)
{
	_flashEntry *p = _GetFlashEntry(key);
	if (!p || p->t.type != NVProperty::T_BLOB)
		return NVProperty::NVP_ENOENT;
	
	int cplen = std::min(*size, (int)p->u.option.d_len);
	if (blob)
		memcpy((void *)blob, p->data.v_blob, cplen);
	*size = cplen;
	
    return NVProperty::NVP_OK;
}


int
NVProperty_L4OTP::SetProperty(int key, int64_t value, int type)
{
	UNUSED(type);
	uint8_t valbuf[FLASH_ENTRY_MIN_SIZE + sizeof(int64_t)];
	_flashEntry *p = (_flashEntry *) valbuf;
	int storeType;
	
	if (GetProperty64(key) == value) // no need to save it again.
	    return NVProperty::NVP_OK;
	
	memset(valbuf, 0, sizeof(valbuf));
	
	if (value == 0 ||  value == 1)
		storeType = NVProperty::T_BIT;
	else if (value >= -128 && value < 128)
		storeType = NVProperty::T_8BIT;
	else if (value >= -32768 && value < 32768)
		storeType = NVProperty::T_16BIT;
	else if (value > -2147483647LL && value < 2147483648LL)
		storeType = NVProperty::T_32BIT;
	else
		storeType = NVProperty::T_64BIT;
	
	p->key = key;
	p->t.type = storeType;


	switch(storeType) {
		case NVProperty::T_BIT:
			p->t.t_bit = value;
			break;
		case NVProperty::T_8BIT:
			p->u.v_8bit = value;
			break;
		case NVProperty::T_16BIT:
			p->u.v_16bit = value;
			break;
		case NVProperty::T_32BIT:
			p->u.option.d_len = sizeof(p->data.v_32bit);
			{
				int32_t v = value;
				memcpy(&p->data.v_32bit, &v, sizeof(p->data.v_32bit));
			}
			break;
		case NVProperty::T_64BIT:
			p->u.option.d_len = sizeof(p->data.v_64bit);
			memcpy(p->data.v_64bit, &value, sizeof(p->data.v_64bit));
			break;
	}
	int len;
	if (storeType == NVProperty::T_BIT || storeType == NVProperty::T_8BIT || storeType == NVProperty::T_16BIT || storeType == NVProperty::T_32BIT) {
		len = FLASH_ENTRY_MIN_SIZE;
	} else { // 64/STR/BLOB
		len = (FLASH_ENTRY_MIN_SIZE - 4) + p->u.option.d_len;
		len += _GetFlashPaddingSize(len);
	}
	if ((uint8_t *)_lastEntry + len >= _endAddress) {
		if (!_FlashReorgEntries(len))
			return NVProperty::NVP_ERR_NOSPACE;
	}

	_OTPWrite((uint8_t *)_lastEntry, p, len);
	_lastEntry = (_flashEntry *)((uint8_t *)_lastEntry + len);

	// _DumpAllEntires();
    return NVProperty::NVP_OK;
}


int
NVProperty_L4OTP::SetPropertyStr(int key, const char *value, int type)
{
	if (type != NVProperty::T_STR)
		return NVProperty::NVP_INVALD_PARM;
	
	_flashEntry *p = _GetFlashEntry(key);
	if (p && p->t.type == NVProperty::T_STR && strcmp(p->data.v_str, value) == 0) {
		return NVProperty::NVP_OK;
	}

	int err = NVProperty::NVP_OK;
	
	p = new _flashEntry();
	if (!p)
		return NVProperty::NVP_ERR_NOSPACE;
	
	p->key = key;
	p->t.type = NVProperty::T_STR;
	int cplen = std::min(strlen(value), sizeof(p->data.v_str)-1);
	memcpy(p->data.v_str, value, cplen);
	p->u.option.d_len = cplen + 1; // zero term
	
	int len = (FLASH_ENTRY_MIN_SIZE - 4) + p->u.option.d_len;
	len += _GetFlashPaddingSize(len);

	if ((uint8_t *)_lastEntry + len >= _endAddress) {
		if (!_FlashReorgEntries(len)) {
			err = NVProperty::NVP_ERR_NOSPACE;
			goto done;
		}
	}

	_OTPWrite((uint8_t *)_lastEntry, p, len);
	_lastEntry = (_flashEntry *)((uint8_t *)_lastEntry + len);

done:
	delete[] p;
	// _DumpAllEntires();
    return err;
}

int
NVProperty_L4OTP::SetPropertyBlob(int key, const void *blob, int size, int type)
{
	if (type != NVProperty::T_BLOB)
		return NVProperty::NVP_INVALD_PARM;
	
	_flashEntry *p = _GetFlashEntry(key);
	if (p && p->t.type == NVProperty::T_BLOB && size == p->u.option.d_len) { // check for duplicate
		if (memcmp(blob, p->data.v_blob, size) == 0)
			return NVProperty::NVP_OK;
	}
	int err = NVProperty::NVP_OK;
	
	p = new _flashEntry();
	if (!p)
		return NVProperty::NVP_ERR_NOSPACE;
	
	p->key = key;
	p->t.type = NVProperty::T_BLOB;
	int cplen = std::min(size, (int)sizeof(p->data.v_blob));
	p->u.option.d_len = cplen;
	memcpy(p->data.v_blob, blob, cplen);
	
	int len = (FLASH_ENTRY_MIN_SIZE - 4) + p->u.option.d_len;
	len += _GetFlashPaddingSize(len);

	if ((uint8_t *)_lastEntry + len >= _endAddress) {
		if (!_FlashReorgEntries(len)) {
			err = NVProperty::NVP_ERR_NOSPACE;
			goto done;
		}
	}

	_OTPWrite((uint8_t *)_lastEntry, p, len);
	_lastEntry = (_flashEntry *)((uint8_t *)_lastEntry + len);

done:
	delete[] p;
	// _DumpAllEntires();
    return err;
}

int
NVProperty_L4OTP::EraseProperty(int key)
{
	uint8_t valbuf[FLASH_ENTRY_MIN_SIZE];
	_flashEntry *p = (_flashEntry *) valbuf;

	_flashEntry *op = _GetFlashEntry(key);
	if (!op)
		return NVProperty::NVP_ENOENT;
	if (op->t.deleted)
		return NVProperty::NVP_OK;
	
	memset(valbuf, 0, sizeof(valbuf));
	p->key = key;
	p->t.type = op->t.type;
	p->t.deleted = true;
	
	if ((uint8_t *)_lastEntry + FLASH_ENTRY_MIN_SIZE > _endAddress) {
		if (!_FlashReorgEntries(FLASH_ENTRY_MIN_SIZE))
			return NVProperty::NVP_ERR_NOSPACE;
	}

	_OTPWrite((uint8_t *)_lastEntry, p, FLASH_ENTRY_MIN_SIZE);
	_lastEntry = (_flashEntry *)((uint8_t *)_lastEntry + FLASH_ENTRY_MIN_SIZE);

	// _DumpAllEntires();
	return NVProperty::NVP_OK;
}

int
NVProperty_L4OTP::ReorgProperties(void)
{
    return NVProperty::NVP_OK;
}


int
NVProperty_L4OTP::OpenPropertyStore(bool forWrite)
{
	UNUSED(forWrite);
    return NVProperty::NVP_OK;
}

int
NVProperty_L4OTP::ClosePropertyStore(bool flush)
{
    return NVProperty::NVP_OK;
}

#if 1
void
NVProperty_L4OTP::_DumpAllEntires(void)
{
	if (!_debug)
		return;
	
	dprintf("------------- DumpAllEntires -------- ");

	int index = 0;
	_flashEntry *p = (_flashEntry *)(_startAddress + sizeof(_flash_header));
	while((uint8_t *)p < _endAddress && p->key != _flashErasedValue) {

		int64_t value = 0;
    	switch(p->t.type) {
		case NVProperty::T_BIT:
			if (p->t.t_bit)
				value = 1;
			else
				value = 0;
			break;
		case NVProperty::T_8BIT:
			value = p->u.v_8bit;
			break;
		case NVProperty::T_16BIT:
			{
				int16_t v;
				memcpy(&v, &p->u.v_16bit, sizeof(p->u.v_16bit));
				value = v;
			}
			break;
		case NVProperty::T_32BIT:
			{
				int32_t v;
				memcpy(&v, &p->data.v_32bit, sizeof(p->data.v_32bit));
				value = v;
			}
			break;
		case NVProperty::T_64BIT:
			memcpy(&value, p->data.v_64bit, sizeof(p->data.v_64bit));
			break;
		case NVProperty::T_STR:
		case NVProperty::T_BLOB:
			value = p->u.option.d_len;
			break;
		}
		index++;
		if (p->t.deleted) {
			dprintf("Dump[%.2d] Key: %d Type: %d deleted(%d)", index, p->key, p->t.type, p->t.deleted);

		} else if (p->t.type == NVProperty::T_STR) {
			dprintf("Dump[%.2d] Key: %d Type: %d value: %s", index, p->key, p->t.type, p->data.v_str);
		} else if (p->t.type == NVProperty::T_BLOB) {
			dprintf("Dump[%.2d] Key: %d Type: %d len: %d", index, p->key, p->t.type, p->u.option.d_len);
			dump("Blob",  p->data.v_str, p->u.option.d_len);
		} else {
			if (p->t.type == NVProperty::T_64BIT) {
				dprintf("Dump[%.2d] Key: %d Type: %d value: %lld (0x%llx)", index, p->key, p->t.type, value, value);
			} else {
				dprintf("Dump[%.2d] Key: %d Type: %d value: %ld (0x%x)", index, p->key, p->t.type, (int32_t)value, (unsigned int)value);
			}
		}
		
		p = (_flashEntry *)((uint8_t *)p + _GetFlashEntryLen(p));
	}
	int freebytes = _endAddress -(uint8_t *)_lastEntry;
	if (_lastEntry)
		dprintf("------ %d bytes free -------", freebytes);
}
#endif

NVProperty_L4OTP::_flashEntry *
NVProperty_L4OTP::_GetFlashEntry(int key, uint8_t *start)
{
	_flashEntry *p;

	if (start)
		p = (_flashEntry *)start;
	else
		p = (_flashEntry *)(_startAddress + sizeof(_flash_header));
	_flashEntry *lastP = NULL;
	while(true) {
		if ((uint8_t*)p >= _endAddress || p->key == _flashErasedValue) {
			if ((uint8_t*)p <= _endAddress)
				_lastEntry = p;
			if (!lastP || lastP->t.deleted)
				return NULL;
			break;
		}
		if (p->key == key)
			lastP = p;

		p = (_flashEntry *)((uint8_t *)p + _GetFlashEntryLen(p));
	}
	return lastP;
}


int
NVProperty_L4OTP::_GetFlashEntryLen(_flashEntry *p)
{
	int len = 0;
	
	switch(p->t.type) {
		case NVProperty::T_64BIT:
		case NVProperty::T_STR:
		case NVProperty::T_BLOB:
			len = (FLASH_ENTRY_MIN_SIZE - 4) + p->u.option.d_len;
			len += _GetFlashPaddingSize(len);
			break;
		default:
			len = FLASH_ENTRY_MIN_SIZE;
	}
	return len;
}

int
NVProperty_L4OTP::_GetFlashPaddingSize(int len)
{
	int remain = len % FLASH_PADDING_SIZE;
	
	if (remain == 0)
		return 0;
	
	return (len + FLASH_PADDING_SIZE - remain) - len;
}


int
NVProperty_L4OTP::_FlashReorgEntries(int minRequiredSpace)
{
	return 0; // no reorg on OTP
}


void
NVProperty_L4OTP::_OTPWrite(uint8_t *address, const void *d, size_t length)
{
#ifdef OTP_TEST_IN_RAM
	memcpy(address, d, length);
#else
	OTPWrite(address, d, length);
#endif
}

#endif // TARGET_STM32L4
