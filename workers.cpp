#include "workers.h"
#include "common.h"
#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <windows.h>

void CopyWorker::run(){
    double totalBytes = 0;
    for(const CopyJob& j : m_jobs) totalBytes += (double)j.size;
    if(totalBytes < 1) totalBytes = 1;
    QElapsedTimer timer; timer.start();
    double done = 0; int ok=0;
    qint64 lastEmit = 0;

    for(const CopyJob& j : m_jobs){
        if(m_stop) break;
        QString name = j.name;
        double base = done;
        auto prog = [&](quint64 d, quint64 /*t*/)->bool{
            double cur = base + (double)d;
            qint64 now = timer.elapsed();
            if(now - lastEmit >= 200 || cur>=totalBytes){
                lastEmit = now;
                double sp = cur / (qMax<qint64>(1, now)/1000.0);
                emit progress(cur, totalBytes, sp, name);
            }
            return !m_stop;
        };
        if(m_dir==ToUsb){
            if(j.marker){
                m_sfs->writeSmall(name, QByteArray());
            } else {
                bool okk = m_sfs->writeStream(name, j.src, prog);
                done = base + (double)j.size;
                if(!okk && m_stop) break;
            }
        } else {
            QString out = j.out;
            QString d = QFileInfo(out).absolutePath();
            if(!d.isEmpty()) QDir().mkpath(d);
            // tránh ghi đè
            QString b = out; int dot = b.lastIndexOf('.');
            QString stem = (dot>0)? b.left(dot):b;
            QString ext = (dot>0)? b.mid(dot):QString();
            int c=1;
            while(QFile::exists(out)){ out = QString("%1(%2)%3").arg(stem).arg(c++).arg(ext); }
            if(m_sfs->isStreamFile(name)){
                bool okk = m_sfs->readStream(name, out, prog);
                done = base + (double)j.size;
                if(!okk && m_stop) break;
            } else {
                QByteArray data = m_sfs->readSmall(name);
                QFile f(out);
                if(f.open(QIODevice::WriteOnly)){ f.write(data); f.close(); }
                done = base + (double)j.size;
            }
        }
        ok++;
    }
    emit finishedAll(ok, m_jobs.size(), (bool)m_stop);
}

void UsbGuard::run(){
    static const char* allowed[] = {
        USB_EXE, "autorun.inf", "System Volume Information",
        "$RECYCLE.BIN", "desktop.ini", "Thumbs.db"
    };
    while(!m_stop){
        QDir dir(m_root);
        QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries|QDir::Hidden|QDir::System);
        for(const QFileInfo& fi : list){
            if(m_stop) break;
            QString n = fi.fileName();
            bool keep=false;
            for(const char* a : allowed){
                if(n.compare(a, Qt::CaseInsensitive)==0){ keep=true; break; }
            }
            if(keep) continue;
            QString fp = fi.absoluteFilePath();
            if(fi.isDir()){ QDir(fp).removeRecursively(); }
            else {
                SetFileAttributesW((LPCWSTR)fp.utf16(), FILE_ATTRIBUTE_NORMAL);
                QFile::remove(fp);
            }
            emit alert(QString("Đã xoá file copy trực tiếp: '%1' (chỉ copy qua app!)").arg(n));
        }
        QThread::msleep(500);
    }
}
