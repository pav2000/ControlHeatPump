// Arduino DNS client for WizNet5100-based Ethernet shield
// (c) Copyright 2009-2010 MCQN Ltd.
// Released under Apache License, version 2.0
//
// Added TCP-DNS client and some optimisation by vad7@yahoo.com

#include "utility/w5100.h"
#include "EthernetUdp.h"
#include "utility/util.h"

#include "Dns.h"
#include <string.h>
//#include <stdlib.h>
#include "Arduino.h"

#include "FreeRTOS_ARM.h"
#define RTOS_delay(ms) { if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(ms/portTICK_PERIOD_MS); else delay(ms); }

#define MAX_RETRIES              2  // first attempt by UDP, second by TCP
// Various flags and header field values for a DNS message
#define UDP_HEADER_SIZE	         8
#define DNS_HEADER_SIZE          12
#define TTL_SIZE                 4
#define QUERY_FLAG               (0)
#define RESPONSE_FLAG            (1<<15)
#define QUERY_RESPONSE_MASK      (1<<15)
#define OPCODE_STANDARD_QUERY    (0)
#define OPCODE_INVERSE_QUERY     (1<<11)
#define OPCODE_STATUS_REQUEST    (2<<11)
#define OPCODE_MASK              (15<<11)
#define AUTHORITATIVE_FLAG       (1<<10)
#define TRUNCATION_FLAG          (1<<9)
#define RECURSION_DESIRED_FLAG   (1<<8)
#define RECURSION_AVAILABLE_FLAG (1<<7)
#define RESP_NO_ERROR            (0)
#define RESP_FORMAT_ERROR        (1)
#define RESP_SERVER_FAILURE      (2)
#define RESP_NAME_ERROR          (3)
#define RESP_NOT_IMPLEMENTED     (4)
#define RESP_REFUSED             (5)
#define RESP_MASK                (15)
#define TYPE_A                   (0x0001)
#define CLASS_IN                 (0x0001)
#define LABEL_COMPRESSION_MASK   (0xC0)
// Port number that DNS servers listen on
#define DNS_PORT                 53

// Possible return codes from ProcessResponse
#define SUCCESS             1
#define TIMED_OUT          -1
#define INVALID_SERVER     -2
#define TRUNCATED          -3
#define CONNECT_ERROR      -4
#define ERROR_SEND         -5
#define START_UDP_ERROR    -6
#define INVALID_RESPONSE   -7
#define INVALID_ANSWER_CNT -8
#define INVALID_RESPONSE2  -9
#define INVALID_RESPONSE3 -10
#define INVALID_RESPONSE4 -11
#define INVALID_RESPONSE5 -12
#define INVALID_RESPONSE6 -13
#define TRUNCATED2        -14

void DNSClient::begin(const IPAddress& aDNSServer)
{

    iDNSServer = aDNSServer;
    iRequestId = 0;
}

int8_t DNSClient::getHostByName(const char* aHostname, IPAddress& aResult)
{
	return getHostByName(aHostname, aResult, MAX_SOCK_NUM);
}

//  использовать конкретный сокет для запроса dns
int8_t DNSClient::getHostByName(const char* aHostname, IPAddress& aResult, uint8_t sock)
{
    int8_t ret = 0;
    static uint8_t save_protocol = protocol;

    // See if it's a numeric IP address
    if (inet_aton(aHostname, aResult)) return SUCCESS;

    // Check we've got a valid DNS server to use
    if (iDNSServer == INADDR_NONE) return INVALID_SERVER;

    for(uint8_t retries = 0; retries < MAX_RETRIES; retries++) {
    	if(protocol == 0) protocol = retries; // switch to TCP if there was the first attempt failure
		if(protocol) {
			if(sock == MAX_SOCK_NUM) {
				ret = iTCP.connect(iDNSServer, DNS_PORT);
			} else {
				ret = iTCP.connect(iDNSServer, DNS_PORT, sock);
			}
		} else {
			if(sock == MAX_SOCK_NUM) {
				if((ret = iUdp.begin((1024+(millis() & 0xF)))) == 0) ret = START_UDP_ERROR;
			} else {
				ret = iUdp.begin((1024+(millis() & 0xF)), sock);
			}
			if(ret > 0) ret = iUdp.beginPacket(iDNSServer, DNS_PORT);
		}
		if(ret == 0) ret = CONNECT_ERROR;
		if(ret > 0) {
			BuildRequest(aHostname);
			if(protocol) ret = iTCP.write((const uint8_t *)NULL, (size_t)0); else ret = iUdp.endPacket();
			if(ret == 0) ret = ERROR_SEND;
			else {
				ret = ProcessResponse(2000, aResult);
			}
			if(protocol) iTCP.stop(); else iUdp.stop();
		}
		if(ret > 0) break;
    }
    if(ret <= 0 && protocol != save_protocol) protocol = save_protocol;
    return ret;
}

void DNSClient::write(const uint8_t *buf, size_t size)
{
	if(protocol) iTCP.write_buffer(buf, size);
	else iUdp.write(buf, size);
}

int DNSClient::read(uint8_t *buf, size_t size)
{
	if(protocol) return iTCP.read(buf, size);
	else return iUdp.read(buf, size);
}

int8_t DNSClient::ProcessResponse(uint16_t aTimeout, IPAddress& aAddress)
{
    int8_t   ret = 0;
    uint16_t answerCount;
    // Wait for a response packet
    // Ожидание ответа пакета
    uint32_t startTime = millis();
    while(1) {
    	WDT_Restart(WDT);
    	if(protocol) {
    		if((answerCount = iTCP.available())) break;
    	} else {
    		if(iUdp.parsePacket() > 0) {
    			answerCount = iUdp.available();
    			break;
    		}
    	}
        if((millis() - startTime) > aTimeout) return TIMED_OUT;
        RTOS_delay(10);
    }

    // We've had a reply! Read the UDP header
    // У нас был ответ!   Чтение заголовка UDP
    static   uint16_t header[DNS_HEADER_SIZE / sizeof(uint16_t)]; // Буфер для заголовка Enough space to reuse for the DNS header
    uint16_t header_flags;
    uint8_t  len;

    // Read through the rest of the response
    // Прочитать оставшуюся часть ответа
    if(answerCount < DNS_HEADER_SIZE)
    {
    	ret = TRUNCATED; // Размер маловат
    } else if(protocol) {
        read((uint8_t *)&answerCount, 2);
        if(htons(answerCount) < DNS_HEADER_SIZE) {
        	ret = TRUNCATED2;
        }
    }
    if(ret >= 0) {
    	ret = read((uint8_t *)header, DNS_HEADER_SIZE) == DNS_HEADER_SIZE;   // читаем заголовок пакета

        header_flags = htons(header[1]);
        // Check that it's a response to this request
        // Проверьте, что это ответ на этот запрос
        if ( iRequestId != header[0] || ((header_flags & QUERY_RESPONSE_MASK) != (uint16_t)RESPONSE_FLAG) )
        {
            // Mark the entire packet as read  ответ не верный
            ret = INVALID_RESPONSE;
        } else {
            // Check for any errors in the response (or in our request)
            // although we don't do anything to get round these
            // Проверяем любые ошибки в ответе (или в нашем запросе)
            // хотя мы ничего не делаем, чтобы обойти эти ошибки
            // TRUNCATION_FLAG=9 RESP_MASK=15
            if ( (header_flags & TRUNCATION_FLAG) || (header_flags & RESP_MASK) )
            {
                // Mark the entire packet as read
            	ret = INVALID_RESPONSE2;
            } else {
                // And make sure we've got (at least) one answer
                // И убедитесь, что у нас есть (по крайней мере) один ответ
                answerCount = htons(header[3]);
                if (answerCount == 0 )
                {
                    // Mark the entire packet as read
                	ret = INVALID_ANSWER_CNT;
                }
            }
        }
    }
    if(ret >= 0) {
        // Skip over any questions
        for (uint16_t i =0; i < htons(header[2]); i++)
        {
            // Skip over the name
            do
            {
                if(read(&len, sizeof(len)) != sizeof(len)) {
                	ret = INVALID_RESPONSE3;
                	break;
                }
                if (len > 0)
                {
                    // Don't need to actually read the data out for the string, just
                    // advance ptr to beyond it
                    while(len--)
                    {
                        read((uint8_t *)header, 1); // we don't care about the returned byte
                    }
                }
            } while (len != 0);

            // Now jump over the type and class
            for (int i =0; i < 4; i++)
            {
                read((uint8_t *)header, 1); // we don't care about the returned byte
            }
        }
    }

    // Now we're up to the bit we're interested in, the answer
    // There might be more than one answer (although we'll just use the first
    // type A answer) and some authority and additional resource records but
    // we're going to ignore all of them.
    if(ret >= 0) {
		for (uint16_t i = 0; i < answerCount; i++)
		{
			// Skip the name
			do
			{
				if(read((uint8_t *)&len, sizeof(len)) != sizeof(len)) {
					ret = INVALID_RESPONSE4;
					break;
				}
				if((len & LABEL_COMPRESSION_MASK) == 0)
				{
					// It's just a normal label
					if (len > 0)
					{
						// And it's got a length
						// Don't need to actually read the data out for the string,
						// just advance ptr to beyond it
						while(len--)
						{
							read((uint8_t *)header, 1); // we don't care about the returned byte
						}
					}
				} else {
					// This is a pointer to a somewhere else in the message for the
					// rest of the name.  We don't care about the name, and RFC1035
					// says that a name is either a sequence of labels ended with a
					// 0 length octet or a pointer or a sequence of labels ending in
					// a pointer.  Either way, when we get here we're at the end of
					// the name
					// Skip over the pointer
					read((uint8_t *)header, 1); // we don't care about the returned byte
					// And set len so that we drop out of the name loop
					len = 0;
				}
			} while (len != 0);

			// Check the type and class
			uint16_t answerType;
			uint16_t answerClass;
			read((uint8_t*)&answerType, sizeof(answerType));
			read((uint8_t*)&answerClass, sizeof(answerClass));

			// Ignore the Time-To-Live as we don't do any caching
			for (int i =0; i < TTL_SIZE; i++)
			{
				read((uint8_t *)header, 1); // we don't care about the returned byte
			}

			// And read out the length of this answer
			// Don't need header_flags anymore, so we can reuse it here
			read((uint8_t*)&header_flags, sizeof(header_flags));

			if ( (htons(answerType) == TYPE_A) && (htons(answerClass) == CLASS_IN) )
			{
				if (htons(header_flags) != 4)
				{
					// It's a weird size
					// Mark the entire packet as read
					ret = INVALID_RESPONSE5;
					break;
				}
				read(aAddress.raw_address(), 4);
				ret = 1; // ok
				break;
			} else {
				// This isn't an answer type we're after, move onto the next one
				for (uint16_t i =0; i < htons(header_flags); i++)
				{
					read((uint8_t *)header, 1); // we don't care about the returned byte
				}
			}
		}
    }
    if(ret == 0) ret = INVALID_RESPONSE6;
    // Mark the entire packet as read
    if(protocol) iTCP.flush(); else iUdp.flush();

    // If we get here then we haven't found an answer
    return ret;
}


void DNSClient::BuildRequest(const char* aName)
{
    // Build header
    //                                    1  1  1  1  1  1
    //      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                      ID                       |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                    QDCOUNT                    |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                    ANCOUNT                    |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                    NSCOUNT                    |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                    ARCOUNT                    |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    // As we only support one request at a time at present, we can simplify
    // some of this header
    uint8_t len;
    iRequestId = millis(); // generate a random ID
    uint16_t twoByteBuffer;
    // FIXME We should also check that there's enough space available to write to, rather
    // FIXME than assume there's enough space (as the code does at present)
    if(protocol) {
    	// Write packet size
    	twoByteBuffer = htons(DNS_HEADER_SIZE + 1+strlen(aName)+1 + 4);
    	write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));
    }
    write((uint8_t*)&iRequestId, sizeof(iRequestId));

    //#define QUERY_FLAG               (0)
    //#define OPCODE_STANDARD_QUERY    (0)
    //#define RECURSION_DESIRED_FLAG   (1<<8)
    twoByteBuffer = htons(QUERY_FLAG | OPCODE_STANDARD_QUERY | RECURSION_DESIRED_FLAG);
    write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

    twoByteBuffer = htons(1);  // One question record
    write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

    twoByteBuffer = 0;  // Zero answer records and zero additional records
    for(len = 0; len < 3; len++) {
    	write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));
    }

    // Build question
    const char* start =aName;
    const char* end =start;
    // Run through the name being requested
    while (*end)
    {
        // Find out how long this section of the name is
        end = start;
        while (*end && (*end != '.') ) end++;
        if (end-start > 0)
        {
            // Write out the size of this section
            len = end-start;
            write(&len, 1);
            // And then write out the section
            write((uint8_t*)start, end-start);
        }
        start = end+1;
    }

    // We've got to the end of the question name, so
    // terminate it with a zero-length section
    len = 0;
    write(&len, 1);
    // Finally the type and class of question
    twoByteBuffer = htons(TYPE_A);
    write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

    twoByteBuffer = htons(CLASS_IN);  // Internet class of question
    write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));
    // Success!  Everything buffered okay

}

int8_t DNSClient::inet_aton(const char* aIPAddrString, IPAddress& aResult)
{
    // See if we've been given a valid IP address
    const char* p =aIPAddrString;
    while (*p &&
// ( (*p == '.') || (*p >= '0') || (*p <= '9') ))
   ( (*p == '.') || ((*p >= '0') && (*p <= '9')) ))         // pav2000
    {
        p++;
    }

    if (*p == '\0')
    {
        // It's looking promising, we haven't found any invalid characters
        p = aIPAddrString;
        int segment =0;
        int segmentValue =0;
        while (*p && (segment < 4))
        {
            if (*p == '.')
            {
                // We've reached the end of a segment
                if (segmentValue > 255)
                {
                    // You can't have IP address segments that don't fit in a byte
                    return 0;
                } else {
                    aResult[segment] = (byte)segmentValue;
                    segment++;
                    segmentValue = 0;
                }
            } else {
                // Next digit
                segmentValue = (segmentValue*10)+(*p - '0');
            }
            p++;
        }
        // We've reached the end of address, but there'll still be the last
        // segment to deal with
        if ((segmentValue > 255) || (segment > 3))
        {
            // You can't have IP address segments that don't fit in a byte,
            // or more than four segments
            return 0;
        } else {
            aResult[segment] = (byte)segmentValue;
            return 1;
        }
    } else {
        return 0;
    }
}
