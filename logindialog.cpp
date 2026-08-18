#include "logindialog.h"
#include "diskutil.h"
#include "common.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

LoginDialog::LoginDialog(const QString& salt, const QString& pwHash, QWidget* p)
    : QDialog(p), m_salt(salt), m_hash(pwHash){
    setWindowTitle(QString::fromUtf8(APP_NAME) + " - Đăng nhập");
    setFixedSize(340, 200);
    QVBoxLayout* v = new QVBoxLayout(this);
    QLabel* t = new QLabel(QString::fromUtf8("USB AN TOÀN"));
    t->setStyleSheet("font-size:18px;font-weight:bold;color:#1565c0;");
    t->setAlignment(Qt::AlignCenter);
    v->addWidget(t);
    v->addWidget(new QLabel(QString::fromUtf8("Nhập mật khẩu đăng nhập:")));
    m_edit = new QLineEdit;
    m_edit->setEchoMode(QLineEdit::Password);
    v->addWidget(m_edit);
    QPushButton* b = new QPushButton(QString::fromUtf8("Đăng nhập"));
    b->setStyleSheet("background:#1565c0;color:white;font-weight:bold;padding:8px;border:none;border-radius:4px;");
    v->addWidget(b);
    connect(b, &QPushButton::clicked, this, &LoginDialog::tryLogin);
    connect(m_edit, &QLineEdit::returnPressed, this, &LoginDialog::tryLogin);
}

void LoginDialog::tryLogin(){
    QString pw = m_edit->text();
    QByteArray salt = QByteArray::fromHex(m_salt.toUtf8());
    QString h = hashPw(pw, salt);
    if(h == m_hash){
        m_pw = pw;
        accept();
    } else {
        m_att--;
        if(m_att<=0){
            QMessageBox::critical(this, "", QString::fromUtf8("Sai mật khẩu nhiều lần!"));
            reject();
        } else {
            QMessageBox::warning(this, "", QString::fromUtf8("Sai mật khẩu! Còn %1 lần.").arg(m_att));
            m_edit->clear();
        }
    }
}
