/************************************************************************/
/*                                                                      */
/*    LexJSON.h                                                         */
/*                                                                      */
/*    Header for the JSON parser                                        */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/* 
*
* Copyright (c) 2013-2016, Digilent <www.digilentinc.com>
* Contact Digilent for the latest version.
*
* This program is free software; distributed under the terms of 
* BSD 3-clause license ("Revised BSD License", "New BSD License", or "Modified BSD License")
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1.    Redistributions of source code must retain the above copyright notice, this
*        list of conditions and the following disclaimer.
* 2.    Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
* 3.    Neither the name(s) of the above-listed copyright holder(s) nor the names
*        of its contributors may be used to endorse or promote products derived
*        from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/************************************************************************/
/*  Revision History:                                                   */
/*    7/11/2016(KeithV): Created                                        */
/************************************************************************/

#ifndef JSONLex_h
#define JSONLex_h

#ifdef __cplusplus

class JSONCallBack
{
public:
 
    typedef enum
    {
        tokNone = 0,
        tokFalse,         
        tokNull,       
        tokTrue,
        tokObject,
        tokEndObject,
        tokArray,
        tokEndArray,
        tokNumber,
        tokMemberName,
        tokStringValue,
        tokNameSep,
        tokValueSep,
        tokJSONSyntaxError,
        tokLexingError,
        tokEndOfJSON,
        tokAny
    } JSONTOKEN;
        
    virtual STATE ParseToken(char const * const szToken, uint32_t cbToken, JSONTOKEN jsonToken) = 0; 
};

class JSON : public JSONCallBack
{

private:

    static const uint32_t maxAggregate      = 80;   // depth is 1/2 == 40 
    static const uint32_t maxObjArray       = 127;  //int8_t positive number
    static const uint32_t cAny              = 0xFF;

    typedef enum
    {
        parValue,          
        parName,           
        parValueSep,       
        parNameSep,        
        parArray          
    } PARCD;

    STATE           state;
    STATE           tokenLexState;
    PARCD           parState;
    JSONTOKEN       jsonToken;
    uint32_t        cbToken;
    int32_t         cZero;
    uint32_t        iAggregate;    // even object, odd array 
    int8_t          aggregate[maxAggregate]; // even object cnt, odd array cnt
    uint8_t         fNegative;
    uint8_t         fFractional;
    uint8_t         fExponent;
    uint8_t         fNegativeExponent;

    char const *    SkipWhite(char const * sz, uint32_t cbsz);

protected:
    bool            IsWhite(char const ch);
    STATE           tokenErrorState;
    char const *    szMoveInput;

public:

    JSON() 
    {
        szMoveInput = NULL;
        cbToken     = 0;
        Init();
    }

    void Init(void)
    {
        state               = Idle;
        tokenLexState       = Idle;
        tokenErrorState     = Idle;
        parState            = parValue;
        jsonToken           = tokNone;
        cbToken             = 0;
        cZero               = 0;
        iAggregate          = 0;
        fNegative           = false;
        fFractional         = false;
        fExponent           = false;
        fNegativeExponent   = false;
        memset(aggregate, 0, sizeof(aggregate));

        // this is so the HTTP code can move past the token
        // even when done parsing the JSON
        szMoveInput         += cbToken;
        cbToken             = 0;
    }

    GCMD::ACTION LexJSON(char const * szInput, uint32_t cbInput);
};

#endif

#endif