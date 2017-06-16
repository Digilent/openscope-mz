/************************************************************************/
/*                                                                      */
/*    OpenScope.h                                                       */
/*                                                                      */
/*    Header file for OpenScope                                         */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    2/17/2016 (KeithV): Created                                       */
/************************************************************************/
#ifndef OpenScope_h
#define OpenScope_h

#include "p32xxxx.h"
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

// Feedback Channels
#define	CH_ADC1_OFFSET_FB   24	
#define	CH_ADC2_OFFSET_FB   29	
#define	CH_DC1_FB           6	
#define	CH_DC2_FB           7	
#define	CH_AWG_FB           8	
#define	CH_USB5V0_FB        11	
#define	CH_VCC3V3_FB        23	
#define	CH_VREF3V0_FB       22
#define	CH_VREF1V5_FB       20	
#define	CH_VSS5V0_FB        19	

#include <OSSerial.h>
#include <./libraries/DEIPcK/DEIPcK.h>
#include <./libraries/DFATFS/DFATFS.h>         
#include <./libraries/HTTPServer/HTTPServer.h>
#include <./libraries/DSDVOL/DSDVOL.h>
#include <./libraries/FLASHVOL/FLASHVOL.h> 

//#define SERIALBAUDRATE 9600
//#define SERIALBAUDRATE 115200
//#define SERIALBAUDRATE 625000
//#define SERIALBAUDRATE 500000
//#define SERIALBAUDRATE 1000000
#define SERIALBAUDRATE 1250000

// what version of the calibration info we have
#define CALVER  17   
#define WFVER   8

/************************************************************************/ 
/************************************************************************/
/********************** Defines *****************************************/
/************************************************************************/
/************************************************************************/
// This is a Macro so if you stop in the debugger, it will stop at
// the location in code of the assert
//static inline void ASSERT(bool f) 
#define ASSERT(f)       \
{                       \
    if(!(f))            \
    {                   \
        LATJSET = 0x17; \
        while(1);       \
    }                   \
}                       

#define NEVER_SHOULD_GET_HERE false

// string macros
#define MKSTR2(a) #a
#define MKSTR(a) MKSTR2(a)

// min, max
#ifndef max
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
#endif

#define cBeforeTrig(_iTrig, _iPOI, _cRec, _cOver) (((_iPOI == -1) ? _cRec : ((_iTrig < _iPOI) ? _iTrig : _iPOI) + _cOver))

// PB 3, ADC, OC, Timers 
#define PBUS3DIV    2                                   // divide system clock by (1-128)
#define PB3FREQ     (F_CPU / PBUS3DIV)

#define KVA_2_PA(v)             (((uint32_t) (v)) & 0x1fffffff)
#define KVA_2_KSEG0(v)          (KVA_2_PA(v) | 0x80000000)
#define KVA_2_KSEG1(v)          (((uint32_t) (v)) | 0xA0000000)

#define PYS_2_CACHE(_ptr, _size) memcpy((void *) KVA_2_KSEG0(_ptr), (void *) KVA_2_KSEG1(_ptr), _size);  
#define CACHE_2_PHY(_ptr, _size) memcpy((void *) KVA_2_KSEG1(_ptr), (void *) KVA_2_KSEG0(_ptr), _size);  

// the PB bus runs at 100MHz, the output driver can only run at 50MHz
// so we need at least 2 steps for the output driver to respond.
// but it is a sloppy signal not making full VCC at 50MHz
// we really want something much more, so set the min at...
#define PWMMINPULSE     10
#define PWMHIGHLIMIT    (PWMPERIOD - PWMMINPULSE)   
#define PWMLOWLIMIT     PWMMINPULSE   

#define PWMPRESCALER    0       // this is 2^^T3PRESCALER (0 - 7, however 7 == 256 not 128) off of PBCLK3 (100 MHz)
#define PWMPERIOD       330     // currently we allow 330 resolution on the PWM, so 3.3v / 330 = 10mv resolution; at 100 Mhz / 330 = 303 KHz freq
#define PWMIDEALCENTER  175      // this is ideal, not actual; this should only be used as a starting point for calibration

#define PWM_SETTLING_TIME   500         // code will break if ALL PWM settling times don't use this value

/************************************************************************/
/********************** GPIO ********************************************/
/************************************************************************/
#define NBRGPIO 10
#define SDWAITTIME 500      // how long to wait after sensing an sd insertion and mounting
 
/************************************************************************/
/********************** Timers ******************************************/
/************************************************************************/
#define TMRPBCLK                100000000               // PB CLK, ticks per second
#define PSPERTMRPBCLKTICK       10000                   // there are 10^^12 ps/sec and 10^^8 ticks/sec or 10^^12 / 10^^8 == 10^^4 ps/tick
#define ten2ten                 10000000000ull          // 10^^10
#define ten2twelve              1000000000000ull        // 10^^12

/************************************************************************/
/********************** Core Timer **************************************/
/************************************************************************/
#define CORE_TMR_TICKS_PER_SEC (F_CPU / 2ul)
#define CORE_TMR_TICKS_PER_MSEC (CORE_TMR_TICKS_PER_SEC / 1000ul)
#define CORE_TMR_TICKS_PER_USEC (CORE_TMR_TICKS_PER_SEC / 1000000ul)

static inline uint32_t ReadCoreTimer(void)
{
    uint32_t coreTimerCount;
    __asm__ __volatile__("mfc0 %0,$9" : "=r" (coreTimerCount));
    return(coreTimerCount);
}


/************************************************************************/
/********************** Interrupts  **************************************/
/************************************************************************/
static inline uint32_t __attribute__((nomips16)) OSEnableInterrupts(void)
{
    uint32_t status = 0;
    asm volatile("ei    %0" : "=r"(status));
    return status;
}

static inline uint32_t __attribute__((nomips16)) OSDisableInterrupts(void)
{
    uint32_t status = 0;
    asm volatile("di    %0" : "=r"(status));
    return status;
}

static inline void __attribute__((nomips16))  OSRestoreInterrupts(uint32_t st)
{
    if (st & 0x00000001) asm volatile("ei");
    else asm volatile("di");
}

/************************************************************************/
/********************** Common Timer Values *****************************/
/************************************************************************/
#define MINTMRVALUE             32                      // for max sample rate on the ADC, what is the min timer prescaler (100,000,000 / 32) = 3,125,000 (max ADC rate).
#define MAXTMRVALUE             65530                   // Max we will set the timer until we go to a single ADC
#define MININTERLEAVEmHZ        2000000                 // below 2000 SPS we go to single ADCs
#define PeriodToPRx(_Period)    ((_Period)-1)           // converting a Tmr value to a PRx match register value               

/************************************************************************/
/********************** Analog Input ************************************/
/************************************************************************/
#define AINBUFFSIZE             0x8000          // # of elements in the buffer array (each element is 2 bytes)  
#define AINPBCLK                100000000               // PBCLK speed
#define AINDMASIZE              (0x8000-2)              // # of elements in the buffer array (each element is 2 bytes) -- 64K BUG: DMA contoller freaks out if you transfer 64K, back off by a few bytes.
#define AINOVERSIZE             128                     // How much we oversize the sample buffer to ensure we can wrap and stop and get data, we need at least 2 samples slop at the begininning and AINOVERSHOOT and interupt time at the end. 
#define AINOVERSHOOT            5                       // how many samples to over shoot in our timing, just to make sure we get valid data at the end, this must fit in the 128 sample slop
#define AINMAXBUFFSIZE          (AINDMASIZE- AINOVERSIZE)     // # of elements in the buffer array (each element is 2 bytes) -- 64K 
#define NbrOfADCGains 4                                 // Number of gain selections
#define MAXmSAMPLEFREQ          6250000000ll            // max sample frequency in mHz - must be mult of 2 -- (100,000,000 / 32) * 2 = 6,250,000
#define MINmSAMPLEFREQ          5961ll                  // min sample frequency in mHz 
#define ADCpsDELAY              320000ll                // Full conversion time of the Dedicated ADCs in pico seconds.

// WARNING: Interleaving only works if the timer prescalar is set to 1:1 with the PB clock.
// The timer event will occur 1 PB and 2 sysclocks after the PR match. With a PBCLK3 at 2:1, and a timer prescalar of 1:1, that
// works that the interrupt event occurs on timer value 1 (2 ticks after PR)
// WARNING: the ADC is triggered off of the rising edge of the OC, not the interrupt event, this means the ADC will trigger on a match to R
// WARNING: however, the DMA will trigger off of the OC interrupt event, which is a match to RS
// As documented the interrupt for the OC should occur one PB after the R match for the ADC and 1 PB after the RS match for the DMA
// however there must be extra logic for the ADC as the ADC fires 3 PB after the R match, determined experimentally.
#define INTERLEAVEPR(_Period)         ((_Period)-1)                 
#define INTERLEAVEOC(_Period)         (((_Period)/2)-2)  

#define mVDCChg     40                                  // The input can change by about 40mv
#define DadcDCChg   (((40l * 1365l) + 500) / 1000l)     // The input can change by about 40mv
#define DadcLimit   (2l * DadcDCChg)                    // stop looking when we get this close  


/************************************************************************/
/********************** AWG Parameters **********************************/
/************************************************************************/
#define AWGBUFFSIZE     0x8000          // # of elements in the buffer array (each element is 2 bytes)  
#define AWGPRESCALER    0       // this is 2^^T3PRESCALER (0 - 7, however 7 == 256 not 128) off of PBCLK3 (100 MHz)
#define AWGPBCLK        (AWGPRESCALER == 7 ? (F_CPU) >> (AWGPRESCALER+2) : (F_CPU) >> (AWGPRESCALER+1))
#define AWGMAXDMA       10000000l                   // How fast the DMA can run without missing events.
#define AWGMAXSPS       (AWGPBCLK > AWGMAXDMA ? AWGMAXDMA : AWGPBCLK)
#define AWGMINTMRCNT    (AWGPBCLK / AWGMAXSPS) 
#define AWGMAXFREQ      1000000l
#define AWGMAXP2P       3000l

// in the custom waveform generator we have
// uint32_t tmr        = (int32_t) (((pbx1000 / freqHz / AWGMAXBUF) + 500) / 1000); 
// where pbx1000 is PB*100 = 100,000,000,000
// pbx1000/AWGMAXBUF should be an even number say 100,000,000,000 / 25,000 = 4,000,000
// our max freq is 1,000,000 which would yield a tmr of 4.
#define AWGMAXBUF       25000l       // make this a mult of 2s and 5s as 100,000,000 == 5^^8 * 2^^8, we want to divide evenly

#define AWG_SETTLING_TIME   2           // 2ms is min
#define HWDACSIZE   1024l


#define DACDATA(a) ((uint16_t) (~((((a) << 4) & 0x3000) | (((a) << 3) & 0x0700) | (((a) << 2) & 0x0070) | ((a) & 0x0003))))

#define DACP2P          3000l
#define DACMVLIMIT      1500l
#define DACOFFSET       1500l

// #define DACIDEAL(index) (((DACP2P * ((int32_t) index)) + ((SWDACSIZE-1)/2)) / (SWDACSIZE-1) - 1500l)
// #define SCALEDDACIDEAL(index) (DACIDEAL(index) * 69030l)

/************************************************************************/
/********************** LA Parameters **********************************/
/************************************************************************/
#define LABUFFSIZE      0x8000                      // # of elements in the buffer array (each element is 2 bytes)  
#define LAOVERSIZE      128                         // How much we oversize the sample buffer to ensure we can wrap and stop and get data, we need at least 2 samples slop at the begininning and AINOVERSHOOT and interupt time at the end. 
#define LADMASIZE       (LABUFFSIZE - 2)            // How much the DMA will transfer before a roll, must be less than 64K, must have room for missed points
#define LAMAXBUFFSIZE   (LADMASIZE - LAOVERSIZE)    // How much the DMA will transfer before a roll, must be less than 64K, must have room for missed points
#define LAMAXmSPS       (10000000000ll)             // How fast can the LA run in mSPS
#define LAMINmSPS       (5961ll)                    // How slow can the LA run, 100,000,000 / 256 / 65536 => PB / peScalar / PRx in mSPS
#define LAPBCLK         100000000l                  // how fast is the LA PBclk
#define LAOVERSHOOT     5                           // how many samples to over shoot in our timing, just to make sure we get valid data at the end, this must fit in the 128 sample slop

// supported waveforms
typedef enum
{
    waveNone,
    waveDC,
    waveSine,
    waveSquare,
    waveTriangle,
    waveSawtooth,
    waveArbitrary,
} WAVEFORM;

/************************************************************************/
/************************************************************************/
/********************** Instrument handle indexs ************************/
/********************** Must map to indexes in rgInstr ******************/
/************************************************************************/
/************************************************************************/

typedef enum
{
    NULL_ID     = 0,
    DCVOLT1_ID,
    DCVOLT2_ID,
    AWG1_ID,
    OSC1_ID,
    OSC1_DC_ID,
    OSC2_ID,
    OSC2_DC_ID,
    LOGIC1_ID,
    INSTR_END_ID,
    PWM1_ID,
    PWM2_ID,
    I2C1_ID,
    UART1_ID,
    SPI1_ID,
    WIFIPARAM_ID,
    EXT_TRG_ID,
    FORCE_TRG_ID,
    END_ID
} INSTR_ID;

// instrument attributes
typedef enum
{
    instrIDNone = 0,
    instrIDCal,
    instrIDCalSrc,
} INSTR_ATT;

typedef enum
{
    nicNone = 0,
    nicWorking,
    nicWiFi0
} NIC_ADP;

#define CBMAXINSTNAME   64

/************************************************************************/
/********************** Other Defines  **********************************/
/************************************************************************/

typedef const void * HINSTR;
#define HINSTRFromInstr(a) ((HINSTR) &(a))

// Macros to identify warning and error states
#define IsStateAnError(s)   (STATEError <= s)
#define IsStateAWarning(s)  (STATEWarning <= s && s < STATEError)

// #define VOLWFPARM VOLFLASH
#define VOLWFPARM VOLSD

// how many static acqusition buffers we have.
#define nbrAcqBuffers 4

/************************************************************************/
/************************************************************************/
/********************** External Variables  *****************************/
/************************************************************************/
/************************************************************************/
    
    extern const char           szEnumVersion[] ;
    extern const char           szProgVersion[];
    extern char const * const   rgszSecurityMode[];
    extern char const * const   rgszAdapter[];
    extern const HINSTR         rgInstr[];              // instrument handles by inst ID
    extern char const * const   rgszInstr[];            // instrument name by inst ID ie. OSC1, AWG1 ....
    extern char const * const   rgCFGNames[];           // FAT FILE qualifyer by CFG Name .. ie. USER, MFG, TEMP
    extern MACADDR              macOpenScope;           // my MAC address
    extern IPv4                 ipOpenScope;            // my IP address
    extern const char           szDefaultPage[];        // default home page string
    
        // the Power supply and reference voltages.   
    extern uint32_t uVUSB;
    extern uint32_t uV3V3;
    extern uint32_t uVRef3V0;
    extern uint32_t uVRef1V5;
    extern uint32_t uVNUSB;
    
    // flags to tell the UI engine to 
    // do some things up front.
    extern bool fModeJSON;
    extern bool fBlockIOBus;
    extern uint32_t aveLoopTime;

    // static buffers used by the instruments
    extern uint32_t                                 trigAcqCount;
    extern int16_t      __attribute__((coherent))   rgOSC1Buff[AINBUFFSIZE];
    extern int16_t      __attribute__((coherent))   rgOSC2Buff[AINBUFFSIZE];
    extern uint16_t     __attribute__((coherent))   rgAWGBuff[AWGBUFFSIZE];
    extern uint16_t     __attribute__((coherent))   rgLOGICBuff[LABUFFSIZE];


/************************************************************************/
/********************** Data used in constructors  **********************************/
/************************************************************************/
extern uint32_t const dcInitData[2][2];
extern uint32_t const oscInitData[2][12];

/************************************************************************/
/************************************************************************/
/********************** FAT FILE VOL and NAMES **************************/
/************************************************************************/
/************************************************************************/
// volumes to use.
// except for VOLNONE, these index into
// DFATFS::szFatFsVols[vol] for vol strings
typedef enum
{
    VOLNONE     = 0,
    VOLSD       = 1,
    VOLFLASH    = 2,
} VOLTYPE;

typedef enum
{
    CFGNONE     = 0, 
    CFGUNCAL    = 1,
    CFGFLASH    = 2,
    CFGSD       = 3,
    CFGCAL      = 4,
    CFGEND      = 5
} CFGNAME;

#define FACTORY_CALIBRATION VOLFLASH, CFGFLASH
#define USER_CALIBRATION VOLSD, CFGSD

/************************************************************************/
/************************************************************************/
/********************** state Machine states  ***************************/
/************************************************************************/
/************************************************************************/
typedef uint32_t STATE;

typedef enum
{
    // common states
    // these are printable states in the rgInstrumentStates array
    Idle = 0,
    Armed,
    Acquiring,
    Triggered,
    Stopped,
    Running,
    Busy,

    // more common states
    Working,
    Waiting,
    Done,
    Queued,
    Calibrating,
    Reading,
    Writing,

    WaitingRun,
    Run,
    ReRun,

    // Buffer lock states
    LOCKAvailable,
    LOCKAcq,
    LOCKOutput,

    // main loop states
    MSysInit,
    MSysVoltages,
    MHeaders,
    MWaitSDDetTime,
    MLookUpWiFi,
    MConnectWiFi,         
    MReadCalibrationInfo,
    MLoop,

    // initialization states
    INITInstrInit,
    INITWebServer,
    INITInitFileSystem,
    INITAdaptor,
    INITWaitForMacAddress,
    INITMRFVer,

    // JSON UI
    UIJSONMain,
    UIJSONWaitOSLEX,
    UIJSONWaitInput,
    UIJSONProcessJSON,
    UIJSONWriteChunkSize,
    UIJSONWriteJSON,
    UIJSONWriteBinary,
    UIJSONWriteBinaryEntry,
    UIJSONDone,
    
    // UI states
    UIMainMenu,
    UIWaitForInput,
    UIWriteOutput,
    UIWriteDone,
    UIStartCalibrateInstruments,
    UIWaitForCalibrationButton,
    UIAWGStop,
    UICalibrateInstruments,
    UIPrtCalibrationInfo,
    UISaveCalibration,
    UIWaitForSaveCalibration,

    UIPrtszInput,
    UIDeleteszInputFile,

    UIDeleteWiFiConnection,
    UICheckWiFiConnectionToDelete,

    UISetOSCGain,
    UIOSCGainInput,
    UIOSCSetGainOffset,
            
    UIManageWiFi,
    UIWaitForManageWiFiInput,
    UIWalkFiles,
    UIProcessFileEntry,

    UIWiFiScan,
    UIWalkScan,
    UIAskToShutdownWiFi,
    UIWaitForShutdownReply,
    UIWaitForDisconnect,
    UIPrtWiFiInfo,
    UIListSSID,
    UIRequestNetwork,
    UIReadLine,
    UISelectSSID,
    UIGetPassPrase,
    UIReadPassPhrase,
    UICalculatePSKey,
    UIReadAutoConnect,
    UICheckFileName,
    UIWaitForNetwork,
    UICheckFileAgainstScan,
    UICheckFileAgainstSelection,

    UIPrtWiFiConnectionFile,
    UICheckNetwork,
    UIWaitForNetworkSelection,
    UIGetNetwork,
    UIConnectToNetwork,
    UIAddWiFiConnection,
    UISaveWiFiConnection,
    UIWiFiConnect,
    UIWiFiDisconnect,
    UIDisconnectAfterConnect,
    UIAskToDisconnect,
    UIAskToAutoSave,
            
    // config states
    IOWrite,
    IORead,
    IOReadToEndOfLine,
            
    // config states
    CFGCal,
    CFGCalNext,
    CFGCalTime,
    CFGCalSave,
    CFGCalSaveNext,
    CFGCalCheckInstr,
    CFGCalCkValid,
    CFGCalRead,
    CFGCalReadNext,
    CFGCalReadUser,
    CFGCalReadFactory,
    CFGCalReadIdeal,

    CFGCalPrint,
    CFGCalPrintNext,
    CFGOSCPrtCalInit,
    CFGOSCPrtCal,
    CFGAWGPrtCalStart,
    CFGAWGPrtCalIdeal,
    CFGAWGPrtCal,          
    CFGCheckingFileSystems,
    CFGCheckHomeHTMLPage,

    CFGUSBInserted,
 
    // system voltage states
    VOLTUSB,
    VOLT3V3,
    VOLT3V0,
    VOLT1V5,
    VOLTNUSB,
            
    // HTTP States
    HTTPEnabled,

    // WiFi states
    WiFiScanning,
    WiFiCreateFileName,
    WiFiCheckFileName,
    WiFiCheckNextFileName,
    WiFiWaitingConnect,
    WiFiWaitingConnectNone,
    WiFiWaitingConnectWPAWithKey,
    WiFiEnable,
    WiFiWaitingDisconnect,
    WiFiReadFile,
    WiFiWriteFile,
    
    // DC states
    DCReadHigh,
    DCWaitHigh,
    DCReadLow,
    DCWaitLow,
    DCSetLow,
    DCSetWait,
    
    // AWG states        
    AWGWaitPWMLow,
    AWGReadPWMLow,
    AWGWaitPWMHigh,
    AWGReadPWMHigh,
    AWGWaitDACLow,
    AWGReadDACLow,
    AWGWaitDACHigh,
    AWGReadDACHigh,
    AWGWait,
    AWGWaitHW,
    AWGReadHW,
    AWGSort,
    AWGEncode,   
    AWGMakeDMABuffer,
    AWGWaitOffset,
    AWGBeginRun,
    AWGWaitCustomWaveform,
    AWGDC,
    AWGSine,
    AWGSquare,
    AWGTriangle,
    AWGSawtooth,
    AWGBode,
     
    // OSC states        
    OSCPlusDC,
    OSCPlusReadDC,
    OSCPlusReadOSC,
    OSCMinusDC,
    OSCMinusReadDC,
    OSCMinusReadOSC,
    OSCZeroDC,
    OSCWaitPWMHigh,
    OSCReadPWMHigh,
    OSCWaitPWMLow,
    OSCReadPWMLow,
    OSCZeroPWM,
    OSCReadZeroDC,
    OSCReadZeroOffset,
    OSCNextGain,
    OSCWaitOffset,
    OSCSetDMA,
    OSCBeginRun,

    // JSON Parsing states
    JSONSkipWhite,
    JSONToken,
    JSONNextToken,
    JSONfalse,
    JSONnull,
    JSONtrue,
    JSONString,
    JSONNumber,
    JSONCallOSLex,
    JSONSyntaxError,
    JSONTokenLexingError,
    JSONNestingError,

    // OpenScope Parsing
    OSPARSkipObject,
    OSPARMemberName,
    OSPARSkipNameSep,
    OSPARSkipArray,
    OSPARSkipValueSep,
    OSPARSeparatedObject,
    OSPARSeparatedNameValue,
    OSPARErrObjectEnd,
    OSPARErrArrayEnd,
    OSPARTopObjEnd,

    // Endpoint
    OSPARLoadEndPoint,

    // Mode
    OSPARMode,
    OSPARDebugPrint,

    // triggering
    OSPARTrgChannelObject,
    OSPARTrgCh1,
    OSPARTrgCmd,
    OSPARTrgSetParm,

    OSPARTrgSource,
    OSPARTrgInstrument,
    OSPARTrgInstrumentChannel,
    OSPARTrgType,
    OSPARTrgLowerThreashold,
    OSPARTrgUpperThreashold,
    OSPARTrgRisingEdge,
    OSPARTrgFallingEdge,
    OSPARTrgSourceObjectEnd,

    OSPARTrgTargets,

    OSPARTrgTargetOsc,
    OSPARTrgTargetOscCh,

    OSPARTrgTargetsObjectEnd,
    OSPARTrgChSetParmObjectEnd,
    OSPARTrgTargetLa,
    OSPARTrgTargetLaCh,
    OSPARTrgTargetEndChArray,

    OSPARTrgSingle,
    OSPARTrgChSingleObjectEnd,
    OSPARTrgRun,
    OSPARTrgChRunObjectEnd,
    OSPARTrgStop,
    OSPARTrgChStopObjectEnd,
    OSPARTrgForceTrigger,
    OSPARTrgChForceTriggerEnd,
    OSPARTrgGetCurrentState,
    OSPARTrgChGetCurrentStateObjectEnd,

    OSPARTrgChArrayEnd,
 
    // device
    OSPARDeviceArray,
    OSPARDeviceEndArray,
    OSPARDeviceCmd,
    OSPARDeviceEndObject,
    OSPARDeviceEnmerate,
    OSPARDeviceEnterBootloader,
    OSPARDeviceAveLoopTime,
    OSPARDeviceStorageGetLocations,
    OSPARDeviceStorageLocation,
    OSPARDeviceResetInstruments,

    OSPARDeviceCalibrationGetTypes,
    OSPARDeviceCalibrationGetInstructions,
    OSPARDeviceCalibrationStart,
    OSPARDeviceCalibrationRead,
    OSPARDeviceCalibrationSave,
    OSPARDeviceCalibrationLoad,
    OSPARDeviceCalibrationType,
    OSPARDeviceEndCalibration,

    // WiFi
    OSPARDeviceNicList,
    OSPARDeviceNicGetStatus,
    OSPARDeviceNicDisconnect,
    OSPARDeviceNicConnect,
    OSPARDeviceAdapter,
    OSPARDeviceWiFiForce,
    OSPARDeviceWiFiParameterSet,
    OSPARDeviceWiFiSaveParameters,
    OSPARDeviceWiFiLoadParameters,
    OSPARDeviceWiFiDeleteParameters,
    OSPARDeviceWiFiListSavedParameters,

    OSPARDeviceWiFiScan,
    OSPARDeviceWiFiListScan,

    OSPARDeviceWiFiSetParameters,
    OSPARDeviceWiFiSSID,
    OSPARDeviceWiFiSecurityType,
    OSPARDeviceWiFiPassphrase,
    OSPARDeviceWiFiAutoConnect,

    OSPARDeviceEndWiFiParam,
    OSPARDeviceEndWiFi,


    // DC power Supply
    OSPARDCChannelObject,
    OSPARDcCh1,
    OSPARDcCh2,
    OSPARDcCmd,
    OSPARDcGetVoltage,
    OSPARDcSetVoltage,
    OSPARDcSetmVoltage,
    OSPARDcObjectEnd,
    OSPARDcChEnd,

    // GPIO state
    OSPARGpioChannelObject,
    OSPARGpioCh1,
    OSPARGpioCh2,
    OSPARGpioCh3,
    OSPARGpioCh4,
    OSPARGpioCh5,
    OSPARGpioCh6,
    OSPARGpioCh7,
    OSPARGpioCh8,
    OSPARGpioCh9,
    OSPARGpioCh10,
    OSPARGpioCmd,
    OSPARGpioSetParameters,
    OSPARGpioRead,
    OSPARGpioWrite,
    OSPARGpioDirection,
    OSPARGpioValue,
    OSPARGpioObjectEnd,
    OSPARGpioChEnd,

    // Waveform generator
    OSPARAwgChannelObject,
    OSPARAwgCh1,
    OSPARAwgCmd,
    OSPARAwgSetRegularWaveform,
    OSPARAwgGetCurrentState,
    OSPARAwgRun,
    OSPARAwgStop,
    OSPARAwgSignalType,
    OSPARAwgSignalFreq,
    OSPARAwgVP2P,
    OSPARAwgOffset,
    OSPARAwgDutyCycle,
    OSPARAwgObjectEnd,
    OSPARAwgChEnd,

    // Oscilloscope 
    OSPAROscChannelObject,
    OSPAROscCh1,
    OSPAROscCh2,
    OSPAROscCmd,
    OSPAROscSetParm,
    OSPAROscSetOffset,
    OSPAROscSetGain,
    OSPAROscSetSampleFreq,
    OSPAROscSetBufferSize,
    OSPAROscSetTrigDelay,
    OSPAROscRead,
    OSPAROscSetAcqCount,
    OSPAROscGetCurrentState,
    OSPAROscObjectEnd,
    OSPAROscChEnd,

    // Logic Analyzer
    OSPARLaChannelObject,
    OSPARLaCh1,
    OSPARLaCmd,
    OSPARLaChArrayEnd,
    OSPARLaSetParm,
    OSPARLaRead,
    OSPARLaGetCurrentState,
    OSPARLaRun,
    OSPARLaStop,
    OSPARLaSetSampleFreq,
    OSPARLaSetBufferSize,
    OSPARLaSetAcqCount,
    OSPARLaSetTrigDelay,
    OSPARLaBitMask,
    OSPARLaObjectEnd,

    // common JSPAR
    JSPARSetParm,
            
    // States used for processing OS JSON commands
    JSPARTrgTriggered,
    JSPARTrgArmed,
    JSPARTrgRead,
    JSPARTrgGetCurrentState,

    JSPARDcGetVoltage,
    JSPARDcSetVoltage,

    JSPARGpioRead,
    JSPARGpioWrite,

    JSPARAwgSetRegularWaveform,
    JSPARAwgGetCurrentState,
    JSPARAwgRun,
    JSPARAwgRunReady,
    JSPARAwgStop,
    JSPARAwgWaitingRegularWaveform,
    JSPARAwgRunRegularWaveform,
    JSPARAwgWaitingArbitraryWaveform,
    JSPARAwgRunArbitraryWaveform,

    JSPAROscRead,
    JSPAROscGetCurrentState,

    JSPARLaRead,
    JSPARLaGetCurrentState,
    JSPARLaRun,
    JSPARLaStop,

    JSPARCalibrationStart, 
    JSPARFailedCalibrating, 
    JSPARCalibrationLoading,
    JSPARCalibrationSave,
    JSPARCalibrationLoad,
    JSPARCalibratingRead,

    JSPARNicGetStatus, 
    JSPARNicDisconnect,
    JSPARNicConnect,
    JSPARWiFiScan,
    JSPARWiFiListScan,
    JSPARWiFiSetParameters,
    JSPARWiFiSaveParameters,
    JSPARWiFiLoadParameters,
    JSPARWiFiDeleteParameters,
    JSPARWiFiListSavedParameters,

    // termination
    OSPARSyntaxError = 0x08000000,      // must be a big number, but not an error
    OSPAREnd,


    // Undocumeneted Manufacturing test commands
    OSPARTestArray,
    OSPARTestCmd,
    OSPARTestNbr,
    OSPARTestRun,
    OSPARTestEndObject,
            
    /************************************************************************/
    /********************** Compound ERROR/WARNINGS have this bit set *******/
    /************************************************************************/
    STATECompound = 0x10000000,

    /************************************************************************/
    /********************** PREDEFINED ERROR/WARNINGS ***********************/
    /************************************************************************/
    STATEPredefined = 0x20000000,
    
    /************************************************************************/
    /********************** WARNINGS ****************************************/
    /************************************************************************/
    STATEWarning = 0x40000000,
    
    UINoNetworksFound,
    GPIODirectionMissMatch,
            
    /************************************************************************/
    /********************** ERRORS ******************************************/
    /************************************************************************/
    STATEError = 0x80000000,
 
    IOVolError,

    // configuration errors 
    CFGInvalidParm,
    CFGInvalidInstrument,
    CFGNoInstrumentsInGroup,
    CFGUnableToReadConfigFile,
    CFGNotImplemented,
    CFGNoFileSystem,
    CFGNoHomeHTMLPage,
    CFGInstrumentNotCalibrated,
    CFGCalibrating,

    // Initialization Errors        
    INITUnableToSetNetworkAdaptor,
    INITMACFailedToResolve,

    // WiFi Errors
    WiFiNoScanData,
    WiFiNoMatchingSSID,
    WiFiUnsuporttedSecurity,
    
    // AWG Errors        
    AWGBufferTooBig,
    AWGCurrentlyRunning,
    AWGExceedsMaxSamplPerSec,
    AWGValueOutOfRange,
    AWGWaveformNotSet,
    AWGWaveformNotSupported,

    // OSC Errors        
    OSCPWMOutOfRange,
    OSCGainOutOfRange,
    OSCDcNotHookedToOSC,

    //TRG errors
    TRGUnableToSetTrigger,
    TRGAcqCountOutTooLow,

    // GPIO Errors
    GPIOInvalidDirection,

    // compound errors
    // errors OR'ed with underlying error codes 
    // we can have 255 of these from 0x101xxxxx - 0x1FFxxxxx
    // OR'd in errors can be from 0x00000000 -  0x000FFFFF
    CFGMountError                   = (STATEError | STATECompound | 0x00100000),
    CFGFileSystemError              = (STATEError | STATECompound | 0x00200000),
    WiFiScanFailure                 = (STATEError | STATECompound | 0x00300000),
    WiFiConnectionError             = (STATEError | STATECompound | 0x00400000),

    // common predefined errors
    InvalidState                    = (STATEError | STATEPredefined | 0x00000001),  // 0xA0000001, --> 2684354561
    InvalidFileName                 = (STATEError | STATEPredefined | 0x00000002),  // 0xA0000002, --> 2684354562
    InvalidCommand                  = (STATEError | STATEPredefined | 0x00000003),  // 0xA0000003, --> 2684354563
    InvalidChannel                  = (STATEError | STATEPredefined | 0x00000004),  // 0xA0000004, --> 2684354564
    NotEnoughMemory                 = (STATEError | STATEPredefined | 0x00000005),  // 0xA0000005, --> 2684354565
    FileInUse                       = (STATEError | STATEPredefined | 0x00000006),  // 0xA0000006, --> 2684354566
    InvalidSyntax                   = (STATEError | STATEPredefined | 0x00000007),  // 0xA0000007, --> 2684354567
    UnsupportedSyntax               = (STATEError | STATEPredefined | 0x00000008),  // 0xA0000008, --> 2684354568
    Unimplemented                   = (STATEError | STATEPredefined | 0x00000009),  // 0xA0000009, --> 2684354569
    ValueOutOfRange                 = (STATEError | STATEPredefined | 0x0000000A),  // 0xA000000A, --> 2684354570
    InstrumentArmed                 = (STATEError | STATEPredefined | 0x0000000B),  // 0xA000000B, --> 2684354571
    AcqCountTooOld                  = (STATEError | STATEPredefined | 0x0000000C),  // 0xA000000C, --> 2684354572
    InstrumentInUse                 = (STATEError | STATEPredefined | 0x0000000D),  // 0xA000000D, --> 2684354573

    JSONLexingError                 = (STATEError | STATEPredefined | 0x0000000E),  // 0xA000000E, --> 2684354574
    JSONObjArrayNestingError        = (STATEError | STATEPredefined | 0x0000000F),  // 0xA000000F, --> 2684354575
    OSInvalidSyntax                 = (STATEError | STATEPredefined | 0x00000010),  // 0xA0000010, --> 2684354576
    EndOfStream                     = (STATEError | STATEPredefined | 0x00000011),  // 0xA0000011, --> 2684354577

    NotCfgForCalibration            = (STATEError | STATEPredefined | 0x00000012),  // 0xA0000012, --> 2684354578
    UnableToSaveData                = (STATEError | STATEPredefined | 0x00000013),  // 0xA0000013, --> 2684354579
    InvalidAdapter                  = (STATEError | STATEPredefined | 0x00000014),  // 0xA0000014, --> 2684354580
    NoSSIDConfigured                = (STATEError | STATEPredefined | 0x00000015),  // 0xA0000015, --> 2684354581
    MustBeDisconnected              = (STATEError | STATEPredefined | 0x00000016),  // 0xA0000016, --> 2684354582
    NoScanDataAvailable             = (STATEError | STATEPredefined | 0x00000017),  // 0xA0000017, --> 2684354583
    UnableToGenKey                  = (STATEError | STATEPredefined | 0x00000018),  // 0xA0000018, --> 2684354584
    WiFiIsRunning                   = (STATEError | STATEPredefined | 0x00000019),  // 0xA0000019, --> 2684354585
    WiFiUnableToStartHTTPServer     = (STATEError | STATEPredefined | 0x0000001A),  // 0xA000001A, --> 2684354586
    WiFiNoNetworksFound             = (STATEError | STATEPredefined | 0x0000001B),  // 0xA000001B, --> 2684354587
    FileUnableToReadFile            = (STATEError | STATEPredefined | 0x0000001C),  // 0xA000001C, --> 2684354588
    InstrumentNotArmed              = (STATEError | STATEPredefined | 0x0000001D),  // 0xA000001D, --> 2684354589
    InstrumentNotArmedYet           = (STATEError | STATEPredefined | 0x0000001E),  // 0xA000001E, --> 2684354590
    NoSDCard                        = (STATEError | STATEPredefined | 0x0000001F),  // 0xA000001F, --> 2684354591
} OPEN_SCOPE_STATES;

/************************************************************************/
/************************************************************************/
/********************** State Machine Functions  ************************/
/************************************************************************/
/************************************************************************/
typedef enum
{
    SMFnNone = 0,
            
    DCFnCal,
    DCFnSet,
            
    AWGFnCal,
    AWGFnSetOffset,
    AWGFnSetWaveform,
    AWGFnSetCustomWaveform,
    AWGFnRd,
    AWGFnRun,
    AWGFnStop,
            
    OSCFnCal,
    OSCFnSetOff,
    OSCFnRun,

    LAFnRun,

    WIFIFnManConnect,
    WIFIFnAutoConnect,
} OPEN_SCOPE_FUNC;

typedef struct _PORTCH
{
    uint32_t ANSEL;
    uint32_t ANSELCLR;
    uint32_t ANSELSET;
    uint32_t ANSELINV;
    uint32_t TRIS;
    uint32_t TRISCLR;
    uint32_t TRISSET;
    uint32_t TRISINV;
    uint32_t PORT;
    uint32_t PORTCLR;
    uint32_t PORTSET;
    uint32_t PORTINV;
    uint32_t LAT;
    uint32_t LATCLR;
    uint32_t LATSET;
    uint32_t LATINV;
    uint32_t ODC;
    uint32_t ODCCLR;
    uint32_t ODCSET;
    uint32_t ODCINV;
    uint32_t CNPU;
    uint32_t CNPUCLR;
    uint32_t CNPUSET;
    uint32_t CNPUINV;
    uint32_t CNPD;
    uint32_t CNPDCLR;
    uint32_t CNPDSET;
    uint32_t CNPDINV;
    uint32_t CNCON;
    uint32_t CNCONCLR;
    uint32_t CNCONSET;
    uint32_t CNCONINV;
    uint32_t CNEN;
    uint32_t CNENCLR;
    uint32_t CNENSET;
    uint32_t CNENINV;
    uint32_t CNSTAT;
    uint32_t CNSTATCLR;
    uint32_t CNSTATSET;
    uint32_t CNSTATINV;
    uint32_t CNNE;
    uint32_t CNNECLR;
    uint32_t CNNESET;
    uint32_t CNNEINV;
    uint32_t CNF;
    uint32_t CNFCLR;
    uint32_t CNFSET;
    uint32_t CNFINV;
    uint32_t SRCON0;
    uint32_t SRCON0CLR;
    uint32_t SRCON0SET;
    uint32_t SRCON0INV;
    uint32_t SRCON1;
    uint32_t SRCON1CLR;
    uint32_t SRCON1SET;
    uint32_t SRCON1INV;
} PORTCH;

typedef struct _TMRCH
{
    __T2CONbits_t   TxCON;          // careful, T1CON is type A timer, and we are type B timers
    uint32_t        TxCONClr;
    uint32_t        TxCONSet;
    uint32_t        TxCONInv;
    uint32_t        TMRx;
    uint32_t        TMRxClr;
    uint32_t        TMRxSet;
    uint32_t        TMRxInv;
    uint32_t        PRx;
    uint32_t        PRxClr;
    uint32_t        PRxSet;
    uint32_t        PRxInv;
} __attribute__((packed)) TMRCH;

typedef struct _OCCH
{
    __OC1CONbits_t  OCxCON;
    uint32_t        OCxCONClr;
    uint32_t        OCxCONSet;
    uint32_t        OCxCONInv;
    uint32_t        OCxR;
    uint32_t        OCxRClr;
    uint32_t        OCxRSet;
    uint32_t        OCxRInv;
    uint32_t        OCxRS;
    uint32_t        OCxRSClr;
    uint32_t        OCxRSSet;
    uint32_t        OCxRSInv;
} __attribute__((packed)) OCCH;

typedef struct _DMACH
{
    __DCH0CONbits_t DCHxCON;
    uint32_t DCHxCONClr;
    uint32_t DCHxCONSet;
    uint32_t DCHxCONInv;

    __DCH0ECONbits_t DCHxECON;
    uint32_t DCHxECONClr;
    uint32_t DCHxECONSet;
    uint32_t DCHxECONInv;

    __DCH0INTbits_t DCHxINT;
    uint32_t DCHxINTClr;
    uint32_t DCHxINTSet;
    uint32_t DCHxINTInv;

    uint32_t DCHxSSA;
    uint32_t DCHxSSAClr;
    uint32_t DCHxSSASet;
    uint32_t DCHxSSAInv;

    uint32_t DCHxDSA;
    uint32_t DCHxDSAClr;
    uint32_t DCHxDSASet;
    uint32_t DCHxDSAInv;

    uint32_t DCHxSSIZ;
    uint32_t DCHxSSIZClr;
    uint32_t DCHxSSIZSet;
    uint32_t DCHxSSIZInv;

    uint32_t DCHxDSIZ;
    uint32_t DCHxDSIZClr;
    uint32_t DCHxDSIZSet;
    uint32_t DCHxDSIZInv;

    uint32_t DCHxSPTR;
    uint32_t DCHxSPTRClr;
    uint32_t DCHxSPTRSet;
    uint32_t DCHxSPTRInv;

    uint32_t DCHxDPTR;
    uint32_t DCHxDPTRClr;
    uint32_t DCHxDPTRSet;
    uint32_t DCHxDPTRInv;

    uint32_t DCHxCSIZ;
    uint32_t DCHxCSIZClr;
    uint32_t DCHxCSIZSet;
    uint32_t DCHxCSIZInv;

    uint32_t DCHxCPTR;
    uint32_t DCHxCPTRClr;
    uint32_t DCHxCPTRSet;
    uint32_t DCHxCPTRInv;

    uint32_t DCHxDAT;
    uint32_t DCHxDATClr;
    uint32_t DCHxDATSet;
    uint32_t DCHxDATInv;
} __attribute__((packed)) DMACH;

typedef struct _IDHDR
{
    uint32_t    const   cbInfo; // size of this structure
    uint32_t    const   ver;    // version of the structure
    INSTR_ID    const   id;     // the ID of this instrument
    CFGNAME             cfg;    // what cfg did this come from USER / FACTORY / UNCALIBRATED
    const MACADDR       mac;    // the mac address of the hardware
} IDHDR;

typedef struct _COMHDR
{
    IDHDR       idhdr;
    
    // some local variables
    uint32_t    activeFunc;     // only one function can run at a time.
    uint32_t    state;          // state machine state
    uint32_t    cNest;          // If we are nesting to another function
    uint32_t    tStart;         // a start time for waiting
    
} COMHDR;

typedef struct _DCVOLT
{
    COMHDR                      comhdr;

    // PWM registers
    OCCH volatile *     const   pOCdc;
    
    // ADC feedback PIN
    uint32_t            const   channelFB;
    
    // A = (uVDC1 - uVDC2) / (pwm2-pwm1) 
    int32_t             const   A;
    
    // B = (uVDC1) + (A)(pwm)
    int32_t             const   B;
    
    // local variables
    int32_t     mvDCout;

#ifdef __cplusplus
    _DCVOLT(INSTR_ID id) :  comhdr({{sizeof(struct _DCVOLT), CALVER, id, CFGUNCAL, {0,0,0,0,0,0}}, SMFnNone, Idle, 0, 0}),

                            pOCdc((OCCH *) dcInitData[id-DCVOLT1_ID][0]),

                            channelFB(dcInitData[id-DCVOLT1_ID][1]),

                            A(40000), 
                            B(7000000),

                            // non const values
                            mvDCout(0)
    {
    }
#endif

} DCVOLT;

typedef struct _AWG
{
    COMHDR          comhdr;

    // PWM registers for offset control
    OCCH            volatile    *   const   pOCoffset;
    
    // timer
    TMRCH           volatile    *   const   pTMR;
    
    // DMA Channels to transfer bit pattern
    DMACH           volatile    *   const   pDMA;
    uint32_t                                addrPhySrc;
    uint32_t                                cbSrc;

    // ADC feedback channel
    uint32_t                        const   channelFB;
    
    // R2R output Latch
    uint32_t        volatile    *   const   pLATx;
    
    // AWGoffset = B - A(PWM)
    int32_t                         const   A;  // ~11995
    int32_t                         const   B;  // ~2099125
    
    // Local temp variables
    int32_t                     t1;   
    int32_t                     t2;
    int32_t                     t3;
    uint16_t                    PRXOnRun;
    int16_t                     mvDCOffset;

    // the DAC MAP
    uint16_t                dacMap[HWDACSIZE];  
    int16_t                 mVFB[HWDACSIZE];   

#ifdef __cplusplus
    _AWG(INSTR_ID id) :     comhdr({{sizeof(struct _AWG), CALVER, id, CFGUNCAL, {0,0,0,0,0,0}}, SMFnNone, Idle, 0, 0}),

                            pOCoffset((OCCH *) &OC7CON),
                            pTMR((TMRCH *) &T7CON),
                            pDMA((DMACH *) &DCH7CON), addrPhySrc(0), cbSrc(0),
 
                            channelFB(CH_AWG_FB),

                            pLATx((uint32_t *) &LATH),

                            A(11995),
                            B(2099125),

                            t1(0),  
                            t2(0),
                            t3(0),
                            PRXOnRun(9),
                            mvDCOffset(0)
    {
        for(int i=0; i<HWDACSIZE; i++) dacMap[i] = DACDATA(511);
        for(int i=0; i<HWDACSIZE; i++) mVFB[i] = 0;
    }
#endif

} AWG;

typedef struct _LA
{
    COMHDR          comhdr;

    // DMA Channels to transfer bit pattern
    TMRCH           volatile    *   const   pTMR;
    DMACH           volatile    *   const   pDMA;
    uint32_t                        const   addPhySrc;
    uint32_t                        const   addPhyDst;
    uint32_t                        const   cbDst;

#ifdef __cplusplus
    _LA(INSTR_ID id) :     comhdr({{sizeof(struct _LA), CALVER, id, CFGUNCAL, {0,0,0,0,0,0}}, SMFnNone, Idle, 0, 0}),
                            pTMR((TMRCH *) &T7CON),
                            pDMA((DMACH *) &DCH7CON), addPhySrc(KVA_2_PA(&PORTE)), addPhyDst(KVA_2_PA(rgLOGICBuff)), cbDst((LADMASIZE * sizeof(rgLOGICBuff[0])))
    {
    }
#endif
} LA;

typedef struct _OSCGainCal
{
    int32_t const   dPWM;       // for calibration, max PWM delta
    int32_t const   dVin;       // for calibration, Voltage Swing
    
    // Vadc = (A')Vin - (B')Vpwm + C
    // uVin = A(Dadc) + B(PWM) - C
    int32_t const   A;        
    int32_t const   B;           
    int32_t const   C;            
} OSCGainCal;

typedef struct _OSC
{
    COMHDR          comhdr;

    // PWM input offset control
    OCCH            volatile    *   const   pOCoffset;
    
    // Gain control pins
    uint32_t        volatile    *   const   pLATIN1;
    uint32_t                        const   maskIN1;    
    uint32_t        volatile    *   const   pLATIN2;
    uint32_t                        const   maskIN2;   
    
    // ADC channels
    uint32_t                        const   channelInA;
    uint32_t                        const   channelInB;

    // timer
    TMRCH           volatile    *   const   pTMRtrg1;
    OCCH            volatile    *   const   pOCtrg2;

    // DMA channels
    DMACH           volatile    *   const   pDMAch1;
    DMACH           volatile    *   const   pDMAch2;

    // current Gain channel
    uint32_t                    curGain;
    
    // base DC voltage to zero the ADC channel for calibration
    int32_t                     t1;   
    
    // Gain channel calibration constants.
    OSCGainCal                  rgGCal[NbrOfADCGains];
    
#ifdef __cplusplus
    _OSC(INSTR_ID id) :     comhdr({{sizeof(struct _OSC), CALVER, id, CFGUNCAL, {0,0,0,0,0,0}}, SMFnNone, Idle, 0, 0}), 

                            pOCoffset((OCCH *) oscInitData[(id-OSC1_ID)/2][0]),

                            pLATIN1((uint32_t *) oscInitData[(id-OSC1_ID)/2][1]),
                            maskIN1(oscInitData[(id-OSC1_ID)/2][2]),   
                            pLATIN2((uint32_t *) oscInitData[(id-OSC1_ID)/2][3]),
                            maskIN2(oscInitData[(id-OSC1_ID)/2][4]),   

                            channelInA(oscInitData[(id-OSC1_ID)/2][5]),
                            channelInB(oscInitData[(id-OSC1_ID)/2][6]),

                            pTMRtrg1((TMRCH *) oscInitData[(id-OSC1_ID)/2][7]),
                            pOCtrg2((OCCH *) oscInitData[(id-OSC1_ID)/2][8]),

                            pDMAch1((DMACH *) oscInitData[(id-OSC1_ID)/2][9]),
                            pDMAch2((DMACH *) oscInitData[(id-OSC1_ID)/2][10]),

                            // non const values
                            curGain(0),
                            t1(0),

                            // gain constants
                            rgGCal({
                            // dPWM   dVOLT   A       B        C 
                                {7,   1000,  732, 151312, 26470588},    // Gain 1   (1)
                                {40,  3000, 2910, 112656, 19708029},    // Gain 2   (1/4)
                                {125, 4000, 5833,  64308, 11250000},    // Gain 3   (1/8)
                                {125, 4000, 9913,  15434,  2700000}     // Gain 4   (3/40)
                            })
    {
    }
#endif

} OSC;

#define MAX_OSC_AWG_SIZE        (sizeof(OSC) > sizeof(AWG) ? sizeof(OSC) : sizeof(AWG))
#define MAX_OSC_AWG_DCVOLT_SIZE (MAX_OSC_AWG_SIZE > sizeof(DCVOLT) ? MAX_OSC_AWG_SIZE : sizeof(DCVOLT))
#define MAX_INSTR_SIZE          (MAX_OSC_AWG_DCVOLT_SIZE > sizeof(LA) ? MAX_OSC_AWG_DCVOLT_SIZE : sizeof(LA))

typedef struct _WiFiScanInfo
{
    STATE       state;
    IPSTATUS    status;
    int32_t     cNetworks;
    int32_t     iNetwork;
    int32_t     t1;
    SCANINFO    scanInfo;

#ifdef __cplusplus
    _WiFiScanInfo() : state(Idle), status(ipsSuccess), cNetworks(0), iNetwork(0)
    {
        memset(&scanInfo, 0, sizeof(scanInfo));
    }
#endif

} WiFiScanInfo;

typedef struct _WiFiConnectInfo
{
    COMHDR          comhdr;
    IPSTATUS        status;
    SECURITY        wifiKey;
    char            ssid[DEWF_MAX_SSID_LENGTH+1];       // Network SSID value
    union
    {
        WPA2KEY     wpa2Key;
        WEP40KEY    wep40Key;
        WEP104KEY   wep104Key;
    } key;

#ifdef __cplusplus
    _WiFiConnectInfo() : comhdr({{sizeof(struct _WiFiConnectInfo), WFVER, WIFIPARAM_ID, CFGNONE, {0,0,0,0,0,0}}, SMFnNone, Idle, 0, 0}), 
                        status(ipsSuccess), wifiKey(DEWF_SECURITY_OPEN)
    {
        memset(ssid, 0, sizeof(ssid));
    }
#endif

} WiFiConnectInfo;

#ifdef __cplusplus

    typedef struct _INSTRGRP
    {
        uint32_t                state;          // state machine state
        uint32_t                state2;         // state machine state
        uint32_t                tStart;         // a start time for waiting
        uint32_t                iInstr;         // index into instr and Usage arrays
        DFILE&                  dFile;          // file handle for use
        uint32_t const          chInstr;        // how many instruments in the array
        INSTR_ATT const * const rgUsage;        // Instrument attribute
        HINSTR const * const    rghInstr;       // the array of instruments
    } INSTRGRP;

/************************************************************************/
/************************************************************************/
/********************** External Variables  *****************************/
/************************************************************************/
/************************************************************************/
    
    // this is our handle array of instruments
    extern DFILE            dWiFiFile;
    extern OSSerial         Serial;
    extern uint8_t          uartBuff[512];
    extern INSTRGRP         instrGrp;
   extern FLASHVOL         flashVol;
    extern DSDVOL           dSDVol;
    extern STATE            MState;

  
#endif // c++

    #include <Trigger.h>
    #include <ProcessJSONCmd.h>
    #include <ParseOpenScope.h>   // OpenScope token parsing
 
    extern PJCMD pjcmd;

/************************************************************************/
/************************************************************************/
/********************** Forward References  *****************************/
/************************************************************************/
/************************************************************************/
#ifdef __cplusplus
    
    extern STATE JSONCmdTask(void);

    extern OSPAR oslex;
    extern STATE LEDTask(void);
    extern STATE CFGSdHotSwapTask(void);

    extern STATE IOReadFile(DFILE& dFile, VOLTYPE const vol, char const * const szFileName, IDHDR& idhdr);
    extern STATE IOWriteFile(DFILE& dFile, VOLTYPE const vol, char const * const szFileName, IDHDR const & idhdr);
    extern STATE IOReadLine(char * szInput, uint32_t cb);

    extern STATE UIMainPage(DFILE& dFile, VOLTYPE const wifiVol, WiFiConnectInfo& wifiConn);
    
    // C++  forward references
    extern uint32_t CFGCreateFileName(const IDHDR& idHDR, char const * const szSuffix, char * sz, uint32_t cb);
    extern STATE CFGSysInit(void);
    extern STATE CFGOpenVol(void);

    extern STATE CFGCalibrateInstruments(INSTRGRP& instrGrp);
    extern STATE CFGPrintCalibrationInformation(INSTRGRP& instrGrp);

    extern STATE CFGSaveCalibration(INSTRGRP& instrGrp, VOLTYPE const vol, CFGNAME const cfgName);
    extern STATE CFGReadCalibrationInfo(INSTRGRP& instrGrp, VOLTYPE const vol, CFGNAME const  cfgName);
    extern STATE CFGGetCalibrationInfo(INSTRGRP& instrGrp);

    extern STATE WiFiLookupConnInfo(DFILE& dFile, WiFiConnectInfo& wifiConn);
    extern STATE WiFiLoadConnInfo(DFILE& dFile, VOLTYPE const vol, char const szSSID[], WiFiConnectInfo& wifiConn);
    extern STATE WiFiSaveConnInfo(DFILE& dFile, VOLTYPE const vol, WiFiConnectInfo& wifiConn);
    extern STATE WiFiConnect(WiFiConnectInfo& wifiConn, bool fForce);
    extern STATE WiFiDisconnect(void);
    extern STATE WiFiScan(WiFiScanInfo& wifiScan);

    extern STATE ResetInstruments(void);

extern "C" {
#endif
    extern void __attribute__((noreturn)) _softwareReset(void);
    extern bool OSAdd(uint8_t m1[], uint32_t cm1, uint8_t m2[], uint32_t cm2, uint8_t r[], uint32_t cr);
    extern bool OSMakeNeg(uint8_t m1[], uint32_t cm1);
    extern bool OSUMult(uint8_t m1[], uint32_t cm1, uint8_t m2[], uint32_t cm2, uint8_t r[], uint32_t cr);
    extern bool OSMult(int8_t m1[], uint32_t cm1, int8_t m2[], uint32_t cm2, int8_t r[], uint32_t cr);
    extern bool OSDivide(int8_t m1[], uint32_t cm1, int64_t d1, int8_t r[], uint32_t cr);

    extern char * ulltoa(uint64_t val, char * buf, uint32_t base);
    extern char * illtoa(int64_t val, char * buf, uint32_t base);
    extern char * GetPercent(int32_t diff, int32_t ideal, int32_t cbD, char * pchOut, int32_t cbOut);

    // C forward references
    extern STATE InitInstruments(void);
    extern STATE InitSystemVoltages(void);
    extern uint32_t UpdateTime(uint32_t timeUnits);
    
    extern STATE DCCalibrate(HINSTR hDCVolt);
    extern STATE DCSetVoltage(HINSTR hDCVolt, int32_t mvDCOut);
    
    extern STATE AWGCalibrate(HINSTR hAWG);
    extern STATE AWGSetOffsetVoltage(HINSTR hAWG, int32_t mvDCOffset);
    extern STATE AWGSetCustomWaveform(HINSTR hAWG, int16_t rgWaveform[], uint32_t cWaveformEntries, int16_t mvDCOffset, uint32_t cSamplesPerSec);
    extern STATE AWGSetWaveform(HINSTR hAWG, WAVEFORM waveform , uint32_t freqHz, int32_t mvP2P, int32_t mvDCOffset);
    extern uint32_t AWGCalculateBuffAndSps(uint32_t reqFreq, uint32_t * pcBuff, uint32_t * pSps);
    extern STATE AWGRun(HINSTR hAWG);
    extern STATE AWGStop(HINSTR hAWG);
    
    #define AWGMV2PWM(_a, _mv) max(min(((uint16_t) (((_a)->B - (1000l * (_mv)) + (((_a)->A) / 2)) / (_a)->A)), 325), 5)
    #define AWGPWV2MV(_a, _pwm) ((int16_t) (((_a)->B - ((_a)->A * (_pwm)) + 500l) / 1000l))
    #define AWGRequestedmvOffsetToActual(_h, _mv)  AWGPWV2MV((AWG *) _h, AWGMV2PWM((AWG *) _h, _mv))
    extern int16_t AWGmVGetActualOffsets(HINSTR hAWG, int16_t mvOffset, int16_t * pTableOffset);

    extern STATE AverageADC(uint8_t channelNumber, uint8_t pwr2Ave, int32_t * pResult);
    extern STATE FBAWGorDCuV(uint32_t channelFB, int32_t * puVolts);
    extern STATE FBUSB5V0uV(uint32_t * puVolts);
    extern STATE FBNUSB5V0uV(uint32_t * puVolts);
    extern STATE FBREF1V5uV(uint32_t * puVolts);
    extern STATE FBREF3V0uV(uint32_t * puVolts);
    extern STATE FBVCC3V3uV(uint32_t * puVolts);
    
    extern STATE OSCReset(HINSTR hOSC);
    extern STATE OSCCalibrate(HINSTR hOSC, HINSTR hDCVolt);
    extern bool  OSCSetGain(HINSTR hOSC, uint32_t iGain);
    extern STATE OSCSetOffset(HINSTR hOSC, int32_t mvOffset);
    extern STATE OSCSetGainAndOffset(HINSTR hOSC, uint32_t iGain, int32_t mvOffset);
    extern STATE OSCRun(HINSTR hOSC, IOSC * piosc);
    extern bool  OSCVinFromDadcArray(HINSTR hOSC, int16_t rgDadc[], uint32_t cDadc);
    
    extern STATE LARun(HINSTR hLA, ILA * pila);
    extern STATE LAReset(HINSTR hLA);

    extern bool CalculateBufferIndexes(BIDX * pbidx);
    extern bool ScrollBuffer(uint16_t rgBuff[], int32_t cBuff, int32_t iNew, int32_t iCur);
    
    #define OSCPWM(_pOSC, _gain, _mVOff) ((((_mVOff) * 1000l) + (_pOSC)->rgGCal[_gain].C + ((_pOSC)->rgGCal[_gain].B / 2)) / (_pOSC)->rgGCal[_gain].B)
    #define OSCBandC(_pOSC, _gain, _pwm) (((int32_t) _pwm) * (_pOSC)->rgGCal[_gain].B - (_pOSC)->rgGCal[_gain].C)
    #define OSCDadcFromVinBandC(_pOSC, _gain, _mVIN, _BandC) ((int16_t) (((_mVIN)*1000 - _BandC + (_pOSC)->rgGCal[_gain].A/2) / (_pOSC)->rgGCal[_gain].A))
    #define OSCVinFromDadcBandC(_pOSC, _gain, _dadc, _BandC) ((int16_t) (((((int32_t) (_dadc)) * (_pOSC)->rgGCal[(_gain)].A + (_BandC)) + 500) / 1000))
    #define OSCDadcFromVinGainOffset(_pOSC, _mVIN, _gain, _mVOff) OSCDadcFromVinBandC((_pOSC), (_gain), (_mVIN), OSCBandC((_pOSC), (_gain), OSCPWM((_pOSC), (_gain), (_mVOff))))
    #define OSCVinFromDadcGainOffset(_pOSC,  _dadc, _gain,_mVOff) ((int16_t) (((((int32_t) (_dadc)) * (_pOSC)->rgGCal[(_gain)].A + OSCBandC((_pOSC), (_gain),  OSCPWM((_pOSC), (_gain), (_mVOff)))) + 500) / 1000))

    extern void TRGAbort(void);
    extern bool TRGSetUp(void);
    extern bool TRGSingle(void);
    extern bool TRGForce(void);

    // all of the LA locking is so we don't run the AWG when the LA is running
    #define LockLA()        (DCH7DSA = KVA_2_PA(rgLOGICBuff))
    #define UnLockLA()      (DCH7DSA = KVA_2_PA(&LATH))
    #define IsLALocked()    (DCH7DSA == KVA_2_PA(rgLOGICBuff))

    // Instrument Idle macros
    #define IsDCIdle()  (pjcmd.idcCh1.state.processing == Idle  && pjcmd.idcCh2.state.processing == Idle)
    #define IsAWGIdle() (pjcmd.iawg.state.processing == Idle    || pjcmd.iawg.state.processing == Stopped)
    #define IsLAIdle()  ((pjcmd.ila.state.processing == Idle || pjcmd.ila.state.processing == Triggered || pjcmd.ila.state.processing == Waiting) && !IsLALocked())
    #define IsOSCIdle() ((pjcmd.ioscCh1.state.processing == Idle  || pjcmd.ioscCh1.state.processing == Triggered || pjcmd.ioscCh1.state.processing == Waiting) && (pjcmd.ioscCh2.state.processing == Idle || pjcmd.ioscCh1.state.processing == Triggered || pjcmd.ioscCh2.state.processing == Waiting)) 
    #define IsTrgIdle() (pjcmd.trigger.state.processing == Idle || pjcmd.trigger.state.processing == Triggered)
    #define AreInstrumentsIdle()  (IsDCIdle() && IsLAIdle() && IsAWGIdle() && IsOSCIdle() && IsTrgIdle())
 
    extern STATE HTTPSetup(void);
    extern void HTTPTask(void);
    extern bool HTTPEnable(bool fEnable);
    #define IsHTTPRunning() (HTTPState == HTTPEnabled && deIPcK.isIPReady())
    
    extern int64_t GetSamples(int64_t psec, int64_t msps);
    extern int64_t GetPicoSec(int64_t samp, int64_t msps);
    
    extern t_deviceInfo     myMRFDeviceInfo;            // The MRF device info.
    extern STATE            HTTPState;

#ifdef __cplusplus
            }
#endif

    // undocumented call, aid to manufacturing test.
    extern uint32_t MfgTest(uint32_t testNbr);

// some gpio pins
#define PIN_SD_DET  PORTD,  (1 << 1)    // RD1,  pin 36

#define PIN_INT_MRF PORTG,  (1 << 8)    // RG8,  pin 59
#define PIN_HIB_MRF LATD,   (1 << 13)   // RD13, pin 60
#define PIN_RST_MRF LATA,   (1 << 4)    // RA4,  pin 61
#define PIN_WP_MRF  LATA,   (1 << 14)   // RA14, pin 62

#define PIN_LED_1   LATJ,   (1 << 4)    // RJ4,  pin 13
#define PIN_LED_2   LATJ,   (1 << 2)    // RJ2,  pin 48
#define PIN_LED_3   LATJ,   (1 << 1)    // RJ1,  pin 49
#define PIN_LED_4   LATJ,   (1 << 0)    // RJ0,  pin 50

#define PIN_BTN1    PORTG,  (1 << 12)   // G12  pin 42

#if defined(NO_IO_BUS)
    #define PIN_CS_SD   U3STA,  (1 << 13)   // U3STAbits.UTXINV
    #define PIN_CS_MRF  U4STA,  (1 << 13)   // U4STAbits.UTXINV
#else
    #define PIN_CS_SD   LATD, (1 << 14)   // RD14, pin 52
    #define PIN_CS_MRF  LATB,   (1 << 15)   // RB15, pin 56
#endif

// gpio macros
#define SetGPIO(a,c)  SetGPIO2(a,c)
// #define SetGPIO2(a,b,c) {if(c==0) a##CLR = b; else a##SET = b;}
#define SetGPIO2(a,b,c) {if(c==0) a &= ~(b); else a |= b;}

#define GetGPIO(a)  GetGPIO2(a)
#define GetGPIO2(a,b) ((a & b) != 0)

/* ------------------------------------------------------------ */
/*					A/D Converter Declarations					*/
/* ------------------------------------------------------------ */
#define ADCRANGE        4096ul              // analog read will return a max number of ADCRANGE-1
#define ADCTADFREQ      50000000ul          // How fast to run the TAD ADC clock
#define VREFMV          3000                // we are using a 3v reference voltage

#define ADCCLASS1       5                   // adc 0-5 are class 1
#define ADCCLASS2       12                  // adc 0-11 are class 2
#define ADCCLASS3       45                  // adc 0-44 are class 3
#define ADCALT          50                  // adc 45-49 are alt class 1

#define ADCTADSH        87ul                // How many TADs the Sample and Hold will charge for the shared ADCs, total conversion is this +13 TADs (12 bits)
#define ADCTADDC        3ul                 // How many TADs the Sample and Hold will charge the analog input ADCs (Dedicated)

// we are using 12 bit resoluiton, or 13 TADs, the TAD clock is 50MHz
    // each shared conversion is 100 TADs or 2us per conversion
    // each dedicated conversion is 16 TADs or 320ns

//**************************************************************************
//**************************************************************************
//******************* ADC Macros       *********************************
//**************************************************************************
//**************************************************************************

// ADC resolution
#define RES6BITS        0b00        // 6 bit resolution
#define RES8BITS        0b01        // 8 bit resolution
#define RES10BITS       0b10        // 10 bit resolution
#define RES12BITS       0b11        // 12 bit resolution

// ADC reference soruces
#define VREFPWR         0b000       // internal 3.3v ref
#define VREFHEXT        0b001       // external high ref 
#define VREFLEXT        0b010       // external low ref

// ADC clock sources
#define CLKSRCFRC       0b11        // Internal 8 MHz FRC clock source
#define CLKSRCREFCLK3   0b10        // External Clk 3 clock source
#define CLKSRCSYSCLK    0b01        // System clock source
#define CLKSRCPBCLK3    0b00        // PB Bus 3 as clock source

// PB 3, ADC, OC, Timers 
#define PBUS3DIV    2               // divide system clock by (1-128); the default is 2 and that is what we are using
#define PB3FREQ     (F_CPU / PBUS3DIV)

// TQ CLOCK
#define TQCLKDIV    1                               // we want to run the TQ at F_CPU == 200MHz
// Global ADC TQ Clock prescaler 0 - 63; Divide by (CONCLKDIV*2) However, the value 0 means divide by 1
#define TQCONCLKDIV (TQCLKDIV >> 1)            
#define TQ          (F_CPU / TQCLKDIV)              // ADC TQ clock frequency

// TAD = TQ / (2 * ADCTADDIV), ADCTADDIV may not be zero
#define ADCTADDIV   ((TQ / ADCTADFREQ) / 2)
#if (ADCTADDIV == 0)
    #error ADCTADFREQ is too high or TQ is too low
#endif

#define CBITSRES        ((2ul * RES12BITS) + 6ul)
#define CTADCONV        (CBITSRES + 1ul)
#define SHCONVFREQSH    (ADCTADFREQ / (CTADCONV + ADCTADSH))        // how fast we can turn around and do the next sample, must be faster than sample rate
#define SHCONVFREQDC    (ADCTADFREQ / (CTADCONV + ADCTADDC))        // how fast we can turn around and do the next sample, must be faster than sample rate

#define __PIC32MZXX__
 
#endif // OpenScope_h
