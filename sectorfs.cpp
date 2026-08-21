#include "sectorfs.h"
#include "diskio.h"
#include <QFile>
#include <cstring>

SectorFS::SectorFS(int diskNumber, quint64 partOffset)
    : dn(diskNumber), off(partOffset) {}

bool SectorFS::open(){
    h = diskOpen(dn);
    if(h==INVALID_HANDLE_VALUE) return false;
    readTable();
    return true;
}
void SectorFS::close(){
    if(h!=INVALID_HANDLE_VALUE){ diskClose(h); h=INVALID_HANDLE_VALUE; }
}

// ===== Pack/Unpack entry (512 byte) =====
// layout: [456 name][8 sec][8 sz][8 esz][4 act][28 pad]
QByteArray SectorFS::packEntry(const SectorEntry& e){
    QByteArray out(ENTRY_SZ, '\0');
    QByteArray nb = e.name.toUtf8();
    if(nb.size() > 455) nb = nb.left(455);
    memcpy(out.data(), nb.constData(), nb.size());
    char* p = out.data();
    quint64 sec=e.sec, sz=e.sz, esz=e.esz; quint32 act = e.act?1:0;
    memcpy(p+456, &sec, 8);
    memcpy(p+464, &sz, 8);
    memcpy(p+472, &esz, 8);
    memcpy(p+480, &act, 4);
    return out;
}
bool SectorFS::unpackEntry(const QByteArray& d, SectorEntry& e){
    if(d.size() < (int)ENTRY_SZ) return false;
    const char* p = d.constData();
    // tên: tới byte null đầu tiên trong 456 byte
    int len=0; while(len<456 && p[len]!='\0') len++;
    if(len==0) return false;
    e.name = QString::fromUtf8(p, len);
    quint64 sec,sz,esz; quint32 act;
    memcpy(&sec, p+456, 8);
    memcpy(&sz,  p+464, 8);
    memcpy(&esz, p+472, 8);
    memcpy(&act, p+480, 4);
    e.sec=sec; e.sz=sz; e.esz=esz; e.act=(act==1);
    return true;
}

void SectorFS::readTable(){
    files.clear(); maxTblSec = TBL_START;
    for(quint64 sec=TBL_START; sec<TBL_END; sec++){
        QByteArray data = diskReadSectors(h, absSec(sec), 1);
        if(data.size() < (int)SECTOR) break;
        bool foundInSec=false;
        for(quint32 i=0;i<SECTOR/ENTRY_SZ;i++){
            QByteArray ed = data.mid(i*ENTRY_SZ, ENTRY_SZ);
            // sector rỗng?
            bool allzero=true;
            for(int k=0;k<8;k++){ if(ed[k]!='\0'){ allzero=false; break; } }
            if(allzero) continue;
            SectorEntry e;
            if(unpackEntry(ed, e) && e.act){ files.append(e); foundInSec=true; }
        }
        if(foundInSec) maxTblSec = sec;
    }
}

void SectorFS::writeTable(){
    // gom entry active thành các sector, ghi từ TBL_START
    QByteArray blob;
    for(const SectorEntry& e : files){
        if(e.act) blob += packEntry(e);
    }
    // số sector cần
    quint64 needSec = (blob.size() + SECTOR - 1)/SECTOR;
    if(needSec==0) needSec=0;
    // ghi blob
    quint64 sec = TBL_START;
    int pos=0;
    while(pos < blob.size()){
        QByteArray chunk = blob.mid(pos, SECTOR);
        diskWriteSectors(h, absSec(sec), chunk);
        sec++; pos += SECTOR;
    }
    // xoá các sector bảng cũ còn sót (tới maxTblSec)
    quint64 clearTo = maxTblSec;
    QByteArray zero(SECTOR, '\0');
    for(quint64 s=sec; s<=clearTo && s<TBL_END; s++){
        diskWriteSectors(h, absSec(s), zero);
    }
    maxTblSec = (sec>TBL_START)? (sec-1) : TBL_START;
}

QList<SectorEntry> SectorFS::listFiles(){
    QList<SectorEntry> out;
    for(const SectorEntry& e : files) if(e.act) out.append(e);
    return out;
}

quint64 SectorFS::nextFreeSector(){
    quint64 mx = DATA_START;
    bool any=false;
    for(const SectorEntry& e : files){
        if(!e.act) continue;
        any=true;
        quint64 end = e.sec + ((e.esz + SECTOR - 1)/SECTOR) + 4;
        if(end>mx) mx=end;
    }
    return any? mx : DATA_START;
}

quint64 SectorFS::getUsed(){
    quint64 s=0; for(const SectorEntry& e:files) if(e.act) s+=e.esz; return s;
}
quint64 SectorFS::dataBytes(){
    quint64 n = getDiskLengthBytes(dn);
    if(n==0) return 0;
    quint64 base = (off + DATA_START)*SECTOR;
    return (n>base)? (n-base) : 0;
}
quint64 SectorFS::getFree(){
    quint64 db=dataBytes(); if(db==0) return 0;
    quint64 u=getUsed(); return (db>u)? (db-u):0;
}

// ===== File nhỏ (marker) =====
bool SectorFS::writeSmall(const QString& name, const QByteArray& data){
    // xoá trùng tên
    for(int i=files.size()-1;i>=0;i--)
        if(files[i].name==name && files[i].act) files.removeAt(i);
    quint64 start = nextFreeSector();
    // định dạng: [SRAW][8 size][data...]  (giống stream để đọc đồng nhất)
    QByteArray hdr(SECTOR, '\0');
    memcpy(hdr.data(), FILE_MAGIC, 4);
    quint64 sz=(quint64)data.size();
    memcpy(hdr.data()+4, &sz, 8);
    diskWriteSectors(h, absSec(start), hdr);
    if(!data.isEmpty()){
        int pos=0; quint64 sec=start+1;
        while(pos<data.size()){
            QByteArray chunk = data.mid(pos, SECTOR*128);
            diskWriteSectors(h, absSec(sec), chunk);
            quint64 secs=(chunk.size()+SECTOR-1)/SECTOR;
            sec+=secs; pos+=chunk.size();
        }
    }
    SectorEntry e; e.name=name; e.sec=start; e.sz=sz; e.esz=SECTOR+sz; e.act=true;
    files.append(e);
    writeTable();
    return true;
}
QByteArray SectorFS::readSmall(const QString& name){
    for(const SectorEntry& e: files){
        if(e.name==name && e.act){
            QByteArray hdr = diskReadSectors(h, absSec(e.sec), 1);
            if(hdr.size()<12 || memcmp(hdr.constData(), FILE_MAGIC,4)!=0) return QByteArray();
            quint64 sz; memcpy(&sz, hdr.constData()+4, 8);
            if(sz==0) return QByteArray();
            quint64 needSec = (sz + SECTOR -1)/SECTOR;
            QByteArray data = diskReadSectors(h, absSec(e.sec+1), (quint32)needSec);
            return data.left((int)sz);
        }
    }
    return QByteArray();
}

// ===== Streaming (raw, khối 16MB) =====
static const quint32 BLOCK = 16*1024*1024;

bool SectorFS::writeStream(const QString& name, const QString& srcPath, ProgressFn progress){
    for(int i=files.size()-1;i>=0;i--)
        if(files[i].name==name && files[i].act) files.removeAt(i);
    QFile fi(srcPath);
    if(!fi.open(QIODevice::ReadOnly)) return false;
    quint64 total = (quint64)fi.size();
    quint64 start = nextFreeSector();
    // header sector
    QByteArray hdr(SECTOR, '\0');
    memcpy(hdr.data(), FILE_MAGIC, 4);
    memcpy(hdr.data()+4, &total, 8);
    diskWriteSectors(h, absSec(start), hdr);
    // dữ liệu bắt đầu từ start+1, ghi khối 16MB
    void* buf = allocAligned(BLOCK);
    if(!buf){ fi.close(); return false; }
    quint64 curSec = start+1;
    // seek 1 lần
    LARGE_INTEGER li; li.QuadPart = (LONGLONG)(absSec(curSec)*SECTOR);
    SetFilePointerEx(h, li, NULL, FILE_BEGIN);
    quint64 done=0; bool stopped=false;
    QByteArray tmp; tmp.resize(BLOCK);
    while(true){
        qint64 n = fi.read(tmp.data(), BLOCK);
        if(n<=0) break;
        quint32 wlen = (quint32)n;
        if(wlen % SECTOR){
            quint32 pad = SECTOR - (wlen % SECTOR);
            memset(tmp.data()+wlen, 0, pad);
            wlen += pad;
        }
        memcpy(buf, tmp.constData(), wlen);
        DWORD wr=0;
        if(!WriteFile(h, buf, wlen, &wr, NULL) || wr!=wlen){ break; }
        done += (quint64)n;
        if(progress && !progress(done>total?total:done, total)){ stopped=true; break; }
    }
    freeAligned(buf);
    fi.close();
    if(stopped){ writeTable(); return false; }
    SectorEntry e; e.name=name; e.sec=start; e.sz=total; e.esz=SECTOR+total; e.act=true;
    files.append(e);
    writeTable();
    return true;
}

bool SectorFS::readStream(const QString& name, const QString& dstPath, ProgressFn progress){
    SectorEntry ent; bool found=false;
    for(const SectorEntry& e: files) if(e.name==name && e.act){ ent=e; found=true; break; }
    if(!found) return false;
    QByteArray hdr = diskReadSectors(h, absSec(ent.sec), 1);
    if(hdr.size()<4 || memcmp(hdr.constData(), FILE_MAGIC,4)!=0) return false;
    quint64 total = ent.sz;
    QFile fo(dstPath);
    if(!fo.open(QIODevice::WriteOnly)) return false;
    void* buf = allocAligned(BLOCK);
    if(!buf){ fo.close(); return false; }
    LARGE_INTEGER li; li.QuadPart = (LONGLONG)(absSec(ent.sec+1)*SECTOR);
    SetFilePointerEx(h, li, NULL, FILE_BEGIN);
    quint64 written=0; bool stopped=false;
    while(written < total){
        quint64 remain = total - written;
        quint32 want = (quint32)qMin<quint64>(BLOCK, ((remain + SECTOR -1)/SECTOR)*SECTOR);
        DWORD rd=0;
        if(!ReadFile(h, buf, want, &rd, NULL) || rd==0) break;
        quint64 take = qMin<quint64>(rd, remain);
        fo.write((const char*)buf, (int)take);
        written += take;
        if(progress && !progress(written, total)){ stopped=true; break; }
    }
    freeAligned(buf);
    fo.close();
    if(stopped){ QFile::remove(dstPath); return false; }
    return written==total;
}

bool SectorFS::isStreamFile(const QString& name){
    for(const SectorEntry& e: files){
        if(e.name==name && e.act){
            QByteArray hdr = diskReadSectors(h, absSec(e.sec), 1);
            return hdr.size()>=4 && memcmp(hdr.constData(), FILE_MAGIC,4)==0;
        }
    }
    return false;
}

bool SectorFS::exists(const QString& name){
    for(const SectorEntry& e: files) if(e.name==name && e.act) return true;
    return false;
}

void SectorFS::deleteFile(const QString& name){
    for(int i=files.size()-1;i>=0;i--) if(files[i].name==name) files.removeAt(i);
    writeTable();
}
bool SectorFS::renameEntry(const QString& oldName, const QString& newName){
    for(SectorEntry& e: files){
        if(e.name==oldName && e.act){ e.name=newName; writeTable(); return true; }
    }
    return false;
}
void SectorFS::rebuild(){
    files.clear();
    writeTable();
}

// ===== Config =====
bool SectorFS::writeConfig(const QByteArray& jsonBytes){
    QByteArray blob;
    quint32 len = (quint32)jsonBytes.size();
    blob.append((const char*)&len, 4);
    blob.append(jsonBytes);
    return diskWriteSectors(h, absSec(CFG_SEC), blob);
}
QByteArray SectorFS::readConfig(){
    QByteArray raw = diskReadSectors(h, absSec(CFG_SEC), (quint32)(TBL_START - CFG_SEC));
    if(raw.size()<4) return QByteArray();
    quint32 len; memcpy(&len, raw.constData(), 4);
    if(len==0 || (int)len > raw.size()-4) return QByteArray();
    return raw.mid(4, len);
}
