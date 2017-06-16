/************************************************************************/
/*                                                                      */
/*    UI.cpp                                                            */
/*                                                                      */
/*    To support User Interface via the Serial Monitor or COMM          */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*    6/1/2016(KeithV): Created                                        */
/************************************************************************/
#include    <OpenScope.h>
static const char szModeJSON[] = "{\"mode\":\"JSON\"}";
static const char szTerminateChunk[] = "\r\n0\r\n\r\n";

STATE UIMainPage(DFILE& dFile, VOLTYPE const wifiVol, WiFiConnectInfo& wifiConn)
{
//    static WiFiScanInfo wifiScan = WiFiScanInfo();
    static STATE state = Idle;
    static char szInput[1024];
    static uint32_t cbInput = 0;

    // WiFi Scan states
    static STATE nextScanWalk = Idle;
    static STATE endScanWalk = Idle;
    static STATE endScanState = Idle;

    // for walking files
    static STATE walkFileEntry = Idle;
    static STATE walkFileDone = Idle;
    static VOLTYPE walkVol = VOLSD;;
    static uint32_t iFileWalk = 0;

    // for write output to the COM port
    static STATE    stateNext = Idle;
    static uint8_t *   pbOutput = NULL;
    static int32_t  cbOutput = 0;
//    static int32_t  cbWritten = 0;
    static char     szChunk[32];
    static uint32_t iodata = 0;

    // for setting gain
    static HINSTR   hInstrt = NULL;
    
    // for general use
    static STATE nextState = Idle;
    static uint32_t u32t1 = 0;

    // for local general use
    STATE retState = Idle;
    FRESULT fr = FR_OK;

    switch(state)
    {
        case Idle:
        case UIMainMenu:

            if(fModeJSON)
            {
                state = UIJSONMain;
            }
            else
            {
                Serial.println();
                Serial.println("Enter the number of the operation you would like to do:");
                Serial.println("1. Enter JSON mode");
                Serial.println("2. Calibrate the instruments");
                Serial.println("3. Save the current calibration values");
                Serial.println("4. Manage your WiFi connections");
                Serial.println("5. View all files names on the SD card");
                Serial.println("6. View all files names in flash");
                Serial.println("7. Set the Oscilloscope input gain");
                Serial.println();
                state = UIWaitForInput;
            }
            Serial.purge(); // clear the serial input buffer
            break;

        case UIWaitForInput:
            if(Serial.available() > 0)
            {
                szInput[0] = Serial.read();
                cbInput = 1;
                switch(szInput[0])
                {                        
                case '1':
                    memcpy(szInput, szModeJSON, sizeof(szModeJSON)-1);
                    cbInput = sizeof(szModeJSON)-1;
                    state = UIJSONWaitOSLEX;
                   break;

                case '2':
                    state = UIStartCalibrateInstruments;
                   break;

                case '3':
                    Serial.println("Saving calibration data");
                    state = UISaveCalibration;
                    break;

                case '4':
                    state = UIManageWiFi;
                    break;

                case '5':
                    walkVol = VOLSD;
                    walkFileEntry = UIPrtszInput;
                    nextState = UIProcessFileEntry;
                    walkFileDone = UIMainMenu;
                    state = UIWalkFiles;
                    break;

                case '6':
                    walkVol = VOLFLASH;
                    walkFileEntry = UIPrtszInput;
                    nextState = UIProcessFileEntry;
                    walkFileDone = UIMainMenu;
                    state = UIWalkFiles;
                    break;

                case '7':
                    state = UISetOSCGain;
                    break;

                case '{':
                    state = UIJSONWaitOSLEX;
                    break;

                // else this is an error
                default:
                    Serial.println("Unsupported option, please try again");
                    // fall thru
                    
                case '\n':
                case '\0':// just a carriage return
                    state = UIMainMenu;
                    break;

                // skip White
                case ' ':
                case '\r':
                    break;
                }
            }
            break;

        case UIWriteOutput:
            if(Serial.writeBuffer(pbOutput, cbOutput, &DCH1CON))
            {
                state = UIWriteDone;
            }
            break;

        case UIWriteDone:
            if(Serial.isDMATxDone())
            {
                state = stateNext;
            }
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** JSON PARSING  ***********************************/
        /************************************************************************/
        /************************************************************************/
        case UIJSONMain:
            if(Serial.available() > 0) 
            {
                szInput[0] = Serial.read();

                if(oslex.IsOSCmdStart(szInput[0]) == OSPAR::JSON)
                {
                    cbInput = 1;
                    state = UIJSONWaitOSLEX;
                    break;
                }
            }
            break;

        case UIJSONWaitOSLEX:

            // keep collecting data
            while(Serial.available() > 0 && cbInput < sizeof(szInput)) szInput[cbInput++] = Serial.read();

            // see if we can parse
            if(!oslex.fLocked)
            {
                oslex.Init();
                oslex.fLocked = true;
                state = UIJSONProcessJSON;  // we know we have at least '{'
            }
            break;

        case UIJSONWaitInput:
            cbInput = 0;
            if(Serial.available() > 0)
            {
                while(Serial.available() > 0 && cbInput < sizeof(szInput)) szInput[cbInput++] = Serial.read();
                state = UIJSONProcessJSON;      
            }
            break;

        case UIJSONProcessJSON:
            switch(oslex.StreamJSON(szInput, cbInput))
            {
                case GCMD::READ:
                    state = UIJSONWaitInput;
                    break;

                case GCMD::ERROR:
                    // clear the input stream, we lost sync
                    // so wait for a new command.
                    Serial.purge();

                    // fall thru

                case GCMD::DONE:

                    // if this is chunked data, put the chunk out
                    if(oslex.cOData > 1) 
                    {
                        cbOutput = oslex.odata[0].cb;
                        stateNext = UIJSONWriteJSON;
                        state = UIJSONWriteChunkSize;
                    }

                    // this is just JSON
                    else 
                    {
                        state = UIJSONWriteJSON;
                    }
                    break;

                    
                // continue
                default:
                    break;

            }
            break;

        case UIJSONWriteChunkSize:
            if(stateNext == UIJSONDone)
            {
                pbOutput = (uint8_t *) szTerminateChunk;
                cbOutput = sizeof(szTerminateChunk) - 1;
            }
            else
            {
                // create the length of the JSON part of the message
                utoa(cbOutput, szChunk, 16);
                cbOutput = strlen(szChunk);
                szChunk[cbOutput++] = '\r';
                szChunk[cbOutput++] = '\n';
                pbOutput = (uint8_t *) szChunk;
            }

            // write it out
            state = UIWriteOutput;     
            break;

        case UIJSONWriteJSON:

            // if we are doing chunks
            if(oslex.cOData > 1) 
            {
                // all chunks end with a \r\n
                ASSERT(oslex.odata[0].pbOut == (uint8_t *) oslex.rgchOut);
                oslex.odata[0].pbOut[oslex.odata[0].cb++] = '\r';
                oslex.odata[0].pbOut[oslex.odata[0].cb++] = '\n';

                // then we need to write the binary out
                stateNext = UIJSONWriteBinary;
            }
            else 
            {
                stateNext = UIJSONDone;
            }

            // write out the JSON
            cbOutput = oslex.odata[0].cb;
            pbOutput = oslex.odata[0].pbOut;
            state = UIWriteOutput;
            break;

        case UIJSONWriteBinary:
            iodata = 1;
            cbOutput = 0;
            for(uint32_t i=1; i<oslex.cOData; i++) cbOutput += oslex.odata[i].cb;
            stateNext = UIJSONWriteBinaryEntry;
            state = UIJSONWriteChunkSize;
            break;

        case UIJSONWriteBinaryEntry:

            if(iodata < oslex.cOData)
            {
                cbOutput = oslex.odata[iodata].cb;
                pbOutput = oslex.odata[iodata].pbOut;
                stateNext = UIJSONWriteBinaryEntry;
                state = UIWriteOutput;
                iodata++;
            }
            else
            {
                // put out the final zero size chunk
                cbOutput = 0;
                stateNext = UIJSONDone;
                state = UIJSONWriteChunkSize;
            }
            break;

        case UIJSONDone:

            // Unlock all of the buffers
            for(uint32_t i=0; i<oslex.cOData; i++)
            {
                if(oslex.odata[i].pLockState != NULL && *oslex.odata[i].pLockState == LOCKOutput)
                {
                    *oslex.odata[i].pLockState = LOCKAvailable;
                }
            }

            // we are no longer parsing JSON
            oslex.Init();

            // get out of JSON mode
            if(fModeJSON)
            {
                // wait for a command to come in
                state = UIJSONMain;
            }

            // go back into menu mode
            else
            {
                state = UIMainMenu;
            }
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** Manage Networks   *******************************/
        /************************************************************************/
        /************************************************************************/
        case UIManageWiFi:
            Serial.println();
            Serial.println("Enter the number of the operation you would like to do:");
            Serial.println("1. View availabe WiFi networks");
            Serial.println("2. Connect to a WiFi network");
            Serial.println("3. View saved WiFi connections");
            Serial.println("4. Add a new WiFi connection");
            Serial.println("5. Delete a saved WiFi connection");
            Serial.println("6. Exit to main menu");
            Serial.println();
            state = UIWaitForManageWiFiInput;
            break;

        case UIWaitForManageWiFiInput:
            if(Serial.available() > 0)
            {
                szInput[0] = Serial.read();
                cbInput = 1;
                switch(szInput[0])
                {                        
                case '1':
                    endScanState = UIWalkScan;       
                    nextScanWalk = UIPrtWiFiInfo;
                    endScanWalk = UIManageWiFi;
                    state = UIAskToShutdownWiFi;
                    break;

                case '2':
                    memcpy((void *) &wifiConn.comhdr.idhdr.mac , &macOpenScope, sizeof(wifiConn.comhdr.idhdr.mac));
                    u32t1 = 1;
                    endScanState = UIWalkFiles;       
                    walkVol = wifiVol;
                    walkFileEntry = UICheckFileName;
                    walkFileDone = UIWaitForNetwork;
                    nextState = UICheckFileAgainstScan;
                    state = UIAskToShutdownWiFi;
                    break;

                case '3':
                    u32t1 = 1;
                    walkVol = wifiVol;
                    walkFileEntry = UIPrtWiFiConnectionFile;
                    walkFileDone = UIManageWiFi;
                    state = UIWalkFiles;
                    break;

                case '4':
                    state = UIAddWiFiConnection;
                    break;
                    
                case '5':
                    Serial.println("Select connection to delete");
                    u32t1 = 1;
                    walkVol = wifiVol;
                    walkFileEntry = UIPrtWiFiConnectionFile;
                    walkFileDone = UIDeleteWiFiConnection;
                    state = UIWalkFiles;
                    break;

                case '6':
                    state = UIMainMenu;
                    break;

                case '{':
                    state = UIJSONWaitOSLEX;
                    break;

                default:
                    Serial.println("Unsupported option, please try again");
                    // fall thru
                    
                case '\n':
                case '\0':// just a carriage return
                    state = UIManageWiFi;
                    break;

                // skip White
                case ' ':
                case '\r':
                    break;
                }
            }
            break;
        
        /************************************************************************/
        /************************************************************************/
        /********************** OSC Gain Setting   ******************************/
        /************************************************************************/
        /************************************************************************/
        case UISetOSCGain:
            Serial.println();
            Serial.println("Enter the Net Gain to set the OSC Input too");
            Serial.println("1. Set OSC1 gain to 1");
            Serial.println("2. Set OSC1 gain to 1/4");
            Serial.println("3. Set OSC1 gain to 1/8");
            Serial.println("4. Set OSC1 gain to 3/40");
            Serial.println("5. Set OSC2 gain to 1");
            Serial.println("6. Set OSC2 gain to 1/4");
            Serial.println("7. Set OSC2 gain to 1/8");
            Serial.println("8. Set OSC2 gain to 3/40");
            Serial.println("9. Exit to main menu");
            Serial.println();
            state = UIOSCGainInput;
            break;

        case UIOSCGainInput:
            if(Serial.available() > 0)
            {
                szInput[0] = Serial.read();
                cbInput = 1;
                state = UIOSCSetGainOffset;
                switch(szInput[0])
                {                        
                case '1':
                    Serial.println("OSC1 gain set to 1");
                    hInstrt = instrGrp.rghInstr[OSC1_ID];
                    iFileWalk = 1;
                    break;

                case '2':
                    Serial.println("OSC1 gain set to 1/4");
                    hInstrt = instrGrp.rghInstr[OSC1_ID];
                    iFileWalk = 2;
                    break;

                case '3':
                    Serial.println("OSC1 gain set to 1/8");
                    hInstrt = instrGrp.rghInstr[OSC1_ID];
                    iFileWalk = 3;
                    break;

                case '4':
                    Serial.println("OSC1 gain set to 3/40");
                    hInstrt = instrGrp.rghInstr[OSC1_ID];
                    iFileWalk = 4;
                    break;
                    
                case '5':
                    Serial.println("OSC2 gain set to 1");
                    hInstrt = instrGrp.rghInstr[OSC2_ID];
                    iFileWalk = 1;
                    break;

                case '6':
                    Serial.println("OSC2 gain set to 1/4");
                    hInstrt = instrGrp.rghInstr[OSC2_ID];
                    iFileWalk = 2;
                    break;

                case '7':
                    Serial.println("OSC2 gain set to 1/8");
                    hInstrt = instrGrp.rghInstr[OSC2_ID];
                    iFileWalk = 3;
                    break;

                case '8':
                    Serial.println("OSC2 gain set to 3/40");
                    hInstrt = instrGrp.rghInstr[OSC2_ID];
                    iFileWalk = 4;
                    break;
                    
                case '9':
                    state = UIMainMenu;
                    break;

                default:
                    Serial.println("Unsupported option, please try again");
                    // fall thru
                    
                case '\n':
                case '\0':// just a carriage return
                    state = UISetOSCGain;
                    break;

                // skip White
                case ' ':
                case '\r':
                    break;
                }
            }
            break;

        case UIOSCSetGainOffset:
            if(OSCSetGainAndOffset(hInstrt, iFileWalk, 0) == Idle)
            {
                state = UISetOSCGain;
            }
            break;

        /************************************************************************/
        /************************************************************************/
        /******************************** WiFi Scan *****************************/
        /************************************************************************/
        /*********************  endScanState = state to goto after scanning  ****/
        /*********************  endScanWalk = called when all SSIDs processed ***/
        /************************************************************************/
        case UIAskToShutdownWiFi:
            if(deIPcK.isLinked())
            {
                Serial.println("The WiFi connection must be dropped to proceed");
                Serial.println("Is it okay to shutdown the WiFi connection? Y/N");
                state = UIWaitForShutdownReply;                        
            }
            else
            {
                Serial.println("Scanning WiFi");
                pjcmd.iWiFi.wifiScan.iNetwork = 0;
                state = UIWiFiScan;
            }

        case UIWaitForShutdownReply:
            if(Serial.available() > 0)
            {
                szInput[0] = Serial.read();
                cbInput = 1;
                switch(szInput[0])
                {
                case 'Y':
                case 'y':
                    Serial.println("Scanning WiFi");
                    pjcmd.iWiFi.wifiScan.iNetwork = 0;
                    state = UIWiFiScan;
                    break;

                case 'N':
                case 'n':
                    Serial.println("Not touching WiFi connection");
                    state = UIManageWiFi;
                    break;

                // skip White
                case ' ':
                case '\r':
                case '\n':
                    break;

                default:
                    Serial.println("Unknown response, you must specify Y or N");
                    state = UIWaitForShutdownReply;
                    break;
                }
            }
            break;

        case UIWiFiScan:
            if((retState = WiFiScan(pjcmd.iWiFi.wifiScan)) == Idle)
            {
                state = endScanState;
            }
            else if(IsStateAnError(retState))
            {
                Serial.print("WiFi Scan failed, Error: 0x");
                Serial.println(retState, 16);
                state = UIManageWiFi;
            }
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** Walk the Scan data ******************************/
        /************************************************************************/
        /*********************  nextScanWalk = called on each SSID **************/
        /*********************  endScanWalk = called when all SSIDs processed ***/
        /************************************************************************/
        /************************************************************************/
        case UIWalkScan:
            
            if(pjcmd.iWiFi.wifiScan.iNetwork < pjcmd.iWiFi.wifiScan.cNetworks)
            {
                if(!deIPcK.getScanInfo(pjcmd.iWiFi.wifiScan.iNetwork, &pjcmd.iWiFi.wifiScan.scanInfo))
                {
                    state = UIManageWiFi;
                    return(WiFiNoScanData);
                }
                pjcmd.iWiFi.wifiScan.iNetwork++;
                state = nextScanWalk;
            }
            else
            {
                state = endScanWalk;
            }
            break;
            
        /************************************************************************/
        /************************************************************************/
        /********************** WiFi connect / disconnect ***********************/
        /************************************************************************/
        /********  nextState = where to go after the  connect / disconnect ******/
        /************************************************************************/
        /************************************************************************/
        case UIWiFiConnect:
            if((retState = WiFiConnect(wifiConn, false)) == Idle)
            {
                Serial.print("Connected to AP: ");
                Serial.println(wifiConn.ssid);
                state = nextState;
            }
            else if(IsStateAnError(retState))
            {
                Serial.print("Failed to connect, error: 0x");
                Serial.println(retState, 16);
                state = UIManageWiFi;
            }
            break;

        case UIWiFiDisconnect:
            if((retState = WiFiDisconnect()) == Idle)
            {
                Serial.print("Disconnected from AP: ");
                Serial.println(wifiConn.ssid);
                state = nextState;
            }
            else if(IsStateAnError(retState))
            {
                Serial.print("Failed to disconnect, error: 0x");
                Serial.println(retState, 16);
                state = UIManageWiFi;
            }
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** Walk the file system ****************************/
        /********************** walkVol = vol to walk. **************************/
        /*********************  walkFileEntry = state on each file entry ********/
        /********************** walkFileDone = state when all files walked ******/
        /*********************** szInput contains the file name *****************/
        /************************************************************************/
        /************************************************************************/
        case UIWalkFiles:
            if((fr = DDIRINFO::fsopendir(DFATFS::szFatFsVols[walkVol])) == FR_OK)
            {
                iFileWalk = 0;
                state = UIProcessFileEntry;
            }
            else
            {
                Serial.println("Unable to open volumn: ");
                Serial.println(DFATFS::szFatFsVols[walkVol]);
                state = UIMainMenu;
            }
            break;

        case UIProcessFileEntry:
            {
                char szFileName[128];
                char const * szFile = NULL;

                // always assign a valid filename locationOh
                DDIRINFO::fssetLongFilename(szFileName);
                DDIRINFO::fssetLongFilenameLength(sizeof(szFileName));

                state = walkFileEntry;
                if(DDIRINFO::fsreaddir() == FR_OK)
                {
                    if(DDIRINFO::fsgetLongFilename()[0] != '\0')
                    {
                        szFile = DDIRINFO::fsgetLongFilename();
                    }
                    else if(DDIRINFO::fsget8Dot3Filename()[0] != 0)
                    {
                        szFile = DDIRINFO::fsget8Dot3Filename();
                    }

                    // save this file
                    if(szFile != NULL)
                    {
                        strcpy(szInput, szFile);
                    }

                    // done walking the list
                    else
                    {
                        DDIRINFO::fsclosedir();
                        state = walkFileDone;
                        szInput[0] = '\0';
                    }
                    iFileWalk++;
                }
                else
                {
                    state = UIMainMenu;
                }
            }
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** szInput processing  *****************************/
        /************************************************************************/
        /************************************************************************/
        case UIPrtszInput:
            Serial.print(iFileWalk, 10);
            Serial.print(". ");
            Serial.println(szInput);
            state = nextState;
            break;

        case UIDeleteszInputFile:
            if((fr = DFATFS::fsunlink(szInput)) == FR_OK)
            {
                Serial.print("File ");
                Serial.print(szInput);
                Serial.println(" deleted");
            }
            else
            {
                Serial.print("Unable to delete file ");
                Serial.print(szInput);
                Serial.print(" fat error 0x");
                Serial.println(fr, 16);
            }
            state = nextState;
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** Calibrate Instruments  **************************/
        /************************************************************************/
        /************************************************************************/
        case UIStartCalibrateInstruments:
                Serial.println("Calibrating instruments");
                Serial.println("Connect DCOUT1 to OCS1 and DCOUT2 to OCS2");
                Serial.println("To connect DCOUT1 to OCS1, wire the solid red wire to the solid orange wire");
                Serial.println("To connect DCOUT2 to OCS2, wire the solid white wire to the solid blue wire");
                Serial.println("Enter C when ready");
                state = UIWaitForCalibrationButton;
                break;

        case UIWaitForCalibrationButton:
            if(Serial.available() > 0)
            {
                szInput[0] = Serial.read();
                cbInput = 1;
                switch(szInput[0])
                {
                case 'c':
                case 'C':
                    Serial.println("Starting Calibration");
                    state = UIAWGStop;
                    break;

                case 'x':
                case 'X':
                    Serial.println("Calibration Terminated");
                    state = UIMainMenu;
                    break;

                // skip White
                case ' ':
                case '\r':
                case '\n':
                    break;

                default:
                    Serial.println("Invalid command, Enter C to continue with calibration, Enter X to terminate calibration.");
                    break;
                }
            }
            break;

        case UIAWGStop:
            if(AWGStop(rgInstr[AWG1_ID]) == Idle)
            {
                state = UICalibrateInstruments;
            }
            break;

        case UICalibrateInstruments:
            if((retState = CFGCalibrateInstruments(instrGrp)) == Idle) 
            {
                Serial.print("Calibration time was: ");
                Serial.print(instrGrp.tStart);
                Serial.println(" msec");
                state = UIPrtCalibrationInfo;
            }
            else if(IsStateAnError(retState))
            {
                Serial.print("Unable to calibrate info for ");
                Serial.print(rgszInstr[((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->id]);
                Serial.print(" Error: 0x");
                Serial.println(retState, 16);

                if(retState == OSCDcNotHookedToOSC)
                {
                    Serial.println("Most likely error is that the DC output is not hooked to the Analog Inputs");
                }
                state = UIMainMenu;
            }
            break;

        case UIPrtCalibrationInfo:
            Serial.println("Do you want to save this calibration Y/N?");
            state = UIWaitForSaveCalibration;
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** Save Calibration  *******************************/
        /************************************************************************/
        /************************************************************************/
        case UIWaitForSaveCalibration:
            if(Serial.available() > 0)
            {
                szInput[0] = Serial.read();
                cbInput = 1;
                switch(szInput[0])
                {
                case 'Y':
                case 'y':
                    state = UISaveCalibration;
                    break;

                case 'N':
                case 'n':
                    Serial.println("Calibration information is not saved");
                    state = UIMainMenu;
                    break;

                // skip White
                case ' ':
                case '\r':
                case '\n':
                    break;

                default:
                    Serial.println("Invalid response, calibration information was not saved");
                    Serial.println("You can save it manually by selecting to save it");
                    state = UIMainMenu;
                    break;
                }
            }
            break;

        case UISaveCalibration:
            if((retState = CFGSaveCalibration(instrGrp, VOLSD, CFGSD)) == Idle) 
            {
                Serial.print("Calibration was saved");
                state = UIMainMenu;
            }
            else if(IsStateAnError(retState))
            {
                Serial.print("Unable to save calibration info for ");
                Serial.print(rgszInstr[((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->id]);
                Serial.print(" Error: 0x");
                Serial.println(retState, 16);
                state = UIMainMenu;
            }
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** Connect to a network   **************************/
        /************************************************************************/
        /************************************************************************/
        case UICheckFileName:
            {
                char sz[128];
                int cbPrefix = CFGCreateFileName(wifiConn.comhdr.idhdr, NULL, sz, sizeof(sz));
                int cbInput = strlen(szInput);

                if(cbPrefix < cbInput && memcmp(sz, szInput, cbPrefix) == 0)
                {
                    // this will include the terminating null
                    // remember that we index past the '_', 
                    // so the length is one to long when calculating the size
                    // and that will include the null when memcpy
                    // use memcpy instead of strcpy because it is in place
                    memcpy(szInput, &szInput[cbPrefix+1], cbInput-cbPrefix);    
                    pjcmd.iWiFi.wifiScan.iNetwork = 0;
                    nextScanWalk = nextState;
                    endScanWalk = UIProcessFileEntry;
                    state = UIWalkScan;
                }
                else
                {
                    state = UIProcessFileEntry;
                }
            }
            break;
 
        case UICheckFileAgainstScan:
            {
                // a match
                if(strlen(szInput) == pjcmd.iWiFi.wifiScan.scanInfo.ssidLen && memcmp(szInput, pjcmd.iWiFi.wifiScan.scanInfo.ssid, pjcmd.iWiFi.wifiScan.scanInfo.ssidLen) == 0)
                {
                    Serial.print(u32t1++, 10);
                    Serial.print(". ");
                    Serial.println(szInput);
                    state = UIProcessFileEntry;
                }
                else
                {
                    state = UIWalkScan;
                }
            }
            break;


        case UIWaitForNetwork:
            if(IOReadLine(szInput, sizeof(szInput)) == Idle)
            {
                uint32_t index  = atoi(szInput);
                    
                if(0 < index && index < u32t1)
                {                    
                    u32t1 = index;
                    endScanState = UIWalkFiles;       
                    walkVol = wifiVol;
                    walkFileEntry = UICheckFileName;
                    walkFileDone = UIManageWiFi;
                    nextState = UICheckFileAgainstSelection;
                    state = UIWalkFiles;
                }
                else
                {
                    Serial.println("Invalid selection, try again");
                    state = UIManageWiFi;
                }
            }
            break;
 
        case UICheckFileAgainstSelection:
            {
                // a match
                if(strlen(szInput) == pjcmd.iWiFi.wifiScan.scanInfo.ssidLen && memcmp(szInput, pjcmd.iWiFi.wifiScan.scanInfo.ssid, pjcmd.iWiFi.wifiScan.scanInfo.ssidLen) == 0)
                {
                     if((--u32t1) == 0)
                     {
                        char sz[128];
                        memcpy(sz, pjcmd.iWiFi.wifiScan.scanInfo.ssid, pjcmd.iWiFi.wifiScan.scanInfo.ssidLen);
                        sz[pjcmd.iWiFi.wifiScan.scanInfo.ssidLen] = '\0';
                        CFGCreateFileName(wifiConn.comhdr.idhdr, sz, szInput, sizeof(szInput));
                        state = WiFiReadFile;
                     }
                     else
                     {
                         state = UIProcessFileEntry;
                     }
                }
                else
                {
                    state = UIWalkScan;
                }
            }
            break;

        case WiFiReadFile:
            if((retState = IOReadFile(dFile, wifiVol, szInput, wifiConn.comhdr.idhdr)) == Idle)
            {
                // put this to the starting state, no matter how it was saved
                wifiConn.comhdr.state = Idle;
                nextState = UIMainMenu;
                state = UIWiFiConnect;
            }
            else if(IsStateAnError(retState))
            {
                Serial.print("Unable to read file: ");
                Serial.println(szInput);
                state = UIManageWiFi;
            }
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** Walk Saved WiFi Connections *********************/
        /************************************************************************/
        /************************************************************************/
        case UIPrtWiFiConnectionFile:
            {
                char szWiFiPrefix[32];
                uint32_t cbPrefix = 0;
                uint32_t cbFile = 0;
                IDHDR idHdr = {sizeof(IDHDR), WFVER, WIFIPARAM_ID, CFGSD, macOpenScope};

                // create the matching template
                cbFile = strlen(szInput);
                cbPrefix = CFGCreateFileName(idHdr, NULL, szWiFiPrefix, sizeof(szWiFiPrefix));

                if(cbFile >= cbPrefix && memcmp(szWiFiPrefix, szInput, cbPrefix) == 0 )
                {
                    Serial.print(u32t1++);
                    Serial.print(". ");
                    Serial.println(&szInput[cbPrefix+1]);
                }
            }
            state = UIProcessFileEntry;
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** Delete Saved WiFi Connections *********************/
        /************************************************************************/
        /************************************************************************/
        case UIDeleteWiFiConnection:
            if(IOReadLine(szInput, sizeof(szInput)) == Idle)
            {
                uint32_t index  = atoi(szInput);
                
                if(0 < index && index < u32t1)
                {
                    u32t1 = index;
                    walkVol = wifiVol;
                    walkFileEntry = UICheckWiFiConnectionToDelete;
                    walkFileDone = UIManageWiFi;
                    state = UIWalkFiles;
                }
                else
                {
                    Serial.println("Invalid selection");
                    state = UIManageWiFi;
                }

            }
            break;
  
        case UICheckWiFiConnectionToDelete:
            {
                char szWiFiPrefix[32];
                uint32_t cbPrefix = 0;
                uint32_t cbFile = 0;
                IDHDR idHdr = {sizeof(IDHDR), WFVER, WIFIPARAM_ID, CFGSD, macOpenScope};

                // create the matching template
                cbFile = strlen(szInput);
                cbPrefix = CFGCreateFileName(idHdr, NULL, szWiFiPrefix, sizeof(szWiFiPrefix));

                state = UIProcessFileEntry;
                if(cbFile >= cbPrefix && memcmp(szWiFiPrefix, szInput, cbPrefix) == 0 && (--u32t1) == 0)
                {
                    nextState = UIManageWiFi;
                    state = UIDeleteszInputFile;
                }
            }
            break;

        /************************************************************************/
        /************************************************************************/
        /********************** Print Available WiFi SSIDs  *********************/
        /************************************************************************/
        /************************************************************************/
        case UIPrtWiFiInfo:
            {
                uint32_t j = 0;

                Serial.println("");
                Serial.print("Scan info for index: ");
                Serial.println(pjcmd.iWiFi.wifiScan.iNetwork-1, 10);

                Serial.print("SSID: ");
                for(j=0; j<pjcmd.iWiFi.wifiScan.scanInfo.ssidLen; j++)
                {
                    Serial.print((char) pjcmd.iWiFi.wifiScan.scanInfo.ssid[j]);
                }
                Serial.println();

                Serial.print("BSSID / MAC: ");
                for(j=0; j<sizeof(pjcmd.iWiFi.wifiScan.scanInfo.bssid); j++)
                {
                    if(pjcmd.iWiFi.wifiScan.scanInfo.bssid[j] < 16)
                    {
                        Serial.print(0, 16);
                    }
                    Serial.print(pjcmd.iWiFi.wifiScan.scanInfo.bssid[j], 16);
                }
                Serial.println("");

                Serial.print("Channel: ");
                Serial.println(pjcmd.iWiFi.wifiScan.scanInfo.channel, 10);

                Serial.print("Signal Strength: ");
                Serial.println(pjcmd.iWiFi.wifiScan.scanInfo.rssi, 10);

                if(pjcmd.iWiFi.wifiScan.scanInfo.bssType == DEWF_INFRASTRUCTURE)
                {
                    Serial.println("Infrastructure Network");
                }
                else if(pjcmd.iWiFi.wifiScan.scanInfo.bssType == DEWF_ADHOC)
                {
                    Serial.println("AdHoc Network");
                }
                else
                {
                    Serial.println("Unknown Network Type");
                }

                Serial.print("Beacon Period: ");
                Serial.println(pjcmd.iWiFi.wifiScan.scanInfo.beaconPeriod, 10);

                Serial.print("dtimPeriod: ");
                Serial.println(pjcmd.iWiFi.wifiScan.scanInfo.dtimPeriod, 10);

                Serial.print("atimWindow: ");
                Serial.println(pjcmd.iWiFi.wifiScan.scanInfo.atimWindow, 10);

                Serial.println("Secuity info: WPA2  WPA  Preamble  Privacy  Reserved  Reserved  Reserved  IE");
                  Serial.print("                ");
                  Serial.print((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b10000000) >> 7, 10);
                                   Serial.print("    ");
                                   Serial.print((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b01000000) >> 6, 10);
                                        Serial.print("       ");
                                        Serial.print((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00100000) >> 5, 10);
                                                Serial.print("        ");
                                                Serial.print((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00010000) >> 4, 10);
                                                         Serial.print("         ");
                                                         Serial.print((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00001000) >> 3, 10);
                                                                   Serial.print("         ");
                                                                    Serial.print((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00000100) >> 2, 10);
                                                                             Serial.print("         ");
                                                                             Serial.print((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00000010) >> 1, 10);
                                                                                       Serial.print("      ");
                                                                                       Serial.println((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00000001), 10);

                Serial.print("Count of support bit rates: ");
                Serial.println(pjcmd.iWiFi.wifiScan.scanInfo.cBasicRates, 10);    

                for( j= 0; j< pjcmd.iWiFi.wifiScan.scanInfo.cBasicRates; j++)
                {
                    uint32_t rate = (pjcmd.iWiFi.wifiScan.scanInfo.basicRateSet[j] & 0x7F) * 500;
                    Serial.print("\tSupported Rate: ");
                    Serial.print(rate, 10);
                    Serial.println(" kbps");
                }
            }

            state = UIWalkScan;
            break;

        /************************************************************************/
        /************************************************************************/
        /************************** Add a network   *****************************/
        /************************************************************************/
        /************************************************************************/
        case UIAddWiFiConnection:
            u32t1 = 1;
            endScanState = UIWalkScan;       
            nextScanWalk = UICheckNetwork;
            endScanWalk = UIWaitForNetworkSelection;
            state = UIAskToShutdownWiFi;
            break;

        case UICheckNetwork:
            {
                char sz[128];
                memcpy(sz, pjcmd.iWiFi.wifiScan.scanInfo.ssid, pjcmd.iWiFi.wifiScan.scanInfo.ssidLen);
                sz[pjcmd.iWiFi.wifiScan.scanInfo.ssidLen] = '\0';
                CFGCreateFileName(wifiConn.comhdr.idhdr, sz, szInput, sizeof(szInput));

                // if we don't have a file for it already
                if(     !(DFATFS::fschdrive(DFATFS::szFatFsVols[wifiVol]) == FR_OK && DFATFS::fschdir(DFATFS::szRoot) == FR_OK && DFATFS::fsexists(szInput)) &&
                        pjcmd.iWiFi.wifiScan.scanInfo.bssType == DEWF_INFRASTRUCTURE && 
                        (((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b11000000) != 0) ||  ((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00010000) == 0)) )  // WPA OR no security
                {
                    Serial.print(u32t1++, 10);
                    Serial.print(". ");
                    Serial.println(sz);
                }
                
                state = UIWalkScan;
            }
            break;
                       
        case UIWaitForNetworkSelection:
            if(IOReadLine(szInput, sizeof(szInput)) == Idle)
            {
                uint32_t index  = atoi(szInput);
                    
                if(index == 0)
                {
                    Serial.println("No selection made, exiting menu");
                    state = UIManageWiFi;
                }
                else if(index >= u32t1)
                {
                    Serial.println("Invalid selection, try again");
                }
                else
                {                    
                    u32t1 = index;
                    pjcmd.iWiFi.wifiScan.iNetwork = 0;
                    nextScanWalk = UIGetNetwork;
                    endScanWalk = Idle;
                    state = UIWalkScan;
                }
            }
            break;

        case UIGetNetwork:
            {
                char sz[128];
                memcpy(sz, pjcmd.iWiFi.wifiScan.scanInfo.ssid, pjcmd.iWiFi.wifiScan.scanInfo.ssidLen);
                sz[pjcmd.iWiFi.wifiScan.scanInfo.ssidLen] = '\0';
                CFGCreateFileName(wifiConn.comhdr.idhdr, sz, szInput, sizeof(szInput));

                state = UIWalkScan;
                if( !(DFATFS::fschdrive(DFATFS::szFatFsVols[wifiVol]) == FR_OK &&  DFATFS::fschdir(DFATFS::szRoot) == FR_OK  &&  DFATFS::fsexists(szInput)) &&
                    pjcmd.iWiFi.wifiScan.scanInfo.bssType == DEWF_INFRASTRUCTURE && 
                    (((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b11000000) != 0) || ((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00010000) == 0)) && // WPA OR no security
                    (--u32t1) == 0 )
                {
                    state = UIConnectToNetwork;
                }
            }
            break;

        case UIConnectToNetwork:
            memcpy(wifiConn.ssid, pjcmd.iWiFi.wifiScan.scanInfo.ssid, pjcmd.iWiFi.wifiScan.scanInfo.ssidLen);
            wifiConn.ssid[pjcmd.iWiFi.wifiScan.scanInfo.ssidLen] = '\0';
            wifiConn.comhdr.activeFunc = WIFIFnManConnect; // Default is Manual connect
//            memcpy(wifiConn.bssid, pjcmd.iWiFi.wifiScan.scanInfo.bssid, sizeof(wifiConn.bssid));

            if((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b00010000) == 0)
            {
                // No security
                wifiConn.wifiKey = DEWF_SECURITY_OPEN;
                Serial.print("Using Open Security Network for: ");
                Serial.println(wifiConn.ssid);  
                nextState = UIDisconnectAfterConnect;
                state = UIWiFiConnect;
            }
            else
            {
                if((pjcmd.iWiFi.wifiScan.scanInfo.apConfig & 0b10000000) != 0)
                {
                    wifiConn.wifiKey = DEWF_SECURITY_WPA2_WITH_KEY;
                }
                else
                {
                    wifiConn.wifiKey = DEWF_SECURITY_WPA_WITH_KEY;
                }

                Serial.print("Please enter PassPhrase for: ");
                Serial.println(wifiConn.ssid);        
                state = UIReadPassPhrase;
            }
            break;
                
        case UIReadPassPhrase:
            if(IOReadLine(szInput, sizeof(szInput)) == Idle)
            {
                if(strlen(szInput) > WF_MAX_PASSPHRASE_LENGTH)
                {
                    Serial.println("PassPhrase is too long.");
                    Serial.print("Please enter PassPhrase for: ");
                    Serial.println(wifiConn.ssid);        
                }
                else
                {
                    state = UICalculatePSKey;
                }
            }
           break;

        case UICalculatePSKey:
            if(deIPcK.wpaCalPSK(wifiConn.ssid, szInput, wifiConn.key.wpa2Key))
            {
                Serial.print("PSK value: ");
                GetNumb(wifiConn.key.wpa2Key.rgbKey, sizeof(wifiConn.key.wpa2Key), ':', szInput);
                Serial.println(szInput);

                nextState = UIAskToAutoSave;
                state = UIWiFiConnect;
            }
            else
            {
                Serial.println("Unable to calculate PSK key");
                state = UIManageWiFi;
            }
            break;
        
        case UIAskToAutoSave:
                Serial.println("Would you like to auto-connect to this network? Y/N");
                state = UIReadAutoConnect;
            break;

        case UIReadAutoConnect:
            if(Serial.available() > 0)
            {
                szInput[0] = Serial.read();
                cbInput = 1;
                state = UISaveWiFiConnection;
                switch(szInput[0])
                {
                case 'Y':
                case 'y':
                    Serial.println("This network will be connected to automatically.");
                    wifiConn.comhdr.activeFunc = WIFIFnAutoConnect; // Default is Manual connect
                    break;

                case 'N':
                case 'n':
                    Serial.println("This network must be connected to manually.");
                    wifiConn.comhdr.activeFunc = WIFIFnManConnect; // Default is Manual connect
                    break;

                // skip White
                case ' ':
                case '\r':
                case '\n':
                    break;

                default:
                    Serial.println("Invalid response, please type Y or N");
                    Serial.println("Would you like to auto-connect to this network? Y/N");
                    state = UIReadAutoConnect;
                    break;
                }
            }
            break;

        case UISaveWiFiConnection:
            if((retState = WiFiSaveConnInfo(dFile, wifiVol, wifiConn)) == Idle)
            {
                char sz[128];
                CFGCreateFileName(wifiConn.comhdr.idhdr, wifiConn.ssid, sz, sizeof(sz));
                Serial.print("Saved WiFi File: ");
                Serial.println(sz);
                state = UIAskToDisconnect;
            }
            else if(IsStateAnError(retState))
            {
                char sz[128];
                CFGCreateFileName(wifiConn.comhdr.idhdr, wifiConn.ssid, sz, sizeof(sz));
                Serial.print("Error Saving file: ");
                Serial.println(sz);
                state = UIManageWiFi;
            }
            break;

        case UIAskToDisconnect:
            Serial.println("Do you want to stay connected to this network? Y/N");
            state = UIDisconnectAfterConnect;
            break;

        case UIDisconnectAfterConnect:
            if(Serial.available() > 0)
            {
                szInput[0] = Serial.read();
                cbInput = 1;
                state = UISaveWiFiConnection;
                switch(szInput[0])
                {
                case 'Y':
                case 'y':
                    state = UIMainMenu;
                    break;

                case 'N':
                case 'n':
                    Serial.println("Disconnecting from the network");
                    nextState = UIManageWiFi;
                    state = UIWiFiDisconnect;
                    break;

                // skip White
                case ' ':
                case '\r':
                case '\n':
                    break;

                default:
                    Serial.println("Invalid response, please type Y or N");
                    state = UIAskToDisconnect;
                    break;
                }
            }
            break;
            
        /************************************************************************/
        /************************************************************************/
        /****************************** Default   *******************************/
        /************************************************************************/
        /************************************************************************/
        default:
            Serial.println("Unsupported UI state");
            state = Idle;
            break;
    }

    return(state);
}

STATE IOReadFile(DFILE& dFile, VOLTYPE const vol, char const * const szFileName, IDHDR& idhdr)
{
    static STATE state = Idle;
    uint8_t * const pData   = (uint8_t * const) &idhdr;
    FRESULT fr = FR_OK;

    switch(state)
    {
        case Idle:
            dFile.fsclose();
            if((fr = DFATFS::fschdrive(DFATFS::szFatFsVols[vol])) != FR_OK || (fr = DFATFS::fschdir(DFATFS::szRoot)) != FR_OK)
            {
                return(fr | STATEError);
            }
            else if((fr = dFile.fsopen(szFileName, FA_READ)) != FR_OK)
            {
                return(fr | STATEError);
            }
            else if(dFile.fssize() != idhdr.cbInfo)
            {
                dFile.fsclose();
                return(NotEnoughMemory);
            }
            else if((fr = dFile.fslseek(0)) != FR_OK)
            {
                dFile.fsclose();
                return(fr | STATEError);
            }
            state = IORead;
            break;

        case IORead:
            {
                uint32_t iFileOff   = dFile.fstell();
                uint32_t cbToRead   = dFile.fssize() - iFileOff;
                uint32_t cbRead     = 0;

                // if we are done
                if(cbToRead == 0)
                {
                    dFile.fsclose();
                    state = Idle;
                    return(Idle);
                }
                else if(cbToRead > DFILE::FS_DEFAULT_BUFF_SIZE)
                {
                    cbToRead = DFILE::FS_DEFAULT_BUFF_SIZE;
                }

                // in more data
                if((fr = dFile.fsread(&pData[iFileOff], cbToRead, &cbRead)) != FR_OK)
                {
                    dFile.fsclose();
                    state = Idle;
                    return(fr | STATEError);
                }
            }
            break;

        default:
            state = Idle;
            dFile.fsclose();
            return(IOVolError);
            break;

    }

    return(state);
}

STATE IOWriteFile(DFILE& dFile, VOLTYPE const vol, char const * const szFileName, IDHDR const & idhdr)
{
    static STATE state = Idle;
    uint8_t const * const pData   = (uint8_t const * const) &idhdr;
    FRESULT fr = FR_OK;

    switch(state)
    {
        case Idle:
            dFile.fsclose();
            if((fr = DFATFS::fschdrive(DFATFS::szFatFsVols[vol])) != FR_OK || (fr = DFATFS::fschdir(DFATFS::szRoot)) != FR_OK)
            {
                return(fr | STATEError);
            }
            else if((fr = dFile.fsopen(szFileName, FA_CREATE_ALWAYS | FA_WRITE | FA_READ)) != FR_OK)
            {
                return(fr | STATEError);
            }
            else if((fr = dFile.fslseek(0)) != FR_OK)
            {
                dFile.fsclose();
                return(fr | STATEError);
            }
            state = IOWrite;
            break;

        case IOWrite:
            {
                uint32_t iFileOff   = dFile.fstell();
                uint32_t cbWrite    = idhdr.cbInfo - iFileOff;
                uint32_t cbWritten  = 0;

                // if we are done
                if(cbWrite == 0)
                {
                    dFile.fsclose();
                                        
                    state = Idle;
                    return(Idle);
                }
                else if(cbWrite > DFILE::FS_DEFAULT_BUFF_SIZE)
                {
                    cbWrite = DFILE::FS_DEFAULT_BUFF_SIZE;
                }

                // write out some data
                if((fr = dFile.fswrite(&pData[iFileOff], cbWrite, &cbWritten)) != FR_OK)
                {
                    dFile.fsclose();
                    state = Idle;
                    return(fr | STATEError);
                }
            }
            break;

        default:
            state = Idle;
            dFile.fsclose();
            return(IOVolError);
            break;
    }

    return(state);
}

STATE IOReadLine(char * szInput, uint32_t cb)
{
    static STATE state = Idle;
    static uint32_t index = 0;

    switch(state)
    {
        // this must clear the buffer on the first call
        case Idle:
            if(szInput == NULL || cb == 0)
            {
                return(NotEnoughMemory);
            }

            Serial.purge(); // clear the serial input buffer
            index = 0;
            state = IOReadToEndOfLine;
            break;

        case IOReadToEndOfLine:
            if(Serial.available() > 0)
            {
                while(Serial.available() > 0 && index < cb)
                {
                    szInput[index] = Serial.read();
                    
                    if(szInput[index] == '\n' || szInput[index] == '\r' || index == cb-1)
                    {
                        szInput[index] = '\0';
                        index = 0;
                        state = Idle;
                        return(Idle);
                    }
                    index++;
                }
            }
            break;

        default:
            index = 0;
            state = Idle;
            if(szInput != NULL && cb > 0)
            {
                szInput[0] = '\0';
            }
            break;
    }

    return(state);
}
