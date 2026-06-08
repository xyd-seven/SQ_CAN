#ifndef IMUPARSER_H
#define IMUPARSER_H

#include <QString>
#include <QMutex>
#include <QByteArray>
#include <QList>
#include <QQueue>

// 姿态传感器解析数据结构
struct IMUData {
    // 1. 加速度 (IMUa: 0x18EE00D0)
    double ax; // X轴加速度 (g)
    double ay; // Y轴加速度 (g)
    double az; // Z轴加速度 (g)

    // 2. 角速度 (IMUas: 0x18EE01D0)
    double gx; // X轴角速度 (°/s)
    double gy; // Y轴角速度 (°/s)
    double gz; // Z轴角速度 (°/s)

    // 3. 角度 (IMUang: 0x18EE02D0)
    double roll;  // X轴角度 (°)
    double pitch; // Y轴角度 (°)
    double yaw;   // Z轴角度 (°)

    // 4. 故障反馈 (IMUAF: 0x18EE04D0)
    unsigned char faultBytes[7]; // 7字节系统故障状态 (共56个故障位)
    unsigned char faultLevel;     // 故障等级 (0:完全正常, 1:轻微故障, 2:一般故障, 3:严重故障)

    // 5. 软件版本 (IMUVer: 0x18FF90D0)
    QString softwareVersion;      // 格式：Vx.yy.zz
    QString releaseDate;          // 格式：YYYY-MM-DD
    int platformType;             // 车型平台
    int internalVersion;          // 内部版本号
    int versionType;              // 版本类型
    int platformVarNo;            // 平台变体号

    IMUData() {
        ax = ay = az = 0.0;
        gx = gy = gz = 0.0;
        roll = pitch = yaw = 0.0;
        for (int i = 0; i < 7; ++i) {
            faultBytes[i] = 0;
        }
        faultLevel = 0;
        softwareVersion = "V0.00.00";
        releaseDate = "2000-01-01";
        platformType = 0;
        internalVersion = 0;
        versionType = 0;
        platformVarNo = 0;
    }
};

// 原始透传数据结构 (96字节)
struct IMURawData {
    unsigned int pcNum;
    unsigned char sysStatus;
    unsigned char rtState;
    unsigned char overRangeStatus;
    unsigned char cmdAck;
    unsigned short totalAlignTime;
    unsigned short remainAlignTime;

    double gx; // deg/s
    double gy; // deg/s
    double gz; // deg/s
    double ax; // g
    double ay; // g
    double az; // g

    double longitude;
    double latitude;
    double altitude;

    double velNorth;
    double velUp;
    double velEast;

    double roll;
    double yaw;
    double pitch;

    char gyroTemp;
    char accelTemp;

    float q[4];

    unsigned char imuOverRangeCounters[6];

    double reverseRoll;
    double reverseYaw;
    double reversePitch;

    unsigned char verticalFlag;
    unsigned char outputFormat;

    IMURawData() {
        pcNum = 0;
        sysStatus = rtState = overRangeStatus = cmdAck = 0;
        totalAlignTime = remainAlignTime = 0;
        gx = gy = gz = 0.0;
        ax = ay = az = 0.0;
        longitude = latitude = altitude = 0.0;
        velNorth = velUp = velEast = 0.0;
        roll = yaw = pitch = 0.0;
        gyroTemp = accelTemp = 0;
        q[0] = q[1] = q[2] = q[3] = 0.0f;
        for (int i = 0; i < 6; ++i) imuOverRangeCounters[i] = 0;
        reverseRoll = reverseYaw = reversePitch = 0.0;
        verticalFlag = outputFormat = 0;
    }
};

struct IMURawPacket {
    QString receiveTimeText;
    QString hexText;
    QByteArray rawBytes;
    IMURawData decodedData;
};

class IMUParser
{
public:
    static IMUParser* getInstance();

    // 解析收到的 CAN 帧
    void parseCANFrame(unsigned int id, const unsigned char* data, unsigned int len, const QString &receiveTimeText = QString());

    // 获取当前最新数据
    IMUData getIMUData();

    // 获取当前最新原始透传数据
    IMURawData getIMURawData();
    
    // 获取最新的 Hex 数据串 (如果有新包，返回 true)
    bool getIMURawHex(QString &hexStr);
    bool getIMURawHex(QString &hexStr, QString &receiveTimeText);
    QList<IMURawPacket> takeIMURawPackets();
    QList<IMURawPacket> takeIMURawPackets(int maxCount);
    
    // 获取拼包校验统计
    void getIMURawStats(unsigned int &total, unsigned int &success, unsigned int &failed);
    void getIMURawStats(unsigned int &total, unsigned int &success, unsigned int &failed, unsigned int &dropped);

    // 获取当前最新原始二进制数据块
    QByteArray getIMURawBytes();

private:
    IMUParser();
    static IMUParser* instance;
    static QMutex mutex;

    IMUData m_data;
    QMutex m_dataMutex; // 保护共享数据

    // 原始透传相关成员
    IMURawData m_rawData;
    QMutex m_rawDataMutex;
    unsigned char m_rawBuffer[96];
    unsigned int m_rawReceivedMask;
    unsigned int m_totalRawPackets;
    unsigned int m_successRawPackets;
    unsigned int m_failedRawPackets;
    unsigned int m_droppedRawPackets;
    QQueue<IMURawPacket> m_rawPacketQueue;
    QString m_lastRawHexStr;
    QString m_lastRawTimeText;
    bool m_newRawHexFlag;

    static const int MAX_RAW_PACKET_QUEUE_SIZE = 1000;
};

#endif // IMUPARSER_H
