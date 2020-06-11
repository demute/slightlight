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


#include <mbed.h>
#include "PinMap.h"
#include "main.h"
#include "arch.h"

#ifdef FEATURE_NVPROPERTYEDITOR
#include <NVPropertyProviderInterface.h>
#include "NVProperty.h"

static const char *getNiceName(int id, int val);

/*
 * add custom user defined properties here
 */
class UserProperty : public NVProperty {
public:
	enum Properties {
    	MY_DEVNAME = PRIVATE_RANGE_START,
		MY_CITY,
		MY_APP_PASSWORD,
	};
};

// #define ARRAYLEN(arr) (sizeof(arr) / sizeof(0[arr]))

struct propArray {
	int id;
	NVProperty::NVPType type;
	const char *name;
	int valueInt;
	const char *valueStr;
} propArray[] = {
	{ NVProperty::RTC_AGING_CAL,      NVProperty::T_32BIT,  "RTC_AGING_CAL", 0, NULL },
	{ NVProperty::ADC_VREF,           NVProperty::T_32BIT,  "ADC_VREF", 0, NULL },
	{ NVProperty::HARDWARE_REV,       NVProperty::T_32BIT,  "HARDWARE_REV", 0, NULL },

	{ NVProperty::LORA_DEVICE_ID,     NVProperty::T_32BIT,  "LORA_DEVICE_ID", 0, NULL },  
	{ NVProperty::LORA_CODE_ID,       NVProperty::T_32BIT,  "LORA_CODE_ID", 0, NULL },  
	{ NVProperty::LORA_REMOTE_ID,     NVProperty::T_32BIT,  "LORA_REMOTE_ID", 0, NULL },
	{ NVProperty::LORA_REMOTE_ID_ALT, NVProperty::T_32BIT,  "LORA_REMOTE_ID_ALT", 0, NULL },
	{ NVProperty::LORA_RADIO_TYPE,    NVProperty::T_32BIT,  "LORA_RADIO_TYPE", 0, NULL },
	{ NVProperty::LORA_FREQUENCY,     NVProperty::T_32BIT,  "LORA_FREQUENCY", 0, NULL },
	{ NVProperty::LORA_BANDWIDTH,     NVProperty::T_32BIT,  "LORA_BANDWIDTH", 0, NULL },
	{ NVProperty::LORA_SPREADING_FACTOR, NVProperty::T_32BIT,  "LORA_SPREADING_FACTOR", 0, NULL },
	{ NVProperty::LORA_TXPOWER,       NVProperty::T_32BIT,  "LORA_TXPOWER", 0, NULL },
	{ NVProperty::LORA_FREQUENCY_OFFSET, NVProperty::T_32BIT,  "LORA_FREQUENCY_OFFSET", 0, NULL },
	{ NVProperty::LORA_APP_PWD,       NVProperty::T_STR,	"LORA_APP_PWD", 0, NULL },
	{ NVProperty::LORA_APP_PWD_ALT,   NVProperty::T_STR,	"LORA_APP_PWD_ALT", 0, NULL },

	{ NVProperty::LOC_LONGITUDE,      NVProperty::T_STR,	"LOC_LONGITUDE", 0, NULL },
	{ NVProperty::LOC_LATITUDE,       NVProperty::T_STR,	"LOC_LATITUDE", 0, NULL },
	{ NVProperty::LOC_NAME,           NVProperty::T_STR,	"LOC_NAME", 0, NULL },
	{ NVProperty::HOSTNAME,           NVProperty::T_STR,	"HOSTNAME", 0, NULL },

	{ NVProperty::WIFI_SSID,          NVProperty::T_STR,    "WIFI_SSID", 0, NULL },
	{ NVProperty::WIFI_PASSWORD,      NVProperty::T_STR,    "WIFI_PASSWORD", 0, NULL },
	{ NVProperty::WIFI_SSID_ALT,      NVProperty::T_STR,    "WIFI_SSID_ALT", 0, NULL },
	{ NVProperty::WIFI_PASSWORD_ALT,  NVProperty::T_STR,    "WIFI_PASSWORD_ALT", 0, NULL },
	{ NVProperty::USE_DHCP,           NVProperty::T_32BIT,  "USE_DHCP", 0, NULL },
	{ NVProperty::MAC_ADDR,           NVProperty::T_STR,    "MAC_ADDR", 0, NULL },
	{ NVProperty::NET_IP_ADDR,        NVProperty::T_STR,    "NET_IP_ADDR", 0, NULL },
	{ NVProperty::NET_IP_ROUTER,      NVProperty::T_STR,    "NET_IP_ROUTER", 0, NULL },
	{ NVProperty::NET_IP_DNS_SERVER,  NVProperty::T_STR,    "NET_IP_DNS_SERVER", 0, NULL },
	{ NVProperty::NET_IP_DNS_SERVER_ALT,  NVProperty::T_STR,    "NET_IP_DNS_SERVer_ALT", 0, NULL },

	{ NVProperty::NET_NTP_SERVER,  NVProperty::T_STR,    "NET_NTP_SERVER", 0, NULL },
	{ NVProperty::NET_NTP_SERVER_ALT,NVProperty::T_STR,    "NET_NTP_SERVER_ALT", 0, NULL },
	{ NVProperty::NET_NTP_GMTOFFSET, NVProperty::T_32BIT,  "NET_NTP_GMTOFFSET", 0, NULL },
	{ NVProperty::NET_NTP_DAYLIGHTOFFSET, NVProperty::T_32BIT,  "NET_NTP_DAYLIGHTOFFSET", 0, NULL },
	
	{ NVProperty::MQTT_SERVER,        NVProperty::T_STR,    "MQTT_SERVER", 0, NULL },
	{ NVProperty::MQTT_SERVER_ALT,    NVProperty::T_STR,    "MQTT_SERVER_ALT", 0, NULL },
	{ NVProperty::MQTT_TOPIC_INFO,    NVProperty::T_STR,    "MQTT_TOPIC_INFO", 0, NULL },
	{ NVProperty::MQTT_TOPIC_ALARM,   NVProperty::T_STR,    "MQTT_TOPIC_ALARM", 0, NULL },
	{ NVProperty::MQTT_TOPIC_CONTROL, NVProperty::T_STR,    "MQTT_TOPIC_CONTROL", 0, NULL },
	{ NVProperty::MQTT_TOPIC_4,       NVProperty::T_STR,    "MQTT_TOPIC_4", 0, NULL },
	{ NVProperty::MQTT_TOPIC_5,       NVProperty::T_STR,    "MQTT_TOPIC_5", 0, NULL },

	{ NVProperty::ALARM_STATUS,       NVProperty::T_32BIT,  "ALARM_STATUS", 0, NULL },

	/*
	* A user defined property
	*/
	{ UserProperty::MY_CITY,          NVProperty::T_STR,    "MY_CITY", 0, NULL },
};

void NVPropertyEditor(void)
{
	NVProperty p;
	
	rprintf("\nWelcome to the Property Editor which allows reading/writing/erasing non volatile settings\r\n\r\n");
	const char *help = "Properties cmds are:\r\n l (list properties), s (set e.g. s20=value), d (delete e.g. d20), q (quit)\r\n";
	rprintf(help);
	
	while(true) {
		char buf[80];
		
		memset(buf, 0, sizeof(buf));
		
		rprintf("$ ");
		const char *cmd = ConsoleReadline(buf, (int)sizeof(buf), true);
		if (!cmd)
			continue;
		
		switch(*cmd) {
			case 'l': {
				int cnt = ARRAYLEN(propArray);
				rprintf("List of Properties:\r\n");
				for (int i = 0; i < cnt; i++) {
					char tbuf[128];
					const char *value = "(not set)";
					memset(tbuf, 0, sizeof(tbuf));
					if (propArray[i].type <= NVProperty::T_32BIT) {
						int val = p.GetProperty(propArray[i].id, NVProperty::NVP_ENOENT);
						if (val != NVProperty::NVP_ENOENT) {
							value = tbuf;
							snprintf(tbuf, sizeof(tbuf), "%d (0x%x) %s", val, val, getNiceName(propArray[i].id, val));
						}
					} else if (propArray[i].type == NVProperty::T_STR) {
						const char *s = p.GetProperty(propArray[i].id, (const char *)NULL);
						if (s)
							value = s;
					}
					rprintf("%24s: %d=%s\r\n", propArray[i].name, propArray[i].id, value);
					wait_ms(2); // to flush buffers
				}
				rprintf("\r\n");
			}
			break;
			case 'd': {
				int id = strtol(++cmd, NULL, 0);

				if (id < 1) {
					dprintf("invalid parameter");
					break;
				}
				int cnt = ARRAYLEN(propArray);
				int slot = -1;
				for (int i = 0; i < cnt; i++) {
					if (propArray[i].id == id) {
						slot = i;
						break;
					}
				}
				if (slot == -1)
					dprintf("Property: %d not found in table", id);
				else {
					dprintf("Deleted Property: %d %s", id, propArray[slot].name);
					p.OpenPropertyStore(true); // enable for write
					p.EraseProperty(id);
				}
			}
			break;
			case 's': {
				char *v = (char *)strchr(++cmd, '=');
				if (!v)
					continue;
				*v++ = 0;
				
				int id = strtol(cmd, NULL, 0);
				if (id < 1) {
					dprintf("invalid parameter");
					break;
				}

				int cnt = ARRAYLEN(propArray);
				int slot = -1;
				for (int i = 0; i < cnt; i++) {
					if (propArray[i].id == id) {
						slot = i;
						break;
					}
				}
				if (slot == -1)
					dprintf("Property: %d not found in table", id);
				else {
					dprintf("Set Property: %s: %d=%s", propArray[slot].name, id, v);
					p.OpenPropertyStore(true); // enable for write
					if (propArray[slot].type == NVProperty::T_STR) {
						p.SetProperty(id, p.T_STR, v, p.S_FLASH);
					} else if (propArray[slot].type <= NVProperty::T_32BIT) {
						p.SetProperty(id, p.T_32BIT, (int)strtoll(v, NULL, 0), p.S_FLASH);
					}
				}
			}
			break;
			case 'i':
				dump("Flash-Area", (void *)0x803c000, 2048);
			break;
			case 'r':
				dprintf("ReorgProperties");
				p.ReorgProperties();
				break;
			case 'q':
				return;
			default:
				rprintf(help);
				break;
		}
	}
}


static const char *getNiceName(int id, int val)
{
  const char *name = "";
	
  switch(id) {
    case NVProperty::LORA_RADIO_TYPE:
      if (val == 1)
        return "RS_Node_Offline";
      else if (val == 2)
        return "RS_Node_Checking";
      else if (val == 3)
        return "RS_Node_Online";
      else if (val == 4)
        return "RS_Station_Basic";
      else if (val == 5)
        return "RS_Station_Server";
      break;
    default:
      break;
  }
  return name;
}
#endif
