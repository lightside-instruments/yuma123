/*
    RFC 6991 hex-string formated data support, from hex_string.c

*/

#include "procdefs.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/


/**
 * Decode a hex-string string.
 * This function decodes the supplied hex-string string. It has the
 * following constraints:
 * 1 - If any non hex-string characters are encountered the length of
 *     decoded string will be truncated.
 * 2 - CR and LF characters in the encoded string will be skipped.
 *
 * \param inbuff pointer to buffer of hex-string chars
 * \param inbufflen number of chars in inbuff
 * \param outbuff pointer to the output buffer to use
 * \param outbufflen the length of outbuff.
 * \param retlen the number of decoded bytes
 * \return NO_ERR if all OK
 *         ERR_BUFF_OVFL if outbuff not big enough
 */
status_t hex_string_decode ( const uint8_t* inbuff, uint32_t inbufflen,
                            uint8_t* outbuff, uint32_t outbufflen,
                            uint32_t* retlen );

#ifdef __cplusplus
}  /* end extern 'C' */
#endif
