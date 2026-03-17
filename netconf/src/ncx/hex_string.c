#include "hex_string.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "log.h"


/**
 * Determine if a character is a valid hex-string value.
 *
 * \param c the character to test.
 * \return true if c is a hex-string value.
 */
static bool is_hex_char(unsigned char c)
{
   return ((c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F'));
}

static uint8_t decode_nibble(char c)
{
    if((c>='0' && c<='9')) {
        return c-'0';
    } else if((c>='a' && c<='f')) {
        return c-'a' + 10;
    } else if((c>='A' && c<='F')) {
        return c-'A' + 10;
    } else {
        assert(0);
    }
}

static void decode_byte(char hex_string_byte[2], uint8_t* byte)
{
    *byte = decode_nibble(hex_string_byte[0])<<4 | decode_nibble(hex_string_byte[1]);
}

static uint8_t encode_nibble(const uint8_t data)
{
    if(data<10) {
        return data+'0';
    } else {
        return data-10+'a';
    }
}

static void encode_byte(const uint8_t data, uint8_t hex_string_2[2])
{

    hex_string_2[0] = encode_nibble ( (data & 0xf0) >> 4 );
    hex_string_2[1] = encode_nibble ( data & 0x0f); 
}

/*************** E X T E R N A L    F U N C T I O N S  *************/

status_t hex_string_encode ( const uint8_t* inbuff, uint32_t inbufflen,
                            uint8_t* outbuff, uint32_t outbufflen,
                            uint32_t linesize, uint32_t* retlen)
{
    uint32_t i,j,wrapindex;
    uint8_t hex_string_2[2];
    uint8_t* outptr;

    assert( inbuff && "hex_string_decode() inbuff is NULL!" );
    assert( outbuff && "hex_string_decode() outbuff is NULL!" );

//    if ( hex_string_get_encoded_str_len( inbufflen, linesize ) > outbufflen ) {
//        return ERR_BUFF_OVFL;
//    }

    outptr=outbuff;
    wrapindex=0;

    for(i=0;i<inbufflen;i++) {
        encode_byte(*(inbuff+i),hex_string_2);


        for(j=0;j<3;j++) {
            if(j<2) {
                *outptr++=hex_string_2[j];
            } else {
                if((i+1)<inbufflen) {
                    *outptr++=':';
                } else {
                    continue;
                }
            }
            if(linesize && ++wrapindex==linesize) {
                *outptr++='\r';
                *outptr++='\n';
                wrapindex=0;
            }
        }
    }

    *retlen=outptr-outbuff;
    *outptr++='\0';
    return NO_ERR;
}

status_t hex_string_decode( const uint8_t* inbuff, uint32_t inbufflen,
                            uint8_t* outbuff, uint32_t outbufflen,
                            uint32_t* retlen )
{
    int i;
    int hex_string_byte_index;
    int expected_colon;
    char hex_string_2[2]; //e.g. 01 9A 9a etc.

    assert( inbuff && "hex_string_decode() inbuff is NULL!" );
    assert( outbuff && "hex_string_decode() outbuff is NULL!" );

    hex_string_byte_index=0;
    *retlen=0;
    expected_colon=0;
    for(i=0;i<inbufflen;i++) {
        if(expected_colon && inbuff[i]==':') {
            expected_colon=0;
        } else if(is_hex_char(inbuff[i])) {
            hex_string_2[hex_string_byte_index++]=inbuff[i];
        } else if(inbuff[i]=='\r' || inbuff[i]=='\n') {
            /*do nothing skip*/
        } else {
            /* encountered a dodgy character */
            log_warn( "hex_string_decode() encountered invalid character(%c), "
                      "output string truncated!", inbuff[i]);
            return ERR_NCX_WRONG_TKVAL;
        }
        if(hex_string_byte_index==2) {
          decode_byte(hex_string_2, outbuff+*retlen);
          hex_string_byte_index=0;
          expected_colon=1;
          *retlen+=1;
        }
    }

    return NO_ERR;
}
