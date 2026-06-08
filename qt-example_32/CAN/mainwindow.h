#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <zlgcan.h>
#include "canthread.h"
#include <QThread>
#include <QCloseEvent>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QTime>
#include <QTimer>
#include <QComboBox>
#include <QCheckBox>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include "imuwidgets.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSettings>
#include <QPlainTextEdit>
#include <QLineEdit>


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void canRecvedCANData(QVector<ZCAN_Receive_Data> recvCANData,UINT frameCount,UINT channel);
    void canRecvedCANFDData(QVector<ZCAN_ReceiveFD_Data> recvCANFDData,UINT frameCount,UINT channel);
    void on_cleanListBtn_clicked();

    void on_openDeviceBtn_clicked();

    void on_closeDeviceBtn_clicked();

    void on_initCANBtn_clicked();

    void on_StartCANBtn_clicked();

    bool afterReSet();

    void on_reSetCANBtn_clicked();

    void on_sendBtn_clicked();

    void AddDataToList(QStringList strList);

    void closeEvent(QCloseEvent *event);
    
    // 二次开发新增：定时刷新UI的槽函数
    void refreshIMUDashboard();

private:
    Ui::MainWindow *ui;
    CANThread *canthread;

    // 二次开发新增：监控面板组件与定时器
    QTimer *refreshTimer;
    
    // UI 指针
    QWidget *imuTab;
    QLabel *valAx;
    QLabel *valAy;
    QLabel *valAz;
    QLabel *valGx;
    QLabel *valGy;
    QLabel *valGz;
    QLabel *valRoll;
    QLabel *valPitch;
    QLabel *valYaw;
    
    // 图形化指示条 (QProgressBar)
    QProgressBar *barRoll;
    QProgressBar *barPitch;
    QProgressBar *barYaw;
    
    QLabel *valFaultLevel;
    QLabel *valVer;
    QLabel *valDate;
    QLabel *valInternalVer;
    QLabel *valVerType;
    QLabel *valPlatformType;
    QLabel *valPlatformVarNo;
    QTableWidget *faultTableWidget;
    
    // 二次开发新增：动态罗盘和姿态仪控件指针
    CompassWidget *compassWidget;
    AttitudeIndicatorWidget *attitudeWidget;
    QComboBox *cmbDisplayAxisX; // 图形显示坐标系 X 轴来源，仅影响罗盘和2D姿态图动画
    QComboBox *cmbDisplayAxisY; // 图形显示坐标系 Y 轴来源，仅影响罗盘和2D姿态图动画
    QComboBox *cmbDisplayAxisZ; // 图形显示坐标系 Z 轴来源，仅影响罗盘和2D姿态图动画
    
    // 第三阶段新增：原始数据透传界面组件
    QWidget *rawTab;
    QPushButton *btnStartRawMode;
    QPushButton *btnStopRawMode;
    QLabel *lblRawStatus;
    QLabel *lblRawStats;
    QTableWidget *rawTableWidget;
    CompassWidget *rawCompassWidget;
    AttitudeIndicatorWidget *rawAttitudeWidget;
    QTimer *rawTimeoutTimer; // 监测透传帧是否在正常到达的定时器
    QTime m_lastRawTime;
    QPushButton *btnQuickStart; // 一键启动按钮

    // CAN 透传下行指令相关的 UI 组件
    QLineEdit *rawEditLon;
    QLineEdit *rawEditLat;
    QLineEdit *rawEditAlt;
    QLineEdit *rawEditAlignTime;
    QLineEdit *rawEditBindHeading;
    QLineEdit *rawEditFrontLever;
    QLineEdit *rawEditRightLever;
    QLineEdit *rawEditUpLever;
    QComboBox *rawCmbOutBaud;
    QComboBox *rawCmbOutPeriod;
    QLineEdit *rawEditRollError;
    QLineEdit *rawEditPitchError;
    QLineEdit *rawEditYawError;
    QComboBox *rawCmbCmdType;
    QLineEdit *rawEditCustomHexCmd;
    QLineEdit *rawEditSendHex;
    QPushButton *rawBtnSendCmd;

    // 第四阶段新增：数据保存及格式选择组件
    QCheckBox *chkSaveCAN;
    QComboBox *cmbFormatCAN;
    QCheckBox *chkSaveRaw;
    QCheckBox *chkSaveParsedIMU;

    QFile m_canFile;
    QTextStream m_canStream;
    QFile m_parsedIMUFile;
    QTextStream m_parsedIMUStream;
    QDateTime m_parsedIMUStartTime;
    bool m_parsedIMUElapsedBaseValid;
    qint64 m_parsedIMUElapsedBaseMs;
    QFile m_rawFile;
    QTextStream m_rawStream;
    QFile m_rawBinFile; // 追加的原始数据二进制流文件
    QTimer *flushTimer; // 数据定时Flush定时器

    void setupIMUDashboard(); // 初始化仪表盘界面的函数
    void setupRawTab();        // 初始化原始透传界面的函数
    void setupSerialTab();     // 初始化串口控制界面的函数
    void convertDisplayAttitude(double roll, double pitch, double yaw,
                                double &displayRoll, double &displayPitch, double &displayYaw);
    void saveParsedIMUSnapshot(UINT canId, UINT channel, const QString &receiveTimeText, qint64 receiveElapsedMs);
    void stopCANSaveWithWarning(const QString &errorMessage);
    void stopRawSaveWithWarning(const QString &errorMessage);
    
    // 串口发送指令的底层封包逻辑
    QByteArray encodeCommandFrame(double lon, double lat, double alt, double frontLever, double rightLever, double upLever, int validMode, int baudrate, int period, double rollErr, double yawErr, double pitchErr, int userCmd, double alignTime);
    void showFlashBlockData(int blockId, const QByteArray &blockData, const QByteArray &rawFrame);

private slots:
    void startRawModeClicked();
    void stopRawModeClicked();
    void checkRawDataTimeout();
    void onSaveCANStateChanged(int state);
    void onSaveParsedIMUStateChanged(int state);
    void onSaveRawStateChanged(int state);
    void onQuickStartClicked();
    void onFlushTimeout();
    void onSendRawCommand();
    void onRawCmdTypeChanged(int index);
    // 串口相关的槽函数
    void onRefreshSerialPorts();
    void onToggleSerialConnection();
    void onSerialReadyRead();
    void onSendSerialCommand();
    void onSaveSerialStateChanged(int state);
    void onDecodeBinClicked();

private:
    // 串口控制界面组件
    QWidget *serialTab;
    QSerialPort *m_serialPort;
    QByteArray m_serialBuffer;
    bool m_isWaitingForAck;
    QTime m_lastCommandSentTime;
    
    QComboBox *serialPortCombo;
    QPushButton *btnRefreshSerial;
    QComboBox *serialBaudCombo;
    QComboBox *serialDownsampleCombo;
    QPushButton *btnToggleSerial;
    
    QLineEdit *editLon;
    QLineEdit *editLat;
    QLineEdit *editAlt;
    QLineEdit *editAlignTime;
    QLineEdit *editBindHeading;
    QComboBox *cmbOutBaud;
    QComboBox *cmbOutPeriod;
    QLineEdit *editRollError;
    QLineEdit *editPitchError;
    QLineEdit *editYawError;
    
    // 杆臂输入控件
    QLineEdit *editFrontLever;
    QLineEdit *editRightLever;
    QLineEdit *editUpLever;
    
    QComboBox *cmbCmdType;
    QPushButton *btnSendSerialCmd;
    QLineEdit *editSendHex;
    
    CompassWidget *serialCompassWidget;
    AttitudeIndicatorWidget *serialAttitudeWidget;
    QTableWidget *serialTableWidget;
    QCheckBox *chkSaveSerial;
    QPushButton *btnDecodeBin;
    QPlainTextEdit *serialConsoleLog;
    
    QFile m_serialTxtFile;
    QFile m_serialBinFile;
    QTextStream m_serialTxtStream;
    
    unsigned int m_serialUiUpdateCounter = 0;

    bool m_canTimestampBaseValid[2];
    UINT64 m_canTimestampBaseRaw[2];
    QDateTime m_canTimestampBaseHostTime[2];
    QString formatCANReceiveTime(UINT64 deviceTimestampRaw, UINT channel);
    qint64 getCANReceiveElapsedMs(UINT64 deviceTimestampRaw, UINT channel);
};

#endif // MAINWINDOW_H
