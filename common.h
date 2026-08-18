#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <cstdint>

// ====== Hằng số chung (giống bản Python) ======
#define APP_NAME     "USB AN TOAN"
#define APP_VER      "10C-Cpp"
#define USB_EXE      "USB_AN_TOAN.exe"
#define ADMIN_PW     "M@nh6868"

static const quint32 SECTOR    = 512;
static const quint64 HDR_SEC   = 0;      // sector chứa magic "SECVAULT"
static const quint64 CFG_SEC   = 1;      // sector chứa config (JSON)
static const quint64 TBL_START = 8;      // bảng entry bắt đầu
static const quint64 TBL_END   = 1032;   // bảng entry kết thúc
static const quint64 DATA_START= 1032;   // dữ liệu file bắt đầu
static const quint32 ENTRY_SZ  = 512;    // mỗi entry 1 sector
static const quint32 MAX_FILES = 2048;

// magic ở HDR_SEC để nhận diện USB AN TOAN (8 byte)
static const char SEC_MAGIC[8] = {'S','E','C','V','A','U','L','T'};
// magic đầu file stream (raw, KHÔNG mã hoá)
static const char FILE_MAGIC[4] = {'S','R','A','W'};

// Định dạng dung lượng có số lẻ giống H04 (vd 164.606 MB)
inline QString fmtSize(double n){
    if(n < 1024.0) return QString::number((qint64)n) + " B";
    const char* units[4] = {"KB","MB","GB","TB"};
    for(int i=0;i<4;i++){
        n/=1024.0;
        if(n<1024.0) return QString::number(n,'f',3) + " " + units[i];
    }
    return QString::number(n,'f',3) + " PB";
}

#endif // COMMON_H
