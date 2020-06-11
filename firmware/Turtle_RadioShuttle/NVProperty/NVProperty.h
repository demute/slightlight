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

#ifndef __NVPROPERTY_H__
#define __NVPROPERTY_H__


#ifndef UNUSED
 #define UNUSED(x) (void)(x)
#endif

class NVProperty {
public:
	/*
	 * The property store size depends on the implementation
	 * The ESP32 uses the NVS partition therefore the size is being ignored.
	 * The D21 uses the default 16kByte which is a good tradeoff between
	 * space needed for the application versus space for properties.
     * Larger D21 property space (e.g. 20kB) has the advantage that the
     * flash blocks are less busy. For the D21 it is a good idea to
	 * use increments of 16kB because this is the region locking area size
 	 */
#ifndef TARGET_STM32L0
    NVProperty(int propSizekB = 16, bool erase = false);
#else
    NVProperty(int propSizekB = 4, bool erase = false);
#endif
    ~NVProperty();
public:
    enum NVPType {
        T_BIT	= 1,
        T_8BIT	= 2,
        T_16BIT	= 3,
        T_32BIT	= 4,
        T_64BIT	= 5,
        T_STR	= 6,	// strings can be up to 256 bytes including zero
        T_BLOB	= 7,	// blobs can be up to 256 bytes long
		T_MAX	= 15	// we have only 4 bit space for the NVP types
    };
    
    enum NVPStore {
        S_OTP	= 0x01,
        S_FLASH	= 0x02,
        S_RAM	= 0x04,
    };
    
    enum NVPErrCode {
        NVP_OK = 0,
        NVP_NO_FLASH 	= -1,
        NVP_NO_RAM 		= -2,
        NVP_NO_STORE 	= -3,
        NVP_NO_PERM 	= -4,
        NVP_ERR_NOSPACE	= -5,
        NVP_ERR_FAIL	= -6,
        NVP_INVALD_PARM	= -7,
        NVP_ENOENT		= -0x12345687,
    };

    /*
     * Get property protocol version to allow
     * API differences between multiple versions
     */
    int GetVersion(void) { return 100; };

    /*
     * A simple GetProperty retuns its values as an int or int64
     * The order should always be S_RAM,S_FLASH, S_OTP
     */
    int GetProperty(int key, int defaultValue = 0);
    int64_t GetProperty64(int key, int64_t defaultValue = 0);
	/*
	 * const char *GetProperty: receives a copy of the propery, use free to release it.
	 */
    const char *GetProperty(int key, const char *defaultValue = NULL);
    /*
     * when a block gets returned the buffer is filled up to the property
     * or max at the bsize length.
	 * if the buffer is NULL only the size value will be set
     */
    int GetProperty(int key, void  *buffer, int *size);

    /*
     * SetProperty
     * It requires to use OpenPropertyStore and finally ClosePropertyStore(true)
     * to write out all properties.
	 * Properties are being limited to 256 bytes. (e.g. 255 long strings or 256 bytes blobs)
     *
	 * Number properties e.g. 0 or 1, or 123 are highly optimized in storage sizes
	 * therefore the value is automatically being compressed to a bit or a little
	 * number to use less flash storage space.
     */
    int SetProperty(int key, NVPType ptype, int64_t value, NVPStore store = S_FLASH);
    int SetProperty(int key, NVPType ptype, const char *value, NVPStore store = S_FLASH);
    int SetProperty(int key, NVPType ptype, const void *blob, int length, NVPStore store = S_FLASH);
    
    int EraseProperty(int key, NVPStore store = S_FLASH);
	
	/*
	 * ReorgProperties is usually not needed because when a property storage is
	 * full it reorganizes itself to make space for new properties.
	 */
	int ReorgProperties(NVPStore store = S_FLASH);
	/*
	 * Opens a property store for reading or writing.
	 */
    int OpenPropertyStore(bool forWrite = false);
	/*
	 * Closes the property store and flushes the data, depending on the
	 * implementation flush may be not needed, e.g. D21
	 */
    int ClosePropertyStore(bool flush = false);

    enum Properties {
		RTC_AGING_CAL	= 1, // int8_t the RTC aging calibration value
		ADC_VREF		= 2, // the adc refernce volatge in millivolt
		HARDWARE_REV	= 3, // the hardware revision of the board
		CPUID			= 4, // the internal CPU ID
        
		LORA_DEVICE_ID	= 10, // uint32_t the LoRa device ID
		LORA_CODE_ID	= 11, // uint32_t the Code for the RadioShuttle protocol
		LORA_REMOTE_ID	= 12, // specifies the server address
		LORA_REMOTE_ID_ALT = 13, // specifies the alternate server address
		LORA_RADIO_TYPE	= 14,	// 1 = Offline, 3 = Online, 4 = RS_Station_Basic
		LORA_FREQUENCY 	= 15,	// channel frequency in Hz, e.g. 868100000
		LORA_BANDWIDTH	= 16,	// channel bandwidth in Hz, e.g. 125000
		LORA_SPREADING_FACTOR = 17, // e.g. 7
		LORA_TXPOWER	= 18,	// e.g. 14 for 15 dBm.
		LORA_FREQUENCY_OFFSET = 19,
		LORA_APP_PWD	= 20,	// passwords are per app, there are only two placeholders
		LORA_APP_PWD_ALT = 21,

		LOC_LONGITUDE	= 25,	// a string
		LOC_LATITUDE 	= 26,	// a string
		LOC_NAME 		= 27, 	// a string with the location name
		HOSTNAME 		= 28,	// the device host name
        
		WIFI_SSID		= 30,
		WIFI_PASSWORD	= 31,
		WIFI_SSID_ALT	= 32,
		WIFI_PASSWORD_ALT = 33,
		USE_DHCP		= 34,
		MAC_ADDR		= 35,
		NET_IP_ADDR		= 36,
		NET_IP_ROUTER	= 37,
		NET_IP_DNS_SERVER = 38,
		NET_IP_DNS_SERVER_ALT = 39,
		
		MQTT_SERVER		= 40,	// url mqtt or mqtts://user.password@server:port
		MQTT_SERVER_ALT	= 41,
		MQTT_TOPIC_INFO	= 42,
		MQTT_TOPIC_ALARM= 43,
		MQTT_TOPIC_CONTROL = 44,
		MQTT_TOPIC_4	= 45,
		MQTT_TOPIC_5	= 46,
		
		NET_NTP_SERVER	= 50,
		NET_NTP_SERVER_ALT = 51,
		NET_NTP_GMTOFFSET = 52,
		NET_NTP_DAYLIGHTOFFSET = 53,

		ALARM_STATUS 	= 60,   // alarm on=0, off !0
		
		PRIVATE_RANGE_START = 128,
		PRIVATE_RANGE_END 	= 254,
		MAX_PROPERTIES		= 254, // 1-254
    };

private:
    NVPropertyProviderInterface *_ram;
    NVPropertyProviderInterface *_flash;
    NVPropertyProviderInterface *_otp;
    bool _allowWrite;
    bool _didOpen;
};

#endif
