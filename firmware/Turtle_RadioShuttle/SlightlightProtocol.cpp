#include "mbed.h"
#include "PinMap.h"
#include "crc32.h"
#include "GenericPingPong.h"
#include "mbed/hal/us_ticker_api.h"
#if defined(DEVICE_LPTICKER) || defined(DEVICE_LOWPOWERTIMER) // LOWPOWERTIMER in older mbed versions
#define MyTimeout LowPowerTimeout
#else
#define MyTimeout Timeout
#endif
#include "sx1276-mbed-hal.h"
#include "main.h"
#ifdef  FEATURE_NVPROPERTY
#include <NVPropertyProviderInterface.h>
#include "NVProperty.h"
#endif

#ifdef FEATURE_LORA_PING_PONG

/* Set this flag to '1' to display debug messages on the console */
#define DEBUG_MESSAGE   0

/* Set this flag to '1' to use the LoRa modulation or to '0' to use FSK modulation */
#define USE_MODEM_LORA  1
#define USE_MODEM_FSK   !USE_MODEM_LORA
#define RF_FREQUENCY            RF_FREQUENCY_868_1  // Hz
#define TX_OUTPUT_POWER         14                  // 14 dBm
//#define TX_OUTPUT_POWER         0                  // 14 dBm

#if USE_MODEM_LORA == 1

#define LORA_BANDWIDTH          125000 // see LoRa bandwidth map for details
#define LORA_SPREADING_FACTOR   LORA_SF7
#define LORA_CODINGRATE         LORA_ERROR_CODING_RATE_4_5

#define LORA_PREAMBLE_LENGTH    8       // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT     5       // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_FHSS_ENABLED       false
#define LORA_NB_SYMB_HOP        4
#define LORA_IQ_INVERSION_ON    false
#define LORA_CRC_ENABLED        true

#elif USE_MODEM_FSK == 1

#define FSK_FDEV                25000     // Hz
#define FSK_DATARATE            9600     // bps
#define FSK_BANDWIDTH           50000     // Hz
#define FSK_AFC_BANDWIDTH       83333     // Hz
#define FSK_PREAMBLE_LENGTH     5         // Same for Tx and Rx
#define FSK_FIX_LENGTH_PAYLOAD_ON   false
#define FSK_CRC_ENABLED         true

#else
    #error "Please define a modem in the compiler options."
#endif

//#define RX_TIMEOUT_VALUE    3500	// in ms
#define RX_TIMEOUT_VALUE    3500	// in ms

#define BUFFER_SIZE         256       // Define the payload size here

/*
 *  Global variables declarations
 */
extern double gpsLatitude_d;
extern double gpsLongitude_d;
extern uint8_t gpsNumsat;
extern USBSerialBuffered *usb;
typedef enum
{
    LOWPOWER = 0,
    IDLE,

    RX,
    RX_TIMEOUT,
    RX_ERROR,
    RX_ACTIVE,

    TX,
    TX_TIMEOUT,

    CAD,
    CAD_DONE,
} AppStates_t;

#define PACKET_MAGIC_0 0xDEC1BEL
#define MAX_NODES 10
#define STRENGTH_TO_FLOAT(x)   ((float)    (x) * (0.01f))
#define STRENGTH_TO_INT(x)     ((int16_t) ((x) * (1.0f / 0.01f)))

// latitude  is the angle from south pole (-90 degrees) to north pole (+90 degrees), [-90, 90]
// longitude is the angle from the meridian (0 degrees), positive towards asia, negative towards usa [-180, 180]

#define LATITUDE_DOUBLE_TO_INT_FACTOR  ((double) (1u << 31u) / 90.0)
#define LATITUDE_INT_TO_DOUBLE_FACTOR  (90.0 / (double) (1u << 31u))

#define LONGITUDE_DOUBLE_TO_INT_FACTOR ((double) (1u << 31u) / 180.0)
#define LONGITUDE_INT_TO_DOUBLE_FACTOR (180.0 / (double) (1u << 31u))

#define LATITUDE_TO_INT(x)     ((x) * LATITUDE_DOUBLE_TO_INT_FACTOR)
#define LATITUDE_TO_DOUBLE(x)  ((x) * LATITUDE_INT_TO_DOUBLE_FACTOR)
#define LONGITUDE_TO_INT(x)    ((x) * LONGITUDE_DOUBLE_TO_INT_FACTOR)
#define LONGITUDE_TO_DOUBLE(x) ((x) * LONGITUDE_INT_TO_DOUBLE_FACTOR)

#if USE_MODEM_LORA == 1

  #if LORA_SPREADING_FACTOR == LORA_SF7
    #define NUM 128 // set this number to 2^LORA_SPREADING_FACTOR
    #define DATA_RATE (LORA_SPREADING_FACTOR * LORA_BANDWIDTH / NUM) * LORA_CODINGRATE
  #else
    #error "can't derive DATA_RATE, fix the code please"
  #endif
  #define PACKET_LENGTH (LORA_PREAMBLE_LENGTH + 8 * sizeof (Packet))

#elif USE_MODEM_FSK == 1

  #define DATA_RATE FSK_DATARATE
  #define PACKET_LENGTH (FSK_PREAMBLE_LENGTH + 8 * sizeof (Packet))

#else

  #error "please define DATA_RATE and PACKET_LENGTH"

#endif

#define RX_TIMEOUT_MIN_MS ((2 * PACKET_LENGTH * 1000) / DATA_RATE) // PACKET_LENGTH * 1000 / DATA_RATE = 62 ms if nothing has changed...
#define RX_TIMEOUT_MAX_MS ((40 * PACKET_LENGTH * 1000) / DATA_RATE) 

#define MYRAND_MAX 4294967294

typedef struct
{
    uint16_t address;
    int16_t strength;
} NodeStrength;

typedef struct
{
    uint32_t magic0;
    uint16_t srcAddress;
    uint8_t  state;
    uint8_t  numNodes;
    uint8_t  gpsNumsat;
    int32_t  gpsLongitude;
    int32_t  gpsLatitude;
    NodeStrength nodes[MAX_NODES];
    uint32_t crc32;
} Packet;

typedef struct
{
    float strength;
    uint16_t address;
    uint32_t tspMs;
} NodeCache;

NodeCache cache[MAX_NODES];
uint8_t numNodes = 0;
uint16_t deviceId = 0;
float gainOffsetTx = 0;
float gainOffsetRx = 0;
int32_t totalRssi = 0;
int32_t totalSamples = 0;
uint32_t lastSendTspMs = 0;
static uint32_t myrandState = 0xCAFEBABE;
volatile AppStates_t State = LOWPOWER;
static RadioEvents_t RadioEvents;
SX1276Generic *Radio = NULL;

uint16_t BufferSize = BUFFER_SIZE;
uint8_t *Buffer;

int16_t lastRssi = 0;
int8_t lastSnr = 0;

#define NODE_TIMEOUT_MS 180000

static uint32_t get_ms (void)
{
    // FIXME will overflow every 4000 s
    return us_ticker_read () / 1000;
}

void myrand_init (void)
{
    if (! deviceId)
    {
        dprintf ("pseudorandom generator failed to initialise: deviceId is zero");
        return;
    }

    myrandState *= deviceId;
}

uint32_t myrand ()
{
    myrandState = (myrandState << 1) |
        (((myrandState >> (31)) ^
          (myrandState >> (21)) ^
          (myrandState >> (1 )) ^
          (myrandState >> (0 ))) & 1);

    return myrandState;
}


void clean_up_unresponsive_nodes (void)
{
    uint32_t tspMs = get_ms ();
    int i0,i1;
    for (i0=0,i1=0; i0<numNodes && i1<numNodes; i0++, i1++)
    {
        uint32_t elapsedMs = tspMs - cache[i1].tspMs;
        if (elapsedMs > 2 * NODE_TIMEOUT_MS)
        {
            // FIXME: handling of overflow in get_ms will come later...
            dprintf ("elapsedMs: %lu, working around overflow", elapsedMs);
            cache[i1].tspMs = get_ms ();
            elapsedMs = 0;
        }
        while (elapsedMs > NODE_TIMEOUT_MS && i1 < numNodes)
        {
            dprintf ("%04x: elapsed time was %f s, removing node", cache[i1].address, 1e-3f * elapsedMs);
            elapsedMs = tspMs - cache[++i1].tspMs;
        }
        if (i1 == numNodes)
            break;
        if (i0 != i1)
            memcpy(& cache[i0], & cache[i1], sizeof (cache[i0]));
    }
    numNodes = i0;
}

Packet *craft_packet (Packet *p)
{
    uint32_t tspMs = get_ms ();
    clean_up_unresponsive_nodes ();
    p->magic0 = PACKET_MAGIC_0;
    p->srcAddress = deviceId;
    p->state = 0;
    p->numNodes = numNodes;
    p->gpsLatitude  = LATITUDE_TO_INT (gpsLatitude_d);
    p->gpsLongitude = LONGITUDE_TO_INT (gpsLongitude_d);
    dprintf ("TX BEACON %x (%u nodes) (gps[%d] %lf %lf => %ld %ld)", p->srcAddress, p->numNodes, gpsNumsat, gpsLatitude_d, gpsLongitude_d, p->gpsLatitude, p->gpsLongitude);
    gpsLatitude_d = 0;
    gpsLongitude_d = 0;
    int ci;
    for (ci=0; ci<numNodes; ci++)
    {
        int pi = ci;
        p->nodes[pi].address  = cache[ci].address;
        p->nodes[pi].strength = STRENGTH_TO_INT (cache[ci].strength);
        uint32_t elapsedMs = tspMs - cache[ci].tspMs;
        dprintf (">>> %x: %f dBm elapsed: %f s", p->nodes[pi].address, STRENGTH_TO_FLOAT (p->nodes[pi].strength), elapsedMs * 1e-3f);
    }
    for (; ci<MAX_NODES; ci++)
    {
        int pi = ci;
        p->nodes[pi].address  = 0;
        p->nodes[pi].strength = 0;
    }
    p->crc32 = 0;
    p->crc32 = crc32 (0, p, sizeof (*p));
    return p;
}

void on_packet (Packet *p, int16_t rssi, int8_t snr, float rssiFine)
{
    //dprintf ("on_packet called");
    if (p->magic0 != PACKET_MAGIC_0)
    {
        dprintf ("bad magic0, ignoring packet");
        return;
    }

    uint32_t crc32Expected = p->crc32;
    p->crc32 = 0;
    uint32_t crc32Computed = crc32 (0, p, sizeof (*p));
    if (crc32Expected != crc32Computed)
    {
        dprintf ("bad crc32 from %04x, ignoring packet", p->srcAddress);
        return;
    }

    //dprintf ("%x <- %x: rssi: %d snr: %d", deviceId, p->srcAddress, rssi, snr);

    // first, print the information about all the nodes in the packet.
    if (p->numNodes > MAX_NODES)
    {
        dprintf ("sanity check failed: p->numNodes=%u > MAX_NODES=%u", p->numNodes, MAX_NODES);
        return;
    }

    double latd  = LATITUDE_TO_DOUBLE (p->gpsLatitude);
    double longd = LONGITUDE_TO_DOUBLE (p->gpsLongitude);
    dprintf ("RX BEACON %x (%u nodes) (gps[%d] %lf %lf <= %ld %ld)", p->srcAddress, p->numNodes, p->gpsNumsat, latd, longd, p->gpsLatitude, p->gpsLongitude);
    //dprintf ("RX BEACON %x (%u nodes)", p->srcAddress, p->numNodes);
    for (int pi=0; pi<p->numNodes; pi++)
        dprintf ("    %x: %f dBm", p->nodes[pi].address, STRENGTH_TO_FLOAT (p->nodes[pi].strength));

    // update signal strength for node
    int ci;
    for (ci=0; ci<numNodes; ci++)
        if (cache[ci].address == p->srcAddress)
            break;

    if (ci == numNodes)
    {
        if (numNodes < MAX_NODES)
        {
            numNodes++;
            dprintf ("node %04x added", p->srcAddress);
            cache[ci].address = p->srcAddress;
        }
        else
        {
            dprintf ("could not add new node: cache is full");
            return;
        }
    }

    float newStrength; // strength computed below according to datasheet sx1278 p.87
    if (snr < 0)
       newStrength = -164.0 + rssiFine + 0.25f * snr;
    else
       newStrength = -164.0 + (16.0f / 15.0f) * rssiFine;
    cache[ci].strength = newStrength; // FIXME: when it's working, apply filtering
    cache[ci].tspMs = get_ms ();
    //dprintf ("cache[%d].{address=%04x strength=%f tspMs=%ld}. distance:%f", ci, cache[ci].address, cache[ci].strength, cache[ci].tspMs, distance);
}

uint32_t random_wait_time_ms (void)
{
    uint32_t min = RX_TIMEOUT_MIN_MS;
    uint32_t max = RX_TIMEOUT_MAX_MS;

    uint32_t randWait;
    randWait = (uint32_t) (min + (((uint64_t) (max - min) * myrand ()) / MYRAND_MAX));// + offset;

    return randWait;
}

extern DigitalOut greenLED;
extern DigitalOut redLED;

static void radio_init (void)
{
    if (Radio)
        delete Radio;
    Radio = NULL;

#ifdef TARGET_DISCO_L072CZ_LRWAN1
#error "I didn't know this code would be compiled"
    Radio = new SX1276Generic(NULL, MURATA_SX1276,
    		LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
        	LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5,
        	LORA_ANT_RX, LORA_ANT_TX, LORA_ANT_BOOST, LORA_TCXO);
#elif defined(HELTECL432_REV1)
    // NOTE: This is the if-branch that is compiled / Manne
    Radio = new SX1276Generic(NULL, HELTEC_L4_1276,
			LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5,
			LORA_ANT_PWR);
#else // RFM95
#error "I didn't know this code would be compiled"
    Radio = new SX1276Generic(NULL, RFM95_SX1276,
			LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5);

#endif

    dprintf("SX1276 Ping Pong Demo Application" );
    dprintf("Freqency: %.1f", (double)RF_FREQUENCY/1000000.0);
    dprintf("TXPower: %d dBm",  TX_OUTPUT_POWER);
#if USE_MODEM_LORA == 1
    dprintf("Bandwidth: %d Hz", LORA_BANDWIDTH);
    dprintf("Spreading factor: SF%d", LORA_SPREADING_FACTOR);
#elif USE_MODEM_FSK == 1
    dprintf("Bandwidth: %d kHz",  FSK_BANDWIDTH);
    dprintf("Baudrate: %d", FSK_DATARATE);
#endif
    // Initialize Radio driver
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.RxError = OnRxError;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.FhssChangeChannel = OnFhssChangeChannel;
    RadioEvents.CadDone = OnCadDone;
    if (Radio->Init( &RadioEvents ) == false) {
        while(1) {
        	dprintf("Radio could not be detected!");
        	wait( 1 );
        }
    }

    switch(Radio->DetectBoardType()) {
        case SX1276MB1LAS:
            if (DEBUG_MESSAGE)
                dprintf(" > Board Type: SX1276MB1LAS <");
            break;
        case SX1276MB1MAS:
            if (DEBUG_MESSAGE)
                dprintf(" > Board Type: SX1276MB1LAS <");
			break;
        case MURATA_SX1276:
            if (DEBUG_MESSAGE)
            	dprintf(" > Board Type: MURATA_SX1276_STM32L0 <");
            break;
        case RFM95_SX1276:
            if (DEBUG_MESSAGE)
                dprintf(" > HopeRF RFM95xx <");
            break;
        default:
            dprintf(" > Board Type: unknown <");
    }

    // FIXME: RF_FREQUENCY is a 868 MHz frequency. What should it be set to?
    //Radio->SetChannel(RF_FREQUENCY );
    Radio->SetChannel(433925000);

#if USE_MODEM_LORA == 1

    if (LORA_FHSS_ENABLED)
        dprintf("             > LORA FHSS Mode <");
    if (!LORA_FHSS_ENABLED)
        dprintf("             > LORA Mode <");

    Radio->SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                         LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                         LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                         LORA_CRC_ENABLED, LORA_FHSS_ENABLED, LORA_NB_SYMB_HOP,
                         LORA_IQ_INVERSION_ON, 2000 );

    Radio->SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                         LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                         LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON, 0,
                         LORA_CRC_ENABLED, LORA_FHSS_ENABLED, LORA_NB_SYMB_HOP,
                         LORA_IQ_INVERSION_ON, true );

#elif USE_MODEM_FSK == 1

    dprintf("              > FSK Mode <");

    /*!
     * @brief Sets the transmission parameters
     *
     * @param [IN] modem        Radio modem to be used [0: FSK, 1: LoRa]
     * @param [IN] power        Sets the output power [dBm]
     * @param [IN] fdev         Sets the frequency deviation ( FSK only )
     *                          FSK : [Hz]
     *                          LoRa: 0
     * @param [IN] bandwidth    Sets the bandwidth ( LoRa only )
     *                          FSK : 0
     *                          LoRa: [0: 125 kHz, 1: 250 kHz,
     *                                 2: 500 kHz, 3: Reserved]
     * @param [IN] datarate     Sets the Datarate
     *                          FSK : 600..300000 bits/s
     *                          LoRa: [6: 64, 7: 128, 8: 256, 9: 512,
     *                                10: 1024, 11: 2048, 12: 4096  chips]
     * @param [IN] coderate     Sets the coding rate ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
     * @param [IN] preambleLen  Sets the preamble length
     * @param [IN] fixLen       Fixed length packets [0: variable, 1: fixed]
     * @param [IN] crcOn        Enables disables the CRC [0: OFF, 1: ON]
     * @param [IN] freqHopOn    Enables disables the intra-packet frequency hopping  [0: OFF, 1: ON] (LoRa only)
     * @param [IN] hopPeriod    Number of symbols bewteen each hop (LoRa only)
     * @param [IN] iqInverted   Inverts IQ signals ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: [0: not inverted, 1: inverted]
     * @param [IN] timeout      Transmission timeout [ms]
     */
    //virtual void SetTxConfig( RadioModems_t modem, int8_t power, uint32_t fdev,
    //                          uint32_t bandwidth, uint32_t datarate,
    //                          uint8_t coderate, uint16_t preambleLen,
    //                          bool fixLen, bool crcOn, bool freqHopOn,
    //                          uint8_t hopPeriod, bool iqInverted, uint32_t timeout );
    Radio->SetTxConfig( MODEM_FSK, TX_OUTPUT_POWER, FSK_FDEV,
                        0, FSK_DATARATE,
                        0, FSK_PREAMBLE_LENGTH,
                        FSK_FIX_LENGTH_PAYLOAD_ON, FSK_CRC_ENABLED, 0,
                        0, 0, 2000 );

    /*!
     * @brief Sets the reception parameters
     *
     * @param [IN] modem        Radio modem to be used [0: FSK, 1: LoRa]
     * @param [IN] bandwidth    Sets the bandwidth
     *                          FSK : >= 2600 and <= 250000 Hz
     *                          LoRa: [0: 125 kHz, 1: 250 kHz,
     *                                 2: 500 kHz, 3: Reserved]
     * @param [IN] datarate     Sets the Datarate
     *                          FSK : 600..300000 bits/s
     *                          LoRa: [6: 64, 7: 128, 8: 256, 9: 512,
     *                                10: 1024, 11: 2048, 12: 4096  chips]
     * @param [IN] coderate     Sets the coding rate ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
     * @param [IN] bandwidthAfc Sets the AFC Bandwidth ( FSK only )
     *                          FSK : >= 2600 and <= 250000 Hz
     *                          LoRa: N/A ( set to 0 )
     * @param [IN] preambleLen  Sets the Preamble length ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: Length in symbols ( the hardware adds 4 more symbols )
     * @param [IN] symbTimeout  Sets the RxSingle timeout value
     *                          FSK : timeout number of bytes
     *                          LoRa: timeout in symbols
     * @param [IN] fixLen       Fixed length packets [0: variable, 1: fixed]
     * @param [IN] payloadLen   Sets payload length when fixed lenght is used
     * @param [IN] crcOn        Enables/Disables the CRC [0: OFF, 1: ON]
     * @param [IN] freqHopOn    Enables disables the intra-packet frequency hopping  [0: OFF, 1: ON] (LoRa only)
     * @param [IN] hopPeriod    Number of symbols bewteen each hop (LoRa only)
     * @param [IN] iqInverted   Inverts IQ signals ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: [0: not inverted, 1: inverted]
     * @param [IN] rxContinuous Sets the reception in continuous mode
     *                          [false: single mode, true: continuous mode]
     */
    //virtual void SetRxConfig ( RadioModems_t modem, uint32_t bandwidth, uint32_t datarate,
    //                           uint8_t coderate, uint32_t bandwidthAfc, uint16_t preambleLen,
    //                           uint16_t symbTimeout, bool fixLen, uint8_t payloadLen,
    //                           bool crcOn, bool freqHopOn, uint8_t hopPeriod,
    //                           bool iqInverted, bool rxContinuous );
    Radio->SetRxConfig( MODEM_FSK, FSK_BANDWIDTH, FSK_DATARATE,
                         0, FSK_AFC_BANDWIDTH, FSK_PREAMBLE_LENGTH,
                         0, FSK_FIX_LENGTH_PAYLOAD_ON, 0,
                         FSK_CRC_ENABLED, 0, 0,
                         false, true );
#else

#error "Please define a modem in the compiler options."

#endif
}

int SX1276BasicTXRX()
{
    NVProperty prop;
    uint32_t deviceIdU32 = prop.GetProperty(prop.LORA_DEVICE_ID, 0);
    deviceId = (uint16_t) deviceIdU32;
    if (deviceId != deviceIdU32)
        deviceId = 0xffff; // if we have any nodes with this id in the system we know 16 bits is not enough (and we need to increase)

    myrand_init ();

    gainOffsetTx = 1e-6 * prop.GetProperty(prop.GAIN_OFFSET_TX, 0);
    gainOffsetRx = 1e-6 * prop.GetProperty(prop.GAIN_OFFSET_RX, 0);

#if( defined ( TARGET_KL25Z ) || defined ( TARGET_LPC11U6X ) )
    DigitalOut *led = new DigitalOut(LED2);
#error "I didn't know this code would be compiled"
#elif defined(TARGET_NUCLEO_L073RZ) || defined (TARGET_DISCO_L072CZ_LRWAN1)
    DigitalOut *led = new DigitalOut(LED4);   // RX red
#error "I didn't know this code would be compiled"
#else
    // NOTE: This is the if-branch that is compiled / Manne
#endif

    Buffer = new  uint8_t[BUFFER_SIZE];

    radio_init ();

    if (DEBUG_MESSAGE)
        dprintf("Starting Ping-Pong loop");

    Radio->Rx( RX_TIMEOUT_VALUE );

    while( 1 )
    {
#ifdef TARGET_STM32L4
        // NOTE: This is the if-branch that is compiled / Manne
        WatchDogUpdate();
#endif

        static int headerReceived = 0;
        if (headerReceived)
        {
             // FIXME: make sure we are not spamming the spi-bus
             if (Radio->RxSignalPending ())
             {
                 uint16_t rssi = Radio->MyGetRssi ();
                 totalRssi += rssi;
                 totalSamples++;
                 //dprintf ("rx active rssi=%d total=%ld cnt=%ld", rssi, totalRssi, totalSamples);
             }
             else
             {
                 //dprintf ("signal not pending");
                 headerReceived = 0;
             }
        }
        else
        {
            headerReceived = Radio->RxHeaderReceived ();
            if (headerReceived)
            {
                //dprintf ("header received");
                totalRssi = 0;
                totalSamples = 0;
            }
        }

        static Packet packet;
        uint32_t elapsedMsSinceTx = get_ms () - lastSendTspMs;
        uint32_t rxMs;
        if (elapsedMsSinceTx > 5000)
            rxMs = RX_TIMEOUT_MAX_MS;
        else if (elapsedMsSinceTx > 1000)
            rxMs = 1000;
        else
            rxMs = 1000 - elapsedMsSinceTx + RX_TIMEOUT_MIN_MS;

        switch (State)
        {
         case RX_ERROR:
             BufferSize = 0;
             // fall through
         case RX:
             //dprintf ("rx recv");
             // we have received a packet. Parse it and wait for next packet.
             if( BufferSize == sizeof (packet))
             {
                 redLED = 1;
                 Packet *p = (Packet *) Buffer;
                 float rssiFine = totalSamples ? (float) totalRssi / (float) totalSamples : lastRssi;
                 BufferSize = 0;
                 on_packet (p, lastRssi, lastSnr, rssiFine);
                 memset ( p, 0, sizeof (Packet));
                 //dprintf ("on_packet. rssi=%d rssiFine=%f", lastRssi, rssiFine);
             }
             else if (BufferSize)
             {
                 dprintf ("incorrect packet size, %u != %u", BufferSize, sizeof (packet));
             }
             State = LOWPOWER;
             //dprintf ("%lu line %d, rxMs %lu", us_ticker_read (), __LINE__, rxMs);
             Radio->Rx (rxMs);
             redLED = 0;
             break;
         case RX_TIMEOUT:
             // we have waited a random time without receiving anything. Send our packet
             //dprintf ("rx timeout");
             if ((get_ms () - lastSendTspMs) > 5000)
             {
                 wait_ms (random_wait_time_ms ());
                 while (Radio->RxSignalPending ())
                 {
                     dprintf ("RxSignalPending, waiting");
                     wait_ms (random_wait_time_ms ());
                 }

                 craft_packet (& packet);
                 greenLED = 1;
                 State = LOWPOWER;
                 lastSendTspMs = get_ms ();
                 //dprintf ("%lu line %d, rxMs %lu", us_ticker_read (), __LINE__, rxMs);
                 Radio->Send (& packet, sizeof (packet));
             }
             else
             {
                 //dprintf ("%lu line %d, rxMs %lu", us_ticker_read (), __LINE__, rxMs);
                 Radio->Rx (rxMs);
                 State = LOWPOWER;
             }
             break;
         case TX:
             //dprintf ("tx");
             // we have sent a packet. we're done. start listening for replies from nodes.
             greenLED = 0;
             State = LOWPOWER;
             //dprintf ("%lu line %d, rxMs %lu", us_ticker_read (), __LINE__, rxMs);
             Radio->Rx (rxMs);
             break;
         case TX_TIMEOUT:
             State = LOWPOWER;
             //dprintf ("%lu line %d, rxMs %lu", us_ticker_read (), __LINE__, rxMs);
             Radio->Rx (rxMs);
             greenLED = 0;
             break;
         case LOWPOWER:
             break;
         default:
             State = LOWPOWER;
             break;
        }
        wait_ms (5);
    }
}

void OnTxDone(void *radio, void *userThisPtr, void *userData)
{
    //dprintf ("%lu line %d", us_ticker_read (), __LINE__);
    if (State != LOWPOWER) dprintf ("overriding state");
    //SX1276Generic *r = (SX1276Generic *)radio;
    //r->Sleep( );
    State = TX;
    if (DEBUG_MESSAGE)
        dprintf("> OnTxDone");
}

void OnRxDone(void *radio, void *userThisPtr, void *userData, uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    //dprintf ("%lu line %d", us_ticker_read (), __LINE__);
    if (State != LOWPOWER) dprintf ("overriding state");
    SX1276Generic *r = (SX1276Generic *)radio;
    r->Sleep( );
    if (size > BUFFER_SIZE)
    {
        dprintf ("error: received packet does not fit in buffer. size:%u", size);
        return;
    }
    memcpy( Buffer, payload, size );
    BufferSize = size;
    State = RX;
    lastRssi = rssi;
    lastSnr  = snr;
    if (DEBUG_MESSAGE)
    {
        dprintf("> OnRxDone");
        dump("Data:", payload, size);
    }
}

void OnTxTimeout(void *radio, void *userThisPtr, void *userData)
{
    //dprintf ("OnTxTimeout, resetting modem");
    if (State != LOWPOWER) dprintf ("overriding state");
    SX1276Generic *r = (SX1276Generic *)radio;
    r->Sleep( );
    State = TX_TIMEOUT;
    radio_init ();
    if(DEBUG_MESSAGE)
        dprintf("> OnTxTimeout");
}

void OnRxTimeout(void *radio, void *userThisPtr, void *userData)
{
    //dprintf ("%lu line %d", us_ticker_read (), __LINE__);
    if (State != LOWPOWER) dprintf ("overriding state");
    SX1276Generic *r = (SX1276Generic *)radio;
    r->Sleep( );
    State = RX_TIMEOUT;
    if (DEBUG_MESSAGE)
        dprintf("> OnRxTimeout");
}

void OnRxError(void *radio, void *userThisPtr, void *userData)
{
    //dprintf ("%lu line %d", us_ticker_read (), __LINE__);
    SX1276Generic *r = (SX1276Generic *)radio;
    r->Sleep( );
    State = RX_ERROR;
    if (DEBUG_MESSAGE)
        dprintf("> OnRxError");
}
void OnFhssChangeChannel (void *radio, void *userThisPtr, void *userData, uint8_t currentChannel )
{
    dprintf ("%lu line %d", us_ticker_read (), __LINE__);
}

void OnCadDone (void *radio, void *userThisPtr, void *userData, bool channelActivityDetected )
{
    dprintf ("%lu line %d", us_ticker_read (), __LINE__);
}
#endif
