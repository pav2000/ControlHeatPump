// Arduino DNS client for WizNet5100-based Ethernet shield
// (c) Copyright 2009-2010 MCQN Ltd.
// Released under Apache License, version 2.0
//
// Added TCP-DNS client by vad7@yahoo.com

#ifndef DNSClient_h
#define DNSClient_h

#include <EthernetUdp.h>
#include "EthernetClient.h"

class DNSClient
{
public:
	DNSClient(uint8_t p = 0) { protocol = p; } // udp=0, tcp=1
    void begin(const IPAddress& aDNSServer);

    /** Convert a numeric IP address string into a four-byte IP address.
        @param aIPAddrString IP address to convert
        @param aResult IPAddress structure to store the returned IP address
        @result 1 if aIPAddrString was successfully converted to an IP address,
                else error code
    */
    int8_t inet_aton(const char *aIPAddrString, IPAddress& aResult);

    /** Resolve the given hostname to an IP address.
        @param aHostname Name to be resolved
        @param aResult IPAddress structure to store the returned IP address
        @result 1 if aIPAddrString was successfully converted to an IP address,
                else error code
    */
    virtual int8_t getHostByName(const char* aHostname, IPAddress& aResult);
    //  использовать конкретный сокет для запроса dns
    virtual int8_t getHostByName(const char* aHostname, IPAddress& aResult, uint8_t sock); // pav2000
    void 		   set_protocol(uint8_t tcp); // udp=0, tcp=1
    void 		   write(const uint8_t *buf, size_t size);
    int 		   read(uint8_t *buf, size_t size);
    uint8_t 	   get_protocol(void) { return protocol; }

protected:
    void   BuildRequest(const char* aName);
    int8_t ProcessResponse(uint16_t aTimeout, IPAddress& aAddress);

    IPAddress iDNSServer;
    uint16_t  iRequestId;
    uint8_t   protocol; // udp=0, tcp=1
    EthernetUDP iUdp;
    EthernetClient iTCP;
};

#endif
