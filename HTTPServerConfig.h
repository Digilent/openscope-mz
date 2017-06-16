/************************************************************************/
/*                                                                      */
/*    HTTPServerConfig.h                                                */
/*                                                                      */
/*    The network and WiFi configuration file required                  */
/*    to specify the network parameters to the network libraries        */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2013, Digilent Inc.                                     */
/************************************************************************/
/* 
*
* Copyright (c) 2013-2014, Digilent <www.digilentinc.com>
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
/*    7/19/2013(KeithV): Created                                        */
/************************************************************************/
#if !defined(_WEBSERVERCONFIG_H)
#define	_WEBSERVERCONFIG_H

//************************************************************************
//************************************************************************
//**************************  READ THIS  *********************************
//************************************************************************
//***********  !! COPY THIS SKETCH TO YOUR SKETCH DIRECTORY !! ***********
//************************************************************************
//************************************************************************
//************** You will not be able to modify the network **************
//************** parameters as long as this sketch is in a  **************
//************* library subdirectory. Even if you modify in **************
//************* MPIDE it will not take effect when you build  ************
//************** and you will not be able to connect to ******************
//************** your router, the connection will timeout ****************
//************************************************************************
//************************************************************************

//************************************************************************
//************************************************************************
//******************  Sketch controlled defines  *************************
//************************************************************************
//************************************************************************
// the sketch controlled IO pin configuration

// if you are connected to router that is NOT connected to the internet
// you will not be able to get internet related services such as the 
// current time. Such functions if called will delay until a timeout
// to by pass such function define NOINTERNET. If you are connected to the
// internet, comment out the define.
#define NOTIME

// how many sockets/connections will we be able to handle at once? Also required
#define cMaxSocketsToListen 10

// this will be instantiated in ProcessServer
#ifdef INCLUDE_SERVER_DATA
//************************************************************************
//************************************************************************
//******************  SET THESE VALUES FOR YOUR NETWORK  *****************
//************************************************************************
//************************************************************************

// You have a choice of either calculating a static IP based on LocalStaticIP (next variable)
// Or setting the whole IP yourself. If you want DHCP to supply and automatically set
// your network parameters, set this to {0,0,0,0}. However, you then MUST set localStaticIP.
// If you fully specify your static IP, then you do NOT need to set localStaticIP
// but you MUST then set the remaining Gateway, subnet, and DNS values below.

//static IPv4 ipMyStatic = {192,168,1,225};    // a place to calculate our static IP 
static IPv4 ipMyStatic = {0,0,0,0};    // a place to calculate our static IP 

// This will be ignored if ipMyStatic is NOT set to {0,0,0,0}
// If ipMyStatic == {0,0,0,0} AND localStaticIP == 0, then the full IP returned by DHCP will be used
// Otherwise this is the last octet in the IP address... so if DHCP comes up with a subnet of 192.168.1.x
// this will set your IP address to 192.168.1.(localStaticIP) for example if 
// localStaticIP == 190; your final IP address would be 192.168.1.190 assuming the subnet address of 192.168.1.x
//static byte localStaticIP = 10;   // this will be the gateway IP with the last octet of the IP being 195
//static byte localStaticIP = 205;   // this will be the gateway IP with the last octet of the IP being 195
//static byte localStaticIP = 201;   // this will be the gateway IP with the last octet of the IP being 195
//static byte localStaticIP = 235;   // this will be the gateway IP with the last octet of the IP being 195
static uint8_t localStaticIP = 0;   // this will be the gateway IP with the last octet of the IP being 195

// Set the port to listen on; this is a required item
static unsigned short listeningPort = 80;      // 80 is the default for an HTTP server

// You ONLY MUST set these if you specifically assigned ipMyStatic to a static
// IP address other than {0,0,0,0}; otherwise DHCP will overwrite these.
static IPv4 ipGateway   = {192,168,1,1};
static IPv4 subnetMask  = {255,255,255,0};

// here you set some default DNS servers
// make sure that cDNSServer is big enough to handle all of the DNS servers
// you initialize. By default cDNSServers is 4 but check DEIPcK.h for the current value
// if you attempt to initialize more than the size of the array, you will only initialize
// cDNSServers.
static IPv4 rgIpDNS[] = {{8,8,8,8}, {8,8,4,4}};

#endif // INCLUDE_SERVER_DATA

//************************************************************************
//************************************************************************
//***************************** END OF CONFIGURATION *********************
//************************************************************************
//************************************************************************

#endif // _WEBSERVERCONFIG_H
