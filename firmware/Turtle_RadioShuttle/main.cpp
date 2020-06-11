/*
 * Copyright (c) 2019 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 * Licensed under the Apache License, Version 2.0);
 */

/*
 * TODO:
 * Compiler Date/Time is not set correctly on startup using gcc
 * USB Serial block deepsleep
 */
#include "main.h"
#include "RadioTest.h"
#include "GenericPingPong.h"

DigitalOut greenLED(LED);
DigitalOut redLED(LED2);
PwmOut armatur(PB_6);
InterruptIn buttonIntr(USER_BUTTON);
volatile int pressedCount;
double  gpsLatitude_d = 0;
double  gpsLongitude_d = 0;
uint8_t gpsNumsat = 0;

BufferedSerial *gps = NULL;

void switchInput(void) {
    InterruptMSG(INT_BUTTON1);
}

void timerUpdate(void) {
    static LowPowerTimeout timeout;
    if (redLED == 0)
        timeout.attach_us(&timerUpdate, 20000); // setup to call timerUpdate after 20 millis
    else
        timeout.attach_us(&timerUpdate, 2000000); // setup to call timerUpdate after 2 seconds

    InterruptMSG(INT_TIMEOUT);
}


void armaturUpdate(void) {
    static LowPowerTimeout timeout;
    timeout.attach_us(&armaturUpdate, 700); // setup to call armaturUpdate after 20 millis

    static float ramper = 0.0;
    static float step = 0.001f;
    ramper += step;
    if (ramper < 0.0f || ramper > 1.0f)
    {
        step *= -1;
        ramper += step;
    }
    armatur.write (ramper);
}

char *parse_csv_no_copy (char *buf, int *argc, char **argv, char sep)
{
    if (!buf)
        return NULL;

    char *ret = buf;
    int argcMax = *argc;
    int i = 0;
    argv[i++] = buf;
    while (*buf)
    {
        if (*buf == sep)
        {
            *buf++ = '\0';
            if (i >= argcMax)
            {
                ret = NULL;
                break;
            }
            argv[i++] = buf;
        }
        else
            buf++;
    }

    *argc = i;
    return ret;
}

#define DUMP_ARGS \
    for (int i=0; i<argc; i++) \
        rprintf ("<%s> ", argv[i]); \
    rprintf ("\n")

#define equal(a,b) (strcmp((a),(b))==0)
#define STARTS_WITH(buf,start) (strncmp (buf, start, strlen (start)) == 0)

void message_handler_gpgga (int argc, char **argv)
{
    // DUMP_ARGS;
    //char *idStr        = argv[ 0]; (void) idStr;
    //char *timeStr      = argv[ 1]; (void) timeStr;
    //char *latStr       = argv[ 2]; (void) latStr;
    //char *card0Str     = argv[ 3]; (void) card0Str;
    //char *longStr      = argv[ 4]; (void) longStr;
    //char *card1Str     = argv[ 5]; (void) card1Str;
    //char *qualityStr   = argv[ 6]; (void) qualityStr;
    char *numsatStr    = argv[ 7]; (void) numsatStr;
    //char *hdopStr      = argv[ 8]; (void) hdopStr;
    //char *altStr       = argv[ 9]; (void) altStr;
    //char *altunitStr   = argv[10]; (void) altunitStr;
    //char *hgeoidStr    = argv[11]; (void) hgeoidStr;
    //char *hunitStr     = argv[12]; (void) hunitStr;
    //char *timeLastStr  = argv[13]; (void) timeLastStr;
    //char *dgpsRefIdStr = argv[14]; (void) dgpsRefIdStr;
    //char *cksumStr     = argv[15]; (void) cksumStr;

    //dprintf ("id:%s time:%s warnFlag:%s lat:%s latDir:%s long:%s longDir:%s speedKnots:%s course:%s date:%s magVar:%s magCard1:%s fixMode:%s cksum:%s",
    //         idStr, timeStr, warnFlagStr, latStr, latDirStr, longStr, longDirStr, speedKnotsStr, courseStr, dateStr, magVarStr, magCard1Str, fixModeStr, cksumStr);

    gpsNumsat      = atoi (numsatStr);
}

void message_handler_gprmc (int argc, char **argv)
{
    // DUMP_ARGS;
    char *idStr         = argv[ 0]; (void) idStr;              // $GPRMC
    char *timeStr       = argv[ 1]; (void) timeStr;            // 085859.00
    char *warnFlagStr   = argv[ 2]; (void) warnFlagStr;        // A = OK, V = Warning
    char *latStr        = argv[ 3]; (void) latStr;             // [0, 90]*100, e.g. 5922.66172 for 59 degrees and 22.66172 minutes (59+22.66172/60 degrees)
    char *latDirStr     = argv[ 4]; (void) latDirStr;          // N/S
    char *longStr       = argv[ 5]; (void) longStr;            // [0,180]*100, e.g. 01802.29009 for 18 degrees and 2.29009 minutes (18+2.29/60 degrees)
    char *longDirStr    = argv[ 6]; (void) longDirStr;         // W/E
    char *speedKnotsStr = argv[ 7]; (void) speedKnotsStr;      // 000.5
    char *courseStr     = argv[ 8]; (void) courseStr;          // 054.7
    char *dateStr       = argv[ 9]; (void) dateStr;            // 140520 for 2020-05-14
    char *magVarStr     = argv[10]; (void) magVarStr;          // 020.3
    char *magCard1Str   = argv[11]; (void) magCard1Str;        // W/E
    char *fixModeStr    = argv[12]; (void) fixModeStr;         // A/D/E/N/S
    char *cksumStr      = argv[13]; (void) cksumStr;           // TBD

    //dprintf ("id:%s time:%s warnFlag:%s lat:%s latDir:%s long:%s longDir:%s speedKnots:%s course:%s date:%s magVar:%s magCard1:%s fixMode:%s cksum:%s",
    //         idStr, timeStr, warnFlagStr, latStr, latDirStr, longStr, longDirStr, speedKnotsStr, courseStr, dateStr, magVarStr, magCard1Str, fixModeStr, cksumStr);

    if (! equal (warnFlagStr, "A"))
    {
        dprintf ("gps flag not ok, ignoring message");
        //return;
    }

    //dprintf ("warnFlagStr: '%s'", warnFlagStr);

    char latIntStr[3]  = { latStr[0], latStr[1], '\0' };
    char longIntStr[4] = { longStr[0], longStr[1], longStr[2], '\0' };
    latStr  += 2;
    longStr += 3;

    gpsLatitude_d  = atoi (latIntStr)  + strtod (latStr, NULL)  * (1.0 / 60.0);
    gpsLongitude_d = atoi (longIntStr) + strtod (longStr, NULL) * (1.0 / 60.0);

    if (equal (latDirStr, "S"))
        gpsLatitude_d = -gpsLatitude_d;

    if (equal (longDirStr, "W"))
        gpsLongitude_d = -gpsLongitude_d;
}

void on_gps_message (char *buf)
{
    //dprintf ("%s", buf);
    void (*handler) (int argc, char **argv);

    if (STARTS_WITH (buf, "$GPRMC,"))
        handler = message_handler_gprmc;
    else if (STARTS_WITH (buf, "$GPGGA,"))
        handler = message_handler_gpgga;
    else
        return;

    int argc=20;
    char *argv[20];
    char *p = strrchr (buf, '*');
    if (p)
        *p = ',';

    if (parse_csv_no_copy (buf, & argc, argv, ','))
        handler (argc, argv);
    else
        dprintf ("error while parsing csv");
}

void gpsProcess(void)
{
    static LowPowerTimeout timeout;
    timeout.attach_us(&gpsProcess, 500);

    static char buf[128] = {0};
    static int bi = 0;
    int bufLen = sizeof(buf) / sizeof(buf[0]);

    while (gps->readable ())
    {
        char c = gps->getc ();
        if (c == '\r')
            continue;

        if (bi < bufLen)
        {
            if (c == '\n')
            {
                buf[bi++] = '\0';
                on_gps_message (buf);
                bi = 0;
            }
            else
                buf[bi++] = c;
        }
        else
        {
            if (c == '\n')
                bi = 0;
        }
    }
}

void gpsInit (void)
{
    gps = new BufferedSerial(PB_6, PB_7);
    gps->baud (9600);
    gpsProcess ();
}

int main() {
    /*
     * inits the Serial or USBSerial when available (230400 baud).
     * If the serial uart is not is not connected it swiches to USB Serial
     * blinking LED means USBSerial detected, waiting for a connect.
     * It waits up to 30 seconds for a USB terminal connections 
     */
    InitSerial(30*1000, &greenLED, &buttonIntr);
    RunStartup();
    dprintf("Welcome till RadioShuttle v%d.%d", RS_MAJOR, RS_MINOR);
    timerUpdate(); // start timer for status blinked, can be disalbed to save energy
    armaturUpdate(); // start timer for status blinked, can be disalbed to save energy
    gpsInit ();
#if defined (USER_BUTTON_RISE) // attach switchInput function to the rising or falling edge
    buttonIntr.rise(&switchInput);
#else
    buttonIntr.fall(&switchInput);
#endif
    RunCommands(3000); // check x ms for any commands
    SX1276BasicTXRX(); // never completes

#ifdef FEATURE_LORA
    InitRadio();
#endif

    armatur.period (1.0f / 1000.0f);

    /*
     * Main event loop, process interrupts and goes to sleep when idle.
     * the green greenLED indicates CPU activity
     * the red redLED indicates that low power timerUpdate function is running.
     */
    while(true) {
        while ((readPendingInterrupts() == 0)) {
            greenLED = 0;
            sleep();
            greenLED = 1;
        }

        uint32_t pendirqs = readclrPendingInterrupts();
        if (pendirqs & INT_BUTTON1) {
#ifdef FEATURE_LORA
            greenLED = !greenLED;
            RadioUpdate(true); // pass the pressed user button to RadioShuttle
            dprintf ("radio update true\n");
#endif
        }
        if (pendirqs & INT_LORA) {
#ifdef FEATURE_LORA
            RadioUpdate(false);
            dprintf ("radio update false\n");
#endif
        }
        if (pendirqs & INT_TIMEOUT) {
            redLED = ! redLED;
        }
    }
}
