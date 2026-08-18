#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <windows.h>
#include "common.h"
#include "diskutil.h"
#include "diskio.h"
#include "sectorfs.h"
#include "setupwindow.h"
#include "logindialog.h"
#include "mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
// Ban static: qmake tu dong import qwindows -> KHONG Q_IMPORT_PLUGIN thu cong
// (neu them tay se bi trung ky hieu luc link).

static bool isSetup(){
    QString base = QFileInfo(QCoreApplication::applicationFilePath()).fileName().toLower();
    return base.contains("setup");
}
static bool isAdmin(){
    BOOL admin = FALSE;
    PSID grp = NULL;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if(AllocateAndInitializeSid(&nt,2,SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&grp)){
        CheckTokenMembership(NULL, grp, &admin);
        FreeSid(grp);
    }
    return admin;
}
static bool runAsAdmin(){
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    HINSTANCE r = ShellExecuteW(NULL, L"runas", path, L"", NULL, SW_SHOWNORMAL);
    return (INT_PTR)r > 32;
}

int main(int argc, char** argv){
    // Runner cần Admin để đọc/ghi sector -> tự nâng quyền
    if(!isSetup() && !isAdmin()){
        if(runAsAdmin()) return 0;
    }
    QApplication app(argc, argv);
    app.setApplicationName(APP_NAME);

    if(isSetup()){
        if(!isAdmin()) QMessageBox::warning(nullptr,"",QString::fromUtf8("Cần chạy bằng quyền Admin!"));
        SetupWindow* w = new SetupWindow();
        w->show();
        return app.exec();
    }

    // Runner: tìm USB, đọc config từ sector, đăng nhập
    QString root = findUsbRoot();
    if(root.isEmpty()){ QMessageBox::critical(nullptr,"",QString::fromUtf8("Không tìm thấy USB!")); return 1; }
    int dn = getPhysicalDriveNumber(root);
    quint64 s,n, off=0;
    if(readMbrPartition(dn,1,s,n)) off=s+n;
    QByteArray cfgJson;
    {
        SectorFS sfs(dn, off);
        if(sfs.open()){ cfgJson = sfs.readConfig(); sfs.close(); }
    }
    QJsonObject o = QJsonDocument::fromJson(cfgJson).object();
    if(cfgJson.isEmpty() || !o.contains("pw_hash")){
        QMessageBox::critical(nullptr,"",QString::fromUtf8("Chưa khởi tạo đúng! Chạy Setup lại."));
        return 1;
    }
    LoginDialog dlg(o.value("salt").toString(), o.value("pw_hash").toString());
    if(dlg.exec()==QDialog::Accepted){
        MainWindow* w = new MainWindow(root, dlg.password(), cfgJson);
        w->show();
        return app.exec();
    }
    return 0;
}
