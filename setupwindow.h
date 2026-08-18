#ifndef SETUPWINDOW_H
#define SETUPWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QList>
#include "diskutil.h"

class SetupWindow : public QMainWindow {
    Q_OBJECT
public:
    SetupWindow(QWidget* p=nullptr);
private slots:
    void refreshUsb();
    void doInit();
private:
    QComboBox* m_usbCombo;
    QLineEdit* m_pw1;
    QLineEdit* m_pw2;
    QLabel* m_status;
    QList<UsbCandidate> m_cands;
};

#endif // SETUPWINDOW_H
