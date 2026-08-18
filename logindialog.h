#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QString>

// Dialog đăng nhập: kiểm tra mật khẩu theo salt+hash lưu trong config
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    LoginDialog(const QString& salt, const QString& pwHash, QWidget* p=nullptr);
    QString password() const { return m_pw; }
private slots:
    void tryLogin();
private:
    QLineEdit* m_edit;
    QString m_salt, m_hash, m_pw;
    int m_att = 5;
};

#endif // LOGINDIALOG_H
