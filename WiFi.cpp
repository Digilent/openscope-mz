/************************************************************************/
/*                                                                      */
/*    WiFi.cpp                                                          */
/*                                                                      */
/*    To support WiFi connection and processing                         */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*    5/27/2016(KeithV): Created                                        */
/************************************************************************/
#include    <OpenScope.h>

STATE WiFiDisconnect(void)
{
    static STATE    state = Idle;
    uint8_t         connectionState;
            
    switch(state)
    {
        case Idle:
            // everything must shutdown to do a scan
            HTTPEnable(false);
            deIPcK.end();
            // only works after the end because end does not remove
            // the adaptor pointer, but does take it out of the link layer
            deIPcK.wfDisconnect();
            state = WiFiWaitingDisconnect;
            break;

        case WiFiWaitingDisconnect:
            // wait until we disconnect
            // unfortunately we have no clear indication when 
            // this is done, isLinked will immediately return
            // disconnected, but we don't want to start a control
            // function until it really IS disconnected.
            WF_ConnectionStateGet(&connectionState);
            // FF is what is returned if the SPI port is not connected
            // clearly not connected if the  SPI is working
            if(connectionState == WF_CSTATE_NOT_CONNECTED || connectionState == 0xFF)
            {
                state = Idle;
            }
            break;
            
        default:
            state = Idle;
            return(InvalidState);
            break;
    }
    return(state);
}

STATE WiFiScan(WiFiScanInfo& wifiScan)
{   
    STATE retState = Idle;
            
    switch(wifiScan.state)
    {

        case Idle:
            wifiScan.state = WiFiWaitingDisconnect;
            // fall thru
            
        case WiFiWaitingDisconnect:
            if((retState = WiFiDisconnect()) == Idle)
            {
                wifiScan.state = WiFiScanning;
            }
            else if(IsIPStatusAnError(retState))
            {
                wifiScan.state = Idle;
                return(retState);
            }
            break;
        
        case WiFiScanning:
            if(deIPcK.wfScan((int *) &wifiScan.cNetworks, &wifiScan.status))
            {
                wifiScan.state = Idle;
            }
            else if(IsIPStatusAnError(wifiScan.status))
            {
                wifiScan.state = Idle;
                return(WiFiScanFailure | wifiScan.status);
            }
            break;

        default:
            wifiScan.state = Idle;
            return(InvalidState);
            break;
    }

    return(wifiScan.state);
}

STATE WiFiConnect(WiFiConnectInfo& wifiConn, bool fForce)
{
    switch(wifiConn.comhdr.state)
    {
        case Idle:
            // we are going to use the start time as an error code
            wifiConn.comhdr.tStart = Idle;

        case WiFiWaitingConnect:
            if(wifiConn.wifiKey == DEWF_SECURITY_WPA2_WITH_KEY || wifiConn.wifiKey == DEWF_SECURITY_WPA_WITH_KEY)
            {
                wifiConn.comhdr.state = WiFiWaitingConnectWPAWithKey;
            }
            else if(wifiConn.wifiKey == DEWF_SECURITY_OPEN)
            {
                wifiConn.comhdr.state = WiFiWaitingConnectNone;
            }
            else
            {
                wifiConn.comhdr.state = Idle;
                return(WiFiUnsuporttedSecurity);
            }

            // if we are to force a disconnect
            if(IsHTTPRunning())
            {
                if(fForce) 
                {
                    wifiConn.comhdr.state = WiFiWaitingDisconnect;
                }
                else 
                {
                    wifiConn.comhdr.state = Idle;
                    return(WiFiIsRunning);
                }
            }
            break;

        case WiFiWaitingDisconnect:
            if(WiFiDisconnect() == Idle)
            {

                if(wifiConn.comhdr.tStart == 0)
                {
                    // go back to the connect
                    wifiConn.comhdr.state = WiFiWaitingConnect;
                }
                else
                {
                    STATE retError = wifiConn.comhdr.tStart;
                    wifiConn.comhdr.tStart = 0;
                    wifiConn.comhdr.state = Idle;
                    return(retError);
                }
            }
            break;

        case WiFiWaitingConnectNone:
            if(deIPcK.wfConnect(wifiConn.ssid, &wifiConn.status))
            {
                wifiConn.comhdr.state = WiFiEnable;
            }
            else if(IsIPStatusAnError(wifiConn.status))
            {
                wifiConn.comhdr.tStart = WiFiConnectionError | wifiConn.status;
                wifiConn.comhdr.state = WiFiWaitingDisconnect;
            }
            break;

        case WiFiWaitingConnectWPAWithKey:
            if(deIPcK.wfConnect(wifiConn.ssid, wifiConn.key.wpa2Key, &wifiConn.status))
            {
                wifiConn.comhdr.state = WiFiEnable;
            }
            else if(IsIPStatusAnError(wifiConn.status))
            {
                wifiConn.comhdr.tStart = WiFiConnectionError | wifiConn.status;
                wifiConn.comhdr.state = WiFiWaitingDisconnect;
            }
            break;

        case WiFiEnable:
            if(HTTPEnable(true))
            {
                wifiConn.comhdr.state = Idle;

                // make this active
                memcpy(&pjcmd.iWiFi.wifiAConn, &wifiConn, sizeof(WiFiConnectInfo));
            }
            else
            {
                wifiConn.comhdr.tStart = WiFiUnableToStartHTTPServer;
                wifiConn.comhdr.state = WiFiWaitingDisconnect;
            }
            break;

        default:
            wifiConn.comhdr.state = Idle;
            return(InvalidState);
            break;
    }

    return(wifiConn.comhdr.state);
}

STATE WiFiLookupConnInfo(DFILE& dFile, WiFiConnectInfo& wifiConn)
{
    static WiFiScanInfo wifiScan = WiFiScanInfo();
    static char sz[128] = {0};
    static char szSSID[DEWF_MAX_SSID_LENGTH+1];
    static char szSSIDMax[DEWF_MAX_SSID_LENGTH+1];
    static uint8_t rssiMax = 0;
    static VOLTYPE volMax  = VOLNONE;
    static IDHDR idHdr = wifiConn.comhdr.idhdr;
    static WiFiConnectInfo wifiConnT =  WiFiConnectInfo();

    STATE retState = Idle;

    switch(wifiConn.comhdr.state)
    {
        case Idle:
            wifiConn.comhdr.state   = WiFiScanning;
            szSSID[0]               = '\0';
            szSSIDMax[0]            = '\0';
            rssiMax                 = 0;
            volMax                  = VOLNONE;
            memcpy(&idHdr , &wifiConn.comhdr.idhdr, sizeof(IDHDR));
            memcpy((void *) &idHdr.mac , &macOpenScope, sizeof(idHdr.mac));

            // fall thru

        case WiFiScanning:
            if((retState = WiFiScan(wifiScan)) == Idle)
            {
                wifiScan.iNetwork = 0;
                wifiConn.comhdr.state = WiFiCheckFileName;
            }
            else if(IsStateAnError(retState))
            {
                wifiConn.comhdr.state = Idle;
                return(retState);
            }
            break;

        case WiFiCheckFileName:

            // see if we are done looking at all available SSIDs
            if(wifiScan.iNetwork >= wifiScan.cNetworks)
            {
                if(volMax != VOLNONE)
                {
                    // no go read the file
                    wifiConn.comhdr.state = WiFiCreateFileName;
                }
                else
                {
                    wifiConn.comhdr.state = Idle;
                    return(WiFiNoNetworksFound);
                }
                
            }

            // get the next SSID
            else if(!deIPcK.getScanInfo(wifiScan.iNetwork, &wifiScan.scanInfo))
            {
                wifiConn.comhdr.state = Idle;
                return(WiFiNoScanData);
            }

            memcpy(szSSID, wifiScan.scanInfo.ssid, wifiScan.scanInfo.ssidLen);
            szSSID[wifiScan.scanInfo.ssidLen] = '\0';

            CFGCreateFileName(idHdr, szSSID, sz, sizeof(sz));

            if(wifiScan.scanInfo.rssi > rssiMax)
            {
                COMHDR  cmdHdr = {{sizeof(COMHDR), WFVER, WIFIPARAM_ID, CFGUNCAL, {0,0,0,0,0,0}}, SMFnNone, Idle, 0, 0};
                uint32_t cbRead;

                // look in flash
                if( DFATFS::fschdrive(DFATFS::szFatFsVols[VOLFLASH]) == FR_OK   && 
                    DFATFS::fschdir(DFATFS::szRoot) == FR_OK                    &&
                    DFATFS::fsexists(sz)                                        && 
                    dFile.fsopen(sz, FA_READ) == FR_OK                          )
                {
                    if( dFile.fssize() == sizeof(WiFiConnectInfo)                   &&  
                        dFile.fsread(&cmdHdr, sizeof(COMHDR), &cbRead) == FR_OK     && 
                        cbRead == sizeof(COMHDR)                                    && 
                        cmdHdr.activeFunc == WIFIFnAutoConnect                      )
                    {
                        rssiMax = wifiScan.scanInfo.rssi;
                        strcpy(szSSIDMax, szSSID);
                        volMax = VOLFLASH;
                    }
                    dFile.fsclose();
                }

                // look on the SD card
                else if( DFATFS::fschdrive(DFATFS::szFatFsVols[VOLSD]) == FR_OK && 
                    DFATFS::fschdir(DFATFS::szRoot) == FR_OK                    &&
                    DFATFS::fsexists(sz)                                        && 
                    dFile.fsopen(sz, FA_READ) == FR_OK                          )
                {
                    if( dFile.fssize() == sizeof(WiFiConnectInfo)                   &&  
                        dFile.fsread(&cmdHdr, sizeof(COMHDR), &cbRead) == FR_OK     && 
                        cbRead == sizeof(COMHDR)                                    && 
                        cmdHdr.activeFunc == WIFIFnAutoConnect                      )
                    {
                        rssiMax = wifiScan.scanInfo.rssi;
                        strcpy(szSSIDMax, szSSID);
                        volMax = VOLSD;
                    }
                    dFile.fsclose();
                }
            }

            // look at the next SSID
            wifiScan.iNetwork++;
            break;

        case WiFiCreateFileName:

            CFGCreateFileName(idHdr, szSSIDMax, sz, sizeof(sz));
            wifiConn.comhdr.state = WiFiReadFile;

            // we are going to read into a temp
            memcpy((void*) &wifiConnT, &wifiConn, sizeof(WiFiConnectInfo));

            // fall thru

         case WiFiReadFile:
            if((retState = IOReadFile(dFile, volMax, sz, wifiConnT.comhdr.idhdr)) == Idle)
            {
                // make sure we have the correct calibration version.
                if((wifiConnT.comhdr.idhdr.ver != WFVER))
                {
                    wifiConn.comhdr.state = Idle;
                    wifiConn.ssid[0] = '\0';
                    return(CFGUnableToReadConfigFile);
                }

                // we got it.
                memcpy((void*) &wifiConn, &wifiConnT, sizeof(WiFiConnectInfo));

                // we are done, and we read the file
                wifiConn.comhdr.state = Idle;
            }

            // something bad happened
            else if(IsStateAnError(retState))
            {
                wifiConn.comhdr.state = Idle;
                wifiConn.ssid[0] = '\0';
                return(retState);
            }
            else
            {
                // have to do this we are reading in wifiConn and that could
                // blow our state away, so we must put it back.
                wifiConn.comhdr.state = WiFiReadFile;
            }
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }

    return(wifiConn.comhdr.state);
}

STATE WiFiLoadConnInfo(DFILE& dFile, VOLTYPE const vol, char const szSSID[], WiFiConnectInfo& wifiConn)
{
    static char sz[128] = {0};
    STATE retState = Idle;

    switch(wifiConn.comhdr.state)
    {
        case Idle:
            memcpy((void *) &wifiConn.comhdr.idhdr.mac , &macOpenScope, sizeof(wifiConn.comhdr.idhdr.mac));
            wifiConn.ssid[0] = '\0';
            wifiConn.comhdr.state = WiFiCreateFileName;
            // fall thru

        case WiFiCreateFileName:
            CFGCreateFileName(wifiConn.comhdr.idhdr, szSSID, sz, sizeof(sz));
            wifiConn.comhdr.state = WiFiCheckFileName;
            break;

        case WiFiCheckFileName:
            if((retState = (STATE) DFATFS::fschdrive(DFATFS::szFatFsVols[vol])) != (STATE) FR_OK   || (retState = (STATE) DFATFS::fschdir(DFATFS::szRoot)) != (STATE) FR_OK)
            {
                wifiConn.comhdr.state = Idle;
                return(retState | CFGMountError);
            }
            else if(!DFATFS::fsexists(sz))
            {
                wifiConn.comhdr.state = Idle;
                return(WiFiNoMatchingSSID);
            }
            else
            {
                wifiConn.comhdr.state = WiFiReadFile;
            }
            break;

         case WiFiReadFile:
            if((retState = IOReadFile(dFile, vol, sz, wifiConn.comhdr.idhdr)) == Idle)
            {
                wifiConn.comhdr.state = Idle;
            }
            else if(IsStateAnError(retState))
            {
                wifiConn.comhdr.state = Idle;
                return(retState);
            }
            else
            {
                // have to do this we are reading in wifiConn and that could
                // blow our state away, so we must put it back.
                wifiConn.comhdr.state = WiFiReadFile;
            }
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;

    }

    return(wifiConn.comhdr.state);
}

STATE WiFiSaveConnInfo(DFILE& dFile, VOLTYPE const vol, WiFiConnectInfo& wifiConn)
{
    static char sz[128] = {0};
    STATE retState = Idle;

    if(sz[0] == '\0')
    {
        CFGCreateFileName(wifiConn.comhdr.idhdr, wifiConn.ssid, sz, sizeof(sz));
    }
    
    if((retState = IOWriteFile(dFile, vol, sz, wifiConn.comhdr.idhdr)) == Idle)
    {
        sz[0] = '\0';
        return(Idle);
    }
    else if(IsStateAnError(retState))
    {
        sz[0] = '\0';
        return(retState);
    }
    else
    {
        return(WiFiWriteFile);
    }
}
