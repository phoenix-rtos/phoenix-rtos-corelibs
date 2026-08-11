/*
 * Phoenix-RTOS
 *
 * libsha256 - single file sha256 extracted from libtomcrypt
 *
 * webpage: https://www.libtom.net/LibTomCrypt/
 * repo: https://github.com/libtom/libtomcrypt
 * commit: 7e863d21429f94ed6a720e24499a12a3f852bb31
 * see sbom.json for the tracking entry
 *
 * SPDX-License-Identifier: Unlicense
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "libsha256.h"

/* disabling formatting to allow comparison with upstream */
/* clang-format off */

/*****************************************************************************
 * src/headers/tomcrypt_hash.h
 *****************************************************************************
 * NOTE: upstream hashes share one "hash_state" union; with a single algorithm
 * extracted it has one member, so it is aliased to the public sha256_ctx_t
 * instead of being redeclared - the code below stays comparable with upstream
 * while its prototypes are checked against libsha256.h
 ****************************************************************************/
typedef sha256_ctx_t hash_state;


/*****************************************************************************
 * src/headers/tomcrypt_argchk.h
 ****************************************************************************/
#define LTC_ARGCHK(x)   if (!(x)) return CRYPT_INVALID_ARG;


/*****************************************************************************
 * src/headers/tomcrypt_macros.h
 ****************************************************************************/
#define LOAD32H(x, y)                            \
  do { x = ((ulong32)((y)[0] & 255)<<24) | \
           ((ulong32)((y)[1] & 255)<<16) | \
           ((ulong32)((y)[2] & 255)<<8)  | \
           ((ulong32)((y)[3] & 255)); } while(0)

#define STORE32H(x, y)                                                                     \
  do { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
       (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); } while(0)

#define STORE64H(x, y)                                                                     \
do { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);     \
     (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);     \
     (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);     \
     (y)[6] = (unsigned char)(((x)>>8)&255); (y)[7] = (unsigned char)((x)&255); } while(0)

#define ROLc(x, y) ( (((ulong32)(x)<<(ulong32)((y)&31)) | (((ulong32)(x)&0xFFFFFFFFUL)>>(ulong32)((32-((y)&31))&31))) & 0xFFFFFFFFUL)
#define RORc(x, y) ( ((((ulong32)(x)&0xFFFFFFFFUL)>>(ulong32)((y)&31)) | ((ulong32)(x)<<(ulong32)((32-((y)&31))&31))) & 0xFFFFFFFFUL)
#define MIN(x, y) ( ((x)<(y))?(x):(y) )


/*****************************************************************************
 * src/headers/tomcrypt_custom.h
 ****************************************************************************/
#define XMEMCPY  memcpy


/*****************************************************************************
 * src/headers/tomcrypt.h
 ****************************************************************************/
enum {
   CRYPT_OK=0,             /* Result OK */
   CRYPT_ERROR,            /* Generic Error */
   CRYPT_NOP,              /* Not a failure but no operation was performed */
   CRYPT_FAIL_TESTVECTOR,  /* Algorithm failed test vectors */
   CRYPT_INVALID_ARG,      /* Generic invalid argument */
   CRYPT_HASH_OVERFLOW      /* Hash applied to too many bits */
};

/* tomcrypt_cfg.h */
typedef uint32_t ulong32;
typedef uint64_t ulong64;


/*****************************************************************************
 * src/headers/tomcrypt_private.h
 ****************************************************************************/

/* a simple macro for making hash "process" functions */
#define HASH_PROCESS(func_name, compress_name, state_var, block_size)                       \
int func_name (hash_state * md, const unsigned char *in, unsigned long inlen)               \
{                                                                                           \
    unsigned long n;                                                                        \
    int           err;                                                                      \
    LTC_ARGCHK(md != NULL);                                                                 \
    LTC_ARGCHK(in != NULL);                                                                 \
    if (md-> state_var .curlen > sizeof(md-> state_var .buf)) {                             \
       return CRYPT_INVALID_ARG;                                                            \
    }                                                                                       \
    if (((md-> state_var .length + inlen * 8) < md-> state_var .length)                     \
          || ((inlen * 8) < inlen)) {                                                       \
      return CRYPT_HASH_OVERFLOW;                                                           \
    }                                                                                       \
    while (inlen > 0) {                                                                     \
        if (md-> state_var .curlen == 0 && inlen >= block_size) {                           \
           if ((err = compress_name (md, in)) != CRYPT_OK) {                                \
              return err;                                                                   \
           }                                                                                \
           md-> state_var .length += block_size * 8;                                        \
           in             += block_size;                                                    \
           inlen          -= block_size;                                                    \
        } else {                                                                            \
           n = MIN(inlen, (block_size - md-> state_var .curlen));                           \
           XMEMCPY(md-> state_var .buf + md-> state_var.curlen, in, (size_t)n);             \
           md-> state_var .curlen += n;                                                     \
           in             += n;                                                             \
           inlen          -= n;                                                             \
           if (md-> state_var .curlen == block_size) {                                      \
              if ((err = compress_name (md, md-> state_var .buf)) != CRYPT_OK) {            \
                 return err;                                                                \
              }                                                                             \
              md-> state_var .length += 8*block_size;                                       \
              md-> state_var .curlen = 0;                                                   \
           }                                                                                \
       }                                                                                    \
    }                                                                                       \
    return CRYPT_OK;                                                                        \
}


/*****************************************************************************
 * src/misc/mem_neq.c
 ****************************************************************************/
int mem_neq(const void *a, const void *b, size_t len)
{
   unsigned char ret = 0;
   const unsigned char* pa;
   const unsigned char* pb;

   LTC_ARGCHK(a != NULL);
   LTC_ARGCHK(b != NULL);

   pa = a;
   pb = b;

   while (len-- > 0) {
      ret |= *pa ^ *pb;
      ++pa;
      ++pb;
   }

   ret |= ret >> 4;
   ret |= ret >> 2;
   ret |= ret >> 1;
   ret &= 1;

   return ret;
}


/*****************************************************************************
 * src/hashes/sha2/sha256.c
 ****************************************************************************
 * Preprocessor directives were resolved with:
 * - LTC_SHA256 defined
 * - LTC_SMALL_CODE defined
 * - LTC_CLEAN_STACK undefined
 */

/* the K array */
static const ulong32 K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
    0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
    0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
    0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
    0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
    0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
    0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
    0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
    0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

/* Various logical functions */
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y))
#define S(x, n)         RORc((x),(n))
#define R(x, n)         (((x)&0xFFFFFFFFUL)>>(n))
#define Sigma0(x)       (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)       (S(x, 6) ^ S(x, 11) ^ S(x, 25))
#define Gamma0(x)       (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)       (S(x, 17) ^ S(x, 19) ^ R(x, 10))

/* compress 512-bits */
static int s_sha256_compress(hash_state * md, const unsigned char *buf)
{
    ulong32 S[8], W[64], t0, t1;
    ulong32 t;
    int i;

    /* copy state into S */
    for (i = 0; i < 8; i++) {
        S[i] = md->sha256.state[i];
    }

    /* copy the state into 512-bits into W[0..15] */
    for (i = 0; i < 16; i++) {
        LOAD32H(W[i], buf + (4*i));
    }

    /* fill W[16..63] */
    for (i = 16; i < 64; i++) {
        W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
    }

    /* Compress */
#define RND(a,b,c,d,e,f,g,h,i)                         \
     t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];   \
     t1 = Sigma0(a) + Maj(a, b, c);                    \
     d += t0;                                          \
     h  = t0 + t1;

     for (i = 0; i < 64; ++i) {
         RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],i);
         t = S[7]; S[7] = S[6]; S[6] = S[5]; S[5] = S[4];
         S[4] = S[3]; S[3] = S[2]; S[2] = S[1]; S[1] = S[0]; S[0] = t;
     }
#undef RND

    /* feedback */
    for (i = 0; i < 8; i++) {
        md->sha256.state[i] = md->sha256.state[i] + S[i];
    }
    return CRYPT_OK;
}

/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int sha256_init(hash_state * md)
{
    LTC_ARGCHK(md != NULL);

    md->sha256.curlen = 0;
    md->sha256.length = 0;
    md->sha256.state[0] = 0x6A09E667UL;
    md->sha256.state[1] = 0xBB67AE85UL;
    md->sha256.state[2] = 0x3C6EF372UL;
    md->sha256.state[3] = 0xA54FF53AUL;
    md->sha256.state[4] = 0x510E527FUL;
    md->sha256.state[5] = 0x9B05688CUL;
    md->sha256.state[6] = 0x1F83D9ABUL;
    md->sha256.state[7] = 0x5BE0CD19UL;
    return CRYPT_OK;
}

/**
   Process a block of memory though the hash
   @param md     The hash state
   @param in     The data to hash
   @param inlen  The length of the data (octets)
   @return CRYPT_OK if successful
*/
HASH_PROCESS(sha256_process,s_sha256_compress, sha256, 64)

/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (32 bytes)
   @return CRYPT_OK if successful
*/
int sha256_done(hash_state * md, unsigned char *out)
{
    int i;

    LTC_ARGCHK(md  != NULL);
    LTC_ARGCHK(out != NULL);

    if (md->sha256.curlen >= sizeof(md->sha256.buf)) {
       return CRYPT_INVALID_ARG;
    }


    /* increase the length of the message */
    md->sha256.length += md->sha256.curlen * 8;

    /* append the '1' bit */
    md->sha256.buf[md->sha256.curlen++] = (unsigned char)0x80;

    /* if the length is currently above 56 bytes we append zeros
     * then compress.  Then we can fall back to padding zeros and length
     * encoding like normal.
     */
    if (md->sha256.curlen > 56) {
        while (md->sha256.curlen < 64) {
            md->sha256.buf[md->sha256.curlen++] = (unsigned char)0;
        }
        s_sha256_compress(md, md->sha256.buf);
        md->sha256.curlen = 0;
    }

    /* pad upto 56 bytes of zeroes */
    while (md->sha256.curlen < 56) {
        md->sha256.buf[md->sha256.curlen++] = (unsigned char)0;
    }

    /* store length */
    STORE64H(md->sha256.length, md->sha256.buf+56);
    s_sha256_compress(md, md->sha256.buf);

    /* copy output */
    for (i = 0; i < 8; i++) {
        STORE32H(md->sha256.state[i], out+(4*i));
    }
    return CRYPT_OK;
}

/* clang-format on */
