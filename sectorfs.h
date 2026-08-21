#ifndef SECTORFS_H
#define SECTORFS_H

#include <windows.h>
#include <QString>
#include <QList>
#include <QByteArray>
#include <functional>
#include "common.h"

struct SectorEntry {
    QString name;      // đường dẫn ảo (vd "folder/sub/file.txt")
    quint64 sec = 0;   // sector bắt đầu (tương đối offset)
    quint64 sz  = 0;   // kích thước gốc của file
    quint64 esz = 0;   // số byte thực đã ghi (gồm header + data)
    bool    act = true;
};

// callback tiến trình: (đã xong byte, tổng byte) -> trả false để DỪNG
typedef std::function<bool(quint64,quint64)> ProgressFn;

class SectorFS {
public:
    SectorFS(int diskNumber, quint64 partOffset);
    bool open();
    void close();

    QList<SectorEntry> listFiles();          // chỉ file active
    void readTable();
    quint64 getUsed();
    quint64 dataBytes();
    quint64 getFree();

    // Ghi/đọc file nhỏ (marker thư mục .keep, ...) - raw
    bool writeSmall(const QString& name, const QByteArray& data);
    QByteArray readSmall(const QString& name);

    // Ghi/đọc file lớn theo luồng (raw, KHÔNG mã hoá), khối 16MB
    // progress trả false -> dừng (huỷ, không lưu / xoá file đích)
    bool writeStream(const QString& name, const QString& srcPath, ProgressFn progress);
    bool readStream(const QString& name, const QString& dstPath, ProgressFn progress);
    bool isStreamFile(const QString& name);
    bool exists(const QString& name);        // co file active cung ten khong

    void deleteFile(const QString& name);
    bool renameEntry(const QString& oldName, const QString& newName);
    void rebuild(); // xoá sạch (format)

    // Config lưu ở CFG_SEC (JSON)
    bool writeConfig(const QByteArray& jsonBytes);
    QByteArray readConfig();

    HANDLE handle() const { return h; }
    quint64 offset() const { return off; }

private:
    int dn;
    quint64 off;
    HANDLE h = INVALID_HANDLE_VALUE;
    QList<SectorEntry> files;
    quint64 maxTblSec = TBL_START;

    quint64 absSec(quint64 rel){ return off + rel; }
    quint64 nextFreeSector();
    void writeTable();
    QByteArray packEntry(const SectorEntry& e);
    bool unpackEntry(const QByteArray& d, SectorEntry& e);
};

#endif // SECTORFS_H
