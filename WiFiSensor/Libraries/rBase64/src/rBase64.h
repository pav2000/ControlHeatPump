// ---------------------------------------------------------------------------
// Created by Abhijit Bose (boseji) on 22/02/16.
// Copyright 2016 - Under creative commons license 3.0:
//        Attribution-ShareAlike CC BY-SA
//
// This software is furnished "as is", without technical support, and with no 
// warranty, express or implied, as to its usefulness for any purpose.
//
// Thread Safe: No
// Extendable: Yes
//
// @file rBase64.h
//
// @brief 
// Library to provide the BASE64 conversion and vis-versa
// 
// @attribution
// This is based on the prior work done by Markus Sattler for ESP8266 BASE64
// implementation and Per Ejeklint for ArduinoWebsocketServer project
//
// @version API 1.0.0
//
//
// @author boseji - salearj@hotmail.com
// ---------------------------------------------------------------------------

#ifndef _RBASE64_H
#define _RBASE64_H

/* b64_alphabet:
 *      Description: Base64 alphabet table, a mapping between integers
 *                   and base64 digits
 *      Notes: This is an extern here but is defined in Base64.c
 */
extern const char b64_alphabet[];

/* base64_encode:
 *      Description:
 *          Encode a string of characters as base64
 *      Parameters:
 *          output: the output buffer for the encoding, stores the encoded string
 *          input: the input buffer for the encoding, stores the binary to be encoded
 *          inputLen: the length of the input buffer, in bytes
 *      Return value:
 *          Returns the length of the encoded string
 *      Requirements:
 *          1. output must not be null or empty
 *          2. input must not be null
 *          3. inputLen must be greater than or equal to 0
 */
size_t rbase64_encode(char *output, char *input, size_t inputLen);

/* base64_decode:
 *      Description:
 *          Decode a base64 encoded string into bytes
 *      Parameters:
 *          output: the output buffer for the decoding,
 *                  stores the decoded binary
 *          input: the input buffer for the decoding,
 *                 stores the base64 string to be decoded
 *          inputLen: the length of the input buffer, in bytes
 *      Return value:
 *          Returns the length of the decoded string
 *      Requirements:
 *          1. output must not be null or empty
 *          2. input must not be null
 *          3. inputLen must be greater than or equal to 0
 */
size_t rbase64_decode(char *output, char *input, size_t inputLen);

/* base64_enc_len:
 *      Description:
 *          Returns the length of a base64 encoded string whose decoded
 *          form is inputLen bytes long
 *      Parameters:
 *          inputLen: the length of the decoded string
 *      Return value:
 *          The length of a base64 encoded string whose decoded form
 *          is inputLen bytes long
 *      Requirements:
 *          None
 */
size_t rbase64_enc_len(size_t inputLen);

/* base64_dec_len:
 *      Description:
 *          Returns the length of the decoded form of a
 *          base64 encoded string
 *      Parameters:
 *          input: the base64 encoded string to be measured
 *          inputLen: the length of the base64 encoded string
 *      Return value:
 *          Returns the length of the decoded form of a
 *          base64 encoded string
 *      Requirements:
 *          1. input must not be null
 *          2. input must be greater than or equal to zero
 */
size_t rbase64_dec_len(char *input, size_t inputLen);

/*
  Base 64 Class for Implementing the bidirectional transactions on BASE64
 */
class rBASE64 {
    public:
        String encode(uint8_t *data, size_t length);
        String encode(char *data);
        String encode(String text);
        String decode(uint8_t *data, size_t length);
        String decode(char *data);
        String decode(String text);
};

extern rBASE64 rbase64;

#endif // _RBASE64_H
