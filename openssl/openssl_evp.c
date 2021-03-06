/**
 * The MIT License
 *
 * Copyright (c) 2018-2020 Ilwoong Jeong (https://github.com/ilwoong)
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
 */

#include <stdint.h>
#include <stdbool.h>

#include <openssl/conf.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include "../print_hex.h"

void handleErrors(void)
{
    ERR_print_errors_fp(stderr);
    abort();
}

void CheckError(int state) {
    if (state != 1) {
        printf("State: %d\n", state);
        handleErrors();
    }
}

EVP_CIPHER_CTX* get_encrypt_ctx(const EVP_CIPHER* cipher_mode, const uint8_t* key, const uint8_t* iv) 
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(ctx == NULL) {
        handleErrors();
    }

    CheckError(EVP_EncryptInit_ex(ctx, cipher_mode, NULL, key, iv));

    return ctx;
}

EVP_CIPHER_CTX* get_decrypt_ctx(const EVP_CIPHER* cipher_mode, const uint8_t* key, const uint8_t* iv) 
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(ctx == NULL) {
        handleErrors();
    }

    CheckError(EVP_DecryptInit_ex(ctx, cipher_mode, NULL, key, iv));

    return ctx;
}

size_t evp_encrypt(uint8_t* ct, const uint8_t* pt, size_t pt_len, EVP_CIPHER_CTX* ctx) 
{
    int len = 0;    
    size_t ct_len = 0;
    
    CheckError(EVP_EncryptUpdate(ctx, ct, &len, pt, pt_len));    
    ct_len = len;

    CheckError(EVP_EncryptFinal_ex(ctx, ct + len, &len));
    ct_len += len;

    EVP_CIPHER_CTX_free(ctx);

    return ct_len;
}

size_t evp_decrypt(uint8_t* pt, const uint8_t* ct, size_t ct_len, EVP_CIPHER_CTX* ctx)
{
    int len = 0;
    size_t pt_len = 0;
        
    CheckError(EVP_DecryptUpdate(ctx, pt, &len, ct, ct_len));    
    pt_len = len;
    
    CheckError(EVP_DecryptFinal_ex(ctx, pt + len, &len));
    pt_len += len;

    EVP_CIPHER_CTX_free(ctx);

    return pt_len;
}

int evp_gcm_encrypt(const EVP_CIPHER* cipher, const uint8_t* plaintext, size_t plaintext_len, 
    const uint8_t* aad, size_t aad_len, const uint8_t* key, const uint8_t* iv, size_t iv_len,
	uint8_t* ciphertext, uint8_t* tag)
{
	int len = 0;
	int ciphertext_len = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	
	if (ctx == NULL) {
        handleErrors();
    }

	CheckError(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL));

	CheckError(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL));
	CheckError(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv));
	CheckError(EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len));
	
    CheckError(EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len));
	ciphertext_len = len;
	
	CheckError(EVP_EncryptFinal_ex(ctx, ciphertext + len, &len));
	ciphertext_len += len;

	CheckError(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag));

	EVP_CIPHER_CTX_free(ctx);

	return ciphertext_len;
}

int evp_gcm_decrypt(const EVP_CIPHER* cipher, const uint8_t* ciphertext, size_t ciphertext_len, 
    const uint8_t* aad,	size_t aad_len, uint8_t *tag, 
    const uint8_t *key, const uint8_t* iv, size_t iv_len, uint8_t *plaintext)
{
	int len = 0;
	int plaintext_len = 0;
	int ret = -1;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
        handleErrors();
    } 

	CheckError(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL));
	
    CheckError(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL));
	CheckError(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv));
	CheckError(EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len));

	CheckError(EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len));
	plaintext_len = len;

	CheckError(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag));

	ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

	EVP_CIPHER_CTX_free(ctx);

	if(ret > 0)
	{	/* Success */
		plaintext_len += len;
		return plaintext_len;
	}
	else
	{	/* Verify failed */
		return -1;
	}
}

void aes_gcm_sample()
{
    uint8_t key[32] = {0, };
    uint8_t iv[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0};

    uint8_t pt[32] = { 0, };
    uint8_t enc[32] = { 0, };
    uint8_t dec[32] = { 0, };
    uint8_t aad[16] = { 0, };
    uint8_t tag[16] = { 0, };
    int ret = 0;

    const EVP_CIPHER* cipher = EVP_aes_256_gcm();
    evp_gcm_encrypt(cipher, pt, 32, aad, 16, key, iv, 16, enc, tag);
    ret = evp_gcm_decrypt(cipher, enc, 32, aad, 16, tag, key, iv, 16, dec);

    printf("AES_256_GCM\n");
    print_hex("    enc", enc, 32);
    print_hex("    tag", tag, 16);

    if (ret > 0){
        print_hex("    dec", dec, 32);
    } else {
        printf("    dec: decryption failed\n");
    }
}

static void makeSequelData(uint8_t* data, uint8_t start, size_t length) {
    for (int i = 0; i < length; ++i) {
        data[i] = (start++);
    }
}

void aes_evp_sample(const EVP_CIPHER* cipher, bool useRandom)
{
    const size_t block_size = 16;
    const size_t length = 32;
    uint8_t key[32] = { 0, };
    uint8_t iv[16] = { 0, };
    uint8_t pt[32] = { 0, };
    uint8_t encrypted[48] = { 0, };
    uint8_t decrypted[48] = { 0, };

    if (useRandom) {
        RAND_bytes(key, 32);
        RAND_bytes(iv, 16);
        RAND_bytes(pt, 32);
    } else {
        makeSequelData(key, 0, 32);
        makeSequelData(iv, 32, 16);
        makeSequelData(pt, 48, 32);
    }

    EVP_CIPHER_CTX* enc_ctx = get_encrypt_ctx(cipher, key, iv);
    EVP_CIPHER_CTX* dec_ctx = get_decrypt_ctx(cipher, key, iv);
    
    int enclen = evp_encrypt(encrypted, pt, length, enc_ctx);
    int declen = evp_decrypt(decrypted, encrypted, enclen, dec_ctx);

    printf("%s\n", EVP_CIPHER_name(cipher));    
    print_hex("    ENC", encrypted, enclen);    
    print_hex("    DEC", decrypted, declen);
    printf("\n");
}

int main(int argc, const char** argv)
{
    bool useRandomData = false;

    aes_evp_sample(EVP_aes_256_ecb(), useRandomData);
    aes_evp_sample(EVP_aes_256_cbc(), useRandomData);
    aes_evp_sample(EVP_aes_256_cfb(), useRandomData);
    aes_evp_sample(EVP_aes_256_ofb(), useRandomData);
    aes_evp_sample(EVP_aes_256_ctr(), useRandomData);
    aes_gcm_sample();

    return 0;
}