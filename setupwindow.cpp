#include "setupwindow.h"
#include "common.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QApplication>

SetupWindow::SetupWindow(QWidget* p): QMainWindow(p){
    setWindowTitle(QString::fromUtf8(APP_NAME) + " - Cài đặt USB");
    setFixedSize(460, 380);
    QWidget* cw = new QWidget; setCentralWidget(cw);
    QVBoxLayout* v = new QVBoxLayout(cw);

    QLabel* t = new QLabel(QString::fromUtf8("KHỞI TẠO USB AN TOÀN"));
    t->setStyleSheet("font-size:18px;font-weight:bold;color:#1565c0;");
    t->setAlignment(Qt::AlignCenter);
    v->addWidget(t);

    v->addWidget(new QLabel(QString::fromUtf8("Chọn USB (sẽ bị XOÁ toàn bộ):")));
    QHBoxLayout* r1 = new QHBoxLayout;
    m_usbCombo = new QComboBox;
    r1->addWidget(m_usbCombo, 1);
    QPushButton* rf = new QPushButton(QString::fromUtf8("Làm mới"));
    connect(rf, &QPushButton::clicked, this, &SetupWindow::refreshUsb);
    r1->addWidget(rf);
    v->addLayout(r1);

    v->addWidget(new QLabel(QString::fromUtf8("Mật khẩu đăng nhập (≥6 ký tự):")));
    m_pw1 = new QLineEdit; m_pw1->setEchoMode(QLineEdit::Password);
    v->addWidget(m_pw1);
    v->addWidget(new QLabel(QString::fromUtf8("Nhập lại mật khẩu:")));
    m_pw2 = new QLineEdit; m_pw2->setEchoMode(QLineEdit::Password);
    v->addWidget(m_pw2);

    QPushButton* b = new QPushButton(QString::fromUtf8("KHỞI TẠO"));
    b->setStyleSheet("background:#1565c0;color:white;font-weight:bold;padding:10px;border:none;border-radius:4px;font-size:14px;");
    connect(b, &QPushButton::clicked, this, &SetupWindow::doInit);
    v->addWidget(b);

    m_status = new QLabel("");
    m_status->setStyleSheet("color:#555;font-size:12px;");
    m_status->setWordWrap(true);
    v->addWidget(m_status);
    v->addStretch();

    refreshUsb();
}

void SetupWindow::refreshUsb(){
    m_usbCombo->clear();
    m_cands = listRemovableUsb();
    for(const UsbCandidate& c : m_cands){
        m_usbCombo->addItem(QString("%1  (disk %2, %3)")
            .arg(c.letter).arg(c.diskNumber).arg(fmtSize((double)c.sizeBytes)));
    }
    if(m_cands.isEmpty()) m_status->setText(QString::fromUtf8("Không thấy USB nào. Cắm USB rồi bấm Làm mới."));
}

void SetupWindow::doInit(){
    int idx = m_usbCombo->currentIndex();
    if(idx<0 || idx>=m_cands.size()){ QMessageBox::warning(this,"",QString::fromUtf8("Chọn USB!")); return; }
    QString p1=m_pw1->text(), p2=m_pw2->text();
    if(p1.size()<6){ QMessageBox::warning(this,"",QString::fromUtf8("Mật khẩu ≥6 ký tự!")); return; }
    if(p1!=p2){ QMessageBox::warning(this,"",QString::fromUtf8("Mật khẩu nhập lại không khớp!")); return; }
    int dn = m_cands[idx].diskNumber;
    if(QMessageBox::warning(this,"",QString::fromUtf8("XOÁ TOÀN BỘ dữ liệu trên %1?").arg(m_cands[idx].letter),
                            QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes) return;
    QString msg;
    bool ok = setupUsb(dn, p1, [this](const QString& s){
        m_status->setText(s); QApplication::processEvents();
    }, msg);
    if(ok) QMessageBox::information(this,"",msg);
    else   QMessageBox::critical(this,"",msg);
    m_status->setText(msg);
}
