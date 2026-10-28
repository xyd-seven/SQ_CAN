#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include <QDir>

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    QApplication a(argc, argv);
    // zlgcan.dll 按当前工作目录查找 kerneldlls，确保从快捷方式或脚本启动时也能找到设备驱动。
    QDir::setCurrent(QCoreApplication::applicationDirPath());
    a.setWindowIcon(QIcon(":/icons/icons/app.png"));
    MainWindow w;
    w.setWindowIcon(QIcon(":/icons/icons/app.png"));
    w.show();

    return a.exec();
}
