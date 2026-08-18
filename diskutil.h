#ifndef DISKUTIL_H
#define DISKUTIL_H

#include <QString>
#include <QStringList>
#include <functional>

struct UsbCandidate { int diskNumber; QString model; quint64 sizeBytes; QString letter; };

// Liệt kê các USB removable để setup chọn
QList<UsbCandidate> listRemovableUsb();

// Chạy diskpart với script
bool runDiskpart(const QString& script);

// Đặt volume read-only (chặn copy trực tiếp)
bool setVolumeReadonly(const QString& driveRoot, bool readonly);

// Setup: tạo partition vừa khít EXE, copy EXE, NHỒI EXE lấp 0 byte,
// read-only, ghi magic+config vào sector. KHÔNG tạo file ẩn.
// Trả về (ok, message). cb: báo tiến trình text.
bool setupUsb(int diskNumber, const QString& loginPw,
              std::function<void(const QString&)> cb, QString& outMsg);

// Runner: tìm ổ USB đang chạy EXE, xác nhận bằng magic ở sector.
QString findUsbRoot();

// Danh sách ổ đĩa logic của PC (C:\, D:\ ...)
QStringList logicalDrives();

// Hash mật khẩu đăng nhập (SHA-256 nhiều vòng + salt)
QString hashPw(const QString& pw, const QByteArray& salt);

#endif // DISKUTIL_H
