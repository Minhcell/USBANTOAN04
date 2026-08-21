#include "crypto.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstring>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

// ---- PBKDF2-HMAC-SHA256 ----
QByteArray deriveKey(const QString& pw, const QByteArray& salt, int iter, int dkLen){
    QByteArray out(dkLen, '\0');
    QByteArray pwb = pw.toUtf8();
    BCRYPT_ALG_HANDLE hPrf = NULL;
    if(BCryptOpenAlgorithmProvider(&hPrf, BCRYPT_SHA256_ALGORITHM, NULL,
            BCRYPT_ALG_HANDLE_HMAC_FLAG) != STATUS_SUCCESS)
        return QByteArray();
    NTSTATUS st = BCryptDeriveKeyPBKDF2(
        hPrf,
        (PUCHAR)pwb.data(), (ULONG)pwb.size(),
        (PUCHAR)salt.data(), (ULONG)salt.size(),
        (ULONGLONG)iter,
        (PUCHAR)out.data(), (ULONG)dkLen, 0);
    BCryptCloseAlgorithmProvider(hPrf, 0);
    if(st != STATUS_SUCCESS) return QByteArray();
    return out;
}

QByteArray randomBytes(int n){
    QByteArray b(n, '\0');
    if(BCryptGenRandom(NULL, (PUCHAR)b.data(), (ULONG)n,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG) != STATUS_SUCCESS){
        // du phong (khong nen xay ra)
        for(int i=0;i<n;i++) b[i] = (char)(rand() & 0xFF);
    }
    return b;
}

// ---- AES-256-CTR qua ECB ----
AesCtr::AesCtr() : m_hAlg(NULL), m_hKey(NULL), m_ok(false) {}
AesCtr::~AesCtr(){
    if(m_hKey) BCryptDestroyKey((BCRYPT_KEY_HANDLE)m_hKey);
    if(m_hAlg) BCryptCloseAlgorithmProvider((BCRYPT_ALG_HANDLE)m_hAlg, 0);
}

bool AesCtr::init(const QByteArray& key){
    BCRYPT_ALG_HANDLE hAlg=NULL;
    if(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0) != STATUS_SUCCESS)
        return false;
    // ECB de sinh keystream (moi khoi counter doc lap)
    if(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
            (PUCHAR)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0) != STATUS_SUCCESS){
        BCryptCloseAlgorithmProvider(hAlg,0); return false;
    }
    DWORD objLen=0, cb=0;
    if(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0) != STATUS_SUCCESS){
        BCryptCloseAlgorithmProvider(hAlg,0); return false;
    }
    m_keyObj.resize((int)objLen);
    BCRYPT_KEY_HANDLE hKey=NULL;
    if(BCryptGenerateSymmetricKey(hAlg, &hKey, (PUCHAR)m_keyObj.data(), objLen,
            (PUCHAR)key.data(), (ULONG)key.size(), 0) != STATUS_SUCCESS){
        BCryptCloseAlgorithmProvider(hAlg,0); return false;
    }
    m_hAlg = hAlg; m_hKey = hKey; m_ok = true;
    return true;
}

// Cong 1 vao bo dem counter 16 byte (big-endian, tang tai cho)
static inline void ctrInc(unsigned char* c){
    for(int i=15;i>=0;i--){ if(++c[i]!=0) break; }
}
// Cong 1 gia tri 64-bit vao counter 16 byte big-endian (tai cho)
static void ctrAdd(unsigned char* c, quint64 v){
    unsigned int carry = 0;
    for(int i=15; i>=0; i--){
        unsigned int add = (unsigned int)(v & 0xFF) + carry;
        v >>= 8;
        unsigned int sum = (unsigned int)c[i] + add;
        c[i] = (unsigned char)(sum & 0xFF);
        carry = sum >> 8;
        if(v==0 && carry==0) break;
    }
}

bool AesCtr::process(const unsigned char iv[16], quint64 startBlock,
                     const char* in, char* out, int len){
    if(!m_ok || len<=0) return m_ok && len==0;
    int numBlocks = (len + 15) / 16;
    QByteArray ks; ks.resize(numBlocks*16);
    unsigned char ctr[16]; memcpy(ctr, iv, 16);
    ctrAdd(ctr, startBlock);
    unsigned char* p = (unsigned char*)ks.data();
    for(int j=0;j<numBlocks;j++){
        memcpy(p + j*16, ctr, 16);
        ctrInc(ctr);
    }
    // ECB ma hoa toan bo cac khoi counter -> keystream (dung AES-NI)
    ULONG res=0;
    NTSTATUS st = BCryptEncrypt((BCRYPT_KEY_HANDLE)m_hKey,
            p, (ULONG)(numBlocks*16), NULL,
            NULL, 0,
            p, (ULONG)(numBlocks*16), &res, 0);
    if(st != STATUS_SUCCESS) return false;
    // XOR keystream voi du lieu (trinh bien dich tu vector hoa -> nhanh)
    for(int i=0;i<len;i++) out[i] = in[i] ^ (char)p[i];
    return true;
}
