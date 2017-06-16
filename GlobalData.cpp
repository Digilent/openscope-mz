/************************************************************************/
/*                                                                      */
/*    GlobalData.cpp                                                    */
/*                                                                      */
/*   This file contains the Global data declarations for the            */
/*    OpenScope project                                                 */
/*                                                                      */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    12/20/2016 (KeithV): Created                                       */
/************************************************************************/
#include <OpenScope.h>

/************************************************************************/
/*    Power Supply voltages                                             */
/************************************************************************/
uint32_t uVUSB       = 5000000;
uint32_t uV3V3       = 3300000;
uint32_t uVRef3V0    = 3000000;
uint32_t uVRef1V5    = 1500000;
uint32_t uVNUSB      = 5000000;

/************************************************************************/
/*    JSON data                                                         */
/************************************************************************/
// not in JSON mode
bool fModeJSON = false;
uint32_t aveLoopTime = 0;

// the JSON command structure
PJCMD pjcmd = PJCMD();
OSPAR oslex = OSPAR();

/************************************************************************/
/*    block IO                                                          */
/*    when the logic analyser runs we can do zero IO                    */
/*    LEDs, and SD DET must stop!                                       */
/************************************************************************/
bool fBlockIOBus = false;

/************************************************************************/
/*    Serial Object                                                     */
/************************************************************************/
// the serial buffer
uint8_t uartBuff[512];

// the serial object (AND ISR)
OSSerialOBJ(Serial, uartBuff, sizeof(uartBuff), 5, 0, 4);

/************************************************************************/
/*    WiFi Objects                                                      */
/************************************************************************/
MACADDR  macOpenScope = MACNONE;
t_deviceInfo myMRFDeviceInfo = t_deviceInfo();
DFILE    dWiFiFile;

char const * const rgszSecurityMode[DEWF_MAX_SECURITY_TYPE+1] = {"open", "wep40", "wep104", "wpa", "wpa", "wpa2", "wpa2", "wpaAuto", "wpaAuto", "wps", "wps"};
char const * const rgszAdapter[] = {"none", "workingParameterSet", "wlan0"};

/************************************************************************/
/*    SD Card Reader variables                                          */
/************************************************************************/
DSPIPOBJ(dSDSpi, 6, 3, PIN_CS_SD);
DSDVOL dSDVol(dSDSpi);

// create the FLASH volume memory space in flash
const uint8_t __attribute__((aligned(BYTE_PAGE_SIZE), section(".flashfat"))) flashVolMem[_FLASHVOL_CBFLASH(FLASHVOL::CBMINFLASH)] = { 0xFF };
FLASHVOL    flashVol(flashVolMem, sizeof(flashVolMem));

/************************************************************************/
/************************************************************************/
/********************** Instrument Instances ****************************/
/************************************************************************/
/************************************************************************/
static DCVOLT dc1VoltData = DCVOLT(DCVOLT1_ID);
static const HINSTR hDC1Volt = HINSTRFromInstr(dc1VoltData);

static DCVOLT dc2VoltData = DCVOLT(DCVOLT2_ID);
static const HINSTR hDC2Volt = HINSTRFromInstr(dc2VoltData);

static AWG awgData = AWG(AWG1_ID);
static const HINSTR hAWG = HINSTRFromInstr(awgData);

static LA laData = LA(LOGIC1_ID);
static const HINSTR hLA = HINSTRFromInstr(laData);

static OSC osc1Data = OSC(OSC1_ID);
static const HINSTR hOSC1 = HINSTRFromInstr(osc1Data);

static OSC osc2Data = OSC(OSC2_ID);
static const HINSTR hOSC2 = HINSTRFromInstr(osc2Data);

// very carefully constructed... this is just a list of instruments to be calibrated
// however.... if you are calibrating a OSC, you must follow it with the DC output that 
// is connected to it. For best results, you should ensure the DC outputs are calibrated first,
// that is either previousely calibrated, or calibrated earlier in the list of instruments to calibrate.
// The DC instruments specified immedately after the OSC, will not be calibrated, they are just
// used to calibrate the OSC.
static DFILE            dCfgFile;              // Create a File handle to use to open files with
static const INSTR_ATT  rgUsage[INSTR_END_ID]       = {instrIDNone, instrIDCal,  instrIDCal, instrIDCal, instrIDCal, instrIDCalSrc, instrIDCal,     instrIDCalSrc,  instrIDNone};
const HINSTR            rgInstr[INSTR_END_ID]       = {NULL,        hDC1Volt,    hDC2Volt,   hAWG,       hOSC1,      hDC1Volt,      hOSC2,          hDC2Volt,       hLA};
char const * const      rgszInstr[END_ID]           = {"NONE",      "DCOUT1",    "DCOUT2",   "AWG1",     "OSC1",     "OSC1DC1",     "OSC2",        "OSC2DC2",       "LA1",
                                                       "NONE","NONE","NONE","NONE","NONE","NONE","WFPARM","EXT-TRG","FORCE-TRG"};
INSTRGRP                instrGrp = {Idle, Idle, 0, 0, dCfgFile, INSTR_END_ID, rgUsage, rgInstr};

/************************************************************************/
/************************************************************************/
/****************************  Instrument Buffers ***********************/
/************************************************************************/
/************************************************************************/
int16_t     __attribute__((coherent, keep, address(0x80040000))) rgOSC1Buff[AINBUFFSIZE];
int16_t     __attribute__((coherent, keep, address(0x80050000))) rgOSC2Buff[AINBUFFSIZE];
uint16_t    __attribute__((coherent, keep, address(0x80060000))) rgAWGBuff[AWGBUFFSIZE];
uint16_t    __attribute__((coherent, keep, address(0x80070000))) rgLOGICBuff[LABUFFSIZE];

/************************************************************************/
/************************************************************************/
/********************** Instrument Initialization Constants *************/
/************************************************************************/
/************************************************************************/
uint32_t const dcInitData[2][2] = 
{
    {(uint32_t) &OC6CON, CH_DC1_FB}, 
    {(uint32_t) &OC4CON, CH_DC2_FB}
};

uint32_t const oscInitData[2][12] = 
{
    {
        (uint32_t) &OC8CON,                         // Offset PWM
        (uint32_t) &LATA, (0x00000001 << 2),        // Gain select 1      
        (uint32_t) &LATA, (0x00000001 << 3),        // Gain select 2
        0, 1,                                       // ADC channels
        (uint32_t) &T3CON,                          // DMA trigger timer
        (uint32_t) &OC5CON,                         // DMA interleave trigger OC
        (uint32_t) &DCH3CON, (uint32_t) &DCH4CON,   // DMA channels
    }, 
    {
        (uint32_t) &OC9CON,                         // Offset PWM
        (uint32_t) &LATA, (0x00000001 << 6),        // Gain select 1      
        (uint32_t) &LATA, (0x00000001 << 7),        // Gain select 2
        2, 3,                                       // ADC channels
        (uint32_t) &T5CON,                          // DMA trigger timer
        (uint32_t) &OC1CON,                         // DMA interleave trigger OC
        (uint32_t) &DCH5CON, (uint32_t) &DCH6CON,   // DMA channels
    }
};

char const * const rgCFGNames[CFGEND] = {"NONE", "UNCALIBRATED", "flash", "sd0", "CALIBRATED"};

//************************************************************************
//*	This sets the MPIDE version number in the image header as defined in the linker script
extern const uint32_t _verMPIDE_Stub;
const uint32_t __attribute__((section(".mpide_version"))) _verMPIDE_Stub = MPIDEVER;    // assigns the build number in the header section in the image
