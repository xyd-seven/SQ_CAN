#include "canthread.h"
#include <QTime>
#include <QCoreApplication>
#include <QMetaType>
#include <string.h>

CANThread::CANThread()
{
    stopped = false;
    m_dev = INVALID_DEVICE_HANDLE;
    m_channel1 = INVALID_CHANNEL_HANDLE;
    m_channel2 = INVALID_CHANNEL_HANDLE;
    devPtr = nullptr;
    memset(&m_config,0,sizeof(m_config));
}

void CANThread::stop()
{
    stopped = true;
}

//1.打开设备
bool CANThread::openDevice(UINT device_type, UINT device_index, UINT reserved)
{
    if(INVALID_DEVICE_HANDLE != m_dev)//设备已打开
        return false;
    m_dev = ZCAN_OpenDevice(device_type,device_index,reserved);
    if(INVALID_DEVICE_HANDLE == m_dev)
        return false;
    devPtr = GetIProperty(m_dev);
    if(devPtr == nullptr)//设置指定路径属性失败
    {
        ZCAN_CloseDevice(m_dev);
        return false;
    }
    return true;
}

//2.设置CANFD标准
bool CANThread::setCANFDStandard(UINT standard)
{
    if(1 != devPtr->SetValue("0/canfd_standard",QString::number(standard).toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("1/canfd_standard",QString::number(standard).toStdString().c_str()))
        return false;
    //       if(1 != ZCAN_SetCANFDStandard(m_dev,0,standard))
    //           return false;
    //       if(1 != ZCAN_SetCANFDStandard(m_dev,1,standard))
    //           return false;
    return true;
}

//3.设置波特率
bool CANThread::setBaudrateFD(UINT rate1,UINT rate2)
{
    if(1 != devPtr->SetValue("0/canfd_abit_baud_rate",QString::number(rate1).toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("1/canfd_abit_baud_rate",QString::number(rate1).toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("0/canfd_dbit_baud_rate",QString::number(rate2).toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("1/canfd_dbit_baud_rate",QString::number(rate2).toStdString().c_str()))
        return false;
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
    if(1 != devPtr->SetValue("0/baud_rate_custom",rate.toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("1/baud_rate_custom",rate.toStdString().c_str()))
        return false;
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
    m_channel2 = ZCAN_InitCAN(m_dev,1,&m_config);
    if(nullptr == m_channel1 || nullptr == m_channel2)
        return false;
    return true;
}

//5.使能终端电阻
bool CANThread::setResistanceEnable(UINT enable)
{
    if(1 != devPtr->SetValue("0/initenal_resistance",QString::number(enable).toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("1/initenal_resistance",QString::number(enable).toStdString().c_str()))
        return false;
    //        if(1 != ZCAN_SetResistanceEnable(m_dev,0,enable))
    //            return false;
    //        if(1 != ZCAN_SetResistanceEnable(m_dev,1,enable))
    //            return false;
    return true;
}

//6.设置滤波 可不设置
bool CANThread::setFilter(UINT filterMode,QString startID,QString endID)
{
    if(1 != devPtr->SetValue("0/filter_clear","0"))//清除滤波
        return false;
    if(1 != devPtr->SetValue("1/filter_clear","0"))
        return false;
    if(1 != devPtr->SetValue("0/filter_mode",QString::number(filterMode).toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("1/filter_mode",QString::number(filterMode).toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("0/filter_start",startID.toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("0/filter_end",endID.toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("1/filter_start",startID.toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("1/filter_end",endID.toStdString().c_str()))
        return false;
    if(1 != devPtr->SetValue("0/filter_ack","0"))//滤波生效
        return false;
    if(1 != devPtr->SetValue("1/filter_ack","0"))
        return false;
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
    if(ZCAN_StartCAN(m_channel1) != 1)//初始化通道1
        return false;
    if(ZCAN_StartCAN(m_channel2) != 1)//初始化通道2
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
    UINT result; //发送的帧数
    if (0 == protocol_index) //can
    {
        ZCAN_Transmit_Data can_data;
        memset(&can_data, 0, sizeof(can_data));
        can_data.frame.can_id = MakeCanId(id, frame_type_index, 0, 0);
        memcpy(can_data.frame.data,data,len);
        can_data.frame.can_dlc = len > 8 ? 8 : len;
        can_data.transmit_type = 1;//单次发送
        result = ZCAN_Transmit(((channel == 0) ? m_channel1 : m_channel2),&can_data,1);
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
        result = ZCAN_TransmitFD(((channel == 0) ? m_channel1 : m_channel2),&canfd_data,1);
    }
    if (result != 1)
        return false;
    return true;
}

//9.关闭设备
void CANThread::closeDevice()
{
    ZCAN_CloseDevice(m_dev);
    m_dev = INVALID_DEVICE_HANDLE;
}

//0.复位设备，  复位后回到4
bool CANThread::reSetCAN()
{
    if(ZCAN_ResetCAN(m_channel1) != 1)//复位通道1
        return false;
    if(ZCAN_ResetCAN(m_channel2) != 1)//复位通道2
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
        msleep(5);
    }
}

void CANThread::sleep(unsigned int msec)
{
    msleep(msec);
}
