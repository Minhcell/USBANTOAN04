#include "mainwindow.h"
#include "diskio.h"
#include "diskutil.h"
#include "common.h"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QHeaderView>
#include <QStyle>
#include <QMessageBox>
#include <QAbstractButton>
#include <QInputDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>
#include <QDesktopServices>
#include <QUrl>
#include <QTemporaryDir>
#include <QCloseEvent>
#include <QDateTime>
#include <QRandomGenerator>
#include <QSet>
#include <QStatusBar>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QMenu>
#include <QMap>
#include <QPair>
#include <QTimer>
#include <algorithm>
#include <windows.h>

static const char* STYLE =
"*{font-family:'Segoe UI';font-size:13px;}"
"QMainWindow,QDialog{background:#ffffff;color:#202020;}"
"QLabel{color:#202020;font-size:13px;}"
"QWidget#tb{background:#ffffff;border-bottom:1px solid #e0e0e0;}"
"QToolButton{background:transparent;border:none;padding:6px 12px;color:#202020;font-size:13px;}"
"QToolButton:hover{background:#eaf1fb;border-radius:4px;}"
"QToolButton#arrow{background:transparent;border:none;}"
"QPushButton{background:#f0f0f0;border:1px solid #c0c0c0;border-radius:4px;padding:5px 12px;}"
"QPushButton:hover{background:#e5eefb;border-color:#7aa7e0;}"
"QTreeWidget{background:white;border:1px solid #d0d0d0;outline:none;font-size:13px;}"
"QTreeWidget::item{padding:4px 2px;border-bottom:1px solid #f0f0f0;}"
"QTreeWidget::item:selected{background:#cfe3ff;color:#202020;}"
"QHeaderView::section{background:#ffffff;color:#404040;border:none;border-bottom:1px solid #d0d0d0;padding:6px 4px;font-weight:bold;}"
"QComboBox{background:white;border:1px solid #c0c0c0;border-radius:3px;padding:4px 8px;}"
"QLineEdit{background:white;border:1px solid #c0c0c0;border-radius:3px;padding:5px 8px;color:#202020;}"
"QLineEdit:focus{border-color:#1565c0;}"
"QProgressBar{background:#f0f0f0;border:1px solid #d0d0d0;border-radius:2px;height:18px;text-align:center;}"
"QProgressBar::chunk{background:#1565c0;}"
"QStatusBar{background:#ffffff;color:#404040;border-top:1px solid #e0e0e0;font-size:12px;}";

MainWindow::MainWindow(const QString& usbRoot, const QString& loginPw, const QByteArray& cfgJson, QWidget* p)
    : QMainWindow(p), m_usbRoot(usbRoot), m_loginPw(loginPw), m_cfg(cfgJson){
    // parse config
    QJsonObject o = QJsonDocument::fromJson(cfgJson).object();
    m_salt = o.value("salt").toString();
    m_pwHash = o.value("pw_hash").toString();
    m_diskNumber = getPhysicalDriveNumber(usbRoot);
    quint64 s,n; m_dataOffset=0;
    if(readMbrPartition(m_diskNumber,1,s,n)) m_dataOffset=s+n;
    else m_dataOffset = (quint64)o.value("data_offset").toDouble();

    m_cp = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if(m_cp.isEmpty() || !QDir(m_cp).exists()) m_cp = QDir::homePath();
    m_usbPath = "";

    setWindowTitle(QString::fromUtf8(APP_NAME));
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint
                   | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint);
    resize(1150, 660);
    setMinimumSize(820, 500);
    setStyleSheet(STYLE);

    m_sfs = new SectorFS(m_diskNumber, m_dataOffset);
    if(!m_sfs->open())
        QMessageBox::critical(nullptr,"",QString::fromUtf8("Lỗi mở disk %1").arg(m_diskNumber));

    buildUi();
    loadPc(m_cp);
    loadUsb();

    m_guard = new UsbGuard(m_usbRoot, this);
    connect(m_guard, &UsbGuard::alert, this, [this](QString m){ statusBar()->showMessage(m, 3000); });
    m_guard->start();
}

MainWindow::~MainWindow(){}

QStringList MainWindow::pcLocations(QStringList& labels){
    QStringList paths;
    QString home = QDir::homePath();
    QString dk = home + "/Desktop";
    if(QDir(dk).exists()){ labels<<"Desktop"; paths<<dk; }
    labels<<"Documents"; paths<< home+"/Documents";
    labels<<"Downloads"; paths<< home+"/Downloads";
    for(const QString& d : logicalDrives()){ labels<<d; paths<<d; }
    return paths;
}

QIcon MainWindow::arrowIcon(bool right){
    QPixmap pm(52,44); pm.fill(Qt::transparent);
    QPainter pt(&pm); pt.setRenderHint(QPainter::Antialiasing);
    pt.setBrush(QColor("#1f6fd6")); pt.setPen(Qt::NoPen);
    QPolygon poly;
    if(right) poly << QPoint(4,15)<<QPoint(30,15)<<QPoint(30,6)<<QPoint(48,22)
                   << QPoint(30,38)<<QPoint(30,29)<<QPoint(4,29);
    else      poly << QPoint(48,15)<<QPoint(22,15)<<QPoint(22,6)<<QPoint(4,22)
                   << QPoint(22,38)<<QPoint(22,29)<<QPoint(48,29);
    pt.drawPolygon(poly); pt.end();
    return QIcon(pm);
}

void MainWindow::buildUi(){
    QStyle* st = style();
    QWidget* cw = new QWidget; setCentralWidget(cw);
    QVBoxLayout* rt = new QVBoxLayout(cw); rt->setContentsMargins(0,0,0,0); rt->setSpacing(0);

    // ===== Thanh công cụ =====
    QWidget* tb = new QWidget; tb->setObjectName("tb"); tb->setFixedHeight(52);
    QHBoxLayout* tl = new QHBoxLayout(tb); tl->setContentsMargins(10,4,10,4); tl->setSpacing(4);
    auto tbtn = [&](const QString& text, QStyle::StandardPixmap ic, const char* slot)->QToolButton*{
        QToolButton* b = new QToolButton;
        b->setText(" "+text); b->setIcon(st->standardIcon(ic));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); b->setIconSize(QSize(22,22));
        connect(b, SIGNAL(clicked()), this, slot);
        return b;
    };
    tl->addWidget(tbtn(QString::fromUtf8("Tạo TM (PC)"), QStyle::SP_FileDialogNewFolder, SLOT(mkPcFolder())));
    tl->addWidget(tbtn(QString::fromUtf8("Tạo TM (USB)"), QStyle::SP_FileDialogNewFolder, SLOT(mkUsbFolder())));
    tl->addWidget(tbtn(QString::fromUtf8("Đổi tên"), QStyle::SP_FileDialogDetailedView, SLOT(renameItem())));
    tl->addWidget(tbtn(QString::fromUtf8("Xóa dữ liệu"), QStyle::SP_TrashIcon, SLOT(deleteData())));
    tl->addStretch();
    tl->addWidget(tbtn(QString::fromUtf8("Đổi mật khẩu"), QStyle::SP_DialogYesButton, SLOT(changeLoginPw())));
    tl->addWidget(tbtn(QString::fromUtf8("Format USB"), QStyle::SP_BrowserReload, SLOT(formatUsb())));
    tl->addWidget(tbtn(QString::fromUtf8("Thoát"), QStyle::SP_DialogCloseButton, SLOT(close())));
    QToolButton* hb = new QToolButton; hb->setText("?"); hb->setFixedSize(30,30);
    connect(hb, SIGNAL(clicked()), this, SLOT(showHelp()));
    tl->addWidget(hb);
    rt->addWidget(tb);

    // ===== Thân: 2 khung + mũi tên =====
    QWidget* bd = new QWidget; QHBoxLayout* bl = new QHBoxLayout(bd);
    bl->setContentsMargins(8,6,8,6); bl->setSpacing(4);

    // --- PC ---
    QWidget* pc = new QWidget; QVBoxLayout* pl = new QVBoxLayout(pc);
    pl->setContentsMargins(2,2,2,2); pl->setSpacing(4);
    QHBoxLayout* r1 = new QHBoxLayout; r1->setSpacing(6);
    m_pcCombo = new QComboBox; m_pcCombo->setFixedHeight(28); m_pcCombo->setMinimumWidth(180);
    QStringList labels; QStringList paths = pcLocations(labels);
    for(int i=0;i<paths.size();i++) m_pcCombo->addItem(labels[i], paths[i]);
    connect(m_pcCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){
        loadPc(m_pcCombo->currentData().toString());
    });
    r1->addWidget(m_pcCombo); r1->addStretch();
    m_pcSpace = new QLabel(QString::fromUtf8("Dung lượng còn lại: --"));
    m_pcSpace->setStyleSheet("font-size:14px;");
    r1->addWidget(m_pcSpace);
    pl->addLayout(r1);
    QHBoxLayout* r2 = new QHBoxLayout; r2->setSpacing(4);
    QToolButton* bu = new QToolButton; bu->setIcon(st->standardIcon(QStyle::SP_FileDialogToParent));
    bu->setIconSize(QSize(20,20)); connect(bu, SIGNAL(clicked()), this, SLOT(pcUp())); r2->addWidget(bu);
    m_pcPath = new QLineEdit; m_pcPath->setReadOnly(true); r2->addWidget(m_pcPath, 2);
    QToolButton* rf1 = new QToolButton; rf1->setIcon(st->standardIcon(QStyle::SP_BrowserReload));
    rf1->setIconSize(QSize(18,18)); connect(rf1, &QToolButton::clicked, this, [this](){ loadPc(m_cp); });
    r2->addWidget(rf1);
    m_pcSearch = new QLineEdit; m_pcSearch->setPlaceholderText(QString::fromUtf8("Tìm kiếm"));
    connect(m_pcSearch, &QLineEdit::textChanged, this, [this](){ loadPc(m_cp); });
    r2->addWidget(m_pcSearch, 1);
    pl->addLayout(r2);
    m_tp = new QTreeWidget;
    m_tp->setHeaderLabels(QStringList()<<QString::fromUtf8("Tên")<<QString::fromUtf8("Định dạng")
                          <<QString::fromUtf8("Kích cỡ")<<QString::fromUtf8("Ngày chỉnh sửa"));
    m_tp->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tp->setRootIsDecorated(false);
    m_tp->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tp->setColumnWidth(1,90); m_tp->setColumnWidth(2,100); m_tp->setColumnWidth(3,150);
    connect(m_tp, &QTreeWidget::itemDoubleClicked, this, &MainWindow::pcDoubleClicked);
    connect(m_tp, &QTreeWidget::itemSelectionChanged, this, &MainWindow::pcSelSize);
    pl->addWidget(m_tp);
    m_pcStat = new QLabel(QString::fromUtf8("0 thư mục, 0 file"));
    m_pcStat->setStyleSheet("color:#404040;padding:2px;");
    pl->addWidget(m_pcStat);
    bl->addWidget(pc, 1);

    // --- Giữa: mũi tên ---
    QWidget* ct = new QWidget; ct->setFixedWidth(70);
    QVBoxLayout* cl = new QVBoxLayout(ct); cl->setContentsMargins(2,0,2,0); cl->setSpacing(16);
    cl->addStretch(1);
    QToolButton* b1 = new QToolButton; b1->setObjectName("arrow");
    b1->setIcon(arrowIcon(true)); b1->setIconSize(QSize(52,44));
    b1->setToolTip(QString::fromUtf8("Copy sang USB"));
    connect(b1, SIGNAL(clicked()), this, SLOT(copyToUsb())); cl->addWidget(b1, 0, Qt::AlignCenter);
    QToolButton* b2 = new QToolButton; b2->setObjectName("arrow");
    b2->setIcon(arrowIcon(false)); b2->setIconSize(QSize(52,44));
    b2->setToolTip(QString::fromUtf8("Copy về máy"));
    connect(b2, SIGNAL(clicked()), this, SLOT(copyFromUsb())); cl->addWidget(b2, 0, Qt::AlignCenter);
    cl->addStretch(2); bl->addWidget(ct);

    // --- USB ---
    QWidget* ub = new QWidget; QVBoxLayout* ul = new QVBoxLayout(ub);
    ul->setContentsMargins(2,2,2,2); ul->setSpacing(4);
    QHBoxLayout* r3 = new QHBoxLayout; r3->setSpacing(6);
    m_usbCombo = new QComboBox; m_usbCombo->setFixedHeight(28); m_usbCombo->setMinimumWidth(180);
    m_usbCombo->addItem(QString::fromUtf8("USB AN TOÀN"));
    r3->addWidget(m_usbCombo); r3->addStretch();
    m_usbSpace = new QLabel(QString::fromUtf8("Dung lượng còn lại: --"));
    m_usbSpace->setStyleSheet("font-size:14px;");
    r3->addWidget(m_usbSpace);
    ul->addLayout(r3);
    QHBoxLayout* r4 = new QHBoxLayout; r4->setSpacing(4);
    QToolButton* buu = new QToolButton; buu->setIcon(st->standardIcon(QStyle::SP_FileDialogToParent));
    buu->setIconSize(QSize(20,20)); connect(buu, SIGNAL(clicked()), this, SLOT(usbUp())); r4->addWidget(buu);
    m_usbPathEdit = new QLineEdit; m_usbPathEdit->setReadOnly(true); r4->addWidget(m_usbPathEdit, 2);
    QToolButton* rf2 = new QToolButton; rf2->setIcon(st->standardIcon(QStyle::SP_BrowserReload));
    rf2->setIconSize(QSize(18,18)); connect(rf2, SIGNAL(clicked()), this, SLOT(loadUsb())); r4->addWidget(rf2);
    m_usbSearch = new QLineEdit; m_usbSearch->setPlaceholderText(QString::fromUtf8("Tìm kiếm"));
    connect(m_usbSearch, &QLineEdit::textChanged, this, [this](){ loadUsb(); });
    r4->addWidget(m_usbSearch, 1);
    ul->addLayout(r4);
    m_tu = new QTreeWidget;
    m_tu->setHeaderLabels(QStringList()<<QString::fromUtf8("Tên")<<QString::fromUtf8("Định dạng")
                          <<QString::fromUtf8("Kích cỡ")<<QString::fromUtf8("Ngày chỉnh sửa"));
    m_tu->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tu->setRootIsDecorated(false);
    m_tu->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tu->setColumnWidth(1,90); m_tu->setColumnWidth(2,100); m_tu->setColumnWidth(3,150);
    connect(m_tu, &QTreeWidget::itemDoubleClicked, this, &MainWindow::usbDoubleClicked);
    connect(m_tu, &QTreeWidget::itemSelectionChanged, this, &MainWindow::usbSelSize);
    ul->addWidget(m_tu);
    m_usbStat = new QLabel(QString::fromUtf8("0 thư mục, 0 file"));
    m_usbStat->setStyleSheet("color:#404040;padding:2px;");
    ul->addWidget(m_usbStat);
    bl->addWidget(ub, 1);

    rt->addWidget(bd, 1);

    m_pb = new QProgressBar; m_pb->setVisible(false); m_pb->setTextVisible(true);
    rt->addWidget(m_pb);
    m_prow = new QWidget; QHBoxLayout* prl = new QHBoxLayout(m_prow);
    prl->setContentsMargins(0,0,0,0); prl->setSpacing(6);
    m_copyLbl = new QLabel("");
    m_copyLbl->setStyleSheet("font-size:14px;font-weight:bold;color:#1565c0;padding:4px 10px;background:#eaf2fd;");
    prl->addWidget(m_copyLbl, 1);
    m_stopBtn = new QPushButton(QString::fromUtf8("Dừng"));
    m_stopBtn->setStyleSheet("QPushButton{background:#e53935;color:white;font-weight:bold;border:none;border-radius:4px;padding:6px 18px;}QPushButton:hover{background:#f44336;}");
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::stopCopy);
    prl->addWidget(m_stopBtn);
    m_prow->setVisible(false);
    rt->addWidget(m_prow);

    statusBar()->showMessage(QString::fromUtf8("USB AN TOÀN | Dữ liệu ẩn, chặn copy trực tiếp"));
}

// ==================== HELPERS ====================
QString MainWindow::fmtCol(const QString& name, bool isDir){
    if(isDir) return "";
    int dot = name.lastIndexOf('.');
    if(dot>0 && dot<name.size()-1) return name.mid(dot+1).toLower();
    return "file";
}

void MainWindow::showSel(const QString& text){
    if(m_worker && m_worker->isRunning()) return; // đang copy -> không ghi đè %
    if(!text.isEmpty()){
        m_pb->setVisible(false);
        m_stopBtn->setVisible(false);
        m_copyLbl->setText(text);
        m_prow->setVisible(true);
    } else {
        m_copyLbl->setText("");
        m_prow->setVisible(false);
    }
}

// ==================== LOAD PC ====================
void MainWindow::loadPc(const QString& path){
    m_tp->clear(); m_cp = path; m_pcPath->setText(path);
    QString q = m_pcSearch->text().trimmed().toLower();
    QStyle* st = style();
    // dòng ".."
    QDir dir(path);
    QString parent = QFileInfo(path).absolutePath();
    if(!parent.isEmpty() && parent!=path){
        QTreeWidgetItem* up = new QTreeWidgetItem(QStringList()<<"..."<<""<<""<<"");
        up->setData(0, Qt::UserRole, parent);
        up->setData(0, Qt::UserRole+1, true);
        up->setData(0, Qt::UserRole+2, QString("up"));
        up->setIcon(0, st->standardIcon(QStyle::SP_FileDialogToParent));
        m_tp->addTopLevelItem(up);
    }
    int nd=0, nf=0;
    QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries, QDir::DirsFirst|QDir::Name);
    for(const QFileInfo& fi : list){
        QString nm = fi.fileName();
        if(!q.isEmpty() && !nm.toLower().contains(q)) continue;
        bool isDir = fi.isDir();
        QString sz = isDir? "" : fmtSize((double)fi.size());
        QString dt = fi.lastModified().toString("hh:mm:ss, dd/MM/yyyy");
        QTreeWidgetItem* it = new QTreeWidgetItem(QStringList()<<nm<<fmtCol(nm,isDir)<<sz<<dt);
        it->setData(0, Qt::UserRole, fi.absoluteFilePath());
        it->setData(0, Qt::UserRole+1, isDir);
        it->setIcon(0, st->standardIcon(isDir?QStyle::SP_DirIcon:QStyle::SP_FileIcon));
        m_tp->addTopLevelItem(it);
        if(isDir) nd++; else nf++;
    }
    m_pcStat->setText(QString::fromUtf8("%1 thư mục, %2 file").arg(nd).arg(nf));
    QStorageInfo si(path);
    if(si.isValid()) m_pcSpace->setText(QString::fromUtf8("Dung lượng còn lại: %1").arg(fmtSize((double)si.bytesAvailable())));
}

// ==================== LOAD USB ====================
void MainWindow::loadUsb(){
    m_tu->clear();
    if(!m_sfs) return;
    QStyle* st = style();
    QString q = m_usbSearch->text().trimmed().toLower();
    m_sfs->readTable();
    QString prefix = m_usbPath;
    m_usbPathEdit->setText(prefix.isEmpty()? "USB AN TOÀN:/" : ("USB AN TOÀN:/"+prefix));
    if(!prefix.isEmpty()){
        QTreeWidgetItem* up = new QTreeWidgetItem(QStringList()<<"..."<<""<<""<<"");
        up->setData(0, Qt::UserRole, QString(".."));
        up->setData(0, Qt::UserRole+1, true);
        up->setIcon(0, st->standardIcon(QStyle::SP_FileDialogToParent));
        m_tu->addTopLevelItem(up);
    }
    QList<SectorEntry> all = m_sfs->listFiles();
    QSet<QString> subfolders;
    QList<QPair<QString,QPair<QString,quint64>>> filesHere; // disp, (fullname, sz)
    for(const SectorEntry& e : all){
        QString name = e.name;
        if(name.endsWith("/.keep")){
            QString relMarker = name.left(name.size()-6);
            if(!prefix.isEmpty()){
                if(relMarker.startsWith(prefix+"/")){
                    QString sub = relMarker.mid(prefix.size()+1);
                    if(!sub.contains('/')) subfolders.insert(sub);
                }
            } else {
                if(!relMarker.contains('/')) subfolders.insert(relMarker);
            }
            continue;
        }
        QString rest;
        if(!prefix.isEmpty()){
            if(!name.startsWith(prefix+"/")) continue;
            rest = name.mid(prefix.size()+1);
        } else rest = name;
        if(rest.contains('/')) subfolders.insert(rest.section('/',0,0));
        else filesHere.append(qMakePair(rest, qMakePair(name, e.sz)));
    }
    int nd=0, nf=0;
    QList<QString> folds = subfolders.values(); std::sort(folds.begin(), folds.end());
    for(const QString& fold : folds){
        if(!q.isEmpty() && !fold.toLower().contains(q)) continue;
        QTreeWidgetItem* it = new QTreeWidgetItem(QStringList()<<fold<<""<<""<<" ");
        it->setData(0, Qt::UserRole, fold);
        it->setData(0, Qt::UserRole+1, true);
        it->setIcon(0, st->standardIcon(QStyle::SP_DirIcon));
        m_tu->addTopLevelItem(it); nd++;
    }
    std::sort(filesHere.begin(), filesHere.end());
    for(const auto& pr : filesHere){
        QString disp = pr.first, fullname = pr.second.first; quint64 sz = pr.second.second;
        if(!q.isEmpty() && !disp.toLower().contains(q)) continue;
        QTreeWidgetItem* it = new QTreeWidgetItem(QStringList()<<disp<<fmtCol(disp,false)<<fmtSize((double)sz)<<" ");
        it->setData(0, Qt::UserRole, fullname);
        it->setData(0, Qt::UserRole+1, false);
        it->setIcon(0, st->standardIcon(QStyle::SP_FileIcon));
        m_tu->addTopLevelItem(it); nf++;
    }
    m_usbStat->setText(QString::fromUtf8("%1 thư mục, %2 file").arg(nd).arg(nf));
    m_usbSpace->setText(QString::fromUtf8("Dung lượng còn lại: %1").arg(fmtSize((double)m_sfs->getFree())));
}

// ==================== NAV ====================
void MainWindow::pcDoubleClicked(QTreeWidgetItem* it, int){
    QString fp = it->data(0, Qt::UserRole).toString();
    if(!fp.isEmpty() && it->data(0, Qt::UserRole+1).toBool()) loadPc(fp);
}
void MainWindow::pcUp(){
    QString p = QFileInfo(m_cp).absolutePath();
    if(!p.isEmpty() && p!=m_cp) loadPc(p);
}
void MainWindow::usbUp(){
    if(m_usbPath.isEmpty()) return;
    int idx = m_usbPath.lastIndexOf('/');
    m_usbPath = (idx>0)? m_usbPath.left(idx) : "";
    loadUsb();
}
void MainWindow::usbDoubleClicked(QTreeWidgetItem* it, int){
    QString name = it->data(0, Qt::UserRole).toString();
    bool isFolder = it->data(0, Qt::UserRole+1).toBool();
    if(name==".."){ usbUp(); return; }
    if(isFolder){
        m_usbPath = (m_usbPath.isEmpty()? "" : m_usbPath+"/") + name;
        loadUsb();
    } else if(!name.isEmpty()){
        // mở file: đọc raw ra temp rồi mở
        QTemporaryDir* td = new QTemporaryDir();
        QString out = td->path() + "/" + QFileInfo(name).fileName();
        bool ok=false;
        if(m_sfs->isStreamFile(name))
            ok = m_sfs->readStream(name, out, [](quint64,quint64){ return true; });
        else {
            QByteArray data = m_sfs->readSmall(name);
            QFile f(out); if(f.open(QIODevice::WriteOnly)){ f.write(data); f.close(); ok=true; }
        }
        if(ok) QDesktopServices::openUrl(QUrl::fromLocalFile(out));
        else QMessageBox::critical(this,"",QString::fromUtf8("Không mở được file!"));
    }
}

// ==================== COPY (worker) ====================
// Hoi nguoi dung khi co file trung ten. Tra ve: -1=Huy, 0=Giu ca hai, 1=Ghi de, 2=Bo qua
static int askOverwritePolicy(QWidget* parent, int n, const QString& sample=QString()){
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QString::fromUtf8("Trùng tên"));
    QString msg = QString::fromUtf8("Có %1 mục trùng tên đã tồn tại ở nơi đến.").arg(n);
    if(!sample.isEmpty())
        msg += QString::fromUtf8("\nVí dụ: \"%1\"").arg(sample);
    msg += QString::fromUtf8("\n\nBạn muốn xử lý thế nào?");
    box.setText(msg);
    QPushButton* bOver = box.addButton(QString::fromUtf8("Ghi đè"), QMessageBox::AcceptRole);
    QPushButton* bBoth = box.addButton(QString::fromUtf8("Giữ cả hai"), QMessageBox::ActionRole);
    QPushButton* bSkip = box.addButton(QString::fromUtf8("Bỏ qua"), QMessageBox::ActionRole);
    QPushButton* bCancel = box.addButton(QString::fromUtf8("Huỷ"), QMessageBox::RejectRole);
    box.setDefaultButton(bOver);
    box.exec();
    QAbstractButton* c = box.clickedButton();
    if(c==bCancel) return -1;
    if(c==bOver)   return 1;   // Ghi de (Replace)
    if(c==bSkip)   return 2;   // Bo qua (Skip)
    (void)bBoth;
    return 0;                  // Giu ca hai (KeepBoth)
}

void MainWindow::copyToUsb(){
    if(m_worker && m_worker->isRunning()){ QMessageBox::information(this,"",QString::fromUtf8("Đang copy, đợi hoặc bấm Dừng.")); return; }
    QList<QTreeWidgetItem*> sel = m_tp->selectedItems();
    if(sel.isEmpty() || !m_sfs) return;
    QString prefix = m_usbPath.isEmpty()? "" : (m_usbPath+"/");
    QList<CopyJob> jobs;
    for(QTreeWidgetItem* it : sel){
        QString fp = it->data(0, Qt::UserRole).toString();
        if(fp.isEmpty()) continue;
        if(it->data(0, Qt::UserRole+2).toString()=="up") continue;
        if(it->data(0, Qt::UserRole+1).toBool()){
            // thư mục -> duyệt file bên trong
            QString base = QFileInfo(fp).absolutePath();
            bool hasFile=false;
            QDirIterator dit(fp, QDir::Files, QDirIterator::Subdirectories);
            while(dit.hasNext()){
                QString full = dit.next();
                QString rel = QDir(base).relativeFilePath(full).replace('\\','/');
                CopyJob j; j.src=full; j.name=prefix+rel; j.size=(quint64)QFileInfo(full).size();
                jobs.append(j); hasFile=true;
            }
            if(!hasFile){ CopyJob j; j.marker=true; j.name=prefix+QFileInfo(fp).fileName()+"/.keep"; jobs.append(j); }
        } else {
            CopyJob j; j.src=fp; j.name=prefix+QFileInfo(fp).fileName(); j.size=(quint64)QFileInfo(fp).size();
            jobs.append(j);
        }
    }
    if(jobs.isEmpty()){ QMessageBox::information(this,"",QString::fromUtf8("Không có gì để copy!")); return; }
    // Kiem tra trung ten tren USB -> hoi ghi de
    CopyWorker::Overwrite policy = CopyWorker::KeepBoth;
    int collisions=0; QString sample;
    for(const CopyJob& j : jobs){
        if(j.marker) continue;
        if(m_sfs->exists(j.name)){ collisions++; if(sample.isEmpty()) sample=j.name.section('/',-1); }
    }
    if(collisions>0){
        int r = askOverwritePolicy(this, collisions, sample);
        if(r<0) return;                       // Huy -> khong copy
        policy = (CopyWorker::Overwrite)r;
    }
    m_pb->setVisible(true); m_pb->setValue(0);
    m_prow->setVisible(true); m_stopBtn->setVisible(true); m_stopBtn->setEnabled(true);
    m_copyLbl->setText(QString::fromUtf8("Đang chuẩn bị..."));
    m_worker = new CopyWorker(m_sfs, jobs, CopyWorker::ToUsb, this);
    m_worker->setOverwrite(policy);
    connect(m_worker, &CopyWorker::progress, this, &MainWindow::onProgress);
    connect(m_worker, &CopyWorker::errorMsg, this, [this](QString m){ statusBar()->showMessage("Lỗi: "+m, 4000); });
    connect(m_worker, &CopyWorker::finishedAll, this, [this](int ok,int tot,bool st){ onCopyDone(ok,tot,st,0); });
    m_worker->start();
}

void MainWindow::copyFromUsb(){
    if(m_worker && m_worker->isRunning()){ QMessageBox::information(this,"",QString::fromUtf8("Đang copy, đợi hoặc bấm Dừng.")); return; }
    QList<QTreeWidgetItem*> sel = m_tu->selectedItems();
    if(sel.isEmpty() || !m_sfs) return;
    QStringList names;
    QList<SectorEntry> all = m_sfs->listFiles();
    for(QTreeWidgetItem* it : sel){
        QString nm = it->data(0, Qt::UserRole).toString();
        bool isFolder = it->data(0, Qt::UserRole+1).toBool();
        if(nm=="..") continue;
        if(isFolder){
            QString foldPrefix = (m_usbPath.isEmpty()? "" : m_usbPath+"/") + nm + "/";
            for(const SectorEntry& e : all)
                if(e.name.startsWith(foldPrefix) && !e.name.endsWith("/.keep")) names<<e.name;
        } else if(!nm.isEmpty()) names<<nm;
    }
    if(names.isEmpty()){ QMessageBox::information(this,"",QString::fromUtf8("Không có file!")); return; }
    QMap<QString,quint64> nameSz;
    for(const SectorEntry& e : all) nameSz[e.name]=e.sz;
    QList<CopyJob> jobs;
    for(const QString& name : names){
        CopyJob j; j.name=name; j.out = QDir(m_cp).absoluteFilePath(QString(name).replace('/','/'));
        j.size = nameSz.value(name,0); jobs.append(j);
    }
    // Kiem tra trung ten ben MAY TINH -> hoi ghi de
    CopyWorker::Overwrite policy = CopyWorker::KeepBoth;
    int collisions=0; QString sample;
    for(const CopyJob& j : jobs){
        if(QFileInfo::exists(j.out)){ collisions++; if(sample.isEmpty()) sample=QFileInfo(j.out).fileName(); }
    }
    if(collisions>0){
        int r = askOverwritePolicy(this, collisions, sample);
        if(r<0) return;
        policy = (CopyWorker::Overwrite)r;
    }
    m_pb->setVisible(true); m_pb->setValue(0);
    m_prow->setVisible(true); m_stopBtn->setVisible(true); m_stopBtn->setEnabled(true);
    m_copyLbl->setText(QString::fromUtf8("Đang chuẩn bị..."));
    m_worker = new CopyWorker(m_sfs, jobs, CopyWorker::FromUsb, this);
    m_worker->setOverwrite(policy);
    connect(m_worker, &CopyWorker::progress, this, &MainWindow::onProgress);
    connect(m_worker, &CopyWorker::errorMsg, this, [this](QString m){ statusBar()->showMessage("Lỗi: "+m, 4000); });
    connect(m_worker, &CopyWorker::finishedAll, this, [this](int ok,int tot,bool st){ onCopyDone(ok,tot,st,1); });
    m_worker->start();
}

void MainWindow::onProgress(double done, double total, double speed, QString name){
    int pct = (int)(done/qMax(1.0,total)*100.0);
    m_pb->setValue(pct); m_pb->setFormat(QString::number(pct)+"%");
    m_copyLbl->setText(QString("%1  |  %2 / %3  (%4%)  |  %5/s")
        .arg(QFileInfo(name).fileName()).arg(fmtSize(done)).arg(fmtSize(total)).arg(pct).arg(fmtSize(speed)));
}

void MainWindow::onCopyDone(int ok, int total, bool stopped, int dir){
    m_stopBtn->setEnabled(false);
    if(dir==0) loadUsb(); else loadPc(m_cp);
    if(stopped){
        m_copyLbl->setText(QString::fromUtf8("Đã DỪNG. Copy được %1/%2 file.").arg(ok).arg(total));
    } else {
        m_pb->setValue(100); m_pb->setFormat(QString::fromUtf8("Hoàn tất 100%"));
        m_copyLbl->setText(QString::fromUtf8("Hoàn tất: %1/%2 file.").arg(ok).arg(total));
    }
    QTimer::singleShot(4000, this, [this](){ m_pb->setVisible(false); m_prow->setVisible(false); });
}

void MainWindow::stopCopy(){
    if(m_worker && m_worker->isRunning()){
        m_stopBtn->setEnabled(false);
        m_copyLbl->setText(QString::fromUtf8("Đang dừng..."));
        m_worker->requestStop();
    }
}

// ==================== FOLDERS / RENAME / DELETE ====================
void MainWindow::mkPcFolder(){
    if(m_cp.isEmpty() || !QDir(m_cp).exists()){ QMessageBox::information(this,"",QString::fromUtf8("Chọn thư mục PC trước.")); return; }
    bool ok; QString n = QInputDialog::getText(this, QString::fromUtf8("Tạo thư mục (PC)"),
                        QString::fromUtf8("Tên thư mục mới:"), QLineEdit::Normal, "", &ok);
    if(!ok || n.trimmed().isEmpty()) return;
    n = n.trimmed().replace('/','_').replace('\\','_');
    QDir(m_cp).mkdir(n);
    loadPc(m_cp);
}
void MainWindow::mkUsbFolder(){
    bool ok; QString n = QInputDialog::getText(this, QString::fromUtf8("Tạo thư mục (USB)"),
                        QString::fromUtf8("Tên thư mục mới:"), QLineEdit::Normal, "", &ok);
    if(!ok || n.trimmed().isEmpty()) return;
    n = n.trimmed().replace('/','_').replace('\\','_');
    QString marker = (m_usbPath.isEmpty()? "" : m_usbPath+"/") + n + "/.keep";
    m_sfs->writeSmall(marker, QByteArray());
    loadUsb();
}
void MainWindow::renameItem(){
    QList<QTreeWidgetItem*> sel = m_tu->selectedItems();
    if(sel.isEmpty()){ QMessageBox::information(this,"",QString::fromUtf8("Chọn 1 mục bên USB để đổi tên.")); return; }
    QTreeWidgetItem* it = sel.first();
    QString oldn = it->data(0, Qt::UserRole).toString();
    bool isFolder = it->data(0, Qt::UserRole+1).toBool();
    if(oldn=="..") return;
    QString cur = isFolder? oldn : oldn.section('/',-1);
    bool ok; QString nw = QInputDialog::getText(this, QString::fromUtf8("Đổi tên"),
                        QString::fromUtf8("Tên mới:"), QLineEdit::Normal, cur, &ok);
    if(!ok || nw.trimmed().isEmpty()) return;
    nw = nw.trimmed().replace('/','_').replace('\\','_');
    m_sfs->readTable();
    QString prefix = m_usbPath;
    if(isFolder){
        QString base = (prefix.isEmpty()? "" : prefix+"/") + oldn;
        QString newbase = (prefix.isEmpty()? "" : prefix+"/") + nw;
        QList<SectorEntry> all = m_sfs->listFiles();
        for(const SectorEntry& e : all){
            if(e.name==base+"/.keep" || e.name.startsWith(base+"/")){
                QString rest = e.name.mid(base.size());
                m_sfs->renameEntry(e.name, newbase+rest);
            }
        }
    } else {
        QString parent = oldn.contains('/')? oldn.section('/',0,-2) : "";
        QString newn = (parent.isEmpty()? "" : parent+"/") + nw;
        m_sfs->renameEntry(oldn, newn);
    }
    loadUsb();
}
void MainWindow::deleteData(){
    QList<QTreeWidgetItem*> selPc = m_tp->selectedItems();
    QList<QTreeWidgetItem*> selUsb = m_tu->selectedItems();
    // Bo qua dong ".." (di len) khi dem lua chon
    auto realCount=[](const QList<QTreeWidgetItem*>& lst)->int{
        int c=0;
        for(QTreeWidgetItem* it : lst){
            if(it->data(0, Qt::UserRole+2).toString()=="up") continue;
            if(it->data(0, Qt::UserRole).toString()=="..") continue;
            c++;
        }
        return c;
    };
    int nPc = realCount(selPc), nUsb = realCount(selUsb);
    if(nPc==0 && nUsb==0){
        QMessageBox::information(this,"",QString::fromUtf8("Chọn mục (bên MÁY TÍNH hoặc USB) để xoá."));
        return;
    }
    QString where = (nPc>0 && nUsb>0)? QString::fromUtf8("MÁY TÍNH và USB")
                   : (nPc>0? QString::fromUtf8("MÁY TÍNH") : QString::fromUtf8("USB"));
    if(QMessageBox::warning(this,"",
        QString::fromUtf8("Xoá các mục đã chọn bên %1?\nKhông thể khôi phục!").arg(where),
        QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes) return;

    // 1) Xoa ben MAY TINH (file/thu muc that)
    if(nPc>0){
        for(QTreeWidgetItem* it : selPc){
            if(it->data(0, Qt::UserRole+2).toString()=="up") continue;
            QString fp = it->data(0, Qt::UserRole).toString();
            if(fp.isEmpty()) continue;
            SetFileAttributesW((LPCWSTR)fp.utf16(), FILE_ATTRIBUTE_NORMAL);
            if(it->data(0, Qt::UserRole+1).toBool()) QDir(fp).removeRecursively();
            else QFile::remove(fp);
        }
        loadPc(m_cp);
    }

    // 2) Xoa ben USB (sector FS) - giu nguyen logic cu
    if(nUsb>0){
        m_sfs->readTable();
        QList<SectorEntry> all = m_sfs->listFiles();
        for(QTreeWidgetItem* it : selUsb){
            QString nm = it->data(0, Qt::UserRole).toString();
            if(nm=="..") continue;
            if(it->data(0, Qt::UserRole+1).toBool()){
                QString fold = (m_usbPath.isEmpty()? "" : m_usbPath+"/") + nm;
                for(const SectorEntry& e : all)
                    if(e.name==fold+"/.keep" || e.name.startsWith(fold+"/")) m_sfs->deleteFile(e.name);
            } else {
                m_sfs->deleteFile(nm);
            }
        }
        loadUsb();
    }
}
void MainWindow::formatUsb(){
    bool ok; QString pw = QInputDialog::getText(this, QString::fromUtf8("Format USB"),
        QString::fromUtf8("Nhập mật khẩu Admin:"), QLineEdit::Password, "", &ok);
    if(!ok) return;
    if(pw != ADMIN_PW){ QMessageBox::critical(this,"",QString::fromUtf8("Sai mật khẩu Admin!")); return; }
    if(QMessageBox::warning(this,"",QString::fromUtf8("XOÁ TOÀN BỘ dữ liệu USB? Không thể khôi phục!"),
        QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes) return;
    m_sfs->rebuild();
    m_usbPath="";
    loadUsb();
    QMessageBox::information(this,"",QString::fromUtf8("Đã format USB AN TOÀN."));
}
void MainWindow::changeLoginPw(){
    bool ok;
    QString oldp = QInputDialog::getText(this, QString::fromUtf8("Đổi mật khẩu"),
        QString::fromUtf8("Mật khẩu hiện tại:"), QLineEdit::Password, "", &ok);
    if(!ok) return;
    QByteArray salt = QByteArray::fromHex(m_salt.toUtf8());
    if(hashPw(oldp, salt) != m_pwHash){ QMessageBox::critical(this,"",QString::fromUtf8("Sai mật khẩu!")); return; }
    QString n1 = QInputDialog::getText(this, QString::fromUtf8("Đổi mật khẩu"),
        QString::fromUtf8("Mật khẩu mới (≥6):"), QLineEdit::Password, "", &ok);
    if(!ok || n1.size()<6){ QMessageBox::warning(this,"",QString::fromUtf8("Mật khẩu ≥6 ký tự!")); return; }
    QByteArray nsalt(16,'\0'); QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(nsalt.data()), 4);
    QString nhash = hashPw(n1, nsalt);
    QJsonObject o = QJsonDocument::fromJson(m_cfg).object();
    o["salt"] = QString(nsalt.toHex()); o["pw_hash"] = nhash;
    m_cfg = QJsonDocument(o).toJson(QJsonDocument::Compact);
    m_salt = o["salt"].toString(); m_pwHash = nhash;
    m_sfs->writeConfig(m_cfg);
    QMessageBox::information(this,"",QString::fromUtf8("Đổi mật khẩu thành công!"));
}
void MainWindow::showHelp(){
    QMessageBox::information(this, QString::fromUtf8("Hướng dẫn"),
        QString::fromUtf8("USB AN TOÀN\n\n"
        "• Chọn file bên trái (MÁY TÍNH), bấm mũi tên → để copy sang USB\n"
        "• Chọn file bên phải (USB), bấm mũi tên ← để copy về máy\n"
        "• Nhấn đúp file trên USB để mở xem\n"
        "• Tạo TM (PC)/(USB), Đổi tên, Xóa dữ liệu, Format USB (cần Admin)\n"
        "• Thoát: đóng ứng dụng\n\n"
        "Dữ liệu ẩn hoàn toàn, KHÔNG copy trực tiếp vào USB được."));
}

// ==================== SELECTED SIZE ====================
void MainWindow::pcSelSize(){
    QList<QTreeWidgetItem*> items = m_tp->selectedItems();
    double total=0; int nf=0, nd=0;
    for(QTreeWidgetItem* it : items){
        QString fp = it->data(0, Qt::UserRole).toString();
        if(fp.isEmpty() || it->data(0, Qt::UserRole+2).toString()=="up") continue;
        if(it->data(0, Qt::UserRole+1).toBool()){
            nd++;
            QDirIterator dit(fp, QDir::Files, QDirIterator::Subdirectories);
            while(dit.hasNext()){ dit.next(); total += (double)dit.fileInfo().size(); }
        } else { nf++; total += (double)QFileInfo(fp).size(); }
    }
    if(nf==0 && nd==0){ showSel(""); return; }
    QStringList parts;
    if(nd) parts<<QString::fromUtf8("%1 thư mục").arg(nd);
    if(nf) parts<<QString::fromUtf8("%1 file").arg(nf);
    showSel(QString::fromUtf8("Đã chọn (PC) — %1:  %2").arg(parts.join(", ")).arg(fmtSize(total)));
}
void MainWindow::usbSelSize(){
    if(!m_sfs) return;
    QList<QTreeWidgetItem*> items = m_tu->selectedItems();
    double total=0; int nf=0, nd=0;
    QList<SectorEntry> all = m_sfs->listFiles();
    for(QTreeWidgetItem* it : items){
        QString nm = it->data(0, Qt::UserRole).toString();
        if(nm=="..") continue;
        if(it->data(0, Qt::UserRole+1).toBool()){
            nd++;
            QString fold = (m_usbPath.isEmpty()? "" : m_usbPath+"/") + nm + "/";
            for(const SectorEntry& e : all)
                if(e.name.startsWith(fold) && !e.name.endsWith("/.keep")) total += (double)e.sz;
        } else if(!nm.isEmpty()){
            nf++;
            for(const SectorEntry& e : all) if(e.name==nm){ total += (double)e.sz; break; }
        }
    }
    if(nf==0 && nd==0){ showSel(""); return; }
    QStringList parts;
    if(nd) parts<<QString::fromUtf8("%1 thư mục").arg(nd);
    if(nf) parts<<QString::fromUtf8("%1 file").arg(nf);
    showSel(QString::fromUtf8("Đã chọn (USB) — %1:  %2").arg(parts.join(", ")).arg(fmtSize(total)));
}

void MainWindow::closeEvent(QCloseEvent* ev){
    if(m_worker && m_worker->isRunning()){ m_worker->requestStop(); m_worker->wait(3000); }
    if(m_guard){ m_guard->requestStop(); m_guard->wait(2000); }
    if(m_sfs){ m_sfs->close(); }
    ev->accept();
}
