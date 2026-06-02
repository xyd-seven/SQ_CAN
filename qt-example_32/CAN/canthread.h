#ifndef CANTHREAD_H
#define CANTHREAD_H

#include <QThread>
#include <QMutex>
#include <atomic>
#include <QVector>
#include <zlgcan.h>
#include <QDebug>

class CANThread:public QThread
{
    Q_OBJECT
public:
    CANThread();//构造函数声明；

    void stop();

    //1.打开设备
    bool openDevice(UINT device_type, UINT device_index, UINT reserved);

    //2.设置CANFD标准
    bool setCANFDStandard(UINT standard);

    //3-1.设置波特率
    bool setBaudrateFD(UINT rate1,UINT rate2);

    //3-2.设置波特率
    bool setCustomBaudrateFD(QString rate);

    //4.初始化CAN
    bool initCAN();

    //5.使能终端电阻
    bool setResistanceEnable(UINT enable);

    //6.设置滤波 可不设置
    bool setFilter(UINT filterMode,QString startID,QString endID);

    //7.启动CAN
    bool startCAN();

    //8-1.发送数据
    bool sendData(UINT id,UINT frame_type_index,UINT protocol_index,UINT canfd_exp_index,UINT channel,const char *data,UINT len);

    //9.关闭设备
    void closeDevice();

    //0.复位设备，  复位后回到4
    bool reSetCAN();

    UINT MakeCanId(uint id, int eff, int rtr, int err);

    std::atomic<bool> stopped;
    QMutex m_channelMutex;
    DEVICE_HANDLE m_dev;
    CHANNEL_HANDLE m_channel1;
    CHANNEL_HANDLE m_channel2;
    IProperty* devPtr;
    ZCAN_CHANNEL_INIT_CONFIG m_config;
//    bool m_isSetValue;

signals:
    //8-2.接收数据
    void recvedCANData(QVector<ZCAN_Receive_Data> recvCANData,UINT frameCount,UINT channel);
    void recvedCANFDData(QVector<ZCAN_ReceiveFD_Data> recvCANFDData,UINT frameCount,UINT channel);

private:
    void run();
    void sleep(unsigned int msec);

};

#endif // CANTHREAD_H
