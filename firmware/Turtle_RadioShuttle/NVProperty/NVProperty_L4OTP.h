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

#ifndef __NVPROPERTY_L4OTP__
#define __NVPROPERTY_L4OTP__

class NVProperty_L4OTP : public NVPropertyProviderInterface {
public:
	NVProperty_L4OTP();
	~NVProperty_L4OTP();

    virtual int GetProperty(int key);
    virtual int64_t GetProperty64(int key);
    virtual int GetPropertyBlob(int key, const void *blob, int *size);
    virtual const char *GetPropertyStr(int key);
    virtual int SetProperty(int key, int64_t value, int type);
    virtual int SetPropertyStr(int key, const char *value, int type);
    virtual int SetPropertyBlob(int key, const void *blob, int size, int type);
    virtual int EraseProperty(int key);
    virtual int ReorgProperties(void);
    virtual int OpenPropertyStore(bool forWrite = false);
    virtual int ClosePropertyStore(bool flush = false);

private:
	void _FlashInititalize(void);
	void _OTPWrite(uint8_t *address, const void *data, size_t length);

	struct _flash_header {
		uint32_t magic;
		uint16_t version;
		uint16_t size;
	};

	static const int FLASH_ENTRY_MIN_SIZE		= 8;
	static const int MAX_DATA_ENTRY				= 256-FLASH_ENTRY_MIN_SIZE;
	static const int FLASH_PADDING_SIZE			= 8; // writes sizes and alignment must be multiple of 64-bit,
	
	struct _flashEntry {
		uint8_t key;	// Property key
		struct {
			uint8_t deleted	: 1; // this key has been deleted
			uint8_t t_bit  	: 1; // contains the bool value
			uint8_t reserv1	: 1; //
			uint8_t reserv2	: 1; //
			uint8_t type   	: 4; // NVPType
		} t;
		union {
			int16_t v_16bit;
			int8_t	v_8bit;
			struct {
				uint8_t d_len;				 // data length
				uint8_t f_reserv1  		: 8;
			} option;
		} u;
		union {
			int32_t v_32bit;
			int32_t v_64bit[2];	// use use 2 x 32-bit to avoid 64-bit struct padding
			char v_str[MAX_DATA_ENTRY];
			uint8_t v_blob[MAX_DATA_ENTRY];
		} data;
	};
	
	_flashEntry * _GetFlashEntry(int key, uint8_t *start = NULL);
	void _DumpAllEntires(void);
	int _GetFlashEntryLen(_flashEntry *k);
	int _GetFlashPaddingSize(int len);
	int _FlashReorgEntries(int minRequiredSpace);

	_flashEntry *_lastEntry;
	uint8_t *_startAddress;
	uint8_t *_endAddress;
	bool _debug;
	int _propSize;

	static const uint8_t _flashErasedValue = 0xff;
	static const int FLASH_PROP_MAGIC = 0x4c4f5450; // "LOTP"
	static const int FLASH_PROP_VERSION = 1;
};

#endif // __NVPROPERTY_L4OTP__
