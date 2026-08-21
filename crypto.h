#pragma once
#include <QByteArray>
#include <QString>

// Sinh khoa 32 byte tu mat khau bang PBKDF2-HMAC-SHA256 (BCrypt).
QByteArray deriveKey(const QString& pw, const QByteArray& salt, int iter=200000, int dkLen=32);

// Sinh ngau nhien n byte.
QByteArray randomBytes(int n);

// AES-256-CTR (ma hoa == giai ma, chi XOR keystream). Tang toc bang AES-NI.
// Dung ECB de sinh keystream tu cac khoi bo dem (counter).
class AesCtr {
public:
    AesCtr();
    ~AesCtr();
    bool init(const QByteArray& key);        // mo khoa ECB 1 lan
    // Xu ly len byte tai vi tri khoi startBlock (= soByteDaXuLy/16).
    // in/out co the trung nhau (xu ly tai cho). Tra ve false neu loi.
    bool process(const unsigned char iv[16], quint64 startBlock,
                 const char* in, char* out, int len);
    bool ok() const { return m_ok; }
private:
    void* m_hAlg;
    void* m_hKey;
    QByteArray m_keyObj;
    bool m_ok;
};
