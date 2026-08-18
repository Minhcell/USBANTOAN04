#include "diskio.h"
#include "common.h"
#include <winioctl.h>

HANDLE diskOpen(int diskNumber){
    QString path = QString("\\\\.\\PhysicalDrive%1").arg(diskNumber);
    // NO_BUFFERING: bỏ cache Windows + căn chỉnh. KHÔNG WRITE_THROUGH -> ghi nhanh.
    HANDLE h = CreateFileW((LPCWSTR)path.utf16(),
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING,
                           FILE_FLAG_NO_BUFFERING, NULL);
    return h;
}

void diskClose(HANDLE h){
    if(h && h != INVALID_HANDLE_VALUE){
        FlushFileBuffers(h);
        CloseHandle(h);
    }
}

void* allocAligned(size_t size){
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
void freeAligned(void* p){
    if(p) VirtualFree(p, 0, MEM_RELEASE);
}

static bool seekSector(HANDLE h, quint64 sector){
    LARGE_INTEGER li; li.QuadPart = (LONGLONG)(sector * SECTOR);
    return SetFilePointerEx(h, li, NULL, FILE_BEGIN);
}

QByteArray diskReadSectors(HANDLE h, quint64 sector, quint32 count){
    QByteArray out;
    if(!h || h==INVALID_HANDLE_VALUE || count==0) return out;
    quint32 bytes = count * SECTOR;
    void* buf = allocAligned(bytes);
    if(!buf) return out;
    if(seekSector(h, sector)){
        DWORD rd=0;
        if(ReadFile(h, buf, bytes, &rd, NULL) && rd>0)
            out = QByteArray((const char*)buf, (int)rd);
    }
    freeAligned(buf);
    return out;
}

bool diskWriteSectors(HANDLE h, quint64 sector, const QByteArray& data){
    if(!h || h==INVALID_HANDLE_VALUE || data.isEmpty()) return false;
    // pad tới bội số sector
    quint32 n = (quint32)data.size();
    quint32 padded = ((n + SECTOR - 1)/SECTOR)*SECTOR;
    void* buf = allocAligned(padded);
    if(!buf) return false;
    memset(buf, 0, padded);
    memcpy(buf, data.constData(), n);
    bool ok=false;
    if(seekSector(h, sector)){
        DWORD wr=0;
        ok = WriteFile(h, buf, padded, &wr, NULL) && wr==padded;
    }
    freeAligned(buf);
    return ok;
}

int getPhysicalDriveNumber(const QString& driveRoot){
    // driveRoot dạng "E:\\" -> mở \\.\E:
    QString letter = driveRoot.left(2); // "E:"
    QString path = QString("\\\\.\\%1").arg(letter);
    HANDLE h = CreateFileW((LPCWSTR)path.utf16(), 0,
                           FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, 0, NULL);
    if(h==INVALID_HANDLE_VALUE) return -1;
    STORAGE_DEVICE_NUMBER sdn; DWORD ret=0;
    int num=-1;
    if(DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0,
                       &sdn, sizeof(sdn), &ret, NULL)){
        num = (int)sdn.DeviceNumber;
    }
    CloseHandle(h);
    return num;
}

quint64 getDiskLengthBytes(int diskNumber){
    QString path = QString("\\\\.\\PhysicalDrive%1").arg(diskNumber);
    HANDLE h = CreateFileW((LPCWSTR)path.utf16(), 0,
                           FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, 0, NULL);
    if(h==INVALID_HANDLE_VALUE) return 0;
    GET_LENGTH_INFORMATION gli; DWORD ret=0; quint64 len=0;
    if(DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0,
                       &gli, sizeof(gli), &ret, NULL)){
        len = (quint64)gli.Length.QuadPart;
    }
    CloseHandle(h);
    return len;
}

bool readMbrPartition(int diskNumber, int idx, quint64& startLBA, quint64& numSectors){
    HANDLE h = diskOpen(diskNumber);
    if(h==INVALID_HANDLE_VALUE) return false;
    QByteArray mbr = diskReadSectors(h, 0, 1);
    diskClose(h);
    if(mbr.size() < 512) return false;
    int base = 446 + 16*(idx-1);
    const unsigned char* p = (const unsigned char*)mbr.constData();
    quint32 lba = p[base+8] | (p[base+9]<<8) | (p[base+10]<<16) | ((quint32)p[base+11]<<24);
    quint32 num = p[base+12] | (p[base+13]<<8) | (p[base+14]<<16) | ((quint32)p[base+15]<<24);
    if(lba==0) return false;
    startLBA = lba; numSectors = num;
    return true;
}
