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

#define MAX_PATH 260l   

#ifdef __cplusplus
extern DFILE dLFile1;             // Create a File handle to use with Logging
extern DFILE dLFile2;             // Create a File handle to use with Logging
#endif

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

typedef enum
{
    OVFNone,
    OVFStop,
    OVFCircular
} OVF;

typedef struct _BIDX
{
    // these are input parameters.
    uint64_t        xsps;           // xsps in  xHz; x defined by the units of the instrument, either mHz or uHz
    int64_t         psDelay;        // what is the delay in pico sec
    int32_t         cBuff;          // the requested size of the buffer

    uint16_t        tmrPreScalar;   // The prescalar to use on the sample rate timer
    uint32_t        tmrPeriod;      // what is the timer value to use to achieve the sample rate (must use the PeriodToPRx() to get PRx value for PWM)
    uint32_t        tmrCnt;         // If we roll the full counter, how many time must we do that to get the sample rate we want, for very slow sample rates <6sps.
    bool            fInterleave;    // if we are to interleave the ADCs, or just use one

    union
    {
        // for OSC / LA
        // this get calculated by CalculateBufferIndexes()
        struct
        {
            int64_t             dlTrig2POI;     // the delay in samples at the msps freq                                                                
            int32_t             iPOI;           // where in the return buffer is the POI;                                   
            int32_t             iTrg;           // where in the return buffer is the TRG; same as iTrigDMA except when -1   
            int32_t             iTrigDMA;       // Where the Trigger is;                                                    
            volatile int32_t    iDMATrig;       // ISR: The DMA pointer when entering the trigger ISR; iTrigDMA <= iDMATrig by time it takes to enter the ISR  
            int32_t             cBeforeTrig;    // How many samples before the trigger can be armed
            int64_t             cDelayTmr;      // total number of ticks of the delay timer
        };

        // used for Logging
        struct
        {
            int64_t             cTotalSamples;  // total number of valid samples                             
            int32_t             iDMAEnd;        // end of valid data in DMA buffer
            int32_t             iDMAStart;      // start of unsaved samples in DMA buffer
            volatile int32_t    cDMARoll;       // ISR: this is the roll count
            int32_t             cSavedRoll;     // roll count of what was saved to the file
            int32_t             cBackLog;       // How many samples waiting to be written to the SD card
            int64_t             spare;          // spare; available for use
        };
    };


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

    // some constant data, this should be in with the instrument, but that will cause an calibration change
    OSC *               const       posc;
    uint8_t *           const       pTrgSrcADC1;
    uint8_t *           const       pTrgSrcADC2;

    // timer to use for sampling
    uint32_t volatile * const       pADCDATA1;
    uint32_t volatile * const       pADCDATA2;

    uint8_t             const       adcData1Vector;
    uint8_t             const       adcData2Vector;
    uint8_t             const       trgSrcADC1;
    uint8_t             const       trgSrcADC2;
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

typedef struct _IALOG
{
    PSTATE          state;          // all of the parsing states
    INSTR_ID const  id;             // the instrument ID
    VOLTYPE         vol;            // where to store the data
    int64_t         maxSamples;     // how many samples to take
    int64_t         iStart;         // where to start reading from
    OVF             overflow;       // what to do on an overflow of the buffer, file, or maxSamples
    int32_t         mvOffset;       // mVolt offset of the input
    int32_t         gain;           // gain, set to 1-4//
    BIDX            bidx;           // samples per second
    STCD            stcd;           // stop condition
    uint32_t        curCnt;         // current count of the slow timer, compare to completion count in bidx.cnt
    uint32_t        tStart;         // how often we must do an SD card save, for really slow sample rates
    void *          pdFile;         // pointer to the dfile to use for writting the LOG file
    STATE           buffLock;       // the locked state of the buffer
    uint32_t        iBinOffset;     // the offset of the binary in the file after the JSON
    int16_t * const pBuff;          // point to the data buffer
    char            szURI[MAX_PATH+1]; // The file name or URL to the place to store the data logs
} IALOG;

typedef struct _IDLOG
{
    PSTATE          state;          // all of the parsing states
    INSTR_ID const  id;             // the instrument ID
    uint64_t        sps;            // samples per second
    uint16_t        bitMask;        // bit Mask of channels in use
    STATE           buffLock;       // the locked state of the buffer
    uint32_t        iBinOffset;     // the offset of the binary in the file after the JSON
    uint32_t        cBuff;          // how many entries in the buffer
    uint16_t * const pBuff;          // point to the data buffer
} IDLOG;

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
    CFGNAME     config;         // where to save the data, sd0, flash 
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

    // logging
    IALOG   iALog1;
    IALOG   iALog2;
    IDLOG   iDLog1;

    // manufacturing test data
    MFGTEST iMfgTest;

#ifdef __cplusplus

    _PJCMD() :  trigger({{Idle, Idle, Idle}, false, NULL_ID, TRGTPNone, 0, 0, 0, 0, 0, 0, {{NULL_ID, NULL, NULL, false, 0, 0, 0}, {NULL_ID, NULL, NULL, false, 0, 0, 0}, {NULL_ID, NULL, NULL, false, 0, 0, 0}, {NULL_ID, NULL, NULL, false, 0, 0, 0}}, 0, 0, 0, 0}),
                idcCh1({ {Idle, Idle, Idle}, DCVOLT1_ID, 0}),                                           
                idcCh2({ {Idle, Idle, Idle}, DCVOLT2_ID, 0}),      
                iawg({   {Idle, Idle, Idle}, AWG1_ID, waveNone, 0, 0, 0, 0, 50, AWGBUFFSIZE, rgAWGBuff}),
                ioscCh1({   {Idle, Idle, Idle}, OSC1_ID, 0, 4, 0, 
                        {MAXmSAMPLEFREQ, 0, AINMAXBUFFSIZE, 0, 32, 1, true, {0, AINMAXBUFFSIZE/2, AINMAXBUFFSIZE/2, AINMAXBUFFSIZE/2, 0, AINMAXBUFFSIZE/2, 0}, AINPBCLK, MININTERLEAVEmHZ, AINDMASIZE, AINMAXBUFFSIZE, AINOVERSIZE},
                        0, LOCKAvailable, rgOSC1Buff, (OSC *) rgInstr[OSC1_ID], &((uint8_t *) &ADCTRG1)[0], &((uint8_t *) &ADCTRG1)[1], (uint32_t *) &ADCDATA0, (uint32_t *) &ADCDATA1, _ADC_DATA0_VECTOR, _ADC_DATA1_VECTOR, 0b00110, 0b01010}),  
                ioscCh2({{Idle, Idle, Idle}, OSC2_ID, 0, 4, 0, 
                        {MAXmSAMPLEFREQ, 0, AINMAXBUFFSIZE, 0, 32, 1, true, {0, AINMAXBUFFSIZE/2, AINMAXBUFFSIZE/2, AINMAXBUFFSIZE/2, 0, AINMAXBUFFSIZE/2, 0}, AINPBCLK, MININTERLEAVEmHZ, AINDMASIZE, AINMAXBUFFSIZE, AINOVERSIZE},
                        0, LOCKAvailable, rgOSC2Buff, (OSC *) rgInstr[OSC2_ID], &((uint8_t *) &ADCTRG1)[2], &((uint8_t *) &ADCTRG1)[3], (uint32_t *) &ADCDATA2, (uint32_t *) &ADCDATA3, _ADC_DATA2_VECTOR, _ADC_DATA3_VECTOR, 0b00111, 0b01000}),
                ila({    {Idle, Idle, Idle}, 0, 0, 
                        {LAMAXmSPS, 0, LAMAXBUFFSIZE, 0, 10, 1, false, {0, LAMAXBUFFSIZE/2, LAMAXBUFFSIZE/2, LAMAXBUFFSIZE/2, 0, LAMAXBUFFSIZE/2, 0}, LAPBCLK, 2ll*LAMAXmSPS, LADMASIZE, LAMAXBUFFSIZE, LAOVERSIZE},
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
                iALog1({ {Idle, Idle, Idle}, ALOG1_ID, VOLRAM, -1, 0, OVFCircular, 0, 4,
                            {100000000, 0, 0, 0, 2000, 1, false, {0, 0, 0, 0, 0, 0, 0}, LOGPBCLK, 2ll*LOGuSPS, LOGDMASIZE, LOGMAXBUFFSIZE, LOGOVERSIZE},
                            STCDNormal, 0, 0, &dLFile1, LOCKAvailable, 0, rgOSC1Buff, {0}}),
                iALog2({ {Idle, Idle, Idle}, ALOG2_ID, VOLRAM, -1, 0, OVFCircular, 0, 4,
                            {100000000, 0, 0, 0, 2000, 1, false, {0, 0, 0, 0, 0, 0, 0}, LOGPBCLK, 2ll*LOGuSPS, LOGDMASIZE, LOGMAXBUFFSIZE, LOGOVERSIZE},
                            STCDNormal, 0, 0, &dLFile2, LOCKAvailable, 0, rgOSC2Buff, {0}}),
                iDLog1({ {Idle, Idle, Idle}, DLOG1_ID, LOGuSPS, 0, LOCKAvailable, 0, 0, rgLOGICBuff}),
                iMfgTest({0})
    {
        memset(iWiFi.szPassphrase, 0, sizeof(iWiFi.szPassphrase));
        memset(iWiFi.szSSID, 0, sizeof(iWiFi.szSSID));
//        memset(iFile.szPath, 0, sizeof(iFile.szPath));
    }

#endif
} PJCMD;


#ifdef __cplusplus

typedef struct _IFILE
{
    PSTATE              state;              // all of the parsing states
    VOLTYPE             vol;                // where to save the data, sd0, flash 
    int32_t             iFilePosition;
    int32_t             iBinOffset;
    int32_t             cbLength;
    OSPAR::FNWRITEDATA  WriteFile;
    STATE               buffLock;           // the locked state of the buffer
    char                szPath[MAX_PATH+1];
    DFILE               dFile;              // must be at the end; we memcopy to here from iFileT
} IFILE;

typedef struct _UICMD
{
    // file info
    IFILE   iFile;

    _UICMD() :   iFile({  {Idle, Idle, Idle}, VOLNONE, 0, 0, 0, &OSPAR::WriteOSJBFile, LOCKAvailable, {0}, DFILE()})
    {
        memset(iFile.szPath, 0, sizeof(iFile.szPath));
    }
    
} UICMD;

#endif

static void inline __attribute__((always_inline)) StopInstrumentTimer(INSTR_ID idInstr)
{
    // who is the target
    switch(idInstr)
    {
        case OSC1_ID:
            T3CONCLR = _T3CON_ON_MASK;                  // Stop taking DMA samples on the ADC0/1 channel
            break;

        case OSC2_ID:
            T5CONCLR = _T5CON_ON_MASK;                  // Stop taking DMA samples on the ADC2/3 channel
            break;

        case LOGIC1_ID:
            T7CONCLR = _T7CON_ON_MASK;                  // Stop taking DMA samples on the LA channel
            break;

        case ALOG1_ID:
            T5CONCLR = _T5CON_ON_MASK;                  // Stop taking DMA samples on the ADC0 channel
//            IEC0CLR  = _IEC0_T5IE_MASK;                 // Stop slow ISR if any
//            IEC4CLR  = _IEC4_DMA4IE_MASK;               // Stop rollover counter
            break;

        case ALOG2_ID:
            T8CONCLR = _T8CON_ON_MASK;                  // Stop taking DMA samples on the ADC2 channel
//            IEC1CLR  = _IEC1_T8IE_MASK;                 // Stop slow ISR if any
//            IEC4CLR  = _IEC4_DMA6IE_MASK;               // Stop rollover counter
            break;

            // sometimes we invalidate ID on the fly
            // if we get a force stop
        case NULL_ID:
            // do nothing
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }
}

#if(AINDMASIZE - AINOVERSIZE != AINMAXBUFFSIZE)
    #error ADC DMA BUFFERS ARE WRONG
#endif

#if(LADMASIZE - LAOVERSIZE != LAMAXBUFFSIZE)
    #error ADC DMA BUFFERS ARE WRONG
#endif

#endif // ProcessJSONCmd_h

