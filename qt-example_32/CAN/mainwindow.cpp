#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDialog>
#include <QDebug>
#include <QScrollBar>
#include <QMessageBox>
#include <QTime>
#include <QDateTime>
#include <QThread>
#include <QListView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QProgressBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QBrush>
#include <QColor>
#include <QFileDialog>
#include <QtMath>
#include "imuparser.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    m_isWaitingForAck = false;
    for (int i = 0; i < 2; ++i) {
        m_canTimestampBaseValid[i] = false;
        m_canTimestampBaseRaw[i] = 0;
    }
    m_parsedIMUElapsedBaseValid = false;
    m_parsedIMUElapsedBaseMs = 0;
    ui->setupUi(this);

    // 二次开发：解除窗口尺寸锁定，允许窗口最大化和自由拉伸
    this->setMinimumSize(1024, 768);
    this->setMaximumSize(16777215, 16777215);


    ui->filterModeCombo->setCurrentIndex(2);

    QStringList listHeader;
    listHeader << "时间"  << "通道" << "/" << "ID" << "Frame" << "类型" << "DLC" << "CAN-FD" << "数据";

    ui->tableWidget->setColumnCount(listHeader.count());
    ui->tableWidget->setHorizontalHeaderLabels(listHeader);
    ui->tableWidget->setColumnWidth(0,80);
    ui->tableWidget->setColumnWidth(1,40);
    ui->tableWidget->setColumnWidth(2,60);
    ui->tableWidget->setColumnWidth(3,80);
    ui->tableWidget->setColumnWidth(4,90);
    ui->tableWidget->setColumnWidth(5,90);
    ui->tableWidget->setColumnWidth(6,80);
    ui->tableWidget->setColumnWidth(7,90);
    ui->tableWidget->setColumnWidth(8,200);

    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

    // 强行渲染表头（避开 Windows 系统默认主题干扰）并用整体背景覆盖解决右侧白条问题
    ui->tableWidget->horizontalHeader()->setStyleSheet(
        "QHeaderView { background-color: #2e3440; }"
        "QHeaderView::section { background-color: #3b4252; color: #eceff4; border: 1px solid #2e3440; padding: 4px; font-weight: bold; }"
    );
    ui->tableWidget->verticalHeader()->setStyleSheet(
        "QHeaderView { background-color: #2e3440; }"
        "QHeaderView::section { background-color: #3b4252; color: #eceff4; border: 1px solid #2e3440; padding: 4px; }"
    );

    QStringList ss = {
        "ZQWL-UCANFD-100C", "ZQWL-UCAN-101C", "ZQWL-UCANFD-100K", "ZQWL-UCAN-101K",
        "ZQWL-UCANFD-100E", "ZQWL-UCAN-101E", "ZQWL-UCANFD-200U", "ZQWL-UCAN-201U",
        "ZQWL-UCANFD-200C", "ZQWL-UCAN-201C", "ZQWL-UCAN-401U", "ZQWL-UCANFD-400U"
    };

    ui->deviceTypeCombo->clear();
    for(int i = 0; i < ss.count(); i++){
         ui->deviceTypeCombo->addItem(ss.at(i), "");
    }

    // 默认选择设备类型和仲裁域波特率 (必须在 addItem 之后调用才生效)
    ui->deviceTypeCombo->setCurrentIndex(5);  // ZQWL-UCAN-101E
    ui->ABIT1Combo->setCurrentIndex(2);       // 500kbps

    canthread = new CANThread();
    connect(canthread, &CANThread::recvedCANData, this, &MainWindow::canRecvedCANData, Qt::BlockingQueuedConnection);
    connect(canthread, &CANThread::recvedCANFDData, this, &MainWindow::canRecvedCANFDData, Qt::BlockingQueuedConnection);

            // 二次开发：重置主窗体为 QTabWidget 以集成监控仪表盘
    QWidget *oldCentral = centralWidget();
    QTabWidget *mainTab = new QTabWidget(this);
    
    // 隐藏 Qt Designer 留下的固定坐标布局容器，防止其冲突
    for (QWidget *child : oldCentral->findChildren<QWidget*>()) {
        if (child->objectName().startsWith("layoutWidget")) {
            child->hide();
        }
    }
    
    // 给原“收发控制”页面建立主自适应垂直布局
    QVBoxLayout *oldLayout = new QVBoxLayout(oldCentral);
    oldLayout->setContentsMargins(15, 15, 15, 15);
    oldLayout->setSpacing(10);
    
    // 1. 设备选择行 (紧凑靠左)
    QHBoxLayout *devLayout = new QHBoxLayout();
    devLayout->setSpacing(8);
    devLayout->addWidget(ui->label);
    devLayout->addWidget(ui->deviceTypeCombo);
    devLayout->addWidget(ui->label_2);
    devLayout->addWidget(ui->deviceIndexCombo);
    devLayout->addStretch(1);
    oldLayout->addLayout(devLayout);
    
    // 2. 参数配置 GroupBox 重构布局 (紧凑靠左)
    QVBoxLayout *gbLayout = new QVBoxLayout(ui->groupBox);
    gbLayout->setContentsMargins(15, 22, 15, 12);
    gbLayout->setSpacing(10);
    
    ui->workStatusCombo->setMaximumWidth(120);
    ui->canFDStandardCombo->setMaximumWidth(120);
    ui->ABIT1Combo->setMaximumWidth(120);
    ui->ABIT2Combo->setMaximumWidth(120);
    ui->filterModeCombo->setMaximumWidth(120);
    ui->StartIDEdit->setMaximumWidth(120);
    ui->endIDEdit->setMaximumWidth(120);
    ui->CustomBaudrateEdit->setMaximumWidth(150);
    
    QHBoxLayout *gbRow1 = new QHBoxLayout();
    gbRow1->addWidget(ui->label_3);
    gbRow1->addWidget(ui->workStatusCombo);
    gbRow1->addSpacing(15);
    gbRow1->addWidget(ui->label_4);
    gbRow1->addWidget(ui->canFDStandardCombo);
    gbRow1->addSpacing(15);
    gbRow1->addWidget(ui->resistanceCheckBox);
    gbRow1->addStretch(1);
    
    QHBoxLayout *gbRow2 = new QHBoxLayout();
    gbRow2->addWidget(ui->label_5);
    gbRow2->addWidget(ui->ABIT1Combo);
    gbRow2->addSpacing(15);
    gbRow2->addWidget(ui->label_6);
    gbRow2->addWidget(ui->ABIT2Combo);
    gbRow2->addStretch(1);
    
    QHBoxLayout *gbRow3 = new QHBoxLayout();
    gbRow3->addWidget(ui->CustomBaudrateCheckBox);
    gbRow3->addWidget(ui->CustomBaudrateEdit);
    gbRow3->addWidget(ui->label_7);
    gbRow3->addStretch(1);
    
    QHBoxLayout *gbRow4 = new QHBoxLayout();
    gbRow4->addWidget(ui->label_8);
    gbRow4->addWidget(ui->filterModeCombo);
    gbRow4->addSpacing(15);
    gbRow4->addWidget(ui->label_9);
    gbRow4->addWidget(ui->StartIDEdit);
    gbRow4->addSpacing(15);
    gbRow4->addWidget(ui->label_10);
    gbRow4->addWidget(ui->endIDEdit);
    gbRow4->addStretch(1);
    
    gbLayout->addLayout(gbRow1);
    gbLayout->addLayout(gbRow2);
    gbLayout->addLayout(gbRow3);
    gbLayout->addLayout(gbRow4);
    oldLayout->addWidget(ui->groupBox);
    
    // 3. 打开/关闭/初始化等按钮行 (紧凑靠左)
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    
    btnQuickStart = new QPushButton(QString::fromUtf8("⭐ 一键启动"), this);
    btnQuickStart->setStyleSheet(
        "QPushButton { background-color: #a3be8c; color: #2e3440; border-radius: 4px; padding: 6px 16px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background-color: #b5cca3; }"
        "QPushButton:pressed { background-color: #8faa78; }"
    );
    connect(btnQuickStart, &QPushButton::clicked, this, &MainWindow::onQuickStartClicked);
    btnLayout->addWidget(btnQuickStart);
    
    btnLayout->addWidget(ui->openDeviceBtn);
    btnLayout->addWidget(ui->initCANBtn);
    btnLayout->addWidget(ui->StartCANBtn);
    btnLayout->addWidget(ui->reSetCANBtn);
    btnLayout->addWidget(ui->closeDeviceBtn);
    btnLayout->addStretch(1);
    oldLayout->addLayout(btnLayout);
    
    // 4. 数据发送 GroupBox 重构布局
    QVBoxLayout *gb2Layout = new QVBoxLayout(ui->groupBox_2);
    gb2Layout->setContentsMargins(15, 22, 15, 12);
    gb2Layout->setSpacing(10);
    
    ui->label_11->setText(QString::fromUtf8("ID (Hex):"));
    ui->label_11->setMinimumWidth(60);
    ui->sendIDEdit->setMinimumWidth(100);
    ui->sendIDEdit->setMaximumWidth(150);
    ui->frameTypeCombo->setMaximumWidth(120);
    ui->protocolCombo->setMaximumWidth(120);
    ui->sendPathCombo->setMinimumWidth(80);
    ui->sendPathCombo->setMaximumWidth(120);
    
    QHBoxLayout *sendRow1 = new QHBoxLayout();
    sendRow1->addWidget(ui->label_11);
    sendRow1->addWidget(ui->sendIDEdit);
    sendRow1->addSpacing(15);
    sendRow1->addWidget(ui->label_12);
    sendRow1->addWidget(ui->frameTypeCombo);
    sendRow1->addSpacing(15);
    sendRow1->addWidget(ui->label_13);
    sendRow1->addWidget(ui->protocolCombo);
    sendRow1->addSpacing(15);
    sendRow1->addWidget(ui->CANFDaccCheck);
    sendRow1->addStretch(1); // 强制第一行内容靠左
    
    QHBoxLayout *sendRow2 = new QHBoxLayout();
    sendRow2->addWidget(ui->label_14);
    sendRow2->addWidget(ui->sendDataEdit, 1); // 允许输入框横向无限拉伸，方便输入长报文
    
    QHBoxLayout *sendRow3 = new QHBoxLayout();
    sendRow3->addWidget(ui->label_15);
    sendRow3->addWidget(ui->sendPathCombo);
    sendRow3->addSpacing(25);
    sendRow3->addWidget(ui->sendBtn);
    sendRow3->addStretch(1); // 发送按钮和通道靠左
    
    gb2Layout->addLayout(sendRow1);
    gb2Layout->addLayout(sendRow2);
    gb2Layout->addLayout(sendRow3);
    oldLayout->addWidget(ui->groupBox_2);
    
    // 5. 数据接收 GroupBox 重构布局
    QHBoxLayout *recvLayout = new QHBoxLayout(ui->groupBox_3);
    recvLayout->setContentsMargins(15, 22, 15, 12);
    recvLayout->setSpacing(15);
    recvLayout->addWidget(ui->tableWidget, 1); // 接收数据表格占满剩余纵向空间
    
    QVBoxLayout *recvRightLayout = new QVBoxLayout();
    recvRightLayout->setSpacing(8);
    
    ui->checkBox_4->setText(QString::fromUtf8("实时显示"));
    ui->checkBox_4->setFixedWidth(100);
    ui->cleanListBtn->setFixedWidth(100);
    
    recvRightLayout->addWidget(ui->checkBox_4);
    recvRightLayout->addWidget(ui->cleanListBtn);

    chkSaveCAN = new QCheckBox(QString::fromUtf8("保存数据"), ui->groupBox_3);
    chkSaveCAN->setFixedWidth(100);
    cmbFormatCAN = new QComboBox(ui->groupBox_3);
    cmbFormatCAN->addItem(QString::fromUtf8("CSV 格式"));
    cmbFormatCAN->addItem(QString::fromUtf8("TXT 格式"));
    cmbFormatCAN->setStyleSheet("QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 3px; }");
    
    recvRightLayout->addWidget(chkSaveCAN);
    cmbFormatCAN->hide(); // 隐藏格式选择，默认为txt
    recvRightLayout->addWidget(cmbFormatCAN);
    recvRightLayout->addStretch(1);
    
    recvLayout->addLayout(recvRightLayout);
    oldLayout->addWidget(ui->groupBox_3, 1);

    connect(chkSaveCAN, &QCheckBox::stateChanged, this, &MainWindow::onSaveCANStateChanged);
    
    // 应用于原收发控制页面的深色样式表
    oldCentral->setStyleSheet(
        "#centralWidget { background-color: #2e3440; color: #d8dee9; }"
        "QLabel { color: #d8dee9; font-size: 12px; }"
        "QGroupBox { color: #88c0d0; font-weight: bold; border: 2px solid #3b4252; border-radius: 6px; margin-top: 10px; }"
        "QPushButton { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 4px; padding: 5px 10px; font-weight: bold; }"
        "QPushButton:hover { background-color: #434c5e; border-color: #81a1c1; }"
        "QPushButton:pressed { background-color: #4c566a; }"
        "QPushButton:disabled { background-color: #2e3440; color: #4c566a; border-color: #2e3440; }"
        "QLineEdit { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 3px; font-family: 'Consolas'; }"
        "QLineEdit:focus { border-color: #88c0d0; }"
        "QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 3px; }"
        "QComboBox::drop-down { border: none; }"
        "QCheckBox { color: #eceff4; font-size: 13px; }"
        "QTableWidget { background-color: #2e3440; gridline-color: #3b4252; color: #eceff4; font-size: 12px; }"
        "QHeaderView::section { background-color: #3b4252; color: #eceff4; padding: 5px; border: 1px solid #2e3440; font-weight: bold; }"
    );

    // 优化后的选项卡样式表 (微调内边距和最小宽度，防止文字被截断)
    mainTab->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #3b4252; background: #2e3440; }"
        "QTabBar::tab { background: #2e3440; color: #d8dee9; padding: 8px 15px; font-family: 'Segoe UI', Arial; font-size: 13px; border: 1px solid #3b4252; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; min-width: 100px; }"
        "QTabBar::tab:selected { background: #3b4252; color: #88c0d0; font-weight: bold; border-top: 3px solid #88c0d0; }"
        "QTabBar::tab:hover { background: #434c5e; color: #eceff4; }"
    );
    
    oldCentral->setParent(mainTab);
    mainTab->addTab(oldCentral, QString::fromUtf8("CAN收发控制"));
    
    imuTab = new QWidget(mainTab);
    setupIMUDashboard();
    mainTab->addTab(imuTab, QString::fromUtf8("车姿监控"));
    
    rawTab = new QWidget(mainTab);
    setupRawTab();
    mainTab->addTab(rawTab, QString::fromUtf8("数据透传"));

    serialTab = new QWidget(mainTab);
    m_serialPort = nullptr;
    setupSerialTab();
    mainTab->addTab(serialTab, QString::fromUtf8("串口控制"));
    
    setCentralWidget(mainTab);
    
    //  50ms UI 刷新定时
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::refreshIMUDashboard);
    refreshTimer->start(50);

    // 5秒定时将日志缓冲区刷入磁盘
    flushTimer = new QTimer(this);
    connect(flushTimer, &QTimer::timeout, this, &MainWindow::onFlushTimeout);
    flushTimer->start(5000);
}

static const int deviceType_index_arr[12] = {42,3,42,3,42,3,41,4,41,4,200,201};


void MainWindow::closeEvent(QCloseEvent *event)
{
    canthread->stop();
    canthread->closeDevice();
    if (!canthread->wait(2000)) {
        canthread->terminate();
        canthread->wait();
    }
    
    if (m_canFile.isOpen()) {
        m_canFile.close();
    }
    if (m_parsedIMUFile.isOpen()) {
        m_parsedIMUStream.flush();
        m_parsedIMUFile.close();
    }
    if (m_rawFile.isOpen()) {
        m_rawFile.close();
    }
    if (m_rawBinFile.isOpen()) {
        m_rawBinFile.close();
    }

    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    if (m_serialTxtFile.isOpen()) {
        m_serialTxtFile.close();
    }
    if (m_serialBinFile.isOpen()) {
        m_serialBinFile.close();
    }

    QMainWindow::closeEvent(event);
}

MainWindow::~MainWindow()
{
    if (canthread) {
        canthread->stop();
        canthread->wait(2000);
        delete canthread;
        canthread = nullptr;
    }
    if (m_serialPort) {
        if (m_serialPort->isOpen()) {
            m_serialPort->close();
        }
        delete m_serialPort;
        m_serialPort = nullptr;
    }
    if (refreshTimer) {
        refreshTimer->stop();
    }
    if (flushTimer) {
        flushTimer->stop();
    }
    delete ui;
}

QString MainWindow::formatCANReceiveTime(UINT64 deviceTimestampRaw, UINT channel)
{
    // 周立功接收结构里的 timestamp 单位为 us。它比 UI 线程取当前时间更适合区分批量接收的多帧间隔。
    // 由于 timestamp 通常是设备相对计数，不是绝对年月日，这里用“第一帧设备时间 + 第一帧主机时间”
    // 建立映射，后续用设备微秒差值换算为本地显示时间。
    if (channel >= 2 || deviceTimestampRaw == 0) {
        return QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    }

    const UINT64 resetJumpThresholdMs = 10ULL * 60ULL * 1000ULL; // 10分钟，防止重连/计数器异常跳变

    if (!m_canTimestampBaseValid[channel] ||
        deviceTimestampRaw < m_canTimestampBaseRaw[channel] ||
        deviceTimestampRaw - m_canTimestampBaseRaw[channel] > resetJumpThresholdMs) {
        m_canTimestampBaseValid[channel] = true;
        m_canTimestampBaseRaw[channel] = deviceTimestampRaw;
        m_canTimestampBaseHostTime[channel] = QDateTime::currentDateTime();
        return m_canTimestampBaseHostTime[channel].toString("hh:mm:ss.zzz");
    }

    UINT64 deltaMs = deviceTimestampRaw - m_canTimestampBaseRaw[channel];
    QDateTime displayTime = m_canTimestampBaseHostTime[channel].addMSecs(static_cast<qint64>(deltaMs));
    return displayTime.toString("hh:mm:ss.zzz");
}

qint64 MainWindow::getCANReceiveElapsedMs(UINT64 deviceTimestampRaw, UINT channel)
{
    if (channel >= 2 || deviceTimestampRaw == 0 || !m_canTimestampBaseValid[channel]) {
        return m_parsedIMUStartTime.msecsTo(QDateTime::currentDateTime());
    }
    if (deviceTimestampRaw < m_canTimestampBaseRaw[channel]) {
        return 0;
    }
    return static_cast<qint64>(deviceTimestampRaw - m_canTimestampBaseRaw[channel]);
}

void MainWindow::canRecvedCANData(QVector<ZCAN_Receive_Data> recvCANData,UINT frameCount,UINT channel)
{
    QStringList messageList;
    QString str;
    for(int i = 0;i < frameCount;i ++)
    {
        // 提取 ID
        UINT can_id = GET_ID(recvCANData[i].frame.can_id);
        
        // 馈给姿传感器解析
        QString receiveTimeText = formatCANReceiveTime(recvCANData[i].timestamp, channel);
        qint64 receiveElapsedMs = getCANReceiveElapsedMs(recvCANData[i].timestamp, channel);
        IMUParser::getInstance()->parseCANFrame(can_id, recvCANData[i].frame.data, recvCANData[i].frame.can_dlc, receiveTimeText);
        saveParsedIMUSnapshot(can_id, channel, receiveTimeText, receiveElapsedMs);

        messageList.clear();
        messageList << receiveTimeText;//时间
        messageList << QString::number(channel);//通道
        messageList << QString::fromUtf8("接收");//收发
        messageList << "0x" + QString::number(can_id,16);//ID
        messageList << (IS_EFF(recvCANData[i].frame.can_id) ? "扩展" : "标准");//Frame
        messageList << (IS_RTR(recvCANData[i].frame.can_id) ? "远程" : "数据");//类型
        messageList << QString::number(recvCANData[i].frame.can_dlc);//DLC
        messageList << "CAN";//CAN-FD
        str = "";
        if(!IS_RTR(recvCANData[i].frame.can_id))//标准帧显示数
        {
            for(int j = 0;j < recvCANData[i].frame.can_dlc;j ++)
                str += QString("%1 ").arg(recvCANData[i].frame.data[j],2,16,QChar('0'));//QString::number(recvCANData[i].frame.data[j],16) + " ";
        }
        messageList << str;//数据
        AddDataToList(messageList);
    }
}

void MainWindow::canRecvedCANFDData(QVector<ZCAN_ReceiveFD_Data> recvCANFDData,UINT frameCount,UINT channel)
{
    QStringList messageList;
    QString str;
    for(int i = 0;i < frameCount;i ++)
    {
        // 提取 ID
        UINT can_id = GET_ID(recvCANFDData[i].frame.can_id);

        // 馈给姿传感器解析
        QString receiveTimeText = formatCANReceiveTime(recvCANFDData[i].timestamp, channel);
        qint64 receiveElapsedMs = getCANReceiveElapsedMs(recvCANFDData[i].timestamp, channel);
        IMUParser::getInstance()->parseCANFrame(can_id, recvCANFDData[i].frame.data, recvCANFDData[i].frame.len, receiveTimeText);
        saveParsedIMUSnapshot(can_id, channel, receiveTimeText, receiveElapsedMs);

        messageList.clear();
        messageList << receiveTimeText;//时间
        messageList << QString::number(channel);//通道
        messageList << QString::fromUtf8("接收");//收发
        messageList << "0x" + QString::number(can_id,16);//ID
        messageList << (IS_EFF(recvCANFDData[i].frame.can_id) ? "扩展" : "标准");//Frame
        messageList << (IS_RTR(recvCANFDData[i].frame.can_id) ? "远程" : "数据");//类型
        messageList << QString::number(recvCANFDData[i].frame.len);//DLC
        messageList << "CANFD";//CAN-FD
        str = "";
        if(!IS_RTR(recvCANFDData[i].frame.can_id))//标准帧显示数
        {
            for(int j = 0;j < recvCANFDData[i].frame.len;j ++)
                str += QString("%1 ").arg(recvCANFDData[i].frame.data[j],2,16,QChar('0'));//QString::number(recvCANFDData[i].frame.data[j],16) + " ";
        }
        if(IS_RTR(recvCANFDData[i].frame.can_id))
            messageList << "";
        else
            messageList << str;//数据
        AddDataToList(messageList);
    }
}

void MainWindow::on_cleanListBtn_clicked()
{
    ui->tableWidget->setRowCount(0);
}

void MainWindow::on_openDeviceBtn_clicked()
{
    //1.打开设
    if(canthread->openDevice(deviceType_index_arr[ui->deviceTypeCombo->currentIndex()],ui->deviceIndexCombo->currentIndex(),0))
    {
        ui->openDeviceBtn->setEnabled(false);
        ui->initCANBtn->setEnabled(true);
        ui->closeDeviceBtn->setEnabled(true);
    }
    else
        QMessageBox::warning(this,"警告","设打失败");
}

void MainWindow::on_initCANBtn_clicked()
{
    //2.设置CANFD标准
    if(!canthread->setCANFDStandard(ui->canFDStandardCombo->currentIndex()))
    {
        QMessageBox::warning(this,"警告","设置CANFD标准失败");
        return;
    }
    UINT rate1 = 0,rate2 = 0;
    if(ui->CustomBaudrateCheckBox->checkState())//定义
    {
        if(!canthread->setCustomBaudrateFD(ui->CustomBaudrateEdit->text()))
        {
            QMessageBox::warning(this,"警告","设置定义波特率失败！");
            return;
        }
    }
    else
    {
        switch(ui->ABIT1Combo->currentIndex())
        {
            case 0:
                rate1 = 1000000;
            break;
            case 1:
                rate1 = 800000;
            break;
            case 2:
                rate1 = 500000;
            break;
            case 3:
                rate1 = 250000;
            break;
            case 4:
                rate1 = 125000;
            break;
            case 5:
                rate1 = 100000;
            break;
            case 6:
                rate1 = 50000;
            break;
            default:
                rate1 = 50000;
            break;
        }
        switch(ui->ABIT2Combo->currentIndex())
        {
            case 0:
                rate2 = 5000000;
            break;
            case 1:
                rate2 = 4000000;
            break;
            case 2:
                rate2 = 2000000;
            break;
            case 3:
                rate2 = 1000000;
            break;
            default:
                rate2 = 1000000;
            break;
        }
        if(!canthread->setBaudrateFD(rate1,rate2))
        {
            QMessageBox::warning(this,"警告","设置波特率失败！");
            return;
        }
    }
    afterReSet();
}

void MainWindow::on_closeDeviceBtn_clicked()
{
    //9.关闭设
    canthread->stop();
    canthread->closeDevice();
    for (int i = 0; i < 2; ++i) {
        m_canTimestampBaseValid[i] = false;
        m_canTimestampBaseRaw[i] = 0;
    }
    ui->initCANBtn->setEnabled(false);
    ui->StartCANBtn->setEnabled(false);
    ui->reSetCANBtn->setEnabled(false);
    ui->closeDeviceBtn->setEnabled(false);
    ui->sendBtn->setEnabled(false);
    ui->openDeviceBtn->setEnabled(true);

    btnQuickStart->setEnabled(true);
    btnQuickStart->setText(QString::fromUtf8("⭐ 一键启动"));
}


void MainWindow::on_StartCANBtn_clicked()
{
    //7.动CAN
    if(!canthread->startCAN())
    {
        QMessageBox::warning(this,"警告","CAN动失败！");
        return;
    }
    ui->StartCANBtn->setEnabled(false);
    ui->reSetCANBtn->setEnabled(true);
    ui->sendBtn->setEnabled(true);
    canthread->start();//动接
}

//定义函数 非事件操作函
bool MainWindow::afterReSet()
{
    //4.初化CAN
    if(!canthread->initCAN())
    {
        QMessageBox::warning(this,"警告","CAN初化失败");
        return false;
    }
    //5.使能终电阻
    if(!canthread->setResistanceEnable(ui->resistanceCheckBox->checkState() ? 1: 0))
    {
        QMessageBox::warning(this,"警告","使能终电阻失败");
        return false;
    }
    //6.设置滤波 不置
    if(ui->filterModeCombo->currentIndex() != 2)
    {
        if(!canthread->setFilter(ui->filterModeCombo->currentIndex(),"0x" + ui->StartIDEdit->text(),"0x" + ui->endIDEdit->text()))
        {
            QMessageBox::warning(this,"警告","设置滤波失败");
            return false;
        }
    }
    ui->initCANBtn->setEnabled(false);
    ui->StartCANBtn->setEnabled(true);
    return true;
}

void MainWindow::on_reSetCANBtn_clicked()
{
    //0.复位设，  复位后回4
    canthread->stop();
    if(!canthread->reSetCAN())
    {
        QMessageBox::warning(this,"警告","CAN复位失败");
        return;
    }
    for (int i = 0; i < 2; ++i) {
        m_canTimestampBaseValid[i] = false;
        m_canTimestampBaseRaw[i] = 0;
    }
    ui->initCANBtn->setEnabled(true);
    ui->StartCANBtn->setEnabled(false);
    ui->reSetCANBtn->setEnabled(false);
    ui->sendBtn->setEnabled(false);
}

void MainWindow::on_sendBtn_clicked()
{
    if(ui->frameTypeCombo->currentIndex() == 0)//标准帧，ID 范围0-0x7FF
    {
        if(ui->sendIDEdit->text().toInt(Q_NULLPTR,16) > 0x7FF)
        {
            QMessageBox::warning(this,"警告","发失败，标准ID范围0~0x7FF");
            return;
        }
    }
    else
    {
        if(ui->sendIDEdit->text().toInt(Q_NULLPTR,16) > 0x1FFFFFFF)
        {
            QMessageBox::warning(this,"警告","发失败，扩展ID范围0~0x1FFFFFFF");
            return;
        }
    }
    QStringList strList = ui->sendDataEdit->text().split(" ");
    char data[64];
    memset(data,0,64);
    for(int i = 0;i < strList.count();i ++)
        data[i] = strList.at(i).toInt(Q_NULLPTR,16);
    UINT dlc = 0;
    if(ui->protocolCombo->currentIndex() == 0)//CAN
        dlc = strList.count() > 8 ? 8 : strList.count();
    else if(ui->protocolCombo->currentIndex() == 1)//CANFD
    {
        if(strList.count() <= 8)
            dlc = strList.count();
        else if(strList.count() <= 12)
            dlc = 12;
        else if(strList.count() <= 16)
            dlc = 16;
        else if(strList.count() <= 20)
            dlc = 20;
        else if(strList.count() <= 24)
            dlc = 24;
        else if(strList.count() <= 32)
            dlc = 32;
        else if(strList.count() <= 48)
            dlc = 48;
        else
            dlc = 64;
    }
    //8.发接收数
    if(canthread->sendData(ui->sendIDEdit->text().toInt(Q_NULLPTR,16),
                           ui->frameTypeCombo->currentIndex(),
                           ui->protocolCombo->currentIndex(),
                           (ui->CANFDaccCheck->checkState() ? 1 : 0),
                           ui->sendPathCombo->currentIndex(),
                           data,dlc))
    {//发成功，打印数据
        QStringList messageList;
        messageList << QTime::currentTime().toString("hh:mm:ss.zzz");//时间
        messageList << QString::number(ui->sendPathCombo->currentIndex());//通道
        messageList << QString::fromUtf8("发送");//收发
        messageList << "0x" + QString::number(ui->sendIDEdit->text().toUInt(nullptr, 16), 16).toUpper();//ID
        messageList << ((ui->frameTypeCombo->currentIndex() == 0) ? "标准" : "扩展");//Frame
        messageList << "数据";//类型
        messageList << QString::number(dlc);//DLC
        messageList << ((ui->protocolCombo->currentIndex() == 0) ? "CAN" : "CANFD");//CAN-FD
        QString str = "";
        for(int j = 0;j < dlc;j ++)
            str += QString("%1 ").arg((unsigned char)data[j],2,16,QChar('0'));//QString::number((unsigned char)data[j],16) + " ";
        messageList << str;//数据
        AddDataToList(messageList);
    }
}

void MainWindow::AddDataToList(QStringList strList)
{
    // 实时保存必须独立于界面刷新状态：
    // 用户滚动表格查看历史数据或关闭“实时显示”时，日志仍然需要完整记录。
    if (m_canFile.isOpen() && strList.count() >= 9) {
        QString logLine = QString("[%1] [通道%2] [%3] ID:%4 %5 帧类型:%6 DLC:%7 协议:%8 数据:%9\n")
                          .arg(strList.at(0))
                          .arg(strList.at(1))
                          .arg(strList.at(2))
                          .arg(strList.at(3))
                          .arg(strList.at(4))
                          .arg(strList.at(5))
                          .arg(strList.at(6))
                          .arg(strList.at(7))
                          .arg(strList.at(8));
        m_canStream << logLine;
    }

    if (ui->checkBox_4->isChecked()) {
        const int maxDisplayRows = 1000;
        const int bottomThreshold = 2;

        // 智能滚动判定：在插入数据前，获取并判断当前滚动条位置
        QScrollBar *vBar = ui->tableWidget->verticalScrollBar();
        bool autoScroll = true;
        if (vBar) {
            autoScroll = (vBar->value() >= vBar->maximum() - bottomThreshold);
        }

        // 用户离开底部时冻结表格显示，便于查看历史数据；
        // CAN 接收解析和文件保存仍在上面继续执行。
        if (!autoScroll) {
            return;
        }

        // 性能优化 1：暂时禁用界面刷新更新，防止写入每个单元格都触发重绘
        ui->tableWidget->setUpdatesEnabled(false);

        int colCount = ui->tableWidget->columnCount();
        int listSize = strList.count();
        int colsToFill = qMin(colCount, listSize);

        int currentRowCount = ui->tableWidget->rowCount();
        int targetRow = currentRowCount;

        // 性能优化 2：若表满 maxDisplayRows 行，则先删首行再复用尾部位置，规避表格无限增长。
        if (currentRowCount >= maxDisplayRows) {
            ui->tableWidget->removeRow(0);
            targetRow = maxDisplayRows - 1;
        }

        // 在尾部插入新空行
        ui->tableWidget->insertRow(targetRow);

        // 容错安全写入单元格
        for (int i = 0; i < colsToFill; ++i) {
            QTableWidgetItem *item = new QTableWidgetItem(strList.at(i), 0);
            if (i != listSize - 1) {
                item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
            } else {
                item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            }
            ui->tableWidget->setItem(targetRow, i, item);
        }

        // 重新启用表格刷新
        ui->tableWidget->setUpdatesEnabled(true);

        // 智能滚动：如果需要滚动且存在滚动条，则滚动到底部
        if (autoScroll && vBar) {
            vBar->setValue(vBar->maximum());
        }
    }

}

// 静故障数结构
struct FaultStaticInfo {
    int id;
    const char* name;
    const char* level;
    const char* action;
};

static const FaultStaticInfo faultTableData[] = {
    {1, "件常复位", "级故", "重新上电解除。查高精度IMU"},
    {2, "过压警告", "二级故障", "查传感器件（内部基准电压电路"},
    {3, "欠压警告", "二级故障", "查传感器件（内部基准电压电路"},
    {4, "件异常位", "二级故障", "重新上电解除。查硬件（时钟"},
    {5, "gyroX量程超限", "三级故障", "数据超过±200°/s，查传感器"},
    {6, "gyroX零偏异常", "三级故障", "静下零偏绝≥5°/h，查硬"},
    {7, "accX数据超限", "三级故障", "数据超过±10g，查传感器"},
    {8, "accX零偏异常", "三级故障", "静下零偏绝≥20mg，查硬"},
    {9, "gyroY数据超限", "三级故障", "数据超过±200°/s，查传感器"},
    {10, "gyroY零偏异常", "三级故障", "静下零偏绝≥5°/h，查硬"},
    {11, "accY数据超限", "三级故障", "数据超过±10g，查传感器"},
    {12, "accY零偏异常", "三级故障", "静下零偏绝≥20mg，查硬"},
    {13, "gyroZ数据超限", "三级故障", "数据超过±200°/s，查传感器"},
    {14, "gyroZ零偏异常", "三级故障", "静下零偏绝≥5°/h，查硬"},
    {15, "accZ数据超限", "三级故障", "数据超过±10g，查传感器"},
    {16, "accZ零偏异常", "三级故障", "静下零偏绝≥20mg，查硬"},
    {17, "gyroX异常", "三级故障", "重新上电或维更换"},
    {18, "gyroY异常", "三级故障", "重新上电或维更换"},
    {19, "gyroZ异常", "三级故障", "重新上电或维更换"},
    {20, "accX异常", "三级故障", "重新上电或维更换"},
    {21, "accY异常", "三级故障", "重新上电或维更换"},
    {22, "accZ异常", "三级故障", "重新上电或维更换"},
    {23, "过压故障", "三级故障", "电压12v，重新上电或维修"},
    {24, "欠压故障", "三级故障", "电压低于5v，重新上电或维修"},
    {25, "标定数据异常", "三级故障", "IMU标定参数失效，重新上电或维修"},
    {26, "IMU接口通信异常", "三级故障", "IMU接口通信超时，重新上电或维修"},
    {27, "时钟异常", "三级故障", "外部时钟切换失效，重新上电或维修"},
    {28, "温度过温异常", "三级故障", "温度高于85℃，重新上电或维"},
    {29, "温度低温异常", "三级故障", "温度低于-41℃，重新上电或维"}
};

void MainWindow::setupIMUDashboard()
{
    // 主布局：左右分栏
    QHBoxLayout *mainLayout = new QHBoxLayout(imuTab);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(15);
    
    // 左栏：动态罗盘和姿态仪控件
    QVBoxLayout *vboxInstruments = new QVBoxLayout();
    vboxInstruments->setSpacing(15);
    
    compassWidget = new CompassWidget(imuTab);
    compassWidget->setRange180(true);
    vboxInstruments->addWidget(compassWidget, 1);

    // 姿态2D图轴向设置：
    // 这里只修正下方姿态仪动画使用的 Roll/Pitch，不改变中间数值显示、协议解析和日志保存。
    // 当 IMU 内部做过轴向变换或安装方向与车辆坐标系不一致时，可用这里手动匹配实物动作方向。
    QGroupBox *boxAttitudeAxis = new QGroupBox(QString::fromUtf8("姿态图轴向设置"), imuTab);
    boxAttitudeAxis->setStyleSheet(
        "QGroupBox { background-color: #2e3440; border: 1px solid #3b4252; border-radius: 6px; margin-top: 12px; color: #88c0d0; font-weight: bold; font-size: 13px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 5px; background-color: #2e3440; color: #88c0d0; }"
        "QLabel { color: #d8dee9; font-size: 12px; }"
        "QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 3px 6px; min-width: 80px; }"
    );
    QGridLayout *gridAttitudeAxis = new QGridLayout(boxAttitudeAxis);
    gridAttitudeAxis->setContentsMargins(10, 18, 10, 8);
    gridAttitudeAxis->setHorizontalSpacing(8);
    gridAttitudeAxis->setVerticalSpacing(6);

    boxAttitudeAxis->setTitle(QString::fromUtf8("图形坐标系设置"));

    QStringList axisOptions;
    axisOptions << QString::fromUtf8("+X") << QString::fromUtf8("-X")
                << QString::fromUtf8("+Y") << QString::fromUtf8("-Y")
                << QString::fromUtf8("+Z") << QString::fromUtf8("-Z");

    cmbDisplayAxisX = new QComboBox(boxAttitudeAxis);
    cmbDisplayAxisX->addItems(axisOptions);
    cmbDisplayAxisY = new QComboBox(boxAttitudeAxis);
    cmbDisplayAxisY->addItems(axisOptions);
    cmbDisplayAxisZ = new QComboBox(boxAttitudeAxis);
    cmbDisplayAxisZ->addItems(axisOptions);

    // 读取上次保存的图形坐标系映射。
    // 该配置只影响本机上位机的罗盘和2D姿态图动画，不会写入IMU，也不会改变原始数据解析和日志。
    QSettings attitudeSettings("SQ_CAN_APP", "AttitudeDisplayConfig");
    int savedAxisX = attitudeSettings.value("axisXIndex", 0).toInt(); // 默认 X = +X
    int savedAxisY = attitudeSettings.value("axisYIndex", 2).toInt(); // 默认 Y = +Y
    int savedAxisZ = attitudeSettings.value("axisZIndex", 4).toInt(); // 默认 Z = +Z
    if (savedAxisX >= 0 && savedAxisX < cmbDisplayAxisX->count()) {
        cmbDisplayAxisX->setCurrentIndex(savedAxisX);
    }
    if (savedAxisY >= 0 && savedAxisY < cmbDisplayAxisY->count()) {
        cmbDisplayAxisY->setCurrentIndex(savedAxisY);
    }
    if (savedAxisZ >= 0 && savedAxisZ < cmbDisplayAxisZ->count()) {
        cmbDisplayAxisZ->setCurrentIndex(savedAxisZ);
    }

    // 用户修改坐标系映射后立即保存，下一次启动软件时自动恢复。
    connect(cmbDisplayAxisX, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int index) {
        QSettings settings("SQ_CAN_APP", "AttitudeDisplayConfig");
        settings.setValue("axisXIndex", index);
    });
    connect(cmbDisplayAxisY, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int index) {
        QSettings settings("SQ_CAN_APP", "AttitudeDisplayConfig");
        settings.setValue("axisYIndex", index);
    });
    connect(cmbDisplayAxisZ, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int index) {
        QSettings settings("SQ_CAN_APP", "AttitudeDisplayConfig");
        settings.setValue("axisZIndex", index);
    });

    QWidget *instrumentOptionRow = new QWidget(imuTab);
    QHBoxLayout *instrumentOptionLayout = new QHBoxLayout(instrumentOptionRow);
    instrumentOptionLayout->setContentsMargins(0, 0, 0, 0);
    instrumentOptionLayout->setSpacing(8);

    gridAttitudeAxis->addWidget(new QLabel(QString::fromUtf8("X ="), boxAttitudeAxis), 0, 0);
    gridAttitudeAxis->addWidget(cmbDisplayAxisX, 0, 1);
    gridAttitudeAxis->addWidget(new QLabel(QString::fromUtf8("Y ="), boxAttitudeAxis), 1, 0);
    gridAttitudeAxis->addWidget(cmbDisplayAxisY, 1, 1);
    gridAttitudeAxis->addWidget(new QLabel(QString::fromUtf8("Z ="), boxAttitudeAxis), 2, 0);
    gridAttitudeAxis->addWidget(cmbDisplayAxisZ, 2, 1);

    QGroupBox *boxDataRecord = new QGroupBox(QString::fromUtf8("数据记录"), imuTab);
    boxDataRecord->setStyleSheet(
        "QGroupBox { background-color: #2e3440; border: 1px solid #3b4252; border-radius: 6px; margin-top: 12px; color: #88c0d0; font-weight: bold; font-size: 13px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 5px; background-color: #2e3440; color: #88c0d0; }"
        "QCheckBox { color: #eceff4; font-size: 13px; font-weight: bold; }"
    );
    QVBoxLayout *layDataRecord = new QVBoxLayout(boxDataRecord);
    layDataRecord->setContentsMargins(10, 18, 10, 8);
    chkSaveParsedIMU = new QCheckBox(QString::fromUtf8("保存解析数据"), boxDataRecord);
    connect(chkSaveParsedIMU, &QCheckBox::stateChanged, this, &MainWindow::onSaveParsedIMUStateChanged);
    layDataRecord->addWidget(chkSaveParsedIMU);
    layDataRecord->addStretch(1);

    instrumentOptionLayout->addWidget(boxAttitudeAxis, 2);
    instrumentOptionLayout->addWidget(boxDataRecord, 1);
    vboxInstruments->addWidget(instrumentOptionRow, 0);
    
    attitudeWidget = new AttitudeIndicatorWidget(imuTab);
    vboxInstruments->addWidget(attitudeWidget, 1);
    
    mainLayout->addLayout(vboxInstruments, 2); // 占比 2
    
    // 中栏：姿态与动态数据 (原左栏)
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(10);
    
    // 统一卡片样式表 (Nord 暗系风格)
    QString cardStyle = 
        "QGroupBox { background-color: #2e3440; border: 2px solid #3b4252; border-radius: 8px; margin-top: 15px; color: #88c0d0; font-weight: bold; font-size: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 5px; background-color: #2e3440; color: #88c0d0; }"
        "QLabel { color: #d8dee9; font-size: 15px; font-weight: 500; }"; // 强制卡片内所有默认文本为浅灰色
    
    // 覆盖样式表 (直接写属性，不写类选择器，防Qt解析失效)
    QString valStyle = "color: #8fbcbb; font-size: 34px; font-weight: bold; font-family: 'Consolas', 'Courier New', monospace;";
    QString unitStyle = "color: #eceff4; font-size: 16px; font-weight: bold;";
    
    // 1. 角度卡片 (Roll, Pitch, Yaw)
    QGroupBox *boxAng = new QGroupBox(QString::fromUtf8("1. 实时姿态角度 (Intel 格式)"), imuTab);
    boxAng->setStyleSheet(cardStyle);
    QGridLayout *gridAng = new QGridLayout(boxAng);
    
    gridAng->addWidget(new QLabel(QString::fromUtf8("翻滚角 (Roll)"), boxAng), 0, 0);
    valRoll = new QLabel("0.00", boxAng);
    valRoll->setStyleSheet(valStyle);
    gridAng->addWidget(valRoll, 0, 1);
    
    QLabel *unitRoll = new QLabel(QString::fromUtf8("°"), boxAng);
    unitRoll->setStyleSheet(unitStyle);
    gridAng->addWidget(unitRoll, 0, 2);
    
    barRoll = new QProgressBar(boxAng);
    barRoll->setRange(-180, 180);
    barRoll->setValue(0);
    barRoll->setTextVisible(false);
    barRoll->setStyleSheet("QProgressBar { border: 1px solid #4c566a; border-radius: 3px; background: #3b4252; height: 10px; }"
                          "QProgressBar::chunk { background: #88c0d0; }");
    gridAng->addWidget(barRoll, 0, 3);
    
    gridAng->addWidget(new QLabel(QString::fromUtf8("俯仰角 (Pitch)"), boxAng), 1, 0);
    valPitch = new QLabel("0.00", boxAng);
    valPitch->setStyleSheet(valStyle);
    gridAng->addWidget(valPitch, 1, 1);
    
    QLabel *unitPitch = new QLabel(QString::fromUtf8("°"), boxAng);
    unitPitch->setStyleSheet(unitStyle);
    gridAng->addWidget(unitPitch, 1, 2);
    
    barPitch = new QProgressBar(boxAng);
    barPitch->setRange(-90, 90);
    barPitch->setValue(0);
    barPitch->setTextVisible(false);
    barPitch->setStyleSheet("QProgressBar { border: 1px solid #4c566a; border-radius: 3px; background: #3b4252; height: 10px; }"
                           "QProgressBar::chunk { background: #a3be8c; }");
    gridAng->addWidget(barPitch, 1, 3);
    
    gridAng->addWidget(new QLabel(QString::fromUtf8("偏航角 (Yaw)"), boxAng), 2, 0);
    valYaw = new QLabel("0.00", boxAng);
    valYaw->setStyleSheet(valStyle);
    gridAng->addWidget(valYaw, 2, 1);
    
    QLabel *unitYaw = new QLabel(QString::fromUtf8("°"), boxAng);
    unitYaw->setStyleSheet(unitStyle);
    gridAng->addWidget(unitYaw, 2, 2);
    
    barYaw = new QProgressBar(boxAng);
    barYaw->setRange(-180, 180);
    barYaw->setValue(0);
    barYaw->setTextVisible(false);
    barYaw->setStyleSheet("QProgressBar { border: 1px solid #4c566a; border-radius: 3px; background: #3b4252; height: 10px; }"
                         "QProgressBar::chunk { background: #ebcb8b; }");
    gridAng->addWidget(barYaw, 2, 3);

    leftLayout->addWidget(boxAng);
    
    // 2. 加速度卡片
    QGroupBox *boxAcc = new QGroupBox(QString::fromUtf8("2. 三轴加速度 (g)"), imuTab);
    boxAcc->setStyleSheet(cardStyle);
    QGridLayout *gridAcc = new QGridLayout(boxAcc);
    
    gridAcc->addWidget(new QLabel(QString::fromUtf8("X轴加速度:"), boxAcc), 0, 0);
    valAx = new QLabel("0.0000", boxAcc);
    valAx->setStyleSheet(valStyle);
    gridAcc->addWidget(valAx, 0, 1);
    QLabel *unitAx = new QLabel("g", boxAcc);
    unitAx->setStyleSheet(unitStyle);
    gridAcc->addWidget(unitAx, 0, 2);
    
    gridAcc->addWidget(new QLabel(QString::fromUtf8("Y轴加速度:"), boxAcc), 1, 0);
    valAy = new QLabel("0.0000", boxAcc);
    valAy->setStyleSheet(valStyle);
    gridAcc->addWidget(valAy, 1, 1);
    QLabel *unitAy = new QLabel("g", boxAcc);
    unitAy->setStyleSheet(unitStyle);
    gridAcc->addWidget(unitAy, 1, 2);
    
    gridAcc->addWidget(new QLabel(QString::fromUtf8("Z轴加速度:"), boxAcc), 2, 0);
    valAz = new QLabel("0.0000", boxAcc);
    valAz->setStyleSheet(valStyle);
    gridAcc->addWidget(valAz, 2, 1);
    QLabel *unitAz = new QLabel("g", boxAcc);
    unitAz->setStyleSheet(unitStyle);
    gridAcc->addWidget(unitAz, 2, 2);
    
    leftLayout->addWidget(boxAcc);
    
    // 3. 角速度卡片
    QGroupBox *boxGyro = new QGroupBox(QString::fromUtf8("3. 三轴角速度 (°/s)"), imuTab);
    boxGyro->setStyleSheet(cardStyle);
    QGridLayout *gridGyro = new QGridLayout(boxGyro);
    
    gridGyro->addWidget(new QLabel(QString::fromUtf8("X轴角速度:"), boxGyro), 0, 0);
    valGx = new QLabel("0.00", boxGyro);
    valGx->setStyleSheet(valStyle);
    gridGyro->addWidget(valGx, 0, 1);
    QLabel *unitGx = new QLabel("°/s", boxGyro);
    unitGx->setStyleSheet(unitStyle);
    gridGyro->addWidget(unitGx, 0, 2);
    
    gridGyro->addWidget(new QLabel(QString::fromUtf8("Y轴角速度:"), boxGyro), 1, 0);
    valGy = new QLabel("0.00", boxGyro);
    valGy->setStyleSheet(valStyle);
    gridGyro->addWidget(valGy, 1, 1);
    QLabel *unitGy = new QLabel("°/s", boxGyro);
    unitGy->setStyleSheet(unitStyle);
    gridGyro->addWidget(unitGy, 1, 2);
    
    gridGyro->addWidget(new QLabel(QString::fromUtf8("Z轴角速度:"), boxGyro), 2, 0);
    valGz = new QLabel("0.00", boxGyro);
    valGz->setStyleSheet(valStyle);
    gridGyro->addWidget(valGz, 2, 1);
    QLabel *unitGz = new QLabel("°/s", boxGyro);
    unitGz->setStyleSheet(unitStyle);
    gridGyro->addWidget(unitGz, 2, 2);
    
    leftLayout->addWidget(boxGyro);
    
    // 4. 设备信息 (完整展示 6 个协议字段)
    QGroupBox *boxVer = new QGroupBox(QString::fromUtf8("4. 传感器设备信息"), imuTab);
    boxVer->setStyleSheet(cardStyle);
    QGridLayout *gridVer = new QGridLayout(boxVer);
    gridVer->setVerticalSpacing(6);
    gridVer->setHorizontalSpacing(15);
    
    gridVer->addWidget(new QLabel(QString::fromUtf8("固件版本:"), boxVer), 0, 0);
    valVer = new QLabel("V0.0.0", boxVer);
    valVer->setStyleSheet("color: #eceff4; font-weight: bold; font-size: 16px;");
    gridVer->addWidget(valVer, 0, 1);
    
    gridVer->addWidget(new QLabel(QString::fromUtf8("内部版本号:"), boxVer), 1, 0);
    valInternalVer = new QLabel("0", boxVer);
    valInternalVer->setStyleSheet("color: #eceff4; font-weight: bold; font-size: 16px;");
    gridVer->addWidget(valInternalVer, 1, 1);
    
    gridVer->addWidget(new QLabel(QString::fromUtf8("版本类别:"), boxVer), 2, 0);
    valVerType = new QLabel("0", boxVer);
    valVerType->setStyleSheet("color: #eceff4; font-weight: bold; font-size: 16px;");
    gridVer->addWidget(valVerType, 2, 1);
    
    gridVer->addWidget(new QLabel(QString::fromUtf8("车型平台:"), boxVer), 3, 0);
    valPlatformType = new QLabel("3", boxVer);
    valPlatformType->setStyleSheet("color: #eceff4; font-weight: bold; font-size: 16px;");
    gridVer->addWidget(valPlatformType, 3, 1);
    
    gridVer->addWidget(new QLabel(QString::fromUtf8("平台变型号:"), boxVer), 4, 0);
    valPlatformVarNo = new QLabel("0", boxVer);
    valPlatformVarNo->setStyleSheet("color: #eceff4; font-weight: bold; font-size: 16px;");
    gridVer->addWidget(valPlatformVarNo, 4, 1);
    
    gridVer->addWidget(new QLabel(QString::fromUtf8("发布日期:"), boxVer), 5, 0);
    valDate = new QLabel("2000-01-01", boxVer);
    valDate->setStyleSheet("color: #eceff4; font-weight: bold; font-size: 16px;");
    gridVer->addWidget(valDate, 5, 1);
    
    leftLayout->addWidget(boxVer);
    
    mainLayout->addLayout(leftLayout, 2); // 占比 2
    
    // 右栏：故障反馈 (占比 3)
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);
    QGroupBox *boxStatus = new QGroupBox(QString::fromUtf8("当前系统健康状态"), imuTab);
    boxStatus->setStyleSheet(cardStyle);
    QHBoxLayout *layStatus = new QHBoxLayout(boxStatus);
    valFaultLevel = new QLabel(QString::fromUtf8("数据未就绪"), boxStatus);
    valFaultLevel->setAlignment(Qt::AlignCenter);
    valFaultLevel->setStyleSheet(
        "font-size: 18px; font-weight: bold; background-color: #434c5e; color: #d8dee9; border-radius: 6px; padding: 15px;"
    );
    layStatus->addWidget(valFaultLevel);
    rightLayout->addWidget(boxStatus);
    
    QGroupBox *boxFaults = new QGroupBox(QString::fromUtf8("故障诊断矩阵 (Active Fault Matrix)"), imuTab);
    boxFaults->setStyleSheet(cardStyle);
    QVBoxLayout *layFaults = new QVBoxLayout(boxFaults);
    
    faultTableWidget = new QTableWidget(boxFaults);
    faultTableWidget->setColumnCount(4);
    faultTableWidget->setHorizontalHeaderLabels(QStringList() << QString::fromUtf8("ID") << QString::fromUtf8("故障名称") << QString::fromUtf8("故障等级") << QString::fromUtf8("状态 / 排除措施"));
    faultTableWidget->setRowCount(29);
    faultTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    faultTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    faultTableWidget->horizontalHeader()->setStretchLastSection(true);
    
    faultTableWidget->setStyleSheet(
        "QTableWidget { background-color: #2e3440; gridline-color: #3b4252; color: #eceff4; font-size: 14px; }"
        "QHeaderView { background-color: #2e3440; }"
        "QHeaderView::section { background-color: #3b4252; color: #d8dee9; padding: 6px; border: 1px solid #2e3440; font-weight: bold; font-size: 13px; }"
    );
    
    for (int i = 0; i < 29; ++i) {
        QTableWidgetItem *item0 = new QTableWidgetItem(QString::number(faultTableData[i].id));
        QTableWidgetItem *item1 = new QTableWidgetItem(QString::fromUtf8(faultTableData[i].name));
        QTableWidgetItem *item2 = new QTableWidgetItem(QString::fromUtf8(faultTableData[i].level));
        QTableWidgetItem *item3 = new QTableWidgetItem(QString::fromUtf8("检测中..."));
        
        item0->setTextAlignment(Qt::AlignCenter);
        item2->setTextAlignment(Qt::AlignCenter);
        item3->setTextAlignment(Qt::AlignCenter);
        
        faultTableWidget->setItem(i, 0, item0);
        faultTableWidget->setItem(i, 1, item1);
        faultTableWidget->setItem(i, 2, item2);
        faultTableWidget->setItem(i, 3, item3);
        
        for (int c = 0; c < 4; ++c) {
            faultTableWidget->item(i, c)->setBackground(QBrush(QColor("#3b4252")));
            faultTableWidget->item(i, c)->setForeground(QBrush(QColor("#abb2bf")));
        }
    }
    
    layFaults->addWidget(faultTableWidget);
    rightLayout->addWidget(boxFaults, 3);
    
    mainLayout->addLayout(rightLayout, 2);
}

void MainWindow::convertDisplayAttitude(double roll, double pitch, double yaw,
                                        double &displayRoll, double &displayPitch, double &displayYaw)
{
    // 默认直接输出原始姿态，任何非法配置或控件未初始化时都保持图形稳定。
    displayRoll = roll;
    displayPitch = pitch;
    displayYaw = yaw;

    if (!cmbDisplayAxisX || !cmbDisplayAxisY || !cmbDisplayAxisZ) {
        return;
    }

    auto axisIndex = [](int comboIndex) -> int {
        return comboIndex / 2; // 0/1=X, 2/3=Y, 4/5=Z
    };
    auto axisSign = [](int comboIndex) -> double {
        return (comboIndex % 2 == 0) ? 1.0 : -1.0;
    };

    int axisSel[3] = {
        cmbDisplayAxisX->currentIndex(),
        cmbDisplayAxisY->currentIndex(),
        cmbDisplayAxisZ->currentIndex()
    };

    // 容错：下拉框索引越界或重复使用同一个原始轴时，坐标系不成立，直接保持原始显示。
    bool axisUsed[3] = { false, false, false };
    for (int row = 0; row < 3; ++row) {
        if (axisSel[row] < 0 || axisSel[row] > 5) {
            return;
        }
        int srcAxis = axisIndex(axisSel[row]);
        if (srcAxis < 0 || srcAxis > 2 || axisUsed[srcAxis]) {
            return;
        }
        axisUsed[srcAxis] = true;
    }

    double cr = qCos(qDegreesToRadians(roll));
    double sr = qSin(qDegreesToRadians(roll));
    double cp = qCos(qDegreesToRadians(pitch));
    double sp = qSin(qDegreesToRadians(pitch));
    double cy = qCos(qDegreesToRadians(yaw));
    double sy = qSin(qDegreesToRadians(yaw));

    // ZYX 欧拉角顺序：R = Rz(yaw) * Ry(pitch) * Rx(roll)。
    // 三个 Tab 都通过同一个矩阵转换，保证罗盘和2D姿态图的补偿行为一致。
    double r[3][3] = {
        { cy * cp, cy * sp * sr - sy * cr, cy * sp * cr + sy * sr },
        { sy * cp, sy * sp * sr + cy * cr, sy * sp * cr - cy * sr },
        { -sp,     cp * sr,                cp * cr }
    };

    double p[3][3] = {};
    for (int row = 0; row < 3; ++row) {
        p[row][axisIndex(axisSel[row])] = axisSign(axisSel[row]);
    }

    double pr[3][3] = {};
    double displayMatrix[3][3] = {};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                pr[i][j] += p[i][k] * r[k][j];
            }
        }
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                displayMatrix[i][j] += pr[i][k] * p[j][k]; // P * R * P^T
            }
        }
    }

    double pitchRad = qAsin(qBound(-1.0, -displayMatrix[2][0], 1.0));
    double rollRad = qAtan2(displayMatrix[2][1], displayMatrix[2][2]);
    double yawRad = qAtan2(displayMatrix[1][0], displayMatrix[0][0]);
    displayRoll = qRadiansToDegrees(rollRad);
    displayPitch = qRadiansToDegrees(pitchRad);
    displayYaw = qRadiansToDegrees(yawRad);
}


void MainWindow::refreshIMUDashboard()
{
    // 获取新解析的数据
    IMUData data = IMUParser::getInstance()->getIMUData();
    
    // 1. 刷新角度
    valRoll->setText(QString::number(data.roll, 'f', 3));
    valPitch->setText(QString::number(data.pitch, 'f', 3));
    valYaw->setText(QString::number(data.yaw, 'f', 3));
    
    barRoll->setValue(qBound(-180, (int)data.roll, 180));
    barPitch->setValue(qBound(-90, (int)data.pitch, 90));
    barYaw->setValue(qBound(-180, (int)data.yaw, 180));

    // 图形坐标系显示补偿：
    // 原始 Roll/Pitch/Yaw 数值仍按 IMU 报文直接显示；这里只把姿态矩阵转换到用户选择的显示坐标系，
    // 再反算出图形控件使用的 Roll/Pitch/Yaw，避免 IMU 轴向配置变化后动画方向与实物不一致。
    double displayRoll = 0.0;
    double displayPitch = 0.0;
    double displayYaw = 0.0;
    convertDisplayAttitude(data.roll, data.pitch, data.yaw, displayRoll, displayPitch, displayYaw);

    compassWidget->setYaw(displayYaw);
    attitudeWidget->setRollPitch(displayRoll, displayPitch);
    
    // 2. 刷新加度
    valAx->setText(QString::number(data.ax, 'f', 4));
    valAy->setText(QString::number(data.ay, 'f', 4));
    valAz->setText(QString::number(data.az, 'f', 4));
    
    // 3. 刷新角度
    valGx->setText(QString::number(data.gx, 'f', 2));
    valGy->setText(QString::number(data.gy, 'f', 2));
    valGz->setText(QString::number(data.gz, 'f', 2));
    
    // 4. 刷新版本信息
    valVer->setText(data.softwareVersion);
    valInternalVer->setText(QString::number(data.internalVersion));
    valVerType->setText(QString::number(data.versionType));
    valPlatformType->setText(QString::number(data.platformType));
    valPlatformVarNo->setText(QString::number(data.platformVarNo));
    valDate->setText(data.releaseDate);
    
    // 5. 刷新总体故障等级
    switch (data.faultLevel) {
    case 0:
        valFaultLevel->setText(QString::fromUtf8("完全正常 (HEALTHY)"));
        valFaultLevel->setStyleSheet("QLabel { font-size: 18px; font-weight: bold; background-color: #a3be8c; color: #2e3440; border-radius: 6px; padding: 15px; }");
        break;
    case 1:
        valFaultLevel->setText(QString::fromUtf8("轻微故障 (MILD FAULT)"));
        valFaultLevel->setStyleSheet("QLabel { font-size: 18px; font-weight: bold; background-color: #ebcb8b; color: #2e3440; border-radius: 6px; padding: 15px; }");
        break;
    case 2:
        valFaultLevel->setText("故障 (MODERATE FAULT)");
        valFaultLevel->setStyleSheet("QLabel { font-size: 18px; font-weight: bold; background-color: #d08770; color: #eceff4; border-radius: 6px; padding: 15px; }");
        break;
    case 3:
        valFaultLevel->setText(QString::fromUtf8("严重故障 (SEVERE FAULT - DATA UNTRUSTED)"));
        valFaultLevel->setStyleSheet("QLabel { font-size: 18px; font-weight: bold; background-color: #bf616a; color: #eceff4; border-radius: 6px; padding: 15px; }");
        break;
    default:
        break;
    }
    
    // 6. 刷新 29 项具体故障的状
    for (int i = 0; i < 29; ++i) {
        int byteIdx = (i) / 8;
        int bitIdx = (i) % 8;
        bool isTriggered = (data.faultBytes[byteIdx] & (1 << bitIdx)) != 0;
        
        QTableWidgetItem *statusItem = faultTableWidget->item(i, 3);
        if (isTriggered) {
            statusItem->setText("已触! " + QString(faultTableData[i].action));
            // 触发后置为红底黑/白字
            for (int c = 0; c < 4; ++c) {
                faultTableWidget->item(i, c)->setBackground(QBrush(QColor("#bf616a")));
                faultTableWidget->item(i, c)->setForeground(QBrush(QColor("#eceff4")));
            }
        } else {
            statusItem->setText(QString::fromUtf8("正常"));
            // 正常时置为淡绿底暗色字，非活动项则显示清爽的灰底白字
            for (int c = 0; c < 4; ++c) {
                faultTableWidget->item(i, c)->setBackground(QBrush(QColor("#4c566a")));
                faultTableWidget->item(i, c)->setForeground(QBrush(QColor("#eceff4")));
            }
        }
    }

    // 7. 原始数据透传 Tab 数据刷新
    const int maxRawPacketsPerRefresh = 200;
    QList<IMURawPacket> rawPackets = IMUParser::getInstance()->takeIMURawPackets(maxRawPacketsPerRefresh);
    if (!rawPackets.isEmpty()) {
        const IMURawPacket &latestRawPacket = rawPackets.last();
        m_lastRawTime = QTime::currentTime();
        lblRawStatus->setText(QString::fromUtf8("透传状态: RUNNING"));
        lblRawStatus->setStyleSheet("color: #2e3440; font-size: 13px; font-weight: bold; background-color: #a3be8c; padding: 6px 15px; border-radius: 4px;");

        // 写入 Hex 日志已被移除以节省 CPU 消耗

        // 获取解算字段数据
        IMURawData rawData = latestRawPacket.decodedData;

        // 刷新高精度小仪表盘
        double rawDisplayRoll = 0.0;
        double rawDisplayPitch = 0.0;
        double rawDisplayYaw = 0.0;
        convertDisplayAttitude(rawData.roll, rawData.pitch, rawData.yaw,
                               rawDisplayRoll, rawDisplayPitch, rawDisplayYaw);
        rawCompassWidget->setYaw(rawDisplayYaw);
        rawAttitudeWidget->setRollPitch(rawDisplayRoll, rawDisplayPitch);

        // 刷新统计看板
        unsigned int total = 0, success = 0, failed = 0, dropped = 0;
        IMUParser::getInstance()->getIMURawStats(total, success, failed, dropped);
        lblRawStats->setText(QString(QString::fromUtf8("包计数 - 已解析: %1 | 校验成功: %2 | 校验失败: %3"))
                             .arg(total).arg(success).arg(failed));

        // 更新数据表格内容
        rawTableWidget->item(0, 2)->setText(QString::number(rawData.pcNum * 0.005, 'f', 2)); // pcNum 为 5ms 周期计数
        lblRawStats->setText(QString("Packets: %1 | OK: %2 | Failed: %3 | Dropped: %4")
                             .arg(total).arg(success).arg(failed).arg(dropped));
        if (dropped > 0) {
            lblRawStats->setStyleSheet("color: #ebcb8b; font-size: 12px;");
        } else {
            lblRawStats->setStyleSheet("color: #88c0d0; font-size: 12px;");
        }

        rawTableWidget->item(0, 6)->setText(QString("%1 / %2").arg(rawData.totalAlignTime).arg(rawData.remainAlignTime));
        rawTableWidget->item(0, 10)->setText(QString(QString::fromUtf8("陀螺: %1℃ / 加表: %2℃"))
                                             .arg((int)rawData.gyroTemp).arg((int)rawData.accelTemp));

        QString sysStateStr;
        switch (rawData.sysStatus) {
            case 0x10: sysStateStr = QString::fromUtf8("系统自检中"); break;
            case 0x12: sysStateStr = QString::fromUtf8("自检好"); break;
            case 0x1F: sysStateStr = QString::fromUtf8("自检错误"); break;
            case 0x33: sysStateStr = QString::fromUtf8("粗对准"); break;
            case 0x35: sysStateStr = QString::fromUtf8("精对准"); break;
            case 0x41: sysStateStr = QString::fromUtf8("导航中"); break;
            case 0x43: sysStateStr = QString::fromUtf8("零速修正"); break;
            default: sysStateStr = QString::fromUtf8("未定义"); break;
        }
        rawTableWidget->item(1, 1)->setText(QString("系统: %1(0x%2) | 状态: 0x%3")
                                            .arg(sysStateStr)
                                            .arg(QString::number(rawData.sysStatus, 16).toUpper().rightJustified(2, '0'))
                                            .arg(QString::number(rawData.rtState, 16).toUpper().rightJustified(2, '0')));

        int orVal = rawData.overRangeStatus;
        QString orStr = QString("0b%1%2%3%4%5%6")
                        .arg(orVal & 0x20 ? 1 : 0)
                        .arg(orVal & 0x10 ? 1 : 0)
                        .arg(orVal & 0x08 ? 1 : 0)
                        .arg(orVal & 0x04 ? 1 : 0)
                        .arg(orVal & 0x02 ? 1 : 0)
                        .arg(orVal & 0x01 ? 1 : 0);
        rawTableWidget->item(1, 4)->setText(QString::fromUtf8("超量程: ") + orStr);
        if (orVal > 0) {
            rawTableWidget->item(1, 4)->setForeground(QBrush(QColor("#bf616a"))); // 超限红色提示
        } else {
            rawTableWidget->item(1, 4)->setForeground(QBrush(QColor("#eceff4")));
        }

        rawTableWidget->item(1, 7)->setText(QString::fromUtf8("通道: 0"));

        QString ackStr;
        if (rawData.cmdAck == 0x10) {
            ackStr = QString::fromUtf8("成功(0x10)");
        } else if (rawData.cmdAck == 0x1F) {
            ackStr = QString::fromUtf8("报错(0x1F)");
        } else {
            ackStr = QString::number(rawData.cmdAck);
        }
        rawTableWidget->item(1, 10)->setText(QString::fromUtf8("回令: ") + ackStr);

        rawTableWidget->item(14, 2)->setText(rawData.outputFormat == 0xFF ? QString::fromUtf8("通用石油寻北") : QString::fromUtf8("通用寻北协议"));

        // 原始角速度
        rawTableWidget->item(3, 1)->setText(QString("X: %1").arg(QString::number(rawData.gx, 'f', 6)));
        rawTableWidget->item(3, 5)->setText(QString("Y: %1").arg(QString::number(rawData.gy, 'f', 6)));
        rawTableWidget->item(3, 9)->setText(QString("Z: %1").arg(QString::number(rawData.gz, 'f', 6)));

        // 原始加速度
        rawTableWidget->item(4, 1)->setText(QString("X: %1").arg(QString::number(rawData.ax, 'f', 6)));
        rawTableWidget->item(4, 5)->setText(QString("Y: %1").arg(QString::number(rawData.ay, 'f', 6)));
        rawTableWidget->item(4, 9)->setText(QString("Z: %1").arg(QString::number(rawData.az, 'f', 6)));

        // 高精度姿态角
        rawTableWidget->item(5, 1)->setText(QString("Roll: %1").arg(QString::number(rawData.roll, 'f', 3)));
        rawTableWidget->item(5, 5)->setText(QString("Heading: %1").arg(QString::number(rawData.yaw, 'f', 3)));
        rawTableWidget->item(5, 9)->setText(QString("Pitch: %1").arg(QString::number(rawData.pitch, 'f', 3)));

        // 反欧拉角
        rawTableWidget->item(6, 1)->setText(QString("Roll: %1").arg(QString::number(rawData.reverseRoll, 'f', 3)));
        rawTableWidget->item(6, 4)->setText(QString("Heading: %1").arg(QString::number(rawData.reverseYaw, 'f', 3)));
        rawTableWidget->item(6, 7)->setText(QString("Pitch: %1").arg(QString::number(rawData.reversePitch, 'f', 3)));
        rawTableWidget->item(6, 10)->setText(QString("Flag: %1").arg(rawData.verticalFlag));

        // 四元数
        rawTableWidget->item(7, 1)->setText(QString("Q1: %1").arg(QString::number(rawData.q[0], 'f', 6)));
        rawTableWidget->item(7, 4)->setText(QString("Q2: %1").arg(QString::number(rawData.q[1], 'f', 6)));
        rawTableWidget->item(7, 7)->setText(QString("Q3: %1").arg(QString::number(rawData.q[2], 'f', 6)));
        rawTableWidget->item(7, 10)->setText(QString("Q4: %1").arg(QString::number(rawData.q[3], 'f', 6)));

        // 经纬高度
        rawTableWidget->item(9, 1)->setText(QString(QString::fromUtf8("纬度: %1")).arg(QString::number(rawData.latitude, 'f', 8)));
        rawTableWidget->item(9, 5)->setText(QString(QString::fromUtf8("经度: %1")).arg(QString::number(rawData.longitude, 'f', 8)));
        rawTableWidget->item(9, 9)->setText(QString(QString::fromUtf8("高程: %1m")).arg(QString::number(rawData.altitude, 'f', 2)));

        // 导航速度
        rawTableWidget->item(10, 1)->setText(QString(QString::fromUtf8("北: %1")).arg(QString::number(rawData.velNorth, 'f', 4)));
        rawTableWidget->item(10, 5)->setText(QString(QString::fromUtf8("天: %1")).arg(QString::number(rawData.velUp, 'f', 4)));
        rawTableWidget->item(10, 9)->setText(QString(QString::fromUtf8("东: %1")).arg(QString::number(rawData.velEast, 'f', 4)));

        // 超限计数
        rawTableWidget->item(12, 1)->setText(QString("Gx: %1").arg(rawData.imuOverRangeCounters[0]));
        rawTableWidget->item(12, 5)->setText(QString("Gy: %1").arg(rawData.imuOverRangeCounters[1]));
        rawTableWidget->item(12, 9)->setText(QString("Gz: %1").arg(rawData.imuOverRangeCounters[2]));

        rawTableWidget->item(13, 1)->setText(QString("Ax: %1").arg(rawData.imuOverRangeCounters[3]));
        rawTableWidget->item(13, 5)->setText(QString("Ay: %1").arg(rawData.imuOverRangeCounters[4]));
        rawTableWidget->item(13, 9)->setText(QString("Az: %1").arg(rawData.imuOverRangeCounters[5]));

        rawTableWidget->item(14, 6)->setText(rawData.verticalFlag == 1 ? QString::fromUtf8("垂向模式") : QString::fromUtf8("水平模式"));
        rawTableWidget->item(14, 11)->setText(QString::number(rawData.pcNum));

        // 实时保存数据到本地文件 (TXT 文本和 BIN 二进制双存)
        for (const IMURawPacket &packet : rawPackets) {
            QString packetTimeText = packet.receiveTimeText;
            if (packetTimeText.isEmpty()) {
                packetTimeText = QTime::currentTime().toString("hh:mm:ss.zzz");
            }
            if (m_rawFile.isOpen()) {
                QString logLine = QString("[%1] %2\n").arg(packetTimeText).arg(packet.hexText);
                m_rawStream << logLine;
            }
            if (m_rawBinFile.isOpen()) {
                m_rawBinFile.write(packet.rawBytes);
            }
        }
    }
}

void MainWindow::setupRawTab()
{
    // Symmetric layout: mainRawLayout is a QHBoxLayout
    QHBoxLayout *mainRawLayout = new QHBoxLayout(rawTab);
    mainRawLayout->setContentsMargins(10, 10, 10, 10);
    mainRawLayout->setSpacing(10);

    // Card style (dark theme)
    QString cardStyle = 
        "QGroupBox { background-color: #2e3440; border: 2px solid #3b4252; border-radius: 8px; margin-top: 10px; color: #88c0d0; font-weight: bold; font-size: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 5px; background-color: #2e3440; color: #88c0d0; }";

    // ---------------- Left Panel: Control, Gauges & Command ----------------
    QWidget *leftPanel = new QWidget(rawTab);
    QVBoxLayout *layLeft = new QVBoxLayout(leftPanel);
    layLeft->setContentsMargins(0, 0, 0, 0);
    layLeft->setSpacing(10);

    // 1. Control GroupBox
    QGroupBox *boxCtrl = new QGroupBox(QString::fromUtf8("透传控制与运行状态"), leftPanel);
    boxCtrl->setStyleSheet(cardStyle);
    QVBoxLayout *layCtrl = new QVBoxLayout(boxCtrl);
    layCtrl->setContentsMargins(15, 20, 15, 15);
    layCtrl->setSpacing(10);

    btnStartRawMode = new QPushButton(QString::fromUtf8("开启透传模式"), boxCtrl);
    btnStartRawMode->setStyleSheet("QPushButton { background-color: #a3be8c; color: #2e3440; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
                                   "QPushButton:hover { background-color: #b5cca3; }");
    btnStopRawMode = new QPushButton(QString::fromUtf8("关闭透传模式"), boxCtrl);
    btnStopRawMode->setStyleSheet("QPushButton { background-color: #bf616a; color: #eceff4; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
                                  "QPushButton:hover { background-color: #c9767e; }");

    lblRawStatus = new QLabel(QString::fromUtf8("透传状态: IDLE"), boxCtrl);
    lblRawStatus->setStyleSheet("QLabel { color: #d8dee9; font-size: 13px; font-weight: bold; background-color: #434c5e; padding: 6px 15px; border-radius: 4px; }");
    lblRawStatus->setAlignment(Qt::AlignCenter);

    lblRawStats = new QLabel(QString::fromUtf8("包计数 - 已解析: 0 | 校验成功: 0 | 校验失败: 0"), boxCtrl);
    lblRawStats->setStyleSheet("QLabel { color: #8fbcbb; font-size: 13px; font-weight: bold; }");

    chkSaveRaw = new QCheckBox(QString::fromUtf8("保存透传数据"), boxCtrl);
    chkSaveRaw->setStyleSheet("QCheckBox { color: #eceff4; font-size: 13px; font-weight: bold; }");

    QHBoxLayout *layCtrlRow1 = new QHBoxLayout();
    layCtrlRow1->addWidget(btnStartRawMode);
    layCtrlRow1->addWidget(btnStopRawMode);
    layCtrlRow1->addWidget(lblRawStatus);
    
    QHBoxLayout *layCtrlRow2 = new QHBoxLayout();
    layCtrlRow2->addWidget(lblRawStats);
    layCtrlRow2->addStretch(1);
    layCtrlRow2->addWidget(chkSaveRaw);

    layCtrl->addLayout(layCtrlRow1);
    layCtrl->addLayout(layCtrlRow2);

    layLeft->addWidget(boxCtrl);

    // 2. Gauges (Placed below status control area)
    QHBoxLayout *layGauges = new QHBoxLayout();
    layGauges->setSpacing(15);
    rawCompassWidget = new CompassWidget(rawTab);
    rawCompassWidget->setFixedSize(180, 180);
    rawAttitudeWidget = new AttitudeIndicatorWidget(rawTab);
    rawAttitudeWidget->setFixedSize(180, 180);
    layGauges->addStretch(1);
    layGauges->addWidget(rawCompassWidget);
    layGauges->addWidget(rawAttitudeWidget);
    layGauges->addStretch(1);

    layLeft->addLayout(layGauges);

    // 3. Command configuration group box
    QGroupBox *boxRawCmd = new QGroupBox(QString::fromUtf8("指令下发与参数装订 (CAN)"), leftPanel);
    boxRawCmd->setStyleSheet(cardStyle);
    QGridLayout *layRawCmd = new QGridLayout(boxRawCmd);
    layRawCmd->setContentsMargins(15, 20, 15, 15);
    layRawCmd->setSpacing(8);

    QString labelStyle = "QLabel { color: #d8dee9; font-size: 14px; }";
    QString editStyle = "QLineEdit { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 4px; font-family: 'Consolas'; font-size: 13px; } QLineEdit:focus { border-color: #88c0d0; }";

    QLabel *lblLon = new QLabel(QString::fromUtf8("经度(°):"), boxRawCmd); lblLon->setStyleSheet(labelStyle);
    rawEditLon = new QLineEdit(boxRawCmd); rawEditLon->setStyleSheet(editStyle); rawEditLon->setPlaceholderText("116.2833175");
    QLabel *lblLat = new QLabel(QString::fromUtf8("纬度(°):"), boxRawCmd); lblLat->setStyleSheet(labelStyle);
    rawEditLat = new QLineEdit(boxRawCmd); rawEditLat->setStyleSheet(editStyle); rawEditLat->setPlaceholderText("39.8287277");
    QLabel *lblAlt = new QLabel(QString::fromUtf8("高度(m):"), boxRawCmd); lblAlt->setStyleSheet(labelStyle);
    rawEditAlt = new QLineEdit(boxRawCmd); rawEditAlt->setStyleSheet(editStyle); rawEditAlt->setPlaceholderText("10.0");
    QLabel *lblAlignTime = new QLabel(QString::fromUtf8("对准时间(s):"), boxRawCmd); lblAlignTime->setStyleSheet(labelStyle);
    rawEditAlignTime = new QLineEdit(boxRawCmd); rawEditAlignTime->setStyleSheet(editStyle); rawEditAlignTime->setPlaceholderText("270.0");

    QLabel *lblBindHeading = new QLabel(QString::fromUtf8("装订航向(°):"), boxRawCmd); lblBindHeading->setStyleSheet(labelStyle);
    rawEditBindHeading = new QLineEdit(boxRawCmd); rawEditBindHeading->setStyleSheet(editStyle); rawEditBindHeading->setPlaceholderText("0.0");

    QLabel *lblFront = new QLabel(QString::fromUtf8("前向杆臂(m):"), boxRawCmd); lblFront->setStyleSheet(labelStyle);
    rawEditFrontLever = new QLineEdit(boxRawCmd); rawEditFrontLever->setStyleSheet(editStyle); rawEditFrontLever->setPlaceholderText("0.0");
    QLabel *lblRight = new QLabel(QString::fromUtf8("左向杆臂(m):"), boxRawCmd); lblRight->setStyleSheet(labelStyle);
    rawEditRightLever = new QLineEdit(boxRawCmd); rawEditRightLever->setStyleSheet(editStyle); rawEditRightLever->setPlaceholderText("0.0");
    QLabel *lblDown = new QLabel(QString::fromUtf8("上向杆臂(m):"), boxRawCmd); lblDown->setStyleSheet(labelStyle);
    rawEditUpLever = new QLineEdit(boxRawCmd); rawEditUpLever->setStyleSheet(editStyle); rawEditUpLever->setPlaceholderText("0.0");

    QLabel *lblOutBaud = new QLabel(QString::fromUtf8("输出波特率:"), boxRawCmd); lblOutBaud->setStyleSheet(labelStyle);
    rawCmbOutBaud = new QComboBox(boxRawCmd);
    rawCmbOutBaud->addItem("9600", 0);
    rawCmbOutBaud->addItem("19200", 6);
    rawCmbOutBaud->addItem("38400", 1);
    rawCmbOutBaud->addItem("57600", 7);
    rawCmbOutBaud->addItem("115200", 2);
    rawCmbOutBaud->addItem("230400", 3);
    rawCmbOutBaud->addItem("460800", 4);
    rawCmbOutBaud->addItem("921600", 5);
    const int defaultRawOutBaudValue = 4; // 协议值 4 对应 460800
    int baudIndex = rawCmbOutBaud->findData(defaultRawOutBaudValue);
    if (baudIndex >= 0) {
        rawCmbOutBaud->setCurrentIndex(baudIndex);
    }
    rawCmbOutBaud->setStyleSheet("QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 3px; }");

    QLabel *lblOutPeriod = new QLabel(QString::fromUtf8("输出周期:"), boxRawCmd); lblOutPeriod->setStyleSheet(labelStyle);
    rawCmbOutPeriod = new QComboBox(boxRawCmd);
    rawCmbOutPeriod->addItems(QStringList() << "5ms" << "10ms" << "20ms" << "50ms" << "100ms" << "200ms" << "500ms" << "1s" << "2s");
    rawCmbOutPeriod->setCurrentIndex(1); // 10ms
    rawCmbOutPeriod->setStyleSheet("QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 3px; }");

    QLabel *lblRollErr = new QLabel(QString::fromUtf8("滚动安装角(°):"), boxRawCmd); lblRollErr->setStyleSheet(labelStyle);
    rawEditRollError = new QLineEdit(boxRawCmd); rawEditRollError->setStyleSheet(editStyle); rawEditRollError->setPlaceholderText("0.0");
    QLabel *lblYawErr = new QLabel(QString::fromUtf8("航向安装角(°):"), boxRawCmd); lblYawErr->setStyleSheet(labelStyle);
    rawEditYawError = new QLineEdit(boxRawCmd); rawEditYawError->setStyleSheet(editStyle); rawEditYawError->setPlaceholderText("0.0");
    QLabel *lblPitchErr = new QLabel(QString::fromUtf8("俯仰安装角(°):"), boxRawCmd); lblPitchErr->setStyleSheet(labelStyle);
    rawEditPitchError = new QLineEdit(boxRawCmd); rawEditPitchError->setStyleSheet(editStyle); rawEditPitchError->setPlaceholderText("0.0");

    QLabel *lblCmd = new QLabel(QString::fromUtf8("选择控制指令:"), boxRawCmd); lblCmd->setStyleSheet(labelStyle);
    rawCmbCmdType = new QComboBox(boxRawCmd);
    rawCmbCmdType->addItems(QStringList() 
        << QString::fromUtf8("启动自寻北 (0x0011)")
        << QString::fromUtf8("惯性器件标定 (0x0011)")
        << QString::fromUtf8("快速航向对准 (0x0011)")
        << QString::fromUtf8("写入经纬高至Flash (0x0066)")
        << QString::fromUtf8("写入对准时间至Flash (0x0077)")
        << QString::fromUtf8("写入通信参数至Flash (0x0099)")
        << QString::fromUtf8("写入安装误差角至Flash (0x00AA)")
        << QString::fromUtf8("写入全部参数至Flash (0x00AA, 模式0x007F)")
        << QString::fromUtf8("写入姿态角转导航至Flash (0x00BB)")
        << QString::fromUtf8("写入陀螺零偏至Flash (0x00CC)")
        << QString::fromUtf8("写入杆臂至Flash (0x00DD)")
        << QString::fromUtf8("自定义十六进制指令")
    );
    rawCmbCmdType->setView(new QListView(this));
    rawCmbCmdType->setStyleSheet(
        "QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 4px; font-weight: bold; }"
        "QComboBox QAbstractItemView { background-color: #2e3440; color: #d8dee9; selection-background-color: #434c5e; selection-color: #88c0d0; border: 1px solid #4c566a; outline: 0px; }"
        "QComboBox QAbstractItemView::item { min-height: 28px; }"
    );

    rawEditCustomHexCmd = new QLineEdit(boxRawCmd);
    rawEditCustomHexCmd->setPlaceholderText(QString::fromUtf8("请输入自定义十六进制指令，空格或不空格均可，如: 55 AA 24 ..."));
    rawEditCustomHexCmd->setStyleSheet(editStyle);
    rawEditCustomHexCmd->hide(); // Hide by default

    rawBtnSendCmd = new QPushButton(QString::fromUtf8("发送指令 (CAN)"), boxRawCmd);
    rawBtnSendCmd->setStyleSheet("QPushButton { background-color: #5e81ac; color: #eceff4; border-radius: 4px; padding: 6px 12px; font-weight: bold; } QPushButton:hover { background-color: #81a1c1; }");

    QLabel *lblSendHex = new QLabel(QString::fromUtf8("发送HEX (下行):"), boxRawCmd); lblSendHex->setStyleSheet(labelStyle);
    rawEditSendHex = new QLineEdit(boxRawCmd);
    rawEditSendHex->setReadOnly(true);
    rawEditSendHex->setPlaceholderText(QString::fromUtf8("尚未发送指令"));
    rawEditSendHex->setStyleSheet(editStyle);

    layRawCmd->addWidget(lblLon, 0, 0); layRawCmd->addWidget(rawEditLon, 0, 1);
    layRawCmd->addWidget(lblLat, 0, 2); layRawCmd->addWidget(rawEditLat, 0, 3);
    layRawCmd->addWidget(lblAlt, 1, 0); layRawCmd->addWidget(rawEditAlt, 1, 1);
    layRawCmd->addWidget(lblAlignTime, 1, 2); layRawCmd->addWidget(rawEditAlignTime, 1, 3);
    layRawCmd->addWidget(lblBindHeading, 2, 0); layRawCmd->addWidget(rawEditBindHeading, 2, 1);
    
    layRawCmd->addWidget(lblOutBaud, 3, 0); layRawCmd->addWidget(rawCmbOutBaud, 3, 1);
    layRawCmd->addWidget(lblOutPeriod, 3, 2); layRawCmd->addWidget(rawCmbOutPeriod, 3, 3);

    // Lever Arms (Front, Left, Up) in one row
    QHBoxLayout *layRawLeverArms = new QHBoxLayout();
    layRawLeverArms->setContentsMargins(0, 0, 0, 0);
    layRawLeverArms->setSpacing(6);
    layRawLeverArms->addWidget(lblFront);
    layRawLeverArms->addWidget(rawEditFrontLever);
    layRawLeverArms->addWidget(lblRight);
    layRawLeverArms->addWidget(rawEditRightLever);
    layRawLeverArms->addWidget(lblDown);
    layRawLeverArms->addWidget(rawEditUpLever);
    layRawCmd->addLayout(layRawLeverArms, 4, 0, 1, 4);

    // Installation angles (Roll X, Yaw Y, Pitch Z) in one row
    QHBoxLayout *layRawAngles = new QHBoxLayout();
    layRawAngles->setContentsMargins(0, 0, 0, 0);
    layRawAngles->setSpacing(6);
    layRawAngles->addWidget(lblRollErr);
    layRawAngles->addWidget(rawEditRollError);
    layRawAngles->addWidget(lblYawErr);
    layRawAngles->addWidget(rawEditYawError);
    layRawAngles->addWidget(lblPitchErr);
    layRawAngles->addWidget(rawEditPitchError);
    layRawCmd->addLayout(layRawAngles, 5, 0, 1, 4);

    layRawCmd->addWidget(lblCmd, 6, 0); layRawCmd->addWidget(rawCmbCmdType, 6, 1, 1, 3);
    layRawCmd->addWidget(rawEditCustomHexCmd, 7, 0, 1, 4);
    layRawCmd->addWidget(rawBtnSendCmd, 8, 0, 1, 4);
    layRawCmd->addWidget(lblSendHex, 9, 0);
    layRawCmd->addWidget(rawEditSendHex, 9, 1, 1, 3);

    layLeft->addWidget(boxRawCmd);
    layLeft->addStretch(1);

    mainRawLayout->addWidget(leftPanel, 4); // 占比 4

    // ---------------- Right Panel: Table Only ----------------
    QWidget *rightPanel = new QWidget(rawTab);
    QVBoxLayout *layRight = new QVBoxLayout(rightPanel);
    layRight->setContentsMargins(0, 0, 0, 0);
    layRight->setSpacing(10);

    // Table Box
    QGroupBox *boxTable = new QGroupBox(QString::fromUtf8("透传解算实时数据表 (VCOM)"), rightPanel);
    boxTable->setStyleSheet(cardStyle);
    QVBoxLayout *layTable = new QVBoxLayout(boxTable);
    layTable->setContentsMargins(10, 20, 10, 10);

    rawTableWidget = new QTableWidget(boxTable);
    rawTableWidget->setRowCount(15);
    rawTableWidget->setColumnCount(13);
    rawTableWidget->setShowGrid(true); // 显式开启网格线显示
    rawTableWidget->horizontalHeader()->hide();
    rawTableWidget->verticalHeader()->hide();
    rawTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rawTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    rawTableWidget->setStyleSheet(
        "QTableWidget { background-color: #2e3440; gridline-color: #4c566a; color: #eceff4; font-size: 14px; }"
        "QTableWidget::item { border: 1px solid #4c566a; }" // 为单元格绘制清晰的边框线
    );

    // 初始化表格
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 13; ++c) {
            rawTableWidget->setItem(r, c, new QTableWidgetItem(""));
            rawTableWidget->item(r, c)->setTextAlignment(Qt::AlignCenter);
        }
    }

    // 设置跨行合并 (Spans)
    rawTableWidget->setSpan(0, 0, 1, 2);
    rawTableWidget->setSpan(0, 2, 1, 2);
    rawTableWidget->setSpan(0, 4, 1, 2);
    rawTableWidget->setSpan(0, 6, 1, 2);
    rawTableWidget->setSpan(0, 8, 1, 2);
    rawTableWidget->setSpan(0, 10, 1, 3);
    
    rawTableWidget->setSpan(1, 1, 1, 3);
    rawTableWidget->setSpan(1, 4, 1, 3);
    rawTableWidget->setSpan(1, 7, 1, 3);
    rawTableWidget->setSpan(1, 10, 1, 3);

    rawTableWidget->setSpan(2, 0, 1, 13);

    rawTableWidget->setSpan(3, 1, 1, 4);
    rawTableWidget->setSpan(3, 5, 1, 4);
    rawTableWidget->setSpan(3, 9, 1, 4);

    rawTableWidget->setSpan(4, 1, 1, 4);
    rawTableWidget->setSpan(4, 5, 1, 4);
    rawTableWidget->setSpan(4, 9, 1, 4);

    rawTableWidget->setSpan(5, 1, 1, 4);
    rawTableWidget->setSpan(5, 5, 1, 4);
    rawTableWidget->setSpan(5, 9, 1, 4);

    rawTableWidget->setSpan(6, 1, 1, 3);
    rawTableWidget->setSpan(6, 4, 1, 3);
    rawTableWidget->setSpan(6, 7, 1, 3);
    rawTableWidget->setSpan(6, 10, 1, 3);

    rawTableWidget->setSpan(7, 1, 1, 3);
    rawTableWidget->setSpan(7, 4, 1, 3);
    rawTableWidget->setSpan(7, 7, 1, 3);
    rawTableWidget->setSpan(7, 10, 1, 3);

    rawTableWidget->setSpan(8, 0, 1, 13);

    rawTableWidget->setSpan(9, 1, 1, 4);
    rawTableWidget->setSpan(9, 5, 1, 4);
    rawTableWidget->setSpan(9, 9, 1, 4);

    rawTableWidget->setSpan(10, 1, 1, 4);
    rawTableWidget->setSpan(10, 5, 1, 4);
    rawTableWidget->setSpan(10, 9, 1, 4);

    rawTableWidget->setSpan(11, 0, 1, 13);

    rawTableWidget->setSpan(12, 1, 1, 4);
    rawTableWidget->setSpan(12, 5, 1, 4);
    rawTableWidget->setSpan(12, 9, 1, 4);

    rawTableWidget->setSpan(13, 1, 1, 4);
    rawTableWidget->setSpan(13, 5, 1, 4);
    rawTableWidget->setSpan(13, 9, 1, 4);

    rawTableWidget->setSpan(14, 0, 1, 2);
    rawTableWidget->setSpan(14, 2, 1, 2);
    rawTableWidget->setSpan(14, 4, 1, 2);
    rawTableWidget->setSpan(14, 6, 1, 3);
    rawTableWidget->setSpan(14, 9, 1, 2);
    rawTableWidget->setSpan(14, 11, 1, 2);

    // 填充静态 Label
    rawTableWidget->item(0, 0)->setText(QString::fromUtf8("工作时间 (s)"));
    rawTableWidget->item(0, 4)->setText(QString::fromUtf8("对准总时间/剩余"));
    rawTableWidget->item(0, 8)->setText(QString::fromUtf8("芯片温度"));
    
    rawTableWidget->item(1, 0)->setText(QString::fromUtf8("状态字"));

    rawTableWidget->item(2, 0)->setText(QString::fromUtf8("--- 惯性测量与姿态数据 ---"));
    rawTableWidget->item(2, 0)->setForeground(QBrush(QColor("#88c0d0")));
    rawTableWidget->item(2, 0)->setFont(QFont("Arial", 12, QFont::Bold));

    rawTableWidget->item(3, 0)->setText(QString::fromUtf8("角速度(°/s)"));
    rawTableWidget->item(4, 0)->setText(QString::fromUtf8("加速度(g)"));
    rawTableWidget->item(5, 0)->setText(QString::fromUtf8("高精度姿态(°)"));
    rawTableWidget->item(6, 0)->setText(QString::fromUtf8("反欧拉角(°)"));
    rawTableWidget->item(7, 0)->setText(QString::fromUtf8("四元数 Q"));

    rawTableWidget->item(8, 0)->setText(QString::fromUtf8("--- 位置定位与运动速度 ---"));
    rawTableWidget->item(8, 0)->setForeground(QBrush(QColor("#88c0d0")));
    rawTableWidget->item(8, 0)->setFont(QFont("Arial", 12, QFont::Bold));

    rawTableWidget->item(9, 0)->setText(QString::fromUtf8("物理位置"));
    rawTableWidget->item(10, 0)->setText(QString::fromUtf8("导航速度(m/s)"));

    rawTableWidget->item(11, 0)->setText(QString::fromUtf8("--- 传感器错误与超量程计数 ---"));
    rawTableWidget->item(11, 0)->setForeground(QBrush(QColor("#88c0d0")));
    rawTableWidget->item(11, 0)->setFont(QFont("Arial", 12, QFont::Bold));

    rawTableWidget->item(12, 0)->setText(QString::fromUtf8("陀螺超量程"));
    rawTableWidget->item(13, 0)->setText(QString::fromUtf8("加表超量程"));

    rawTableWidget->item(14, 0)->setText(QString::fromUtf8("协议格式"));
    rawTableWidget->item(14, 4)->setText(QString::fromUtf8("垂向标志"));
    rawTableWidget->item(14, 9)->setText(QString::fromUtf8("帧计数"));

    // 默认初始值填充
    rawTableWidget->item(0, 2)->setText("0.00");
    rawTableWidget->item(0, 6)->setText("0 / 0");
    rawTableWidget->item(0, 10)->setText(QString::fromUtf8("陀螺: 0℃ / 加表: 0℃"));
    rawTableWidget->item(1, 1)->setText(QString::fromUtf8("系统: IDLE"));
    rawTableWidget->item(1, 4)->setText(QString::fromUtf8("超量程: 0b000000"));
    rawTableWidget->item(1, 7)->setText(QString::fromUtf8("通道: 0"));
    rawTableWidget->item(1, 10)->setText(QString::fromUtf8("回令: 0"));

    rawTableWidget->item(3, 1)->setText("X: 0.000000");
    rawTableWidget->item(3, 5)->setText("Y: 0.000000");
    rawTableWidget->item(3, 9)->setText("Z: 0.000000");

    rawTableWidget->item(4, 1)->setText("X: 0.000000");
    rawTableWidget->item(4, 5)->setText("Y: 0.000000");
    rawTableWidget->item(4, 9)->setText("Z: 0.000000");

    rawTableWidget->item(5, 1)->setText("Roll: 0.000");
    rawTableWidget->item(5, 5)->setText("Heading: 0.000");
    rawTableWidget->item(5, 9)->setText("Pitch: 0.000");

    rawTableWidget->item(6, 1)->setText("Roll: 0.000");
    rawTableWidget->item(6, 4)->setText("Heading: 0.000");
    rawTableWidget->item(6, 7)->setText("Pitch: 0.000");
    rawTableWidget->item(6, 10)->setText("Flag: 0");

    rawTableWidget->item(7, 1)->setText("Q1: 0.000000");
    rawTableWidget->item(7, 4)->setText("Q2: 0.000000");
    rawTableWidget->item(7, 7)->setText("Q3: 0.000000");
    rawTableWidget->item(7, 10)->setText("Q4: 0.000000");

    rawTableWidget->item(9, 1)->setText(QString::fromUtf8("纬度: 0.00000000"));
    rawTableWidget->item(9, 5)->setText(QString::fromUtf8("经度: 0.00000000"));
    rawTableWidget->item(9, 9)->setText(QString::fromUtf8("高程: 0.00m"));

    rawTableWidget->item(10, 1)->setText(QString::fromUtf8("北: 0.0000"));
    rawTableWidget->item(10, 5)->setText(QString::fromUtf8("天: 0.0000"));
    rawTableWidget->item(10, 9)->setText(QString::fromUtf8("东: 0.0000"));

    rawTableWidget->item(12, 1)->setText("Gx: 0");
    rawTableWidget->item(12, 5)->setText("Gy: 0");
    rawTableWidget->item(12, 9)->setText("Gz: 0");

    rawTableWidget->item(13, 1)->setText("Ax: 0");
    rawTableWidget->item(13, 5)->setText("Ay: 0");
    rawTableWidget->item(13, 9)->setText("Az: 0");

    rawTableWidget->item(14, 2)->setText(QString::fromUtf8("高精度寻北协议"));
    rawTableWidget->item(14, 6)->setText(QString::fromUtf8("水平模式"));
    rawTableWidget->item(14, 11)->setText("0");

    // 美化样式
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 13; ++c) {
            QTableWidgetItem *it = rawTableWidget->item(r, c);
            if (r == 2 || r == 8 || r == 11) {
                it->setBackground(QBrush(QColor("#3b4252")));
            } else if (c == 0 || (r == 0 && (c == 4 || c == 8)) || (r == 14 && (c == 4 || c == 9))) {
                it->setBackground(QBrush(QColor("#2e3440")));
                it->setForeground(QBrush(QColor("#d8dee9")));
                it->setFont(QFont("Arial", 11, QFont::Bold));
            } else {
                it->setBackground(QBrush(QColor("#3b4252")));
                it->setForeground(QBrush(QColor("#eceff4")));
            }
        }
    }

    // 自动拉伸列与行
    rawTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rawTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    layTable->addWidget(rawTableWidget);
    layRight->addWidget(boxTable, 1); // 占比 1 (自动铺满)

    mainRawLayout->addWidget(rightPanel, 6); // 占比 6

    // 绑定控制按钮
    connect(btnStartRawMode, &QPushButton::clicked, this, &MainWindow::startRawModeClicked);
    connect(btnStopRawMode, &QPushButton::clicked, this, &MainWindow::stopRawModeClicked);
    connect(chkSaveRaw, &QCheckBox::stateChanged, this, &MainWindow::onSaveRawStateChanged);
    connect(rawBtnSendCmd, &QPushButton::clicked, this, &MainWindow::onSendRawCommand);
    connect(rawCmbCmdType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onRawCmdTypeChanged);

    // 初始化运行状态监控
    m_lastRawTime = QTime::currentTime();
    rawTimeoutTimer = new QTimer(this);
    connect(rawTimeoutTimer, &QTimer::timeout, this, &MainWindow::checkRawDataTimeout);
    rawTimeoutTimer->start(1000);
}

void MainWindow::startRawModeClicked()
{
    if (!canthread || canthread->m_dev == INVALID_DEVICE_HANDLE) {
        QMessageBox::warning(this, "警告", "CAN通道未就绪或未打开！");
        return;
    }
    // 下发透传激活控制帧 (ID=0x0F, DLC=8, data=[1, 0, 0, 0, 0, 0, 0, 0])
    char cmdData[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    bool ok = canthread->sendData(0x0F, 0, 0, 0, 0, cmdData, 8);
    if (ok) {
        lblRawStatus->setText(QString::fromUtf8("透传状态: 命令下发成功"));
        lblRawStatus->setStyleSheet("color: #2e3440; font-size: 13px; font-weight: bold; background-color: #a3be8c; padding: 6px 15px; border-radius: 4px;");
    } else {
        QMessageBox::warning(this, "警告", "下发开启透传指令失败！");
    }
}

void MainWindow::stopRawModeClicked()
{
    if (!canthread || canthread->m_dev == INVALID_DEVICE_HANDLE) {
        QMessageBox::warning(this, "警告", "CAN通道未就绪或未打开！");
        return;
    }
    // 下发透传关闭控制帧 (ID=0x0F, DLC=8, data=[2, 0, 0, 0, 0, 0, 0, 0])
    char cmdData[8] = {2, 0, 0, 0, 0, 0, 0, 0};
    bool ok = canthread->sendData(0x0F, 0, 0, 0, 0, cmdData, 8);
    if (ok) {
        lblRawStatus->setText(QString::fromUtf8("透传状态: 关闭成功"));
        lblRawStatus->setStyleSheet("color: #d8dee9; font-size: 13px; font-weight: bold; background-color: #434c5e; padding: 6px 15px; border-radius: 4px;");
    } else {
        QMessageBox::warning(this, "警告", "下发关闭透传指令失败！");
    }
}

void MainWindow::checkRawDataTimeout()
{
    // 超过 2 秒没有新透传数据包时恢复为 IDLE
    if (m_lastRawTime.msecsTo(QTime::currentTime()) > 2000) {
        lblRawStatus->setText(QString::fromUtf8("透传状态: IDLE"));
        lblRawStatus->setStyleSheet("color: #d8dee9; font-size: 13px; font-weight: bold; background-color: #434c5e; padding: 6px 15px; border-radius: 4px;");
    }
}

void MainWindow::onSaveCANStateChanged(int state)
{
    if (state == Qt::Checked) {
        QString defaultPath = QDir::currentPath() + "/can_log_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt";
        QString fileName = QFileDialog::getSaveFileName(this, QString::fromUtf8("选择CAN收发日志保存路径"), defaultPath, "Text files (*.txt)");
        if (fileName.isEmpty()) {
            chkSaveCAN->blockSignals(true);
            chkSaveCAN->setChecked(false);
            chkSaveCAN->blockSignals(false);
            return;
        }

        m_canFile.setFileName(fileName);
        if (m_canFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_canStream.setDevice(&m_canFile);
            m_canStream.setCodec("UTF-8");
        } else {
            QMessageBox::warning(this, QString::fromUtf8("警告"), QString::fromUtf8("无法创建CAN日志文件：") + fileName);
            chkSaveCAN->blockSignals(true);
            chkSaveCAN->setChecked(false);
            chkSaveCAN->blockSignals(false);
        }
    } else {
        if (m_canFile.isOpen()) {
            m_canFile.close();
        }
    }
}

void MainWindow::onSaveParsedIMUStateChanged(int state)
{
    if (state == Qt::Checked) {
        QString defaultPath = QDir::currentPath() + "/imu_parsed_" +
                              QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".csv";
        QString fileName = QFileDialog::getSaveFileName(this,
                                                        QString::fromUtf8("选择解析数据保存路径"),
                                                        defaultPath,
                                                        "CSV files (*.csv)");
        if (fileName.isEmpty()) {
            chkSaveParsedIMU->blockSignals(true);
            chkSaveParsedIMU->setChecked(false);
            chkSaveParsedIMU->blockSignals(false);
            return;
        }
        if (!fileName.endsWith(".csv", Qt::CaseInsensitive)) {
            fileName += ".csv";
        }

        m_parsedIMUFile.setFileName(fileName);
        if (m_parsedIMUFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            // 写入 UTF-8 BOM，兼容 Windows Excel；表头使用英文，便于脚本和分析软件读取。
            m_parsedIMUFile.write(QByteArray::fromHex("EFBBBF"));
            m_parsedIMUStream.setDevice(&m_parsedIMUFile);
            m_parsedIMUStream.setCodec("UTF-8");
            m_parsedIMUStartTime = QDateTime::currentDateTime();
            m_parsedIMUElapsedBaseValid = false;
            m_parsedIMUElapsedBaseMs = 0;
            m_parsedIMUStream << "timestamp,elapsed_ms,channel,can_id,update_type,"
                              << "roll_deg,pitch_deg,yaw_deg,"
                              << "ax_g,ay_g,az_g,"
                              << "gx_dps,gy_dps,gz_dps,fault_level\n";
        } else {
            QMessageBox::warning(this,
                                 QString::fromUtf8("警告"),
                                 QString::fromUtf8("无法创建解析数据文件：") + fileName);
            chkSaveParsedIMU->blockSignals(true);
            chkSaveParsedIMU->setChecked(false);
            chkSaveParsedIMU->blockSignals(false);
        }
    } else {
        if (m_parsedIMUFile.isOpen()) {
            m_parsedIMUStream.flush();
            m_parsedIMUFile.close();
        }
    }
}

void MainWindow::saveParsedIMUSnapshot(UINT canId, UINT channel, const QString &receiveTimeText, qint64 receiveElapsedMs)
{
    if (!m_parsedIMUFile.isOpen()) {
        return;
    }

    QString updateType;
    switch (canId) {
    case 0x18EE00D0:
        updateType = "accel";
        break;
    case 0x18EE01D0:
        updateType = "gyro";
        break;
    case 0x18EE02D0:
        updateType = "attitude";
        break;
    case 0x18EE04D0:
        updateType = "fault";
        break;
    default:
        return;
    }

    IMUData data = IMUParser::getInstance()->getIMUData();
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;
    double gx = 0.0;
    double gy = 0.0;
    double gz = 0.0;
    unsigned char faultLevel = 0;

    // 每一行只记录当前协议包实际携带的字段；未携带的姿态/加速度/角速度字段保持 0，
    // 便于后续按 update_type 分析，不会误把上一帧缓存值当成本帧数据。
    switch (canId) {
    case 0x18EE00D0:
        ax = data.ax;
        ay = data.ay;
        az = data.az;
        break;
    case 0x18EE01D0:
        gx = data.gx;
        gy = data.gy;
        gz = data.gz;
        break;
    case 0x18EE02D0:
        roll = data.roll;
        pitch = data.pitch;
        yaw = data.yaw;
        break;
    case 0x18EE04D0:
        faultLevel = data.faultLevel;
        break;
    default:
        break;
    }

    QDateTime now = QDateTime::currentDateTime();
    if (!m_parsedIMUElapsedBaseValid || receiveElapsedMs < m_parsedIMUElapsedBaseMs) {
        m_parsedIMUElapsedBaseValid = true;
        m_parsedIMUElapsedBaseMs = receiveElapsedMs;
    }
    qint64 elapsedMs = receiveElapsedMs - m_parsedIMUElapsedBaseMs;
    QString csvTimeText = receiveTimeText;
    if (csvTimeText.isEmpty()) {
        csvTimeText = now.toString("yyyy-MM-dd hh:mm:ss.zzz");
    } else {
        csvTimeText = now.date().toString("yyyy-MM-dd ") + csvTimeText;
    }

    // timestamp 前加单引号，避免 Excel 自动转成日期格式后隐藏毫秒。
    m_parsedIMUStream << "'" << csvTimeText << ","
                      << QString::number(elapsedMs) << ","
                      << QString::number(channel) << ","
                      << "0x" << QString::number(canId, 16).toUpper() << ","
                      << updateType << ","
                      << QString::number(roll, 'f', 6) << ","
                      << QString::number(pitch, 'f', 6) << ","
                      << QString::number(yaw, 'f', 6) << ","
                      << QString::number(ax, 'f', 8) << ","
                      << QString::number(ay, 'f', 8) << ","
                      << QString::number(az, 'f', 8) << ","
                      << QString::number(gx, 'f', 6) << ","
                      << QString::number(gy, 'f', 6) << ","
                      << QString::number(gz, 'f', 6) << ","
                      << QString::number(faultLevel) << "\n";
}

void MainWindow::onSaveRawStateChanged(int state)
{
    if (state == Qt::Checked) {
        QString defaultPath = QDir::currentPath() + "/ins_log_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt";
        QString txtFileName = QFileDialog::getSaveFileName(this, QString::fromUtf8("选择透传日志保存路径"), defaultPath, "Text files (*.txt)");
        if (txtFileName.isEmpty()) {
            chkSaveRaw->blockSignals(true);
            chkSaveRaw->setChecked(false);
            chkSaveRaw->blockSignals(false);
            return;
        }

        // 自动将 .txt 替换为 .bin 获得二进制保存文件路径
        QString binFileName = txtFileName;
        if (binFileName.endsWith(".txt", Qt::CaseInsensitive)) {
            binFileName.chop(4);
            binFileName += ".bin";
        } else {
            binFileName += ".bin";
        }

        m_rawFile.setFileName(txtFileName);
        m_rawBinFile.setFileName(binFileName);

        bool txtOk = m_rawFile.open(QIODevice::WriteOnly | QIODevice::Text);
        bool binOk = m_rawBinFile.open(QIODevice::WriteOnly);

        if (txtOk && binOk) {
            m_rawStream.setDevice(&m_rawFile);
            m_rawStream.setCodec("UTF-8");
            
            // 写入 TXT 文件的首行表头 (已移除，以匹配实际保存为纯Hex的逻辑)
        } else {
            QMessageBox::warning(this, QString::fromUtf8("警告"), QString::fromUtf8("无法创建透传日志文件:\n") + txtFileName + "\n" + binFileName);
            if (m_rawFile.isOpen()) m_rawFile.close();
            if (m_rawBinFile.isOpen()) m_rawBinFile.close();
            chkSaveRaw->blockSignals(true);
            chkSaveRaw->setChecked(false);
            chkSaveRaw->blockSignals(false);
        }
    } else {
        if (m_rawFile.isOpen()) {
            m_rawFile.close();
        }
        if (m_rawBinFile.isOpen()) {
            m_rawBinFile.close();
        }
    }
}

void MainWindow::onQuickStartClicked()
{
    // 步骤1：打开设备
    if (!canthread->openDevice(deviceType_index_arr[ui->deviceTypeCombo->currentIndex()],
                               ui->deviceIndexCombo->currentIndex(), 0)) {
        QMessageBox::warning(this, QString::fromUtf8("一键启动失败"),
                             QString::fromUtf8("\xe7\xac\xac" "1" "\xe6\xad\xa5" " [" "\xe6\x89\x93\xe5\xbc\x80\xe8\xae\xbe\xe5\xa4\x87" "] " "\xe5\xa4\xb1\xe8\xb4\xa5" "\n" "\xe8\xaf\xb7\xe6\xa3\x80\xe6\x9f\xa5\xe8\xae\xbe\xe5\xa4\x87\xe7\xb1\xbb\xe5\x9e\x8b\xe5\x92\x8c" "USB" "\xe8\xbf\x9e\xe6\x8e\xa5"));
        return;
    }
    ui->openDeviceBtn->setEnabled(false);
    ui->closeDeviceBtn->setEnabled(true);

    // 步骤2：初始化CAN（复用现有逻辑）
    if (!canthread->setCANFDStandard(ui->canFDStandardCombo->currentIndex())) {
        QMessageBox::warning(this, QString::fromUtf8("一键启动失败"),
                             QString::fromUtf8("\xe7\xac\xac" "2" "\xe6\xad\xa5" " [" "\xe8\xae\xbe\xe7\xbd\xae" "CAN-FD" "\xe6\xa0\x87\xe5\x87\x86" "] " "\xe5\xa4\xb1\xe8\xb4\xa5"));
        return;
    }

    UINT rate1 = 0, rate2 = 0;
    if (ui->CustomBaudrateCheckBox->checkState()) {
        if (!canthread->setCustomBaudrateFD(ui->CustomBaudrateEdit->text())) {
            QMessageBox::warning(this, QString::fromUtf8("一键启动失败"),
                                 QString::fromUtf8("\xe7\xac\xac" "2" "\xe6\xad\xa5" " [" "\xe8\xae\xbe\xe7\xbd\xae\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe6\xb3\xa2\xe7\x89\xb9\xe7\x8e\x87" "] " "\xe5\xa4\xb1\xe8\xb4\xa5"));
            return;
        }
    } else {
        switch (ui->ABIT1Combo->currentIndex()) {
            case 0: rate1 = 1000000; break;
            case 1: rate1 = 800000; break;
            case 2: rate1 = 500000; break;
            case 3: rate1 = 250000; break;
            case 4: rate1 = 125000; break;
            case 5: rate1 = 100000; break;
            case 6: rate1 = 50000; break;
            default: rate1 = 50000; break;
        }
        switch (ui->ABIT2Combo->currentIndex()) {
            case 0: rate2 = 5000000; break;
            case 1: rate2 = 4000000; break;
            case 2: rate2 = 2000000; break;
            case 3: rate2 = 1000000; break;
            default: rate2 = 1000000; break;
        }
        if (!canthread->setBaudrateFD(rate1, rate2)) {
            QMessageBox::warning(this, QString::fromUtf8("一键启动失败"),
                                 QString::fromUtf8("\xe7\xac\xac" "2" "\xe6\xad\xa5" " [" "\xe8\xae\xbe\xe7\xbd\xae\xe6\xb3\xa2\xe7\x89\xb9\xe7\x8e\x87" "] " "\xe5\xa4\xb1\xe8\xb4\xa5"));
            return;
        }
    }

    if (!afterReSet()) {
        return;
    }

    // 步骤3：启动CAN
    if (!canthread->startCAN()) {
        QMessageBox::warning(this, QString::fromUtf8("一键启动失败"),
                             QString::fromUtf8("\xe7\xac\xac" "3" "\xe6\xad\xa5" " [" "\xe5\x90\xaf\xe5\x8a\xa8" "CAN] " "\xe5\xa4\xb1\xe8\xb4\xa5"));
        return;
    }
    ui->initCANBtn->setEnabled(false);
    ui->StartCANBtn->setEnabled(false);
    ui->reSetCANBtn->setEnabled(true);
    ui->sendBtn->setEnabled(true);
    canthread->start();

    btnQuickStart->setEnabled(false);
    btnQuickStart->setText(QString::fromUtf8("✅ 已启动"));
}

void MainWindow::stopCANSaveWithWarning(const QString &errorMessage)
{
    if (m_canFile.isOpen()) {
        m_canStream.flush();
        m_canFile.close();
    }
    chkSaveCAN->blockSignals(true);
    chkSaveCAN->setChecked(false);
    chkSaveCAN->blockSignals(false);
    QMessageBox::warning(this,
                         QString::fromUtf8("警告"),
                         QString::fromUtf8("CAN日志保存失败，已自动停止：") + errorMessage);
}

void MainWindow::stopRawSaveWithWarning(const QString &errorMessage)
{
    if (m_rawFile.isOpen()) {
        m_rawStream.flush();
        m_rawFile.close();
    }
    if (m_rawBinFile.isOpen()) {
        m_rawBinFile.flush();
        m_rawBinFile.close();
    }
    chkSaveRaw->blockSignals(true);
    chkSaveRaw->setChecked(false);
    chkSaveRaw->blockSignals(false);
    QMessageBox::warning(this,
                         QString::fromUtf8("警告"),
                         QString::fromUtf8("透传日志保存失败，已自动停止：") + errorMessage);
}

void MainWindow::onFlushTimeout()
{
    if (m_canFile.isOpen()) {
        m_canStream.flush();
        m_canFile.flush();
        if (m_canFile.error() != QFileDevice::NoError) {
            stopCANSaveWithWarning(m_canFile.errorString());
        }
    }
    if (m_parsedIMUFile.isOpen()) {
        m_parsedIMUStream.flush();
        m_parsedIMUFile.flush();
        if (m_parsedIMUFile.error() != QFileDevice::NoError) {
            QString err = m_parsedIMUFile.errorString();
            m_parsedIMUFile.close();
            chkSaveParsedIMU->blockSignals(true);
            chkSaveParsedIMU->setChecked(false);
            chkSaveParsedIMU->blockSignals(false);
            QMessageBox::warning(this,
                                 QString::fromUtf8("警告"),
                                 QString::fromUtf8("解析数据保存失败，已自动停止：") + err);
        }
    }
    if (m_rawFile.isOpen()) {
        m_rawStream.flush();
        m_rawFile.flush();
        if (m_rawFile.error() != QFileDevice::NoError) {
            stopRawSaveWithWarning(m_rawFile.errorString());
            return;
        }
    }
    if (m_rawBinFile.isOpen()) {
        m_rawBinFile.flush();
        if (m_rawBinFile.error() != QFileDevice::NoError) {
            stopRawSaveWithWarning(m_rawBinFile.errorString());
            return;
        }
    }
    if (m_serialTxtFile.isOpen()) {
        m_serialTxtStream.flush();
    }
    if (m_serialBinFile.isOpen()) {
        m_serialBinFile.flush();
    }
}

void MainWindow::setupSerialTab()
{
    // 主布局
    QHBoxLayout *mainSerialLayout = new QHBoxLayout(serialTab);
    mainSerialLayout->setContentsMargins(10, 10, 10, 10);
    mainSerialLayout->setSpacing(10);

    // 卡片样式 (深色玻璃质感)
    QString cardStyle = 
        "QGroupBox { background-color: #2e3440; border: 2px solid #3b4252; border-radius: 8px; margin-top: 10px; color: #88c0d0; font-weight: bold; font-size: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 5px; background-color: #2e3440; color: #88c0d0; }";

    // ---------------- 左侧面板: 连接与配置 ----------------
    QWidget *leftPanel = new QWidget(serialTab);
    QVBoxLayout *layLeft = new QVBoxLayout(leftPanel);
    layLeft->setContentsMargins(0, 0, 0, 0);
    layLeft->setSpacing(10);

    // 1. 串口连接配置组
    QGroupBox *boxConn = new QGroupBox(QString::fromUtf8("串口通信配置"), leftPanel);
    boxConn->setStyleSheet(cardStyle);
    QGridLayout *layConn = new QGridLayout(boxConn);
    layConn->setContentsMargins(15, 20, 15, 15);
    layConn->setSpacing(10);

    QLabel *lblPort = new QLabel(QString::fromUtf8("串口号:"), boxConn);
    lblPort->setStyleSheet("color: #eceff4;");
    serialPortCombo = new QComboBox(boxConn);
    serialPortCombo->setMinimumWidth(150);
    serialPortCombo->setMaximumWidth(280);
    serialPortCombo->setStyleSheet("QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 4px; }");
    
    btnRefreshSerial = new QPushButton(QString::fromUtf8("刷新"), boxConn);
    btnRefreshSerial->setStyleSheet("QPushButton { background-color: #434c5e; color: #eceff4; border: 1px solid #4c566a; border-radius: 4px; padding: 4px 8px; } QPushButton:hover { background-color: #4c566a; }");

    QLabel *lblBaud = new QLabel(QString::fromUtf8("波特率:"), boxConn);
    lblBaud->setStyleSheet("color: #eceff4;");
    serialBaudCombo = new QComboBox(boxConn);
    serialBaudCombo->addItems(QStringList() << "9600" << "19200" << "38400" << "57600" << "115200" << "230400" << "460800" << "921600");
    serialBaudCombo->setCurrentIndex(6); // 默认 460800
    serialBaudCombo->setFixedWidth(150);
    serialBaudCombo->setStyleSheet("QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 4px; }");

    btnToggleSerial = new QPushButton(QString::fromUtf8("打开串口"), boxConn);
    btnToggleSerial->setStyleSheet("QPushButton { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 4px; padding: 5px 10px; font-weight: bold; } QPushButton:hover { background-color: #434c5e; }");

    QLabel *lblDownsample = new QLabel(QString::fromUtf8("显示降采样:"), boxConn);
    lblDownsample->setStyleSheet("color: #eceff4;");
    serialDownsampleCombo = new QComboBox(boxConn);
    serialDownsampleCombo->addItems(QStringList() << "1/1" << "1/10" << "1/20" << "1/100" << "1/200");
    serialDownsampleCombo->setCurrentIndex(0); // 默认 1/1
    serialDownsampleCombo->setFixedWidth(150);
    serialDownsampleCombo->setStyleSheet("QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 4px; }");

    layConn->addWidget(lblPort, 0, 0);
    layConn->addWidget(serialPortCombo, 0, 1);
    layConn->addWidget(btnRefreshSerial, 0, 2);
    layConn->addWidget(lblBaud, 1, 0);
    layConn->addWidget(serialBaudCombo, 1, 1, 1, 2);
    layConn->addWidget(lblDownsample, 2, 0);
    layConn->addWidget(serialDownsampleCombo, 2, 1, 1, 2);
    layConn->addWidget(btnToggleSerial, 3, 0, 1, 3);
    
    layConn->setColumnStretch(0, 0);
    layConn->setColumnStretch(1, 0);
    layConn->setColumnStretch(2, 0);
    layConn->setColumnStretch(3, 1); // 增加伸缩列，强制通信配置控件前移紧凑显示

    layLeft->addWidget(boxConn);

    // 2. 指令与参数配置组
    QGroupBox *boxCmd = new QGroupBox(QString::fromUtf8("指令下发与参数装订"), leftPanel);
    boxCmd->setStyleSheet(cardStyle);
    QGridLayout *layCmd = new QGridLayout(boxCmd);
    layCmd->setContentsMargins(15, 20, 15, 15);
    layCmd->setSpacing(8);

    QString labelStyle = "QLabel { color: #d8dee9; font-size: 14px; }";
    QString editStyle = "QLineEdit { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 4px; font-family: 'Consolas'; font-size: 13px; } QLineEdit:focus { border-color: #88c0d0; }";

    // 经纬高与对准时间
    QLabel *lblLon = new QLabel(QString::fromUtf8("经度(°):"), boxCmd); lblLon->setStyleSheet(labelStyle);
    editLon = new QLineEdit(boxCmd); editLon->setStyleSheet(editStyle); editLon->setPlaceholderText("116.2833175");
    QLabel *lblLat = new QLabel(QString::fromUtf8("纬度(°):"), boxCmd); lblLat->setStyleSheet(labelStyle);
    editLat = new QLineEdit(boxCmd); editLat->setStyleSheet(editStyle); editLat->setPlaceholderText("39.8287277");
    QLabel *lblAlt = new QLabel(QString::fromUtf8("高度(m):"), boxCmd); lblAlt->setStyleSheet(labelStyle);
    editAlt = new QLineEdit(boxCmd); editAlt->setStyleSheet(editStyle); editAlt->setPlaceholderText("10.0");
    QLabel *lblAlignTime = new QLabel(QString::fromUtf8("对准时间(s):"), boxCmd); lblAlignTime->setStyleSheet(labelStyle);
    editAlignTime = new QLineEdit(boxCmd); editAlignTime->setStyleSheet(editStyle); editAlignTime->setPlaceholderText("270.0");

    // 外部航向装订
    QLabel *lblBindHeading = new QLabel(QString::fromUtf8("装订航向(°):"), boxCmd); lblBindHeading->setStyleSheet(labelStyle);
    editBindHeading = new QLineEdit(boxCmd); editBindHeading->setStyleSheet(editStyle); editBindHeading->setPlaceholderText("0.0");

    // 杆臂参数配置
    QLabel *lblFront = new QLabel(QString::fromUtf8("前向杆臂(m):"), boxCmd); lblFront->setStyleSheet(labelStyle);
    editFrontLever = new QLineEdit(boxCmd); editFrontLever->setStyleSheet(editStyle); editFrontLever->setPlaceholderText("0.0");
    QLabel *lblRight = new QLabel(QString::fromUtf8("左向杆臂(m):"), boxCmd); lblRight->setStyleSheet(labelStyle);
    editRightLever = new QLineEdit(boxCmd); editRightLever->setStyleSheet(editStyle); editRightLever->setPlaceholderText("0.0");
    QLabel *lblDown = new QLabel(QString::fromUtf8("上向杆臂(m):"), boxCmd); lblDown->setStyleSheet(labelStyle);
    editUpLever = new QLineEdit(boxCmd); editUpLever->setStyleSheet(editStyle); editUpLever->setPlaceholderText("0.0");

    // 通信参数
    QLabel *lblOutBaud = new QLabel(QString::fromUtf8("输出波特率:"), boxCmd); lblOutBaud->setStyleSheet(labelStyle);
    cmbOutBaud = new QComboBox(boxCmd);
    cmbOutBaud->addItem("9600", 0);
    cmbOutBaud->addItem("19200", 6);
    cmbOutBaud->addItem("38400", 1);
    cmbOutBaud->addItem("57600", 7);
    cmbOutBaud->addItem("115200", 2);
    cmbOutBaud->addItem("230400", 3);
    cmbOutBaud->addItem("460800", 4);
    cmbOutBaud->addItem("921600", 5);
    cmbOutBaud->setCurrentIndex(6); // 460800
    cmbOutBaud->setStyleSheet("QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 3px; }");

    QLabel *lblOutPeriod = new QLabel(QString::fromUtf8("输出周期:"), boxCmd); lblOutPeriod->setStyleSheet(labelStyle);
    cmbOutPeriod = new QComboBox(boxCmd);
    cmbOutPeriod->addItems(QStringList() << "5ms" << "10ms" << "20ms" << "50ms" << "100ms" << "200ms" << "500ms" << "1s" << "2s");
    cmbOutPeriod->setCurrentIndex(1); // 10ms
    cmbOutPeriod->setStyleSheet("QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 3px; }");

    // 安装角误差
    QLabel *lblRollErr = new QLabel(QString::fromUtf8("滚动安装角(°):"), boxCmd); lblRollErr->setStyleSheet(labelStyle);
    editRollError = new QLineEdit(boxCmd); editRollError->setStyleSheet(editStyle); editRollError->setPlaceholderText("0.0");
    QLabel *lblYawErr = new QLabel(QString::fromUtf8("航向安装角(°):"), boxCmd); lblYawErr->setStyleSheet(labelStyle);
    editYawError = new QLineEdit(boxCmd); editYawError->setStyleSheet(editStyle); editYawError->setPlaceholderText("0.0");
    QLabel *lblPitchErr = new QLabel(QString::fromUtf8("俯仰安装角(°):"), boxCmd); lblPitchErr->setStyleSheet(labelStyle);
    editPitchError = new QLineEdit(boxCmd); editPitchError->setStyleSheet(editStyle); editPitchError->setPlaceholderText("0.0");

    // 指令选择与发送按钮
    QLabel *lblCmd = new QLabel(QString::fromUtf8("选择控制指令:"), boxCmd); lblCmd->setStyleSheet(labelStyle);
    cmbCmdType = new QComboBox(boxCmd);
    cmbCmdType->addItems(QStringList() 
        << QString::fromUtf8("启动自寻北 (0x0011)")
        << QString::fromUtf8("惯性器件标定 (0x0011)")
        << QString::fromUtf8("快速航向对准 (0x0011)")
        << QString::fromUtf8("写入经纬高至Flash (0x0066)")
        << QString::fromUtf8("写入对准时间至Flash (0x0077)")
        << QString::fromUtf8("写入通信参数至Flash (0x0099)")
        << QString::fromUtf8("写入安装误差角至Flash (0x00AA)")
        << QString::fromUtf8("写入全部参数至Flash (0x00AA, 模式0x007F)")
        << QString::fromUtf8("写入姿态角转导航至Flash (0x00BB)")
        << QString::fromUtf8("写入陀螺零偏至Flash (0x00CC)")
        << QString::fromUtf8("写入杆臂至Flash (0x00DD)")
        << QString::fromUtf8("读取用户使用参数配置 (0xCD44, blk9)")
        << QString::fromUtf8("读取IMU零偏配置 (0xCD44, blk10)")
        << QString::fromUtf8("读取用户协议配置 (0xCD44, blk11)")
    );
    cmbCmdType->setView(new QListView(this));
    cmbCmdType->setStyleSheet(
        "QComboBox { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 3px; padding: 4px; font-weight: bold; }"
        "QComboBox QAbstractItemView { background-color: #2e3440; color: #d8dee9; selection-background-color: #434c5e; selection-color: #88c0d0; border: 1px solid #4c566a; outline: 0px; }"
        "QComboBox QAbstractItemView::item { min-height: 28px; }"
    );

    btnSendSerialCmd = new QPushButton(QString::fromUtf8("发送指令"), boxCmd);
    btnSendSerialCmd->setStyleSheet("QPushButton { background-color: #5e81ac; color: #eceff4; border-radius: 4px; padding: 6px 12px; font-weight: bold; } QPushButton:hover { background-color: #81a1c1; }");

    // 摆放布局
    layCmd->addWidget(lblLon, 0, 0); layCmd->addWidget(editLon, 0, 1);
    layCmd->addWidget(lblLat, 0, 2); layCmd->addWidget(editLat, 0, 3);
    layCmd->addWidget(lblAlt, 1, 0); layCmd->addWidget(editAlt, 1, 1);
    layCmd->addWidget(lblAlignTime, 1, 2); layCmd->addWidget(editAlignTime, 1, 3);
    layCmd->addWidget(lblBindHeading, 2, 0); layCmd->addWidget(editBindHeading, 2, 1);
    
    layCmd->addWidget(lblOutBaud, 3, 0); layCmd->addWidget(cmbOutBaud, 3, 1);
    layCmd->addWidget(lblOutPeriod, 3, 2); layCmd->addWidget(cmbOutPeriod, 3, 3);

    // Lever Arms (Front, Left, Up) in one row
    QHBoxLayout *layLeverArms = new QHBoxLayout();
    layLeverArms->setContentsMargins(0, 0, 0, 0);
    layLeverArms->setSpacing(6);
    layLeverArms->addWidget(lblFront);
    layLeverArms->addWidget(editFrontLever);
    layLeverArms->addWidget(lblRight);
    layLeverArms->addWidget(editRightLever);
    layLeverArms->addWidget(lblDown);
    layLeverArms->addWidget(editUpLever);
    layCmd->addLayout(layLeverArms, 4, 0, 1, 4);

    // Installation angles (Roll X, Yaw Y, Pitch Z) in one row
    QHBoxLayout *layAngles = new QHBoxLayout();
    layAngles->setContentsMargins(0, 0, 0, 0);
    layAngles->setSpacing(6);
    layAngles->addWidget(lblRollErr);
    layAngles->addWidget(editRollError);
    layAngles->addWidget(lblYawErr);
    layAngles->addWidget(editYawError);
    layAngles->addWidget(lblPitchErr);
    layAngles->addWidget(editPitchError);
    layCmd->addLayout(layAngles, 5, 0, 1, 4);

    layCmd->addWidget(lblCmd, 6, 0); layCmd->addWidget(cmbCmdType, 6, 1, 1, 3);
    layCmd->addWidget(btnSendSerialCmd, 7, 0, 1, 4);

    QLabel *lblSendHex = new QLabel(QString::fromUtf8("发送HEX (下行):"), boxCmd); lblSendHex->setStyleSheet(labelStyle);
    editSendHex = new QLineEdit(boxCmd);
    editSendHex->setReadOnly(true);
    editSendHex->setPlaceholderText(QString::fromUtf8("尚未发送指令"));
    editSendHex->setStyleSheet(editStyle);
    layCmd->addWidget(lblSendHex, 8, 0);
    layCmd->addWidget(editSendHex, 8, 1, 1, 3);

    layLeft->addWidget(boxCmd);

    // 1. 操作控制台日志组 (不打印 Hex 流，移到左面板底部)
    QGroupBox *boxConsole = new QGroupBox(QString::fromUtf8("操作日志与数据管理"), leftPanel);
    boxConsole->setStyleSheet(cardStyle);
    QVBoxLayout *layConsole = new QVBoxLayout(boxConsole);
    layConsole->setContentsMargins(10, 20, 10, 10);
    layConsole->setSpacing(10);

    QHBoxLayout *layLogBar = new QHBoxLayout();
    chkSaveSerial = new QCheckBox(QString::fromUtf8("保存串口日志"), boxConsole);
    chkSaveSerial->setStyleSheet("QCheckBox { color: #eceff4; font-size: 13px; font-weight: bold; }");
    layLogBar->addWidget(chkSaveSerial);

    btnDecodeBin = new QPushButton(QString::fromUtf8("解析离线BIN文件"), boxConsole);
    btnDecodeBin->setStyleSheet("QPushButton { background-color: #8fbcbb; color: #2e3440; border-radius: 4px; padding: 4px 12px; font-weight: bold; } QPushButton:hover { background-color: #a3d4d2; }");
    layLogBar->addWidget(btnDecodeBin);

    layLogBar->addStretch(1);

    serialConsoleLog = new QPlainTextEdit(boxConsole);
    serialConsoleLog->setReadOnly(true);
    serialConsoleLog->setStyleSheet("background-color: #1e222a; color: #eceff4; font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; border: 1px solid #3b4252; border-radius: 4px;");
    serialConsoleLog->document()->setMaximumBlockCount(200);

    layConsole->addLayout(layLogBar);
    layConsole->addWidget(serialConsoleLog);
    layConsole->setStretch(1, 1);

    layLeft->addWidget(boxConsole, 1); // 占据左下角拉伸空间

    mainSerialLayout->addWidget(leftPanel, 4);

    // ---------------- 右侧面板: 状态监视与日志 ----------------
    QWidget *rightPanel = new QWidget(serialTab);
    QVBoxLayout *layRight = new QVBoxLayout(rightPanel);
    layRight->setContentsMargins(0, 0, 0, 0);
    layRight->setSpacing(10);

    // 2. 实时数据监视组 (带大表格)
    QGroupBox *boxTable = new QGroupBox(QString::fromUtf8("透传解算实时数据表 (串口)"), rightPanel);
    boxTable->setStyleSheet(cardStyle);
    QVBoxLayout *layTable = new QVBoxLayout(boxTable);
    layTable->setContentsMargins(10, 20, 10, 10);
    layTable->setSpacing(15);

    QHBoxLayout *layGauges = new QHBoxLayout();
    layGauges->setSpacing(20);

    serialCompassWidget = new CompassWidget(serialTab);
    serialCompassWidget->setFixedSize(180, 180);
    serialCompassWidget->setRange180(false);

    serialAttitudeWidget = new AttitudeIndicatorWidget(serialTab);
    serialAttitudeWidget->setFixedSize(180, 180);

    layGauges->addStretch(1);
    layGauges->addWidget(serialCompassWidget);
    layGauges->addWidget(serialAttitudeWidget);
    layGauges->addStretch(1);

    layTable->addLayout(layGauges);

    serialTableWidget = new QTableWidget(boxTable);
    serialTableWidget->setRowCount(15);
    serialTableWidget->setColumnCount(13);
    serialTableWidget->setShowGrid(true);
    serialTableWidget->horizontalHeader()->hide();
    serialTableWidget->verticalHeader()->hide();
    serialTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    serialTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    serialTableWidget->setStyleSheet(
        "QTableWidget { background-color: #2e3440; gridline-color: #4c566a; color: #eceff4; font-size: 14px; }"
        "QTableWidget::item { border: 1px solid #4c566a; }"
    );

    // 初始化表格单元格
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 13; ++c) {
            serialTableWidget->setItem(r, c, new QTableWidgetItem(""));
            serialTableWidget->item(r, c)->setTextAlignment(Qt::AlignCenter);
        }
    }

    // 设置跨行合并 (Spans)
    serialTableWidget->setSpan(0, 0, 1, 2);
    serialTableWidget->setSpan(0, 2, 1, 2);
    serialTableWidget->setSpan(0, 4, 1, 2);
    serialTableWidget->setSpan(0, 6, 1, 2);
    serialTableWidget->setSpan(0, 8, 1, 2);
    serialTableWidget->setSpan(0, 10, 1, 3);
    serialTableWidget->setSpan(1, 1, 1, 3);
    serialTableWidget->setSpan(1, 4, 1, 3);
    serialTableWidget->setSpan(1, 7, 1, 3);
    serialTableWidget->setSpan(1, 10, 1, 3);
    serialTableWidget->setSpan(2, 0, 1, 13);
    serialTableWidget->setSpan(3, 1, 1, 4);
    serialTableWidget->setSpan(3, 5, 1, 4);
    serialTableWidget->setSpan(3, 9, 1, 4);
    serialTableWidget->setSpan(4, 1, 1, 4);
    serialTableWidget->setSpan(4, 5, 1, 4);
    serialTableWidget->setSpan(4, 9, 1, 4);
    serialTableWidget->setSpan(5, 1, 1, 4);
    serialTableWidget->setSpan(5, 5, 1, 4);
    serialTableWidget->setSpan(5, 9, 1, 4);
    serialTableWidget->setSpan(6, 1, 1, 3);
    serialTableWidget->setSpan(6, 4, 1, 3);
    serialTableWidget->setSpan(6, 7, 1, 3);
    serialTableWidget->setSpan(6, 10, 1, 3);
    serialTableWidget->setSpan(7, 1, 1, 3);
    serialTableWidget->setSpan(7, 4, 1, 3);
    serialTableWidget->setSpan(7, 7, 1, 3);
    serialTableWidget->setSpan(7, 10, 1, 3);
    serialTableWidget->setSpan(8, 0, 1, 13);
    serialTableWidget->setSpan(9, 1, 1, 4);
    serialTableWidget->setSpan(9, 5, 1, 4);
    serialTableWidget->setSpan(9, 9, 1, 4);
    serialTableWidget->setSpan(10, 1, 1, 4);
    serialTableWidget->setSpan(10, 5, 1, 4);
    serialTableWidget->setSpan(10, 9, 1, 4);
    serialTableWidget->setSpan(11, 0, 1, 13);
    serialTableWidget->setSpan(12, 1, 1, 4);
    serialTableWidget->setSpan(12, 5, 1, 4);
    serialTableWidget->setSpan(12, 9, 1, 4);
    serialTableWidget->setSpan(13, 1, 1, 4);
    serialTableWidget->setSpan(13, 5, 1, 4);
    serialTableWidget->setSpan(13, 9, 1, 4);
    serialTableWidget->setSpan(14, 0, 1, 2);
    serialTableWidget->setSpan(14, 2, 1, 2);
    serialTableWidget->setSpan(14, 4, 1, 2);
    serialTableWidget->setSpan(14, 6, 1, 3);
    serialTableWidget->setSpan(14, 9, 1, 2);
    serialTableWidget->setSpan(14, 11, 1, 2);

    // 填充静态文本
    serialTableWidget->item(0, 0)->setText(QString::fromUtf8("工作时间 (s)"));
    serialTableWidget->item(0, 4)->setText(QString::fromUtf8("对准总时间/剩余"));
    serialTableWidget->item(0, 8)->setText(QString::fromUtf8("芯片温度"));
    serialTableWidget->item(1, 0)->setText(QString::fromUtf8("状态字"));
    serialTableWidget->item(2, 0)->setText(QString::fromUtf8("--- 惯性测量与姿态数据 ---"));
    serialTableWidget->item(2, 0)->setForeground(QBrush(QColor("#88c0d0")));
    serialTableWidget->item(2, 0)->setFont(QFont("Arial", 12, QFont::Bold));
    serialTableWidget->item(3, 0)->setText(QString::fromUtf8("角速度(°/s)"));
    serialTableWidget->item(4, 0)->setText(QString::fromUtf8("加速度(g)"));
    serialTableWidget->item(5, 0)->setText(QString::fromUtf8("高精度姿态(°)"));
    serialTableWidget->item(6, 0)->setText(QString::fromUtf8("反欧拉角(°)"));
    serialTableWidget->item(7, 0)->setText(QString::fromUtf8("四元数 Q"));
    serialTableWidget->item(8, 0)->setText(QString::fromUtf8("--- 位置定位与运动速度 ---"));
    serialTableWidget->item(8, 0)->setForeground(QBrush(QColor("#88c0d0")));
    serialTableWidget->item(8, 0)->setFont(QFont("Arial", 12, QFont::Bold));
    serialTableWidget->item(9, 0)->setText(QString::fromUtf8("物理位置"));
    serialTableWidget->item(10, 0)->setText(QString::fromUtf8("导航速度(m/s)"));
    serialTableWidget->item(11, 0)->setText(QString::fromUtf8("--- 传感器错误与超量程计数 ---"));
    serialTableWidget->item(11, 0)->setForeground(QBrush(QColor("#88c0d0")));
    serialTableWidget->item(11, 0)->setFont(QFont("Arial", 12, QFont::Bold));
    serialTableWidget->item(12, 0)->setText(QString::fromUtf8("陀螺超量程"));
    serialTableWidget->item(13, 0)->setText(QString::fromUtf8("加表超量程"));
    serialTableWidget->item(14, 0)->setText(QString::fromUtf8("协议格式"));
    serialTableWidget->item(14, 4)->setText(QString::fromUtf8("垂向标志"));
    serialTableWidget->item(14, 9)->setText(QString::fromUtf8("帧计数"));

    // 默认展示初始值
    serialTableWidget->item(0, 2)->setText("0.00");
    serialTableWidget->item(0, 6)->setText("0 / 0");
    serialTableWidget->item(0, 10)->setText(QString::fromUtf8("陀螺: 0℃ / 加表: 0℃"));
    serialTableWidget->item(1, 1)->setText(QString::fromUtf8("系统: IDLE"));
    serialTableWidget->item(1, 4)->setText(QString::fromUtf8("超量程: 0b000000"));
    serialTableWidget->item(1, 7)->setText(QString::fromUtf8("通道: 0"));
    serialTableWidget->item(1, 10)->setText(QString::fromUtf8("回令: 0"));
    serialTableWidget->item(3, 1)->setText("X: 0.000000");
    serialTableWidget->item(3, 5)->setText("Y: 0.000000");
    serialTableWidget->item(3, 9)->setText("Z: 0.000000");
    serialTableWidget->item(4, 1)->setText("X: 0.000000");
    serialTableWidget->item(4, 5)->setText("Y: 0.000000");
    serialTableWidget->item(4, 9)->setText("Z: 0.000000");
    serialTableWidget->item(5, 1)->setText("Roll: 0.000");
    serialTableWidget->item(5, 5)->setText("Heading: 0.000");
    serialTableWidget->item(5, 9)->setText("Pitch: 0.000");
    serialTableWidget->item(6, 1)->setText("Roll: 0.000");
    serialTableWidget->item(6, 4)->setText("Heading: 0.000");
    serialTableWidget->item(6, 7)->setText("Pitch: 0.000");
    serialTableWidget->item(6, 10)->setText("Flag: 0");
    serialTableWidget->item(7, 1)->setText("Q1: 0.000000");
    serialTableWidget->item(7, 4)->setText("Q2: 0.000000");
    serialTableWidget->item(7, 7)->setText("Q3: 0.000000");
    serialTableWidget->item(7, 10)->setText("Q4: 0.000000");
    serialTableWidget->item(9, 1)->setText(QString::fromUtf8("纬度: 0.00000000"));
    serialTableWidget->item(9, 5)->setText(QString::fromUtf8("经度: 0.00000000"));
    serialTableWidget->item(9, 9)->setText(QString::fromUtf8("高程: 0.00m"));
    serialTableWidget->item(10, 1)->setText(QString::fromUtf8("北: 0.0000"));
    serialTableWidget->item(10, 5)->setText(QString::fromUtf8("天: 0.0000"));
    serialTableWidget->item(10, 9)->setText(QString::fromUtf8("东: 0.0000"));
    serialTableWidget->item(12, 1)->setText("Gx: 0");
    serialTableWidget->item(12, 5)->setText("Gy: 0");
    serialTableWidget->item(12, 9)->setText("Gz: 0");
    serialTableWidget->item(13, 1)->setText("Ax: 0");
    serialTableWidget->item(13, 5)->setText("Ay: 0");
    serialTableWidget->item(13, 9)->setText("Az: 0");
    serialTableWidget->item(14, 2)->setText(QString::fromUtf8("通用寻北协议"));
    serialTableWidget->item(14, 6)->setText(QString::fromUtf8("水平模式"));
    serialTableWidget->item(14, 11)->setText("0");

    // 上色
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 13; ++c) {
            QTableWidgetItem *it = serialTableWidget->item(r, c);
            if (r == 2 || r == 8 || r == 11) {
                it->setBackground(QBrush(QColor("#3b4252")));
            } else if (c == 0 || (r == 0 && (c == 4 || c == 8)) || (r == 14 && (c == 4 || c == 9))) {
                it->setBackground(QBrush(QColor("#2e3440")));
                it->setForeground(QBrush(QColor("#d8dee9")));
                it->setFont(QFont("Arial", 11, QFont::Bold));
            } else {
                it->setBackground(QBrush(QColor("#3b4252")));
                it->setForeground(QBrush(QColor("#eceff4")));
            }
        }
    }
    serialTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    serialTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    layTable->addWidget(serialTableWidget, 1);

    layRight->addWidget(boxTable, 1); // 100% 高度拉伸

    mainSerialLayout->addWidget(rightPanel, 6);

    // ---------------- 绑定槽函数 ----------------
    connect(btnRefreshSerial, &QPushButton::clicked, this, &MainWindow::onRefreshSerialPorts);
    connect(btnToggleSerial, &QPushButton::clicked, this, &MainWindow::onToggleSerialConnection);
    connect(btnSendSerialCmd, &QPushButton::clicked, this, &MainWindow::onSendSerialCommand);
    connect(chkSaveSerial, &QCheckBox::stateChanged, this, &MainWindow::onSaveSerialStateChanged);
    connect(btnDecodeBin, &QPushButton::clicked, this, &MainWindow::onDecodeBinClicked);

    // 扫描可用串口并加载上次选择的串口配置
    onRefreshSerialPorts();
    
    QSettings settings("SQ_CAN_APP", "SerialConfig");
    QString lastPort = settings.value("lastPort").toString();
    QString lastBaud = settings.value("lastBaud").toString();
    if (!lastPort.isEmpty()) {
        int portIdx = serialPortCombo->findData(lastPort);
        if (portIdx == -1) {
            portIdx = serialPortCombo->findText(lastPort);
        }
        if (portIdx != -1) {
            serialPortCombo->setCurrentIndex(portIdx);
        }
    }
    if (!lastBaud.isEmpty()) {
        int baudIdx = serialBaudCombo->findText(lastBaud);
        if (baudIdx != -1) {
            serialBaudCombo->setCurrentIndex(baudIdx);
        }
    }
}

QByteArray MainWindow::encodeCommandFrame(double lon, double lat, double alt, double frontLever, double rightLever, double upLever, int validMode, int baudrate, int period, double rollErr, double yawErr, double pitchErr, int userCmd, double alignTime)
{
    QByteArray bytes(40, 0);
    // Header
    bytes[0] = 0x55;
    bytes[1] = 0xAA;
    bytes[2] = 0x24; // 36 data bytes
    
    // Parameters conversion
    int lonVal = qRound(lon / 0.0000001);
    int latVal = qRound(lat / 0.0000001);
    float altVal = static_cast<float>(alt);
    float frontVal = static_cast<float>(frontLever / 0.01);
    unsigned short validVal = static_cast<unsigned short>(validMode);
    short upVal = qRound(upLever / 0.01);
    short rightVal = qRound(rightLever / 0.01);
    unsigned char baudVal = static_cast<unsigned char>(baudrate);
    unsigned char periodVal = static_cast<unsigned char>(period);
    short rollErrVal = qRound(rollErr / 0.006);
    short yawErrVal = qRound(yawErr / 0.006);
    short pitchErrVal = qRound(pitchErr / 0.006);
    unsigned short userCmdVal = static_cast<unsigned short>(userCmd);
    unsigned short alignTimeVal = qRound(alignTime / 0.1);
    
    unsigned char *ptr = reinterpret_cast<unsigned char*>(bytes.data());
    
    // helper to write values in little endian format
    auto write32 = [&](int offset, int val) {
        ptr[offset] = val & 0xFF;
        ptr[offset+1] = (val >> 8) & 0xFF;
        ptr[offset+2] = (val >> 16) & 0xFF;
        ptr[offset+3] = (val >> 24) & 0xFF;
    };
    auto writeFloat = [&](int offset, float val) {
        int val_i;
        memcpy(&val_i, &val, sizeof(float));
        write32(offset, val_i);
    };
    auto write16 = [&](int offset, int val) {
        ptr[offset] = val & 0xFF;
        ptr[offset+1] = (val >> 8) & 0xFF;
    };
    
    write32(3, lonVal);
    write32(7, latVal);
    writeFloat(11, altVal);
    writeFloat(15, frontVal);
    write16(19, validVal);
    write16(21, upVal);
    write16(23, rightVal);
    ptr[25] = baudVal;
    ptr[26] = periodVal;
    write16(27, rollErrVal);
    write16(29, yawErrVal);
    write16(31, pitchErrVal);
    write16(33, 0); // backup
    write16(35, userCmdVal);
    write16(37, alignTimeVal);
    
    // Checksum
    int sum = 0;
    for (int i = 2; i < 39; ++i) {
        sum += ptr[i];
    }
    bytes[39] = sum & 0xFF;
    
    return bytes;
}

void MainWindow::onRefreshSerialPorts()
{
    serialPortCombo->clear();
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        QString displayName = port.portName();
        if (!port.description().isEmpty()) {
            displayName += " (" + port.description() + ")";
        }
        serialPortCombo->addItem(displayName, port.portName());
    }
}

void MainWindow::onToggleSerialConnection()
{
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
        btnToggleSerial->setText(QString::fromUtf8("打开串口"));
        btnToggleSerial->setStyleSheet("QPushButton { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; border-radius: 4px; padding: 5px 10px; font-weight: bold; } QPushButton:hover { background-color: #434c5e; }");
        serialPortCombo->setEnabled(true);
        serialBaudCombo->setEnabled(true);
        btnRefreshSerial->setEnabled(true);
        serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + QString::fromUtf8("串口已关闭。"));
        return;
    }
    
    QString portName = serialPortCombo->currentData().toString();
    if (portName.isEmpty()) {
        portName = serialPortCombo->currentText();
    }
    
    if (portName.isEmpty()) {
        QMessageBox::warning(this, "警告", "没有可用的串口端口！");
        return;
    }
    
    if (!m_serialPort) {
        m_serialPort = new QSerialPort(this);
        m_serialUiUpdateCounter = 0;
        connect(m_serialPort, &QSerialPort::readyRead, this, &MainWindow::onSerialReadyRead);
    }
    
    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(serialBaudCombo->currentText().toInt());
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    
    if (m_serialPort->open(QIODevice::ReadWrite)) {
        btnToggleSerial->setText(QString::fromUtf8("关闭串口"));
        btnToggleSerial->setStyleSheet("QPushButton { background-color: #bf616a; color: #eceff4; border: 1px solid #4c566a; border-radius: 4px; padding: 5px 10px; font-weight: bold; } QPushButton:hover { background-color: #c9767e; }");
        serialPortCombo->setEnabled(false);
        serialBaudCombo->setEnabled(false);
        btnRefreshSerial->setEnabled(false);
        serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + QString::fromUtf8("串口连接成功！"));
        
        // 保存上次连接的配置 (保存实际串口名，方便下一次识别加载)
        QSettings settings("SQ_CAN_APP", "SerialConfig");
        settings.setValue("lastPort", portName);
        settings.setValue("lastBaud", serialBaudCombo->currentText());
    } else {
        QMessageBox::warning(this, "错误", QString::fromUtf8("打开串口失败：%1").arg(m_serialPort->errorString()));
    }
}

void MainWindow::onSerialReadyRead()
{
    if (!m_serialPort) return;
    m_serialBuffer.append(m_serialPort->readAll());
    
    while (m_serialBuffer.size() >= 3) {
        int headerIdx = -1;
        for (int i = 0; i < m_serialBuffer.size() - 1; ++i) {
            if (static_cast<unsigned char>(m_serialBuffer[i]) == 0x55 && 
                static_cast<unsigned char>(m_serialBuffer[i+1]) == 0xAA) {
                headerIdx = i;
                break;
            }
        }
        
        if (headerIdx == -1) {
            if (static_cast<unsigned char>(m_serialBuffer.at(m_serialBuffer.size() - 1)) == 0x55) {
                m_serialBuffer = m_serialBuffer.mid(m_serialBuffer.size() - 1);
            } else {
                m_serialBuffer.clear();
            }
            break;
        }
        
        if (headerIdx > 0) {
            m_serialBuffer.remove(0, headerIdx);
        }
        
        if (m_serialBuffer.size() < 3) {
            break;
        }
        
        unsigned char lenByte = static_cast<unsigned char>(m_serialBuffer.at(2));
        int expectedSize = 0;
        if (lenByte == 0x5C) {
            expectedSize = 96;
        } else if (lenByte == 0xFC) {
            expectedSize = 256;
        } else {
            m_serialBuffer.remove(0, 2);
            continue;
        }
        
        if (m_serialBuffer.size() < expectedSize) {
            break;
        }
        
        QByteArray packet = m_serialBuffer.left(expectedSize);
        unsigned char *bytes = reinterpret_cast<unsigned char*>(packet.data());
        
        if (expectedSize == 256) {
            // Verify 8-bit sum checksum
            int sum = 0;
            for (int i = 2; i < 255; ++i) {
                sum += bytes[i];
            }
            if ((sum & 0xFF) != bytes[255]) {
                m_serialBuffer.remove(0, 2);
                continue;
            }
            
            // Check DownCMDFeedback is 0xFD44 (little endian 44 FD)
            if (bytes[3] != 0x44 || bytes[4] != 0xFD) {
                m_serialBuffer.remove(0, 2);
                continue;
            }
            
            int blockId = bytes[7];
            QByteArray blockData = packet.mid(11, 240);
            showFlashBlockData(blockId, blockData, packet);
            
            m_serialBuffer.remove(0, 256);
            continue;
        }
        
        int sum = 0;
        for (int i = 2; i < 95; ++i) {
            sum += bytes[i];
        }
        
        if ((sum & 0xFF) != bytes[95]) {
            m_serialBuffer.remove(0, 2);
            continue;
        }
        
        // 解析小端字节数据
        auto readInt32 = [&](int offset) -> int {
            return bytes[offset] | (bytes[offset+1] << 8) | (bytes[offset+2] << 16) | (bytes[offset+3] << 24);
        };
        auto readUint16 = [&](int offset) -> unsigned short {
            return bytes[offset] | (bytes[offset+1] << 8);
        };
        auto readInt16 = [&](int offset) -> short {
            return static_cast<short>(bytes[offset] | (bytes[offset+1] << 8));
        };
        auto readFloat32 = [&](int offset) -> float {
            float f;
            int val = readInt32(offset);
            memcpy(&f, &val, sizeof(float));
            return f;
        };
        
        unsigned int pcNum = readInt32(3);
        unsigned char sysStatus = bytes[7];
        unsigned char overRangeStatus = bytes[9];
        unsigned char cmdAck = bytes[10];
        unsigned short totalAlignTime = readUint16(11);
        unsigned short remainAlignTime = readUint16(13);
        
        // 检查指令回复并打印到日志控制台
        if (m_isWaitingForAck) {
            if (cmdAck == 0x10) {
                serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + QString::fromUtf8("设备回令确认：指令已成功接收并执行！(0x10)"));
                m_isWaitingForAck = false;
            } else if (cmdAck == 0x1F) {
                serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + QString::fromUtf8("设备回令确认：指令执行失败/报错！(0x1F)"));
                m_isWaitingForAck = false;
            } else if (m_lastCommandSentTime.msecsTo(QTime::currentTime()) > 2000) {
                serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + QString::fromUtf8("设备回令确认超时！未收到有效应答。"));
                m_isWaitingForAck = false;
            }
        }
        
        double gyroX = readInt32(15) * 0.0000005;
        double gyroY = readInt32(19) * 0.0000005;
        double gyroZ = readInt32(23) * 0.0000005;
        double accelX = readInt32(27) * 0.000001;
        double accelY = readInt32(31) * 0.000001;
        double accelZ = readInt32(35) * 0.000001;
        
        double longitude = readInt32(39) * 0.0000001;
        double latitude = readInt32(43) * 0.0000001;
        double altitude = readInt16(47) * 0.5;
        
        double velN = readInt16(49) * 0.04;
        double velU = readInt16(51) * 0.04;
        double velE = readInt16(53) * 0.04;
        
        double roll = readInt16(55) * 0.006;
        double yaw = readUint16(57) * 0.006;
        double pitch = readInt16(59) * 0.006;
        
        double yaw180 = yaw;
        if (yaw180 > 180.0) {
            yaw180 -= 360.0;
        }

        // 增加显示降采样逻辑 (只过滤UI刷新，不影响原始数据文件记录保存)
        m_serialUiUpdateCounter++;

        int dsFactor = 1;
        int dsIdx = serialDownsampleCombo->currentIndex();
        if (dsIdx == 1) dsFactor = 10;
        else if (dsIdx == 2) dsFactor = 20;
        else if (dsIdx == 3) dsFactor = 100;
        else if (dsIdx == 4) dsFactor = 200;

        if (m_serialUiUpdateCounter % dsFactor == 0) {
            // 更新大表格显示 (对应 rawTableWidget 填充方式)
            serialTableWidget->item(0, 2)->setText(QString::number(pcNum * 0.005, 'f', 2));
            serialTableWidget->item(0, 6)->setText(QString("%1 / %2").arg(totalAlignTime).arg(remainAlignTime));
            serialTableWidget->item(0, 10)->setText(QString::fromUtf8("陀螺: %1℃ / 加表: %2℃").arg(static_cast<int>(bytes[61])).arg(static_cast<int>(bytes[62])));
            
            QString sysStateStr = QString::fromUtf8("未定义");
            switch (sysStatus) {
                case 0x10: sysStateStr = QString::fromUtf8("系统自检中"); break;
                case 0x12: sysStateStr = QString::fromUtf8("自检好"); break;
                case 0x1F: sysStateStr = QString::fromUtf8("自检错误"); break;
                case 0x33: sysStateStr = QString::fromUtf8("粗对准"); break;
                case 0x35: sysStateStr = QString::fromUtf8("精对准"); break;
                case 0x41: sysStateStr = QString::fromUtf8("导航中"); break;
                case 0x43: sysStateStr = QString::fromUtf8("零速修正"); break;
                default: break;
            }
            serialTableWidget->item(1, 1)->setText(QString::fromUtf8("系统: ") + sysStateStr);
            
            serialTableWidget->item(1, 4)->setText(QString::fromUtf8("超量程: 0b%1").arg(QString::number(overRangeStatus, 2).rightJustified(6, '0')));
            serialTableWidget->item(1, 7)->setText(QString::fromUtf8("通道: 串口"));
            
            QString ackStr = QString::number(cmdAck);
            if (cmdAck == 0x10) ackStr = QString::fromUtf8("成功(0x10)");
            else if (cmdAck == 0x1F) ackStr = QString::fromUtf8("报错(0x1F)");
            serialTableWidget->item(1, 10)->setText(QString::fromUtf8("回令: ") + ackStr);
            
            serialTableWidget->item(3, 1)->setText(QString("X: %1").arg(QString::number(gyroX, 'f', 6)));
            serialTableWidget->item(3, 5)->setText(QString("Y: %1").arg(QString::number(gyroY, 'f', 6)));
            serialTableWidget->item(3, 9)->setText(QString("Z: %1").arg(QString::number(gyroZ, 'f', 6)));
            
            serialTableWidget->item(4, 1)->setText(QString("X: %1").arg(QString::number(accelX, 'f', 6)));
            serialTableWidget->item(4, 5)->setText(QString("Y: %1").arg(QString::number(accelY, 'f', 6)));
            serialTableWidget->item(4, 9)->setText(QString("Z: %1").arg(QString::number(accelZ, 'f', 6)));
            
            serialTableWidget->item(5, 1)->setText(QString("Roll: %1").arg(QString::number(roll, 'f', 3)));
            serialTableWidget->item(5, 5)->setText(QString("Heading: %1").arg(QString::number(yaw, 'f', 3)));
            serialTableWidget->item(5, 9)->setText(QString("Pitch: %1").arg(QString::number(pitch, 'f', 3)));
            
            serialTableWidget->item(6, 1)->setText(QString("Roll: %1").arg(QString::number(readInt16(87) * 0.006, 'f', 3)));
            serialTableWidget->item(6, 4)->setText(QString("Heading: %1").arg(QString::number(readUint16(89) * 0.006, 'f', 3)));
            serialTableWidget->item(6, 7)->setText(QString("Pitch: %1").arg(QString::number(readInt16(91) * 0.006, 'f', 3)));
            serialTableWidget->item(6, 10)->setText(QString("Flag: %1").arg(bytes[93]));
            
            serialTableWidget->item(7, 1)->setText(QString("Q1: %1").arg(QString::number(readFloat32(63), 'f', 6)));
            serialTableWidget->item(7, 4)->setText(QString("Q2: %1").arg(QString::number(readFloat32(67), 'f', 6)));
            serialTableWidget->item(7, 7)->setText(QString("Q3: %1").arg(QString::number(readFloat32(71), 'f', 6)));
            serialTableWidget->item(7, 10)->setText(QString("Q4: %1").arg(QString::number(readFloat32(75), 'f', 6)));
            
            serialTableWidget->item(9, 1)->setText(QString(QString::fromUtf8("纬度: %1")).arg(QString::number(latitude, 'f', 8)));
            serialTableWidget->item(9, 5)->setText(QString(QString::fromUtf8("经度: %1")).arg(QString::number(longitude, 'f', 8)));
            serialTableWidget->item(9, 9)->setText(QString(QString::fromUtf8("高程: %1m")).arg(QString::number(altitude, 'f', 2)));
            
            serialTableWidget->item(10, 1)->setText(QString(QString::fromUtf8("北: %1")).arg(QString::number(velN, 'f', 4)));
            serialTableWidget->item(10, 5)->setText(QString(QString::fromUtf8("天: %1")).arg(QString::number(velU, 'f', 4)));
            serialTableWidget->item(10, 9)->setText(QString(QString::fromUtf8("东: %1")).arg(QString::number(velE, 'f', 4)));
            
            serialTableWidget->item(12, 1)->setText(QString("Gx: %1").arg(bytes[79]));
            serialTableWidget->item(12, 5)->setText(QString("Gy: %1").arg(bytes[80]));
            serialTableWidget->item(12, 9)->setText(QString("Gz: %1").arg(bytes[81]));
            
            serialTableWidget->item(13, 1)->setText(QString("Ax: %1").arg(bytes[82]));
            serialTableWidget->item(13, 5)->setText(QString("Ay: %1").arg(bytes[83]));
            serialTableWidget->item(13, 9)->setText(QString("Az: %1").arg(bytes[84]));
            
            serialTableWidget->item(14, 2)->setText(bytes[94] == 0xFF ? QString::fromUtf8("通用石油寻北") : QString::fromUtf8("通用寻北协议"));
            serialTableWidget->item(14, 6)->setText(bytes[93] == 1 ? QString::fromUtf8("垂向模式") : QString::fromUtf8("水平模式"));
            serialTableWidget->item(14, 11)->setText(QString::number(pcNum));
            
            double serialDisplayRoll = 0.0;
            double serialDisplayPitch = 0.0;
            double serialDisplayYaw = 0.0;
            convertDisplayAttitude(roll, pitch, yaw,
                                   serialDisplayRoll, serialDisplayPitch, serialDisplayYaw);
            serialCompassWidget->setYaw(serialDisplayYaw);
            serialAttitudeWidget->setRollPitch(serialDisplayRoll, serialDisplayPitch);
        }
        
        char gyroTemp = static_cast<char>(bytes[61]);
        char accelTemp = static_cast<char>(bytes[62]);
        
        double q1 = readFloat32(63);
        double q2 = readFloat32(67);
        double q3 = readFloat32(71);
        double q4 = readFloat32(75);
        
        unsigned char GxLimit = bytes[79];
        unsigned char GyLimit = bytes[80];
        unsigned char GzLimit = bytes[81];
        unsigned char AxLimit = bytes[82];
        unsigned char AyLimit = bytes[83];
        unsigned char AzLimit = bytes[84];
        
        double revRoll = readInt16(87) * 0.006;
        double revYaw = readUint16(89) * 0.006;
        double revPitch = readInt16(91) * 0.006;
        unsigned char verticalFlag = bytes[93];
        unsigned char outputFormat = bytes[94];
        
        // 保存数据
        if (m_serialTxtFile.isOpen()) {
            m_serialTxtStream << QTime::currentTime().toString("hh:mm:ss zzz") << ","
                              << QString::number(pcNum) << ","
                              << QString::number(totalAlignTime) << "/" << QString::number(remainAlignTime) << ","
                              << QString::number(sysStatus) << ","
                              << QString::number(overRangeStatus) << ","
                              << QString::number(outputFormat) << ","
                              << QString::number(cmdAck) << ","
                              << QString::number(gyroX, 'f', 6) << ","
                              << QString::number(gyroY, 'f', 6) << ","
                              << QString::number(gyroZ, 'f', 6) << ","
                              << QString::number(accelX, 'f', 6) << ","
                              << QString::number(accelY, 'f', 6) << ","
                              << QString::number(accelZ, 'f', 6) << ","
                              << QString::number(latitude, 'f', 7) << ","
                              << QString::number(longitude, 'f', 7) << ","
                              << QString::number(altitude, 'f', 1) << ","
                              << QString::number(velN, 'f', 2) << ","
                              << QString::number(velU, 'f', 2) << ","
                              << QString::number(velE, 'f', 2) << ","
                              << QString::number(roll, 'f', 3) << ","
                              << QString::number(yaw, 'f', 3) << ","
                              << QString::number(pitch, 'f', 3) << ","
                              << QString::number(revRoll, 'f', 3) << ","
                              << QString::number(revYaw, 'f', 3) << ","
                              << QString::number(revPitch, 'f', 3) << ","
                              << QString::number(gyroTemp) << ","
                              << QString::number(accelTemp) << ","
                              << QString::number(q1, 'f', 6) << ","
                              << QString::number(q2, 'f', 6) << ","
                              << QString::number(q3, 'f', 6) << ","
                              << QString::number(q4, 'f', 6) << ","
                              << QString::number(GxLimit) << ","
                              << QString::number(GyLimit) << ","
                              << QString::number(GzLimit) << ","
                              << QString::number(AxLimit) << ","
                              << QString::number(AyLimit) << ","
                              << QString::number(AzLimit) << ","
                              << QString::number(verticalFlag) << ","
                              << QString::number(outputFormat) << "\n";
        }
        
        if (m_serialBinFile.isOpen()) {
            m_serialBinFile.write(packet);
        }
        
        m_serialBuffer.remove(0, expectedSize);
    }
}

void MainWindow::onSendSerialCommand()
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        QMessageBox::warning(this, "警告", "串口未连接，指令无法下发！");
        return;
    }
    
    double lon = editLon->text().toDouble();
    double lat = editLat->text().toDouble();
    double alt = editAlt->text().toDouble();
    double time = editAlignTime->text().toDouble();
    double bindHeading = editBindHeading->text().toDouble();
    
    double frontLever = editFrontLever->text().toDouble();
    double rightLever = editRightLever->text().toDouble();
    double upLever = editUpLever->text().toDouble();
    
    double rollErr = editRollError->text().toDouble();
    double pitchErr = editPitchError->text().toDouble();
    double yawErr = editYawError->text().toDouble();
    
    int periodIdx = cmbOutPeriod->currentIndex();
    
    int baudrateVal = 2; // 默认 115200 (2)
    QVariant baudData = cmbOutBaud->currentData();
    if (baudData.isValid()) {
        baudrateVal = baudData.toInt();
    }
    int periodVal = periodIdx;
    
    int cmdType = cmbCmdType->currentIndex();
    int userCmdVal = 0;
    int validModeVal = 0;
    double rollErrVal = 0;
    double yawErrVal = 0;
    double pitchErrVal = 0;
    double alignTimeVal = time;
    
    switch (cmdType) {
        case 0: // 自寻北
            userCmdVal = 0x0011;
            validModeVal = 0x0000;
            break;
        case 1: // 惯性标定
            userCmdVal = 0x0011;
            validModeVal = 0x0001; // bit0 LLA valid
            break;
        case 2: // 快速对准
            userCmdVal = 0x0011;
            validModeVal = 0x0007; // bit0 LLA, bit1 heading, bit2 time valid
            yawErrVal = bindHeading;
            break;
        case 3: // 写入LLA
            userCmdVal = 0x0066;
            validModeVal = 0x0001;
            break;
        case 4: // 写入对准时间
            userCmdVal = 0x0077;
            validModeVal = 0x0004;
            break;
        case 5: // 写入通信参数
            userCmdVal = 0x0099;
            validModeVal = 0x0008;
            break;
        case 6: // 写入安装误差角
            userCmdVal = 0x00AA;
            validModeVal = 0x0010;
            rollErrVal = rollErr;
            yawErrVal = yawErr;
            pitchErrVal = pitchErr;
            break;
        case 7: // 写入全部参数 (包括三个杆臂)
            userCmdVal = 0x00AA;
            validModeVal = 0x007F; // bit0..bit6 全有效
            rollErrVal = rollErr;
            yawErrVal = yawErr;
            pitchErrVal = pitchErr;
            break;
        case 8: // 写入姿态角到导航
            userCmdVal = 0x00BB;
            validModeVal = 0x0020; // bit5 valid
            rollErrVal = rollErr;
            yawErrVal = yawErr;
            pitchErrVal = pitchErr;
            break;
        case 9: // 写入陀螺零偏
            userCmdVal = 0x00CC;
            validModeVal = 0x0040; // bit6 valid
            rollErrVal = rollErr;
            yawErrVal = yawErr;
            pitchErrVal = pitchErr;
            break;
        case 10: // 写入杆臂
            userCmdVal = 0x00DD;
            validModeVal = 0x0080; // bit7 valid
            break;
        default:
            break;
    }
    
    QByteArray cmdBytes;
    if (cmdType >= 11 && cmdType <= 13) {
        cmdBytes.resize(20);
        cmdBytes.fill(0);
        cmdBytes[0] = static_cast<char>(0x55);
        cmdBytes[1] = static_cast<char>(0xAA);
        cmdBytes[2] = static_cast<char>(0x10);
        // DownCMD is 0xCD44 (little endian: 44 CD)
        cmdBytes[3] = static_cast<char>(0x44);
        cmdBytes[4] = static_cast<char>(0xCD);
        // Block ID
        if (cmdType == 11) cmdBytes[7] = static_cast<char>(0x09);
        else if (cmdType == 12) cmdBytes[7] = static_cast<char>(0x0A);
        else if (cmdType == 13) cmdBytes[7] = static_cast<char>(0x0B);

        int sum = 0;
        for (int i = 2; i < 19; ++i) {
            sum += static_cast<unsigned char>(cmdBytes[i]);
        }
        cmdBytes[19] = static_cast<char>(sum & 0xFF);
    } else {
        cmdBytes = encodeCommandFrame(lon, lat, alt, frontLever, rightLever, upLever, validModeVal, baudrateVal, periodVal, rollErrVal, yawErrVal, pitchErrVal, userCmdVal, alignTimeVal);
    }

    QString hexStr = cmdBytes.toHex().toUpper();
    QString formattedHex;
    for (int i = 0; i < hexStr.length(); i += 2) {
        formattedHex.append(hexStr.mid(i, 2) + " ");
    }
    formattedHex = formattedHex.trimmed();
    editSendHex->setText(formattedHex);
    serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + QString::fromUtf8("发送 HEX: ") + formattedHex);
    
    m_serialPort->write(cmdBytes);
    m_serialPort->flush();
    
    if (cmdType >= 11 && cmdType <= 13) {
        m_isWaitingForAck = false;
        QString blockName = (cmdType == 11) ? "blk9" : ((cmdType == 12) ? "blk10" : "blk11");
        serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + QString::fromUtf8("发送读取指令成功：读取 %1 块配置参数...").arg(blockName));
    } else {
        m_isWaitingForAck = true;
        m_lastCommandSentTime = QTime::currentTime();
        serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + QString::fromUtf8("发送指令成功：0x%1 (模式 0x%2)")
                                      .arg(userCmdVal, 4, 16, QChar('0'))
                                      .arg(validModeVal, 4, 16, QChar('0')).toUpper());
    }
}

void MainWindow::onSaveSerialStateChanged(int state)
{
    if (state == Qt::Checked) {
        QString defaultPath = QDir::currentPath() + "/serial_log_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt";
        QString txtFileName = QFileDialog::getSaveFileName(this, QString::fromUtf8("选择串口日志保存路径"), defaultPath, "Text files (*.txt)");
        if (txtFileName.isEmpty()) {
            chkSaveSerial->blockSignals(true);
            chkSaveSerial->setChecked(false);
            chkSaveSerial->blockSignals(false);
            return;
        }

        QString binFileName = txtFileName;
        if (binFileName.endsWith(".txt", Qt::CaseInsensitive)) {
            binFileName.chop(4);
            binFileName += ".bin";
        } else {
            binFileName += ".bin";
        }

        m_serialTxtFile.setFileName(txtFileName);
        m_serialBinFile.setFileName(binFileName);

        bool txtOk = m_serialTxtFile.open(QIODevice::WriteOnly | QIODevice::Text);
        bool binOk = m_serialBinFile.open(QIODevice::WriteOnly);

        if (txtOk && binOk) {
            m_serialTxtStream.setDevice(&m_serialTxtFile);
            m_serialTxtStream.setCodec("UTF-8");
            
            m_serialTxtStream << "时间,工作时间(s),对准时间(总/剩),系统状态,超量程状态,输出通道,命令回复,"
                                 "角速度X(°/s),角速度Y(°/s),角速度Z(°/s),加速度X(g),加速度Y(g),加速度Z(g),"
                                 "纬度(°),经度(°),高程(m),北速度(m/s),天速度(m/s),东速度(m/s),"
                                 "翻滚角(°),偏航角(°),俯仰角(°),反翻滚角(°),反偏航角(°),反俯仰角(°),"
                                 "陀螺温度(℃),加表温度(℃),四元数Q1,四元数Q2,四元数Q3,四元数Q4,"
                                 "陀螺超量程X,陀螺超量程Y,陀螺超量程Z,加表超量程X,加表超量程Y,加表超量程Z,"
                                 "垂向标志,协议格式\n";
        } else {
            QMessageBox::warning(this, QString::fromUtf8("警告"), QString::fromUtf8("无法创建日志文件:\n") + txtFileName + "\n" + binFileName);
            if (m_serialTxtFile.isOpen()) m_serialTxtFile.close();
            if (m_serialBinFile.isOpen()) m_serialBinFile.close();
            chkSaveSerial->blockSignals(true);
            chkSaveSerial->setChecked(false);
            chkSaveSerial->blockSignals(false);
        }
    } else {
        if (m_serialTxtFile.isOpen()) {
            m_serialTxtFile.close();
        }
        if (m_serialBinFile.isOpen()) {
            m_serialBinFile.close();
        }
    }
}

void MainWindow::onDecodeBinClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择要解析的离线BIN文件"), QDir::currentPath(), "Binary files (*.bin)");
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开所选二进制文件！");
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QString csvName = fileName;
    if (csvName.endsWith(".bin", Qt::CaseInsensitive)) {
        csvName.chop(4);
    }
    csvName += "_decoded.csv";
    
    QFile csvFile(csvName);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建输出的CSV解码文件！");
        return;
    }
    
    QTextStream csvStream(&csvFile);
    csvStream.setCodec("UTF-8");
    
    // 写入表头
    csvStream << "PC_Num,System_Status,Total_Align_Time,Remain_Align_Time,Gyro_X(deg/s),Gyro_Y(deg/s),Gyro_Z(deg/s),"
                 "Accel_X(m/s2),Accel_Y(m/s2),Accel_Z(m/s2),Longitude(deg),Latitude(deg),Altitude(m),"
                 "Vel_N(m/s),Vel_U(m/s),Vel_E(m/s),Roll(deg),Yaw(deg),Pitch(deg),Gyro_Temp(C),Accel_Temp(C),"
                 "Virtual_Yaw(deg),Vertical_Flag\n";
                 
    int len = data.size();
    const unsigned char *bytes = reinterpret_cast<const unsigned char*>(data.constData());
    
    int i = 0;
    int parsedCount = 0;
    while (i <= len - 96) {
        if (bytes[i] == 0x55 && bytes[i+1] == 0xAA) {
            if (bytes[i+2] == 92) {
                int sum = 0;
                for (int k = 2; k < 95; ++k) {
                    sum += bytes[i+k];
                }
                
                if ((sum & 0xFF) == bytes[i+95]) {
                    auto readInt32 = [&](int offset) -> int {
                        return bytes[i+offset] | (bytes[i+offset+1] << 8) | (bytes[i+offset+2] << 16) | (bytes[i+offset+3] << 24);
                    };
                    auto readUint16 = [&](int offset) -> unsigned short {
                        return bytes[i+offset] | (bytes[i+offset+1] << 8);
                    };
                    auto readInt16 = [&](int offset) -> short {
                        return static_cast<short>(bytes[i+offset] | (bytes[i+offset+1] << 8));
                    };
                    auto readFloat32 = [&](int offset) -> float {
                        float f;
                        int val = readInt32(offset);
                        memcpy(&f, &val, sizeof(float));
                        return f;
                    };
                    
                    unsigned int pcNum = readInt32(3);
                    unsigned char sysStatus = bytes[i+7];
                    unsigned short totalAlignTime = readUint16(11);
                    unsigned short remainAlignTime = readUint16(13);
                    
                    double gyroX = readInt32(15) * 0.0000005;
                    double gyroY = readInt32(19) * 0.0000005;
                    double gyroZ = readInt32(23) * 0.0000005;
                    double accelX = readInt32(27) * 0.000001;
                    double accelY = readInt32(31) * 0.000001;
                    double accelZ = readInt32(35) * 0.000001;
                    
                    double longitude = readInt32(39) * 0.0000001;
                    double latitude = readInt32(43) * 0.0000001;
                    double altitude = readInt16(47) * 0.5;
                    
                    double velN = readInt16(49) * 0.04;
                    double velU = readInt16(51) * 0.04;
                    double velE = readInt16(53) * 0.04;
                    
                    double roll = readInt16(55) * 0.006;
                    double yaw = readUint16(57) * 0.006;
                    double pitch = readInt16(59) * 0.006;
                    
                    char gyroTemp = static_cast<char>(bytes[i+61]);
                    char accelTemp = static_cast<char>(bytes[i+62]);
                    
                    double revYaw = readUint16(89) * 0.006;
                    unsigned char verticalFlag = bytes[i+93];
                    
                    csvStream << pcNum << ","
                              << static_cast<int>(sysStatus) << ","
                              << totalAlignTime << ","
                              << remainAlignTime << ","
                              << QString::number(gyroX, 'f', 6) << ","
                              << QString::number(gyroY, 'f', 6) << ","
                              << QString::number(gyroZ, 'f', 6) << ","
                              << QString::number(accelX, 'f', 6) << ","
                              << QString::number(accelY, 'f', 6) << ","
                              << QString::number(accelZ, 'f', 6) << ","
                              << QString::number(longitude, 'f', 8) << ","
                              << QString::number(latitude, 'f', 8) << ","
                              << QString::number(altitude, 'f', 2) << ","
                              << QString::number(velN, 'f', 4) << ","
                              << QString::number(velU, 'f', 4) << ","
                              << QString::number(velE, 'f', 4) << ","
                              << QString::number(roll, 'f', 4) << ","
                              << QString::number(yaw, 'f', 4) << ","
                              << QString::number(pitch, 'f', 4) << ","
                              << static_cast<int>(gyroTemp) << ","
                              << static_cast<int>(accelTemp) << ","
                              << QString::number(revYaw, 'f', 4) << ","
                              << static_cast<int>(verticalFlag) << "\n";
                              
                    parsedCount++;
                    i += 96;
                    continue;
                }
            }
        }
        i++;
    }
    
    csvFile.close();
    
    serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + 
                                     QString::fromUtf8("离线 BIN 解析完成！成功解析包数: %1，已生成 CSV 文件: %2")
                                     .arg(parsedCount)
                                     .arg(QFileInfo(csvName).fileName()));
    QMessageBox::information(this, "成功", QString::fromUtf8("解析离线 BIN 成功！\n共解析有效包: %1\n输出路径: %2").arg(parsedCount).arg(csvName));
}

void MainWindow::onRawCmdTypeChanged(int index)
{
    if (rawCmbCmdType->itemText(index) == QString::fromUtf8("自定义十六进制指令")) {
        rawEditCustomHexCmd->show();
    } else {
        rawEditCustomHexCmd->clear();
        rawEditCustomHexCmd->hide();
    }
}

void MainWindow::onSendRawCommand()
{
    if (!canthread || canthread->m_dev == INVALID_DEVICE_HANDLE) {
        QMessageBox::warning(this, "警告", "CAN通道未就绪或未打开！");
        return;
    }

    int cmdType = rawCmbCmdType->currentIndex();
    QByteArray cmdBytes;

    if (cmdType == 11) { // 自定义十六进制指令
        QString hexText = rawEditCustomHexCmd->text().trimmed();
        hexText.replace(" ", "");
        hexText.replace(",", "");
        hexText.replace("0x", "");
        hexText.replace("0X", "");

        // Validate hex format
        QRegularExpression hexMatcher("^[0-9a-fA-F]*$");
        if (!hexMatcher.match(hexText).hasMatch()) {
            QMessageBox::warning(this, "错误", "请输入有效的十六进制字符串！");
            return;
        }

        cmdBytes = QByteArray::fromHex(hexText.toLatin1());
        if (cmdBytes.isEmpty()) {
            QMessageBox::warning(this, "错误", "十六进制串不能为空！");
            return;
        }

        // Pad or truncate to 40 bytes
        if (cmdBytes.size() > 40) {
            cmdBytes.truncate(40);
        } else if (cmdBytes.size() < 40) {
            cmdBytes.append(40 - cmdBytes.size(), 0);
        }

        // Recalculate checksum at index 39: sum of index 2 to 38
        int sum = 0;
        unsigned char *ptr = reinterpret_cast<unsigned char*>(cmdBytes.data());
        for (int i = 2; i < 39; ++i) {
            sum += ptr[i];
        }
        cmdBytes[39] = static_cast<char>(sum & 0xFF);

    } else { // 0 to 10: Standard commands
        double lon = rawEditLon->text().toDouble();
        double lat = rawEditLat->text().toDouble();
        double alt = rawEditAlt->text().toDouble();
        double time = rawEditAlignTime->text().toDouble();
        double bindHeading = rawEditBindHeading->text().toDouble();
        
        double frontLever = rawEditFrontLever->text().toDouble();
        double rightLever = rawEditRightLever->text().toDouble();
        double upLever = rawEditUpLever->text().toDouble();
        
        double rollErr = rawEditRollError->text().toDouble();
        double pitchErr = rawEditPitchError->text().toDouble();
        double yawErr = rawEditYawError->text().toDouble();
        
        int periodIdx = rawCmbOutPeriod->currentIndex();
        
        int baudrateVal = 2; // 默认 115200 (2)
        QVariant baudData = rawCmbOutBaud->currentData();
        if (baudData.isValid()) {
            baudrateVal = baudData.toInt();
        }
        int periodVal = periodIdx;
        
        int userCmdVal = 0;
        int validModeVal = 0;
        double rollErrVal = 0;
        double yawErrVal = 0;
        double pitchErrVal = 0;
        double alignTimeVal = time;
        
        switch (cmdType) {
            case 0: // 自寻北
                userCmdVal = 0x0011;
                validModeVal = 0x0000;
                break;
            case 1: // 惯性标定
                userCmdVal = 0x0011;
                validModeVal = 0x0001; // bit0 LLA valid
                break;
            case 2: // 快速对准
                userCmdVal = 0x0011;
                validModeVal = 0x0007; // bit0 LLA, bit1 heading, bit2 time valid
                yawErrVal = bindHeading;
                break;
            case 3: // 写入LLA
                userCmdVal = 0x0066;
                validModeVal = 0x0001;
                break;
            case 4: // 写入对准时间
                userCmdVal = 0x0077;
                validModeVal = 0x0004;
                break;
            case 5: // 写入通信参数
                userCmdVal = 0x0099;
                validModeVal = 0x0008;
                break;
            case 6: // 写入安装误差角
                userCmdVal = 0x00AA;
                validModeVal = 0x0010;
                rollErrVal = rollErr;
                yawErrVal = yawErr;
                pitchErrVal = pitchErr;
                break;
            case 7: // 写入全部参数 (包括三个杆臂)
                userCmdVal = 0x00AA;
                validModeVal = 0x007F; // bit0..bit6 全有效
                rollErrVal = rollErr;
                yawErrVal = yawErr;
                pitchErrVal = pitchErr;
                break;
            case 8: // 写入姿态角到导航
                userCmdVal = 0x00BB;
                validModeVal = 0x0020; // bit5 valid
                rollErrVal = rollErr;
                yawErrVal = yawErr;
                pitchErrVal = pitchErr;
                break;
            case 9: // 写入陀螺零偏
                userCmdVal = 0x00CC;
                validModeVal = 0x0040; // bit6 valid
                rollErrVal = rollErr;
                yawErrVal = yawErr;
                pitchErrVal = pitchErr;
                break;
            case 10: // 写入杆臂
                userCmdVal = 0x00DD;
                validModeVal = 0x0080; // bit7 valid
                break;
            default:
                break;
        }

        cmdBytes = encodeCommandFrame(lon, lat, alt, frontLever, rightLever, upLever, validModeVal, baudrateVal, periodVal, rollErrVal, yawErrVal, pitchErrVal, userCmdVal, alignTimeVal);
    }

    QString hexStr = cmdBytes.toHex().toUpper();
    QString formattedHex;
    for (int i = 0; i < hexStr.length(); i += 2) {
        formattedHex.append(hexStr.mid(i, 2) + " ");
    }
    rawEditSendHex->setText(formattedHex.trimmed());

    // Segment into 5 frames (IDs: 0x11 ~ 0x15) and send
    bool allOk = true;
    for (int i = 0; i < 5; ++i) {
        UINT id = 0x11 + i;
        char frameData[8];
        memcpy(frameData, cmdBytes.constData() + (i * 8), 8);
        bool ok = canthread->sendData(id, 0, 0, 0, 0, frameData, 8);
        if (!ok) {
            allOk = false;
        }
        QThread::msleep(5);
    }

    if (allOk) {
        lblRawStatus->setText(QString::fromUtf8("透传状态: 指令下发成功"));
        lblRawStatus->setStyleSheet("color: #2e3440; font-size: 13px; font-weight: bold; background-color: #a3be8c; padding: 6px 15px; border-radius: 4px;");
    } else {
        QMessageBox::warning(this, "警告", "下发透传指令部分或全部失败，请检查设备连接！");
    }
}

void MainWindow::showFlashBlockData(int blockId, const QByteArray &blockData, const QByteArray &rawFrame)
{
    QString rawHex = rawFrame.toHex().toUpper();
    QString formattedHex;
    for (int i = 0; i < rawHex.length(); i += 2) {
        formattedHex.append(rawHex.mid(i, 2) + " ");
    }
    formattedHex = formattedHex.trimmed();

    QString blockName = (blockId == 9) ? "blk9" : ((blockId == 10) ? "blk10" : "blk11");
    QString blockDesc = (blockId == 9) ? QString::fromUtf8("用户使用参数配置块") : 
                        ((blockId == 10) ? QString::fromUtf8("IMU零偏配置块") : QString::fromUtf8("用户协议配置项"));

    serialConsoleLog->appendPlainText(QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + 
                                  QString::fromUtf8("收到 %1 (%2) 读取数据：").arg(blockName).arg(blockDesc));
    serialConsoleLog->appendPlainText(formattedHex);

    const unsigned char *bData = reinterpret_cast<const unsigned char*>(blockData.constData());
    auto readInt32 = [&](int offset) -> int {
        return bData[offset] | (bData[offset+1] << 8) | (bData[offset+2] << 16) | (bData[offset+3] << 24);
    };
    auto readUint16 = [&](int offset) -> unsigned short {
        return bData[offset] | (bData[offset+1] << 8);
    };
    auto readFloat32 = [&](int offset) -> float {
        float f;
        int val = readInt32(offset);
        memcpy(&f, &val, sizeof(float));
        return f;
    };

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("读取结果 - %1 (%2)").arg(blockName).arg(blockDesc));
    dlg.setMinimumSize(600, 500);
    dlg.resize(650, 550);
    dlg.setStyleSheet(
        "QDialog { background-color: #2e3440; color: #eceff4; }"
        "QLabel { color: #88c0d0; font-weight: bold; font-size: 14px; }"
    );

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(12);

    QLabel *titleLabel = new QLabel(QString::fromUtf8("Flash 块 %1 参数解析结果：").arg(blockName), &dlg);
    layout->addWidget(titleLabel);

    QTableWidget *table = new QTableWidget(&dlg);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels(QStringList() << QString::fromUtf8("参数名称") << QString::fromUtf8("字段名") << QString::fromUtf8("解析值") << QString::fromUtf8("单位"));
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setStyleSheet(
        "QTableWidget { background-color: #3b4252; color: #eceff4; gridline-color: #4c566a; font-size: 13px; }"
        "QHeaderView::section { background-color: #2e3440; color: #88c0d0; padding: 6px; border: 1px solid #4c566a; font-weight: bold; font-size: 13px; }"
        "QTableWidget::item { padding: 4px; }"
    );

    struct ParamRow {
        QString name;
        QString field;
        QString value;
        QString unit;
    };
    QList<ParamRow> rows;

    if (blockId == 9) {
        rows << ParamRow{QString::fromUtf8("滚动安装误差角"), "UsrTransRoll", QString::number(readFloat32(0), 'f', 6), "deg"};
        rows << ParamRow{QString::fromUtf8("航向安装误差角"), "UsrTransYaw", QString::number(readFloat32(4), 'f', 6), "deg"};
        rows << ParamRow{QString::fromUtf8("俯仰安装误差角"), "UsrTransPitch", QString::number(readFloat32(8), 'f', 6), "deg"};
        rows << ParamRow{QString::fromUtf8("前向目标杆臂"), "TargetLeverFront", QString::number(readFloat32(116), 'f', 6), "m"};
        rows << ParamRow{QString::fromUtf8("上向目标杆臂"), "TargetLeverUp", QString::number(readFloat32(120), 'f', 6), "m"};
        rows << ParamRow{QString::fromUtf8("左向目标杆臂"), "TargetLeverRight", QString::number(readFloat32(124), 'f', 6), "m"};
    }
    else if (blockId == 10) {
        rows << ParamRow{QString::fromUtf8("X轴陀螺漂移 (前)"), "GyroDriftX", QString::number(readFloat32(0), 'f', 6), "o/h"};
        rows << ParamRow{QString::fromUtf8("Y轴陀螺漂移 (上)"), "GyroDriftY", QString::number(readFloat32(4), 'f', 6), "o/h"};
        rows << ParamRow{QString::fromUtf8("Z轴陀螺漂移 (右)"), "GyroDriftZ", QString::number(readFloat32(8), 'f', 6), "o/h"};
        rows << ParamRow{QString::fromUtf8("X轴加表零偏 (前向)"), "AccelBiasX", QString::number(readFloat32(12), 'f', 6), "m/s2"};
        rows << ParamRow{QString::fromUtf8("Y轴加表零偏 (上向)"), "AccelBiasY", QString::number(readFloat32(16), 'f', 6), "m/s2"};
        rows << ParamRow{QString::fromUtf8("Z轴加表零偏 (右向)"), "AccelBiasZ", QString::number(readFloat32(20), 'f', 6), "m/s2"};
    }
    else if (blockId == 11) {
        rows << ParamRow{QString::fromUtf8("对码 (多路配置)"), "MatchCode", QString("0x%1").arg(readUint16(0), 4, 16, QChar('0')).toUpper(), "na"};
        rows << ParamRow{QString::fromUtf8("用户协议代码"), "ProtocolCode", QString("0x%1").arg(bData[2], 2, 16, QChar('0')).toUpper(), "na"};
        rows << ParamRow{QString::fromUtf8("波特率"), "BaudRate", QString::number(readInt32(4)), "bps"};
        rows << ParamRow{QString::fromUtf8("周期数"), "PeriodUs", QString::number(readInt32(8)), "us"};
        
        QString alignCond = QString::fromUtf8("未知(%1)").arg(bData[12]);
        if (bData[12] == 0) alignCond = QString::fromUtf8("不使能待机时间");
        else if (bData[12] == 2) alignCond = QString::fromUtf8("使能待机时间");
        rows << ParamRow{QString::fromUtf8("进入对准条件"), "AlignCondition", alignCond, "na"};
        
        rows << ParamRow{QString::fromUtf8("待机时间"), "StandbyTime", QString::number(bData[13]), "s"};
        rows << ParamRow{QString::fromUtf8("粗对准时间"), "CoarseAlignTime", QString::number(readUint16(14) * 0.1, 'f', 1), "s"};
        rows << ParamRow{QString::fromUtf8("精对准时间"), "FineAlignTime", QString::number(readUint16(18)), "s"};
        
        QString speedLimit = bData[31] == 0 ? QString::fromUtf8("不限速") : QString::number(bData[31]);
        rows << ParamRow{QString::fromUtf8("限速模式"), "SpeedLimit", speedLimit, "m/s"};
        
        rows << ParamRow{QString::fromUtf8("基准纬度"), "BaseLati", QString::number(readFloat32(32), 'f', 8), "deg"};
        rows << ParamRow{QString::fromUtf8("基准高度"), "BaseHgt", QString::number(readFloat32(36), 'f', 3), "m"};
        rows << ParamRow{QString::fromUtf8("基准经度"), "BaseLogi", QString::number(readFloat32(40), 'f', 8), "deg"};
        rows << ParamRow{QString::fromUtf8("基准航向"), "BaseHeading", QString::number(readFloat32(44), 'f', 6), "deg"};
    }

    table->setRowCount(rows.count());
    for (int i = 0; i < rows.count(); ++i) {
        table->setItem(i, 0, new QTableWidgetItem(rows[i].name));
        table->setItem(i, 1, new QTableWidgetItem(rows[i].field));
        table->setItem(i, 2, new QTableWidgetItem(rows[i].value));
        table->setItem(i, 3, new QTableWidgetItem(rows[i].unit));
        
        for (int c = 0; c < 4; ++c) {
            table->item(i, c)->setTextAlignment(Qt::AlignCenter);
            if (c == 2) {
                table->item(i, c)->setForeground(QBrush(QColor("#8fbcbb")));
                table->item(i, c)->setFont(QFont("Consolas", 10, QFont::Bold));
            }
        }
    }
    layout->addWidget(table, 1);

    QLabel *hexLabel = new QLabel(QString::fromUtf8("256字节原始指令返回帧 (RAW HEX)："), &dlg);
    layout->addWidget(hexLabel);

    QTextEdit *hexEdit = new QTextEdit(&dlg);
    hexEdit->setReadOnly(true);
    hexEdit->setPlainText(formattedHex);
    hexEdit->setStyleSheet(
        "background-color: #1e222a; color: #eceff4; font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; border: 1px solid #4c566a; border-radius: 4px; padding: 5px;"
    );
    hexEdit->setMaximumHeight(100);
    layout->addWidget(hexEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);
    QPushButton *btnOk = new QPushButton(QString::fromUtf8("确定"), &dlg);
    btnOk->setFixedWidth(100);
    btnOk->setStyleSheet(
        "QPushButton { background-color: #5e81ac; color: #eceff4; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #81a1c1; }"
    );
    connect(btnOk, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(btnOk);
    layout->addLayout(btnLayout);

    dlg.exec();
}



