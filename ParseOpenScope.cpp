/************************************************************************/
/*                                                                      */
/*    LexOpenScope.cpp                                                  */
/*                                                                      */
/*    Header for the OpenScope token parser                             */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*    7/11/2016(KeithV): Created                                        */
/************************************************************************/
#include    <OpenScope.h>

//#define JUST_LEX_JSON

/************************************************************************/
/*    RFC 7159 say JSON-text = ws value ws                              */
/*    however JSON-text = ws object ws                                  */
/*    is a valid subset that we are going to adhear to.                 */
/************************************************************************/

/************************************************************************/
/*    To Be Removed, replaced new stings                                */
/************************************************************************/
static const char szTriggerDelay[]      = ",\"triggerDelay\":";

/************************************************************************/
/*    Scope Gains                                                       */
/************************************************************************/
static const char szGain1[] = "1";
static const char szGain2[] = "0.25";
static const char szGain3[] = "0.125";
static const char szGain4[] = "0.075";

/************************************************************************/
/*    HTML Strings                                                      */
/************************************************************************/
static const char szHEX[]           = "0x";
static const char szNewLine[]       = "\r\n";
static const char szValueSep[]      = ",";

static const char szStartObject[]   = "{";
static const char szEndObject[]     = "}";
static const char szEndArray[]      = "]";
static const char szStartChArray[]   = ":[";
static const char szDeviceEnd[]     = "]}";

static const char szCh1Array[]      = "\"1\":[";
static const char szCh2Array[]      = "\"2\":[";
static const char szCh1Object[]     = "\"1\":{";
static const char szCh2Object[]     = "\"2\":{";

// this must follow what is in the OPEN_SCOPE_STATES enum
static char const * const rgInstrumentStates[] = {"\"idle\"", "\"armed\"", "\"acquiring\"", "\"triggered\"", "\"stopped\"", "\"running\"", "\"busy\""};

// must follow TRGTP enum
static char const * const rgThresholdType[] = {"\"none\"", "\"risingEdge\"", "\"fallingEdge\""};

// some common strings
static const char szActualVOffset[]     = ",\"actualVOffset\":";
static const char szActualGain[]        = ",\"actualGain\":";
static const char szActualSampleFreq[]  = ",\"actualSampleFreq\":";
static const char szActualFreq[]        = ",\"actualSignalFreq\":";
static const char szVpp[]               = ",\"actualVpp\":";
static const char szAcqCount[]          = ",\"acqCount\":";
static const char szBitMask[]           = ",\"bitmask\":";
static const char szBufferSize[]        = ",\"actualBufferSize\":";
static const char szBinaryLength[]      = ",\"binaryLength\":";
static const char szBinaryOffset[]      = ",\"binaryOffset\":";
static const char szTriggerIndex[]      = ",\"triggerIndex\":";
static const char szActualTriggerDelay[] = ",\"actualTriggerDelay\":";
static const char szPointOfInterest[]   = ",\"pointOfInterest\":";
static const char szInstrument[]        = "\"instrument\":";
static const char szOsc[]               = "\"osc\"";
static const char szLa[]                = "\"la\"";
static const char szForce[]             = "\"force\"";
static const char szChannel[]           = ",\"channel\":";
static const char szType[]              = ",\"type\":";
static const char szState[]             = ",\"state\":";
static const char szLowerThreshold[]    = ",\"lowerThreshold\":";
static const char szUpperThreshold[]    = ",\"upperThreshold\":";
static const char szRisingEdge[]        = ",\"risingEdge\":";
static const char szFallingEdge[]       = ",\"fallingEdge\":";

static const char szStatusCode0[]       = ",\"statusCode\":0";
static const char szWait[]              = ",\"wait\":";
static const char szWait0[]             = ",\"wait\":0}";
static const char szWait500[]           = ",\"wait\":500}";
static const char szWaitCalTime[]       = ",\"wait\":30000}";
static const char szWaitUntil[]         = ",\"wait\":-1}";

static const char szStatusCode[] = "{\"statusCode\":";
static const char szCharLocation[] = ",\"Char Location\":";
static const char szEndError[] = "}\r\n";

static const char szSetParmStatusCode[]  = "{\"command\":\"setParameters\",\"statusCode\":";
static const char szReadStatusCode[]     = " {\"command\":\"read\",\"statusCode\":";
static const char szGetCurrentStateStatusCode[] = " {\"command\":\"getCurrentState\",\"statusCode\":";

// Mode strings
static const char szJSON[]          = "JSON";
static const char szDebugPrint[]    = "\"debugPrint\":\"";
static const char szMode[]          = "\"mode\":\"";

// Device strings
static const char szDevice[] = "\"device\":[";

static const char szEnumeration1[] = "\
{\
\"command\":\"enumerate\",\
\"statusCode\":0,\
\"wait\":0,\
\"deviceMake\":\"Digilent\",\
\"deviceModel\":\"OpenScope MZ\",\
\"firmwareVersion\":{";

static const char szEnumeration2[] = "\
},\
\"calibrationSource\":\"";

static const char szEnumeration3[] = "\",\
\"requiredCalibrationVer\":" MKSTR(CALVER) ",\
\"requiredWiFiParameterVer\":" MKSTR(WFVER) ",\
\"awg\":{\
\"numChans\":1,\
\"1\":{\
\"signalTypes\":[\
\"sine\",\
\"square\",\
\"sawtooth\",\
\"triangle\",\
\"dc\"\
],\
\"signalFreqMin\":62,\
\"signalFreqMax\":1000000000,\
\"dataType\":\"I16\",\
\"bufferSizeMax\":32766,\
\"dacVpp\":3000,\
\"sampleFreqMin\":1000000,\
\"sampleFreqMax\":10000000000,\
\"vOffsetMin\":-1500,\
\"vOffsetMax\":1500,\
\"vOutMin\":-3000,\
\"vOutMax\":3000\
}\
},\
\"dc\":{\
\"numChans\":2,\
\"1\":{\
\"voltageMin\":-4000,\
\"voltageMax\":4000,\
\"voltageIncrement\":40,\
\"currentMin\":0,\
\"currentMax\":50,\
\"currentIncrement\":0\
},\
\"2\":{\
\"voltageMin\":-4000,\
\"voltageMax\":4000,\
\"voltageIncrement\":40,\
\"currentMin\":0,\
\"currentMax\":50,\
\"currentIncrement\":0\
}\
},\
\"gpio\":{\
\"numChans\": 10,\
\"sourceCurrentMax\": 7000,\
\"sinkCurrentMax\": 12000\
},\
\"la\":{\
\"numChans\":1,\
\"1\":{\
\"bufferDataType\":\"U16\",\
\"numDataBits\": 10,\
\"bitmask\": 1023,\
\"sampleFreqMin\": 6000,\
\"sampleFreqMax\": 6250000000,\
\"bufferSizeMax\": 32638\
}\
},\
\"osc\":{\
\"numChans\":2,\
\"1\":{\
\"resolution\":12,\
\"effectiveBits\":11,\
\"bufferSizeMax\":32638,\
\"bufferDataType\":\"I16\",\
\"sampleFreqMin\":6000,\
\"sampleFreqMax\":6250000000,\
\"delayMax\":4611686018427387904,\
\"delayMin\":-32640000000000000,\
\"adcVpp\":3000,\
\"inputVoltageMax\":20000,\
\"inputVoltageMin\":-20000,\
\"gains\":[\
1,\
0.25,\
0.125,\
0.075\
]\
},\
\"2\":{\
\"resolution\":12,\
\"effectiveBits\":11,\
\"bufferSizeMax\":32640,\
\"bufferDataType\":\"I16\",\
\"sampleFreqMin\":6000,\
\"sampleFreqMax\":6250000000,\
\"delayMax\":4611686018427387904,\
\"delayMin\":-32640000000000000,\
\"adcVpp\":3000,\
\"inputVoltageMax\":20000,\
\"inputVoltageMin\":-20000,\
\"gains\":[\
1,\
0.25,\
0.125,\
0.075\
]\
}\
}\
}";

#if 0
"\
,\"bigString\":\"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 123456789Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec massa quam, consectetur at nunc ut, convallis fringilla neque. Duis nec facilisis augue. In vestibulum nec risus at rutrum. Curabitur id tristique lacus. Morbi ultrices tellus id augue lacinia tempor. Nam arcu dui, iaculis fringilla neque pellentesque, egestas ultricies odio. Duis consequat semper dui, tincidunt vulputate elit porttitor id. Maecenas tempor condimentum blandit. Donec finibus est egestas hendrerit hendrerit. 1234567891234567891234567890\"\
"
#endif

static const char szStorageGetLocationsFlash[] = "\
{\
\"command\":\"storageGetLocations\",\
\"statusCode\":0,\
\"wait\":0,\
\"storageLocations\":[\
\"flash\"";
static const char szStorageGetLocationsSd0[] = ",\"sd0\"";

static const char szEnterBootloader[] = "\
{\
\"command\":\"enterBootloader\",\
\"statusCode\":0,\
\"wait\":1000\
}";

static const char szAveLoopTime1[] = "\
{\
\"command\":\"aveLoopTime\",\
\"uSecAveTime\":";

static const char szStatus0Wait0[] = "\
,\"statusCode\":0,\
\"wait\":0\
}";

static const char szResetInstruments[] = "\
{\
\"command\":\"resetInstruments\",\
\"statusCode\":0,\
\"wait\":1000\
}";

static const char szCalibrationGetTypesFlash[] = "\
{\
\"command\":\"calibrationGetStorageTypes\",\
\"statusCode\":0,\
\"wait\":0,\
\"storageTypes\":[\
\"flash\"";

static const char szCalibrationGetInstructions[] = "\
{\
\"command\":\"calibrationGetInstructions\",\
\"statusCode\":0,\
\"wait\":0,\
\"instructions\":\"Connect DCOUT1 to OSC1, wire the solid red wire to the solid orange wire.\\nConnect DCOUT2 to OSC2, wire the solid white wire to the solid blue wire.\\n\"\
}";

static const char szNicList[] = "\
{\
\"command\":\"nicList\",\
\"statusCode\":0,\
\"wait\":0,\
\"nics\":[\
\"wlan0\"\
]\
}";

// these are temp structures, we only need to initialize them
// to make the compiler happy about const in them. We memcpy
// in the code thus bypassing the const assignment issue.
static ITRG     triggerT    = pjcmd.trigger;
static IDC      idcT        = pjcmd.idcCh1;
static IAWG     iawgT       = pjcmd.iawg;
static IOSC     ioscT       = pjcmd.ioscCh1;
static ILA      ilaT        = pjcmd.ila;
static IWIFI    iWiFiT      = pjcmd.iWiFi;
static uint32_t iBinOffset = 0;

// token mapping
// end points
static const OSPAR::STRU32 rgStrU32Endpoint[] = {
                                                    {"device",      OSPARDeviceArray}, 
                                                    {"trigger",     OSPARTrgChannelObject}, 
                                                    {"dc",          OSPARDCChannelObject}, 
                                                    {"awg",         OSPARAwgChannelObject}, 
                                                    {"osc",         OSPAROscChannelObject}, 
                                                    {"la",          OSPARLaChannelObject}, 
                                                    {"gpio",        OSPARGpioChannelObject}, 
                                                    {"mode",        OSPARMode},
                                                    {"debugPrint",  OSPARDebugPrint},
                                                    {"test",        OSPARTestArray},
                                                };

// mode
static const OSPAR::STRU32 rgStrU32Mode[]  = {{"JSON", true}, {"menu", false}};

// mode
static const OSPAR::STRU32 rgStrU32DebugPrint[]  = {{"on", true}, {"off", false}};

// device
static const OSPAR::STRU32 rgStrU32Device[]     = {     
                                                    {"command", OSPARDeviceCmd}, 
                                                    {"type", OSPARDeviceCalibrationType}, 
                                                    {"adapter", OSPARDeviceAdapter},
                                                    {"ssid", OSPARDeviceWiFiSSID},
                                                    {"securityType", OSPARDeviceWiFiSecurityType},
                                                    {"passphrase", OSPARDeviceWiFiPassphrase},
                                                    {"autoConnect", OSPARDeviceWiFiAutoConnect},
                                                    {"parameterSet", OSPARDeviceWiFiParameterSet},
                                                    {"storageLocation", OSPARDeviceStorageLocation}, 
                                                    {"force", OSPARDeviceWiFiForce}
                                                };

static const OSPAR::STRU32 rgStrU32DeviceCmd[]  = {
                                                    {"enumerate", OSPARDeviceEnmerate}, 
                                                    {"firmware", Unimplemented}, 
                                                    {"enterBootloader", OSPARDeviceEnterBootloader},
                                                    {"aveLoopTime", OSPARDeviceAveLoopTime},
                                                    {"storageGetLocations", OSPARDeviceStorageGetLocations}, 
                                                    {"calibrationGetStorageTypes", OSPARDeviceCalibrationGetTypes}, 
                                                    {"calibrationGetInstructions", OSPARDeviceCalibrationGetInstructions}, 
                                                    {"calibrationStart", OSPARDeviceCalibrationStart}, 
                                                    {"calibrationSave", OSPARDeviceCalibrationSave}, 
                                                    {"calibrationLoad", OSPARDeviceCalibrationLoad}, 
                                                    {"calibrationRead", OSPARDeviceCalibrationRead},
                                                    {"nicList", OSPARDeviceNicList},
                                                    {"nicGetStatus", OSPARDeviceNicGetStatus},
                                                    {"nicDisconnect", OSPARDeviceNicDisconnect},
                                                    {"nicConnect", OSPARDeviceNicConnect},
                                                    {"wifiScan", OSPARDeviceWiFiScan},
                                                    {"wifiReadScannedNetworks", OSPARDeviceWiFiListScan},
                                                    {"wifiSetParameters", OSPARDeviceWiFiSetParameters},
                                                    {"wifiSaveParameters", OSPARDeviceWiFiSaveParameters},
                                                    {"wifiDeleteParameters", OSPARDeviceWiFiDeleteParameters},
                                                    {"wifiLoadParameters", OSPARDeviceWiFiLoadParameters},
                                                    {"wifiListSavedParameters", OSPARDeviceWiFiListSavedParameters},     
                                                    {"resetInstruments", OSPARDeviceResetInstruments}   
                                                };
// Device strings
static const char szDeviceDcCh1[]       = "\"dc\":{\"numChans\":2,\"1\":{\"source\":\"";

static const char szCalIdealA[]         = "\",\"Ideal A\":";
static const char szCalActualA[]        = ",\"Calibrated A\":";
static const char szCalDiffA[]          = ",\"% Diff A\":";

static const char szCalIdealB[]         = ",\"Ideal B\":";
static const char szCalActualB[]        = ",\"Calibrated B\":";
static const char szCalDiffB[]          = ",\"% Diff B\":";

static const char szCalIdealC[]         = ",\"Ideal C\":";
static const char szCalActualC[]        = ",\"Calibrated C\":";
static const char szCalDiffC[]          = ",\"% Diff C\":";

static const char szDeviceOscCh[]       = "}},\"osc\":{\"numChans\":2,";
static const char  * arszChannels[]     = {"\"1\":{", "},\"2\":{"};
static const char  szSource[]           = "\"source\":\"";
static const char szCalGain[]           = "\",\"Gains\":[{";
static const char  * arszCalGains[]     = {"\"1\":{", "},\"0.25\":{", "},\"0.125\":{", "},\"0.075\":{"};

static const char szCalOSCIdealA[]      = "\"Ideal A\":";
static const char szCalEndGains[]       = "}}]";

//static const char szDeviceDcCh2[]       = "\"},\"2\":{\"source\":\"";
static const char szDeviceDcCh2[]       = "},\"2\":{\"source\":\"";
static const char szDeviceOscCh2[]      = "},\"2\":{\"source\":\"";
static const char szDeviceAwgCh1[]      = "}},\"awg\":{\"numChans\":1,\"1\":{\"source\":\"";

// Calibration
static const char szDeviceCalRead[]     = "{\"command\":\"calibrationRead\",\"statusCode\":";
static const char szDeviceCalData[]     = ",\"calibrationData\":{";
static const char szDeviceCalDataEnd[]  = "}}}}";
static const char szDeviceCalStart[]    = "{\"command\":\"calibrationStart\",\"statusCode\":";
static const char szDeviceCalSave[]     = "{\"command\":\"calibrationSave\",\"statusCode\":";
static const char szDeviceCalLoad[]     = "{\"command\":\"calibrationLoad\",\"statusCode\":";
static const OSPAR::STRU32 rgStrU32DeviceCfg[] = {{"sd0", CFGSD}, {"flash", CFGFLASH}, {"USER", CFGSD}, {"FACTORY", CFGFLASH}};

// WiFi
static const OSPAR::STRU32 rgStrU32DeviceAdp[]  = {{"wlan0", nicWiFi0}, {"workingParameterSet", nicWorking}};
static const char szDeviceNicGetStatus[]        = "{\"command\":\"nicGetStatus\",\"statusCode\":";
static const char szDeviceNicAdp[]              = ",\"adapter\":\"";
static const char szDeviceNicSecurityType[]     = "\",\"securityType\":\"";
static const char szDeviceNicStatus[]           = ",\"status\":";
static const char szDeviceNicConnected[]        = "\"connected\",\"ssid\":\"";
static const char szDeviceNicConnect[]          = "{\"command\":\"nicConnect\",\"statusCode\":";
static const char szDeviceNicDisconnected[]     = "\"disconnected\",\"ssid\":\"";
static const char szDeviceNicIpAddress[]        = "\",\"ipAddress\":\"";
static const char szDeviceNicDisconnect[]       = "{\"command\":\"nicDisconnect\",\"statusCode\":";
static const char szDeviceWiFiScan[]            = "{\"command\":\"wifiScan\",\"statusCode\":";
static const char szDeviceWiFiListScan[]        = "{\"command\":\"wifiReadScannedNetworks\",\"statusCode\":";
static const char szDeviceWiFiAPs[]             = ",\"wait\":0,\"adapter\":\"wlan0\",\"networks\":[";
static const char szDeviceWiFiSSID[]            = "\"ssid\":\"";   
static const char szDeviceWiFiAutoConnect[]     = "\",\"autoConnect\":";   
static const char szDeviceWiFiBSSID[]           = "\",\"bssid\":\"";
static const char szDeviceWiFiSecurityType[]    = "\",\"securityType\":\"";
static const char szDeviceWiFiChannel[]         = "\",\"channel\":";             
static const char szDeviceWiFiSignalStrength[]  = ",\"signalStrength\":";  
static const char szDeviceWiFiSetParameters[]   = "{\"command\":\"wifiSetParameters\",\"statusCode\":";
static const OSPAR::STRU32 rgStrU32DeviceSecurityType[]  = {    {"open", DEWF_SECURITY_OPEN}, 
                                                                {"wep40", DEWF_SECURITY_WEP_40}, 
                                                                {"wep104", DEWF_SECURITY_WEP_104}, 
                                                                {"wpa", DEWF_SECURITY_WEP_40},
                                                                {"wpa2", DEWF_SECURITY_WPA2_WITH_KEY}
                                                            };
static const OSPAR::STRU32 rgStrU32DeviceWorkingParameterSet[]  = {{"activeParameterSet", false}, {"workingParameterSet", true}};
static const OSPAR::STRU32 rgStrU32DeviceStorageLocation[]      = {{"flash", VOLFLASH}, {"sd0", VOLSD}};
static const char szDeviceWiFiSaveParameters[]  = "{\"command\":\"wifiSaveParameters\",\"statusCode\":";
static const char szDeviceWiFiLoadParameters[]  = "{\"command\":\"wifiLoadParameters\",\"statusCode\":";
static const char szDeviceWiFiDeleteParameters[]  = "{\"command\":\"wifiDeleteParameters\",\"statusCode\":";
static const char szDeviceWiFiListSavedParameters[]  = "{\"command\":\"wifiListSavedParameters\",\"statusCode\":";
static const char szDeviceWiFiParameterSet[] = ",\"wait\":0,\"parameterSets\":[";



// DC Strings
static const char szDcObject[]          = "\"dc\":{";
static const char szDcGetStatus[]       = "{\"command\":\"getVoltage\",\"statusCode\":";
static const char szDcGetVoltage[]      = ",\"voltage\":";
static const char szDcSetStatus[]       = "{\"command\":\"setVoltage\",\"statusCode\":";

// dc
static const OSPAR::STRU32 rgStrU32DcChannel[]  = {{"1", OSPARDcCh1}, {"2", OSPARDcCh2}};
static const OSPAR::STRU32 rgStrU32Dc[]         = {{"command", OSPARDcCmd}, {"voltage", OSPARDcSetmVoltage}};
static const OSPAR::STRU32 rgStrU32DcCmd[]      = {{"getVoltage", OSPARDcGetVoltage}, {"setVoltage", OSPARDcSetVoltage}};

// AWG Strings
static const char szAwgObject[]                 = "\"awg\":{";
static const char szAwgSetRegWaveStatus[]       = "{\"command\":\"setRegularWaveform\",\"statusCode\":";
static const char szAwgRunStatus[]              = "{\"command\":\"run\",\"statusCode\":";
static const char szAwgStopStatus[]             = "{\"command\":\"stop\",\"statusCode\":";
static const char szAwgGetCurrentStateStatus[]  = "{\"command\":\"getCurrentState\",\"statusCode\":";
static const char szAwgWaveType[]               = ",\"waveType\":\"";

// awg
static const OSPAR::STRU32 rgStrU32AwgChannel[] = {{"1", OSPARAwgCh1}};
static const OSPAR::STRU32 rgStrU32Awg[] = {{"command", OSPARAwgCmd}, {"signalType", OSPARAwgSignalType}, {"signalFreq", OSPARAwgSignalFreq}, {"vpp", OSPARAwgVP2P}, {"vOffset", OSPARAwgOffset}, {"dutyCycle", OSPARAwgDutyCycle}};
static const OSPAR::STRU32 rgStrU32AwgCmd[] = {{"setRegularWaveform", OSPARAwgSetRegularWaveform}, {"getCurrentState", OSPARAwgGetCurrentState}, {"run", OSPARAwgRun}, {"stop", OSPARAwgStop}};
static const OSPAR::STRU32 rgStrU32AwgSignalType[] = {{"sine", waveSine}, {"square", waveSquare}, {"sawtooth", waveSawtooth}, {"triangle", waveTriangle}, {"dc", waveDC}, {"arbitrary", waveArbitrary}};
char const * const rgszAwgWaveforms[] = {"none", "dc", "sine", "square", "triangle", "sawtooth", "arbitrary"};

// OSC Strings
static const char szOscObject[]             = "\"osc\":{";

// osc
static const OSPAR::STRU32 rgStrU32OscChannel[] = {{"1", OSPAROscCh1}, {"2", OSPAROscCh2}};
static const OSPAR::STRU32 rgStrU32Osc[] = {{"command", OSPAROscCmd}, {"offset", OSPAROscSetOffset}, {"vOffset", OSPAROscSetOffset}, {"gain", OSPAROscSetGain}, {"sampleFreq", OSPAROscSetSampleFreq}, {"bufferSize", OSPAROscSetBufferSize}, {"triggerDelay", OSPAROscSetTrigDelay}, {"acqCount", OSPAROscSetAcqCount}};
static const OSPAR::STRU32 rgStrU32OscCmd[] = {{"setParameters", OSPAROscSetParm}, {"read", OSPAROscRead}, {"getCurrentState", OSPAROscGetCurrentState}};

/// TRG Strings
static const char szTrgObject[]             = "\"trigger\":{";
static const char szTrgRunStatus[]          = "{\"command\":\"run\",\"statusCode\":";
static const char szTrgSingleStatus[]       = "{\"command\":\"single\",\"statusCode\":";
static const char szTrgStop[]               = "{\"command\":\"stop\",\"statusCode\":0,\"wait\":0}";
static const char szTrgForceTrigger[]       = "{\"command\":\"forceTrigger\",\"statusCode\":";
static const char szTrgGetCurrentStateStatus[] = "{\"command\":\"getCurrentState\",\"statusCode\":";
static const char szTrgSource[]             = ",\"source\":{";
static const char szTrgTargets[]            = ",\"targets\":{";

// Logic Analyzer
static const OSPAR::STRU32 rgStrU32LaChannel[]  = {{"1", OSPARLaCh1}};
static const OSPAR::STRU32 rgStrU32La[]         = {{"command", OSPARLaCmd}, {"sampleFreq", OSPARLaSetSampleFreq}, {"bufferSize", OSPARLaSetBufferSize}, {"triggerDelay", OSPARLaSetTrigDelay}, {"bitmask", OSPARLaBitMask}, {"acqCount", OSPARLaSetAcqCount}};
static const OSPAR::STRU32 rgStrU32LaCmd[]      = {{"setParameters", OSPARLaSetParm}, {"read", OSPARLaRead}, {"getCurrentState", OSPARLaGetCurrentState}, {"run", OSPARLaRun}, {"stop", OSPARLaStop}};
static const char szLaObject[]                  = "\"la\":{";
static const char szLaRun[]                     = " {\"command\":\"run\",\"statusCode\":";
static const char szLaStop[]                    = " {\"command\":\"stop\",\"statusCode\":";

// trigger
static const OSPAR::STRU32 rgStrU32TrgChannel[] = {{"1", OSPARTrgCh1}};
static const OSPAR::STRU32 rgStrU32Trg[] = {{"command", OSPARTrgCmd}, {"source", OSPARTrgSource}, {"targets", OSPARTrgTargets}};
static const OSPAR::STRU32 rgStrU32TrgCmd[] = {{"setParameters", OSPARTrgSetParm}, {"run", OSPARTrgRun}, {"single", OSPARTrgSingle}, {"forceTrigger", OSPARTrgForceTrigger}, {"stop", OSPARTrgStop}, {"getCurrentState", OSPARTrgGetCurrentState}};
static const OSPAR::STRU32 rgStrU32TrgSrc[] = {{"instrument", OSPARTrgInstrument}, {"channel", OSPARTrgInstrumentChannel}, {"type", OSPARTrgType}, {"lowerThreshold", OSPARTrgLowerThreashold}, {"upperThreshold", OSPARTrgUpperThreashold}, {"risingEdge", OSPARTrgRisingEdge}, {"fallingEdge", OSPARTrgFallingEdge}};

static const OSPAR::STRU32 rgStrU32TrgTargets[] = {{"osc", OSPARTrgTargetOsc}, {"la", OSPARTrgTargetLa}};
static const OSPAR::STRU32 rgStrU32TrgInstrumentID[] = {{"osc", OSC1_ID}, {"la", LOGIC1_ID}, {"awg", AWG1_ID}, {"external", EXT_TRG_ID}, {"force", FORCE_TRG_ID}};
static const OSPAR::STRU32 rgStrU32TrgType[] = {{"risingEdge", TRGTPRising}, {"fallingEdge", TRGTPFalling}};
static const uint32_t triggerSrcID[] = {OSC1_ID, OSC2_ID, LOGIC1_ID, NULL_ID, AWG1_ID, NULL_ID, NULL_ID, NULL_ID};

static char const * const rgszGains[]       = {"0.0", "1", "0.25", "0.125", "0.075"};

// GPIO Strings
static const char szGpioObject[]            = "\"gpio\":{";
static const char szGpioReadStatusCode[]    = " {\"command\":\"read\",\"statusCode\":";
static const char szGpioWriteStatusCode[]   = " {\"command\":\"write\",\"statusCode\":";
static const char szGpioDirection[]         = ",\"direction\":";
static const char szGpioValue0[]            = ",\"value\":0";
static const char szGpioValue1[]            = ",\"value\":1";

// GPIO commands
static const OSPAR::STRU32 rgStrU32GpioChannel[]    = {{"1", OSPARGpioCh1}, {"2", OSPARGpioCh2}, {"3", OSPARGpioCh3}, {"4", OSPARGpioCh4}, {"5", OSPARGpioCh5}, {"6", OSPARGpioCh6}, {"7", OSPARGpioCh7}, {"8", OSPARGpioCh8}, {"9", OSPARGpioCh9}, {"10", OSPARGpioCh10}};
static const OSPAR::STRU32 rgStrU32Gpio[]           = {{"command", OSPARGpioCmd}, {"direction", OSPARGpioDirection}, {"value", OSPARGpioValue}};
static const OSPAR::STRU32 rgStrU32GpioCmd[]        = {{"setParameters", OSPARGpioSetParameters}, {"read", OSPARGpioRead}, {"write", OSPARGpioWrite}};
static const OSPAR::STRU32 rgStrU32GpioDirection[]  = {{"tristate", (uint32_t) gpioTriState}, {"input", (uint32_t) gpioInput}, {"output", (uint32_t) gpioOutput}, {"inputPullUp", (uint32_t) gpioInputPullUp}, {"inputPullDown", (uint32_t) gpioInputPullDown}};
static char const * const  rgszGpioDirections[]     = {"\"none\"", "\"tristate\"", "\"input\"", "\"inputPullDown\"", "\"inputPullUp\"", "\"output\""};

// Undocumented Manufacturing Test commands
static const OSPAR::STRU32 rgStrU32Test[]           = {{"command", OSPARTestCmd}, {"testNbr", OSPARTestNbr}};
static const OSPAR::STRU32 rgStrU32TestCmd[]        = {{"run", OSPARTestRun}};
static const char szTest[]            = "\"test\":[";
static const char szTestRun[]         = "{\"command\":\"run\",\"statusCode\":0,\"wait\":0,\"returnNbr\":";

#ifndef JUST_LEX_JSON

uint32_t OSPAR::Uint32FromStr(STRU32 const * const rgStrU32L, uint32_t cStrU32L, char const * const sz, uint32_t cb, STATE defaultState)
{
    uint32_t i = 0;

    for(i = 0; i < cStrU32L; i++)
    {
        if((strlen(rgStrU32L[i].szToken) == cb) && memcmp(sz, rgStrU32L[i].szToken, cb) == 0)
        {
            return(rgStrU32L[i].u32);
        }
    }

    return(defaultState);
}

GCMD::ACTION OSPAR::StreamJSON(char const * szStream, uint32_t cbStream)
{
    // if we are not given any data, this is a streaming error
    if(cbStream == 0)
    {
        // Error Code
        strcpy(rgchOut, szStatusCode);
        odata[0].cb = sizeof(szStatusCode)-1;
        utoa(EndOfStream, &rgchOut[odata[0].cb], 10);
        odata[0].cb = strlen(rgchOut);

        // location
        memcpy(&rgchOut[odata[0].cb], szCharLocation, sizeof(szCharLocation)-1);
        odata[0].cb += sizeof(szCharLocation)-1;
        itoa(cbProcessed + iBuf, &rgchOut[odata[0].cb], 10);
        odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                
        // end the error code
        memcpy(&rgchOut[odata[0].cb], szEndError, sizeof(szEndError)-1);
        odata[0].cb += sizeof(szEndError)-1;

        return(GCMD::ERROR);
    }

    // make sure we have stuff in our local buffer
    else if(iBufMax < sizeof(szBuf) && (cbStream - iStream) > 0)
    {
        uint32_t cbAdd = min((cbStream - iStream), sizeof(szBuf) - iBufMax);
        memcpy(&szBuf[iBufMax], &szStream[iStream], cbAdd);
        iBufMax += cbAdd;
        iStream += cbAdd;
    }

    // call the lexer
    switch(LexJSON(&szBuf[iBuf], iBufMax - iBuf))
    {
        case GCMD::READ:
            
            // move things to skip
            iBuf += szMoveInput - &szBuf[iBuf];

            // shift to the begining of the buffer
            memcpy(szBuf, &szBuf[iBuf], iBufMax -  iBuf);
            iBufMax -= iBuf;
            cbProcessed += iBuf;
            iBuf = 0;

            // we still have some bytes in the buffer
            if((cbStream - iStream) > 0) return(GCMD::CONTINUE);

            // we need more bytes
            iStream = 0;
            return(GCMD::READ);
            break;

        case GCMD::ERROR:

            // Error Code
            strcpy(rgchOut, szStatusCode);
            odata[0].cb = sizeof(szStatusCode)-1;
            utoa(tokenErrorState, &rgchOut[odata[0].cb], 10);
            odata[0].cb = strlen(rgchOut);

            // location
            memcpy(&rgchOut[odata[0].cb], szCharLocation, sizeof(szCharLocation)-1);
            odata[0].cb += sizeof(szCharLocation)-1;
            itoa(cbProcessed + iBuf, &rgchOut[odata[0].cb], 10);
            odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                
            // end the error code
            memcpy(&rgchOut[odata[0].cb], szEndError, sizeof(szEndError)-1);
            odata[0].cb += sizeof(szEndError)-1;
    
            return(GCMD::ERROR);
            break;

        case GCMD::DONE:
            return(GCMD::DONE);
            break;

        default:
            iBuf += szMoveInput - &szBuf[iBuf];
            break;

    }

    return(GCMD::CONTINUE);
}

STATE OSPAR::ParseToken(char const * szToken, uint32_t cbToken, JSONTOKEN jsonToken)
{
    STATE   curState = state;
    bool    fContinue = false;

    switch(jsonToken)
    {
        case tokJSONSyntaxError:
            curState = OSPARSyntaxError;
            break;

        case tokEndOfJSON:
            if(curState == Idle) return(Idle);
            else if(curState != OSPAREnd) curState = OSPARSyntaxError;
            break;

        // on strings, take off the quotes
        case tokMemberName:
        case tokStringValue:
            szToken++;
            cbToken -= 2;
            break;

        default:
            if(curState == OSPAREnd) curState = OSPARSyntaxError;
            break;
    }

    do {
        fContinue = false;
        state = OSPARSyntaxError;

        switch(curState)
        {

            /************************************************************************/
            /*    Generic LEXing                                                    */
            /************************************************************************/
            case Idle:
                memset(&triggerT, 0, sizeof(triggerT));
                memset(&idcT, 0, sizeof(idcT));
                memset(&ioscT, 0, sizeof(ioscT));
                iBinOffset = 0;

                if(jsonToken == tokObject)
                {
                    odata[0].cb = sizeof(szStartObject)-1;
                    memcpy(rgchOut, szStartObject, odata[0].cb);  
                    rgStrU32 = rgStrU32Endpoint;
                    cStrU32 = sizeof(rgStrU32Endpoint) / sizeof(STRU32);
                    state = OSPARMemberName;
                }
                break;

            case OSPARSkipObject:
                if(jsonToken == tokObject)
                {
                    state = OSPARMemberName;
                }
                break;

            case OSPARMemberName:
                if(jsonToken == tokMemberName)
                {
                    stateNameSep = (STATE) Uint32FromStr(rgStrU32, cStrU32, szToken, cbToken);
                    state = OSPARSkipNameSep;
                }
                break;

            case OSPARSkipNameSep:
                if(jsonToken == tokNameSep)
                {
                    state = stateNameSep;
                }
                break;

            case OSPARSkipArray:
                if(jsonToken == tokArray)
                {
                    state = stateArray;
                }
                break;

            case OSPARSkipValueSep:
                if(jsonToken == tokValueSep)
                {
                    state = stateValueSep;
                }
                else if(jsonToken == tokEndObject)
                {
                    state = stateEndObject;
                    fContinue = true;
                }
                else if(jsonToken == tokEndArray)
                {
                    state = stateEndArray;
                    fContinue = true;
                }
                break;

            case OSPARSeparatedObject:
                if(jsonToken == tokObject)
                {
                    memcpy(&rgchOut[odata[0].cb], szValueSep, sizeof(szValueSep)-1); 
                    odata[0].cb += sizeof(szValueSep)-1;
                    state = OSPARMemberName;
                }
                break;

            case OSPARSeparatedNameValue:
                if(jsonToken == tokMemberName)
                {
                    memcpy(&rgchOut[odata[0].cb], szValueSep, sizeof(szValueSep)-1); 
                    odata[0].cb += sizeof(szValueSep)-1;
                    state = OSPARMemberName;
                    fContinue = true;
                }
                break;

            // if we don't expect an end array or object yet
            case OSPARErrArrayEnd:
            case OSPARErrObjectEnd:
                state = OSPARSyntaxError;
                break;

            case OSPAREnd:
                memcpy(&rgchOut[odata[0].cb], szEndObject, sizeof(szEndObject)-1);                 
                odata[0].cb += sizeof(szEndObject)-1;

                memcpy(&rgchOut[odata[0].cb], szNewLine, sizeof(szNewLine)-1);                 
                odata[0].cb += sizeof(szNewLine)-1;

                state = Idle;
                break;

            case OSPARTopObjEnd:
                if(jsonToken == tokEndObject)
                {      
                    memcpy(&rgchOut[odata[0].cb], szEndObject, sizeof(szEndObject)-1); 
                    odata[0].cb += sizeof(szEndObject)-1;
                    stateEndObject = OSPARLoadEndPoint;
                    stateValueSep = OSPARLoadEndPoint;
                    state = OSPARSkipValueSep;
                }
                break;

            default:
            case OSPARSyntaxError:

                // return the error
                return(OSInvalidSyntax);
                break;

            /************************************************************************/
            /*    endpoint parsing                                                  */
            /************************************************************************/
            case OSPARLoadEndPoint:
                rgStrU32 = rgStrU32Endpoint;
                cStrU32 = sizeof(rgStrU32Endpoint) / sizeof(STRU32);
                stateEndObject = OSPAREnd;
                if(jsonToken == tokMemberName)
                {
                    state = OSPARSeparatedNameValue;
                    fContinue = true;
                }
                else if(jsonToken == tokEndObject) 
                {
                    state = OSPAREnd;
                }
                break;


            /************************************************************************/
            /*    Mode parsing                                                      */
            /************************************************************************/        
            case OSPARMode:
                if(jsonToken == tokStringValue)
                {
                    uint32_t mode = Uint32FromStr(rgStrU32Mode, sizeof(rgStrU32Mode) / sizeof(STRU32), szToken, cbToken, InvalidCommand);

                    if(mode != InvalidCommand)
                    {
                        fModeJSON = mode;

                        Serial.EnablePrint(!fModeJSON);

                        // print out mode
                        memcpy(&rgchOut[odata[0].cb], szMode, sizeof(szMode)-1); 
                        odata[0].cb += sizeof(szMode)-1;

                        memcpy(&rgchOut[odata[0].cb], szToken, cbToken); 
                        odata[0].cb += cbToken;

                        rgchOut[odata[0].cb++] = '\"';
                    
                        memcpy(&rgchOut[odata[0].cb], szStatusCode0, sizeof(szStatusCode0)-1); 
                        odata[0].cb += sizeof(szStatusCode0)-1;

                        if(mode)
                        {
                            memcpy(&rgchOut[odata[0].cb], szWait500, sizeof(szWait500)-2); 
                            odata[0].cb += sizeof(szWait500)-2;
                        }
                        else
                        {
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-2); 
                            odata[0].cb += sizeof(szWait0)-2;
                        }

                        // return to endpoing
                        stateEndObject = OSPARLoadEndPoint;
                        stateValueSep = OSPARLoadEndPoint;
                        state = OSPARSkipValueSep;
                    }
                }
                break;

            case OSPARDebugPrint:
                if(jsonToken == tokStringValue)
                {
                    uint32_t mode = Uint32FromStr(rgStrU32DebugPrint, sizeof(rgStrU32DebugPrint) / sizeof(STRU32), szToken, cbToken, InvalidCommand);

                    if(mode != InvalidCommand)
                    {
                        Serial.EnablePrint(mode);

                        // print out mode
                        memcpy(&rgchOut[odata[0].cb], szDebugPrint, sizeof(szDebugPrint)-1); 
                        odata[0].cb += sizeof(szDebugPrint)-1;

                        memcpy(&rgchOut[odata[0].cb], szToken, cbToken); 
                        odata[0].cb += cbToken;

                        rgchOut[odata[0].cb++] = '\"';
                    
                        memcpy(&rgchOut[odata[0].cb], szStatusCode0, sizeof(szStatusCode0)-1); 
                        odata[0].cb += sizeof(szStatusCode0)-1;

                        if(mode)
                        {
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-2); 
                            odata[0].cb += sizeof(szWait0)-2;
                        }
                        else
                        {
                            memcpy(&rgchOut[odata[0].cb], szWait500, sizeof(szWait500)-2); 
                            odata[0].cb += sizeof(szWait500)-2;
                        }

                        // return to endpoing
                        stateEndObject = OSPARLoadEndPoint;
                        stateValueSep = OSPARLoadEndPoint;
                        state = OSPARSkipValueSep;
                    }
                }
                break;

            /************************************************************************/
            /*    Device parsing                                                    */
            /************************************************************************/        
            case OSPARDeviceArray:
                if(jsonToken == tokArray)
                {
                    memcpy(&iWiFiT, &pjcmd.iWiFi, sizeof(IWIFI));   // get what this is; we may not use it though

                    rgStrU32 = rgStrU32Device;
                    cStrU32 = sizeof(rgStrU32Device) / sizeof(STRU32);

                    memcpy(&rgchOut[odata[0].cb], szDevice, sizeof(szDevice)-1); 
                    odata[0].cb += sizeof(szDevice)-1;
                    
                    state = OSPARSkipObject;
                }
                break;  

            case OSPARDeviceCmd:
                if(jsonToken == tokStringValue)
                {
                    state   = (STATE) Uint32FromStr(rgStrU32DeviceCmd, sizeof(rgStrU32DeviceCmd) / sizeof(STRU32), szToken, cbToken);
                    fContinue = true;
                }
                break;

            case OSPARDeviceResetInstruments:
                if(jsonToken == tokStringValue)
                {
                    memcpy(&rgchOut[odata[0].cb], szResetInstruments, sizeof(szResetInstruments)-1); 
                    odata[0].cb += sizeof(szResetInstruments)-1;

                    ResetInstruments();

                    // get next member name
                    stateEndObject = OSPARDeviceEndObject;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceEnmerate:
                if(jsonToken == tokStringValue)
                {
                    memcpy(&rgchOut[odata[0].cb], szEnumeration1, sizeof(szEnumeration1)-1); 
                    odata[0].cb += sizeof(szEnumeration1)-1;

                    // put version number in
                    strcpy(&rgchOut[odata[0].cb], szEnumVersion);
                    odata[0].cb += strlen(szEnumVersion);

                    memcpy(&rgchOut[odata[0].cb], szEnumeration2, sizeof(szEnumeration2)-1); 
                    odata[0].cb += sizeof(szEnumeration2)-1;

                    // put in the calibration source  
                    strcpy(&rgchOut[odata[0].cb], rgCFGNames[((IDHDR *) rgInstr[OSC2_ID])->cfg]);
                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                    memcpy(&rgchOut[odata[0].cb], szEnumeration3, sizeof(szEnumeration3)-1); 
                    odata[0].cb += sizeof(szEnumeration3)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndObject;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceEnterBootloader:
                if(jsonToken == tokStringValue)
                {
                    memcpy(&rgchOut[odata[0].cb], szEnterBootloader, sizeof(szEnterBootloader)-1); 
                    odata[0].cb += sizeof(szEnterBootloader)-1;

                    pjcmd.iBoot.processing = Queued;

                    // get next member name
                    stateEndObject = OSPARDeviceEndObject;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceAveLoopTime:
                if(jsonToken == tokStringValue)
                {
                    memcpy(&rgchOut[odata[0].cb], szAveLoopTime1, sizeof(szAveLoopTime1)-1); 
                    odata[0].cb += sizeof(szAveLoopTime1)-1;

                    itoa((aveLoopTime / CORE_TMR_TICKS_PER_USEC), &rgchOut[odata[0].cb], 10);
                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                    memcpy(&rgchOut[odata[0].cb], szStatus0Wait0, sizeof(szStatus0Wait0)-1); 
                    odata[0].cb += sizeof(szStatus0Wait0)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndObject;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceEndObject:
                if(jsonToken == tokEndObject)
                {
                    stateEndArray = OSPARDeviceEndArray;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceEndArray:
                if(jsonToken == tokEndArray)
                {
                    memcpy(&rgchOut[odata[0].cb], szEndArray, sizeof(szEndArray)-1); 
                    odata[0].cb += sizeof(szEndArray)-1;
                    stateEndObject = OSPARLoadEndPoint;
                    stateValueSep = OSPARLoadEndPoint;
                    state = OSPARSkipValueSep;
                }
                break;

            /************************************************************************/
            /*    Calibration                                                       */
            /************************************************************************/
            case OSPARDeviceCalibrationGetTypes:
                if(jsonToken == tokStringValue)
                {
                    memcpy(&rgchOut[odata[0].cb], szCalibrationGetTypesFlash, sizeof(szCalibrationGetTypesFlash)-1); 
                    odata[0].cb += sizeof(szCalibrationGetTypesFlash)-1;

                    if(DFATFS::fsvolmounted(DFATFS::szFatFsVols[VOLSD]))
                    {
                        memcpy(&rgchOut[odata[0].cb], szStorageGetLocationsSd0, sizeof(szStorageGetLocationsSd0)-1); 
                        odata[0].cb += sizeof(szStorageGetLocationsSd0)-1;
                    }

                    memcpy(&rgchOut[odata[0].cb], szDeviceEnd, sizeof(szDeviceEnd)-1); 
                    odata[0].cb += sizeof(szDeviceEnd)-1;

                    // just go to Device end because this a simple string
                    // get next member name
                    stateEndObject = OSPARDeviceEndObject;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceStorageGetLocations:
                if(jsonToken == tokStringValue)
                {
                    memcpy(&rgchOut[odata[0].cb], szStorageGetLocationsFlash, sizeof(szStorageGetLocationsFlash)-1); 
                    odata[0].cb += sizeof(szStorageGetLocationsFlash)-1;

                    if(DFATFS::fsvolmounted(DFATFS::szFatFsVols[VOLSD]))
                    {
                        memcpy(&rgchOut[odata[0].cb], szStorageGetLocationsSd0, sizeof(szStorageGetLocationsSd0)-1); 
                        odata[0].cb += sizeof(szStorageGetLocationsSd0)-1;
                    }

                    memcpy(&rgchOut[odata[0].cb], szDeviceEnd, sizeof(szDeviceEnd)-1); 
                    odata[0].cb += sizeof(szDeviceEnd)-1;

                    // just go to Device end because this a simple string
                    // get next member name
                    stateEndObject = OSPARDeviceEndObject;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceCalibrationGetInstructions:
                if(jsonToken == tokStringValue)
                {
                    memcpy(&rgchOut[odata[0].cb], szCalibrationGetInstructions, sizeof(szCalibrationGetInstructions)-1); 
                    odata[0].cb += sizeof(szCalibrationGetInstructions)-1;

                    // just go to Device end because this a simple string
                    // get next member name
                    stateEndObject = OSPARDeviceEndObject;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceCalibrationType:
                if(jsonToken == tokStringValue)
                {
                    pjcmd.iCal.config = (CFGNAME) Uint32FromStr(rgStrU32DeviceCfg, sizeof(rgStrU32DeviceCfg) / sizeof(STRU32), szToken, cbToken, (STATE) CFGNONE);
                    state = OSPARSkipValueSep;

                    // the end states are all set by the command item which must exist for proper syntax.
                }
                break;

            case OSPARDeviceCalibrationStart:
                if(jsonToken == tokStringValue)
                {
                    pjcmd.iCal.state.parsing = JSPARCalibrationStart;

                    memcpy(&rgchOut[odata[0].cb], szDeviceCalStart, sizeof(szDeviceCalStart)-1); 
                    odata[0].cb += sizeof(szDeviceCalStart)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndCalibration;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceCalibrationRead:
                if(jsonToken == tokStringValue)
                {
                    pjcmd.iCal.state.parsing = JSPARCalibratingRead;

                    memcpy(&rgchOut[odata[0].cb], szDeviceCalRead, sizeof(szDeviceCalRead)-1); 
                    odata[0].cb += sizeof(szDeviceCalRead)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndCalibration;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceCalibrationLoad:
                if(jsonToken == tokStringValue)
                {
                    pjcmd.iCal.state.parsing = JSPARCalibrationLoad;

                    // put out the save commmand
                    memcpy(&rgchOut[odata[0].cb], szDeviceCalLoad, sizeof(szDeviceCalLoad)-1); 
                    odata[0].cb += sizeof(szDeviceCalLoad)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndCalibration;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceCalibrationSave:
                if(jsonToken == tokStringValue)
                {
                    pjcmd.iCal.state.parsing = JSPARCalibrationSave;

                    // put out the save commmand
                    memcpy(&rgchOut[odata[0].cb], szDeviceCalSave, sizeof(szDeviceCalSave)-1); 
                    odata[0].cb += sizeof(szDeviceCalSave)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndCalibration;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceEndCalibration:
                if(jsonToken == tokEndObject)
                {
                    switch(pjcmd.iCal.state.parsing)
                    {
                        case JSPARCalibratingRead:

                            if(pjcmd.iCal.state.processing == NotCfgForCalibration)
                            {
                                // Put out the error status
                                utoa(NotCfgForCalibration, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }
                            else if(pjcmd.iCal.state.processing == JSPARCalibrationStart)
                            {
                                // Put out the calibrating status; not an error
                                utoa(Calibrating, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }

                            else if(pjcmd.iCal.state.processing != Idle)
                            {
                                // We are doing something
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }

                            // we can read the calibration info
                            else
                            {
								uint32_t id	= NULL_ID;
								uint32_t ig	= 0;
								uint32_t cb	= 0;
                                DCVOLT dcT  = DCVOLT(DCVOLT1_ID);
                                AWG awgT    = AWG(AWG1_ID);
                                OSC oscT    = OSC(OSC1_ID);

                                // status code
                                rgchOut[odata[0].cb++] = '0';

                                // wait 0; but no closing brace.... -2
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-2); 
                                odata[0].cb += sizeof(szWait0)-2;

                                // calibrationData
                                memcpy(&rgchOut[odata[0].cb], szDeviceCalData, sizeof(szDeviceCalData)-1); 
                                odata[0].cb += sizeof(szDeviceCalData)-1;

                                // dc channel 1
                                memcpy(&rgchOut[odata[0].cb], szDeviceDcCh1, sizeof(szDeviceDcCh1)-1); 
                                odata[0].cb += sizeof(szDeviceDcCh1)-1;
                                strcpy(&rgchOut[odata[0].cb], rgCFGNames[((IDHDR *) rgInstr[DCVOLT1_ID])->cfg]); 
                                odata[0].cb += strlen(rgCFGNames[((IDHDR *) rgInstr[DCVOLT1_ID])->cfg]);

                                memcpy(&rgchOut[odata[0].cb], szCalIdealA, sizeof(szCalIdealA)-1); 
                                odata[0].cb += sizeof(szCalIdealA)-1;
                                itoa(dcT.A, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalActualA, sizeof(szCalActualA)-1); 
                                odata[0].cb += sizeof(szCalActualA)-1;
                                itoa(((DCVOLT *) rgInstr[DCVOLT1_ID])->A, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalDiffA, sizeof(szCalDiffA)-1); 
                                odata[0].cb += sizeof(szCalDiffA)-1;
                                GetPercent(dcT.A - ((DCVOLT *) rgInstr[DCVOLT1_ID])->A, dcT.A, 4, &rgchOut[odata[0].cb], 8);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalIdealB, sizeof(szCalIdealB)-1); 
                                odata[0].cb += sizeof(szCalIdealB)-1;
                                itoa(dcT.B, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalActualB, sizeof(szCalActualB)-1); 
                                odata[0].cb += sizeof(szCalActualB)-1;
                                itoa(((DCVOLT *) rgInstr[DCVOLT1_ID])->B, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalDiffB, sizeof(szCalDiffB)-1); 
                                odata[0].cb += sizeof(szCalDiffB)-1;
                                GetPercent(dcT.B - ((DCVOLT *) rgInstr[DCVOLT1_ID])->B, dcT.B, 4, &rgchOut[odata[0].cb], 8);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // dc channel 2
                                memcpy(&rgchOut[odata[0].cb], szDeviceDcCh2, sizeof(szDeviceDcCh2)-1); 
                                odata[0].cb += sizeof(szDeviceDcCh2)-1;
                                strcpy(&rgchOut[odata[0].cb], rgCFGNames[((IDHDR *) rgInstr[DCVOLT2_ID])->cfg]); 
                                odata[0].cb += strlen(rgCFGNames[((IDHDR *) rgInstr[DCVOLT2_ID])->cfg]);

                                memcpy(&rgchOut[odata[0].cb], szCalIdealA, sizeof(szCalIdealA)-1); 
                                odata[0].cb += sizeof(szCalIdealA)-1;
                                itoa(dcT.A, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalActualA, sizeof(szCalActualA)-1); 
                                odata[0].cb += sizeof(szCalActualA)-1;
                                itoa(((DCVOLT *) rgInstr[DCVOLT2_ID])->A, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalDiffA, sizeof(szCalDiffA)-1); 
                                odata[0].cb += sizeof(szCalDiffA)-1;
                                GetPercent(dcT.A - ((DCVOLT *) rgInstr[DCVOLT2_ID])->A, dcT.A, 4, &rgchOut[odata[0].cb], 8);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalIdealB, sizeof(szCalIdealB)-1); 
                                odata[0].cb += sizeof(szCalIdealB)-1;
                                itoa(dcT.B, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalActualB, sizeof(szCalActualB)-1); 
                                odata[0].cb += sizeof(szCalActualB)-1;
                                itoa(((DCVOLT *) rgInstr[DCVOLT2_ID])->B, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalDiffB, sizeof(szCalDiffB)-1); 
                                odata[0].cb += sizeof(szCalDiffB)-1;
                                GetPercent(dcT.B - ((DCVOLT *) rgInstr[DCVOLT2_ID])->B, dcT.B, 4, &rgchOut[odata[0].cb], 8);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

								// osc channels
								memcpy(&rgchOut[odata[0].cb], szDeviceOscCh, sizeof(szDeviceOscCh)-1); 
								odata[0].cb += sizeof(szDeviceOscCh)-1;

								for(id = OSC1_ID; id <= OSC2_ID; id += 2)
								{
									memcpy(&rgchOut[odata[0].cb], arszChannels[(id-OSC1_ID)/2], (cb = strlen(arszChannels[(id-OSC1_ID)/2]))); 
									odata[0].cb += cb;

									memcpy(&rgchOut[odata[0].cb], szSource, sizeof(szSource)-1); 
									odata[0].cb += sizeof(szSource)-1;
									strcpy(&rgchOut[odata[0].cb], rgCFGNames[((IDHDR *) rgInstr[id])->cfg]); 
									odata[0].cb += strlen(rgCFGNames[((IDHDR *) rgInstr[id])->cfg]);

									memcpy(&rgchOut[odata[0].cb], szCalGain, sizeof(szCalGain)-1); 
									odata[0].cb += sizeof(szCalGain)-1;

									for(ig = 0; ig < 4; ig++)
									{
										memcpy(&rgchOut[odata[0].cb], arszCalGains[ig], (cb = strlen(arszCalGains[ig]))); 
										odata[0].cb += cb;

										memcpy(&rgchOut[odata[0].cb], szCalOSCIdealA, sizeof(szCalOSCIdealA)-1); 
										odata[0].cb += sizeof(szCalOSCIdealA)-1;
										itoa(oscT.rgGCal[ig].A, &rgchOut[odata[0].cb], 10);
										odata[0].cb += strlen(&rgchOut[odata[0].cb]);

										memcpy(&rgchOut[odata[0].cb], szCalActualA, sizeof(szCalActualA)-1); 
										odata[0].cb += sizeof(szCalActualA)-1;
										itoa(((OSC *) rgInstr[id])->rgGCal[ig].A, &rgchOut[odata[0].cb], 10);
										odata[0].cb += strlen(&rgchOut[odata[0].cb]);

										memcpy(&rgchOut[odata[0].cb], szCalDiffA, sizeof(szCalDiffA)-1); 
										odata[0].cb += sizeof(szCalDiffA)-1;
										GetPercent(oscT.rgGCal[ig].A - ((OSC *) rgInstr[id])->rgGCal[ig].A,oscT.rgGCal[ig].A, 4, &rgchOut[odata[0].cb], 8);
										odata[0].cb += strlen(&rgchOut[odata[0].cb]);

										memcpy(&rgchOut[odata[0].cb], szCalIdealB, sizeof(szCalIdealB)-1); 
										odata[0].cb += sizeof(szCalIdealB)-1;
										itoa(oscT.rgGCal[ig].B, &rgchOut[odata[0].cb], 10);
										odata[0].cb += strlen(&rgchOut[odata[0].cb]);

										memcpy(&rgchOut[odata[0].cb], szCalActualB, sizeof(szCalActualB)-1); 
										odata[0].cb += sizeof(szCalActualB)-1;
										itoa(((OSC *) rgInstr[id])->rgGCal[ig].B, &rgchOut[odata[0].cb], 10);
										odata[0].cb += strlen(&rgchOut[odata[0].cb]);

										memcpy(&rgchOut[odata[0].cb], szCalDiffB, sizeof(szCalDiffB)-1); 
										odata[0].cb += sizeof(szCalDiffB)-1;
										GetPercent(oscT.rgGCal[ig].B - ((OSC *) rgInstr[id])->rgGCal[ig].B, oscT.rgGCal[ig].B, 4, &rgchOut[odata[0].cb], 8);
										odata[0].cb += strlen(&rgchOut[odata[0].cb]);

										memcpy(&rgchOut[odata[0].cb], szCalIdealC, sizeof(szCalIdealC)-1); 
										odata[0].cb += sizeof(szCalIdealC)-1;
										itoa(oscT.rgGCal[ig].C, &rgchOut[odata[0].cb], 10);
										odata[0].cb += strlen(&rgchOut[odata[0].cb]);

										memcpy(&rgchOut[odata[0].cb], szCalActualC, sizeof(szCalActualC)-1); 
										odata[0].cb += sizeof(szCalActualC)-1;
										itoa(((OSC *) rgInstr[id])->rgGCal[ig].C, &rgchOut[odata[0].cb], 10);
										odata[0].cb += strlen(&rgchOut[odata[0].cb]);

										memcpy(&rgchOut[odata[0].cb], szCalDiffC, sizeof(szCalDiffC)-1); 
										odata[0].cb += sizeof(szCalDiffC)-1;
										GetPercent(oscT.rgGCal[ig].C - ((OSC *) rgInstr[id])->rgGCal[ig].C, oscT.rgGCal[ig].C, 4, &rgchOut[odata[0].cb], 8);
										odata[0].cb += strlen(&rgchOut[odata[0].cb]);
									}

									memcpy(&rgchOut[odata[0].cb], szCalEndGains, sizeof(szCalEndGains)-1); 
									odata[0].cb += sizeof(szCalEndGains)-1;
								}

                                // awg channel 1
                                memcpy(&rgchOut[odata[0].cb], szDeviceAwgCh1, sizeof(szDeviceAwgCh1)-1); 
                                odata[0].cb += sizeof(szDeviceAwgCh1)-1;
                                strcpy(&rgchOut[odata[0].cb], rgCFGNames[((IDHDR *) rgInstr[AWG1_ID])->cfg]); 
                                odata[0].cb += strlen(rgCFGNames[((IDHDR *) rgInstr[AWG1_ID])->cfg]);

                                memcpy(&rgchOut[odata[0].cb], szCalIdealA, sizeof(szCalIdealA)-1); 
                                odata[0].cb += sizeof(szCalIdealA)-1;
                                itoa(awgT.A, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalActualA, sizeof(szCalActualA)-1); 
                                odata[0].cb += sizeof(szCalActualA)-1;
                                itoa(((AWG *) rgInstr[AWG1_ID])->A, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalDiffA, sizeof(szCalDiffA)-1); 
                                odata[0].cb += sizeof(szCalDiffA)-1;
                                GetPercent(awgT.A - ((AWG *) rgInstr[AWG1_ID])->A, awgT.A, 4, &rgchOut[odata[0].cb], 8);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalIdealB, sizeof(szCalIdealB)-1); 
                                odata[0].cb += sizeof(szCalIdealB)-1;
                                itoa(awgT.B, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalActualB, sizeof(szCalActualB)-1); 
                                odata[0].cb += sizeof(szCalActualB)-1;
                                itoa(((AWG *) rgInstr[AWG1_ID])->B, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                memcpy(&rgchOut[odata[0].cb], szCalDiffB, sizeof(szCalDiffB)-1); 
                                odata[0].cb += sizeof(szCalDiffB)-1;
                                GetPercent(awgT.B - ((AWG *) rgInstr[AWG1_ID])->B, awgT.B, 4, &rgchOut[odata[0].cb], 8);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // end calibrationData
                                memcpy(&rgchOut[odata[0].cb], szDeviceCalDataEnd, sizeof(szDeviceCalDataEnd)-1); 
                                odata[0].cb += sizeof(szDeviceCalDataEnd)-1;

                            }

                            pjcmd.iCal.state.parsing = Idle;
                            break;

                        case JSPARCalibrationStart:

                            // we need to be in a stable finished calibration state to calibrate
                            if(!(pjcmd.iCal.state.processing == Idle || pjcmd.iCal.state.processing == NotCfgForCalibration))
                            {
                                // Put out the error status
                                utoa(CFGCalibrating, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;

                                // we aren't doing anything
                                pjcmd.iCal.state.parsing = Idle;
                            }

                            // if the instruments are not idle, we can't calibrate
                            else if(!AreInstrumentsIdle())
                            {
                                // Put out the error status
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;

                                // we aren't doing anything
                                pjcmd.iCal.state.parsing = Idle;
                            }

                            // good to go
                            else
                            {
                                // say we want to calibrate
                                pjcmd.iCal.state.processing = JSPARCalibrationStart;

                                // all instruments are calibrating
                                pjcmd.trigger.state.processing  = Calibrating;
                                pjcmd.iawg.state.processing     = Calibrating;
                                pjcmd.idcCh1.state.processing   = Calibrating;
                                pjcmd.idcCh2.state.processing   = Calibrating;
                                pjcmd.ioscCh1.state.processing  = Calibrating;
                                pjcmd.ioscCh2.state.processing  = Calibrating;

                                // status code
                                rgchOut[odata[0].cb++] = '0';

                                // wait 30000
                                memcpy(&rgchOut[odata[0].cb], szWaitCalTime, sizeof(szWaitCalTime)-1); 
                                odata[0].cb += sizeof(szWaitCalTime)-1;
                            }
                            break;

                        case JSPARCalibrationSave:
                        case JSPARCalibrationLoad:

                            // if we are asked to load or save from the SD card and it is not mounted, error out
                            if( pjcmd.iCal.config == CFGSD && !DFATFS::fsvolmounted(DFATFS::szFatFsVols[VOLSD]))
                            {
                                // Put out the error status
                                utoa(NoSDCard, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;

                                // we aren't doing anything
                                pjcmd.iCal.state.parsing = Idle;
                            }

                            // we can't load if the instruments are in use, but we can save
                            else if(pjcmd.iCal.state.parsing == JSPARCalibrationLoad && !AreInstrumentsIdle())
                            {
                                // Put out the error status
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;

                                // we aren't doing anything
                                pjcmd.iCal.state.parsing = Idle;
                            }

                            // if we are specifying a valid target to load or save, and we are not in the middle of some other 
                            // calibration process, then we can load or save
                            else if((pjcmd.iCal.config == CFGSD || pjcmd.iCal.config == CFGFLASH) && pjcmd.iCal.state.processing == Idle)
                            {
                                // status code
                                rgchOut[odata[0].cb++] = '0';

                                // wait 500
                                memcpy(&rgchOut[odata[0].cb], szWait500, sizeof(szWait500)-1); 
                                odata[0].cb += sizeof(szWait500)-1;

                                // Either load or save
                                pjcmd.iCal.state.processing = pjcmd.iCal.state.parsing;
                            }
                            else
                            {
                                // Put out the error status
                                utoa(NotCfgForCalibration, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;

                                // we aren't doing anything
                                pjcmd.iCal.state.parsing = Idle;
                            }
                            break;
                    }

                    stateEndArray = OSPARDeviceEndArray;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

            /************************************************************************/
            /*    WiFi command                                                      */
            /************************************************************************/
            case OSPARDeviceNicList:
                if(jsonToken == tokStringValue)
                {
                    memcpy(&rgchOut[odata[0].cb], szNicList, sizeof(szNicList)-1); 
                    odata[0].cb += sizeof(szNicList)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndObject;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceNicGetStatus:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARNicGetStatus;

                    // put out the save commmand
                    memcpy(&rgchOut[odata[0].cb], szDeviceNicGetStatus, sizeof(szDeviceNicGetStatus)-1); 
                    odata[0].cb += sizeof(szDeviceNicGetStatus)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFi;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceAdapter:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.nicADP = (NIC_ADP) Uint32FromStr(rgStrU32DeviceAdp, sizeof(rgStrU32DeviceAdp) / sizeof(STRU32), szToken, cbToken, (STATE) nicNone);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceNicConnect:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARNicConnect;

                    // put out the save command
                    memcpy(&rgchOut[odata[0].cb], szDeviceNicConnect, sizeof(szDeviceNicConnect)-1); 
                    odata[0].cb += sizeof(szDeviceNicConnect)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFi;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiParameterSet:
                if(jsonToken == tokStringValue)
                {                   
                    iWiFiT.fWorking = (bool) Uint32FromStr(rgStrU32DeviceWorkingParameterSet, sizeof(rgStrU32DeviceWorkingParameterSet) / sizeof(STRU32), szToken, cbToken, (STATE) false);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiForce:
                if(jsonToken == tokTrue || jsonToken == tokFalse)
                {
                    iWiFiT.fForceConn = (jsonToken == tokTrue);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceNicDisconnect:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARNicDisconnect;

                    // put out the save command
                    memcpy(&rgchOut[odata[0].cb], szDeviceNicDisconnect, sizeof(szDeviceNicDisconnect)-1); 
                    odata[0].cb += sizeof(szDeviceNicDisconnect)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFi;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiScan:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARWiFiScan;

                    // put out the save command
                    memcpy(&rgchOut[odata[0].cb], szDeviceWiFiScan, sizeof(szDeviceWiFiScan)-1); 
                    odata[0].cb += sizeof(szDeviceWiFiScan)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFi;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiListScan:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARWiFiListScan;

                    // put out the save command
                    memcpy(&rgchOut[odata[0].cb], szDeviceWiFiListScan, sizeof(szDeviceWiFiListScan)-1); 
                    odata[0].cb += sizeof(szDeviceWiFiListScan)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFi;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiSetParameters:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARWiFiSetParameters;

                    // put out the save command
                    memcpy(&rgchOut[odata[0].cb], szDeviceWiFiSetParameters, sizeof(szDeviceWiFiSetParameters)-1); 
                    odata[0].cb += sizeof(szDeviceWiFiSetParameters)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFiParam;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiPassphrase:
                if(jsonToken == tokStringValue)
                {
                    uint32_t cbPassphrase = cbToken > DEWF_MAX_PASS_PHRASE ? DEWF_MAX_PASS_PHRASE : cbToken;

                    memcpy(iWiFiT.szPassphrase, szToken, cbPassphrase);
                    iWiFiT.szPassphrase[cbPassphrase] = '\0';
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiSecurityType:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.wifiWConn.wifiKey = (SECURITY) Uint32FromStr(rgStrU32DeviceSecurityType, sizeof(rgStrU32DeviceSecurityType) / sizeof(STRU32), szToken, cbToken, (STATE) DEWF_SECURITY_OPEN);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiSSID:
                if(jsonToken == tokStringValue)
                {
                    memcpy(iWiFiT.szSSID, szToken, cbToken);
                    iWiFiT.szSSID[cbToken] ='\0';
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiAutoConnect:
                if(jsonToken == tokTrue || jsonToken == tokFalse)
                {
                    if(jsonToken == tokTrue) iWiFiT.wifiWConn.comhdr.activeFunc = WIFIFnAutoConnect;
                    else iWiFiT.wifiWConn.comhdr.activeFunc = WIFIFnManConnect;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiSaveParameters:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARWiFiSaveParameters;

                    // put out the save command
                    memcpy(&rgchOut[odata[0].cb], szDeviceWiFiSaveParameters, sizeof(szDeviceWiFiSaveParameters)-1); 
                    odata[0].cb += sizeof(szDeviceWiFiSaveParameters)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFiParam;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiLoadParameters:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARWiFiLoadParameters;

                    // put out the save command
                    memcpy(&rgchOut[odata[0].cb], szDeviceWiFiLoadParameters, sizeof(szDeviceWiFiLoadParameters)-1); 
                    odata[0].cb += sizeof(szDeviceWiFiLoadParameters)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFiParam;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiDeleteParameters:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARWiFiDeleteParameters;

                    // put out the save command
                    memcpy(&rgchOut[odata[0].cb], szDeviceWiFiDeleteParameters, sizeof(szDeviceWiFiDeleteParameters)-1); 
                    odata[0].cb += sizeof(szDeviceWiFiDeleteParameters)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFiParam;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            // this may be used for more than wifi, so we may have to set this parameter
            // in several of the temporary instrument objects, which should be okay
            // in particular, calibration data one day may specify a storage location
            case OSPARDeviceStorageLocation:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.vol = (VOLTYPE) Uint32FromStr(rgStrU32DeviceStorageLocation, sizeof(rgStrU32DeviceStorageLocation) / sizeof(STRU32), szToken, cbToken, (STATE) VOLFLASH);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceWiFiListSavedParameters:
                if(jsonToken == tokStringValue)
                {
                    iWiFiT.state.parsing = JSPARWiFiListSavedParameters;

                    // put out the save command
                    memcpy(&rgchOut[odata[0].cb], szDeviceWiFiListSavedParameters, sizeof(szDeviceWiFiListSavedParameters)-1); 
                    odata[0].cb += sizeof(szDeviceWiFiListSavedParameters)-1;

                    // get next member name
                    stateEndObject = OSPARDeviceEndWiFiParam;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceEndWiFiParam:
                if(jsonToken == tokEndObject)
                {
                    FRESULT fr = FR_OK;

                    switch(iWiFiT.state.parsing)
                    {
                        case JSPARWiFiSetParameters:
                            if(iWiFiT.szSSID[0] =='\0')
                            {
                                // Put out the error status
                                utoa(NoSSIDConfigured, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }
                            else
                            {
                                switch(iWiFiT.wifiWConn.wifiKey)
                                {
                                    case DEWF_SECURITY_OPEN:
                                        // status code
                                       rgchOut[odata[0].cb++] = '0';

                                       // save the new one away
                                       strcpy(iWiFiT.wifiWConn.ssid, iWiFiT.szSSID);
                                       memcpy(&pjcmd.iWiFi.wifiWConn, &iWiFiT.wifiWConn, sizeof(WiFiConnectInfo));
                                       memcpy((void *) &pjcmd.iWiFi.wifiWConn.comhdr.idhdr.mac , &macOpenScope, sizeof(MACADDR));
                                       break;
                                        
                                    case DEWF_SECURITY_WPA2_WITH_KEY:
                                    case DEWF_SECURITY_WPA_WITH_KEY:
                                        if(strlen(iWiFiT.szPassphrase) > 0 && deIPcK.wpaCalPSK(iWiFiT.szSSID, iWiFiT.szPassphrase, iWiFiT.wifiWConn.key.wpa2Key))
                                        {
                                            // status code
                                            rgchOut[odata[0].cb++] = '0';

                                            // save the new one away
                                            strcpy(iWiFiT.wifiWConn.ssid, iWiFiT.szSSID);
                                            memcpy(&pjcmd.iWiFi.wifiWConn, &iWiFiT.wifiWConn, sizeof(WiFiConnectInfo));
                                            memcpy((void *) &pjcmd.iWiFi.wifiWConn.comhdr.idhdr.mac , &macOpenScope, sizeof(MACADDR));
                                        }
                                        else
                                        {
                                            // Put out the error status
                                            utoa(UnableToGenKey, &rgchOut[odata[0].cb], 10);
                                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                        }
                                        break;

                                    default:
                                        // Put out the error status
                                        utoa(Unimplemented, &rgchOut[odata[0].cb], 10);
                                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                        break;
                                }
                            }

                            // wait 0
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;
                            break;

                        case JSPARWiFiSaveParameters:
                            // doing something else, can't do this
                            if(pjcmd.iWiFi.state.processing != Idle || dWiFiFile)
                            {
                                // we are work on something else
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // we are done, no waiting required
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;                                  
                            }

                            else if(pjcmd.iWiFi.wifiWConn.ssid[0] =='\0')
                            {
                                // Put out the error status
                                utoa(NoSSIDConfigured, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // we are done, no waiting required
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;                                  
                            }

                            // if there is no SD card, we can't save there
                            else if( iWiFiT.vol == VOLSD && !DFATFS::fsvolmounted(DFATFS::szFatFsVols[VOLSD]))
                            {
                                // Put out the error status
                                utoa(NoSDCard, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }

                            // queue up writing out the file
                            else
                            {
                                pjcmd.iWiFi.state.processing = JSPARWiFiSaveParameters;
                                pjcmd.iWiFi.vol = iWiFiT.vol;

                                // status code
                                rgchOut[odata[0].cb++] = '0';

                                // wait 500
                                memcpy(&rgchOut[odata[0].cb], szWait500, sizeof(szWait500)-1); 
                                odata[0].cb += sizeof(szWait500)-1;
                            }
                            break;

                        case JSPARWiFiLoadParameters:
                            // doing something else, can't do this
                            if(pjcmd.iWiFi.state.processing != Idle || dWiFiFile)
                            {
                                // we are work on something else
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // we are done, no waiting required
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;                                  
                            }

                            else if(iWiFiT.szSSID[0] =='\0')
                            {
                                // Put out the error status
                                utoa(NoSSIDConfigured, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // we are done, no waiting required
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;                                  
                            }

                            // queue up writing out the file
                            else
                            {
                                strcpy(pjcmd.iWiFi.szSSID, iWiFiT.szSSID);
                                pjcmd.iWiFi.vol = iWiFiT.vol;
                                pjcmd.iWiFi.state.processing = JSPARWiFiLoadParameters;

                                // status code
                                rgchOut[odata[0].cb++] = '0';

                                // wait 500
                                memcpy(&rgchOut[odata[0].cb], szWait500, sizeof(szWait500)-1); 
                                odata[0].cb += sizeof(szWait500)-1;
                            }
                            break;

                        case JSPARWiFiListSavedParameters:
                            // doing something else, can't do this
                            if(pjcmd.iWiFi.state.processing != Idle || dWiFiFile)
                            {
                                // we are work on something else
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // we are done, no waiting required
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;                                  
                            }

                            // if no file system, error out
                            else if((fr = DFATFS::fschdrive(DFATFS::szFatFsVols[iWiFiT.vol]))   != FR_OK    || 
                                    (fr = DFATFS::fschdir(DFATFS::szRoot)) != FR_OK                         ||
                                    (fr = DDIRINFO::fsopendir(DFATFS::szFatFsVols[iWiFiT.vol])) != FR_OK    ) 
                            {
                                // Put out the error status
                                utoa((CFGMountError | fr), &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }

                            else
                            {
                                IDHDR idHdr = iWiFiT.wifiWConn.comhdr.idhdr;
                                char sz[128];
                                char szFileName[128];
                                char const * szFile = NULL;
                                uint32_t cbsz = 0;
                                uint32_t cbReadIn = OFFSETOF(WiFiConnectInfo, ssid);

                                // create the name without and SSID on the end
                                memcpy((void *) &idHdr.mac , &macOpenScope, sizeof(idHdr.mac));
                                sz[0] = '\0';
                                CFGCreateFileName(idHdr, NULL, sz, sizeof(sz));
                                cbsz = strlen(sz);

                                // status code
                                rgchOut[odata[0].cb++] = '0';

                                // put out the parameter Set
                                memcpy(&rgchOut[odata[0].cb], szDeviceWiFiParameterSet, sizeof(szDeviceWiFiParameterSet)-1); 
                                odata[0].cb += sizeof(szDeviceWiFiParameterSet)-1;

                                // always assign a valid filename location to put the long filename in
                                DDIRINFO::fssetLongFilename(szFileName);
                                DDIRINFO::fssetLongFilenameLength(sizeof(szFileName));

                                do
                                {
                                    szFile = NULL;
                                    uint32_t cbRead;

                                    // read the next directory element
                                    if((fr = DDIRINFO::fsreaddir()) == FR_OK)
                                    {
                                        if(DDIRINFO::fsgetLongFilename()[0] != '\0')
                                        {
                                            szFile = DDIRINFO::fsgetLongFilename();
                                        }
                                        else if(DDIRINFO::fsget8Dot3Filename()[0] != 0)
                                        {
                                            szFile = DDIRINFO::fsget8Dot3Filename();
                                        }

                                        // See if this is a WiFi file
                                        if( szFile != NULL                              && 
                                            memcmp(sz, szFile, cbsz) == 0               && 
                                            dWiFiFile.fsopen(szFile, FA_READ) == FR_OK  )
                                        {
                                            if( dWiFiFile.fssize() == sizeof(WiFiConnectInfo)                   && 
                                                dWiFiFile.fsread(&iWiFiT.wifiWConn, cbReadIn, &cbRead) == FR_OK && 
                                                cbRead == cbReadIn                                              )
                                            {
                                                rgchOut[odata[0].cb++] = '{';
                                                
                                                // put out the ssid
                                                memcpy(&rgchOut[odata[0].cb], szDeviceWiFiSSID, sizeof(szDeviceWiFiSSID)-1); 
                                                odata[0].cb += sizeof(szDeviceWiFiSSID)-1;
                                                strcpy(&rgchOut[odata[0].cb], &szFile[cbsz+1]);
                                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                                // put out the securitye type
                                                memcpy(&rgchOut[odata[0].cb], szDeviceWiFiSecurityType, sizeof(szDeviceWiFiSecurityType)-1); 
                                                odata[0].cb += sizeof(szDeviceWiFiSecurityType)-1;
                                                strcpy(&rgchOut[odata[0].cb], rgszSecurityMode[iWiFiT.wifiWConn.wifiKey]);
                                                odata[0].cb += strlen(rgszSecurityMode[iWiFiT.wifiWConn.wifiKey]);

                                                // put out the autoConnect 
                                                memcpy(&rgchOut[odata[0].cb], szDeviceWiFiAutoConnect, sizeof(szDeviceWiFiAutoConnect)-1); 
                                                odata[0].cb += sizeof(szDeviceWiFiAutoConnect)-1;

                                                // auto connect
                                                if(iWiFiT.wifiWConn.comhdr.activeFunc == WIFIFnAutoConnect)
                                                {
                                                    memcpy(&rgchOut[odata[0].cb], "true", 4);
                                                    odata[0].cb += 4;
                                                }
                                                else
                                                {
                                                   memcpy(&rgchOut[odata[0].cb], "false", 5);
                                                   odata[0].cb += 5;
                                                }

                                                // close up the object
                                                rgchOut[odata[0].cb++] = '}';
                                                rgchOut[odata[0].cb++] = ',';
                                            }

                                        dWiFiFile.fsclose();
                                        }
                                    }
                                } while(szFile != NULL);

                                // take out the last ,
                                if(rgchOut[odata[0].cb-1] == ',') odata[0].cb--;
                                rgchOut[odata[0].cb++] = ']';
                                rgchOut[odata[0].cb++] = '}';

                                DDIRINFO::fsclosedir();
                            }
                            break;

                        case JSPARWiFiDeleteParameters:
                            // doing something else, can't do this
                            if(pjcmd.iWiFi.state.processing != Idle || dWiFiFile)
                            {
                                // we are work on something else
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // we are done, no waiting required
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;                                  
                            }

                            else if(iWiFiT.szSSID[0] =='\0')
                            {
                                // Put out the error status
                                utoa(NoSSIDConfigured, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // we are done, no waiting required
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;                                  
                            }

                            // delete the file
                            else
                            {
                                char sz[128];

                                // create the name without and SSID on the end
                                memcpy((void *) &iWiFiT.wifiWConn.comhdr.idhdr.mac , &macOpenScope, sizeof(MACADDR));
                                CFGCreateFileName(iWiFiT.wifiWConn.comhdr.idhdr, iWiFiT.szSSID, sz, sizeof(sz));

                                if((fr = DFATFS::fschdrive(DFATFS::szFatFsVols[iWiFiT.vol])) == FR_OK   &&
                                   (fr = DFATFS::fschdir(DFATFS::szRoot)) == FR_OK                      &&
                                   (fr = DFATFS::DFATFS::fsunlink(sz)) == FR_OK                         )
                                {
                                    // status code
                                    rgchOut[odata[0].cb++] = '0';
                                }

                                // else, something didn't work
                                else
                                {
                                    // Put out the error status
                                    utoa((CFGFileSystemError | fr), &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                               }

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }
                            break;

                        default:
                            // Put out the error status
                            utoa(InvalidSyntax, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // wait 0
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;
                            break;
                    }

                    // restore for the next WiFi command, we may be in a multi command
                    memcpy(&iWiFiT, &pjcmd.iWiFi, sizeof(IWIFI));

                    stateEndArray = OSPARDeviceEndArray;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDeviceEndWiFi:
                if(jsonToken == tokEndObject)
                {
                    // make sure they asked for the NIC we support
                    if(iWiFiT.nicADP == nicWiFi0 || iWiFiT.nicADP == nicWorking)
                    {
                        WiFiConnectInfo& wifiConn = (iWiFiT.nicADP == nicWiFi0) ? pjcmd.iWiFi.wifiAConn : pjcmd.iWiFi.wifiWConn;
                        char szIpAddr[32];
                        
                        // status code
                        rgchOut[odata[0].cb++] = '0';

                        switch(iWiFiT.state.parsing)
                        {
                            case JSPARNicGetStatus:

                                // wait 0, but do not inculded the closing brace
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-2); 
                                odata[0].cb += sizeof(szWait0)-2;

                                // put out the adaptor
                                memcpy(&rgchOut[odata[0].cb], szDeviceNicAdp, sizeof(szDeviceNicAdp)-1); 
                                odata[0].cb += sizeof(szDeviceNicAdp)-1;
                                strcpy(&rgchOut[odata[0].cb], rgszAdapter[iWiFiT.nicADP]);
                                odata[0].cb += strlen(rgszAdapter[iWiFiT.nicADP]);
 
                                // put out the security type
                                memcpy(&rgchOut[odata[0].cb], szDeviceNicSecurityType, sizeof(szDeviceNicSecurityType)-1); 
                                odata[0].cb += sizeof(szDeviceNicSecurityType)-1;
                                strcpy(&rgchOut[odata[0].cb], rgszSecurityMode[wifiConn.wifiKey]);
                                odata[0].cb += strlen(rgszSecurityMode[wifiConn.wifiKey]);
                                rgchOut[odata[0].cb++] = '\"';

                                // put out the status
                                memcpy(&rgchOut[odata[0].cb], szDeviceNicStatus, sizeof(szDeviceNicStatus)-1); 
                                odata[0].cb += sizeof(szDeviceNicStatus)-1;                              

                                // status connected or not
                                if(iWiFiT.nicADP == nicWiFi0 && IsHTTPRunning())
                                {
                                    IPv4    ipv4;

                                    // connected
                                    memcpy(&rgchOut[odata[0].cb], szDeviceNicConnected, sizeof(szDeviceNicConnected)-1); 
                                    odata[0].cb += sizeof(szDeviceNicConnected)-1;

                                    // get my ip, this should not fail
                                    deIPcK.getMyIP(ipv4);
                                    GetNumb(ipv4.u8, sizeof(ipv4), '.', szIpAddr);
                                }
                                else
                                {
                                    IPv4    ipv4;

                                    // disconnected
                                    memcpy(&rgchOut[odata[0].cb], szDeviceNicDisconnected, sizeof(szDeviceNicDisconnected)-1); 
                                    odata[0].cb += sizeof(szDeviceNicDisconnected)-1;

                                    // put in a null ip
                                    ipv4.u32 = 0;
                                    GetNumb(ipv4.u8, sizeof(ipv4), '.', szIpAddr);
                                }

                                 
                                // SSID, make sure we pull from the real iWiFi object and not the temp because
                                // this may have changed since we copied the iWiFi object
                                // also this could be an empty string if nothing has ever been loaded into the iWiFi object
                                // but if empty, it should be disconnected
                                strcpy(&rgchOut[odata[0].cb], wifiConn.ssid);
                                odata[0].cb += strlen(wifiConn.ssid);

                                // put out the ipaddress
                                memcpy(&rgchOut[odata[0].cb], szDeviceNicIpAddress, sizeof(szDeviceNicIpAddress)-1); 
                                odata[0].cb += sizeof(szDeviceNicIpAddress)-1;                              
                                strcpy(&rgchOut[odata[0].cb], szIpAddr);
                                odata[0].cb += strlen(szIpAddr);

                                rgchOut[odata[0].cb++] = '\"';
                                rgchOut[odata[0].cb++] = '}';
                                break;

                            case JSPARNicDisconnect:

                                // currently doing something else
                                if(pjcmd.iWiFi.state.processing != Idle)
                                {
                                    // remove the ok status code
                                    odata[0].cb--;

                                    // we are work on something else
                                    utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // we are done, no waiting required
                                    memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                    odata[0].cb += sizeof(szWait0)-1;                                  
                                }

                                // queue up the quit
                                else if(IsHTTPRunning())
                                {
                                    pjcmd.iWiFi.state.processing = JSPARNicDisconnect;

                                    // wait until
                                    memcpy(&rgchOut[odata[0].cb], szWaitUntil, sizeof(szWaitUntil)-1); 
                                    odata[0].cb += sizeof(szWaitUntil)-1;                                  
                                }

                                // already quit, just say we are done
                                else
                                {
                                    // we are done, no waiting required
                                    memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                    odata[0].cb += sizeof(szWait0)-1;                                  
                                }
                                break;

                            case JSPARNicConnect:
                                {
                                    // override the wifiConn variable
                                    WiFiConnectInfo& wifiConn = iWiFiT.fWorking ? pjcmd.iWiFi.wifiWConn : pjcmd.iWiFi.wifiAConn;

                                    // currently doing something else
                                    if(pjcmd.iWiFi.state.processing != Idle)
                                    {
                                        // remove the ok status code
                                        odata[0].cb--;

                                        // we are work on something else
                                        utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                        // we are done, no waiting required
                                        memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                        odata[0].cb += sizeof(szWait0)-1;                                  
                                    }

                                    else if(iWiFiT.nicADP != nicWiFi0)
                                    {
                                        // remove the ok status code
                                        odata[0].cb--;

                                        // no SSID is set up
                                        utoa(InvalidAdapter, &rgchOut[odata[0].cb], 10);
                                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                        // we are done, no waiting required
                                        memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                        odata[0].cb += sizeof(szWait0)-1;                                  
                                    }

                                    else if(strlen(wifiConn.ssid) == 0)
                                    {
                                        // remove the ok status code
                                        odata[0].cb--;

                                        // no SSID is set up
                                        utoa(NoSSIDConfigured, &rgchOut[odata[0].cb], 10);
                                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                        // we are done, no waiting required
                                        memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                        odata[0].cb += sizeof(szWait0)-1;                                  
                                    }

                                    // queue up the quit
                                    else if(!IsHTTPRunning() || iWiFiT.fForceConn)
                                    {
                                        // save away the temp value as the working value
                                        memcpy(&pjcmd.iWiFi.wifiWConn, &iWiFiT.wifiWConn, sizeof(WiFiConnectInfo));

                                        // save away our working flags.
                                        pjcmd.iWiFi.fForceConn      = iWiFiT.fForceConn;
                                        pjcmd.iWiFi.fWorking        = iWiFiT.fWorking;

                                        // no go connect
                                        pjcmd.iWiFi.state.processing = JSPARNicConnect;

                                        // wait until
                                        memcpy(&rgchOut[odata[0].cb], szWaitUntil, sizeof(szWaitUntil)-1); 
                                        odata[0].cb += sizeof(szWaitUntil)-1;                                  
                                    }

                                    // already running, say to disconnect
                                    else
                                    {
                                        // remove the ok status code
                                        odata[0].cb--;

                                        // Disconnect first
                                        utoa(MustBeDisconnected, &rgchOut[odata[0].cb], 10);
                                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                        // we are done, no waiting required
                                        memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                        odata[0].cb += sizeof(szWait0)-1;                                  
                                    }
                                }
                                break;

                            case JSPARWiFiScan:

                                // currently doing something else
                                if(pjcmd.iWiFi.state.processing != Idle)
                                {
                                    // remove the ok status code
                                    odata[0].cb--;

                                    // we are work on something else
                                    utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // we are done, no waiting required
                                    memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                    odata[0].cb += sizeof(szWait0)-1;                                  
                                }

                                // we must be disconneted to do a scan
                                else if(IsHTTPRunning())
                                {
                                    // remove the ok status code
                                    odata[0].cb--;

                                    // we must be disconnect to do a scan
                                    utoa(MustBeDisconnected, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // we are done, no waiting required
                                    memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                    odata[0].cb += sizeof(szWait0)-1;                                  
                                }

                                // already quit, just say we are done
                                else
                                {
                                    pjcmd.iWiFi.state.processing = JSPARWiFiScan;

                                    // wait until
                                    memcpy(&rgchOut[odata[0].cb], szWaitUntil, sizeof(szWaitUntil)-1); 
                                    odata[0].cb += sizeof(szWaitUntil)-1;                                  
                                }
                                break;

                            case JSPARWiFiListScan:

                               // currently doing something else
                                if(pjcmd.iWiFi.state.processing != Idle)
                                {
                                    // remove the ok status code
                                    odata[0].cb--;

                                    // we are working on something else
                                    utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // we are done, no waiting required
                                    memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                    odata[0].cb += sizeof(szWait0)-1;                                  
                                }

                                else if(pjcmd.iWiFi.fScanReady)
                                {
                                    int32_t iNetwork = 0;

                                    // wait until
                                    memcpy(&rgchOut[odata[0].cb], szDeviceWiFiAPs, sizeof(szDeviceWiFiAPs)-1); 
                                    odata[0].cb += sizeof(szDeviceWiFiAPs)-1; 

                                    while(iNetwork < pjcmd.iWiFi.wifiScan.cNetworks &&
                                            deIPcK.getScanInfo(iNetwork, &pjcmd.iWiFi.wifiScan.scanInfo) )
                                    {
                                        SECURITY securityType = DEWF_SECURITY_OPEN;

                                        rgchOut[odata[0].cb++] = '{';

                                        // SSID
                                        memcpy(&rgchOut[odata[0].cb], szDeviceWiFiSSID, sizeof(szDeviceWiFiSSID)-1); 
                                        odata[0].cb += sizeof(szDeviceWiFiSSID)-1; 

                                        // some routers broadcast 32 bytes of 0 for an SSID if no SSID is broadcasted.
                                        if(pjcmd.iWiFi.wifiScan.scanInfo.ssid[0] != '\0') 
                                        {
                                            memcpy(&rgchOut[odata[0].cb], pjcmd.iWiFi.wifiScan.scanInfo.ssid, pjcmd.iWiFi.wifiScan.scanInfo.ssidLen); 
                                            odata[0].cb += pjcmd.iWiFi.wifiScan.scanInfo.ssidLen; 
                                        }

                                        // BSSID
                                        memcpy(&rgchOut[odata[0].cb], szDeviceWiFiBSSID, sizeof(szDeviceWiFiBSSID)-1); 
                                        odata[0].cb += sizeof(szDeviceWiFiBSSID)-1; 
                                        odata[0].cb += GetNumb(pjcmd.iWiFi.wifiScan.scanInfo.bssid, DEWF_BSSID_LENGTH, ':', &rgchOut[odata[0].cb]);

                                        // Secruity Type
                                        // get the key type
                                        if      ((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b10010000) == 0b10010000)   securityType = DEWF_SECURITY_WPA2_WITH_KEY;
                                        else if ((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b01010000) == 0b01010000)   securityType = DEWF_SECURITY_WPA_WITH_KEY;
                                        else if ((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00010000) == 0b00010000)   securityType = DEWF_SECURITY_WEP_40;
                                        else                                                                            securityType = DEWF_SECURITY_OPEN;

                                        // print out the key type
                                        memcpy(&rgchOut[odata[0].cb], szDeviceWiFiSecurityType, sizeof(szDeviceWiFiSecurityType)-1); 
                                        odata[0].cb += sizeof(szDeviceWiFiSecurityType)-1;
                                        strcpy(&rgchOut[odata[0].cb], rgszSecurityMode[securityType]); 
                                        odata[0].cb += strlen(rgszSecurityMode[securityType]);

                                        // Channel
                                        memcpy(&rgchOut[odata[0].cb], szDeviceWiFiChannel, sizeof(szDeviceWiFiChannel)-1); 
                                        odata[0].cb += sizeof(szDeviceWiFiChannel)-1; 
                                        utoa(pjcmd.iWiFi.wifiScan.scanInfo.channel, &rgchOut[odata[0].cb], 10);
                                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                        // Signal Strength
                                        memcpy(&rgchOut[odata[0].cb], szDeviceWiFiSignalStrength, sizeof(szDeviceWiFiSignalStrength)-1); 
                                        odata[0].cb += sizeof(szDeviceWiFiSignalStrength)-1; 
                                        utoa(pjcmd.iWiFi.wifiScan.scanInfo.rssi, &rgchOut[odata[0].cb], 10);
                                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                        // finish out this entry
                                        rgchOut[odata[0].cb++] = '}';
                                        rgchOut[odata[0].cb++] = ','; 

                                        iNetwork++;
                                    }

                                    // remove the trailing ,
                                    if(rgchOut[odata[0].cb-1] == ',') odata[0].cb--;
                                    rgchOut[odata[0].cb++] = ']';
                                    rgchOut[odata[0].cb++] = '}';
                                }

                                // no scan data available
                                else
                                {
                                    // remove the ok status code
                                    odata[0].cb--;

                                    // Put out the error status
                                    utoa(NoScanDataAvailable, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // wait 0
                                    memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                    odata[0].cb += sizeof(szWait0)-1;
                                }
                                break;
                        }
                    }
                    else
                    {
                        // Put out the error status
                        utoa(InvalidAdapter, &rgchOut[odata[0].cb], 10);
                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                        // wait 0
                        memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                        odata[0].cb += sizeof(szWait0)-1;
                    }

                    // restore for the next WiFi command, we may be in a multi command
                    memcpy(&iWiFiT, &pjcmd.iWiFi, sizeof(IWIFI));

                    stateEndArray = OSPARDeviceEndArray;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

            /************************************************************************/
            /*    DC Power Supply                                                   */
            /************************************************************************/
            case OSPARDCChannelObject:
                if(jsonToken == tokObject)
                {
                    rgStrU32 = rgStrU32DcChannel;
                    cStrU32 = sizeof(rgStrU32DcChannel) / sizeof(STRU32);
                    memcpy(&rgchOut[odata[0].cb], szDcObject, sizeof(szDcObject)-1); 
                    odata[0].cb += sizeof(szDcObject)-1;
                    state = OSPARMemberName;
                }
                break;

            case OSPARDcCh1:
            case OSPARDcCh2:
                if(jsonToken == tokArray)
                {
                    if(curState == OSPARDcCh1) 
                    {
                        memcpy(&idcT, &pjcmd.idcCh1, sizeof(idcT));
                        memcpy(&rgchOut[odata[0].cb], szCh1Array, sizeof(szCh1Array)-1); 
                        odata[0].cb += sizeof(szCh1Array)-1;
                    }
                    else 
                    {
                        memcpy(&idcT, &pjcmd.idcCh2, sizeof(idcT));
                        memcpy(&rgchOut[odata[0].cb], szCh2Array, sizeof(szCh2Array)-1); 
                        odata[0].cb += sizeof(szCh2Array)-1;
                    }

                    rgStrU32 = rgStrU32Dc;
                    cStrU32 = sizeof(rgStrU32Dc) / sizeof(STRU32);
                    stateEndArray = OSPARDcChEnd;
                    stateEndObject = OSPARDcObjectEnd;
                    state = OSPARSkipObject;
                 }
                break;

            case OSPARDcCmd:
                if(jsonToken == tokStringValue)
                {
                    state = (STATE) Uint32FromStr(rgStrU32DcCmd, sizeof(rgStrU32DcCmd) / sizeof(STRU32), szToken, cbToken);
                    stateValueSep = OSPARMemberName;
                    fContinue = true;
                }
               break;

            case OSPARDcGetVoltage:
                if(jsonToken == tokStringValue)
                {
                    idcT.state.parsing = JSPARDcGetVoltage;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDcSetVoltage:
                if(jsonToken == tokStringValue)
                {
                    idcT.state.parsing = JSPARDcSetVoltage;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDcSetmVoltage:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    idcT.mVolts = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARDcObjectEnd:
                if(jsonToken == tokEndObject)
                {

                    STATE curProcessState =  idcT.id == DCVOLT1_ID ? pjcmd.idcCh1.state.processing : pjcmd.idcCh2.state.processing;

                    // assume we pass this state
                    stateEndArray = OSPARDcChEnd;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;

                    switch(idcT.state.parsing)
                    {
                        case JSPARDcGetVoltage:

                            // get command
                            memcpy(&rgchOut[odata[0].cb], szDcGetStatus, sizeof(szDcGetStatus)-1); 
                            odata[0].cb += sizeof(szDcGetStatus)-1;

                            if(curProcessState == Idle)
                            {
                                int32_t dcVolts;
                                STATE curState;

                                // this will take about 140us
                                while(!((curState = FBAWGorDCuV(((DCVOLT *) rgInstr[idcT.id])->channelFB, &dcVolts)) == Idle || IsStateAnError(curState)));

                                if(curState == Idle)
                                {
                                    // status code
                                    rgchOut[odata[0].cb++] = '0';
 
                                    // voltage
                                    memcpy(&rgchOut[odata[0].cb], szDcGetVoltage, sizeof(szDcGetVoltage)-1); 
                                    odata[0].cb += sizeof(szDcGetVoltage)-1;

                                    itoa((dcVolts + 500) / 1000, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                }
                                else
                                {
                                    // Put out the error status
                                    utoa(curState, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                }

                                memset(&idcT, 0, sizeof(idcT));
                            }
                            else
                            {
                                // Put out the error status
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }

                            // wait 0
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;
                            break;

                        case JSPARDcSetVoltage:

                            // get command
                            memcpy(&rgchOut[odata[0].cb], szDcSetStatus, sizeof(szDcSetStatus)-1); 
                            odata[0].cb += sizeof(szDcSetStatus)-1;

                            if(curProcessState == Idle)
                            {
                                idcT.state.processing = Working;

                                if(idcT.id == DCVOLT1_ID) memcpy(&pjcmd.idcCh1, &idcT, sizeof(idcT)); 
                                else memcpy(&pjcmd.idcCh2, &idcT, sizeof(idcT)); 

                                // status code
                                rgchOut[odata[0].cb++] = '0';

                                // wait 500
                                memcpy(&rgchOut[odata[0].cb], szWait500, sizeof(szWait500)-1); 
                                odata[0].cb += sizeof(szWait500)-1;
                            }
                            else
                            {
                                // Put out the error status
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }
                            break;

                        default:
                            state = OSPARSyntaxError;
                            fContinue = true;
                            break;
                    }
                }
                break;

            case OSPARDcChEnd:
                if(jsonToken == tokEndArray) 
                {
                    // put out end of array
                    memcpy(&rgchOut[odata[0].cb], szEndArray, sizeof(szEndArray)-1); 
                    odata[0].cb += sizeof(szEndArray)-1;

                    // reload channel members
                    rgStrU32 = rgStrU32DcChannel;
                    cStrU32 = sizeof(rgStrU32DcChannel) / sizeof(STRU32);

                    // set up for what is to come
                    stateEndObject = OSPARTopObjEnd;
                    stateValueSep = OSPARSeparatedNameValue;
                    state = OSPARSkipValueSep;
                }

            /************************************************************************/
            /*    AWG Parsing                                                       */
            /************************************************************************/
            case OSPARAwgChannelObject:
                if(jsonToken == tokObject)
                {
                    rgStrU32 = rgStrU32AwgChannel;
                    cStrU32 = sizeof(rgStrU32AwgChannel) / sizeof(STRU32);
                    memcpy(&rgchOut[odata[0].cb], szAwgObject, sizeof(szAwgObject)-1); 
                    odata[0].cb += sizeof(szAwgObject)-1;
                    state = OSPARMemberName;
                }
                break;

            case OSPARAwgCh1:
                if(jsonToken == tokArray)
                {
                    memcpy(&iawgT, &pjcmd.iawg, sizeof(iawgT)); 
                    memcpy(&rgchOut[odata[0].cb], szCh1Array, sizeof(szCh1Array)-1); 
                    odata[0].cb += sizeof(szCh1Array)-1;

                    iawgT.state.processing = Idle;
                    rgStrU32 = rgStrU32Awg;
                    cStrU32 = sizeof(rgStrU32Awg) / sizeof(STRU32);
                    stateEndArray = OSPARAwgChEnd;
                    stateEndObject = OSPARAwgObjectEnd;
                    state = OSPARSkipObject;
                }
                break;

            case OSPARAwgCmd:
                if(jsonToken == tokStringValue)
                {
                    state = (STATE) Uint32FromStr(rgStrU32AwgCmd, sizeof(rgStrU32AwgCmd) / sizeof(STRU32), szToken, cbToken);
                    stateValueSep = OSPARMemberName;
                    fContinue = true;
                }
                break;

            case OSPARAwgSetRegularWaveform:
                if(jsonToken == tokStringValue)
                {
                    // set regular waveform
                    memcpy(&rgchOut[odata[0].cb], szAwgSetRegWaveStatus, sizeof(szAwgSetRegWaveStatus)-1); 
                    odata[0].cb += sizeof(szAwgSetRegWaveStatus)-1;

                    iawgT.state.parsing = JSPARAwgSetRegularWaveform;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARAwgGetCurrentState:
                if(jsonToken == tokStringValue)
                {
                    // the get current state command
                    memcpy(&rgchOut[odata[0].cb], szAwgGetCurrentStateStatus, sizeof(szAwgGetCurrentStateStatus)-1); 
                    odata[0].cb += sizeof(szAwgGetCurrentStateStatus)-1;                               

                    iawgT.state.parsing = JSPARAwgGetCurrentState;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARAwgRun:
                if(jsonToken == tokStringValue)
                {
                    // the run command
                    memcpy(&rgchOut[odata[0].cb], szAwgRunStatus, sizeof(szAwgRunStatus)-1); 
                    odata[0].cb += sizeof(szAwgRunStatus)-1;                               

                    iawgT.state.parsing = JSPARAwgRun;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARAwgStop:
                if(jsonToken == tokStringValue)
                {
                    // put out stop command
                    memcpy(&rgchOut[odata[0].cb], szAwgStopStatus, sizeof(szAwgStopStatus)-1); 
                    odata[0].cb += sizeof(szAwgStopStatus)-1; 

                    iawgT.state.parsing = JSPARAwgStop;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARAwgSignalType:
                if(jsonToken == tokStringValue)
                {
                    iawgT.waveform = (WAVEFORM) Uint32FromStr(rgStrU32AwgSignalType, sizeof(rgStrU32AwgSignalType) / sizeof(STRU32), szToken, cbToken);
                    if((STATE) iawgT.waveform < OSPARSyntaxError)
                    {
                        state = OSPARSkipValueSep;
                    }
                }
                break;

            case OSPARAwgSignalFreq:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    iawgT.freq = atoi(szT);  // get in mHz
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARAwgVP2P:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    iawgT.mvP2P = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARAwgOffset:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    iawgT.mvOffset = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARAwgDutyCycle:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    iawgT.dutyCycle = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARAwgObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    // assume we pass this state
                    stateEndArray = OSPARAwgChEnd;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;

                    switch(iawgT.state.parsing)
                    {
                        case JSPARAwgSetRegularWaveform:

                            if( iawgT.mvP2P > AWGMAXP2P || iawgT.mvP2P < 0  || (iawgT.waveform == waveDC && iawgT.mvP2P != 0) ||
                                (iawgT.mvOffset - iawgT.mvP2P/2)  < -AWGMAXP2P || AWGMAXP2P < (iawgT.mvP2P/2 + iawgT.mvOffset) ||
                                iawgT.freq > (AWGMAXFREQ * 1000) 
                                )
                            {
                                    // Put out the error status
                                    utoa(AWGValueOutOfRange, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }

                            else if(pjcmd.iawg.state.processing != Running) 
                            {
                                int32_t cBuff, sps;
                                int16_t mvTblOffset;

                                // get and actual offset we can use, this should be identical to what was requested
                                iawgT.mvOffset = AWGmVGetActualOffsets(rgInstr[iawgT.id], (int16_t) iawgT.mvOffset, &mvTblOffset);
                                iawgT.mvOffset += mvTblOffset;

                                // get the actual frequency
                                iawgT.freq = AWGCalculateBuffAndSps(iawgT.freq, (uint32_t *) &cBuff, (uint32_t *) &sps);

                                // put in the status code
                                rgchOut[odata[0].cb++] = '0';

                                // return actual freq
                                memcpy(&rgchOut[odata[0].cb], szActualFreq, sizeof(szActualFreq)-1); 
                                odata[0].cb += sizeof(szActualFreq)-1;                               
                                utoa(iawgT.freq, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // return actual P2P
                                memcpy(&rgchOut[odata[0].cb], szVpp, sizeof(szVpp)-1); 
                                odata[0].cb += sizeof(szVpp)-1;                               
                                itoa(min(iawgT.mvP2P, AWGMAXP2P), &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // return actual Offset
                                memcpy(&rgchOut[odata[0].cb], szActualVOffset, sizeof(szActualVOffset)-1); 
                                odata[0].cb += sizeof(szActualVOffset)-1;                               
                                itoa(iawgT.mvOffset, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // queue the request
                                memcpy(&pjcmd.iawg, &iawgT, sizeof(iawgT)); 
                                memset(&iawgT, 0, sizeof(iawgT));
                                pjcmd.iawg.state.processing = JSPARAwgWaitingRegularWaveform;
                            }
                            else 
                            {
                                // Put out the error status
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }

                            // put parsing back to an Idle state
                            pjcmd.iawg.state.parsing = Idle;

                            // return await time
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;                               
                            break;
                        
                        case JSPARAwgGetCurrentState:
 
                            // put in the status code
                            rgchOut[odata[0].cb++] = '0';

                            // the current state 
                            memcpy(&rgchOut[odata[0].cb], szState, sizeof(szState)-1); 
                            odata[0].cb += sizeof(szState)-1;
                            switch(pjcmd.iawg.state.processing)
                            {
                                case Idle:
                                    strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Idle]);
                                    break;

                                case Stopped:
                                case JSPARAwgWaitingRegularWaveform:
                                case JSPARAwgWaitingArbitraryWaveform:
                                    strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Stopped]);
                                    break;

                                default:
                                    strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Running]);
                                    break;
                            }
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]); 

                            // the current wave type 
                            memcpy(&rgchOut[odata[0].cb], szAwgWaveType, sizeof(szAwgWaveType)-1); 
                            odata[0].cb += sizeof(szAwgWaveType)-1; 
                            strcpy(&rgchOut[odata[0].cb], rgszAwgWaveforms[pjcmd.iawg.waveform]);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]); 
                            rgchOut[odata[0].cb++] = '\"';

                            // the signal frequency 
                            memcpy(&rgchOut[odata[0].cb], szActualFreq, sizeof(szActualFreq)-1); 
                            odata[0].cb += sizeof(szActualFreq)-1; 
                            utoa(pjcmd.iawg.freq, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // return actual P2P
                            memcpy(&rgchOut[odata[0].cb], szVpp, sizeof(szVpp)-1); 
                            odata[0].cb += sizeof(szVpp)-1;                               
                            itoa(min(pjcmd.iawg.mvP2P, AWGMAXP2P), &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // return actual Offset
                            memcpy(&rgchOut[odata[0].cb], szActualVOffset, sizeof(szActualVOffset)-1); 
                            odata[0].cb += sizeof(szActualVOffset)-1;                               
                            itoa(pjcmd.iawg.mvOffset, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // wait 0
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;  
                            break;

                        case JSPARAwgRun:

                            // see if we can run the AWG
                            if((pjcmd.iawg.state.processing == Stopped                              ||
                                pjcmd.iawg.state.processing == JSPARAwgWaitingRegularWaveform       || 
                                pjcmd.iawg.state.processing == JSPARAwgWaitingArbitraryWaveform)    &&
                                IsLAIdle()                                                          ) 
                            {
                                if(pjcmd.iawg.state.processing == JSPARAwgWaitingRegularWaveform)           pjcmd.iawg.state.processing = JSPARAwgRunRegularWaveform;
                                else if(pjcmd.iawg.state.processing == JSPARAwgWaitingArbitraryWaveform)    pjcmd.iawg.state.processing = JSPARAwgRunArbitraryWaveform;
                                else                                                                        pjcmd.iawg.state.processing = Run;
 
                                // put in the status code
                                rgchOut[odata[0].cb++] = '0';

                                // wait 500 ms
                                memcpy(&rgchOut[odata[0].cb], szWait500, sizeof(szWait500)-1); 
                                odata[0].cb += sizeof(szWait500)-1;  
                                
                            }   
                            else 
                            {
                                // Put out the error status
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;  

                            }
                            break;

                        case JSPARAwgStop:

                            if(pjcmd.iawg.state.processing == Running)
                            {
                                if((pjcmd.iawg.state.instrument = AWGStop(rgInstr[pjcmd.iawg.id])) == Idle)
                                {
                                    // instrument is stopped
                                    pjcmd.iawg.state.processing = Stopped;

                                    // put in the status code
                                    rgchOut[odata[0].cb++] = '0';
                                }
                                else if(IsStateAnError(pjcmd.iawg.state.instrument))
                                {
                                    // error, we are idle
                                    pjcmd.iawg.state.processing = Idle;

                                    // Put out the error status
                                    utoa(pjcmd.iawg.state.instrument, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                }
                            }

                            // we are still in the process of getting going
                            else if(pjcmd.iawg.state.processing == JSPARAwgRunRegularWaveform || pjcmd.iawg.state.processing == JSPARAwgRunArbitraryWaveform)
                            {
                                // Put out the error status
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }

                            // otherwise reset to an Idle condition
                            else
                            {
                                // stop whatever we are doing
                                pjcmd.iawg.state.processing = Idle;

                                // put in the status code
                                rgchOut[odata[0].cb++] = '0';
                            }

                            // put parsing back to an Idle state
                            pjcmd.iawg.state.parsing = Idle;

                            // wait 0
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;  

                            break;

                        default:
                            state = OSPARSyntaxError;
                            fContinue = true;
                            break;
                    }
                }
                break;

            case OSPARAwgChEnd:
                if(jsonToken == tokEndArray) 
                {
                    // put out end of array
                    memcpy(&rgchOut[odata[0].cb], szEndArray, sizeof(szEndArray)-1); 
                    odata[0].cb += sizeof(szEndArray)-1;

                    // reload channel members
                    rgStrU32 = rgStrU32AwgChannel;
                    cStrU32 = sizeof(rgStrU32AwgChannel) / sizeof(STRU32);

                    // set up for what is to come
                    stateEndObject = OSPARTopObjEnd;
                    stateValueSep = OSPARSeparatedNameValue;
                    state = OSPARSkipValueSep;
                }

            /************************************************************************/
            /*    OSC Parsing                                                       */
            /************************************************************************/
            case OSPAROscChannelObject:
                if(jsonToken == tokObject)
                {
                    rgStrU32 = rgStrU32OscChannel;
                    cStrU32 = sizeof(rgStrU32OscChannel) / sizeof(STRU32);
                    memcpy(&rgchOut[odata[0].cb], szOscObject, sizeof(szOscObject)-1); 
                    odata[0].cb += sizeof(szOscObject)-1;
                    state = OSPARMemberName;
                }
                break;

            case OSPAROscCh1:
            case OSPAROscCh2:
                if(jsonToken == tokArray)
                {
                    if(curState == OSPAROscCh1)
                    {
                        memcpy(&ioscT, &pjcmd.ioscCh1, sizeof(ioscT));
                        memcpy(&rgchOut[odata[0].cb], szCh1Array, sizeof(szCh1Array)-1); 
                        odata[0].cb += sizeof(szCh1Array)-1;
                    }
                    else if(curState == OSPAROscCh2) 
                    {
                        memcpy(&ioscT, &pjcmd.ioscCh2, sizeof(ioscT));
                        memcpy(&rgchOut[odata[0].cb], szCh2Array, sizeof(szCh2Array)-1); 
                        odata[0].cb += sizeof(szCh2Array)-1;
                    }

                    rgStrU32 = rgStrU32Osc;
                    cStrU32 = sizeof(rgStrU32Osc) / sizeof(STRU32);
                    stateEndArray = OSPAROscChEnd;
                    stateEndObject = OSPAROscObjectEnd;
                    state = OSPARSkipObject;
                }
                break;

            case OSPAROscCmd:
                if(jsonToken == tokStringValue)
                {
                    state = (STATE) Uint32FromStr(rgStrU32OscCmd, sizeof(rgStrU32OscCmd) / sizeof(STRU32), szToken, cbToken);
                    stateValueSep = OSPARMemberName;
                    fContinue = true;
                }
               break;

            case OSPAROscSetParm:
                if(jsonToken == tokStringValue)
                {
                    ioscT.state.parsing = JSPARSetParm;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPAROscSetOffset:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    ioscT.mvOffset = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPAROscSetGain:
                if(jsonToken == tokNumber && cbToken < 32)
                {
                    state = OSPARSkipValueSep;
                    if(cbToken == (sizeof(szGain1)-1) && strncmp(szGain1, szToken, (sizeof(szGain1)-1)) == 0) ioscT.gain = 1;
                    else if(cbToken == (sizeof(szGain2)-1) && strncmp(szGain2, szToken, (sizeof(szGain2)-1)) == 0) ioscT.gain = 2;
                    else if(cbToken == (sizeof(szGain3)-1) && strncmp(szGain3, szToken, (sizeof(szGain3)-1)) == 0) ioscT.gain = 3;
                    else if(cbToken == (sizeof(szGain4)-1) && strncmp(szGain4, szToken, (sizeof(szGain4)-1)) == 0) ioscT.gain = 4;
                    else state = OSPARSyntaxError;
                }
                break;

            case OSPAROscSetSampleFreq:
                if(jsonToken == tokNumber && cbToken <= 20)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    ioscT.bidx.msps = atoll(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPAROscSetBufferSize:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    ioscT.bidx.cBuff = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPAROscSetTrigDelay:
                if(jsonToken == tokNumber  && cbToken <= 21)
                {
                    char szT[22]; // allow for the NULL

                    // we are going to get this int64 in 3 groups of 7
                    // We will have to check for the negative
                    memset(szT, '0', sizeof(szT));
                    szT[sizeof(szT)-1] = 0;

                    // now copy the number into the buffer
                    memcpy(&szT[sizeof(szT) - cbToken - 1], szToken, cbToken);

                    // see if we have a negative number
                    if(szToken[0] == '-')
                    {
                        // wipe out the - sign
                        szT[sizeof(szT) - cbToken - 1] = '0';
                    }

                    // put the least significant 7 digits in 
                    ioscT.bidx.psDelay = (int64_t) atoi(&szT[14]);
                    szT[14] = 0;

                    // the next 7 digits
                    ioscT.bidx.psDelay += ((int64_t) atoi(&szT[7])) * 10000000ll;
                    szT[7] = 0;

                    // the last 6 digits, the MSDigit will be 0 (may have initially been a - sign)
                    ioscT.bidx.psDelay += ((int64_t) atoi(&szT[0])) * 100000000000000ll;
                    
                    // see if we have a negative number
                    if(szToken[0] == '-')
                    {
                        // wipe out the - sign
                        ioscT.bidx.psDelay *= -1ll;
                    }
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPAROscRead:
                if(jsonToken == tokStringValue)
                {
                    state = OSPARSkipValueSep;
                    ioscT.state.parsing = JSPAROscRead;
                }
                break;

            case OSPAROscGetCurrentState:
                if(jsonToken == tokStringValue)
                {
                    state = OSPARSkipValueSep;
                    ioscT.state.parsing = JSPAROscGetCurrentState;
                }
                break;

            case OSPAROscSetAcqCount:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    ioscT.acqCount = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPAROscObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    int32_t pwm = 0;

                    // get the latest in this state, we have a stale state in there
                    ioscT.state.processing = (ioscT.id == OSC1_ID) ? pjcmd.ioscCh1.state.processing : pjcmd.ioscCh2.state.processing;

                    // assume we pass this state
                    stateEndArray = OSPAROscChEnd;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;

                    switch(ioscT.state.parsing)
                    {

                        case JSPARSetParm:

                            // put out the command and status
                            memcpy(&rgchOut[odata[0].cb], szSetParmStatusCode, sizeof(szSetParmStatusCode)-1); 
                            odata[0].cb += sizeof(szSetParmStatusCode)-1;

                            // are we in a state we can't change the parameters
                            if(ioscT.buffLock != LOCKAvailable)
                            {
                                // Error Code
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }

                            // Do a check of parameter Ranging
                            else if(    !(1 <= ioscT.gain && ioscT.gain <= 4)                                                                               ||                                        
                                        !(PWMLOWLIMIT <= (pwm = OSCPWM(((OSC *) rgInstr[ioscT.id]), ioscT.gain, ioscT.mvOffset)) && pwm <= PWMHIGHLIMIT)    ||
                                        !(MINmSAMPLEFREQ <= ioscT.bidx.msps && ioscT.bidx.msps <= MAXmSAMPLEFREQ)                                                     ||
                                        !(2 <= ioscT.bidx.cBuff && ioscT.bidx.cBuff <= AINMAXBUFFSIZE  && (ioscT.bidx.cBuff % 2) == 0)  
                                    )
                            {
                                // Error Code
                                utoa(ValueOutOfRange, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }
                            else
                            { 
                                // status code
                                rgchOut[odata[0].cb] = '0';
                                odata[0].cb++;

                                // put out the actual offset
                                memcpy(&rgchOut[odata[0].cb], szActualVOffset, sizeof(szActualVOffset)-1); 
                                odata[0].cb += sizeof(szActualVOffset)-1;
                                itoa(OSCVinFromDadcGainOffset(((OSC *) rgInstr[ioscT.id]),  0, (ioscT.gain-1), ioscT.mvOffset), &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                CalculateBufferIndexes(&ioscT.bidx);

                                // put out the actual Freq
                                memcpy(&rgchOut[odata[0].cb], szActualSampleFreq, sizeof(szActualSampleFreq)-1); 
                                odata[0].cb += sizeof(szActualSampleFreq)-1;
                                ulltoa(ioscT.bidx.msps, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // trigger delay
                                memcpy(&rgchOut[odata[0].cb], szActualTriggerDelay, sizeof(szActualTriggerDelay)-1); 
                                odata[0].cb += sizeof(szActualTriggerDelay)-1;
                                odata[0].cb += strlen(illtoa(ioscT.bidx.psDelay, &rgchOut[odata[0].cb], 10));
                                    
                                // say we are waiting to apply the scope
                                ioscT.state.processing = Waiting;

                                // copy it over into the actual data structure
                                if (ioscT.id == OSC1_ID) memcpy(&pjcmd.ioscCh1, &ioscT, sizeof(ioscT));
                                else memcpy(&pjcmd.ioscCh2, &ioscT, sizeof(ioscT));
                            }

                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;

                            break;

                        case JSPAROscRead:

                            // put out the command and status
                            memcpy(&rgchOut[odata[0].cb], szReadStatusCode, sizeof(szReadStatusCode)-1); 
                            odata[0].cb += sizeof(szReadStatusCode)-1;
  
                            if(ioscT.state.processing == Triggered && ioscT.buffLock == LOCKAvailable)
                            {
                                uint32_t    acqCountBuf = 0;

                                // get the correct acq Count
                                if (ioscT.id == OSC1_ID) acqCountBuf = pjcmd.ioscCh1.acqCount;
                                else acqCountBuf = pjcmd.ioscCh2.acqCount;

                                // wrong acqCount
                                if(ioscT.acqCount > acqCountBuf)
                                {
                                    // Error Code
                                    utoa(AcqCountTooOld, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                }

                                else
                                {
                                    // say the buffer is locked for output
                                    ioscT.buffLock = LOCKOutput;

                                    // status code
                                    rgchOut[odata[0].cb] = '0';
                                    odata[0].cb++;

                                    // binary offset
                                    memcpy(&rgchOut[odata[0].cb], szBinaryOffset, sizeof(szBinaryOffset)-1); 
                                    odata[0].cb += sizeof(szBinaryOffset)-1;
                                    utoa(iBinOffset, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // fill in the binary location info
                                    odata[cOData].cb = ioscT.bidx.cBuff * sizeof(int16_t);
                                    odata[cOData].pbOut = (uint8_t *) ioscT.pBuff;
//                                    odata[cOData].pbOut = (uint8_t *) &ioscT.pBuff[ioscT.iStartRetBuf];

                                    // binary length 
                                    memcpy(&rgchOut[odata[0].cb], szBinaryLength, sizeof(szBinaryLength)-1); 
                                    odata[0].cb += sizeof(szBinaryLength)-1;
                                    utoa(odata[cOData].cb, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // update the offset for the next one
                                    ioscT.iBinOffset = iBinOffset;
                                    iBinOffset += odata[cOData].cb;

                                    // Put out the acqCount
                                    ioscT.acqCount = acqCountBuf;
                                    memcpy(&rgchOut[odata[0].cb], szAcqCount, sizeof(szAcqCount)-1); 
                                    odata[0].cb += sizeof(szAcqCount)-1;
                                    utoa(acqCountBuf, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // put out the sps Freq
                                    memcpy(&rgchOut[odata[0].cb], szActualSampleFreq, sizeof(szActualSampleFreq)-1); 
                                    odata[0].cb += sizeof(szActualSampleFreq)-1;
                                    ulltoa(ioscT.bidx.msps, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // POI
                                    memcpy(&rgchOut[odata[0].cb], szPointOfInterest, sizeof(szPointOfInterest)-1); 
                                    odata[0].cb += sizeof(szPointOfInterest)-1;
                                    utoa(ioscT.bidx.iPOI, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // trigger index
                                    memcpy(&rgchOut[odata[0].cb], szTriggerIndex, sizeof(szTriggerIndex)-1); 
                                    odata[0].cb += sizeof(szTriggerIndex)-1;
                                    itoa(ioscT.bidx.iTrg, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // TBD: REMOVE trigger delay
                                    memcpy(&rgchOut[odata[0].cb], szTriggerDelay, sizeof(szTriggerDelay)-1); 
                                    odata[0].cb += sizeof(szTriggerDelay)-1;
                                    odata[0].cb += strlen(illtoa(ioscT.bidx.psDelay, &rgchOut[odata[0].cb], 10));

                                    // trigger delay
                                    memcpy(&rgchOut[odata[0].cb], szActualTriggerDelay, sizeof(szActualTriggerDelay)-1); 
                                    odata[0].cb += sizeof(szActualTriggerDelay)-1;
                                    odata[0].cb += strlen(illtoa(ioscT.bidx.psDelay, &rgchOut[odata[0].cb], 10));
                                    
                                    // Osc Offset
                                    memcpy(&rgchOut[odata[0].cb], szActualVOffset, sizeof(szActualVOffset)-1); 
                                    odata[0].cb += sizeof(szActualVOffset)-1;
                                    itoa(OSCVinFromDadcGainOffset(((OSC *) rgInstr[ioscT.id]),  0, (ioscT.gain-1), ioscT.mvOffset), &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // put in the gain
                                    memcpy(&rgchOut[odata[0].cb], szActualGain, sizeof(szActualGain)-1); 
                                    odata[0].cb += sizeof(szActualGain)-1;
                                    memcpy(&rgchOut[odata[0].cb], rgszGains[ioscT.gain], strlen(rgszGains[ioscT.gain])); 
                                    odata[0].cb += strlen(rgszGains[ioscT.gain]);
                                
                                    if (ioscT.id == OSC1_ID) 
                                    {
                                        memcpy(&pjcmd.ioscCh1, &ioscT, sizeof(ioscT));
                                        odata[cOData].pLockState = &pjcmd.ioscCh1.buffLock;
                                    }
                                    else 
                                    {
                                        memcpy(&pjcmd.ioscCh2, &ioscT, sizeof(ioscT));
                                        odata[cOData].pLockState = &pjcmd.ioscCh2.buffLock;
                                    }
                                       
                                    // now go to the next binary buffer output.
                                    cOData++;
                                }
                            }

                            else if(ioscT.state.processing == Armed) 
                            {     
                                // Error Code
                                utoa(InstrumentArmed, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }
                            
                            else
                            {
                                // Error Code
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }

                            // put out the wait time
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;

                            break;

                        case JSPAROscGetCurrentState:

                            // put out the command and status
                            memcpy(&rgchOut[odata[0].cb], szGetCurrentStateStatusCode, sizeof(szGetCurrentStateStatusCode)-1); 
                            odata[0].cb += sizeof(szGetCurrentStateStatusCode)-1;

                            // refresh the data
                            if (ioscT.id == OSC1_ID) memcpy(&ioscT, &pjcmd.ioscCh1, sizeof(ioscT));
                            else memcpy(&ioscT, &pjcmd.ioscCh2, sizeof(ioscT));

                            // status code
                            rgchOut[odata[0].cb++] = '0';

                            // Put out the acqCount
                            memcpy(&rgchOut[odata[0].cb], szAcqCount, sizeof(szAcqCount)-1); 
                            odata[0].cb += sizeof(szAcqCount)-1;
                            utoa(ioscT.acqCount, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // Osc Offset
                            memcpy(&rgchOut[odata[0].cb], szActualVOffset, sizeof(szActualVOffset)-1); 
                            odata[0].cb += sizeof(szActualVOffset)-1;
                            itoa(OSCVinFromDadcGainOffset(((OSC *) rgInstr[ioscT.id]),  0, (ioscT.gain-1), ioscT.mvOffset), &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // put out the sps Freq
                            memcpy(&rgchOut[odata[0].cb], szActualSampleFreq, sizeof(szActualSampleFreq)-1); 
                            odata[0].cb += sizeof(szActualSampleFreq)-1;
                            ulltoa(ioscT.bidx.msps, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // put in the gain
                            memcpy(&rgchOut[odata[0].cb], szActualGain, sizeof(szActualGain)-1); 
                            odata[0].cb += sizeof(szActualGain)-1;
                            memcpy(&rgchOut[odata[0].cb], rgszGains[ioscT.gain], strlen(rgszGains[ioscT.gain])); 
                            odata[0].cb += strlen(rgszGains[ioscT.gain]);

                            // TBD: REMOVE trigger delay
                            memcpy(&rgchOut[odata[0].cb], szTriggerDelay, sizeof(szTriggerDelay)-1); 
                            odata[0].cb += sizeof(szTriggerDelay)-1;
                            odata[0].cb += strlen(illtoa(ioscT.bidx.psDelay, &rgchOut[odata[0].cb], 10));

                            // trigger delay
                            memcpy(&rgchOut[odata[0].cb], szActualTriggerDelay, sizeof(szActualTriggerDelay)-1); 
                            odata[0].cb += sizeof(szActualTriggerDelay)-1;
                            odata[0].cb += strlen(illtoa(ioscT.bidx.psDelay, &rgchOut[odata[0].cb], 10));

                            // buffer size
                            memcpy(&rgchOut[odata[0].cb], szBufferSize, sizeof(szBufferSize)-1); 
                            odata[0].cb += sizeof(szBufferSize)-1;
                            utoa(ioscT.bidx.cBuff, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                
                            // the current state 
                            memcpy(&rgchOut[odata[0].cb], szState, sizeof(szState)-1); 
                            odata[0].cb += sizeof(szState)-1;

                            // put out the trigger state
                            switch(ioscT.state.processing)
                            {
                                case Idle:
                                case Waiting:
                                    // put out idle
                                    strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Idle]); 
                                    break;

                                case Triggered:
                                    // put out triggered
                                    strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Triggered]); 
                                    break;

                                case Armed:
                                    if(T9CONbits.ON)
                                    {
                                        // put out acquiring
                                        strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Acquiring]); 
                                    }

                                    // otherwise we are armed or in the process of being armed
                                    else
                                    {
                                        // put out acquiring
                                        strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Armed]); 
                                    }
                                    break;

                                // Say armed or acquiring when running
                                default:
                                    // busy doing something esle
                                    strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Busy]); 
                                    break;
                            }
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]); 

                            // put out the wait time
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;
                            break;

                        default:
                            state = OSPARSyntaxError;
                            fContinue = true;
                            break;
                    }                 
                }
                break;

            case OSPAROscChEnd:
                if(jsonToken == tokEndArray) 
                {
                    // put out end of array
                    memcpy(&rgchOut[odata[0].cb], szEndArray, sizeof(szEndArray)-1); 
                    odata[0].cb += sizeof(szEndArray)-1;

                    // reload channel members
                    rgStrU32 = rgStrU32OscChannel;
                    cStrU32 = sizeof(rgStrU32OscChannel) / sizeof(STRU32);

                    // set up for what is to come
                    stateEndObject = OSPARTopObjEnd;
                    stateValueSep = OSPARSeparatedNameValue;
                    state = OSPARSkipValueSep;
                }

            /************************************************************************/
            /*    Logic Analyzer Parsing                                            */
            /************************************************************************/
            case OSPARLaChannelObject:
                if(jsonToken == tokObject)
                {
                    memcpy(&ilaT, &pjcmd.ila, sizeof(ILA));
                    rgStrU32 = rgStrU32LaChannel;
                    cStrU32 = sizeof(rgStrU32LaChannel) / sizeof(STRU32);
                    memcpy(&rgchOut[odata[0].cb], szLaObject, sizeof(szLaObject)-1); 
                    odata[0].cb += sizeof(szLaObject)-1;
                    state = OSPARMemberName;
                }
                break;

            case OSPARLaCh1:
                if(jsonToken == tokArray)
                {
                    rgStrU32 = rgStrU32La;
                    cStrU32 = sizeof(rgStrU32La) / sizeof(STRU32);
                    memcpy(&rgchOut[odata[0].cb], szCh1Array, sizeof(szCh1Array)-1); 
                    odata[0].cb += sizeof(szCh1Array)-1;        
                    stateEndArray = OSPARLaChArrayEnd;
                    stateEndObject = OSPARLaObjectEnd;
                    state = OSPARSkipObject;
                }
                break;

            case OSPARLaCmd:
                if(jsonToken == tokStringValue)
                {
                    state = (STATE) Uint32FromStr(rgStrU32LaCmd, sizeof(rgStrU32LaCmd) / sizeof(STRU32), szToken, cbToken);
                    stateValueSep = OSPARMemberName;
                    fContinue = true;
                }
                break;

            case OSPARLaSetParm:
                if(jsonToken == tokStringValue)
                {
                    ilaT.state.parsing = JSPARSetParm;

                    // put the SetParam command
                    memcpy(&rgchOut[odata[0].cb], szSetParmStatusCode, sizeof(szSetParmStatusCode)-1); 
                    odata[0].cb += sizeof(szSetParmStatusCode)-1;

                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARLaSetSampleFreq:
                if(jsonToken == tokNumber && cbToken <= 20)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    ilaT.bidx.msps = atoll(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARLaSetBufferSize:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    ilaT.bidx.cBuff = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;
                
            case OSPARLaSetAcqCount:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    ilaT.acqCount = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARLaBitMask:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    ilaT.bitMask = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARLaSetTrigDelay:
                if(jsonToken == tokNumber && cbToken <= 20)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    ilaT.bidx.psDelay = atoll(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARLaRead:
                if(jsonToken == tokStringValue)
                {
                    // put out the command and status
                    memcpy(&rgchOut[odata[0].cb], szReadStatusCode, sizeof(szReadStatusCode)-1); 
                    odata[0].cb += sizeof(szReadStatusCode)-1;
  
                    state = OSPARSkipValueSep;
                    ilaT.state.parsing = JSPARLaRead;
                }
                break;

            case OSPARLaGetCurrentState:
                if(jsonToken == tokStringValue)
                {
                    // put out the command and status
                    memcpy(&rgchOut[odata[0].cb], szGetCurrentStateStatusCode, sizeof(szGetCurrentStateStatusCode)-1); 
                    odata[0].cb += sizeof(szGetCurrentStateStatusCode)-1;

                    state = OSPARSkipValueSep;
                    ilaT.state.parsing = JSPARLaGetCurrentState;
                }
                break;

            case OSPARLaRun:
                if(jsonToken == tokStringValue)
                {
                    // put the Run command
                    memcpy(&rgchOut[odata[0].cb], szLaRun, sizeof(szLaRun)-1); 
                    odata[0].cb += sizeof(szLaRun)-1;

                    // get this out so we run the loop commands to stop all IO
//                    fBlockIOBus = true;

                    // say we want to run
                    ilaT.state.parsing = JSPARLaRun;

                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARLaStop:
                if(jsonToken == tokStringValue)
                {
                    // put Stop command
                    memcpy(&rgchOut[odata[0].cb], szLaStop, sizeof(szLaStop)-1); 
                    odata[0].cb += sizeof(szLaStop)-1;

                    // say we want to run
                    ilaT.state.parsing = JSPARLaStop;

                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARLaObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    LA& la = *((LA *) rgInstr[LOGIC1_ID]);

                    switch(ilaT.state.parsing)
                    {
                        case JSPARSetParm:

                            // are we in a state we can't change the parameters
                            if(ilaT.buffLock != LOCKAvailable)
                            {
                                // Error Code
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }

                            // Do a check of parameter Ranging
                            else if(ilaT.bidx.msps > LAMAXmSPS || !CalculateBufferIndexes(&ilaT.bidx))                                      
                            {
                                // Error Code
                                utoa(ValueOutOfRange, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }
                            else
                            { 
                                // status code
                                rgchOut[odata[0].cb] = '0';
                                odata[0].cb++;

                                // put out the actual Freq
                                memcpy(&rgchOut[odata[0].cb], szActualSampleFreq, sizeof(szActualSampleFreq)-1); 
                                odata[0].cb += sizeof(szActualSampleFreq)-1;
                                ulltoa(ilaT.bidx.msps, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // trigger delay
                                memcpy(&rgchOut[odata[0].cb], szActualTriggerDelay, sizeof(szActualTriggerDelay)-1); 
                                odata[0].cb += sizeof(szActualTriggerDelay)-1;
                                odata[0].cb += strlen(illtoa(ilaT.bidx.psDelay, &rgchOut[odata[0].cb], 10));

                                // say we are waiting to apply the scope
                                ilaT.state.processing = Waiting;

                                memcpy(&pjcmd.ila, &ilaT, sizeof(ilaT));
                            }

                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;

                            break;

                        case JSPARLaRead:

                            if(pjcmd.ila.state.processing == Triggered && pjcmd.ila.buffLock == LOCKAvailable)
                            {
                                uint32_t    acqCountBuf = 0;

                                // get the correct acq Count
                                acqCountBuf = pjcmd.ila.acqCount;

                                // wrong acqCount
                                if(ilaT.acqCount > acqCountBuf)
                                {
                                    // Error Code
                                    utoa(AcqCountTooOld, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                }

                                else
                                {
                                    // say the buffer is locked for output
                                    ilaT.buffLock = LOCKOutput;

                                    // status code
                                    rgchOut[odata[0].cb] = '0';
                                    odata[0].cb++;

                                    // binary offset
                                    memcpy(&rgchOut[odata[0].cb], szBinaryOffset, sizeof(szBinaryOffset)-1); 
                                    odata[0].cb += sizeof(szBinaryOffset)-1;
                                    utoa(iBinOffset, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // fill in the binary location info
                                    odata[cOData].cb = ilaT.bidx.cBuff * sizeof(int16_t);
                                    odata[cOData].pbOut = (uint8_t *) ilaT.pBuff;
//                                    odata[cOData].pbOut = (uint8_t *) &ilaT.pBuff[ilaT.iStartRetBuf];

                                    // binary length 
                                    memcpy(&rgchOut[odata[0].cb], szBinaryLength, sizeof(szBinaryLength)-1); 
                                    odata[0].cb += sizeof(szBinaryLength)-1;
                                    utoa(odata[cOData].cb, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // update the offset for the next one
                                    ilaT.iBinOffset = iBinOffset;
                                    iBinOffset += odata[cOData].cb;

                                    // Put out the acqCount
                                    ilaT.acqCount = acqCountBuf;
                                    memcpy(&rgchOut[odata[0].cb], szAcqCount, sizeof(szAcqCount)-1); 
                                    odata[0].cb += sizeof(szAcqCount)-1;
                                    utoa(acqCountBuf, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // Put out the bitmask
                                    memcpy(&rgchOut[odata[0].cb], szBitMask, sizeof(szBitMask)-1); 
                                    odata[0].cb += sizeof(szBitMask)-1;
                                    utoa(ilaT.bitMask, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // put out the sps Freq
                                    memcpy(&rgchOut[odata[0].cb], szActualSampleFreq, sizeof(szActualSampleFreq)-1); 
                                    odata[0].cb += sizeof(szActualSampleFreq)-1;
                                    ulltoa(ilaT.bidx.msps, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // POI
                                    memcpy(&rgchOut[odata[0].cb], szPointOfInterest, sizeof(szPointOfInterest)-1); 
                                    odata[0].cb += sizeof(szPointOfInterest)-1;
                                    utoa(ilaT.bidx.iPOI, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // trigger index
                                    memcpy(&rgchOut[odata[0].cb], szTriggerIndex, sizeof(szTriggerIndex)-1); 
                                    odata[0].cb += sizeof(szTriggerIndex)-1;
                                    itoa(ilaT.bidx.iTrg, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                    // TBD: REMOVE trigger delay
                                    memcpy(&rgchOut[odata[0].cb], szTriggerDelay, sizeof(szTriggerDelay)-1); 
                                    odata[0].cb += sizeof(szTriggerDelay)-1;
                                    odata[0].cb += strlen(illtoa(ilaT.bidx.psDelay, &rgchOut[odata[0].cb], 10));

                                    // trigger delay
                                    memcpy(&rgchOut[odata[0].cb], szActualTriggerDelay, sizeof(szActualTriggerDelay)-1); 
                                    odata[0].cb += sizeof(szActualTriggerDelay)-1;
                                    odata[0].cb += strlen(illtoa(ilaT.bidx.psDelay, &rgchOut[odata[0].cb], 10));

                                    memcpy(&pjcmd.ila, &ilaT, sizeof(ilaT));
                                    odata[cOData].pLockState = &pjcmd.ila.buffLock;
                                       
                                    // now go to the next binary buffer output.
                                    cOData++;
                                }
                            }

                            else if(ilaT.state.processing == Armed) 
                            {     
                                // Error Code
                                utoa(InstrumentArmed, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }
                            
                            else
                            {
                                // Error Code
                                utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                            }

                            // put out the wait time
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;

                            break;

                        case JSPARLaGetCurrentState:

                            // refresh the data
                            memcpy(&ilaT, &pjcmd.ila, sizeof(ilaT));

                            // status code
                            rgchOut[odata[0].cb++] = '0';

                            // Put out the acqCount
                            memcpy(&rgchOut[odata[0].cb], szAcqCount, sizeof(szAcqCount)-1); 
                            odata[0].cb += sizeof(szAcqCount)-1;
                            utoa(ilaT.acqCount, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // Put out the bitmask
                            memcpy(&rgchOut[odata[0].cb], szBitMask, sizeof(szBitMask)-1); 
                            odata[0].cb += sizeof(szBitMask)-1;
                            utoa(ilaT.bitMask, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // put out the sps Freq
                            memcpy(&rgchOut[odata[0].cb], szActualSampleFreq, sizeof(szActualSampleFreq)-1); 
                            odata[0].cb += sizeof(szActualSampleFreq)-1;
                            ulltoa(ilaT.bidx.msps, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // TBD: REMOVE trigger delay
                            memcpy(&rgchOut[odata[0].cb], szTriggerDelay, sizeof(szTriggerDelay)-1); 
                            odata[0].cb += sizeof(szTriggerDelay)-1;
                            odata[0].cb += strlen(illtoa(ilaT.bidx.psDelay, &rgchOut[odata[0].cb], 10));

                            // trigger delay
                            memcpy(&rgchOut[odata[0].cb], szActualTriggerDelay, sizeof(szActualTriggerDelay)-1); 
                            odata[0].cb += sizeof(szActualTriggerDelay)-1;
                            odata[0].cb += strlen(illtoa(ilaT.bidx.psDelay, &rgchOut[odata[0].cb], 10));

                            // buffer size
                            memcpy(&rgchOut[odata[0].cb], szBufferSize, sizeof(szBufferSize)-1); 
                            odata[0].cb += sizeof(szBufferSize)-1;
                            utoa(ilaT.bidx.cBuff, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                
                            // the current state 
                            memcpy(&rgchOut[odata[0].cb], szState, sizeof(szState)-1); 
                            odata[0].cb += sizeof(szState)-1;

                            // put out the trigger state
                            switch(ilaT.state.processing)
                            {
                                case Idle:
                                case Waiting:
                                    // put out idle
                                    strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Idle]); 
                                    break;

                                case Triggered:
                                    // put out triggered
                                    strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Triggered]); 
                                    break;

                                case Armed:
                                    if(T9CONbits.ON)
                                    {
                                        // put out acquiring
                                        strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Acquiring]); 
                                    }

                                    // otherwise we are armed or in the process of being armed
                                    else
                                    {
                                        // put out armed
                                        strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Armed]); 
                                    }
                                    break;

                                // busy doing something esle
                                default:
                                    strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Busy]); 
                                    break;
                            }
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]); 

                            // put out the wait time
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;
                            break;

                        case JSPARLaRun:

                            pjcmd.ila.state.processing = JSPARLaRun;

                            // DMA setup, this is shared with the Logic Analyzer, so we have to set it up each time
                            la.pDMA->DCHxCON.CHEN        = 0;                    // make sure the DMA is disabled
                            la.pDMA->DCHxDSA             = la.addPhyDst;  // RAM location for the LA
                            la.pDMA->DCHxDSIZ            = la.cbDst;      // destination size of our target buffer
                            la.pDMA->DCHxSSA             = la.addPhySrc;  // physical address of the source PORT E
                            la.pDMA->DCHxSSIZ            = 2;                    // how many bytes (not items) of the source buffer

                            la.pTMR->PRx                 = PeriodToPRx(10);
                            la.pTMR->TxCON.TCKPS         = 0;
                            la.pTMR->TMRx                = 0;

                            la.pDMA->DCHxCON.CHEN        = 1;                    // enable the DMA channel
                            la.pTMR->TxCON.ON            = 1;                    // turn the timer on

                            // put out the wait time
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;
                            break;

                        case JSPARLaStop:

                            // cue up the stop
                            pjcmd.ila.state.processing = JSPARLaStop;

                            // put out the wait time
                            memcpy(&rgchOut[odata[0].cb], szWaitUntil, sizeof(szWaitUntil)-1); 
                            odata[0].cb += sizeof(szWaitUntil)-1;

                            break;

                        default:
                            ASSERT(NEVER_SHOULD_GET_HERE);
                            break;
                    }

                    // next state
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARLaChArrayEnd:
                if(jsonToken == tokEndArray) 
                {
                    // put out end of array
                    memcpy(&rgchOut[odata[0].cb], szEndArray, sizeof(szEndArray)-1); 
                    odata[0].cb += sizeof(szEndArray)-1;

                    // reload channel members
                    rgStrU32 = rgStrU32LaChannel;
                    cStrU32 = sizeof(rgStrU32LaChannel) / sizeof(STRU32);

                    // set up for what is to come
                    stateEndObject = OSPARTopObjEnd;
                    stateValueSep = OSPARSeparatedNameValue;
                    state = OSPARSkipValueSep;
                }

            /************************************************************************/
            /*    Trigger Parsing                                                   */
            /************************************************************************/
            case OSPARTrgChannelObject:
                if(jsonToken == tokObject)
                {
                    memcpy(&triggerT, &pjcmd.trigger, sizeof(triggerT));
                    triggerT.state.processing = Idle;
                    rgStrU32 = rgStrU32TrgChannel;
                    cStrU32 = sizeof(rgStrU32TrgChannel) / sizeof(STRU32);
                    memcpy(&rgchOut[odata[0].cb], szTrgObject, sizeof(szTrgObject)-1); 
                    odata[0].cb += sizeof(szTrgObject)-1;
                    state = OSPARMemberName;
                }
                break;

            case OSPARTrgCh1:
                if(jsonToken == tokArray)
                {
                    rgStrU32 = rgStrU32Trg;
                    cStrU32 = sizeof(rgStrU32Trg) / sizeof(STRU32);
                    memcpy(&rgchOut[odata[0].cb], szCh1Array, sizeof(szCh1Array)-1); 
                    odata[0].cb += sizeof(szCh1Array)-1;        
                    stateEndArray = OSPARTrgChArrayEnd;
                    stateEndObject = OSPARErrObjectEnd;
                    state = OSPARSkipObject;
                }
                break;

            case OSPARTrgCmd:
                if(jsonToken == tokStringValue)
                {
                    state = (STATE) Uint32FromStr(rgStrU32TrgCmd, sizeof(rgStrU32TrgCmd) / sizeof(STRU32), szToken, cbToken);
                    stateValueSep = OSPARMemberName;
                    fContinue = true;
                }
               break;

            case OSPARTrgSetParm:
                if( jsonToken == tokStringValue)
                {
                    triggerT.state.parsing = JSPARSetParm;
                    stateEndObject = OSPARTrgChSetParmObjectEnd;
                    state = OSPARSkipValueSep;
                }
                break;
                
            case OSPARTrgSource:
                if(jsonToken == tokObject)
                {
                    triggerT.fUseCh2 = (triggerT.idTrigSrc == OSC2_ID);       // just using this as a flag for channel 2, set it to what it is now
                    rgStrU32 = rgStrU32TrgSrc;
                    cStrU32 = sizeof(rgStrU32TrgSrc) / sizeof(STRU32);
                    stateEndObject = OSPARTrgSourceObjectEnd;
                    stateValueSep = OSPARMemberName;
                    state = OSPARMemberName;
                }
                break;

            case OSPARTrgInstrument:
                if( jsonToken == tokStringValue)
                {
                    triggerT.idTrigSrc = (INSTR_ID) Uint32FromStr(rgStrU32TrgInstrumentID, sizeof(rgStrU32TrgInstrumentID) / sizeof(STRU32), szToken, cbToken);
                    triggerT.fUseCh2 = false;   // these default to channel 1
                    if(((OPEN_SCOPE_STATES) triggerT.idTrigSrc) < OSPARSyntaxError)
                    {
                        state = OSPARSkipValueSep;
                    }
                }
                break;

            case OSPARTrgInstrumentChannel:
                if(jsonToken == tokNumber && cbToken == 1)
                {
                    switch(szToken[0])
                    {
                        case '1':
                            state = OSPARSkipValueSep;
                            triggerT.fUseCh2 = false;
                            break;
                        case '2':
                            triggerT.fUseCh2 = true;
                            state = OSPARSkipValueSep;
                            break;
                    }
                }
                break;

            case OSPARTrgType:
                if(jsonToken == tokStringValue)
                {
                    triggerT.triggerType = (TRGTP) Uint32FromStr(rgStrU32TrgType, sizeof(rgStrU32TrgType) / sizeof(STRU32), szToken, cbToken);
                    if((uint32_t) triggerT.triggerType < (uint32_t) OSPARSyntaxError)
                    {
                        state = OSPARSkipValueSep;
                    }
                }       
                break;

            case OSPARTrgLowerThreashold:
                if(jsonToken == tokNumber && cbToken <= 6)
                {
                    char szValue[7];
                    memcpy(szValue, szToken, cbToken);
                    szValue[cbToken] = '\0';
                    triggerT.mvLower = atoi(szValue);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgUpperThreashold:
                if(jsonToken == tokNumber && cbToken <= 6)
                {
                    char szValue[7];
                    memcpy(szValue, szToken, cbToken);
                    szValue[cbToken] = '\0';
                    triggerT.mvHigher = atoi(szValue);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgRisingEdge:
                if(jsonToken == tokNumber && cbToken <= 4)
                {
                    char szValue[7];
                    memcpy(szValue, szToken, cbToken);
                    szValue[cbToken] = '\0';
                    triggerT.posEdge = atoi(szValue);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgFallingEdge:
                if(jsonToken == tokNumber && cbToken <= 4)
                {
                    char szValue[7];
                    memcpy(szValue, szToken, cbToken);
                    szValue[cbToken] = '\0';
                    triggerT.negEdge = atoi(szValue);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgSourceObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    if(triggerT.idTrigSrc == OSC1_ID && triggerT.fUseCh2)
                    {
                        triggerT.idTrigSrc = OSC2_ID;
                    }

                    // opps, specifying channel 2 on an instrument with now channel 2
                    else if(triggerT.fUseCh2)
                    {
                        triggerT.state.processing = InvalidChannel;
                    }

                    // set up for the next state
                    rgStrU32 = rgStrU32Trg;
                    cStrU32 = sizeof(rgStrU32Trg) / sizeof(STRU32);
                    stateEndObject = OSPARTrgChSetParmObjectEnd;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgTargets:
                if(jsonToken == tokObject)
                {
                    // clear the target array
                    memset(triggerT.rgtte, 0, sizeof(triggerT.rgtte));
                    triggerT.cTargets = 0;
                    triggerT.cRun = 0;

                    rgStrU32 = rgStrU32TrgTargets;
                    cStrU32 = sizeof(rgStrU32TrgTargets) / sizeof(STRU32);
                    stateEndObject = OSPARTrgTargetsObjectEnd;
                    stateValueSep = OSPARMemberName;
                    state = OSPARMemberName;
                }
                break;

            case OSPARTrgTargetOsc:
                if(jsonToken == tokArray)
                {
                    // next state
                    stateValueSep = OSPARTrgTargetOscCh;
                    stateEndArray = OSPARTrgTargetEndChArray;
                    state = OSPARTrgTargetOscCh;
                }
                break;

            case OSPARTrgTargetOscCh:
                if(jsonToken == tokNumber && cbToken == 1)
                {
                    if(szToken[0] == '1')
                    {
                        triggerT.rgtte[0].instrID = OSC1_ID;
                        triggerT.rgtte[0].pstate = &pjcmd.ioscCh1.state;
                        triggerT.rgtte[0].pLockState = &pjcmd.ioscCh1.buffLock;
                        state = OSPARSkipValueSep;
                    }
                    else if(szToken[0] == '2') 
                    {
                        triggerT.rgtte[1].instrID = OSC2_ID;
                        triggerT.rgtte[1].pstate = &pjcmd.ioscCh2.state;
                        triggerT.rgtte[1].pLockState = &pjcmd.ioscCh2.buffLock;
                        state = OSPARSkipValueSep;
                    }
                    else
                    {
                        triggerT.state.processing = InvalidChannel;
                    }
                }
                break;

            case OSPARTrgTargetLa:
                if(jsonToken == tokArray)
                {
                    stateValueSep = OSPARTrgTargetLaCh;
                    stateEndArray = OSPARTrgTargetEndChArray;
                    state = OSPARTrgTargetLaCh;
                }
                break;

            case OSPARTrgTargetLaCh:
                if(jsonToken == tokNumber && cbToken == 1)
                {
                    if(szToken[0] == '1')
                    {
                        triggerT.rgtte[2].instrID = LOGIC1_ID;
                        triggerT.rgtte[2].pstate = &pjcmd.ila.state;;
                        triggerT.rgtte[2].pLockState = &pjcmd.ila.buffLock;;
                        state = OSPARSkipValueSep;
                    }
                }
                break;

            case OSPARTrgTargetEndChArray:
                if(jsonToken == tokEndArray)
                {
                    stateEndArray = OSPARTrgChArrayEnd;
                    stateEndObject = OSPARTrgTargetsObjectEnd;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgTargetsObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    static const uint32_t cPTargets = sizeof(triggerT.rgtte)/sizeof(triggerT.rgtte[0]);
                    uint32_t i;
                    TTE  rgtte[cPTargets];

                    memcpy(rgtte, triggerT.rgtte, sizeof(triggerT.rgtte));

                    for(i=0, triggerT.cTargets=0; i<cPTargets; i++)
                    {
                        if(rgtte[i].instrID != NULL_ID)
                        {
                            triggerT.rgtte[triggerT.cTargets++] = rgtte[i];
                        }
                    }

                    rgStrU32 = rgStrU32Trg;
                    cStrU32 = sizeof(rgStrU32Trg) / sizeof(STRU32);
                    stateEndArray = OSPARTrgChArrayEnd;
                    stateEndObject = OSPARTrgChSetParmObjectEnd;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgChSetParmObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    uint32_t i;

                    // put our the status code (but not the code itself)
                    memcpy(&rgchOut[odata[0].cb], szSetParmStatusCode, sizeof(szSetParmStatusCode)-1); 
                    odata[0].cb += sizeof(szSetParmStatusCode)-1;

                    // do some error checking
                    if((triggerT.idTrigSrc == OSC1_ID || triggerT.idTrigSrc == OSC2_ID) && !(triggerT.triggerType == TRGTPRising || triggerT.triggerType == TRGTPFalling))
                    {
                        // Put out the error status
                        utoa(ValueOutOfRange, &rgchOut[odata[0].cb], 10);
                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                    }

                    // see if the trigger can be assigned
                    else if(triggerT.state.processing == Idle && (pjcmd.trigger.state.processing == Idle || pjcmd.trigger.state.processing == Triggered))
                    {

                        // make sure our src instrument gets run by either being a target, or one past the targets in the run
                        triggerT.cRun = triggerT.cTargets;
                        for(i=0; i<triggerT.cTargets; i++) if(triggerT.idTrigSrc == triggerT.rgtte[i].instrID) break;
                        if(i == triggerT.cTargets && triggerT.idTrigSrc != FORCE_TRG_ID) 
                        {
                            triggerT.rgtte[triggerT.cTargets].instrID = triggerT.idTrigSrc;
                            switch(triggerT.idTrigSrc)
                            {
                                case OSC1_ID:
                                    triggerT.rgtte[triggerT.cTargets].pstate = &pjcmd.ioscCh1.state;
                                    triggerT.rgtte[triggerT.cTargets].pLockState = &pjcmd.ioscCh1.buffLock;

                                    // we must set up some sort of parameters to run
                                    pjcmd.ioscCh1.bidx.msps     = MAXmSAMPLEFREQ;
                                    pjcmd.ioscCh1.bidx.psDelay  = 0;        
                                    pjcmd.ioscCh1.bidx.cBuff    = AINOVERSIZE;         
                                    CalculateBufferIndexes(&pjcmd.ioscCh1.bidx);
                                    break;

                                case OSC2_ID:
                                    triggerT.rgtte[triggerT.cTargets].pstate = &pjcmd.ioscCh2.state;
                                    triggerT.rgtte[triggerT.cTargets].pLockState = &pjcmd.ioscCh2.buffLock;

                                    // we must set up some sort of parameters to run
                                    pjcmd.ioscCh2.bidx.msps     = MAXmSAMPLEFREQ;
                                    pjcmd.ioscCh2.bidx.psDelay  = 0;        
                                    pjcmd.ioscCh2.bidx.cBuff    = AINOVERSIZE;         
                                    CalculateBufferIndexes(&pjcmd.ioscCh2.bidx);
                                    break;

                                case LOGIC1_ID:
                                    triggerT.rgtte[triggerT.cTargets].pstate = &pjcmd.ila.state;
                                    triggerT.rgtte[triggerT.cTargets].pLockState = &pjcmd.ila.buffLock;

                                    // we must set up some sort of parameters to run
                                    pjcmd.ila.bidx.msps     = LAMAXmSPS;
                                    pjcmd.ila.bidx.psDelay  = 0;        
                                    pjcmd.ila.bidx.cBuff    = LAOVERSIZE;         
                                    CalculateBufferIndexes(&pjcmd.ila.bidx);
                                    break;
 
                                // these can only be triggers, but we still have to get them running
                                default:
                                    ASSERT(NEVER_SHOULD_GET_HERE);
                                    break;
                            }

                            // set the run count
                            triggerT.cRun++;
                        }

                        // move temp to real
                        // this will put the trigger state back to Idle
                        memcpy(&pjcmd.trigger, &triggerT, sizeof(pjcmd.trigger));
                        memset(&triggerT, 0, sizeof(pjcmd.trigger));

                        // status is 0, it worked.
                        rgchOut[odata[0].cb++] = '0';
                    }

                    // error during parsing
                    else if(triggerT.state.processing != Idle)
                    {
                        // Put out the error status
                        utoa(triggerT.state.processing, &rgchOut[odata[0].cb], 10);
                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                    }

                    // can't update the trigger right now
                    else
                    {
                        // Put out the error status
                        utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                    }
          
                    // put out the wait
                    memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                    odata[0].cb += sizeof(szWait0)-1;

                    stateEndObject = OSPARErrObjectEnd;
                    stateEndArray = OSPARTrgChArrayEnd;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgRun:
                if(jsonToken == tokStringValue)
                {                   
                    stateEndObject = OSPARTrgChRunObjectEnd;
                    state = OSPARSkipValueSep;
                }
                break;
                              
            case OSPARTrgChRunObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    // put out the command and status
                    memcpy(&rgchOut[odata[0].cb], szTrgRunStatus, sizeof(szTrgRunStatus)-1); 
                    odata[0].cb += sizeof(szTrgRunStatus)-1;

                    // Put out the error status
                    utoa(Unimplemented, &rgchOut[odata[0].cb], 10);
                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                        
                    // put out the wait
                    memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 

                    stateEndObject = OSPARErrObjectEnd;
                    stateEndArray = OSPARTrgChArrayEnd;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgGetCurrentState:
                if( jsonToken == tokStringValue)
                {
                    triggerT.state.parsing = JSPARTrgGetCurrentState;
                    stateEndObject = OSPARTrgChGetCurrentStateObjectEnd;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgChGetCurrentStateObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    // put out the command and status
                    memcpy(&rgchOut[odata[0].cb], szTrgGetCurrentStateStatus, sizeof(szTrgGetCurrentStateStatus)-1); 
                    odata[0].cb += sizeof(szTrgGetCurrentStateStatus)-1;

                    // put out the status code
                    rgchOut[odata[0].cb++] = '0';

                    // Put out the acqCount
                    memcpy(&rgchOut[odata[0].cb], szAcqCount, sizeof(szAcqCount)-1); 
                    odata[0].cb += sizeof(szAcqCount)-1;
                    utoa(pjcmd.trigger.acqCount, &rgchOut[odata[0].cb], 10);
                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                    // put out the state
                    memcpy(&rgchOut[odata[0].cb], szState, sizeof(szState)-1); 
                    odata[0].cb += sizeof(szState)-1;

                    // put out the trigger state
                    switch(pjcmd.trigger.state.processing)
                    {
                        case Idle:
                        case Stopped:
                            // put out idle
                            strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Idle]); 
                            break;

                        case Triggered:
                            // put out triggered
                            strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Triggered]); 
                            break;

                        case Run:
                        case Armed:
                            if(T9CONbits.ON)
                            {
                                // put out acquiring
                                strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Acquiring]); 
                            }

                            // otherwise we are armed or in the process of being armed
                            else
                            {
                                // put out armed
                                strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Armed]); 
                            }
                            break;

                        // busy doing something else
                        default:
                            strcpy(&rgchOut[odata[0].cb], rgInstrumentStates[Busy]); 
                            break;
                    }
                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                    // put out the source, if we have one
                    if(pjcmd.trigger.idTrigSrc != NULL_ID)
                    {
                        // put out the source
                        memcpy(&rgchOut[odata[0].cb], szTrgSource, sizeof(szTrgSource)-1); 
                        odata[0].cb += sizeof(szTrgSource)-1;

                        // put out the instrument
                        memcpy(&rgchOut[odata[0].cb], szInstrument, sizeof(szInstrument)-1); 
                        odata[0].cb += sizeof(szInstrument)-1;

                        // put out source instrument
                        switch(pjcmd.trigger.idTrigSrc)
                        {
                            case OSC1_ID:
                            case OSC2_ID:

                                // put out the instrument
                                memcpy(&rgchOut[odata[0].cb], szOsc, sizeof(szOsc)-1); 
                                odata[0].cb += sizeof(szOsc)-1;

                                // put out the channel
                                memcpy(&rgchOut[odata[0].cb], szChannel, sizeof(szChannel)-1); 
                                odata[0].cb += sizeof(szChannel)-1;
                                if(pjcmd.trigger.idTrigSrc == OSC2_ID) rgchOut[odata[0].cb++] = '2';
                                else rgchOut[odata[0].cb++] = '1';

                                // put out the Threashold type
                                memcpy(&rgchOut[odata[0].cb], szType, sizeof(szType)-1); 
                                odata[0].cb += sizeof(szType)-1;
                                strcpy(&rgchOut[odata[0].cb], rgThresholdType[pjcmd.trigger.triggerType]); 
                                odata[0].cb += strlen(rgThresholdType[pjcmd.trigger.triggerType]);

                                // put out lower threshold
                                memcpy(&rgchOut[odata[0].cb], szLowerThreshold, sizeof(szLowerThreshold)-1); 
                                odata[0].cb += sizeof(szLowerThreshold)-1;
                                itoa(pjcmd.trigger.mvLower, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // put out upper threshold
                                memcpy(&rgchOut[odata[0].cb], szUpperThreshold, sizeof(szUpperThreshold)-1); 
                                odata[0].cb += sizeof(szUpperThreshold)-1;
                                itoa(pjcmd.trigger.mvHigher, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                break;

                            case LOGIC1_ID:

                                // put out the instrument
                                memcpy(&rgchOut[odata[0].cb], szLa, sizeof(szLa)-1); 
                                odata[0].cb += sizeof(szLa)-1;

                                // put out the channel
                                memcpy(&rgchOut[odata[0].cb], szChannel, sizeof(szChannel)-1); 
                                odata[0].cb += sizeof(szChannel)-1;
                                rgchOut[odata[0].cb++] = '1';

                                // put out Rising Edge mask
                                memcpy(&rgchOut[odata[0].cb], szRisingEdge, sizeof(szRisingEdge)-1); 
                                odata[0].cb += sizeof(szRisingEdge)-1;
                                itoa(pjcmd.trigger.posEdge, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                                // put out Falling Edge mask
                                memcpy(&rgchOut[odata[0].cb], szFallingEdge, sizeof(szFallingEdge)-1); 
                                odata[0].cb += sizeof(szFallingEdge)-1;
                                itoa(pjcmd.trigger.negEdge, &rgchOut[odata[0].cb], 10);
                                odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                break;

                            case FORCE_TRG_ID:

                                // put out the instrument
                                memcpy(&rgchOut[odata[0].cb], szForce, sizeof(szForce)-1); 
                                odata[0].cb += sizeof(szForce)-1;
                                break;

                            default:
                                ASSERT(NEVER_SHOULD_GET_HERE);          
                                break;

                        }

                        // end of source
                        rgchOut[odata[0].cb++] = '}';
                    }

                    // put out the target, if we have one
                    if(pjcmd.trigger.cTargets > 0)
                    {
                        uint32_t bitTargets = 0;
                        uint32_t i;

                        // put out the Targets
                        memcpy(&rgchOut[odata[0].cb], szTrgTargets, sizeof(szTrgTargets)-1); 
                        odata[0].cb += sizeof(szTrgTargets)-1;

                        for(i=0; i<pjcmd.trigger.cTargets; i++)
                        {
                            switch(pjcmd.trigger.rgtte[i].instrID)
                            {
                                case OSC1_ID:
                                    bitTargets |= 0x1;
                                    break;

                                case OSC2_ID:
                                    bitTargets |= 0x2;
                                    break;

                                case LOGIC1_ID:
                                    bitTargets |= 0x10;
                                    break;

                                default:
                                    break;
                            }
                        }

                        // if there is an OSC as a target
                        if((bitTargets & 0x3) != 0)
                        {
                            // put out the OSC
                            memcpy(&rgchOut[odata[0].cb], szOsc, sizeof(szOsc)-1); 
                            odata[0].cb += sizeof(szOsc)-1;
                            memcpy(&rgchOut[odata[0].cb], szStartChArray, sizeof(szStartChArray)-1); 
                            odata[0].cb += sizeof(szStartChArray)-1;

                            if((bitTargets & 0x1) != 0)
                            {
                                rgchOut[odata[0].cb++] = '1';
                                rgchOut[odata[0].cb++] = ',';
                            }

                            if((bitTargets & 0x2) != 0)
                            {
                                rgchOut[odata[0].cb++] = '2';
                                rgchOut[odata[0].cb++] = ',';
                            }

                            odata[0].cb--;
                            rgchOut[odata[0].cb++] = ']';

                            if((bitTargets & 0x10) != 0) rgchOut[odata[0].cb++] = ',';
                        }

                        // if there is an LA as a target
                        if((bitTargets & 0x10) != 0)
                        {
                            // put out the LA
                            memcpy(&rgchOut[odata[0].cb], szLa, sizeof(szLa)-1); 
                            odata[0].cb += sizeof(szLa)-1;
                            memcpy(&rgchOut[odata[0].cb], szStartChArray, sizeof(szStartChArray)-1); 
                            odata[0].cb += sizeof(szStartChArray)-1;

                            rgchOut[odata[0].cb++] = '1';
                            rgchOut[odata[0].cb++] = ']';
                        }

                        // end of targets
                        rgchOut[odata[0].cb++] = '}';
                    }


                // put out the wait 0
                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                odata[0].cb += sizeof(szWait0)-1;  

                stateEndObject = OSPARErrObjectEnd;
                stateEndArray = OSPARTrgChArrayEnd;
                stateValueSep = OSPARSeparatedObject;
                state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgSingle:
                if(jsonToken == tokStringValue)
                {                   
                    stateEndObject = OSPARTrgChSingleObjectEnd;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgChSingleObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    bool fLANeeded  = false;
                    uint32_t i      = 0;

                    // put out the command and status
                    memcpy(&rgchOut[odata[0].cb], szTrgSingleStatus, sizeof(szTrgSingleStatus)-1); 
                    odata[0].cb += sizeof(szTrgSingleStatus)-1;

                    if(pjcmd.trigger.idTrigSrc == LOGIC1_ID) fLANeeded = true;
                    for(i=0; i<pjcmd.trigger.cTargets; i++) if(pjcmd.trigger.rgtte[i].instrID == LOGIC1_ID) fLANeeded = true;
                    
                    if( IsTrgIdle() && (!fLANeeded || IsAWGIdle()) &&
                        (pjcmd.trigger.state.parsing == JSPARSetParm)      
                        )
                    {
                        pjcmd.trigger.state.processing = Queued;

                        // make sure the AWG doesn't try to run
                        if(fLANeeded) LockLA();

                        // put out the no error status
                        rgchOut[odata[0].cb++] = '0';

                        // up our acq count
                        pjcmd.trigger.acqCount++;

                        // Put out the acqCount
                        memcpy(&rgchOut[odata[0].cb], szAcqCount, sizeof(szAcqCount)-1); 
                        odata[0].cb += sizeof(szAcqCount)-1;
                        utoa(pjcmd.trigger.acqCount, &rgchOut[odata[0].cb], 10);
                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                        // put out the wait
                        memcpy(&rgchOut[odata[0].cb], szWaitUntil, sizeof(szWaitUntil)-1); 
                        odata[0].cb += sizeof(szWaitUntil)-1;
                    }
                    else
                    {
                        // Put out the error status
                        utoa(InstrumentInUse, &rgchOut[odata[0].cb], 10);
                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                        
                        // put out the wait
                        memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                        odata[0].cb += sizeof(szWait0)-1;
                   }

                stateEndObject = OSPARErrObjectEnd;
                stateEndArray = OSPARTrgChArrayEnd;
                stateValueSep = OSPARSeparatedObject;
                state = OSPARSkipValueSep;
                }
                break;

                
            case OSPARTrgStop:
                if(jsonToken == tokStringValue)
                {                   
                    // we can always stop, never an error on that
                    // put out the stop command
                    memcpy(&rgchOut[odata[0].cb], szTrgStop, sizeof(szTrgStop)-1); 
                    odata[0].cb += sizeof(szTrgStop)-1;
                    
                    stateEndObject = OSPARTrgChStopObjectEnd;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgChStopObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    uint32_t i = 0;

                    // if it is actuall running, then turn it off
                    if( pjcmd.trigger.state.processing == Queued    ||
                        pjcmd.trigger.state.processing == Run       ||
                        pjcmd.trigger.state.processing == Armed     )
                    {

                        // turn the trigger OFF
                        TRGAbort();

                        // since we did not go to completion, we can not say we are triggered
                        // that is, the sample data is corrupt. But the scope value are set up
                        // so we can say the parameters are set, thus waiting.
                        for(i=0; i<pjcmd.trigger.cRun; i++) 
                        {
                            // who is the trigger source
                            switch(pjcmd.trigger.rgtte[i].instrID)
                            {
                                case OSC1_ID:
                                    pjcmd.ioscCh1.state.processing = Waiting;
                                    pjcmd.ioscCh1.buffLock = LOCKAvailable;
                                    OSCReset(rgInstr[OSC1_ID]);
                                    break;

                                case OSC2_ID:
                                    pjcmd.ioscCh2.state.processing = Waiting;
                                    pjcmd.ioscCh2.buffLock = LOCKAvailable;
                                    OSCReset(rgInstr[OSC2_ID]);
                                    break;

                                case LOGIC1_ID:
                                    pjcmd.ila.state.processing = Waiting;
                                    pjcmd.ila.buffLock = LOCKAvailable;
                                    LAReset(rgInstr[LOGIC1_ID]);
                                    break;

                                default:
                                    ASSERT(NEVER_SHOULD_GET_HERE);
                                    break;
                            }
                        }

                        // put the trigger back to an idle state
                        pjcmd.trigger.state.processing = Idle;
                        pjcmd.trigger.state.parsing = JSPARSetParm;  // we did set the parameters already
                    }

                    stateEndObject = OSPARErrObjectEnd;
                    stateEndArray = OSPARTrgChArrayEnd;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgForceTrigger:
                if(jsonToken == tokStringValue)
                {                   
                    // we can always stop, never an error on that
                    // put out the stop command
                    memcpy(&rgchOut[odata[0].cb], szTrgForceTrigger, sizeof(szTrgForceTrigger)-1); 
                    odata[0].cb += sizeof(szTrgForceTrigger)-1;
                    
                    stateEndObject = OSPARTrgChForceTriggerEnd;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgChForceTriggerEnd:
                if(jsonToken == tokEndObject)
                {
                    // see if we are armed
                    if(TRGForce())
                    {
                        // put out the status code
                        rgchOut[odata[0].cb++] = '0';

                        // put out the wait until it is done
                        memcpy(&rgchOut[odata[0].cb], szWaitUntil, sizeof(szWaitUntil)-1); 
                        odata[0].cb += sizeof(szWaitUntil)-1;
                    }

                    // if we are leading up to an armed
                    else if(pjcmd.trigger.state.processing == Queued || pjcmd.trigger.state.processing == Run)
                    {
                            // Put out the error status
                            utoa(InstrumentNotArmedYet, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // put out the wait0
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;
                    }
                    else
                    {
                            // Put out the error status
                            utoa(InstrumentNotArmed, &rgchOut[odata[0].cb], 10);
                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                            // put out the wait0
                            memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                            odata[0].cb += sizeof(szWait0)-1;
                    }
                       
                    stateEndObject = OSPARErrObjectEnd;
                    stateEndArray = OSPARTrgChArrayEnd;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTrgChArrayEnd:
                if(jsonToken == tokEndArray) 
                {
                    // put out end of array
                    memcpy(&rgchOut[odata[0].cb], szEndArray, sizeof(szEndArray)-1); 
                    odata[0].cb += sizeof(szEndArray)-1;

                    // reload channel members
                    rgStrU32 = rgStrU32TrgChannel;
                    cStrU32 = sizeof(rgStrU32TrgChannel) / sizeof(STRU32);

                    // set up for what is to come
                    stateEndObject = OSPARTopObjEnd;
                    stateValueSep = OSPARSeparatedNameValue;
                    state = OSPARSkipValueSep;
                }

            /************************************************************************/
            /*    GPIO                                                              */
            /************************************************************************/
            case OSPARGpioChannelObject:
                if(jsonToken == tokObject)
                {
                    pjcmd.igpio.iPin = 0;
                    rgStrU32 = rgStrU32GpioChannel;
                    cStrU32 = sizeof(rgStrU32GpioChannel) / sizeof(STRU32);
                    memcpy(&rgchOut[odata[0].cb], szGpioObject, sizeof(szGpioObject)-1); 
                    odata[0].cb += sizeof(szGpioObject)-1;
                    state = OSPARMemberName;
                }
                break;

            case OSPARGpioCh10:
                pjcmd.igpio.iPin++;
            case OSPARGpioCh9:
                pjcmd.igpio.iPin++;
            case OSPARGpioCh8:
                pjcmd.igpio.iPin++;
            case OSPARGpioCh7:
                pjcmd.igpio.iPin++;
            case OSPARGpioCh6:
                pjcmd.igpio.iPin++;
            case OSPARGpioCh5:
                pjcmd.igpio.iPin++;
            case OSPARGpioCh4:
                pjcmd.igpio.iPin++;
            case OSPARGpioCh3:
                pjcmd.igpio.iPin++;
            case OSPARGpioCh2:
                pjcmd.igpio.iPin++;
            case OSPARGpioCh1:
                if(jsonToken == tokArray)
                {
                    // put the channel number out
                    rgchOut[odata[0].cb++] = '"';
                    itoa((pjcmd.igpio.iPin+1), &rgchOut[odata[0].cb], 10);
                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                    rgchOut[odata[0].cb++] = '"';

                    // the rest of the channel array
                    memcpy(&rgchOut[odata[0].cb], szStartChArray, sizeof(szStartChArray)-1); 
                    odata[0].cb += sizeof(szStartChArray)-1;

                    // now get the gpio token
                    rgStrU32 = rgStrU32Gpio;
                    cStrU32 = sizeof(rgStrU32Gpio) / sizeof(STRU32);
                    stateEndArray = OSPARGpioChEnd;
                    stateEndObject = OSPARGpioObjectEnd;
                    state = OSPARSkipObject;
                }
                break;

            case OSPARGpioCmd:
                if(jsonToken == tokStringValue)
                {
                    state = (STATE) Uint32FromStr(rgStrU32GpioCmd, sizeof(rgStrU32GpioCmd) / sizeof(STRU32), szToken, cbToken);
                    stateValueSep = OSPARMemberName;
                    fContinue = true;
                }
               break;

            case OSPARGpioDirection:
                if(jsonToken == tokStringValue)
                {
                    pjcmd.igpio.pinState = (GPIOSTATE) Uint32FromStr(rgStrU32GpioDirection, sizeof(rgStrU32GpioDirection) / sizeof(STRU32), szToken, cbToken, (uint32_t) gpioNone);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARGpioValue:
                if(jsonToken == tokNumber && cbToken == 1)
                {
                    switch(szToken[0])
                    {
                    case '0':
                        pjcmd.igpio.value = 0;
                        break;

                    case '1':
                        pjcmd.igpio.value = 1;
                        break;

                    default:
                        pjcmd.igpio.value = 2;
                        break;
                    }
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARGpioSetParameters:
                if(jsonToken == tokStringValue)
                {
                    pjcmd.igpio.parsing = JSPARSetParm;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARGpioRead:
                if(jsonToken == tokStringValue)
                {
                    pjcmd.igpio.parsing = JSPARGpioRead;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARGpioWrite:
                if(jsonToken == tokStringValue)
                {
                    pjcmd.igpio.parsing = JSPARGpioWrite;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARGpioObjectEnd:
                if(jsonToken == tokEndObject)
                {
                    // assume we pass this state
                    stateEndArray = OSPARGpioChEnd;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;

                    switch(pjcmd.igpio.parsing)
                    {
                        case JSPARSetParm:
                            {                                                                        
                                GPIOSTATE   gpioState   = pjcmd.igpio.pin[pjcmd.igpio.iPin].gpioState;
                                PORTCH *    pPort       = pjcmd.igpio.pin[pjcmd.igpio.iPin].pPort;
                                uint32_t    pinMask     = pjcmd.igpio.pin[pjcmd.igpio.iPin].pinMask;

                                memcpy(&rgchOut[odata[0].cb], szSetParmStatusCode, sizeof(szSetParmStatusCode)-1); 
                                odata[0].cb += sizeof(szSetParmStatusCode)-1;

                                // assume we have good status
                                rgchOut[odata[0].cb++] = '0';

                                // assume we can change the PIN state
                                pjcmd.igpio.pin[pjcmd.igpio.iPin].gpioState = pjcmd.igpio.pinState;

                                switch(pjcmd.igpio.pinState)
                                {
                                    case gpioTriState:
                                    case gpioInput:
                                        pPort->TRISSET = pinMask;
                                        pPort->CNPDCLR = pinMask;
                                        pPort->CNPUCLR = pinMask;
                                        break;

                                    case gpioInputPullDown:
                                        pPort->TRISSET = pinMask;
                                        pPort->CNPDSET = pinMask;
                                        pPort->CNPUCLR = pinMask;
                                        break;

                                    case gpioInputPullUp:
                                        pPort->TRISSET = pinMask;
                                        pPort->CNPDCLR = pinMask;
                                        pPort->CNPUSET = pinMask;
                                        break;

                                    case gpioOutput:
                                        pPort->CNPDCLR = pinMask;
                                        pPort->CNPUCLR = pinMask;
                                        pPort->TRISCLR = pinMask;
                                        break;

                                    default:

                                        // restore the pin state
                                        pjcmd.igpio.pin[pjcmd.igpio.iPin].gpioState = gpioState;

                                        // back out the 0 status code
                                        odata[0].cb--;

                                        // put in the error code
                                        itoa((int32_t) GPIOInvalidDirection, &rgchOut[odata[0].cb], 10);
                                        odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                        break;
                                }

                                // write out the wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }
                            break;

                        case JSPARGpioRead:
                            {
                                GPIOSTATE           gpioState   = pjcmd.igpio.pin[pjcmd.igpio.iPin].gpioState;
                                char const * const  szDirection = rgszGpioDirections[pjcmd.igpio.pin[pjcmd.igpio.iPin].gpioState];
                                uint32_t            cbDirection =  strlen(szDirection);

                                memcpy(&rgchOut[odata[0].cb], szGpioReadStatusCode, sizeof(szGpioReadStatusCode)-1); 
                                odata[0].cb += sizeof(szGpioReadStatusCode)-1;

                                // if this is an input
                                if(gpioState == gpioInput || gpioState == gpioInputPullDown || gpioState == gpioInputPullUp)
                                {
                                    rgchOut[odata[0].cb++] = '0';
                                }

                                // otherwise we are getting a direction missmatch
                                else
                                {
                                    // put out a warning that the pin direction is wrong
                                    itoa((int32_t) GPIODirectionMissMatch, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                }

                                // put out the direction
                                memcpy(&rgchOut[odata[0].cb], szGpioDirection, sizeof(szGpioDirection)-1); 
                                odata[0].cb += sizeof(szGpioDirection)-1;
                                memcpy(&rgchOut[odata[0].cb], szDirection, cbDirection); 
                                odata[0].cb += cbDirection;
                                
                                // put out the value
                                if( (pjcmd.igpio.pin[pjcmd.igpio.iPin].pPort->PORT & pjcmd.igpio.pin[pjcmd.igpio.iPin].pinMask) == 0 )
                                {
                                    memcpy(&rgchOut[odata[0].cb], szGpioValue0, sizeof(szGpioValue0)-1); 
                                    odata[0].cb += sizeof(szGpioValue0)-1;      
                                }
                                else
                                {
                                    memcpy(&rgchOut[odata[0].cb], szGpioValue1, sizeof(szGpioValue1)-1); 
                                    odata[0].cb += sizeof(szGpioValue1)-1;      
                                }

                                // write out the wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }
                            break;

                        case JSPARGpioWrite:
                            {
                                memcpy(&rgchOut[odata[0].cb], szGpioWriteStatusCode, sizeof(szGpioWriteStatusCode)-1); 
                                odata[0].cb += sizeof(szGpioWriteStatusCode)-1;

                                // if this is an input
                                if(pjcmd.igpio.pin[pjcmd.igpio.iPin].gpioState == gpioOutput)
                                {
                                    // assume we have good status
                                    rgchOut[odata[0].cb++] = '0';

                                    switch(pjcmd.igpio.value)
                                    {
                                        case 0:
                                            pjcmd.igpio.pin[pjcmd.igpio.iPin].pPort->LATCLR = pjcmd.igpio.pin[pjcmd.igpio.iPin].pinMask;
                                            break;

                                        case 1:
                                            pjcmd.igpio.pin[pjcmd.igpio.iPin].pPort->LATSET = pjcmd.igpio.pin[pjcmd.igpio.iPin].pinMask;
                                            break;

                                        default:

                                            // back out the 0 status code
                                            odata[0].cb--;

                                            // put in the error code
                                            itoa((int32_t) ValueOutOfRange, &rgchOut[odata[0].cb], 10);
                                            odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                            break;
                                    }
                                }

                                // otherwise we are getting a direction missmatch
                                else
                                {
                                    // put out a warning that the pin direction is wrong
                                    itoa((int32_t) GPIODirectionMissMatch, &rgchOut[odata[0].cb], 10);
                                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);
                                }

                                // write out the wait 0
                                memcpy(&rgchOut[odata[0].cb], szWait0, sizeof(szWait0)-1); 
                                odata[0].cb += sizeof(szWait0)-1;
                            }
                            break;

                        default:
                            state = OSPARSyntaxError;
                            fContinue = true;
                            break;
                    }
                }
                break;

            case OSPARGpioChEnd:
                if(jsonToken == tokEndArray) 
                {
                    // put out end of array
                    memcpy(&rgchOut[odata[0].cb], szEndArray, sizeof(szEndArray)-1); 
                    odata[0].cb += sizeof(szEndArray)-1;
                                
                    // needs to be reset for next multi command
                    pjcmd.igpio.iPin = 0;

                    // reload channel members
                    rgStrU32 = rgStrU32GpioChannel;
                    cStrU32 = sizeof(rgStrU32GpioChannel) / sizeof(STRU32);

                    // set up for what is to come
                    stateEndObject = OSPARTopObjEnd;
                    stateValueSep = OSPARSeparatedNameValue;
                    state = OSPARSkipValueSep;
                }
                break;

            /************************************************************************/
            //    Manufacturing Test parsing                                                      
            //
            //  Test parsing is an undocumented JSON command that is used strictly
            //  for manufactuing testing of the device.
            //
            //  command format:
            //
            //  {
            //      "test":[
            //          {
            //              "command":"run",
            //              "testNbr": <number>
            //          }
            //      ]
            //  }
            //
            //
            //  reponse format:
            //
            //  {
            //      "test":[
            //          {
            //              "command":"run",
            //              "statusCode": 0,
            //              "wait": 0,
            //              "returnNbr": <number>
            //          }
            //      ]
            //  }
            //
            //
            /************************************************************************/  
            case OSPARTestArray:
                if(jsonToken == tokArray)
                {
                    memcpy(&rgchOut[odata[0].cb], szTest, sizeof(szTest)-1); 
                    odata[0].cb += sizeof(szTest)-1;

                    // root command strings for test
                    rgStrU32 = rgStrU32Test;
                    cStrU32 = sizeof(rgStrU32Test) / sizeof(STRU32);

                    state = OSPARSkipObject;
                }
                break;  

            case OSPARTestCmd:
                if(jsonToken == tokStringValue)
                {
                    state   = (STATE) Uint32FromStr(rgStrU32TestCmd, sizeof(rgStrU32TestCmd) / sizeof(STRU32), szToken, cbToken);
                    fContinue = true;
                }
                break;

            case OSPARTestNbr:
                if(jsonToken == tokNumber)
                {
                    char szT[32];
                    memcpy(szT, szToken, cbToken);
                    szT[cbToken] = '\0';
                    pjcmd.iMfgTest.testNbr = atoi(szT);
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTestRun:
                if(jsonToken == tokStringValue)
                {
                    memcpy(&rgchOut[odata[0].cb], szTestRun, sizeof(szTestRun)-1); 
                    odata[0].cb += sizeof(szTestRun)-1;

                    // get next member name
                    stateEndObject = OSPARTestEndObject;
                    stateValueSep = OSPARMemberName;
                    state = OSPARSkipValueSep;
                }
                break;

            case OSPARTestEndObject:
                if(jsonToken == tokEndObject)
                {

                    // Run the manufacturing test and put out the result
                    utoa(MfgTest(pjcmd.iMfgTest.testNbr), &rgchOut[odata[0].cb], 10);
                    odata[0].cb += strlen(&rgchOut[odata[0].cb]);

                    // close out the command
                    rgchOut[odata[0].cb++] ='}';

                    // get next member name
                    stateEndArray = OSPARDeviceEndArray;
                    stateValueSep = OSPARSeparatedObject;
                    state = OSPARSkipValueSep;
                }
                break;

        }

        // go to the next state
        curState = state;

    // stay in the state machine if requested
    } while(fContinue || state == OSPARSyntaxError);

    return(Idle);
}


#else  // just to lex

uint32_t ctokens = 0;
uint32_t cError = 0;
uint32_t cEnd = 0;

STATE OSPAR::ParseToken(char const * const szToken, uint32_t cbToken, JSONTOKEN jsonToken)
{
    switch(jsonToken)
    {
        case tokLexingError:
        case tokJSONSyntaxError:
            cError++;
            break;

        case tokEndOfJSON:
            ctokens = 0;
            cError = 0;
            break;

        default:
            ctokens++;
            break;
    }

    return(Idle);   
}
#endif


