#ifndef UDSCLIENT_H
#define UDSCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <QVector>
#include "canthread.h"

class UdsClient : public QObject
{
    Q_OBJECT
public:
    struct UpgradeConfig {
        uint8_t seedSubFunction = 0x01;
        uint8_t keySubFunction = 0x02;
        uint8_t dataFormatIdentifier = 0x00;
        uint8_t addressAndLengthFormat = 0x44;
        int defaultBlockSize = 256;
        bool useEcuBlockSize = true;
        uint16_t checksumRoutineId = 0x0202;
        bool appendCrc32 = true;
        bool crcBigEndian = true;
        uint8_t resetType = 0x01;
    };

    explicit UdsClient(QObject *parent = nullptr);
    ~UdsClient();

    // 配置接口
    void setCanThread(CANThread *thread);
    void setConfig(uint32_t requestID, uint32_t responseID, bool isExtended, int channel, int protocol);
    
    uint32_t requestID() const { return m_requestID; }
    uint32_t responseID() const { return m_responseID; }
    bool isRunning() const { return m_canThread != nullptr; }

    // 主动控制接口
    bool sendUdsRequest(uint8_t serviceId, const QByteArray &payload);
    void setTesterPresentEnabled(bool enable, int intervalMs = 2000);
    
    // 2E 写入接口
    bool writeDataByIdentifier(uint16_t did, const QByteArray &data);
    
    // 固件升级接口
    void setUpgradeConfig(const UpgradeConfig &config);
    bool startUpgrade(const QByteArray &firmwareData, uint32_t startAddress);
    void abortUpgrade();

    // 接收底层 CAN 报文分发入口
    void handleIncomingFrame(uint32_t id, const QByteArray &frameData);

signals:
    // UDS 响应信号
    void udsResponseReceived(uint8_t serviceId, bool isPositive, const QByteArray &payload, uint8_t nrc);
    void udsResponseTimeout();
    
    // 升级过程中的状态及进度信号
    void upgradeStateChanged(const QString &stateName);
    void upgradeProgress(int currentBytes, int totalBytes, int percent);
    void upgradeCompleted(bool success, const QString &errorMsg = "");
    
    // 日志信号
    void logMessage(const QString &msg, int type); // type: 0-Info, 1-TX, 2-RX Positive, 3-RX Negative/Error

private slots:
    void onTesterPresentTimeout();
    void onRxTimeout();
    void onTxCfTimerTimeout();
    void onUdsResponseTimeout();
    void onUdsResponseReceivedSlot(uint8_t serviceId, bool isPositive, const QByteArray &payload, uint8_t nrc);
    void onFcWaitTimeout();

private:
    // ISO-TP 发送辅助方法
    bool transmitIsoTpMessage(const QByteArray &message);
    bool sendSingleFrame(const QByteArray &payload);
    bool sendFirstFrame(const QByteArray &payload);
    bool sendConsecutiveFrame();
    bool sendFlowControl(uint8_t flowStatus, uint8_t blockSize, uint8_t stMin);

    // ISO-TP 接收辅助方法
    void processRxBuffer();

    // UDS 响应处理
    void handleUdsResponse(const QByteArray &udsPayload);
    QString serviceName(uint8_t serviceId) const;
    QString nrcDescription(uint8_t nrc) const;
    QString describeUdsPayload(const QByteArray &udsPayload) const;

    // 固件升级状态机控制
    enum UpgradeState {
        UPG_IDLE,
        UPG_ENTER_EXTENDED,      // 0x10 03
        UPG_SECURITY_SEED,       // 0x27 01
        UPG_SECURITY_KEY,        // 0x27 02
        UPG_ENTER_PROGRAMMING,   // 0x10 02
        UPG_REQUEST_DOWNLOAD,    // 0x34
        UPG_TRANSFER_DATA,       // 0x36
        UPG_EXIT_TRANSFER,       // 0x37
        UPG_CHECKSUM_VERIFY,     // 0x31
        UPG_ECU_RESET,           // 0x11 01
        UPG_COMPLETED
    };
    void runUpgradeStateMachine();
    void transitionUpgradeState(UpgradeState newState);
    uint32_t calculateKey(uint32_t seed);
    void resetTransportState(bool clearUdsWait);
    int getMaxFrameSize() const;
    uint32_t calculateCrc32(const QByteArray &data) const;

    // 成员变量
    CANThread *m_canThread;
    uint32_t m_requestID;
    uint32_t m_responseID;
    bool m_isExtended;
    int m_channel;
    int m_protocol; // 0-CAN, 1-CANFD

    // Tester Present 定时器
    QTimer *m_testerPresentTimer;
    bool m_testerPresentEnabled;
    int m_testerPresentInterval;

    // ISO-TP 接收状态
    enum RxState {
        RX_IDLE,
        RX_WAIT_CF
    };
    RxState m_rxState;
    QByteArray m_rxBuffer;
    int m_rxTotalLen;
    uint8_t m_rxExpectedSn;
    QTimer *m_rxTimer;

    // ISO-TP 发送状态
    enum TxState {
        TX_IDLE,
        TX_WAIT_FC,
        TX_SENDING_CF
    };
    TxState m_txState;
    QByteArray m_txBuffer;
    int m_txIndex;
    uint8_t m_txExpectedSn;
    uint8_t m_fcBs;       // 块大小 (Block Size)
    uint8_t m_fcStMin;    // 最小间隔 (STmin)
    int m_cfSentInBlock;  // 当前块中已发送的连续帧数
    QTimer *m_txCfTimer;
    QTimer *m_fcWaitTimer;

    // UDS 请求/响应状态
    bool m_waitingForUdsResponse;
    uint8_t m_waitingServiceId;
    QTimer *m_udsTimeoutTimer;

    // 固件升级变量
    bool m_upgradeRunning;
    UpgradeState m_upgradeState;
    QByteArray m_firmwareData;
    uint32_t m_startAddress;
    int m_firmwareSentBytes;
    int m_upgradeBlockCounter;
    int m_upgradeTotalBlocks;
    int m_upgradeBlockSize; // 刷写每块大小，协商而来或默认
    UpgradeConfig m_upgradeConfig;
};

#endif // UDSCLIENT_H
