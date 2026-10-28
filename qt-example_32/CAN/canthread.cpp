#include "canthread.h"
#include <QTime>
#include <QCoreApplication>
#include <QMetaType>
#include <QDir>
#include <QFile>
#include <string.h>
#ifdef Q_OS_WIN
extern "C" unsigned long __stdcall GetLastError(void);
extern "C" int __stdcall SetDllDirectoryA(const char *lpPathName);
#endif

CANThread::CANThread()
{
    stopped = false;
    m_dev = INVALID_DEVICE_HANDLE;
    m_channel1 = INVALID_CHANNEL_HANDLE;
    m_channel2 = INVALID_CHANNEL_HANDLE;
    m_channelCount = 2;
    devPtr = nullptr;
    memset(&m_config,0,sizeof(m_config));
}

void CANThread::stop()
{
    stopped = true;
}

void CANThread::setChannelCount(UINT count)
{
    m_channelCount = (count == 0) ? 1 : (count > 2 ? 2 : count);
}

QString CANThread::lastOpenError() const
{
    return m_lastOpenError;
}

//1.打开设备
bool CANThread::openDevice(UINT device_type, UINT device_index, UINT reserved)
{
    m_lastOpenError.clear();
#ifdef Q_OS_WIN
    // zlgcan loads the selected kernel driver dynamically. Make the driver
    // directory part of the process DLL search path, including dependencies
    // such as the vendor USB transport module.
    SetDllDirectoryA("kerneldlls");
#endif
    if(INVALID_DEVICE_HANDLE != m_dev)//设备已打开
    {
        m_lastOpenError = QString("设备句柄仍处于打开状态，当前类型=%1，索引=%2")
                .arg(device_type).arg(device_index);
        return false;
    }
    m_dev = ZCAN_OpenDevice(device_type,device_index,reserved);
    if(INVALID_DEVICE_HANDLE == m_dev)
    {
#ifdef Q_OS_WIN
        const DWORD winError = GetLastError();
#else
        const unsigned long winError = 0;
#endif
        m_lastOpenError = QString("ZCAN_OpenDevice 失败：type=%1，index=%2，GetLastError=%3，工作目录=%4，驱动文件=%5，MSVCR120=%6，MSVCP120=%7")
                .arg(device_type)
                .arg(device_index)
                .arg(static_cast<qulonglong>(winError))
                .arg(QDir::currentPath())
                .arg(QFile::exists(QDir::current().filePath("kerneldlls/USBCANFD.dll")) ? "存在" : "不存在")
                .arg(QFile::exists(QDir::current().filePath("MSVCR120.dll")) ? "存在" : "缺失")
                .arg(QFile::exists(QDir::current().filePath("MSVCP120.dll")) ? "存在" : "缺失");
        return false;
    }
    devPtr = GetIProperty(m_dev);
    if(devPtr == nullptr)//设置指定路径属性失败
    {
        ZCAN_CloseDevice(m_dev);
        m_dev = INVALID_DEVICE_HANDLE;
        m_lastOpenError = QString("GetIProperty 失败：type=%1，index=%2，工作目录=%3，驱动文件=%4")
                .arg(device_type)
                .arg(device_index)
                .arg(QDir::currentPath())
                .arg(QFile::exists(QDir::current().filePath("kerneldlls/USBCANFD.dll")) ? "存在" : "不存在");
        return false;
    }
    return true;
}

//2.设置CANFD标准
bool CANThread::setCANFDStandard(UINT standard)
{
    for (UINT channel = 0; channel < m_channelCount; ++channel) {
        const QString path = QString::number(channel) + "/canfd_standard";
        if (1 != devPtr->SetValue(path.toStdString().c_str(), QString::number(standard).toStdString().c_str()))
            return false;
    }
    //       if(1 != ZCAN_SetCANFDStandard(m_dev,0,standard))
    //           return false;
    //       if(1 != ZCAN_SetCANFDStandard(m_dev,1,standard))
    //           return false;
    return true;
}

//3.设置波特率
bool CANThread::setBaudrateFD(UINT rate1,UINT rate2)
{
    for (UINT channel = 0; channel < m_channelCount; ++channel) {
        const QString prefix = QString::number(channel) + "/";
        if (1 != devPtr->SetValue((prefix + "canfd_abit_baud_rate").toStdString().c_str(), QString::number(rate1).toStdString().c_str()))
            return false;
        if (1 != devPtr->SetValue((prefix + "canfd_dbit_baud_rate").toStdString().c_str(), QString::number(rate2).toStdString().c_str()))
            return false;
    }
    //        if(1 != ZCAN_SetAbitBaud(m_dev,0,rate1))
    //            return false;
    //        if(1 != ZCAN_SetAbitBaud(m_dev,1,rate1))
    //            return false;
    //        if(1 != ZCAN_SetDbitBaud(m_dev,0,rate1))
    //            return false;
    //        if(1 != ZCAN_SetDbitBaud(m_dev,1,rate1))
    //            return false;
    return true;
}

bool CANThread::setCustomBaudrateFD(QString rate)
{
    for (UINT channel = 0; channel < m_channelCount; ++channel) {
        const QString path = QString::number(channel) + "/baud_rate_custom";
        if (1 != devPtr->SetValue(path.toStdString().c_str(), rate.toStdString().c_str()))
            return false;
    }
    //        const char * cpc = rate.toStdString().c_str();
    //        char *pc = new char[strlen(cpc) + 1];
    //        strcpy(pc, cpc);
    //        if(1 != ZCAN_SetBaudRateCustom(m_dev,0,pc))
    //            return false;
    //        if(1 != ZCAN_SetBaudRateCustom(m_dev,1,pc))
    //            return false;
    return true;
}

//4.初始化CAN
bool CANThread::initCAN()
{
    m_config.can_type = 1;
    m_channel1 = ZCAN_InitCAN(m_dev,0,&m_config);
    if(nullptr == m_channel1)
        return false;
    if (m_channelCount >= 2) {
        m_channel2 = ZCAN_InitCAN(m_dev,1,&m_config);
        if(nullptr == m_channel2)
            return false;
    }
    return true;
}

//5.使能终端电阻
bool CANThread::setResistanceEnable(UINT enable)
{
    for (UINT channel = 0; channel < m_channelCount; ++channel) {
        const QString path = QString::number(channel) + "/initenal_resistance";
        if (1 != devPtr->SetValue(path.toStdString().c_str(), QString::number(enable).toStdString().c_str()))
            return false;
    }
    //        if(1 != ZCAN_SetResistanceEnable(m_dev,0,enable))
    //            return false;
    //        if(1 != ZCAN_SetResistanceEnable(m_dev,1,enable))
    //            return false;
    return true;
}

//6.设置滤波 可不设置
bool CANThread::setFilter(UINT filterMode,QString startID,QString endID)
{
    for (UINT channel = 0; channel < m_channelCount; ++channel) {
        const QString prefix = QString::number(channel) + "/";
        if(1 != devPtr->SetValue((prefix + "filter_clear").toStdString().c_str(), "0"))
            return false;
        if(1 != devPtr->SetValue((prefix + "filter_mode").toStdString().c_str(), QString::number(filterMode).toStdString().c_str()))
            return false;
        if(1 != devPtr->SetValue((prefix + "filter_start").toStdString().c_str(), startID.toStdString().c_str()))
            return false;
        if(1 != devPtr->SetValue((prefix + "filter_end").toStdString().c_str(), endID.toStdString().c_str()))
            return false;
        if(1 != devPtr->SetValue((prefix + "filter_ack").toStdString().c_str(), "0"))
            return false;
    }
    //如果要设置多条滤波，在清除滤波和滤波生效之间设置多条滤波即可
    //        if(1 != ZCAN_ClearFilter(m_channel1))
    //            return false;
    //        if(1 != ZCAN_ClearFilter(m_channel2))
    //            return false;
    //        if(1 != ZCAN_SetFilterMode(m_channel1,filterMode))
    //            return false;
    //        if(1 != ZCAN_SetFilterMode(m_channel2,filterMode))
    //            return false;
    //        if(1 != ZCAN_SetFilterStartID(m_channel1,startID.toUInt()))
    //            return false;
    //        if(1 != ZCAN_SetFilterEndID(m_channel1,endID.toUInt()))
    //            return false;
    //        if(1 != ZCAN_SetFilterStartID(m_channel2,startID.toUInt()))
    //            return false;
    //        if(1 != ZCAN_SetFilterEndID(m_channel2,endID.toUInt()))
    //            return false;
    //        if(1 != ZCAN_AckFilter(m_channel1))
    //            return false;
    //        if(1 != ZCAN_AckFilter(m_channel2))
    //            return false;
    //        //如果要设置多条滤波，在清除滤波和滤波生效之间设置多条滤波即可
    return true;
}

//7.启动CAN
bool CANThread::startCAN()
{
    // A QThread instance can be started again after closeDevice().
    // Clear the previous stop request before entering the receive loop.
    stopped = false;
    if(ZCAN_StartCAN(m_channel1) != 1)//初始化通道1
        return false;
    if (m_channelCount >= 2 && ZCAN_StartCAN(m_channel2) != 1)//初始化通道2
        return false;
    return true;
}

uint CANThread::MakeCanId(uint id, int eff, int rtr, int err)//1:extend frame 0:standard frame
{
    uint ueff = (uint)((eff != 0) ? 1 : 0);
    uint urtr = (uint)((rtr != 0) ? 1 : 0);
    uint uerr = (uint)((err != 0) ? 1 : 0);
    return id | ueff << 31 | urtr << 30 | uerr << 29;
}

//8-1.发送数据
bool CANThread::sendData(UINT id,UINT frame_type_index,UINT protocol_index,UINT canfd_exp_index,UINT channel,const char *data,UINT len)
{
    QMutexLocker locker(&m_channelMutex);
    if (channel >= m_channelCount || data == nullptr)
        return false;
    CHANNEL_HANDLE channelHandle = (channel == 0) ? m_channel1 : m_channel2;
    if (channelHandle == INVALID_CHANNEL_HANDLE)
        return false;
    UINT result; //发送的帧数
    if (0 == protocol_index) //can
    {
        ZCAN_Transmit_Data can_data;
        memset(&can_data, 0, sizeof(can_data));
        can_data.frame.can_id = MakeCanId(id, frame_type_index, 0, 0);
        memcpy(can_data.frame.data,data,len);
        can_data.frame.can_dlc = len > 8 ? 8 : len;
        can_data.transmit_type = 1;//单次发送
        result = ZCAN_Transmit(channelHandle,&can_data,1);
    }
    else //canfd
    {
        ZCAN_TransmitFD_Data canfd_data;
        memset(&canfd_data, 0, sizeof(canfd_data));
        canfd_data.frame.can_id = MakeCanId(id, frame_type_index, 0, 0);
        memcpy(canfd_data.frame.data,data,len);
        canfd_data.frame.len = len > 64 ? 64 : len;
        canfd_data.transmit_type = 1;//单次发送
        canfd_data.frame.flags = ((canfd_exp_index != 0) ? 1 : 0);
        result = ZCAN_TransmitFD(channelHandle,&canfd_data,1);
    }
    if (result != 1)
        return false;
    return true;
}

//9.关闭设备
void CANThread::closeDevice()
{
    // Stop the receive loop before touching channel/device handles.
    stop();
    if (isRunning())
        wait(1000);

    // ZCAN has no separate StopCAN API. Resetting an active channel brings
    // it out of started/bus state before the device handle is released.
    if (m_channel1 != INVALID_CHANNEL_HANDLE)
        ZCAN_ResetCAN(m_channel1);
    if (m_channelCount >= 2 && m_channel2 != INVALID_CHANNEL_HANDLE)
        ZCAN_ResetCAN(m_channel2);

    if (m_dev != INVALID_DEVICE_HANDLE)
        ZCAN_CloseDevice(m_dev);
    m_dev = INVALID_DEVICE_HANDLE;
    m_channel1 = INVALID_CHANNEL_HANDLE;
    m_channel2 = INVALID_CHANNEL_HANDLE;
}

//0.复位设备，  复位后回到4
bool CANThread::reSetCAN()
{
    if(ZCAN_ResetCAN(m_channel1) != 1)//复位通道1
        return false;
    if (m_channelCount >= 2 && ZCAN_ResetCAN(m_channel2) != 1)//复位通道2
        return false;
    return true;
}

void CANThread::run()
{
    UINT frameCount = 0;
    ZCAN_Receive_Data recvCANData[200];
    ZCAN_ReceiveFD_Data recvCANFDData[200];
    while(!stopped)
    {
        //CAN1
        frameCount = ZCAN_GetReceiveNum(m_channel1, TYPE_CAN);
        if(frameCount > 0)
        {
            int actualRead = ZCAN_Receive(m_channel1,recvCANData,(frameCount > 200 ? 200 : frameCount),50);
            if (actualRead > 0)
            {
                QVector<ZCAN_Receive_Data> vec(actualRead);
                memcpy(vec.data(), recvCANData, actualRead * sizeof(ZCAN_Receive_Data));
                emit recvedCANData(vec,actualRead,0); //触发信号
            }
        }
        frameCount = ZCAN_GetReceiveNum(m_channel1, TYPE_CANFD);
        if(frameCount > 0)
        {
            int actualRead = ZCAN_ReceiveFD(m_channel1,recvCANFDData,(frameCount > 200 ? 200 : frameCount),50);
            if (actualRead > 0)
            {
                QVector<ZCAN_ReceiveFD_Data> vec(actualRead);
                memcpy(vec.data(), recvCANFDData, actualRead * sizeof(ZCAN_ReceiveFD_Data));
                emit recvedCANFDData(vec,actualRead,0);
            }
        }
        if (m_channelCount >= 2) {
            //CAN2
            frameCount = ZCAN_GetReceiveNum(m_channel2, TYPE_CAN);
            if(frameCount > 0)
            {
                int actualRead = ZCAN_Receive(m_channel2,recvCANData,(frameCount > 200 ? 200 : frameCount),50);
                if (actualRead > 0)
                {
                    QVector<ZCAN_Receive_Data> vec(actualRead);
                    memcpy(vec.data(), recvCANData, actualRead * sizeof(ZCAN_Receive_Data));
                    emit recvedCANData(vec,actualRead,1);
                }
            }
            frameCount = ZCAN_GetReceiveNum(m_channel2, TYPE_CANFD);
            if(frameCount > 0)
            {
                int actualRead = ZCAN_ReceiveFD(m_channel2,recvCANFDData,(frameCount > 200 ? 200 : frameCount),50);
                if (actualRead > 0)
                {
                    QVector<ZCAN_ReceiveFD_Data> vec(actualRead);
                    memcpy(vec.data(), recvCANFDData, actualRead * sizeof(ZCAN_ReceiveFD_Data));
                    emit recvedCANFDData(vec,actualRead,1);
                }
            }
        }
        msleep(5);
    }
}

void CANThread::sleep(unsigned int msec)
{
    msleep(msec);
}
