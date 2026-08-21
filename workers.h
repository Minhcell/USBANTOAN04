#ifndef WORKERS_H
#define WORKERS_H

#include <QThread>
#include <QList>
#include <QString>
#include <atomic>
#include "sectorfs.h"

// Một job copy
struct CopyJob {
    QString src;    // đường dẫn nguồn (khi copy PC->USB)
    QString name;   // tên lưu trên USB (đường dẫn ảo)
    QString out;    // đường dẫn đích (khi copy USB->PC)
    quint64 size = 0;
    bool marker = false; // thư mục rỗng (.keep)
};

class CopyWorker : public QThread {
    Q_OBJECT
public:
    enum Dir { ToUsb, FromUsb };
    enum Overwrite { KeepBoth=0, Replace=1, Skip=2 }; // xu ly khi trung ten
    CopyWorker(SectorFS* sfs, QList<CopyJob> jobs, Dir dir, QObject* p=nullptr)
        : QThread(p), m_sfs(sfs), m_jobs(jobs), m_dir(dir) { m_stop=false; m_ow=KeepBoth; }
    void requestStop(){ m_stop = true; }
    void setOverwrite(Overwrite o){ m_ow = o; }
signals:
    void progress(double done, double total, double speed, QString name);
    void finishedAll(int ok, int total, bool stopped);
    void errorMsg(QString msg);
protected:
    void run() override;
private:
    SectorFS* m_sfs;
    QList<CopyJob> m_jobs;
    Dir m_dir;
    std::atomic<bool> m_stop;
    Overwrite m_ow;
};

class UsbGuard : public QThread {
    Q_OBJECT
public:
    UsbGuard(const QString& root, QObject* p=nullptr): QThread(p), m_root(root){ m_stop=false; }
    void requestStop(){ m_stop=true; }
signals:
    void alert(QString msg);
protected:
    void run() override;
private:
    QString m_root;
    std::atomic<bool> m_stop;
};

#endif // WORKERS_H
