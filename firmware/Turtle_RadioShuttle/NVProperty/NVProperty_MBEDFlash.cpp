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

#ifdef __MBED__

#include <mbed.h>
#include "main.h"
#include "arch.h"
#include <algorithm>
#include <NVPropertyProviderInterface.h>
#include <NVProperty_MBEDFlash.h>
#include <NVProperty.h>


#if 0	// sample test code for a main app.
	{
		NVProperty p;
		p.OpenPropertyStore(true);
		
		dprintf("LORA_REMOTE_ID: %d", p.GetProperty(p.LORA_REMOTE_ID, 0));
		p.SetProperty(p.LORA_REMOTE_ID, p.T_32BIT, 123);
		dprintf("LORA_REMOTE_ID: %d", p.GetProperty(p.LORA_REMOTE_ID, 0));
		dprintf("Host: %s", p.GetProperty(p.HOSTNAME, "MyHost"));
		p.SetProperty(p.HOSTNAME, p.T_STR, "Wunstorf", p.S_FLASH);
		dprintf("Host: %s", p.GetProperty(p.HOSTNAME, "MyHost"));
		p.SetProperty(p.LORA_REMOTE_ID, p.T_32BIT, 456);
		dprintf("LORA_REMOTE_ID: %d", p.GetProperty(p.LORA_REMOTE_ID, 0));
		dprintf("Host: %s", p.GetProperty(p.HOSTNAME, "MyHost"));
	}
#endif



NVProperty_MBEDFlash::NVProperty_MBEDFlash(int propSizekB, bool erase)
{
	_flashIAP = new FlashIAP();
	_flashIAP->init();
	
	// a min page size > 8 looks strange
	MBED_ASSERT(_flashIAP->get_page_size() <= sizeof(int64_t));
	

	_debug = false;
	_propSizekB = propSizekB;
	_pageSize = _flashIAP->get_page_size();
	_numPages = _flashIAP->get_flash_size() / _pageSize;
	_rowSize = _flashIAP->get_sector_size(_flashIAP->get_flash_start());
	MBED_ASSERT(((propSizekB *1024) % _rowSize) == 0);
	_startAddress = (uint8_t*)_flashIAP->get_flash_start() + ((_numPages-(_propSizekB * 1024)/_pageSize) * _pageSize);
	_endAddress = _startAddress + (_propSizekB * 1024);
	//	_flashErasedValue = _flashIAP->get_erase_value();
	_flashErasedValue = 0xff; // until the new mbed flashIAP->get_erase_value() is available
#ifdef TARGET_STM32L0
	_flashErasedValue = 0x00;
#endif
	_lastEntry = NULL;

	if (_debug) {
		dprintf("_propSizekB: %d kB", _propSizekB);
		dprintf("_pageSize: %d", _pageSize);
		dprintf("_numPages: %d", _numPages);
		dprintf("_rowSize: %d", _rowSize);
		dprintf("PageOffset: %d", _numPages-((_propSizekB * 1024)/_pageSize));
		dprintf("total: %d", _pageSize * _numPages);
		dprintf("_startAddress: %d", (int)_startAddress);
	}
	
	_FlashInititalize(erase);
	_GetFlashEntry(0); // inits the _lastEntry record
}


NVProperty_MBEDFlash::~NVProperty_MBEDFlash()
{
	_flashIAP->deinit();
	delete _flashIAP;
#if 0
	_debug = true;
	wait_ms(100);
	_DumpAllEntires();
	wait_ms(100);
	dump("buffer: ", _startAddress, 100);
	wait_ms(100);
#endif
}


void
NVProperty_MBEDFlash::_FlashInititalize(bool force)
{
	_flash_header *fh = (_flash_header *)_startAddress;
	if (fh->magic == FLASH_PROP_MAGIC && fh->version == FLASH_PROP_VERSION && fh->sizeKB == _propSizekB) {
		if (_debug)
			dprintf("Flash OK");
		if (!force)
			return;
	}
	
	if (_debug)
		dprintf("Formatting Flash");
	
	_flash_header f;
	memset(&f, 0, sizeof(f));
	f.magic = FLASH_PROP_MAGIC;
	f.version = FLASH_PROP_VERSION;
	f.sizeKB = _propSizekB;
	
	int count = (_propSizekB * 1024) / _rowSize;
	int startRow = (int)_startAddress / _rowSize;
	_FlashEraseRow(startRow, count);
	_FlashWrite(_startAddress, &f, sizeof(f));
}


void
NVProperty_MBEDFlash::_FlashEraseRow(int startRow, int count)
{
	// dprintf("_FlashEraseRow: startRow=%d, count=%d", startRow, count);
	
	for(int i = 0; i < count; i++) {
		uint32_t *startAddr = (uint32_t *)((startRow + i) * _rowSize);
		uint32_t *addr = startAddr;
		bool foundData = false;
		uint32_t emtpyValue = _flashErasedValue << 24 | _flashErasedValue << 16 | _flashErasedValue << 8 | _flashErasedValue;
		for (int offset = 0; offset < _rowSize; offset += sizeof(uint32_t)) {
			if (*addr++ != emtpyValue) {
				foundData = true;
				break;
			}
		}
		if (_debug)
			dprintf("_FlashEraseRow: addr=0x%x, count=%d (%s)", (unsigned int)startAddr, i,
					foundData ? "erased" : "skipped");
		if (!foundData)
			continue;

		_flashIAP->erase((startRow + i) * _rowSize, _rowSize);
	}
}


/*
 * Find out start page, number of pages
 * Check if the page contins FF's than write, otherwise erase first
 */
void
NVProperty_MBEDFlash::_FlashWrite(uint8_t *address, const void *d, size_t length)
{
	uint8_t *data = (uint8_t *)d;
	
	if (address < _startAddress || address > _startAddress + (_pageSize * _numPages))
		return;
	
	int done = 0;
	
	do {
		uint32_t startPage = (uint32_t)(address + done) / _pageSize;
		int pageOffset = (uint32_t)(address + done) % _pageSize;
		int pageWriteSize = _pageSize - pageOffset;
		
		if (_FlashIsCleared((uint8_t *)(startPage * _pageSize) + pageOffset, pageWriteSize)) {
			// single page write
			int writeLength = std::min(pageWriteSize, (int)length);
			_FlashWritePage(startPage, pageOffset, data, writeLength);
			length -= writeLength;
			done += writeLength;
			data += writeLength;
		} else {
			// row write
			// load row copy
			// erase row
			// merge in new data
			// write row in page copies
			uint32_t startRow = (uint32_t)(address + done) / _rowSize;
			int rowOffset = (uint32_t)(address + done) - (startRow * _rowSize);
			int cplen = std::min((int)length, _rowSize - rowOffset);
			uint8_t *saveddata = new uint8_t[_rowSize];
			if (!saveddata)
				return;

			memcpy(saveddata, (uint8_t *)(startRow * _rowSize), _rowSize);
			// dprintf("startRow=%d rowOffset=%d, cplen=%d", startRow, rowOffset, cplen);
			
			memcpy(saveddata + rowOffset, data, cplen);
			
			_FlashEraseRow(startRow);
			for (int i = 0; i < _rowSize/_pageSize; i++) {
				_FlashWritePage(((startRow * _rowSize) / _pageSize) + i, 0, saveddata + (i * _pageSize), _pageSize);
			}
			length -= cplen;
			done += cplen;
			data += cplen;

			delete[] saveddata;
		}
	} while(length > 0);
}


bool
NVProperty_MBEDFlash::_FlashIsCleared(uint8_t *address, int len)
{
	while (len-- > 0) {
		if (*address++ != _flashErasedValue) {
			return false;
		}
	}
	return true;
}


void
NVProperty_MBEDFlash::_FlashWritePage(int page, int offset, uint8_t *data, int length)
{
	uint8_t *addr = (uint8_t *)(page * _pageSize) + offset;
	if (length < 1)
		return;
	
	_flashIAP->program(data, (uint32_t)addr, length);
}



int
NVProperty_MBEDFlash::GetProperty(int key)
{
    return GetProperty64(key);
}


int64_t
NVProperty_MBEDFlash::GetProperty64(int key)
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
NVProperty_MBEDFlash::GetPropertyStr(int key)
{
	_flashEntry *p = _GetFlashEntry(key);
	if (!p || p->t.type != NVProperty::T_STR)
		return NULL;
    return strdup(p->data.v_str);
}

int
NVProperty_MBEDFlash::GetPropertyBlob(int key, const void *blob, int *size)
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
NVProperty_MBEDFlash::SetProperty(int key, int64_t value, int type)
{
	UNUSED(type);
	uint8_t valbuf[16];
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
	if (storeType == NVProperty::T_BIT)
		len = FLASH_ENTRY_HEADER_SHORT;
	else if (storeType == NVProperty::T_8BIT || storeType == NVProperty::T_16BIT)
		len = FLASH_ENTRY_HEADER;
	else // 32/64/STR/BLOB
		len = FLASH_ENTRY_HEADER + p->u.option.d_len;
	len += _GetFlashPaddingSize(len);
	if ((uint8_t *)_lastEntry + len >= _endAddress) {
		if (!_FlashReorgEntries(len))
			return NVProperty::NVP_ERR_NOSPACE;
	}

	_FlashWrite((uint8_t *)_lastEntry, p, len);
	_lastEntry = (_flashEntry *)((uint8_t *)_lastEntry + len);

	// _DumpAllEntires();
    return NVProperty::NVP_OK;
}


int
NVProperty_MBEDFlash::SetPropertyStr(int key, const char *value, int type)
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
	
	int len = FLASH_ENTRY_HEADER + p->u.option.d_len;
	len += _GetFlashPaddingSize(len);

	if ((uint8_t *)_lastEntry + len >= _endAddress) {
		if (!_FlashReorgEntries(len)) {
			err = NVProperty::NVP_ERR_NOSPACE;
			goto done;
		}
	}

	_FlashWrite((uint8_t *)_lastEntry, p, len);
	_lastEntry = (_flashEntry *)((uint8_t *)_lastEntry + len);

done:
	delete[] p;
	// _DumpAllEntires();
    return err;
}

int
NVProperty_MBEDFlash::SetPropertyBlob(int key, const void *blob, int size, int type)
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
	
	int len = FLASH_ENTRY_HEADER + p->u.option.d_len;
	len += _GetFlashPaddingSize(len);

	if ((uint8_t *)_lastEntry + len >= _endAddress) {
		if (!_FlashReorgEntries(len)) {
			err = NVProperty::NVP_ERR_NOSPACE;
			goto done;
		}
	}

	_FlashWrite((uint8_t *)_lastEntry, p, len);
	_lastEntry = (_flashEntry *)((uint8_t *)_lastEntry + len);

done:
	delete[] p;
	// _DumpAllEntires();
    return err;
}

int
NVProperty_MBEDFlash::EraseProperty(int key)
{
	uint8_t valbuf[16];
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
	
	int len = FLASH_ENTRY_HEADER_SHORT;
	len += _GetFlashPaddingSize(len);
	
	if ((uint8_t *)_lastEntry + len > _endAddress) {
			if (!_FlashReorgEntries(len))
				return NVProperty::NVP_ERR_NOSPACE;
	}

	_FlashWrite((uint8_t *)_lastEntry, p, len);
	_lastEntry = (_flashEntry *)((uint8_t *)_lastEntry + len);

	// _DumpAllEntires();
	return NVProperty::NVP_OK;
}

int
NVProperty_MBEDFlash::ReorgProperties(void)
{
	if (_FlashReorgEntries(FLASH_ENTRY_HEADER))
    	return NVProperty::NVP_OK;
	return NVProperty::NVP_ERR_NOSPACE;
}


int
NVProperty_MBEDFlash::OpenPropertyStore(bool forWrite)
{
	UNUSED(forWrite);
    return NVProperty::NVP_OK;
}

int
NVProperty_MBEDFlash::ClosePropertyStore(bool flush)
{
    return NVProperty::NVP_OK;
}

#if 1
void
NVProperty_MBEDFlash::_DumpAllEntires(void)
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

NVProperty_MBEDFlash::_flashEntry *
NVProperty_MBEDFlash::_GetFlashEntry(int key, uint8_t *start)
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
NVProperty_MBEDFlash::_GetFlashEntryLen(_flashEntry *p)
{
	int len = 0;
	
	if (p->t.type == NVProperty::T_BIT || p->t.deleted)
		len = FLASH_ENTRY_HEADER_SHORT;
	else if (p->t.type == NVProperty::T_8BIT || p->t.type == NVProperty::T_16BIT)
		len = FLASH_ENTRY_HEADER;
	else
		len = FLASH_ENTRY_HEADER + p->u.option.d_len;
		
	len += _GetFlashPaddingSize(len);

	return len;
}

int
NVProperty_MBEDFlash::_GetFlashPaddingSize(int len)
{
	int remain = len % _pageSize;
	
	if (remain == 0)
		return 0;
	
	return (len + _pageSize - remain) - len;
}


int
NVProperty_MBEDFlash::_FlashReorgEntries(int minRequiredSpace)
{
	if (_debug) {
		dprintf("_FlashReorgEntries: start");
		// _DumpAllEntires();
	}

	int totalLen = 0;
	int freeSpace = 0;
	
	_flashEntry *p = (_flashEntry *)(_startAddress + sizeof(_flash_header));
	while((uint8_t *)p < _endAddress && p->key != _flashErasedValue) {
		_flashEntry *k = _GetFlashEntry(p->key);
		if (k == p) { // current entry is the lastest one.
			totalLen += _GetFlashEntryLen(k);
		}
		p = (_flashEntry *)((uint8_t *)p + _GetFlashEntryLen(p));
	}

	if (_startAddress + sizeof(_flash_header) + totalLen + minRequiredSpace >= _endAddress)
			return 0;
	
	freeSpace = _endAddress - (_startAddress + sizeof(_flash_header) + totalLen);
	if (_debug)
		dprintf("freeSpace: %d, totalLen: %d", freeSpace, totalLen);
	
	/*
	 * Copy header
	 * while (content {
	 *	- scan until tmp page is full
	 *	- write page
	 * }
	 * Erase remaining pages.
	 *
	 */
	
	p = (_flashEntry *)(_startAddress + sizeof(_flash_header));
	uint8_t *saveddata = new uint8_t[_rowSize+sizeof(struct _flashEntry)];
	if (!saveddata)
		return 0;
	uint8_t *t = saveddata;
	int currentRow = (uint32_t)_startAddress / _rowSize;
	int totalCopied = 0;
	
	t = saveddata;
	memcpy(t, _startAddress, sizeof(_flash_header));
	t += sizeof(_flash_header);
	
	while((uint8_t *)p < _endAddress && p->key != _flashErasedValue) {
		_flashEntry *k = _GetFlashEntry(p->key, (uint8_t *)p);
		if (k == p) {	// current entry is the lastest one.
			if (!p->t.deleted) {
				int plen = _GetFlashEntryLen(p);
				memcpy(t, p, plen);
				t += plen;
				totalCopied += plen;
				if (t - saveddata >= _rowSize) { // copy page
					_FlashEraseRow(currentRow);
					_FlashWrite((uint8_t *)(currentRow++ * _rowSize), saveddata, _rowSize);
					int remainLen = (t - saveddata) - _rowSize;
					if (remainLen) {
						memcpy(saveddata, t - remainLen, remainLen);
					}
					t = saveddata + remainLen;
				}
			}
		}
		p = (_flashEntry *)((uint8_t *)p + _GetFlashEntryLen(p));
	}

	if (t > saveddata) { // copy remaining
		_FlashEraseRow(currentRow);
		_FlashWrite((uint8_t *)(currentRow++ * _rowSize), saveddata, t - saveddata);
	}

	while((uint32_t)0 + currentRow * _rowSize < (uint32_t)_endAddress) {
		_FlashEraseRow(currentRow++);
	}
	delete[] saveddata;
	_GetFlashEntry(0); // inits the _lastEntry record

	if (_debug) {
		dprintf("_FlashReorgEntries: end");
		_DumpAllEntires();
	}
	
	return _endAddress - _startAddress -  (sizeof(_flash_header) + totalCopied);
}


#endif // __MBED__
