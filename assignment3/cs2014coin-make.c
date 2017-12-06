/*!
* @file cs2014coin-make.c
* @brief This is the implementation of the cs2014 coin maker
*
* It should go without saying that these coins are for play:-)
*
* This is part of CS2014
*    https://down.dsg.cs.tcd.ie/cs2014/examples/c-progs-2/README.html
*/

/*
* Copyright (c) 2017 duffyow@tcd.ie
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mbedtls/config.h"
#include "mbedtls/platform.h"

#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"
#include <mbedtls/error.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>

#include <arpa/inet.h>
// needed for getting access to /dev/random
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/random.h>

#include "cs2014coin.h"
#include "cs2014coin-int.h"

#define ECPARAMS	MBEDTLS_ECP_DP_SECP521R1
#define NONCELENGTH 32
#define POWHASHLENGTH 242
#define HASHOUTPUTLENGTH 32

// random byte generation taken from assignment1/rndbytes by stephen.farrell@cs.tcd.ie

unsigned char rndbyte()
{
	unsigned long int s;
	syscall(SYS_getrandom, &s, sizeof(unsigned long int), 0);
	unsigned char byte=(s>>16)%256;
	return(byte);
}

void generateRandomNonce(unsigned char* stream, size_t numBytes)
{
	for(size_t i=0; i < numBytes; i++)
	{
		stream[i] = rndbyte();
	}
	return;
}

/*!
* @brief make a coin
* @param bits specifies how many bits need to be zero in the hash-output
* @param buf is an allocated buffer for the coid
* @param buflen is an in/out parameter reflecting the buffer-size/actual-coin-size
* @return the random byte
*
* Make me a coin of the required quality/strength
*
*/
int cs2014coin_make(int bits, unsigned char *buf, int *buflen)
{
	//housekeeping
	mbedtls_entropy_context entropy;
	mbedtls_pk_context key;
	mbedtls_ctr_drbg_context ctr_drbg;
	const char *pers = "gen_key";

	int lengthofbuffer = *buflen;
	memset(buf, 0, lengthofbuffer);

	//point where to write to in buf
	unsigned char *writeHere = buf;


	//Set the Ciphersuite in buf
	int temp = (CS2014COIN_CS_0);
	memcpy(writeHere, &temp, 4);
	writeHere+=4;


	//Set bit difficulty in buf
	temp= htonl(bits);
	memcpy(writeHere, &temp, 4);
	writeHere+=4;

	int rv;

	mbedtls_pk_init( &key );
	mbedtls_ctr_drbg_init( &ctr_drbg );
	mbedtls_ctr_drbg_seed(&ctr_drbg,mbedtls_entropy_func,&entropy,pers,strlen(pers));


	//Public Key
	int ret;
	if( ( ret = mbedtls_pk_setup( &key, mbedtls_pk_info_from_type( MBEDTLS_PK_ECKEY ) ) ) != 0 )
	{
		mbedtls_printf( " failed\n  !  mbedtls_pk_setup returned -0x%04x", -ret );

	}
	rv = mbedtls_ecp_gen_key( MBEDTLS_ECP_DP_SECP521R1, mbedtls_pk_ec( key ),
	mbedtls_ctr_drbg_random, &ctr_drbg );
	if (rv!=0)
	{
		printf("%s\n", "gen_key failure" );
		return(rv);
	}

	unsigned char keybuf[16000];
	int keyLength = mbedtls_pk_write_pubkey_der(&key, keybuf, sizeof(keybuf));

	//Add publicKeyLength to buf
	temp = htonl(keyLength);
	memcpy(writeHere, &temp, 4);
	writeHere+=4;

	//find start of key in buf
	int offset = sizeof(keybuf) - keyLength;
	unsigned char *keyStart = keybuf + offset;
	memcpy(writeHere, keyStart, keyLength);
	writeHere+=keyLength;

	//add Nonce length to buf
	temp= htonl(NONCELENGTH);
	memcpy(writeHere, &temp, 4);
	writeHere+=4;

	//creat Nonce,
	unsigned char nonce[16000];
	generateRandomNonce(nonce, NONCELENGTH);
	// and write to buffer
	memcpy(writeHere, &nonce, NONCELENGTH);
	//make note of it's location in buf for later use
	unsigned char *nonceLocation = writeHere;
	writeHere+=NONCELENGTH;

	//add POWHASHLENGTH to buf
	temp= htonl(HASHOUTPUTLENGTH);
	memcpy(writeHere, &temp, 4);
	writeHere+=4;

	// create an array to output hash to
	unsigned char hash[16000];
	mbedtls_md_context_t sha_ctx;
	mbedtls_md_init(&sha_ctx);
	rv = mbedtls_md_setup( &sha_ctx, mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 ), 1 );
	int POWFound = 0;
	int i=0;

	while(i<CS2014COIN_MAXITER && POWFound==0)
	{
		mbedtls_md_starts(&sha_ctx);
		mbedtls_md_update(&sha_ctx,(unsigned char *) buf, POWHASHLENGTH);
		mbedtls_md_finish(&sha_ctx, hash);
		if (zero_bits(bits, hash ,32))
		{
			POWFound=1;
			memcpy(writeHere, &hash, 32);
			writeHere+=HASHOUTPUTLENGTH;
			break;
		}
		generateRandomNonce(nonce, NONCELENGTH);
		memcpy(nonceLocation, &nonce, NONCELENGTH);
		i++;
	}

	if(POWFound == 1)
	{
		unsigned char hashToSign[16000];
		mbedtls_md_starts(&sha_ctx);
		mbedtls_md_update(&sha_ctx,(unsigned char *) buf, POWHASHLENGTH);
		mbedtls_md_finish(&sha_ctx, hashToSign);

		
		unsigned char signature[16000];
		size_t olen = 0;

		if( ( ret = mbedtls_pk_sign( &key, MBEDTLS_MD_SHA256, hashToSign, 0, signature, &olen,
			mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 )
		{
			mbedtls_printf( " failed\n  ! mbedtls_pk_sign returned -0x%04x\n", -ret );
		}
		temp=htonl(olen);
		memcpy(writeHere, &temp, 4);
		writeHere+=4;

		memcpy(writeHere, signature, olen);
		(*buflen)=246+olen;

		//tidy up
		mbedtls_md_free(&sha_ctx);
		mbedtls_pk_free(&key);

		return(0);

	}
	else
	{
		printf("%s\n", "Failure. No coin found");
		return(CS2014COIN_GENERR);

	}


}
