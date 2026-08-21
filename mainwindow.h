#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QTreeWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QWidget>
#include "sectorfs.h"
#include "workers.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(const QString& usbRoot, const QString& loginPw, const QByteArray& cfgJson, QWidget* p=nullptr);
    ~MainWindow();
protected:
    void closeEvent(QCloseEvent* ev) override;
private slots:
    void loadPc(const QString& path);
    void loadUsb();
    void pcDoubleClicked(QTreeWidgetItem* it, int col);
    void usbDoubleClicked(QTreeWidgetItem* it, int col);
    void pcUp();
    void usbUp();
    void copyToUsb();
    void copyFromUsb();
    void mkPcFolder();
    void mkUsbFolder();
    void renameItem();
    void deleteData();
    void formatUsb();
    void changeLoginPw();
    void showHelp();
    void stopCopy();
    void pcSelSize();
    void usbSelSize();
    // worker
    void onProgress(double done, double total, double speed, QString name);
    void onCopyDone(int ok, int total, bool stopped, int dir);
private:
    void buildUi();
    QIcon arrowIcon(bool right);
    QStringList pcLocations(QStringList& labels);
    QString fmtCol(const QString& name, bool isDir);
    void showSel(const QString& text);

    QString m_usbRoot, m_loginPw;
    QByteArray m_cfg;
    QString m_salt, m_pwHash;
    QByteArray m_masterKey;   // khoá AES-256 thật (đã mở khoá từ mật khẩu)
    int m_diskNumber; quint64 m_dataOffset;
    SectorFS* m_sfs = nullptr;
    QString m_cp;        // thư mục PC hiện tại
    QString m_usbPath;   // thư mục ảo USB hiện tại
    UsbGuard* m_guard = nullptr;
    CopyWorker* m_worker = nullptr;

    QComboBox *m_pcCombo, *m_usbCombo;
    QLineEdit *m_pcPath, *m_usbPathEdit, *m_pcSearch, *m_usbSearch;
    QLabel *m_pcSpace, *m_usbSpace, *m_pcStat, *m_usbStat, *m_copyLbl;
    QTreeWidget *m_tp, *m_tu;
    QProgressBar *m_pb;
    QWidget *m_prow;
    QPushButton *m_stopBtn;
};

#endif // MAINWINDOW_H
