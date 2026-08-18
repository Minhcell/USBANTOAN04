#ifndef DISKIO_H
#define DISKIO_H

#include <windows.h>
#include <QString>
#include <cstdint>

// ====== I/O sector thô trên \\.\PhysicalDriveN ======
// Mở/đóng handle disk (NO_BUFFERING để bỏ read-cache + căn chỉnh sector).
HANDLE  diskOpen(int diskNumber);
void    diskClose(HANDLE h);

// Đọc/ghi theo sector. buf phải căn chỉnh sector khi dùng handle NO_BUFFERING.
// Các hàm này tự cấp buffer căn chỉnh nội bộ cho lần đọc/ghi nhỏ.
QByteArray diskReadSectors(HANDLE h, quint64 sector, quint32 count);
bool       diskWriteSectors(HANDLE h, quint64 sector, const QByteArray& data);

// Buffer căn chỉnh (VirtualAlloc) cho ghi/đọc khối lớn (streaming)
void*  allocAligned(size_t size);
void   freeAligned(void* p);

// Lấy số PhysicalDrive từ ký tự ổ (vd "E:\\")
int    getPhysicalDriveNumber(const QString& driveRoot);
// Tổng dung lượng disk (byte)
quint64 getDiskLengthBytes(int diskNumber);
// Đọc kích thước partition thứ idx (1-based) từ MBR: trả về (startLBA, numSectors)
bool    readMbrPartition(int diskNumber, int idx, quint64& startLBA, quint64& numSectors);

#endif // DISKIO_H
