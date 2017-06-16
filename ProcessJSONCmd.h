/************************************************************************/
/*                                                                      */
/*    ProcessJSONCmd.h                                                  */
/*                                                                      */
/*    Periodic Task the process command submitted via JSON              */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    8/3/2016 (KeithV): Created                                        */
/************************************************************************/

#ifndef ProcessJSONCmd_h
#define ProcessJSONCmd_h

typedef enum
{
    gpioNone,
    gpioTriState,
    gpioInput,
    gpioInputPullDown,
    gpioInputPullUp,
    gpioOutput,
} GPIOSTATE;

typedef enum
{
    TRGTPNone,
    TRGTPRising,
    TRGTPFalling
} TRGTP;

typedef struct _BIDX
{
    // these are input parameters.
    uint64_t        msps;           // msps in mHz
    int64_t         psDelay;        // what is the delay in pico sec
    int32_t         cBuff;          // the requested size of the buffer

    // this get calculated by CalculateBufferIndexes()
    int64_t         dlTrig2POI;     // the delay in samples at the msps freq
    int32_t         iPOI;           // where in the return buffer is the POI
    int32_t         iTrg;           // where in the return buffer is the TRG; same as iTrigDMA except when -1
    int32_t         iTrigDMA;       // Where the Trigger is

    uint16_t        tmrPreScalar;   // The prescalar to use on the sample rate timer
    uint16_t        tmrPeriod;      // what is the timer value to use to achieve the sample rate
    bool            fInterleave;    // if we are to interleave the ADCs, or just use one

    int32_t         cBeforeTrig;    // How many samples before the trigger can be armed
    int64_t         cDelayTmr;      // total number of ticks of the delay timer

    // these are filled in by the interrupt routine
    int32_t         iDMATrig;       // Where in the DMA pointed when the trigger interrupt entered (will be off by iDMATrig-trigger.indexBuff)

    // these are const values
    uint32_t const  pbClkSampTmr;   // in ticks per second
    uint64_t const  mHzInterleave;  // at what frequency we interleave at
    int32_t  const  cDMA;           // how big is the DMA buffer (must be divisible by 2)
    int32_t  const  cDMABuff;       // What is the usable data in the DMA buffer, this is by definition cbDMA - cbDMASlop
    int32_t  const  cDMASlop;       // how much slop at the end of the dma buffer (must be divisible by 2)
} BIDX;

typedef struct _PSTATE
{
    STATE           parsing;        // parsing state
    STATE           processing;     // processing state in ProcessJSON
    STATE           instrument;     // underlying instrument state
} PSTATE;

typedef struct _TTE
{
    INSTR_ID    instrID;    // what instrument is this
    PSTATE *    pstate;     // a pointer to the instrument state structure
    STATE *     pLockState; // the lock state of the buffer in use   
    bool        fWorking;   // is it complete or still in the works; generic working flag
    uint32_t    cTMR;       // number of timer rolls in the timer before stop
    int32_t     iTMR;       // initial value in the timer
    int64_t     tmrTicks;   // total number of ticks
} TTE;

typedef struct _ITRG
{
    PSTATE          state;          // all of the parsing states
    bool            fUseCh2;        // Just a flag to know if we are setting up for channel 1 or 2
    INSTR_ID        idTrigSrc;      // instrument ID for the trigger
    TRGTP           triggerType;    // what kind of trigger, rising/falling edge
    int16_t         mvLower;        // lower limit of the trigger for OSC
    int16_t         mvHigher;       // upper limit of the trigger for OSC
    int16_t         negEdge;        // negative edge triggers for the LA
    int16_t         posEdge;        // positive edge triggers for the LA
    uint32_t        cTargets;       // number of instruments to trigger; while parsing targets, is scope indicator
    uint32_t        cRun;           // number of instruments to trigger; while parsing targets, is scope indicator
    TTE             rgtte[4];       // 2 analogs and logic analyzer potential triggered instruments, AWG for source

    // these are values for internal trigger use only
    uint32_t        acqCount;       // the count for this trigger
    int32_t         indexBuff;      // the location in the SRC buffer where the trigger is; this is the final buffer.
    uint32_t        cTMR;           // number of timer rolls; may need this per instrument
    uint32_t        iTTE;           // the TTE currently in use
} ITRG;

typedef struct _IDC
{
    PSTATE          state;          // all of the parsing states
    INSTR_ID const  id;             // the instrument ID
    int32_t         mVolts;         // mVolt setting for the DC out
} IDC;

typedef struct _IAWG
{
    PSTATE          state;          // all of the parsing states
    INSTR_ID const  id;             // the instrument ID
    WAVEFORM        waveform;       // the waveform requested
    uint32_t        freq;           // requested frequency
    uint32_t        sps;            // samples per second
    int32_t         mvP2P;          // Peek to Peek voltage in mv
    int32_t         mvOffset;       // offset in mv
    uint32_t        dutyCycle;      // duty cycle of the waveform
    uint32_t        cBuff;          // how many entries in the buffer
    uint16_t * const pBuff;         // pointer to the data buffer
} IAWG;

typedef struct _IOSC
{
    PSTATE          state;          // all of the parsing states
    INSTR_ID const  id;             // the instrument ID
    int32_t         mvOffset;       // mVolt offset of the input
    int32_t         gain;           // gain, set to 1-4//
    uint32_t        acqCount;       // What is the acquisition count of the current buffer
    
    BIDX            bidx;           // calculated indexes
    
    // this are used during sampling
    uint32_t        iBinOffset;     // the offset of the binary in the file after the JSON
    STATE           buffLock;       // the locked state of the buffer
    int16_t * const pBuff;          // point to the data buffer
} IOSC;

typedef struct _ILA
{
    PSTATE          state;          // all of the parsing states
    uint16_t        bitMask;        // which bits to mask for return
    uint32_t        acqCount;       // What is the acquisition count of the current buffer
   
    BIDX            bidx;           // calculated indexes

    // this are used during sampling
    uint32_t        iBinOffset;     // the offset of the binary in the file after the JSON
    STATE           buffLock;       // the locked state of the buffer
    uint16_t * const pBuff;         // point to the data buffer
} ILA;

typedef struct _IGPIOPIN
{
    GPIOSTATE   gpioState;
    PORTCH *    pPort;
    uint32_t    pinMask;
} IGPIOPIN;

typedef struct _IGPIO
{
    STATE       parsing;        // parsing state
    int32_t     iPin;           // the working pin of interest in pin[]
    GPIOSTATE   pinState;       // the working new pin state
    uint32_t    value;          // for output, new state hi or low
    IGPIOPIN    pin[NBRGPIO];   // the current pin states
} IGPIO;

typedef struct _ICAL
{
    PSTATE      state;          // all of the parsing states
    CFGNAME     config;         // where to save the data, FACTORY /  USER / 
} ICAL;

typedef struct _IBOOT
{
    STATE       processing;     // all of the parsing states
    uint32_t    tStart;         // where to save the data, FACTORY /  USER / 
} IBOOT;

typedef struct _IWIFI
{
    PSTATE          state;      // all of the parsing states
    NIC_ADP         nicADP;     // what adaptor we are asking about
    VOLTYPE         vol;        // the volume to use
    bool            fScanReady; // if the scan has been done and data is available
    bool            fForceConn; // if the scan has been done and data is available
    bool            fWorking;   // use the working parameter set
    WiFiConnectInfo wifiAConn;  // active connection info
    WiFiConnectInfo wifiWConn;  // working connection info
    WiFiScanInfo    wifiScan;   // scan info
    char            szPassphrase[DEWF_MAX_PASS_PHRASE+1];
    char            szSSID[DEWF_MAX_SSID_LENGTH+1];
} IWIFI;

typedef struct _MFGTEST
{
    uint32_t testNbr;
} MFGTEST;

// generic parsing.
typedef struct _PJCMD
{
    ITRG    trigger;

    // dc channels
    IDC     idcCh1;
    IDC     idcCh2;

    // awg channel
    IAWG    iawg;

    // OSC channels
    IOSC    ioscCh1;
    IOSC    ioscCh2;

    // Logic analyzer
    ILA   ila;

    // GPIO
    IGPIO   igpio;

    // calibration
    ICAL    iCal;
    
    // enter bootloader
    IBOOT   iBoot;

    // wifi info
    IWIFI   iWiFi;

    // manufacturing test data
    MFGTEST iMfgTest;

#ifdef __cplusplus
    _PJCMD() :  trigger({{Idle, Idle, Idle}, false, NULL_ID, TRGTPNone, 0, 0, 0, 0, 0, 0, {{NULL_ID, NULL, NULL, false, 0, 0, 0}, {NULL_ID, NULL, NULL, false, 0, 0, 0}, {NULL_ID, NULL, NULL, false, 0, 0, 0}, {NULL_ID, NULL, NULL, false, 0, 0, 0}}, 0, 0, 0, 0}),
                idcCh1({ {Idle, Idle, Idle}, DCVOLT1_ID, 0}),                                           
                idcCh2({ {Idle, Idle, Idle}, DCVOLT2_ID, 0}),      
                iawg({   {Idle, Idle, Idle}, AWG1_ID, waveNone, 0, 0, 0, 0, 50, AWGBUFFSIZE, rgAWGBuff}),
                ioscCh1({   {Idle, Idle, Idle}, OSC1_ID, 0, 4, 0, 
                        {MAXmSAMPLEFREQ, 0, AINMAXBUFFSIZE, 0, AINMAXBUFFSIZE/2, AINMAXBUFFSIZE/2, AINMAXBUFFSIZE/2, 0, 32, true, AINMAXBUFFSIZE/2, 0, 0, AINPBCLK, MININTERLEAVEmHZ, AINDMASIZE, AINMAXBUFFSIZE, AINOVERSIZE},
                        0, LOCKAvailable, rgOSC1Buff}),  
                ioscCh2({{Idle, Idle, Idle}, OSC2_ID, 0, 4, 0, 
                        {MAXmSAMPLEFREQ, 0, AINMAXBUFFSIZE, 0, AINMAXBUFFSIZE/2, AINMAXBUFFSIZE/2, AINMAXBUFFSIZE/2, 0, 32, true, AINMAXBUFFSIZE/2, 0, 0, AINPBCLK, MININTERLEAVEmHZ, AINDMASIZE, AINMAXBUFFSIZE, AINOVERSIZE},
                        0, LOCKAvailable, rgOSC2Buff}),
                ila({    {Idle, Idle, Idle}, 0, 0, 
                        {LAMAXmSPS, 0, LAMAXBUFFSIZE, 0, LAMAXBUFFSIZE/2, LAMAXBUFFSIZE/2, LAMAXBUFFSIZE/2, 0, 10, false, LAMAXBUFFSIZE/2, 0, 0, LAPBCLK, 2ll*LAMAXmSPS, LADMASIZE, LAMAXBUFFSIZE, LAOVERSIZE},
                        0, LOCKAvailable, rgLOGICBuff}),  
                igpio(   {Idle, 0, gpioTriState, 3,
                       {{gpioTriState, (PORTCH *) &ANSELE, 0x0001},
                        {gpioTriState, (PORTCH *) &ANSELE, 0x0002},
                        {gpioTriState, (PORTCH *) &ANSELE, 0x0004},
                        {gpioTriState, (PORTCH *) &ANSELE, 0x0008},
                        {gpioTriState, (PORTCH *) &ANSELE, 0x0010},
                        {gpioTriState, (PORTCH *) &ANSELE, 0x0020},
                        {gpioTriState, (PORTCH *) &ANSELE, 0x0040},
                        {gpioTriState, (PORTCH *) &ANSELE, 0x0080},
                        {gpioTriState, (PORTCH *) &ANSELE, 0x0100},
                        {gpioTriState, (PORTCH *) &ANSELE, 0x0200}}}),
                iCal({   {Idle, Idle, Idle}, CFGNONE}),
                iBoot(   {Idle, 0}),
                iWiFi({  {Idle, Idle, Idle}, nicWiFi0, VOLFLASH, false, false, false, WiFiConnectInfo(), WiFiConnectInfo(), WiFiScanInfo(),{0},{0}}),
                iMfgTest({0})
    {
        memset(iWiFi.szPassphrase, 0, sizeof(iWiFi.szPassphrase));
        memset(iWiFi.szSSID, 0, sizeof(iWiFi.szSSID));
    }

#endif
} PJCMD;

#if(AINDMASIZE - AINOVERSIZE != AINMAXBUFFSIZE)
    #error ADC DMA BUFFERS ARE WRONG
#endif

#if(LADMASIZE - LAOVERSIZE != LAMAXBUFFSIZE)
    #error ADC DMA BUFFERS ARE WRONG
#endif

#endif // ProcessJSONCmd_h

