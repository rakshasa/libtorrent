/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is SHA 180-1 Reference Implementation (Optimized)
 * 
 * The Initial Developer of the Original Code is Paul Kocher of
 * Cryptography Research.  Portions created by Paul Kocher are 
 * Copyright (C) 1995-9 by Cryptography Research, Inc.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *
 *     Paul Kocher
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef _SHA_FAST_H_
#define _SHA_FAST_H_

#include <inttypes.h>

namespace torrent {

typedef enum _SECStatus {
    SECWouldBlock = -2,
    SECFailure = -1,
    SECSuccess = 0
} SECStatus;

struct SHA1ContextStr {
  union {
    uint32_t w[80];		/* input buffer, plus 64 words */
    uint8_t  b[320];
  } u;
  uint32_t H[5];		/* 5 state variables */
  uint32_t sizeHi,sizeLo;	/* 64-bit count of hashed bytes. */
};

typedef SHA1ContextStr SHA1Context;

/******************************************/
/*
** SHA-1 secure hash function
*/

/*
** Hash a null terminated string "src" into "dest" using SHA-1
*/
extern SECStatus SHA1_Hash(unsigned char *dest, const char *src);

/*
** Hash a non-null terminated string "src" into "dest" using SHA-1
*/
extern SECStatus SHA1_HashBuf(unsigned char *dest, const unsigned char *src,
			      uint32_t src_length);

/*
** Create a new SHA-1 context
*/
extern SHA1Context *SHA1_NewContext(void);


/*
** Destroy a SHA-1 secure hash context.
**	"cx" the context
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void SHA1_DestroyContext(SHA1Context *cx, bool freeit);

/*
** Reset a SHA-1 context, preparing it for a fresh round of hashing
*/
extern void SHA1_Begin(SHA1Context *cx);

/*
** Update the SHA-1 hash function with more data.
**	"cx" the context
**	"input" the data to hash
**	"inputLen" the amount of data to hash
*/
extern void SHA1_Update(SHA1Context *cx, const unsigned char *input,
			unsigned int inputLen);

/*
** Finish the SHA-1 hash function. Produce the digested results in "digest"
**	"cx" the context
**	"digest" where the 16 bytes of digest data are stored
**	"digestLen" where the digest length (20) is stored
**	"maxDigestLen" the maximum amount of data that can ever be
**	   stored in "digest"
*/
extern void SHA1_End(SHA1Context *cx, unsigned char *digest,
		     unsigned int *digestLen, unsigned int maxDigestLen);

/*
** trace the intermediate state info of the SHA1 hash.
*/
extern void SHA1_TraceState(SHA1Context *cx);

/*
 * Return the the size of a buffer needed to flatten the SHA-1 Context into
 *    "cx" the context
 *  returns size;
 */
extern unsigned int SHA1_FlattenSize(SHA1Context *cx);

/*
 * Flatten the SHA-1 Context into a buffer:
 *    "cx" the context
 *    "space" the buffer to flatten to
 *  returns status;
 */
extern SECStatus SHA1_Flatten(SHA1Context *cx,unsigned char *space);

/*
 * Resurrect a flattened context into a SHA-1 Context
 *    "space" the buffer of the flattend buffer
 *    "arg" ptr to void used by cryptographic resurrect
 *  returns resurected context;
 */
extern SHA1Context * SHA1_Resurrect(unsigned char *space, void *arg);

}

#endif /* _SHA_FAST_H_ */
