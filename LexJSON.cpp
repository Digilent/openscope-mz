/************************************************************************/
/*                                                                      */
/*    LexJSON.cpp                                                       */
/*                                                                      */
/*    JSON Lexer / Parser                                               */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*    7/11/2016(KeithV): Created                                        */
/************************************************************************/
#include    <OpenScope.h>

/************************************************************************/
/*    RFC 7159 say JSON-text = ws value ws                              */
/*    however JSON-text = ws object ws                                  */
/*    is a valid subset that we are going to adhear to.                 */
/************************************************************************/

bool JSON::IsWhite(char const ch)
{
    switch(ch)
    {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return(true);
            break;

        default:
            return(false);
            break;
    }

    return(false);
}

char const *  JSON::SkipWhite(char const * sz, uint32_t cbsz)
{
    uint32_t i = 0;

    for(i=0; i<cbsz && IsWhite(sz[i]); i++);
    return(sz + i);
}

GCMD::ACTION  JSON::LexJSON(char const * szInput, uint32_t cbInput)
{
    szMoveInput = szInput;

    while(true)
    {

        // this should never happen on a state of Idle 
        // as cbToken == 0 at Idle
        if(cbToken > cbInput) return(GCMD::READ);

        switch(state)
        {
            case Idle:
                parState = parValue;
                state = JSONSkipWhite;
                // fall thru

            case JSONSkipWhite:
                {
                    cbToken = 0;
                    szMoveInput = SkipWhite(szMoveInput, cbInput - (szMoveInput - szInput));

                    // found the start of a token
                    if(szMoveInput == szInput && cbInput > 0)
                    {
                        state = JSONToken;
                        break;
                    }

                    // found the start of the token but need to shift the input
                    // we don't need to read more characters.
                    else if(szMoveInput < (szInput + cbInput))
                    {
                        return(GCMD::CONTINUE);
                    }

                    // we need to shift and read more characters
                    else
                    {
                        return(GCMD::READ);
                    }
                }
                break;

            case JSONToken:
                switch(szInput[0])
                {

                    //VALUES

                    case 'f':
                        if(parState != parValue)
                        {
                            state = JSONSyntaxError;
                            break;
                        }
                        jsonToken = tokFalse;
                        cbToken = 5;
                        state = JSONfalse;
                        break;

                    case 'n':
                        if(parState != parValue)
                        {
                            state = JSONSyntaxError;
                            break;
                        }
                        jsonToken = tokNull;
                        cbToken = 4;
                        state = JSONnull;
                        break;

                    case 't':
                        if(parState != parValue)
                        {
                            state = JSONSyntaxError;
                            break;
                        }
                        jsonToken = tokTrue;
                        cbToken = 4;
                        state = JSONtrue;
                        break;

                    case '{':
                        if(parState != parValue)
                        {
                            state = JSONSyntaxError;
                            break;
                        }

                        // we are in an object, up the object count
                        // nesting more in an object.
                        if((iAggregate % 2) == 0)
                        {
                            if(aggregate[iAggregate] == maxObjArray)
                            {
                                state = JSONNestingError;
                                break;
                            }
                            else
                            {
                                aggregate[iAggregate]++;
                            }
                        }

                        // was in an array, now in an object
                        // make sure we have room in our aggregate array
                        else if(iAggregate+1 >= maxAggregate)
                        {
                            state = JSONNestingError;
                            break;
                        }

                        // was in an array, now in an object
                        else
                        {
                            iAggregate++;
                            aggregate[iAggregate] = 1;
                        }

                        jsonToken = tokObject;
                        cbToken = 1;
                        state = JSONCallOSLex;
                        break;

                    case '[':
                        if(parState != parValue)
                        {
                            state = JSONSyntaxError;
                            break;
                        }

                        // we are in an array, up the array count
                        // nesting more in an array.
                        if((iAggregate % 2) == 1)
                        {
                            if(aggregate[iAggregate] == maxObjArray)
                            {
                                state = JSONNestingError;
                                break;
                            }
                            else
                            {
                                aggregate[iAggregate]++;
                            }
                        }

                        // was in an object, now in an array
                        // don't have to worry about overshooting
                        // as the aggregate array has an even value
                        // and that will always occur on an object bump
                        else
                        {
                            iAggregate++;
                            aggregate[iAggregate] = 1;
                        }

                        jsonToken = tokArray;
                        cbToken = 1;
                        state = JSONCallOSLex;
                        break;

                    case '"':
                        cbToken = 2;        // I can ask for another char as there must be an end "
                        state = JSONString;
                        
                        switch(parState)
                        {
                            case parName:
                                jsonToken = tokMemberName;
                                break;

                            case parValue:
                                jsonToken = tokStringValue;
                                break;

                            default:
                                jsonToken = tokNone;
                                state = JSONSyntaxError;
                                break;
                        }
                        break;

                    case '-':
                        fNegative = true;
                        // fall thru

                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        if(parState != parValue)
                        {
                            state = JSONSyntaxError;
                            break;
                        }

                        if(szInput[0] == '0') cZero = 0;
                        else cZero = cAny;

                        // normal case
                        jsonToken = tokNumber;
                        cbToken = 2;  
                        state = JSONNumber;

                        break;

                    // NAME SEPARATORS
                    case ':':
                        if(parState != parNameSep)
                        {
                            state = JSONSyntaxError;
                            break;
                        }
                        jsonToken = tokNameSep;
                        cbToken = 1;
                        state = JSONCallOSLex;
                        break;

                    // VALUE SEPARATORS
                    case ',':
                        if(parState != parValueSep)
                        {
                            state = JSONSyntaxError;
                            break;
                        }
                        jsonToken = tokValueSep;
                        cbToken = 1;
                        state = JSONCallOSLex;
                        break;

                    case ']':
                        if(parState != parValueSep || (iAggregate % 2) != 1 || aggregate[iAggregate] < 1)
                        {
                            state = JSONSyntaxError;
                            break;
                        }

                        // comming out of an array
                        aggregate[iAggregate]--;

                        // if this is the closing array bump down to the object
                        if(aggregate[iAggregate] == 0) iAggregate--;

                        jsonToken = tokEndArray;
                        parState = parValueSep;
                        cbToken = 1;
                        state = JSONCallOSLex;
                        break;

                    case '}':
                        if(parState != parValueSep || (iAggregate % 2) != 0 || aggregate[iAggregate] < 1)
                        {
                            state = JSONSyntaxError;
                            break;
                        }

                        // coming out of an object
                        aggregate[iAggregate]--;

                        // if this is the closing object, bump down to an array
                        // unless this is the bottom and there is nothing below.
                        if(aggregate[iAggregate] == 0 && iAggregate > 0) iAggregate--;

                        jsonToken = tokEndObject;
                        parState = parValueSep;
                        cbToken = 1;
                        state = JSONCallOSLex;
                        break;


                    default:
                        state = JSONSyntaxError;
                        break;
                }
                break;

            case JSONfalse:
                if(memcmp(szInput, "false", 5) == 0) state = JSONCallOSLex;
                else state = JSONSyntaxError;
                break;

            case JSONnull:
                if(memcmp(szInput, "null", 4) == 0) state = JSONCallOSLex;
                else state = JSONSyntaxError;
                break;

            case JSONtrue:
                if(memcmp(szInput, "true", 4) == 0) state = JSONCallOSLex;
                else state = JSONSyntaxError;
                break;

            case JSONString:
                for(; cbToken <= cbInput; cbToken++)
                {
                    if(szInput[cbToken-1] == '"' && szInput[cbToken-2] != '\\')
                    {
                        // the question is, should the underlying code
                        // get escape sequences, or should they somehow be stripped
                        // here. My conclusion is, since I don't know the character set
                        // let the underlying code process the escaped characters.
                        state = JSONCallOSLex;
                        break;
                    }
                }           
                break;

                // what if the number is the last in the file
            case JSONNumber:
                for(; cbToken <= cbInput; cbToken++)
                {
                    char cch = szInput[cbToken-1];

                    if(cch == '0')
                    {
                        if(cZero == cAny)
                        {
                            continue;
                        }
                        else if(cZero == 0)
                        {
                            state = JSONSyntaxError;
                            break;
                        }
                        else
                        {
                            cZero--;
                            continue;
                        }
                        break;
                    }
                    else if(isdigit(cch))
                    {
                        cZero = cAny;
                        continue;
                    }
                    else if(cch == '.')
                    {
                        if(fFractional || fExponent)
                        {
                            state = JSONSyntaxError;
                            break;
                        }
                        else
                        {
                            cZero = cAny;
                            fFractional = true;
                        }
                    }
                    else if(cch == 'e' || cch == 'E')
                    {
                        if(fExponent)
                        {
                            state = JSONSyntaxError;
                            break;
                        }
                        else
                        {
                            fExponent = true;
                            cZero = 0;
                        }
                    }
                    else if(cch == '+' && fExponent && (szInput[cbToken-2] == 'e' || szInput[cbToken-2] == 'E'))
                    {
                            cZero = 0;
                    }
                    else if(cch == '-' && fExponent && (szInput[cbToken-2] == 'e' || szInput[cbToken-2] == 'E'))
                    {
                            fNegativeExponent = true;
                            cZero = 0;
                    }
                    else if(IsWhite(cch) || cch == ',' || cch == ']' || cch == '}')
                    {
                        cbToken--;
                        state = JSONCallOSLex;
                        break;
                    }
                    else
                    {
                        state = JSONSyntaxError;
                        break;
                    }
                } 

                // done processing this number, reset the flags for the next number
                if(state != JSONNumber)
                {
                    fNegative           = false;
                    fFractional         = false;
                    fExponent           = false;
                    fNegativeExponent   = false;
                }
                break;

            case JSONCallOSLex:

                // call the OpenScope lexer here
                // LexOpenScope(char const * szJSON, uint32_t cbJSON);
                if((tokenLexState = ParseToken(szInput, cbToken, jsonToken)) == Idle)
                {
                    state = JSONNextToken;

                }
                else if(IsStateAnError(tokenLexState))
                {
                    state = JSONTokenLexingError;
                }

                // we want to give up the time on each token processing
                // This could take a lot of time.
                return(GCMD::CONTINUE);
                break;
              
           case JSONNextToken:

                if(jsonToken == tokEndOfJSON)
                {
                    state = Done;
                    break;
                }

                if(iAggregate == 0 && aggregate[iAggregate] == 0)
                {
                    jsonToken = tokEndOfJSON;
                    cbToken = 0;
                    state = JSONCallOSLex;
                    break;
                }

                // skip ws
                szMoveInput =  szInput + cbToken;
                state = JSONSkipWhite;

                // put in what the next parsing state is
                switch(jsonToken)
                {
                    case tokFalse:
                    case tokNull:
                    case tokTrue:
                    case tokEndObject:
                    case tokEndArray:
                    case tokNumber:
                    case tokStringValue:
                         parState = parValueSep;
                        break;

                    case tokMemberName:
                        parState = parNameSep;
                        break;

                    case tokObject:
                        parState = parName;
                        break;

                    case tokValueSep:
                        // if we are at the object level
                        // value separators separate members
                        // so we would be looking for a member name
                        if((iAggregate % 2) == 0) parState = parName;

                        // otherwise we are in an array looking for a value
                        else parState = parValue;
                        break;

                    case tokNameSep:
                    case tokArray:
                        parState = parValue;
                        break;

                    case tokJSONSyntaxError:
                    default:
                        state = JSONSyntaxError;
                        break;
                }
                break;

            case JSONTokenLexingError:
                tokenErrorState = tokenLexState;
                state = Done;
                break; 

             case JSONSyntaxError:
                tokenErrorState = JSONLexingError;
                state = Done;
                break;

             case JSONNestingError:
                tokenErrorState = JSONObjArrayNestingError;
                state = Done;
                break;

            case Done:
                // all good, we are done.
                if(tokenErrorState == Idle) return(GCMD::DONE);
                else return(GCMD::ERROR);
                break;
        }
    }

    return(GCMD::CONTINUE);
}

