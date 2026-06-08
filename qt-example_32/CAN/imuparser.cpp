#include "imuparser.h"
#include <QDebug>

IMUParser* IMUParser::instance = nullptr;
QMutex IMUParser::mutex;

IMUParser::IMUParser()
{
    m_rawReceivedMask = 0;
    m_totalRawPackets = 0;
    m_successRawPackets = 0;
    m_failedRawPackets = 0;
    m_droppedRawPackets = 0;
    m_newRawHexFlag = false;
    memset(m_rawBuffer, 0, 96);
}

IMUParser* IMUParser::getInstance()
{
    QMutexLocker locker(&mutex);
    if (instance == nullptr) {
        instance = new IMUParser();
    }
    return instance;
}

IMUData IMUParser::getIMUData()
{
    QMutexLocker locker(&m_dataMutex);
    return m_data;
}

void IMUParser::parseCANFrame(unsigned int id, const unsigned char* data, unsigned int len, const QString &receiveTimeText)
{
    if (len < 8) return; // 姿态传感器报文 DLC 均为 8 字节

    // 1. 原始数据透传拼包逻辑 (CAN ID 0x01 到 0x0C)
    if (id >= 0x01 && id <= 0x0C) {
        QMutexLocker rawLocker(&m_rawDataMutex);
        
        // 收到已存在于掩码中的 ID 时，认为上一包数据已经结束，重置接收掩码，作为新包的起始点
        if (m_rawReceivedMask & (1 << (id - 1))) {
            m_rawReceivedMask = 0;
        }
        
        int offset = (id - 1) * 8;
        memcpy(m_rawBuffer + offset, data, 8);
        m_rawReceivedMask |= (1 << (id - 1));
        
        // 集齐 12 帧后进行整包解包与校验
        if (m_rawReceivedMask == 0x0FFF) {
            m_rawReceivedMask = 0; // 清除掩码以准备接收下一包
            m_totalRawPackets++;
            
            // 校验帧头 0x55 0xAA 和数据长度 0x5C (92)
            if (m_rawBuffer[0] == 0x55 && m_rawBuffer[1] == 0xAA && m_rawBuffer[2] == 0x5C) {
                unsigned char sum = 0;
                for (int i = 2; i < 95; ++i) {
                    sum += m_rawBuffer[i];
                }
                
                if (sum == m_rawBuffer[95]) {
                    m_successRawPackets++;
                    
                    // 读取 16/32 位 LE 辅助 lambda
                    auto readInt32LE = [](const unsigned char* buf, int off) -> int {
                        return (int)(buf[off] | (buf[off+1] << 8) | (buf[off+2] << 16) | ((unsigned int)buf[off+3] << 24));
                    };
                    auto readInt16LE = [](const unsigned char* buf, int off) -> short {
                        return (short)(buf[off] | (buf[off+1] << 8));
                    };
                    auto readUint16LE = [](const unsigned char* buf, int off) -> unsigned short {
                        return (unsigned short)(buf[off] | (buf[off+1] << 8));
                    };
                    
                    m_rawData.pcNum = (unsigned int)(m_rawBuffer[3] | (m_rawBuffer[4] << 8) | (m_rawBuffer[5] << 16) | (m_rawBuffer[6] << 24));
                    m_rawData.sysStatus = m_rawBuffer[7];
                    m_rawData.rtState = m_rawBuffer[8];
                    m_rawData.overRangeStatus = m_rawBuffer[9];
                    m_rawData.cmdAck = m_rawBuffer[10];
                    m_rawData.totalAlignTime = readUint16LE(m_rawBuffer, 11);
                    m_rawData.remainAlignTime = readUint16LE(m_rawBuffer, 13);
                    
                    m_rawData.gx = readInt32LE(m_rawBuffer, 15) * 0.0000005;
                    m_rawData.gy = readInt32LE(m_rawBuffer, 19) * 0.0000005;
                    m_rawData.gz = readInt32LE(m_rawBuffer, 23) * 0.0000005;
                    
                    m_rawData.ax = readInt32LE(m_rawBuffer, 27) * 0.000001;
                    m_rawData.ay = readInt32LE(m_rawBuffer, 31) * 0.000001;
                    m_rawData.az = readInt32LE(m_rawBuffer, 35) * 0.000001;
                    
                    m_rawData.longitude = readInt32LE(m_rawBuffer, 39) * 0.0000001;
                    m_rawData.latitude = readInt32LE(m_rawBuffer, 43) * 0.0000001;
                    m_rawData.altitude = readInt16LE(m_rawBuffer, 47) * 0.5;
                    
                    m_rawData.velNorth = readInt16LE(m_rawBuffer, 49) * 0.04;
                    m_rawData.velUp = readInt16LE(m_rawBuffer, 51) * 0.04;
                    m_rawData.velEast = readInt16LE(m_rawBuffer, 53) * 0.04;
                    
                    m_rawData.roll = readInt16LE(m_rawBuffer, 55) * 0.006;
                    m_rawData.yaw = readUint16LE(m_rawBuffer, 57) * 0.006;
                    m_rawData.pitch = readInt16LE(m_rawBuffer, 59) * 0.006;
                    
                    m_rawData.gyroTemp = (char)m_rawBuffer[61];
                    m_rawData.accelTemp = (char)m_rawBuffer[62];
                    
                    // 浮点四元数解析
                    memcpy(&m_rawData.q[0], m_rawBuffer + 63, 4);
                    memcpy(&m_rawData.q[1], m_rawBuffer + 67, 4);
                    memcpy(&m_rawData.q[2], m_rawBuffer + 71, 4);
                    memcpy(&m_rawData.q[3], m_rawBuffer + 75, 4);
                    
                    for (int i = 0; i < 6; ++i) {
                        m_rawData.imuOverRangeCounters[i] = m_rawBuffer[79 + i];
                    }
                    
                    m_rawData.reverseRoll = readInt16LE(m_rawBuffer, 87) * 0.006;
                    m_rawData.reverseYaw = readUint16LE(m_rawBuffer, 89) * 0.006;
                    m_rawData.reversePitch = readInt16LE(m_rawBuffer, 91) * 0.006;
                    
                    m_rawData.verticalFlag = m_rawBuffer[93];
                    m_rawData.outputFormat = m_rawBuffer[94];
                    
                    // 生成 96 字节 HEX 字符串 (使用 QByteArray 一次性生成，极大提升性能)
                    QByteArray rawDataBytes = QByteArray::fromRawData((const char*)m_rawBuffer, 96);
                    m_lastRawHexStr = QString::fromLatin1(rawDataBytes.toHex(' ').toUpper());
                    m_lastRawTimeText = receiveTimeText;
                    m_newRawHexFlag = true;
                    IMURawPacket packet;
                    packet.receiveTimeText = receiveTimeText;
                    packet.hexText = m_lastRawHexStr;
                    packet.rawBytes = QByteArray((const char*)m_rawBuffer, 96);
                    packet.decodedData = m_rawData;
                    if (m_rawPacketQueue.size() >= MAX_RAW_PACKET_QUEUE_SIZE) {
                        m_rawPacketQueue.dequeue();
                        m_droppedRawPackets++;
                    }
                    m_rawPacketQueue.enqueue(packet);
                } else {
                    m_failedRawPackets++;
                }
            } else {
                m_failedRawPackets++;
            }
        }
        return; // 处理完透传帧直接返回，不走下面的应用报文解析
    }

    QMutexLocker locker(&m_dataMutex);

    switch (id) {
    case 0x18EE00D0: { // IMUa: IMU加速度信息
        unsigned short rawX = (unsigned short)(data[0] | (data[1] << 8));
        unsigned short rawY = (unsigned short)(data[2] | (data[3] << 8));
        unsigned short rawZ = (unsigned short)(data[4] | (data[5] << 8));

        m_data.ax = rawX * 0.0005 - 16.0;
        m_data.ay = rawY * 0.0005 - 16.0;
        m_data.az = rawZ * 0.0005 - 16.0;
        break;
    }
    case 0x18EE01D0: { // IMUas: IMU角速度信息
        unsigned short rawX = (unsigned short)(data[0] | (data[1] << 8));
        unsigned short rawY = (unsigned short)(data[2] | (data[3] << 8));
        unsigned short rawZ = (unsigned short)(data[4] | (data[5] << 8));

        m_data.gx = rawX * 0.0625 - 2000.0;
        m_data.gy = rawY * 0.0625 - 2000.0;
        m_data.gz = rawZ * 0.0625 - 2000.0;
        break;
    }
    case 0x18EE02D0: { // IMUang: IMU角度信息
        unsigned short rawX = (unsigned short)(data[0] | (data[1] << 8));
        unsigned short rawY = (unsigned short)(data[2] | (data[3] << 8));
        unsigned short rawZ = (unsigned short)(data[4] | (data[5] << 8));

        m_data.roll  = rawX * 0.00625 - 200.0;
        m_data.pitch = rawY * 0.00625 - 200.0;
        m_data.yaw   = rawZ * 0.00625 - 200.0;
        break;
    }
    case 0x18EE04D0: { // IMUAF: IMU故障反馈报文
        for (int i = 0; i < 7; ++i) {
            m_data.faultBytes[i] = data[i];
        }
        m_data.faultLevel = data[7] & 0x03;
        break;
    }
    case 0x18FF90D0: { // IMUVer: IMU软件版本信息
        unsigned char major = data[2] & 0x0F;
        unsigned char minor = data[3];
        unsigned char small = data[4];
        m_data.softwareVersion = QString("V%1.%2.%3")
                                 .arg(major)
                                 .arg(QString::number(minor).rightJustified(2, '0'))
                                 .arg(QString::number(small).rightJustified(2, '0'));

        unsigned int year = data[5] + 2000;
        unsigned char month = data[6] & 0x0F;
        unsigned char day = data[7] & 0x1F;
        m_data.releaseDate = QString("%1-%2-%3")
                             .arg(year)
                             .arg(QString::number(month).rightJustified(2, '0'))
                             .arg(QString::number(day).rightJustified(2, '0'));

        m_data.platformType = (data[2] >> 4) & 0x0F;
        m_data.internalVersion = data[0] & 0x0F;
        m_data.versionType = (data[0] >> 4) & 0x0F;
        m_data.platformVarNo = data[1];
        break;
    }
    default:
        qDebug() << "Received unknown CAN ID for IMU:" << QString::number(id, 16);
        break;
    }
}

IMURawData IMUParser::getIMURawData()
{
    QMutexLocker locker(&m_rawDataMutex);
    return m_rawData;
}

bool IMUParser::getIMURawHex(QString &hexStr)
{
    QMutexLocker locker(&m_rawDataMutex);
    if (m_newRawHexFlag) {
        hexStr = m_lastRawHexStr;
        m_newRawHexFlag = false;
        return true;
    }
    return false;
}

bool IMUParser::getIMURawHex(QString &hexStr, QString &receiveTimeText)
{
    QMutexLocker locker(&m_rawDataMutex);
    if (m_newRawHexFlag) {
        hexStr = m_lastRawHexStr;
        receiveTimeText = m_lastRawTimeText;
        m_newRawHexFlag = false;
        return true;
    }
    return false;
}

QList<IMURawPacket> IMUParser::takeIMURawPackets()
{
    return takeIMURawPackets(-1);
}

QList<IMURawPacket> IMUParser::takeIMURawPackets(int maxCount)
{
    QMutexLocker locker(&m_rawDataMutex);
    QList<IMURawPacket> packets;
    while (!m_rawPacketQueue.isEmpty() && (maxCount < 0 || packets.size() < maxCount)) {
        packets.append(m_rawPacketQueue.dequeue());
    }
    if (m_rawPacketQueue.isEmpty()) {
        m_newRawHexFlag = false;
    }
    return packets;
}

void IMUParser::getIMURawStats(unsigned int &total, unsigned int &success, unsigned int &failed)
{
    QMutexLocker locker(&m_rawDataMutex);
    total = m_totalRawPackets;
    success = m_successRawPackets;
    failed = m_failedRawPackets;
}

void IMUParser::getIMURawStats(unsigned int &total, unsigned int &success, unsigned int &failed, unsigned int &dropped)
{
    QMutexLocker locker(&m_rawDataMutex);
    total = m_totalRawPackets;
    success = m_successRawPackets;
    failed = m_failedRawPackets;
    dropped = m_droppedRawPackets;
}

QByteArray IMUParser::getIMURawBytes()
{
    QMutexLocker locker(&m_rawDataMutex);
    return QByteArray((const char*)m_rawBuffer, 96);
}

