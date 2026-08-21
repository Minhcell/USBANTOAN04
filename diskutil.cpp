#include "diskutil.h"
#include "crypto.h"
#include "diskio.h"
#include "sectorfs.h"
#include "common.h"
#include <windows.h>
#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QThread>
#include <QStorageInfo>
#include <QRandomGenerator>

QStringList logicalDrives(){
    QStringList out;
    DWORD bm = GetLogicalDrives();
    for(int i=0;i<26;i++) if(bm & (1u<<i)) out << (QString(QChar('A'+i)) + ":\\");
    return out;
}

QList<UsbCandidate> listRemovableUsb(){
    QList<UsbCandidate> out;
    for(const QString& d : logicalDrives()){
        UINT t = GetDriveTypeW((LPCWSTR)d.utf16());
        if(t==DRIVE_REMOVABLE){
            int dn = getPhysicalDriveNumber(d);
            quint64 sz=0;
            QStorageInfo si(d);
            if(si.isValid()) sz=(quint64)si.bytesTotal();
            UsbCandidate c; c.diskNumber=dn; c.model=d; c.sizeBytes=sz; c.letter=d;
            out.append(c);
        }
    }
    return out;
}

bool runDiskpart(const QString& script){
    QTemporaryFile f(QDir::tempPath()+"/dpXXXXXX.txt");
    f.setAutoRemove(true);
    if(!f.open()) return false;
    f.write(script.toUtf8());
    QString path = f.fileName();
    f.close();
    QProcess p;
    p.start("diskpart", QStringList() << "/s" << path);
    p.waitForFinished(60000);
    return true;
}

bool setVolumeReadonly(const QString& driveRoot, bool readonly){
    QString letter = driveRoot.left(1); // "E"
    QString script = QString("select volume %1\nattributes volume %2 readonly\nexit\n")
                        .arg(letter).arg(readonly?"set":"clear");
    return runDiskpart(script);
}

QString hashPw(const QString& pw, const QByteArray& salt){
    QByteArray data = salt + pw.toUtf8();
    for(int i=0;i<100000;i++)
        data = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return QString(data.toHex());
}

// đọc kích thước partition 1 -> offset vùng dữ liệu
static quint64 computeDataOffset(int dn){
    quint64 s,n;
    if(readMbrPartition(dn,1,s,n)) return s+n;
    return 0;
}

bool setupUsb(int dn, const QString& loginPw,
              std::function<void(const QString&)> cb, QString& outMsg){
    if(dn<0){ outMsg="Disk không hợp lệ!"; return false; }
    QString exePath = QCoreApplication::applicationFilePath();
    quint64 exeSz = (quint64)QFileInfo(exePath).size();
    quint64 exeMbCeil = (exeSz + 1024*1024 - 1)/(1024*1024);
    // GIONG H04: dung FAT16 (fs=fat) -> phan vung nho vua co EXE (khong bi
    // rang buoc toi thieu ~33MB nhu FAT32). Sau do NHOI EXE bang so 0 cho
    // day phan vung -> chi 1 file EXE, 0 byte trong (giong het H04).
    quint64 pubMb = exeMbCeil + 2;
    if(pubMb < 8) pubMb = 8;   // toi thieu 8MB -> chac chan la FAT16, van nho gon

    if(cb) cb("Xóa USB...");
    runDiskpart(QString("select disk %1\nclean\nexit\n").arg(dn));
    Sleep(2000);

    if(cb) cb("Tạo phân vùng EXE...");
    // fs=fat => FAT16 (cho phep phan vung nho vai MB, giong H04). Label <= 11 ky tu.
    runDiskpart(QString("select disk %1\ncreate partition primary size=%2\n"
                        "format fs=fat quick label=\"USB AN TOAN\"\nactive\nassign\nexit\n")
                .arg(dn).arg(pubMb));
    Sleep(3000);

    quint64 dataOff = computeDataOffset(dn);
    if(dataOff==0) dataOff = (pubMb*1024*1024)/SECTOR + 2048;

    // Ghi magic vào HDR_SEC
    HANDLE h = diskOpen(dn);
    if(h==INVALID_HANDLE_VALUE){ outMsg="Không mở được disk (cần Admin)."; return false; }
    QByteArray hdr(SECTOR, '\0');
    memcpy(hdr.data(), SEC_MAGIC, 8);
    diskWriteSectors(h, dataOff, hdr);
    QByteArray chk = diskReadSectors(h, dataOff, 1);
    diskClose(h);
    if(chk.size()<8 || memcmp(chk.constData(), SEC_MAGIC,8)!=0){
        outMsg="Ghi sector thất bại!"; return false;
    }

    // Tìm ký tự ổ phân vùng EXE = ổ removable nằm trên ĐÚNG đĩa vừa tạo
    if(cb) cb("Tìm phân vùng EXE...");
    QString pub;
    for(int tries=0; tries<10 && pub.isEmpty(); tries++){
        for(const QString& d : logicalDrives()){
            if(GetDriveTypeW((LPCWSTR)d.utf16())==DRIVE_REMOVABLE){
                if(getPhysicalDriveNumber(d)==dn){ pub=d; break; }
            }
        }
        if(pub.isEmpty()) Sleep(2000);
    }
    // Dự phòng: nếu chưa khớp được theo số đĩa, tìm theo dung lượng nhỏ
    if(pub.isEmpty()){
        for(const QString& d : logicalDrives()){
            if(GetDriveTypeW((LPCWSTR)d.utf16())==DRIVE_REMOVABLE){
                QStorageInfo si(d); si.refresh();
                if(si.isValid() && si.bytesTotal()>0 && si.bytesTotal() < (qint64)200*1024*1024){ pub=d; break; }
            }
        }
    }
    if(pub.isEmpty()){ outMsg="Không tìm phân vùng EXE! (thử rút/cắm lại USB rồi chạy Setup lại)"; return false; }

    // Ghi config vào sector (JSON đơn giản)
    QByteArray salt(16,'\0');
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(salt.data()), 4);
    QString ph = hashPw(loginPw, salt);
    // Sinh MASTER KEY ngau nhien (thuc su ma hoa file). "Boc" (wrap) master key
    // bang khoa dan xuat tu mat khau -> doi mat khau chi boc lai, KHONG phai
    // ma hoa lai toan bo file.
    QByteArray masterKey = randomBytes(32);
    QByteArray kek = deriveKey(loginPw, salt);
    QByteArray wrapped(32,'\0');
    unsigned char zeroIv[16]; memset(zeroIv,0,16);
    { AesCtr wa; if(wa.init(kek)) wa.process(zeroIv,0,masterKey.constData(),wrapped.data(),32); }
    QString mkeyHex = QString(wrapped.toHex());
    QString json = QString("{\"v\":\"%1\",\"salt\":\"%2\",\"pw_hash\":\"%3\",\"mkey\":\"%4\",\"att\":5,"
                           "\"disk_number\":%5,\"data_offset\":%6}")
                   .arg(APP_VER).arg(QString(salt.toHex())).arg(ph).arg(mkeyHex).arg(dn).arg(dataOff);
    {
        SectorFS sfs(dn, dataOff);
        if(!sfs.open()){ outMsg="Không mở SectorFS."; return false; }
        sfs.writeConfig(json.toUtf8());
        sfs.close();
    }

    // Copy EXE vào phân vùng
    QString dstExe = pub + USB_EXE;
    if(!exePath.toLower().endsWith(".exe")){
        outMsg="Chỉ chạy được khi đã build thành EXE."; return false;
    }
    QFile::remove(dstExe);
    if(!QFile::copy(exePath, dstExe)){ outMsg="Lỗi copy EXE."; return false; }

    // ==== NHỒI EXE lấp phân vùng tới 0 byte (giống H04) ====
    // PE bỏ qua dữ liệu thừa ở cuối -> EXE vẫn chạy. Chỉ có 1 file, 0 byte trống.
    if(cb) cb("Nhồi EXE lấp đầy 0 byte...");
    {
        QStorageInfo si(pub);
        qint64 freeB = si.isValid()? si.bytesAvailable() : 0;
        // trừ hao 1 sector
        if(freeB > (qint64)SECTOR){
            QFile ef(dstExe);
            if(ef.open(QIODevice::Append)){
                QByteArray zero(4*1024*1024, '\0');
                qint64 toWrite = freeB;
                while(toWrite > 0){
                    qint64 n = qMin<qint64>(toWrite, zero.size());
                    qint64 w = ef.write(zero.constData(), n);
                    if(w<=0) break;
                    ef.flush();
                    toWrite -= w;
                }
                ef.close();
            }
        }
        // ghi nốt vài byte lẻ nếu vẫn còn (giảm dần)
        for(int pass=0; pass<3; pass++){
            QStorageInfo s2(pub);
            qint64 fr = s2.isValid()? s2.bytesAvailable() : 0;
            if(fr <= 0) break;
            QFile ef(dstExe);
            if(ef.open(QIODevice::Append)){
                QByteArray z((int)qMin<qint64>(fr, 1024*1024), '\0');
                ef.write(z); ef.flush(); ef.close();
            }
        }
    }

    // Read-only chặn copy trực tiếp
    if(cb) cb("Khoá phân vùng (read-only)...");
    Sleep(1000);
    setVolumeReadonly(pub, true);

    outMsg = QString("Thành công!\n\nPhân vùng EXE: %1 (%2MB) - chỉ có file %3\n"
                     "  - Đã nhồi lấp 0 byte + read-only\n"
                     "  - KHÔNG có file ẩn nào\n"
                     "Vùng dữ liệu: UNALLOCATED - Windows KHÔNG thấy\n\n"
                     "Rút USB, cắm lại, chạy %3")
             .arg(pub).arg(pubMb).arg(USB_EXE);
    return true;
}

QString findUsbRoot(){
    QString exe = QCoreApplication::applicationFilePath();
    QString root = exe.left(3); // "E:\"
    if(root.size()<2) return QString();
    // xác nhận magic ở vùng sector
    int dn = getPhysicalDriveNumber(root);
    if(dn>=0){
        quint64 s,n;
        if(readMbrPartition(dn,1,s,n)){
            quint64 off = s+n;
            HANDLE h = diskOpen(dn);
            if(h!=INVALID_HANDLE_VALUE){
                QByteArray sec = diskReadSectors(h, off, 1);
                diskClose(h);
                if(sec.size()>=8 && memcmp(sec.constData(), SEC_MAGIC,8)==0)
                    return root;
            }
        }
    }
    // fallback: có file EXE trên ổ này
    if(QFile::exists(root + USB_EXE)) return root;
    return root;
}
