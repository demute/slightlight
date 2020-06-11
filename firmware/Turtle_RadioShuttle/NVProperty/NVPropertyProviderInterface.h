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

#ifndef __NVPROPERTYPROVIDERINTERFACE__
#define __NVPROPERTYPROVIDERINTERFACE__

class NVPropertyProviderInterface {
public:
   	virtual ~NVPropertyProviderInterface() { }
    
    virtual int GetProperty(int key) = 0;
    virtual int64_t GetProperty64(int key) = 0;
    virtual const char *GetPropertyStr(int key) = 0;
    virtual int GetPropertyBlob(int key, const void *blob, int *size) = 0;
    virtual int SetProperty(int key, int64_t value, int type) = 0;
    virtual int SetPropertyStr(int key, const char *str, int type) = 0;
    virtual int SetPropertyBlob(int key, const void *blob, int size, int type) = 0;
    virtual int EraseProperty(int key) = 0;
    virtual int ReorgProperties(void) = 0;
    virtual int OpenPropertyStore(bool forWrite = false) = 0;
    virtual int ClosePropertyStore(bool flush = false) = 0;
};

#endif // __NVPROPERTYPROVIDERINTERFACE__
