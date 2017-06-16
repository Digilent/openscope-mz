/************************************************************************/
/*                                                                      */
/*    main.cpp                                                          */
/*                                                                      */
/*    Main function for the OpenScope Application                       */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    2/17/2016 (KeithV): Created                                        */
/************************************************************************/
#include <OpenScope.h>
#include <math.h>

//#define DC1VOLTAGE (1200)
//#define DC2VOLTAGE (-DC1VOLTAGE)

STATE MState = MSysInit;

int main(__attribute__((unused)) int argc, __attribute__((unused)) char** argv) {
    STATE retState = Idle;
    static uint32_t tStart = 0;
    
    InitInstruments();
    
    // We need to wait for the Arduino IDE to open the COM port 
    // if the Serial Monitor is open after the reset
    tStart = SYSGetMilliSecond();
    while(SYSGetMilliSecond() - tStart < 500);
    
    Serial.begin(SERIALBAUDRATE);
    Serial.print("OpenScope v");
    Serial.println(szProgVersion);
    Serial.println("Written by: Keith Vogel, Digilent Inc.");
    Serial.println("Copyright 2016 Digilent Inc.");
    Serial.println();
    
    while (1) {
        switch (MState) {

            // inits the MCU
            case MSysInit:
                if (CFGSysInit() == Idle) {
                    tStart = SYSGetMilliSecond();
                    MState = MSysVoltages;
                }
                break;

            // read the power supply and reference voltages
            case MSysVoltages:
                if (InitSystemVoltages() == Idle) {
                    MState = MHeaders;
                }
                break;

            case MHeaders:
                Serial.print("MRF24 Info -- ");
                Serial.print("DeviceType: 0x");
                Serial.print((int) myMRFDeviceInfo.deviceType, 16);
                Serial.print(" Rom Version: 0x");
                Serial.print((int) myMRFDeviceInfo.romVersion, 16);
                Serial.print(" Patch Version: 0x");
                Serial.println((int) myMRFDeviceInfo.patchVersion, 16);
                Serial.println();

                Serial.print("USB+:     ");
                Serial.print(uVUSB);
                Serial.println("uV");

                Serial.print("VCC  3.3: ");
                Serial.print(uV3V3);
                Serial.println("uV");

                Serial.print("VRef 3.0: ");
                Serial.print(uVRef3V0);
                Serial.println("uV");

                Serial.print("VRef 1.5: ");
                Serial.print(uVRef1V5);
                Serial.println("uV");

                Serial.print("USB-:    -");
                Serial.print(uVNUSB);
                Serial.println("uV");
                Serial.println();

                MState = MWaitSDDetTime;
                break;

            case MWaitSDDetTime:
                if(SYSGetMilliSecond() - tStart >= SDWAITTIME)
                {
                    MState = MReadCalibrationInfo;
                }
                break;

            case MReadCalibrationInfo:
               
                // This should not fail
                // if nothing is found, default values are loaded
                if(CFGGetCalibrationInfo(instrGrp) == Idle) 
                {
                    Serial.print("Using calibration from: ");
                    Serial.println(rgCFGNames[((IDHDR *) rgInstr[DCVOLT1_ID])->cfg]);
                    MState = MLookUpWiFi;
                }
                break;

            case MLookUpWiFi:
                if ((retState = WiFiLookupConnInfo(dWiFiFile, pjcmd.iWiFi.wifiWConn)) == Idle) {
                    Serial.print("Found parameter for AP: ");
                    Serial.println(pjcmd.iWiFi.wifiWConn.ssid);

                    MState = MConnectWiFi;
                }
                else if(IsStateAnError(retState))
                {
                    Serial.print("Unable to connect to WiFi AP. Error 0x");
                    Serial.println(retState, 16);
                    MState = MLoop;
                }
                break;

            case MConnectWiFi:
                if((retState = WiFiConnect(pjcmd.iWiFi.wifiWConn, false)) == Idle)
                {
                    Serial.print("Connected to AP: ");
                    Serial.println(pjcmd.iWiFi.wifiAConn.ssid);
                    Serial.println("Starting Web Server");
                    MState = MLoop;

//                    fBlockIOBus = true;
//                    MState = Calibrating;
                }
                else if(IsStateAnError(retState))
                {
                    Serial.print("Error Connecting to WiFi, Error: 0x");
                    Serial.println(retState, 16);
                    MState = MLoop;

//                    fBlockIOBus = true;
//                    MState = Calibrating;
                }
                break;

            case Calibrating:
//                if(OSCCalibrate(rgInstr[OSC1_ID], rgInstr[OSC1_DC_ID]) == Idle)
                if(AWGCalibrate(rgInstr[AWG1_ID]) == Idle)
                {
                    fBlockIOBus = false;
                    MState = MLoop;
                }
                break;

            case MLoop:
                UIMainPage(dWiFiFile, VOLWFPARM, pjcmd.iWiFi.wifiAConn);
                break;

            default:
                break;
        }

        // loop services
        HTTPTask();                 // Keep the HTTP Server alive
        DEIPcK::periodicTasks();    // Keep Stack alive
        LEDTask();
        CFGSdHotSwapTask();
        JSONCmdTask();              // Process the JSON queued commands
    }

    return 0;
}
