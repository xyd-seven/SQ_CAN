#include "udsclient.h"
#include <QDebug>
#include <QTime>

UdsClient::UdsClient(QObject *parent)
    : QObject(parent),
      m_canThread(nullptr),
      m_requestID(0x7E0),
      m_responseID(0x7E8),
      m_isExtended(false),
      m_channel(0),
      m_protocol(0),
      m_testerPresentEnabled(false),
      m_testerPresentInterval(2000),
      m_rxState(RX_IDLE),
      m_rxTotalLen(0),
      m_rxExpectedSn(0),
      m_txState(TX_IDLE),
      m_txIndex(0),
      m_txExpectedSn(0),
      m_fcBs(0),
      m_fcStMin(0),
      m_cfSentInBlock(0),
      m_waitingForUdsResponse(false),
      m_waitingServiceId(0),
      m_upgradeRunning(false),
      m_upgradeState(UPG_IDLE),
      m_firmwareSentBytes(0),
      m_upgradeBlockCounter(0),
      m_upgradeTotalBlocks(0),
      m_upgradeBlockSize(128)
{
    // Tester Present 定时器
    m_testerPresentTimer = new QTimer(this);
    connect(m_testerPresentTimer, &QTimer::timeout, this, &UdsClient::onTesterPresentTimeout);

    // ISO-TP 接收超时定时器
    m_rxTimer = new QTimer(this);
    m_rxTimer->setSingleShot(true);
    connect(m_rxTimer, &QTimer::timeout, this, &UdsClient::onRxTimeout);

    // ISO-TP 发送连续帧定时器
    m_txCfTimer = new QTimer(this);
    m_txCfTimer->setSingleShot(true);
    connect(m_txCfTimer, &QTimer::timeout, this, &UdsClient::onTxCfTimerTimeout);

    // UDS 响应超时定时器
    m_udsTimeoutTimer = new QTimer(this);
    m_udsTimeoutTimer->setSingleShot(true);
    connect(m_udsTimeoutTimer, &QTimer::timeout, this, &UdsClient::onUdsResponseTimeout);

    // UDS 响应接收自驱动槽连接
    connect(this, &UdsClient::udsResponseReceived, this, &UdsClient::onUdsResponseReceivedSlot);

    // ISO-TP 等待流控帧超时定时器
    m_fcWaitTimer = new QTimer(this);
    m_fcWaitTimer->setSingleShot(true);
    connect(m_fcWaitTimer, &QTimer::timeout, this, &UdsClient::onFcWaitTimeout);
}

UdsClient::~UdsClient()
{
    m_testerPresentTimer->stop();
    m_rxTimer->stop();
    m_txCfTimer->stop();
    m_udsTimeoutTimer->stop();
    m_fcWaitTimer->stop();
}

void UdsClient::setCanThread(CANThread *thread)
{
    m_canThread = thread;
}

void UdsClient::setConfig(uint32_t requestID, uint32_t responseID, bool isExtended, int channel, int protocol)
{
    m_requestID = requestID;
    m_responseID = responseID;
    m_isExtended = isExtended;
    m_channel = channel;
    m_protocol = protocol;
    
    emit logMessage(QString("UDS 配置更新: 请求ID=0x%1, 响应ID=0x%2, 扩展帧=%3, 通道=%4, 协议=%5")
                    .arg(m_requestID, 0, 16).arg(m_responseID, 0, 16)
                    .arg(m_isExtended ? "是" : "否").arg(m_channel + 1)
                    .arg(m_protocol == 1 ? "CAN-FD" : "CAN"), 0);
}

// 主动发送 UDS 请求
bool UdsClient::sendUdsRequest(uint8_t serviceId, const QByteArray &payload)
{
    if (!m_canThread) {
        emit logMessage("错误: CAN 线程未启动，无法发送 UDS 请求！", 3);
        return false;
    }

    QByteArray message;
    message.append(static_cast<char>(serviceId));
    message.append(payload);

    m_waitingForUdsResponse = true;
    m_waitingServiceId = serviceId;
    m_udsTimeoutTimer->start(6000); // UDS 标准超时，设定为 6秒

    // 格式化输出发送日志
    QString hexStr;
    for (int i = 0; i < message.size(); ++i) {
        hexStr += QString("%1 ").arg(static_cast<uint8_t>(message.at(i)), 2, 16, QChar('0')).toUpper();
    }
    emit logMessage(QString("TX -> 0x%1: %2").arg(m_requestID, 0, 16).arg(hexStr.trimmed()), 1);

    transmitIsoTpMessage(message);
    return true;
}

// Tester Present 心跳控制
void UdsClient::setTesterPresentEnabled(bool enable, int intervalMs)
{
    m_testerPresentEnabled = enable;
    m_testerPresentInterval = intervalMs;
    
    if (m_testerPresentEnabled) {
        m_testerPresentTimer->start(m_testerPresentInterval);
        emit logMessage(QString("启用在线保持 (0x3E)，发送间隔: %1 ms").arg(m_testerPresentInterval), 0);
    } else {
        m_testerPresentTimer->stop();
        emit logMessage("禁用在线保持 (0x3E)", 0);
    }
}

// 2E 写入接口
bool UdsClient::writeDataByIdentifier(uint16_t did, const QByteArray &data)
{
    QByteArray payload;
    payload.append(static_cast<char>((did >> 8) & 0xFF));
    payload.append(static_cast<char>(did & 0xFF));
    payload.append(data);
    return sendUdsRequest(0x2E, payload);
}

// 接收底层数据帧入口
void UdsClient::handleIncomingFrame(uint32_t id, const QByteArray &frameData)
{
    Q_UNUSED(id);
    if (frameData.isEmpty()) return;

    uint8_t pci = frameData.at(0) & 0xF0;

    // 1. 单帧 (SF)
    if (pci == 0x00) {
        int len = 0;
        int pciLen = frameData.at(0) & 0x0F;
        int payloadOffset = 1;
        if (pciLen == 0) {
            if (frameData.size() >= 2) {
                len = static_cast<uint8_t>(frameData.at(1));
                payloadOffset = 2;
            }
        } else {
            len = pciLen;
        }
        if (len > 0 && len <= frameData.size() - payloadOffset) {
            QByteArray udsPayload = frameData.mid(payloadOffset, len);
            m_rxState = RX_IDLE;
            m_rxTimer->stop();
            handleUdsResponse(udsPayload);
        }
    }
    // 2. 首帧 (FF)
    else if (pci == 0x10) {
        if (frameData.size() < 2) {
            emit logMessage("ISO-TP 错误: 接收首帧长度小于 2 字节", 3);
            return;
        }
        int len = ((frameData.at(0) & 0x0F) << 8) | static_cast<uint8_t>(frameData.at(1));
        m_rxBuffer = frameData.mid(2);
        m_rxTotalLen = len;
        m_rxExpectedSn = 1;
        m_rxState = RX_WAIT_CF;
        m_rxTimer->start(1500); // 连续帧等待超时 1.5s
        
        // 发送流控帧 (CTS, BS=0, STmin=0)
        sendFlowControl(0, 0, 0);
    }
    // 3. 连续帧 (CF)
    else if (pci == 0x20) {
        if (m_rxState != RX_WAIT_CF) return;
        if (frameData.size() < 1) return;

        uint8_t sn = frameData.at(0) & 0x0F;
        if (sn != m_rxExpectedSn) {
            emit logMessage(QString("ISO-TP 错误: 连续帧序号不匹配 (期望 %1, 收到 %2)").arg(m_rxExpectedSn).arg(sn), 3);
            m_rxState = RX_IDLE;
            m_rxTimer->stop();
            return;
        }

        int maxCfPayload = frameData.size() - 1;
        int bytesToCopy = qMin(maxCfPayload, m_rxTotalLen - m_rxBuffer.size());
        m_rxBuffer.append(frameData.mid(1, bytesToCopy));
        m_rxExpectedSn = (m_rxExpectedSn + 1) & 0x0F;

        if (m_rxBuffer.size() >= m_rxTotalLen) {
            m_rxState = RX_IDLE;
            m_rxTimer->stop();
            handleUdsResponse(m_rxBuffer);
        } else {
            m_rxTimer->start(1500); // 重启超时定时器
        }
    }
    // 4. 流控帧 (FC)
    else if (pci == 0x30) {
        if (m_txState != TX_WAIT_FC) return;
        if (frameData.size() < 3) {
            emit logMessage("ISO-TP 错误: 接收流控帧长度小于 3 字节，忽略该帧", 3);
            return;
        }

        m_fcWaitTimer->stop(); // 停止流控帧等待计时器

        uint8_t fs = frameData.at(0) & 0x0F;
        if (fs == 0) { // Continue to Send
            m_fcBs = frameData.at(1);
            m_fcStMin = frameData.at(2);
            
            // 解析 STmin
            int stMinMs = 0;
            if (m_fcStMin <= 127) {
                stMinMs = m_fcStMin;
            } else if (m_fcStMin >= 0xF1 && m_fcStMin <= 0xF9) {
                stMinMs = 1; // 小于毫秒级别的，上位机默认按 1ms 处理
            }
            if (stMinMs == 0) stMinMs = 2; // 默认防丢包保护，至少隔 2ms

            m_txState = TX_SENDING_CF;
            m_cfSentInBlock = 0;
            m_txCfTimer->start(stMinMs);
        } else if (fs == 1) { // Wait
            // 继续等待，重启流控等待定时器，继续等下一个 FC 帧
            m_fcWaitTimer->start(1000);
            emit logMessage("ISO-TP 提示: 接收到 FlowControl WAIT，继续等待...", 0);
        } else { // Overflow/Error
            emit logMessage("ISO-TP 错误: 接收到 FlowControl 溢出，发送中止", 3);
            m_txState = TX_IDLE;
        }
    }
}

// 模拟安全解锁算法 (Seed-to-Key 算法)
uint32_t UdsClient::calculateKey(uint32_t seed)
{
    // 默认算法：密钥为种子取反加上固定偏移 0x55AA1234
    return (~seed) + 0x55AA1234;
}

// 解析收到的完整 UDS 应用报文
void UdsClient::handleUdsResponse(const QByteArray &udsPayload)
{
    if (udsPayload.isEmpty()) return;

    uint8_t serviceId = udsPayload.at(0);
    bool isPositive = (serviceId != 0x7F);

    // 格式化输出十六进制日志
    QString hexStr;
    for (int i = 0; i < udsPayload.size(); ++i) {
        hexStr += QString("%1 ").arg(static_cast<uint8_t>(udsPayload.at(i)), 2, 16, QChar('0')).toUpper();
    }

    if (isPositive) {
        emit logMessage(QString("RX <- 0x%1: %2").arg(m_responseID, 0, 16).arg(hexStr.trimmed()), 2);
    } else {
        uint8_t nrc = (udsPayload.size() > 2) ? udsPayload.at(2) : 0x00;
        
        // 专门处理 Response Pending 延迟响应 (NRC 0x78)
        if (nrc == 0x78) {
            emit logMessage(QString("RX <- 0x%1: %2 (等待中)").arg(m_responseID, 0, 16).arg(hexStr.trimmed()), 0);
            m_udsTimeoutTimer->start(6000); // 重新启动 6秒超时定时器，继续等待正响应
            return; 
        }
        
        emit logMessage(QString("RX <- 0x%1: %2 (负响应)").arg(m_responseID, 0, 16).arg(hexStr.trimmed()), 3);
    }

    m_waitingForUdsResponse = false;
    m_udsTimeoutTimer->stop();

    // 分发事件信号
    if (isPositive) {
        emit udsResponseReceived(serviceId, true, udsPayload.mid(1), 0x00);
    } else {
        uint8_t reqService = (udsPayload.size() > 1) ? udsPayload.at(1) : 0x00;
        uint8_t nrc = (udsPayload.size() > 2) ? udsPayload.at(2) : 0x00;
        emit udsResponseReceived(reqService, false, QByteArray(), nrc);
    }

    // 触发升级状态机
    if (m_upgradeRunning) {
        if (!isPositive) {
            // 升级过程中发生负响应错误，升级失败
            emit logMessage(QString("升级失败: 服务 0x%1 返回 NRC 0x%2")
                            .arg(udsPayload.at(1), 2, 16, QChar('0')).arg(udsPayload.at(2), 2, 16, QChar('0')), 3);
            m_upgradeRunning = false;
            emit upgradeCompleted(false, QString("ECU返回负响应错误 NRC 0x%1").arg(udsPayload.at(2), 2, 16, QChar('0')));
            transitionUpgradeState(UPG_IDLE);
        } else {
            // 正响应，执行下一升级状态
            runUpgradeStateMachine();
        }
    }
}

// ISO-TP 消息传送入口
void UdsClient::transmitIsoTpMessage(const QByteArray &message)
{
    int maxSfLen = (m_protocol == 1) ? 62 : 7;
    if (message.size() <= maxSfLen) {
        sendSingleFrame(message);
    } else {
        m_txBuffer = message;
        int ffPayloadLen = (m_protocol == 1) ? 62 : 6;
        m_txIndex = ffPayloadLen; // 首帧包含前 ffPayloadLen 字节
        m_txExpectedSn = 1;
        m_txState = TX_WAIT_FC;
        m_fcWaitTimer->start(1000); // 开启等待流控帧 N_Bs 1s 定时器
        sendFirstFrame(message);
    }
}

// 发送单帧 (SF)
void UdsClient::sendSingleFrame(const QByteArray &payload)
{
    int maxSize = getMaxFrameSize();
    QVector<char> data(maxSize, 0);
    
    if (m_protocol == 1) { // CANFD
        if (payload.size() <= 7) {
            data[0] = static_cast<char>(payload.size() & 0x0F);
            memcpy(data.data() + 1, payload.constData(), payload.size());
        } else {
            data[0] = 0x00;
            data[1] = static_cast<char>(payload.size() & 0xFF);
            memcpy(data.data() + 2, payload.constData(), payload.size());
        }
    } else { // Classic CAN
        data[0] = static_cast<char>(payload.size() & 0x0F);
        memcpy(data.data() + 1, payload.constData(), payload.size());
    }
    
    m_canThread->sendData(m_requestID, m_isExtended ? 1 : 0, m_protocol, 0, m_channel, data.constData(), maxSize);
}

// 发送首帧 (FF)
void UdsClient::sendFirstFrame(const QByteArray &payload)
{
    int maxSize = getMaxFrameSize();
    QVector<char> data(maxSize, 0);
    int len = payload.size();
    
    data[0] = static_cast<char>(0x10 | ((len >> 8) & 0x0F));
    data[1] = static_cast<char>(len & 0xFF);
    
    int ffPayloadLen = (m_protocol == 1) ? 62 : 6;
    memcpy(data.data() + 2, payload.constData(), qMin(payload.size(), ffPayloadLen));

    m_canThread->sendData(m_requestID, m_isExtended ? 1 : 0, m_protocol, 0, m_channel, data.constData(), maxSize);
}

// 发送连续帧 (CF)
void UdsClient::sendConsecutiveFrame()
{
    if (m_txState != TX_SENDING_CF) return;

    int maxSize = getMaxFrameSize();
    QVector<char> data(maxSize, 0);
    data[0] = static_cast<char>(0x20 | (m_txExpectedSn & 0x0F));

    int bytesLeft = m_txBuffer.size() - m_txIndex;
    int maxCfPayloadLen = maxSize - 1; // 7 for Classic CAN, 63 for CAN-FD
    int bytesToCopy = qMin(maxCfPayloadLen, bytesLeft);
    memcpy(data.data() + 1, m_txBuffer.constData() + m_txIndex, bytesToCopy);
    
    m_canThread->sendData(m_requestID, m_isExtended ? 1 : 0, m_protocol, 0, m_channel, data.constData(), maxSize);

    m_txIndex += bytesToCopy;
    m_txExpectedSn = (m_txExpectedSn + 1) & 0x0F;
    m_cfSentInBlock++;

    if (m_txIndex >= m_txBuffer.size()) {
        // 数据全部发完
        m_txState = TX_IDLE;
        m_txCfTimer->stop();
    } else {
        // 判断是否需要等待流控帧 (BS 限制)
        if (m_fcBs > 0 && m_cfSentInBlock >= m_fcBs) {
            m_txState = TX_WAIT_FC;
            m_txCfTimer->stop();
            m_fcWaitTimer->start(1000); // 开启等待流控帧 N_Bs 1s 定时器
        } else {
            // 继续发送下一个 CF
            int stMinMs = 0;
            if (m_fcStMin <= 127) {
                stMinMs = m_fcStMin;
            } else if (m_fcStMin >= 0xF1 && m_fcStMin <= 0xF9) {
                stMinMs = 1;
            }
            if (stMinMs == 0) stMinMs = 2; // 默认间隔保护
            m_txCfTimer->start(stMinMs);
        }
    }
}

// 发送流控帧 (FC)
void UdsClient::sendFlowControl(uint8_t flowStatus, uint8_t blockSize, uint8_t stMin)
{
    int maxSize = getMaxFrameSize();
    QVector<char> data(maxSize, 0);
    data[0] = static_cast<char>(0x30 | (flowStatus & 0x0F));
    data[1] = static_cast<char>(blockSize);
    data[2] = static_cast<char>(stMin);

    m_canThread->sendData(m_requestID, m_isExtended ? 1 : 0, m_protocol, 0, m_channel, data.constData(), maxSize);
}

// Tester Present 心跳超时槽函数
void UdsClient::onTesterPresentTimeout()
{
    if (!m_canThread) return;
    
    int maxSize = getMaxFrameSize();
    QVector<char> data(maxSize, 0);
    data[0] = 0x02; // SF Length = 2
    data[1] = 0x3E; // Service ID
    data[2] = 0x80; // Sub-function: Suppress response

    m_canThread->sendData(m_requestID, m_isExtended ? 1 : 0, m_protocol, 0, m_channel, data.constData(), maxSize);
}

// ISO-TP 接收连续帧超时槽
void UdsClient::onRxTimeout()
{
    emit logMessage("ISO-TP 错误: 接收连续帧 (CF) 超时！", 3);
    m_rxState = RX_IDLE;
}

// ISO-TP 发送连续帧槽
void UdsClient::onTxCfTimerTimeout()
{
    sendConsecutiveFrame();
}

// UDS 响应超时槽
void UdsClient::onUdsResponseTimeout()
{
    if (m_waitingForUdsResponse) {
        m_waitingForUdsResponse = false;
        emit logMessage("UDS 错误: 诊断响应超时 (Timeout)！", 3);
        emit udsResponseTimeout();
        
        if (m_upgradeRunning) {
            m_upgradeRunning = false;
            emit upgradeCompleted(false, "诊断请求响应超时");
            transitionUpgradeState(UPG_IDLE);
        }
    }
}

// 等待流控帧超时槽 (N_Bs Timeout)
void UdsClient::onFcWaitTimeout()
{
    if (m_txState == TX_WAIT_FC) {
        m_txState = TX_IDLE;
        emit logMessage("ISO-TP 错误: 等待流控帧 (FC) 超时！(N_Bs Timeout)", 3);
        
        if (m_upgradeRunning) {
            m_upgradeRunning = false;
            emit upgradeCompleted(false, "等待流控帧 (FC) 回复超时");
            transitionUpgradeState(UPG_IDLE);
        }
    }
}

// ---------------------- 固件升级状态机 ----------------------

bool UdsClient::startUpgrade(const QByteArray &firmwareData, uint32_t startAddress)
{
    if (m_upgradeRunning) return false;
    if (firmwareData.isEmpty()) {
        emit logMessage("升级失败: 固件数据为空！", 3);
        return false;
    }

    m_firmwareData = firmwareData;
    m_startAddress = startAddress;
    m_firmwareSentBytes = 0;
    m_upgradeBlockCounter = 1;
    m_upgradeRunning = true;
    m_upgradeBlockSize = 256; // 默认传输块大小为 256 字节，可在 0x34 响应中由 ECU 给出

    if (m_testerPresentEnabled) {
        m_testerPresentTimer->stop();
        emit logMessage("固件升级期间临时暂停 Tester Present (0x3E) 心跳", 0);
    }

    emit logMessage("=== 启动自动固件升级流程 ===", 0);
    transitionUpgradeState(UPG_ENTER_EXTENDED);
    return true;
}

void UdsClient::abortUpgrade()
{
    if (m_upgradeRunning) {
        m_upgradeRunning = false;
        emit logMessage("=== 固件升级已被用户中止！ ===", 3);
        emit upgradeCompleted(false, "用户主动中止");
        transitionUpgradeState(UPG_IDLE);
    }
}

void UdsClient::transitionUpgradeState(UpgradeState newState)
{
    m_upgradeState = newState;
    QString stateName;
    switch(m_upgradeState) {
        case UPG_IDLE: stateName = "未启动"; break;
        case UPG_ENTER_EXTENDED: stateName = "正在进入扩展会话..."; break;
        case UPG_SECURITY_SEED: stateName = "正在请求安全种子..."; break;
        case UPG_SECURITY_KEY: stateName = "正在发送解锁密钥..."; break;
        case UPG_ENTER_PROGRAMMING: stateName = "正在进入编程会话..."; break;
        case UPG_REQUEST_DOWNLOAD: stateName = "正在发送下载请求..."; break;
        case UPG_TRANSFER_DATA: stateName = "正在传输固件数据..."; break;
        case UPG_EXIT_TRANSFER: stateName = "正在退出传输..."; break;
        case UPG_CHECKSUM_VERIFY: stateName = "正在执行固件校验..."; break;
        case UPG_ECU_RESET: stateName = "正在执行ECU重启..."; break;
        case UPG_COMPLETED: stateName = "升级完成"; break;
    }
    emit upgradeStateChanged(stateName);
    
    if (m_upgradeState == UPG_IDLE || m_upgradeState == UPG_COMPLETED) {
        m_fcWaitTimer->stop(); // 确保停止流控帧超时器
        if (m_testerPresentEnabled) {
            m_testerPresentTimer->start(m_testerPresentInterval);
            emit logMessage("固件升级结束，恢复 Tester Present (0x3E) 心跳", 0);
        }
    }

    if (m_upgradeState != UPG_IDLE && m_upgradeState != UPG_COMPLETED) {
        // 执行当前状态对应的 UDS 诊断指令
        runUpgradeStateMachine();
    }
}

void UdsClient::runUpgradeStateMachine()
{
    if (!m_upgradeRunning) return;

    switch(m_upgradeState) {
        case UPG_ENTER_EXTENDED:
            // 步骤 1: 进入扩展会话 (10 03)
            sendUdsRequest(0x10, QByteArray::fromHex("03"));
            break;
            
        case UPG_SECURITY_SEED:
            // 步骤 2: 请求安全种子 (27 01)
            sendUdsRequest(0x27, QByteArray::fromHex("01"));
            break;
            
        case UPG_SECURITY_KEY:
            // 步骤 3: 在上一步的正响应处理中已经由 udsResponseReceived 驱动计算出了密钥并触发了本步骤
            break;
            
        case UPG_ENTER_PROGRAMMING:
            // 步骤 4: 进入编程会话 (10 02)
            sendUdsRequest(0x10, QByteArray::fromHex("02"));
            break;
            
        case UPG_REQUEST_DOWNLOAD: {
            // 步骤 5: 请求下载 (34)
            // 格式: 34 + 00 (压缩/加密算法) + AddressAndLengthFormatIdentifier + Address (4字节) + Length (4字节)
            QByteArray payload;
            payload.append(static_cast<char>(0x00)); // 默认无压缩无加密
            payload.append(static_cast<char>(0x44)); // 起始地址占4字节，数据长度占4字节
            
            // 填充起始地址
            payload.append(static_cast<char>((m_startAddress >> 24) & 0xFF));
            payload.append(static_cast<char>((m_startAddress >> 16) & 0xFF));
            payload.append(static_cast<char>((m_startAddress >> 8) & 0xFF));
            payload.append(static_cast<char>(m_startAddress & 0xFF));
            
            // 填充固件大小
            uint32_t size = m_firmwareData.size();
            payload.append(static_cast<char>((size >> 24) & 0xFF));
            payload.append(static_cast<char>((size >> 16) & 0xFF));
            payload.append(static_cast<char>((size >> 8) & 0xFF));
            payload.append(static_cast<char>(size & 0xFF));
            
            sendUdsRequest(0x34, payload);
            break;
        }
        case UPG_TRANSFER_DATA: {
            // 步骤 6: 数据传输 (36)
            int bytesLeft = m_firmwareData.size() - m_firmwareSentBytes;
            if (bytesLeft <= 0) {
                // 传完所有块，转至退出传输
                transitionUpgradeState(UPG_EXIT_TRANSFER);
                break;
            }
            
            int chunkSize = qMin(m_upgradeBlockSize, bytesLeft);
            QByteArray payload;
            payload.append(static_cast<char>(m_upgradeBlockCounter & 0xFF));
            payload.append(m_firmwareData.mid(m_firmwareSentBytes, chunkSize));
            
            emit upgradeProgress(m_firmwareSentBytes, m_firmwareData.size(), 
                                 (m_firmwareSentBytes * 100) / m_firmwareData.size());
            
            // 仅在此处做计数器增加
            m_firmwareSentBytes += chunkSize;
            m_upgradeBlockCounter++;

            sendUdsRequest(0x36, payload);
            break;
        }
        case UPG_EXIT_TRANSFER:
            // 步骤 7: 退出传输 (37)
            sendUdsRequest(0x37, QByteArray());
            break;
            
        case UPG_CHECKSUM_VERIFY: {
            // 步骤 8: 校验固件 (31) 
            // 格式: 31 01 (启动例程) + 02 02 (自检例程 DID) + CRC32 (4字节)
            uint32_t crcVal = calculateCrc32(m_firmwareData);
            QByteArray payload = QByteArray::fromHex("010202");
            payload.append(static_cast<char>((crcVal >> 24) & 0xFF));
            payload.append(static_cast<char>((crcVal >> 16) & 0xFF));
            payload.append(static_cast<char>((crcVal >> 8) & 0xFF));
            payload.append(static_cast<char>(crcVal & 0xFF));
            
            emit logMessage(QString("固件升级: 发送例程校验命令 31 01 02 02，附加 CRC32=0x%1")
                            .arg(crcVal, 8, 16, QChar('0')).toUpper(), 0);
            
            sendUdsRequest(0x31, payload);
            break;
        }
        case UPG_ECU_RESET:
            // 步骤 9: ECU 重启运行新固件 (11 01)
            sendUdsRequest(0x11, QByteArray::fromHex("01"));
            break;
            
        case UPG_COMPLETED:
            // 升级完成
            m_upgradeRunning = false;
            emit upgradeProgress(m_firmwareData.size(), m_firmwareData.size(), 100);
            emit upgradeCompleted(true);
            transitionUpgradeState(UPG_IDLE);
            break;
            
        default:
            break;
    }
}

// 重载的 UDS 应用层协议自驱动转换点
void UdsClient::onUdsResponseReceivedSlot(uint8_t serviceId, bool isPositive, const QByteArray &payload, uint8_t nrc)
{
    Q_UNUSED(serviceId);
    Q_UNUSED(nrc);
    if (!m_upgradeRunning) return;

    if (!isPositive) return; // 负响应已经在 handleUdsResponse 里被中止了

    switch (m_upgradeState) {
        case UPG_ENTER_EXTENDED:
            // 10 03 成功，转入安全种子请求
            transitionUpgradeState(UPG_SECURITY_SEED);
            break;
            
        case UPG_SECURITY_SEED: {
            // 27 01 成功，提取种子并计算 Key
            if (payload.size() >= 4) {
                uint32_t seed = (static_cast<uint8_t>(payload.at(0)) << 24) |
                                (static_cast<uint8_t>(payload.at(1)) << 16) |
                                (static_cast<uint8_t>(payload.at(2)) << 8)  |
                                 static_cast<uint8_t>(payload.at(3));
                
                uint32_t key = calculateKey(seed);
                
                emit logMessage(QString("安全解锁: 收到 Seed=0x%1, 计算 Key=0x%2")
                                .arg(seed, 8, 16, QChar('0')).arg(key, 8, 16, QChar('0')), 0);
                
                // 转入发送 KEY 状态
                m_upgradeState = UPG_SECURITY_KEY;
                QByteArray keyPayload;
                keyPayload.append(static_cast<char>(0x02)); // sub-function: send key
                keyPayload.append(static_cast<char>((key >> 24) & 0xFF));
                keyPayload.append(static_cast<char>((key >> 16) & 0xFF));
                keyPayload.append(static_cast<char>((key >> 8) & 0xFF));
                keyPayload.append(static_cast<char>(key & 0xFF));
                
                sendUdsRequest(0x27, keyPayload);
            } else {
                emit logMessage("安全解锁错误: 种子字节长度不足", 3);
                abortUpgrade();
            }
            break;
        }
        case UPG_SECURITY_KEY:
            // 27 02 解锁成功，转入编程会话
            transitionUpgradeState(UPG_ENTER_PROGRAMMING);
            break;
            
        case UPG_ENTER_PROGRAMMING:
            // 10 02 成功，发送下载请求
            transitionUpgradeState(UPG_REQUEST_DOWNLOAD);
            break;
            
        case UPG_REQUEST_DOWNLOAD:
            // 34 成功，ECU 通常会返回允许的 BlockSize (由 34 响应负荷高4位和低4位定义)
            // 简单解析：如果是正响应 74 后面带有大小定义，我们可解析它
            if (payload.size() >= 2) {
                // 假设响应格式是 74 [LengthFormat] [MaxNumberOfBlockLength]
                // 我们可以取得 ECU 期望的最大单包大小
                uint8_t lenFormat = payload.at(0);
                int sizeLen = lenFormat & 0x0F;
                if (payload.size() >= 1 + sizeLen) {
                    int ecuMaxLen = 0;
                    for (int k = 0; k < sizeLen; ++k) {
                        ecuMaxLen = (ecuMaxLen << 8) | static_cast<uint8_t>(payload.at(1 + k));
                    }
                    if (ecuMaxLen > 10 && ecuMaxLen < 4096) {
                        m_upgradeBlockSize = ecuMaxLen - 2; // 去掉 0x36 和 blockCounter 的 2 字节
                        emit logMessage(QString("升级协商: ECU 允许的最大 Block 大小为 %1 字节 (UDS 净负荷: %2 字节)")
                                        .arg(ecuMaxLen).arg(m_upgradeBlockSize), 0);
                    }
                }
            }
            transitionUpgradeState(UPG_TRANSFER_DATA);
            break;
            
        case UPG_TRANSFER_DATA:
            // 单包 0x36 传输成功，继续发送下一包
            runUpgradeStateMachine();
            break;
            
        case UPG_EXIT_TRANSFER:
            // 37 退出成功，进行 CRC 校验
            transitionUpgradeState(UPG_CHECKSUM_VERIFY);
            break;
            
        case UPG_CHECKSUM_VERIFY:
            // 31 自检完成，发送 ECU 复位
            transitionUpgradeState(UPG_ECU_RESET);
            break;
            
        case UPG_ECU_RESET:
            // 11 01 复位正响应，代表升级彻底完成！
            transitionUpgradeState(UPG_COMPLETED);
            break;
            
        default:
            break;
    }
}

int UdsClient::getMaxFrameSize() const
{
    return m_protocol == 1 ? 64 : 8;
}

uint32_t UdsClient::calculateCrc32(const QByteArray &data) const
{
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < data.size(); ++i) {
        uint8_t byte = static_cast<uint8_t>(data.at(i));
        crc ^= byte;
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}
