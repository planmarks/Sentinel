/* Public-domain SHA-256 + HMAC + PBKDF2 (compact). */
#include "sha256.h"
#include <string.h>

#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))

static const uint32_t K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };

static void sha256_block(sha256_ctx *c, const uint8_t *p) {
  uint32_t w[64], a,b,cc,d,e,f,g,h,t1,t2;
  for (int i = 0; i < 16; i++)
    w[i] = (uint32_t)p[i*4]<<24 | (uint32_t)p[i*4+1]<<16 | (uint32_t)p[i*4+2]<<8 | p[i*4+3];
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15]>>3);
    uint32_t s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2]>>10);
    w[i] = w[i-16] + s0 + w[i-7] + s1;
  }
  a=c->state[0];b=c->state[1];cc=c->state[2];d=c->state[3];
  e=c->state[4];f=c->state[5];g=c->state[6];h=c->state[7];
  for (int i = 0; i < 64; i++) {
    uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25);
    uint32_t ch = (e & f) ^ ((~e) & g);
    t1 = h + S1 + ch + K[i] + w[i];
    uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22);
    uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
    t2 = S0 + maj;
    h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
  }
  c->state[0]+=a;c->state[1]+=b;c->state[2]+=cc;c->state[3]+=d;
  c->state[4]+=e;c->state[5]+=f;c->state[6]+=g;c->state[7]+=h;
}

void sha256_init(sha256_ctx *c) {
  c->state[0]=0x6a09e667;c->state[1]=0xbb67ae85;c->state[2]=0x3c6ef372;c->state[3]=0xa54ff53a;
  c->state[4]=0x510e527f;c->state[5]=0x9b05688c;c->state[6]=0x1f83d9ab;c->state[7]=0x5be0cd19;
  c->bitlen=0; c->idx=0;
}

void sha256_update(sha256_ctx *c, const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;
  while (len--) {
    c->buf[c->idx++] = *p++;
    c->bitlen += 8;
    if (c->idx == 64) { sha256_block(c, c->buf); c->idx = 0; }
  }
}

void sha256_final(sha256_ctx *c, uint8_t out[32]) {
  uint64_t bits = c->bitlen;
  c->buf[c->idx++] = 0x80;
  if (c->idx > 56) { while (c->idx < 64) c->buf[c->idx++]=0; sha256_block(c,c->buf); c->idx=0; }
  while (c->idx < 56) c->buf[c->idx++] = 0;
  for (int i = 7; i >= 0; i--) c->buf[c->idx++] = (uint8_t)(bits >> (i*8));
  sha256_block(c, c->buf);
  for (int i = 0; i < 8; i++) {
    out[i*4]   = (uint8_t)(c->state[i]>>24);
    out[i*4+1] = (uint8_t)(c->state[i]>>16);
    out[i*4+2] = (uint8_t)(c->state[i]>>8);
    out[i*4+3] = (uint8_t)(c->state[i]);
  }
}

void sha256(const void *data, size_t len, uint8_t out[32]) {
  sha256_ctx c; sha256_init(&c); sha256_update(&c, data, len); sha256_final(&c, out);
}

void hmac_sha256(const uint8_t *key, size_t keylen,
                 const uint8_t *msg, size_t msglen, uint8_t out[32]) {
  uint8_t k[64], ipad[64], opad[64], ihash[32];
  memset(k, 0, 64);
  if (keylen > 64) { sha256(key, keylen, k); }
  else memcpy(k, key, keylen);
  for (int i = 0; i < 64; i++) { ipad[i] = k[i]^0x36; opad[i] = k[i]^0x5c; }
  sha256_ctx c;
  sha256_init(&c); sha256_update(&c, ipad, 64); sha256_update(&c, msg, msglen); sha256_final(&c, ihash);
  sha256_init(&c); sha256_update(&c, opad, 64); sha256_update(&c, ihash, 32); sha256_final(&c, out);
}

static pbkdf2_progress_cb s_prog = 0;
void pbkdf2_set_progress(pbkdf2_progress_cb cb) { s_prog = cb; }

void pbkdf2_sha256(const uint8_t *pw, size_t pwlen,
                   const uint8_t *salt, size_t saltlen,
                   uint32_t iters, uint8_t *out, size_t outlen) {
  uint8_t U[32], T[32], salt_i[128];
  uint32_t block = 1;
  size_t done = 0;
  uint32_t total = iters * (uint32_t)((outlen + 31) / 32);
  uint32_t base = 0;
  while (done < outlen) {
    size_t sl = saltlen;
    if (sl > sizeof(salt_i)-4) sl = sizeof(salt_i)-4;
    memcpy(salt_i, salt, sl);
    salt_i[sl+0]=(uint8_t)(block>>24); salt_i[sl+1]=(uint8_t)(block>>16);
    salt_i[sl+2]=(uint8_t)(block>>8);  salt_i[sl+3]=(uint8_t)(block);
    hmac_sha256(pw, pwlen, salt_i, sl+4, U);
    memcpy(T, U, 32);
    for (uint32_t i = 1; i < iters; i++) {
      hmac_sha256(pw, pwlen, U, 32, U);
      for (int j = 0; j < 32; j++) T[j] ^= U[j];
      if (s_prog && (i & 63) == 0) s_prog(base + i, total);
    }
    base += iters;
    size_t n = outlen - done; if (n > 32) n = 32;
    memcpy(out + done, T, n);
    done += n; block++;
  }
  if (s_prog) s_prog(total, total);
}
