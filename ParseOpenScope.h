/************************************************************************/
/*                                                                      */
/*    LexOpenScope.h                                                    */
/*                                                                      */
/*    Header for the OpenScope token parser                             */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*    7/11/2016(KeithV): Created                                        */
/************************************************************************/

#ifndef LexOpenScope_h
#define LexOpenScope_h

#include <LexJSON.h>

#ifdef __cplusplus

class OSPAR : public JSON
{
public:
    typedef struct _STRU32
    {
        char const * const  szToken;
        const uint32_t      u32;
    } STRU32;

    typedef struct _ODATA
    {
        STATE *     pLockState;
        int32_t     cb;
        uint8_t  *  pbOut;
    } ODATA;

    typedef enum
    {
        NONE,
        JSON,
        OSJB
    } OSCMD;

private:
    STATE           state;
    STATE           stateNameSep;
    STATE           stateArray;
    STATE           stateValueSep;
    STATE           stateEndArray;
    STATE           stateEndObject;
    STATE           stateOutLock;
    STRU32 const *  rgStrU32;
    uint32_t        cStrU32;
    uint32_t        cbProcessed;
    uint32_t        iStream;
    uint32_t        iBuf;
    uint32_t        iBufMax;
    char            szBuf[0x100];
    char            rgchOut[0x4000]; // 16384 bytes

    // JSON callback routine; Must be supplied
    STATE ParseToken(char const * szToken, uint32_t cbToken, JSONTOKEN jsonToken); 

    uint32_t Uint32FromStr(STRU32 const * const rgStrU32L, uint32_t cStrU32L, char const * const sz, uint32_t cb, STATE defaultState=OSPARSyntaxError);

    friend STATE UIMainPage(DFILE& dFile, VOLTYPE const wifiVol, WiFiConnectInfo& wifiConn);

public:
    bool            fLocked;
    uint32_t        cOData;
    ODATA           odata[4];

    OSPAR()
    {
        Init();
    }

    void Init(void)
    {
        JSON::Init();    
        state           = Idle;
        stateNameSep    = OSPARSyntaxError;
        stateArray      = OSPARSyntaxError;
        stateValueSep   = OSPARSyntaxError;
        stateEndArray   = OSPARSyntaxError;
        stateEndObject  = OSPARSyntaxError;
        rgStrU32        = NULL;
        cStrU32         = 0;
        cbProcessed     = 0;
        iStream         = 0;
        iBuf            = 0;
        iBufMax         = 0;
        fLocked         = false;
        cOData          = 1;
        stateOutLock    = LOCKAvailable;
        odata[0].pbOut  = (uint8_t *) rgchOut;
        odata[0].cb     = 0;
        odata[0].pLockState      = &stateOutLock;
    }

    OSCMD IsOSCmdStart(char ch)
    {
        if(ch == '{') return(OSPAR::JSON);
        else if('1' <= ch && ch <= '9') return(OSPAR::OSJB);

        return(OSPAR::NONE);
    }

    GCMD::ACTION StreamJSON(char const * szStream, uint32_t cbStream);
};
#endif // c++
#endif