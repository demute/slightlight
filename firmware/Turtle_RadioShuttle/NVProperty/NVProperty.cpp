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

#ifdef ARDUINO
#include <Arduino.h>
#elif __MBED__
#include <mbed.h>
#else
#error "Unkown operating system"
#endif


#include <NVPropertyProviderInterface.h>
#include <NVProperty_SRAM.h>
#ifdef ARDUINO_ARCH_ESP32
  #include <NVProperty_ESP32NVS.h>
  #include <NVProperty_ESP32efuse.h>
#elif defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_ARCH_SAMD)
  #include <NVProperty_D21Flash.h>
#elif __MBED__
  #include <mbed.h>
  #include <NVProperty_MBEDFlash.h>
  #ifdef TARGET_STM32L4
    #include <NVProperty_L4OTP.h>
  #endif
#else
#error "Unkown implementation"
#endif
#include <NVProperty.h>


NVProperty::NVProperty(int propSizekB, bool erase)
{
    _flash = NULL;
    _otp = NULL;
    _ram = new NVProperty_SRAM();
    
#ifdef ARDUINO_ARCH_ESP32
    _flash = new NVProperty_ESP32NVS();
#elif defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_ARCH_SAMD)
    _flash = new NVProperty_D21Flash(propSizekB, erase);
#elif __MBED__
	_flash = new NVProperty_MBEDFlash(propSizekB, erase);
#else
 #error "unkown platform"
#endif

#ifdef ARDUINO_ARCH_ESP32
    _otp = new NVProperty_ESP32efuse();
#elif TARGET_STM32L4
    _otp = new NVProperty_L4OTP();
#endif
    _allowWrite = false;
    _didOpen = false;
}

NVProperty::~NVProperty()
{
    if (_ram) {
    	delete _ram;
	}
	if (_flash)
        delete _flash;
    if (_otp)
        delete _otp;
}


int
NVProperty::GetProperty(int key, int defaultValue)
{
    int res;
    
    if (!_didOpen)
        OpenPropertyStore();
    
    if (_ram) {
        res = _ram->GetProperty(key);
        if (res != NVP_ENOENT)
            return res;
    }
    if (_flash) {
        res = _flash->GetProperty(key);
        if (res != NVP_ENOENT)
            return res;
    }
    if (_otp) {
        res = _otp->GetProperty(key);
        if (res != NVP_ENOENT)
            return res;
    }
    return defaultValue;
}


int64_t
NVProperty::GetProperty64(int key, int64_t defaultValue)
{
    int64_t res;
    
    if (!_didOpen)
        OpenPropertyStore();

    if (_ram) {
        res = _ram->GetProperty64(key);
        if (res != NVP_ENOENT)
            return res;
    }
    if (_flash) {
        res = _flash->GetProperty64(key);
        if (res != NVP_ENOENT)
            return res;
    }
    if (_otp) {
        res = _otp->GetProperty64(key);
        if (res != NVP_ENOENT)
            return res;
    }
    return defaultValue;
}

const char *
NVProperty::GetProperty(int key, const char *defaultValue)
{
    const char *res;
    
    if (!_didOpen)
        OpenPropertyStore();

    if (_ram) {
        res = _ram->GetPropertyStr(key);
        if (res != NULL)
            return res;
    }
    if (_flash) {
        res = _flash->GetPropertyStr(key);
        if (res != NULL)
            return res;
    }
    if (_otp) {
        res = _otp->GetPropertyStr(key);
        if (res != NULL)
            return res;
    }
    if (res != NULL)
        return res;
    
    return defaultValue;
}

int
NVProperty::GetProperty(int key, void *buffer, int *size)
{
    int res;
    int maxsize = *size;
    
    if (!_didOpen)
        OpenPropertyStore();

    if (_ram) {
        res = _ram->GetPropertyBlob(key, buffer, &maxsize);
        if (res == NVP_OK)
            return res;
    }
    if (_flash) {
        res = _flash->GetPropertyBlob(key, buffer, &maxsize);
        if (res == NVP_OK)
            return res;
    }
    if (_otp) {
        res = _otp->GetPropertyBlob(key, buffer, &maxsize);
        if (res == NVP_OK)
            return res;
    }
 
    return NVP_ENOENT;
}


int
NVProperty::SetProperty(int key, NVPType ptype, int64_t value, NVPStore store)
{
    int res = NVP_OK;
    
    if (!_didOpen)
        OpenPropertyStore();

    if (!_allowWrite)
        return NVP_NO_PERM;
    
    if (store == S_RAM && _ram) {
        	res = _ram->SetProperty(key, value, ptype);
    } else if (store == S_FLASH && _flash) {
            res = _flash->SetProperty(key, value, ptype);
    } else if (store == S_OTP && _otp) {
            res = _otp->SetProperty(key, value, ptype);
    } else {
        return NVP_NO_STORE;
    }
    return res;
}


int
NVProperty::SetProperty(int key, NVPType ptype, const char *value, NVPStore store)
{
    int res = NVP_OK;
    
    if (!_didOpen)
        OpenPropertyStore();

    if (!_allowWrite)
        return NVP_NO_PERM;

    if (store == S_RAM && _ram) {
        res = _ram->SetPropertyStr(key, value, ptype);
    } else if (store == S_FLASH && _flash) {
        res = _flash->SetPropertyStr(key, value, ptype);
    } else if (store == S_OTP && _otp) {
        res = _otp->SetPropertyStr(key, value, ptype);
    } else {
        return NVP_NO_STORE;
    }
    
    return res;
}

// NVProperty_SRAM::SetPropertyBlob(int key, const void *blob, int size, int type)


int
NVProperty::SetProperty(int key, NVPType ptype,  const void *blob, int length, NVPStore store)
{
    int res = NVP_OK;

    if (!_didOpen)
        OpenPropertyStore();

    if (!_allowWrite) {
        return NVP_NO_PERM;
	}
 	
  	if (store == S_RAM && _ram) {
        res = _ram->SetPropertyBlob(key, blob, length, ptype);
    } else if (store == S_FLASH && _flash) {
        res = _flash->SetPropertyBlob(key, blob, length, ptype);
    } else if (store == S_OTP && _otp) {
        res = _otp->SetPropertyBlob(key, blob, length, ptype);
    } else {
        return NVP_NO_STORE;
    }

    return res;
}

int
NVProperty::EraseProperty(int key, NVPStore store)
{
    if (!_didOpen)
        OpenPropertyStore();

    int res = NVP_OK;
    
    if (!_allowWrite)
        return NVP_NO_PERM;

    if (store == S_RAM && _ram) {
        res = _ram->EraseProperty(key);
    } else if (store == S_FLASH && _flash) {
        res = _flash->EraseProperty(key);
    } else if (store == S_OTP && _otp) {
        res = _otp->EraseProperty(key);
    } else {
        return NVP_NO_STORE;
    }
    
    return res;
}

int
NVProperty::ReorgProperties(NVPStore store)
{
    int res = NVP_OK;
    
    if (!_didOpen)
        OpenPropertyStore();

    if (!_allowWrite)
        return NVP_NO_PERM;

    if (store == S_RAM && _ram) {
        res = _ram->ReorgProperties();
    } else if (store == S_FLASH && _flash) {
        res = _flash->ReorgProperties();
    } else if (store == S_OTP && _otp) {
        res = _otp->ReorgProperties();
    } else {
        return NVP_NO_STORE;
    }
    
    return res;
}

int
NVProperty::OpenPropertyStore(bool forWrite)
{
    int res = NVP_OK;

    if (_didOpen) {
        if (_ram)
            _ram->ClosePropertyStore();
        if (_flash)
            _flash->ClosePropertyStore();
        if (_otp)
            _otp->ClosePropertyStore();
    }
    
    if (_ram)
        _ram->OpenPropertyStore(forWrite);
    if (_flash)
        res = _flash->OpenPropertyStore(forWrite);
    if (_otp)
        _otp->OpenPropertyStore(forWrite);
    _didOpen = true;
    if(forWrite)
        _allowWrite = true;
    

    return res;
}

int
NVProperty::ClosePropertyStore(bool flush)
{
    int res = NVP_OK;

    if (_didOpen)
        return NVP_NO_PERM;

    if (_ram)
        _ram->ClosePropertyStore(flush);
    if (_flash)
        res = _flash->ClosePropertyStore(flush);
    if (_otp)
        _otp->ClosePropertyStore(flush);
    return res;
}


