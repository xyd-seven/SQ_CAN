#include "udswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QTime>
#include <QHeaderView>
#include <QScrollBar>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

UdsWidget::UdsWidget(QWidget *parent)
    : QWidget(parent),
      m_udsClient(nullptr),
      m_canThread(nullptr),
      m_testCount(0),
      m_passCount(0),
      m_failCount(0),
      m_flowRunning(false),
      m_flowCurrentIndex(0),
      m_flowCurrentLoop(0),
      m_flowTotalLoops(1)
{
    m_udsClient = new UdsClient(this);
    m_flowTimer = new QTimer(this);
    m_flowTimer->setSingleShot(true);
    connect(m_flowTimer, &QTimer::timeout, this, &UdsWidget::onFlowTimerTimeout);

    setupUi();
    initServiceTree();
    updateStats();

    // 绑定底层信号到 UI
    connect(m_udsClient, &UdsClient::udsResponseReceived, this, &UdsWidget::onUdsResponseReceived);
    connect(m_udsClient, &UdsClient::udsResponseTimeout, this, &UdsWidget::onUdsResponseTimeout);
    connect(m_udsClient, &UdsClient::upgradeStateChanged, this, &UdsWidget::onUpgradeStateChanged);
    connect(m_udsClient, &UdsClient::upgradeProgress, this, &UdsWidget::onUpgradeProgress);
    connect(m_udsClient, &UdsClient::upgradeCompleted, this, &UdsWidget::onUpgradeCompleted);
    connect(m_udsClient, &UdsClient::logMessage, this, &UdsWidget::onLogMessage);

    loadSettings();
}

UdsWidget::~UdsWidget()
{
    m_flowTimer->stop();
    saveSettings();
}

void UdsWidget::setCanThread(CANThread *thread)
{
    m_canThread = thread;
    m_udsClient->setCanThread(thread);
    onApplyConfig();
}

void UdsWidget::setupUi()
{
    setStyleSheet(
        "QWidget { color: #d8dee9; }"
        "QLabel { color: #d8dee9; }"
        "QCheckBox { color: #eceff4; }"
        "QRadioButton { color: #eceff4; }"
        "QGroupBox { color: #88c0d0; }"
        "QToolTip { color: #eceff4; background-color: #2e3440; border: 1px solid #4c566a; }"
    );

    // 主垂直布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // ------------------ 1. 顶部配置 GroupBox ------------------
    QGroupBox *configGroup = new QGroupBox(QString::fromUtf8("UDS 连接与参数配置"), this);
    configGroup->setStyleSheet("QGroupBox { border: 1px solid #3b4252; border-radius: 6px; margin-top: 10px; font-weight: bold; color: #88c0d0; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    QGridLayout *configLayout = new QGridLayout(configGroup);
    configLayout->setSpacing(10);
    configLayout->setContentsMargins(10, 15, 10, 10);

    // 通道选择
    configLayout->addWidget(new QLabel(QString::fromUtf8("通道:"), this), 0, 0);
    m_channelCombo = new QComboBox(this);
    m_channelCombo->addItems(QStringList() << QString::fromUtf8("通道 1") << QString::fromUtf8("通道 2"));
    m_channelCombo->setStyleSheet(getComboBoxStyleSheet());
    configLayout->addWidget(m_channelCombo, 0, 1);

    // 协议类型
    configLayout->addWidget(new QLabel(QString::fromUtf8("协议:"), this), 0, 2);
    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->addItems(QStringList() << "CAN" << "CANFD");
    m_protocolCombo->setStyleSheet(getComboBoxStyleSheet());
    configLayout->addWidget(m_protocolCombo, 0, 3);

    // 帧类型 (标准帧 / 扩展帧)
    configLayout->addWidget(new QLabel(QString::fromUtf8("帧类型:"), this), 0, 4);
    m_frameTypeCombo = new QComboBox(this);
    m_frameTypeCombo->addItems(QStringList() << QString::fromUtf8("标准帧 (11-bit)") << QString::fromUtf8("扩展帧 (29-bit)"));
    m_frameTypeCombo->setStyleSheet(getComboBoxStyleSheet());
    configLayout->addWidget(m_frameTypeCombo, 0, 5);

    QRegularExpression hexIdRegex("^[0-9a-fA-F]{1,8}$");
    QRegularExpressionValidator *hexIdValidator = new QRegularExpressionValidator(hexIdRegex, this);

    // 请求 ID (Hex)
    configLayout->addWidget(new QLabel(QString::fromUtf8("请求 ID:"), this), 0, 6);
    m_reqIdEdit = new QLineEdit("7E0", this);
    m_reqIdEdit->setPlaceholderText("Hex");
    m_reqIdEdit->setMaxLength(8);
    m_reqIdEdit->setValidator(hexIdValidator);
    m_reqIdEdit->setStyleSheet(getLineEditStyleSheet());
    configLayout->addWidget(m_reqIdEdit, 0, 7);

    // 响应 ID (Hex)
    configLayout->addWidget(new QLabel(QString::fromUtf8("响应 ID:"), this), 0, 8);
    m_resIdEdit = new QLineEdit("7E8", this);
    m_resIdEdit->setPlaceholderText("Hex");
    m_resIdEdit->setMaxLength(8);
    m_resIdEdit->setValidator(hexIdValidator);
    m_resIdEdit->setStyleSheet(getLineEditStyleSheet());
    configLayout->addWidget(m_resIdEdit, 0, 9);

    // 功能寻址 ID (Hex)
    configLayout->addWidget(new QLabel(QString::fromUtf8("功能 ID:"), this), 0, 10);
    m_funcIdEdit = new QLineEdit("7DF", this);
    m_funcIdEdit->setMaxLength(8);
    m_funcIdEdit->setValidator(hexIdValidator);
    m_funcIdEdit->setStyleSheet(getLineEditStyleSheet());
    configLayout->addWidget(m_funcIdEdit, 0, 11);

    // Tester Present 保持心跳配置
    m_testerPresentCheck = new QCheckBox(QString::fromUtf8("启用 3E 心跳"), this);
    m_testerPresentCheck->setStyleSheet("QCheckBox { color: #eceff4; } QCheckBox::indicator { width: 14px; height: 14px; }");
    configLayout->addWidget(m_testerPresentCheck, 1, 0, 1, 2);

    m_testerPresentIntervalSpin = new QSpinBox(this);
    m_testerPresentIntervalSpin->setRange(100, 10000);
    m_testerPresentIntervalSpin->setValue(2000);
    m_testerPresentIntervalSpin->setSuffix(" ms");
    m_testerPresentIntervalSpin->setStyleSheet(getLineEditStyleSheet());
    configLayout->addWidget(m_testerPresentIntervalSpin, 1, 2, 1, 2);

    // 参数 DID 配置
    configLayout->addWidget(new QLabel(QString::fromUtf8("参数 DID:"), this), 1, 4);
    m_paramDidEdit = new QLineEdit("F1A0", this);
    m_paramDidEdit->setStyleSheet(getLineEditStyleSheet());
    m_paramDidEdit->setMaxLength(4);
    configLayout->addWidget(m_paramDidEdit, 1, 5, 1, 2);
    mainLayout->addWidget(configGroup, 0);

    // ------------------ 2. 中部左右分栏布局 ------------------
    QSplitter *centerSplitter = new QSplitter(Qt::Horizontal, this);
    centerSplitter->setChildrenCollapsible(false);

    // 2.1 左侧诊断服务树 GroupBox
    QGroupBox *treeGroup = new QGroupBox(QString::fromUtf8("标准 UDS 诊断服务树"), this);
    treeGroup->setStyleSheet("QGroupBox { border: 1px solid #3b4252; border-radius: 6px; margin-top: 10px; font-weight: bold; color: #88c0d0; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    QVBoxLayout *treeLayout = new QVBoxLayout(treeGroup);
    treeLayout->setContentsMargins(5, 15, 5, 5);

    m_serviceSearchEdit = new QLineEdit(this);
    m_serviceSearchEdit->setPlaceholderText(QString::fromUtf8("搜索服务 ID / 名称 / PDU"));
    m_serviceSearchEdit->setClearButtonEnabled(true);
    m_serviceSearchEdit->setStyleSheet(getLineEditStyleSheet());
    treeLayout->addWidget(m_serviceSearchEdit);
    
    m_serviceTree = new QTreeWidget(this);
    m_serviceTree->setHeaderLabel(QString::fromUtf8("UDS 诊断目录 (双击自动填入)"));
    m_serviceTree->setStyleSheet(getTreeStyleSheet());
    treeLayout->addWidget(m_serviceTree);
    centerSplitter->addWidget(treeGroup);

    // 2.2 右侧工作 Tab 选项卡
    m_workTabWidget = new QTabWidget(this);
    m_workTabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #3b4252; background: #2e3440; }"
        "QTabBar::tab { background: #2e3440; color: #d8dee9; padding: 8px 15px; border: 1px solid #3b4252; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; min-width: 160px; }"
        "QTabBar::tab:selected { background: #3b4252; color: #88c0d0; font-weight: bold; border-top: 3px solid #88c0d0; }"
        "QTabBar::tab:hover { background: #434c5e; color: #eceff4; }"
    );

    // 2.2.1 Tab 1：诊断调试与流程测试
    m_diagTab = new QWidget(this);
    QVBoxLayout *diagTabLayout = new QVBoxLayout(m_diagTab);
    diagTabLayout->setSpacing(8);
    diagTabLayout->setContentsMargins(8, 8, 8, 8);

    // 2.2.1.1 诊断单次发送面板
    QGroupBox *manualGroup = new QGroupBox(QString::fromUtf8("单次服务调试"), this);
    manualGroup->setStyleSheet("QGroupBox { border: 1px solid #3b4252; border-radius: 4px; font-weight: bold; color: #88c0d0; }");
    QGridLayout *manualLayout = new QGridLayout(manualGroup);
    manualLayout->setSpacing(6);
    manualLayout->setContentsMargins(8, 12, 8, 8);

    manualLayout->addWidget(new QLabel(QString::fromUtf8("请求 PDU:"), this), 0, 0);
    m_pduReqEdit = new QLineEdit(this);
    m_pduReqEdit->setPlaceholderText(QString::fromUtf8("例如：22 F1 90"));
    m_pduReqEdit->setStyleSheet(getLineEditStyleSheet());
    manualLayout->addWidget(m_pduReqEdit, 0, 1);

    m_sendBtn = new QPushButton(QString::fromUtf8("立即发送"), this);
    m_sendBtn->setStyleSheet(getButtonStyleSheet());
    manualLayout->addWidget(m_sendBtn, 0, 2);

    manualLayout->addWidget(new QLabel(QString::fromUtf8("响应 PDU:"), this), 1, 0);
    m_pduResEdit = new QLineEdit(this);
    m_pduResEdit->setReadOnly(true);
    m_pduResEdit->setStyleSheet(getLineEditStyleSheet() + "QLineEdit { background-color: #1e222b; }");
    manualLayout->addWidget(m_pduResEdit, 1, 1);

    m_addToListBtn = new QPushButton(QString::fromUtf8("添加到列表"), this);
    m_addToListBtn->setStyleSheet(getButtonStyleSheet());
    manualLayout->addWidget(m_addToListBtn, 1, 2);

    m_pduStatusLabel = new QLabel(QString::fromUtf8("就绪"), this);
    m_pduStatusLabel->setStyleSheet("QLabel { color: #d8dee9; padding-left: 4px; }");
    manualLayout->addWidget(m_pduStatusLabel, 2, 0, 1, 3);

    diagTabLayout->addWidget(manualGroup, 0);

    // 2.2.1.2 诊断流程测试面板
    QGroupBox *flowGroup = new QGroupBox(QString::fromUtf8("自动化流程测试列表"), this);
    flowGroup->setStyleSheet("QGroupBox { border: 1px solid #3b4252; border-radius: 4px; font-weight: bold; color: #88c0d0; }");
    QVBoxLayout *flowLayout = new QVBoxLayout(flowGroup);
    flowLayout->setSpacing(6);
    flowLayout->setContentsMargins(8, 12, 8, 8);

    // 控制动作行
    QHBoxLayout *flowActionsLayout = new QHBoxLayout();
    flowActionsLayout->setSpacing(6);
    QLabel *flowEditLabel = new QLabel(QString::fromUtf8("编辑"), this);
    flowEditLabel->setStyleSheet("QLabel { color: #88c0d0; font-weight: bold; padding-right: 4px; }");
    flowActionsLayout->addWidget(flowEditLabel);
    
    m_addDelayBtn = new QPushButton(QString::fromUtf8("添加延时"), this);
    m_addDelayBtn->setStyleSheet(getButtonStyleSheet());
    flowActionsLayout->addWidget(m_addDelayBtn);

    m_deleteBtn = new QPushButton(QString::fromUtf8("删除"), this);
    m_deleteBtn->setStyleSheet(getButtonStyleSheet());
    flowActionsLayout->addWidget(m_deleteBtn);

    m_clearListBtn = new QPushButton(QString::fromUtf8("清空列表"), this);
    m_clearListBtn->setStyleSheet(getButtonStyleSheet() + "QPushButton { background-color: #5a2f36; color: #eceff4; } QPushButton:hover { background-color: #bf616a; }");
    flowActionsLayout->addWidget(m_clearListBtn);

    m_moveUpBtn = new QPushButton(QString::fromUtf8("上移"), this);
    m_moveUpBtn->setStyleSheet(getButtonStyleSheet());
    flowActionsLayout->addWidget(m_moveUpBtn);

    m_moveDownBtn = new QPushButton(QString::fromUtf8("下移"), this);
    m_moveDownBtn->setStyleSheet(getButtonStyleSheet());
    flowActionsLayout->addWidget(m_moveDownBtn);
    flowActionsLayout->addSpacing(12);
    QLabel *flowFileLabel = new QLabel(QString::fromUtf8("文件"), this);
    flowFileLabel->setStyleSheet("QLabel { color: #88c0d0; font-weight: bold; padding-right: 4px; }");
    flowActionsLayout->addWidget(flowFileLabel);

    m_importFlowBtn = new QPushButton(QString::fromUtf8("导入流程"), this);
    m_importFlowBtn->setStyleSheet(getButtonStyleSheet());
    flowActionsLayout->addWidget(m_importFlowBtn);

    m_exportFlowBtn = new QPushButton(QString::fromUtf8("导出流程"), this);
    m_exportFlowBtn->setStyleSheet(getButtonStyleSheet());
    flowActionsLayout->addWidget(m_exportFlowBtn);

    flowActionsLayout->addStretch(1);

    flowLayout->addLayout(flowActionsLayout);

    // 流程展示表格
    m_flowTable = new QTableWidget(this);
    m_flowTable->setColumnCount(9);
    m_flowTable->setHorizontalHeaderLabels(QStringList()
                                           << QString::fromUtf8("选择")
                                           << QString::fromUtf8("名称")
                                           << QString::fromUtf8("请求PDU")
                                           << QString::fromUtf8("响应PDU")
                                           << QString::fromUtf8("状态")
                                           << QString::fromUtf8("耗时")
                                           << QString::fromUtf8("期望响应")
                                           << QString::fromUtf8("匹配")
                                           << QString::fromUtf8("失败策略"));
    m_flowTable->setStyleSheet(getTableStyleSheet());
    m_flowTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_flowTable->horizontalHeader()->setStretchLastSection(true);
    m_flowTable->setColumnWidth(0, 48);
    m_flowTable->setColumnWidth(1, 150);
    m_flowTable->setColumnWidth(2, 180);
    m_flowTable->setColumnWidth(3, 190);
    m_flowTable->setColumnWidth(4, 90);
    m_flowTable->setColumnWidth(5, 78);
    m_flowTable->setColumnWidth(6, 170);
    m_flowTable->setColumnWidth(7, 82);
    m_flowTable->setColumnWidth(8, 96);
    m_flowTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    flowLayout->addWidget(m_flowTable);

    // 底部列表执行触发行
    QHBoxLayout *runLayout = new QHBoxLayout();
    runLayout->addWidget(new QLabel(QString::fromUtf8("循环次数:"), this));
    m_loopSpin = new QSpinBox(this);
    m_loopSpin->setRange(1, 9999);
    m_loopSpin->setValue(1);
    m_loopSpin->setStyleSheet(getLineEditStyleSheet());
    runLayout->addWidget(m_loopSpin);

    runLayout->addWidget(new QLabel(QString::fromUtf8("请求间隔:"), this));
    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(0, 10000);
    m_intervalSpin->setValue(10);
    m_intervalSpin->setSuffix(" ms");
    m_intervalSpin->setStyleSheet(getLineEditStyleSheet());
    runLayout->addWidget(m_intervalSpin);

    runLayout->addStretch(1);

    m_runFlowBtn = new QPushButton(QString::fromUtf8(" 列表发送 "), this);
    m_runFlowBtn->setStyleSheet(getButtonStyleSheet() + "QPushButton { font-weight: bold; background-color: #4c566a; color: #eceff4; min-width: 100px; } QPushButton:hover { background-color: #88c0d0; color: #2e3440; }");
    runLayout->addWidget(m_runFlowBtn);

    flowLayout->addLayout(runLayout);
    diagTabLayout->addWidget(flowGroup, 1);

    m_workTabWidget->addTab(m_diagTab, QString::fromUtf8("诊断调试与流程测试"));

    // 2.2.2 Tab 2：DTC 诊断
    m_dtcTab = new QWidget(this);
    QVBoxLayout *dtcLayout = new QVBoxLayout(m_dtcTab);
    dtcLayout->setSpacing(8);
    dtcLayout->setContentsMargins(8, 8, 8, 8);

    QGroupBox *dtcControlGroup = new QGroupBox(QString::fromUtf8("DTC 快捷诊断"), this);
    dtcControlGroup->setStyleSheet("QGroupBox { border: 1px solid #3b4252; border-radius: 4px; font-weight: bold; color: #88c0d0; }");
    QHBoxLayout *dtcControlLayout = new QHBoxLayout(dtcControlGroup);
    dtcControlLayout->setContentsMargins(8, 12, 8, 8);

    dtcControlLayout->addWidget(new QLabel(QString::fromUtf8("状态掩码:"), this));
    m_dtcStatusMaskEdit = new QLineEdit("FF", this);
    m_dtcStatusMaskEdit->setMaxLength(2);
    m_dtcStatusMaskEdit->setStyleSheet(getLineEditStyleSheet());
    dtcControlLayout->addWidget(m_dtcStatusMaskEdit);

    m_readDtcBtn = new QPushButton(QString::fromUtf8("读取 DTC"), this);
    m_readDtcBtn->setStyleSheet(getButtonStyleSheet());
    dtcControlLayout->addWidget(m_readDtcBtn);

    m_clearDtcBtn = new QPushButton(QString::fromUtf8("清除全部 DTC"), this);
    m_clearDtcBtn->setStyleSheet(getButtonStyleSheet() + "QPushButton { background-color: #5a2f36; color: #eceff4; } QPushButton:hover { background-color: #bf616a; }");
    dtcControlLayout->addWidget(m_clearDtcBtn);
    dtcControlLayout->addStretch(1);
    dtcLayout->addWidget(dtcControlGroup, 0);

    m_dtcTable = new QTableWidget(this);
    m_dtcTable->setColumnCount(3);
    m_dtcTable->setHorizontalHeaderLabels(QStringList() << "DTC" << QString::fromUtf8("状态") << QString::fromUtf8("原始记录"));
    m_dtcTable->setStyleSheet(getTableStyleSheet());
    m_dtcTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_dtcTable->horizontalHeader()->setStretchLastSection(true);
    m_dtcTable->setColumnWidth(0, 120);
    m_dtcTable->setColumnWidth(1, 100);
    m_dtcTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    dtcLayout->addWidget(m_dtcTable, 1);

    m_workTabWidget->addTab(m_dtcTab, QString::fromUtf8("DTC 诊断"));

    // 2.2.3 Tab 3：固件升级
    m_upgradeTab = new QWidget(this);
    QVBoxLayout *upgLayout = new QVBoxLayout(m_upgradeTab);
    upgLayout->setSpacing(10);
    upgLayout->setContentsMargins(15, 15, 15, 15);

    QGroupBox *fileGroup = new QGroupBox(QString::fromUtf8("固件升级参数配置"), this);
    fileGroup->setStyleSheet("QGroupBox { border: 1px solid #3b4252; border-radius: 4px; font-weight: bold; color: #88c0d0; }");
    QGridLayout *fileLayout = new QGridLayout(fileGroup);
    fileLayout->setSpacing(10);
    fileLayout->setContentsMargins(10, 15, 10, 10);

    fileLayout->addWidget(new QLabel(QString::fromUtf8("固件文件:"), this), 0, 0);
    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setReadOnly(true);
    m_filePathEdit->setStyleSheet(getLineEditStyleSheet());
    fileLayout->addWidget(m_filePathEdit, 0, 1);

    m_browseFileBtn = new QPushButton(QString::fromUtf8("选择固件..."), this);
    m_browseFileBtn->setStyleSheet(getButtonStyleSheet());
    fileLayout->addWidget(m_browseFileBtn, 0, 2);

    fileLayout->addWidget(new QLabel(QString::fromUtf8("起始物理地址 (Hex):"), this), 1, 0);
    m_flashAddrEdit = new QLineEdit("08008000", this);
    m_flashAddrEdit->setStyleSheet(getLineEditStyleSheet());
    fileLayout->addWidget(m_flashAddrEdit, 1, 1, 1, 2);

    fileLayout->addWidget(new QLabel(QString::fromUtf8("Reset 类型:"), this), 2, 0);
    m_resetTypeCombo = new QComboBox(this);
    m_resetTypeCombo->addItem(QString::fromUtf8("硬复位 (11 01)"), 0x01);
    m_resetTypeCombo->addItem(QString::fromUtf8("软复位 (11 03)"), 0x03);
    m_resetTypeCombo->setStyleSheet(getComboBoxStyleSheet());
    fileLayout->addWidget(m_resetTypeCombo, 2, 1, 1, 2);

    upgLayout->addWidget(fileGroup, 0);

    QGroupBox *upgradeConfigGroup = new QGroupBox(QString::fromUtf8("刷写参数配置"), this);
    upgradeConfigGroup->setStyleSheet("QGroupBox { border: 1px solid #3b4252; border-radius: 4px; font-weight: bold; color: #88c0d0; }");
    QVBoxLayout *upgradeConfigLayout = new QVBoxLayout(upgradeConfigGroup);
    upgradeConfigLayout->setSpacing(8);
    upgradeConfigLayout->setContentsMargins(10, 15, 10, 10);

    QGroupBox *securityGroup = new QGroupBox(QString::fromUtf8("安全访问"), this);
    securityGroup->setStyleSheet("QGroupBox { border: 1px solid #4c566a; border-radius: 4px; margin-top: 8px; color: #d8dee9; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QGridLayout *securityLayout = new QGridLayout(securityGroup);
    securityLayout->setSpacing(8);
    securityLayout->setContentsMargins(8, 14, 8, 8);
    securityLayout->addWidget(new QLabel(QString::fromUtf8("Seed 子功能:"), this), 0, 0);
    m_seedSubFuncEdit = new QLineEdit("01", this);
    m_seedSubFuncEdit->setMaxLength(2);
    m_seedSubFuncEdit->setStyleSheet(getLineEditStyleSheet());
    securityLayout->addWidget(m_seedSubFuncEdit, 0, 1);

    securityLayout->addWidget(new QLabel(QString::fromUtf8("Key 子功能:"), this), 0, 2);
    m_keySubFuncEdit = new QLineEdit("02", this);
    m_keySubFuncEdit->setMaxLength(2);
    m_keySubFuncEdit->setStyleSheet(getLineEditStyleSheet());
    securityLayout->addWidget(m_keySubFuncEdit, 0, 3);
    upgradeConfigLayout->addWidget(securityGroup);

    QGroupBox *downloadGroup = new QGroupBox(QString::fromUtf8("下载传输"), this);
    downloadGroup->setStyleSheet("QGroupBox { border: 1px solid #4c566a; border-radius: 4px; margin-top: 8px; color: #d8dee9; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QGridLayout *downloadLayout = new QGridLayout(downloadGroup);
    downloadLayout->setSpacing(8);
    downloadLayout->setContentsMargins(8, 14, 8, 8);
    downloadLayout->addWidget(new QLabel(QString::fromUtf8("DFI:"), this), 0, 0);
    m_dataFormatEdit = new QLineEdit("00", this);
    m_dataFormatEdit->setMaxLength(2);
    m_dataFormatEdit->setStyleSheet(getLineEditStyleSheet());
    downloadLayout->addWidget(m_dataFormatEdit, 0, 1);

    downloadLayout->addWidget(new QLabel(QString::fromUtf8("ALFI:"), this), 0, 2);
    m_addressLengthFormatEdit = new QLineEdit("44", this);
    m_addressLengthFormatEdit->setMaxLength(2);
    m_addressLengthFormatEdit->setStyleSheet(getLineEditStyleSheet());
    downloadLayout->addWidget(m_addressLengthFormatEdit, 0, 3);

    downloadLayout->addWidget(new QLabel(QString::fromUtf8("默认块大小:"), this), 1, 0);
    m_defaultBlockSizeSpin = new QSpinBox(this);
    m_defaultBlockSizeSpin->setRange(8, 4095);
    m_defaultBlockSizeSpin->setValue(256);
    m_defaultBlockSizeSpin->setStyleSheet(getLineEditStyleSheet());
    downloadLayout->addWidget(m_defaultBlockSizeSpin, 1, 1);

    m_useEcuBlockSizeCheck = new QCheckBox(QString::fromUtf8("使用 ECU 块大小"), this);
    m_useEcuBlockSizeCheck->setChecked(true);
    m_useEcuBlockSizeCheck->setStyleSheet("QCheckBox { color: #eceff4; } QCheckBox::indicator { width: 14px; height: 14px; }");
    downloadLayout->addWidget(m_useEcuBlockSizeCheck, 1, 2, 1, 2);
    upgradeConfigLayout->addWidget(downloadGroup);

    QGroupBox *checksumGroup = new QGroupBox(QString::fromUtf8("校验"), this);
    checksumGroup->setStyleSheet("QGroupBox { border: 1px solid #4c566a; border-radius: 4px; margin-top: 8px; color: #d8dee9; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QGridLayout *checksumLayout = new QGridLayout(checksumGroup);
    checksumLayout->setSpacing(8);
    checksumLayout->setContentsMargins(8, 14, 8, 8);
    checksumLayout->addWidget(new QLabel(QString::fromUtf8("Routine ID:"), this), 0, 0);
    m_routineIdEdit = new QLineEdit("0202", this);
    m_routineIdEdit->setMaxLength(4);
    m_routineIdEdit->setStyleSheet(getLineEditStyleSheet());
    checksumLayout->addWidget(m_routineIdEdit, 0, 1);

    m_appendCrcCheck = new QCheckBox(QString::fromUtf8("追加 CRC32"), this);
    m_appendCrcCheck->setChecked(true);
    m_appendCrcCheck->setStyleSheet("QCheckBox { color: #eceff4; } QCheckBox::indicator { width: 14px; height: 14px; }");
    checksumLayout->addWidget(m_appendCrcCheck, 0, 2);

    m_crcEndianCombo = new QComboBox(this);
    m_crcEndianCombo->addItems(QStringList() << "Big Endian" << "Little Endian");
    m_crcEndianCombo->setStyleSheet(getComboBoxStyleSheet());
    checksumLayout->addWidget(m_crcEndianCombo, 0, 3);
    upgradeConfigLayout->addWidget(checksumGroup);

    upgLayout->addWidget(upgradeConfigGroup, 0);

    // 升级控制与进度组
    QGroupBox *progGroup = new QGroupBox(QString::fromUtf8("升级进度状态监控"), this);
    progGroup->setStyleSheet("QGroupBox { border: 1px solid #3b4252; border-radius: 4px; font-weight: bold; color: #88c0d0; }");
    QVBoxLayout *progLayout = new QVBoxLayout(progGroup);
    progLayout->setSpacing(12);
    progLayout->setContentsMargins(10, 15, 10, 10);

    m_upgradeStatusLabel = new QLabel(QString::fromUtf8("当前状态：未启动"), this);
    m_upgradeStatusLabel->setStyleSheet("QLabel { color: #d8dee9; font-size: 13px; font-weight: bold; }");
    progLayout->addWidget(m_upgradeStatusLabel);

    m_upgradeProgressBar = new QProgressBar(this);
    m_upgradeProgressBar->setRange(0, 100);
    m_upgradeProgressBar->setValue(0);
    m_upgradeProgressBar->setStyleSheet(
        "QProgressBar { border: 1px solid #3b4252; border-radius: 4px; text-align: center; color: #eceff4; background-color: #1e222b; height: 22px; }"
        "QProgressBar::chunk { background-color: #88c0d0; border-radius: 2px; }"
    );
    progLayout->addWidget(m_upgradeProgressBar);

    QHBoxLayout *upgBtnLayout = new QHBoxLayout();
    m_startUpgradeBtn = new QPushButton(QString::fromUtf8("开始升级"), this);
    m_startUpgradeBtn->setStyleSheet(getButtonStyleSheet() + "QPushButton { background-color: #a3be8c; color: #2e3440; font-weight: bold; } QPushButton:hover { background-color: #b48ead; color: #eceff4; }");
    upgBtnLayout->addWidget(m_startUpgradeBtn);

    m_abortUpgradeBtn = new QPushButton(QString::fromUtf8("中止升级"), this);
    m_abortUpgradeBtn->setEnabled(false);
    m_abortUpgradeBtn->setStyleSheet(getButtonStyleSheet() + "QPushButton { background-color: #bf616a; color: #eceff4; font-weight: bold; }");
    upgBtnLayout->addWidget(m_abortUpgradeBtn);

    progLayout->addLayout(upgBtnLayout);
    upgLayout->addWidget(progGroup, 0);

    upgLayout->addStretch(1);
    m_workTabWidget->addTab(m_upgradeTab, QString::fromUtf8("固件升级与刷写"));

    centerSplitter->addWidget(m_workTabWidget);
    centerSplitter->setStretchFactor(0, 1);
    centerSplitter->setStretchFactor(1, 3);

    mainLayout->addWidget(centerSplitter, 1);

    // ------------------ 4. 底部日志与统计 GroupBox ------------------
    QGroupBox *logGroup = new QGroupBox(QString::fromUtf8("诊断报文控制台日志与执行统计"), this);
    logGroup->setStyleSheet("QGroupBox { border: 1px solid #3b4252; border-radius: 6px; font-weight: bold; color: #88c0d0; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    logLayout->setSpacing(6);
    logLayout->setContentsMargins(8, 15, 8, 8);

    QHBoxLayout *logFilterLayout = new QHBoxLayout();
    logFilterLayout->setSpacing(8);
    m_logFilterCombo = new QComboBox(this);
    m_logFilterCombo->addItem(QString::fromUtf8("全部日志"), -1);
    m_logFilterCombo->addItem(QString::fromUtf8("系统"), 0);
    m_logFilterCombo->addItem("TX", 1);
    m_logFilterCombo->addItem("RX", 2);
    m_logFilterCombo->addItem(QString::fromUtf8("错误"), 3);
    m_logFilterCombo->setStyleSheet(getComboBoxStyleSheet());
    logFilterLayout->addWidget(m_logFilterCombo);

    m_logSearchEdit = new QLineEdit(this);
    m_logSearchEdit->setPlaceholderText(QString::fromUtf8("搜索日志内容"));
    m_logSearchEdit->setClearButtonEnabled(true);
    m_logSearchEdit->setStyleSheet(getLineEditStyleSheet());
    logFilterLayout->addWidget(m_logSearchEdit, 1);
    logLayout->addLayout(logFilterLayout);

    m_consoleLog = new QPlainTextEdit(this);
    m_consoleLog->setReadOnly(true);
    m_consoleLog->setStyleSheet("QPlainTextEdit { background-color: #1e222b; color: #eceff4; font-family: Consolas, 'Courier New', monospace; font-size: 12px; border: 1px solid #2e3440; border-radius: 4px; }");
    logLayout->addWidget(m_consoleLog, 1);

    // 统计行
    QHBoxLayout *statLayout = new QHBoxLayout();
    statLayout->setSpacing(10);
    
    m_testCountLabel = new QLabel(QString::fromUtf8("测试次数: 0"), this);
    m_testCountLabel->setStyleSheet("QLabel { color: #d8dee9; font-weight: bold; }");
    statLayout->addWidget(m_testCountLabel);

    m_passCountLabel = new QLabel(QString::fromUtf8("通过: 0"), this);
    m_passCountLabel->setStyleSheet("QLabel { color: #a3be8c; font-weight: bold; }");
    statLayout->addWidget(m_passCountLabel);

    m_failCountLabel = new QLabel(QString::fromUtf8("未通过: 0"), this);
    m_failCountLabel->setStyleSheet("QLabel { color: #bf616a; font-weight: bold; }");
    statLayout->addWidget(m_failCountLabel);

    statLayout->addStretch(1);

    m_clearLogBtn = new QPushButton(QString::fromUtf8("清空日志"), this);
    m_clearLogBtn->setStyleSheet(getButtonStyleSheet() + "QPushButton { background-color: #5a2f36; color: #eceff4; } QPushButton:hover { background-color: #bf616a; }");
    statLayout->addWidget(m_clearLogBtn);

    m_exportLogBtn = new QPushButton(QString::fromUtf8("导出日志"), this);
    m_exportLogBtn->setStyleSheet(getButtonStyleSheet());
    statLayout->addWidget(m_exportLogBtn);

    m_resetStatsBtn = new QPushButton(QString::fromUtf8("重置统计"), this);
    m_resetStatsBtn->setStyleSheet(getButtonStyleSheet());
    statLayout->addWidget(m_resetStatsBtn);

    logLayout->addLayout(statLayout, 0);

    mainLayout->addWidget(logGroup, 0);

    // ------------------ 信号连接绑定 ------------------
    connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UdsWidget::onApplyConfig);
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UdsWidget::onApplyConfig);
    connect(m_frameTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UdsWidget::onApplyConfig);
    connect(m_reqIdEdit, &QLineEdit::editingFinished, this, &UdsWidget::onApplyConfig);
    connect(m_resIdEdit, &QLineEdit::editingFinished, this, &UdsWidget::onApplyConfig);
    connect(m_funcIdEdit, &QLineEdit::editingFinished, this, &UdsWidget::onApplyConfig);
    connect(m_paramDidEdit, &QLineEdit::editingFinished, this, &UdsWidget::onApplyConfig);

    connect(m_testerPresentCheck, &QCheckBox::stateChanged, this, &UdsWidget::onTesterPresentStateChanged);
    connect(m_serviceSearchEdit, &QLineEdit::textChanged, this, &UdsWidget::filterServiceTree);
    connect(m_serviceTree, &QTreeWidget::itemDoubleClicked, this, &UdsWidget::onServiceTreeDoubleClicked);
    
    connect(m_sendBtn, &QPushButton::clicked, this, &UdsWidget::onSendImmediateClicked);
    connect(m_addToListBtn, &QPushButton::clicked, this, &UdsWidget::onAddToListClicked);

    connect(m_addDelayBtn, &QPushButton::clicked, this, &UdsWidget::onAddDelayClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this, &UdsWidget::onDeleteStepClicked);
    connect(m_clearListBtn, &QPushButton::clicked, this, &UdsWidget::onClearListClicked);
    connect(m_moveUpBtn, &QPushButton::clicked, this, &UdsWidget::onMoveUpClicked);
    connect(m_moveDownBtn, &QPushButton::clicked, this, &UdsWidget::onMoveDownClicked);
    connect(m_runFlowBtn, &QPushButton::clicked, this, &UdsWidget::onRunFlowClicked);
    connect(m_importFlowBtn, &QPushButton::clicked, this, &UdsWidget::onImportFlowClicked);
    connect(m_exportFlowBtn, &QPushButton::clicked, this, &UdsWidget::onExportFlowClicked);
    connect(m_readDtcBtn, &QPushButton::clicked, this, &UdsWidget::onReadDtcClicked);
    connect(m_clearDtcBtn, &QPushButton::clicked, this, &UdsWidget::onClearDtcClicked);

    connect(m_browseFileBtn, &QPushButton::clicked, this, &UdsWidget::onBrowseFileClicked);
    connect(m_startUpgradeBtn, &QPushButton::clicked, this, &UdsWidget::onStartUpgradeClicked);
    connect(m_abortUpgradeBtn, &QPushButton::clicked, this, &UdsWidget::onAbortUpgradeClicked);
    connect(m_logFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UdsWidget::refreshLogView);
    connect(m_logSearchEdit, &QLineEdit::textChanged, this, &UdsWidget::refreshLogView);

    connect(m_clearLogBtn, &QPushButton::clicked, this, [this](){
        if (m_logEntries.isEmpty()) {
            return;
        }
        if (QMessageBox::question(this, "确认", "确定要清空当前日志吗？") == QMessageBox::Yes) {
            m_logEntries.clear();
            m_consoleLog->clear();
        }
    });
    connect(m_exportLogBtn, &QPushButton::clicked, this, &UdsWidget::onExportLogClicked);
    connect(m_resetStatsBtn, &QPushButton::clicked, this, [this](){
        m_testCount = 0;
        m_passCount = 0;
        m_failCount = 0;
        updateStats();
    });
}

void UdsWidget::initServiceTree()
{
    m_serviceTree->clear();

    // 10 诊断会话
    QTreeWidgetItem *sessionItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("10 诊断会话控制"));
    new QTreeWidgetItem(sessionItem, QStringList() << QString::fromUtf8("默认会话 (10 01)") << "1001");
    new QTreeWidgetItem(sessionItem, QStringList() << QString::fromUtf8("编程会话 (10 02)") << "1002");
    new QTreeWidgetItem(sessionItem, QStringList() << QString::fromUtf8("扩展会话 (10 03)") << "1003");
    
    // 11 ECU重置
    QTreeWidgetItem *resetItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("11 ECU重置"));
    new QTreeWidgetItem(resetItem, QStringList() << QString::fromUtf8("硬复位 (11 01)") << "1101");
    new QTreeWidgetItem(resetItem, QStringList() << QString::fromUtf8("软复位 (11 03)") << "1103");

    // 14 清除 DTC
    QTreeWidgetItem *clearDtcItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("14 清除诊断故障码"));
    new QTreeWidgetItem(clearDtcItem, QStringList() << QString::fromUtf8("清除全部 DTC (14 FF FF FF)") << "14FFFFFF");

    // 19 读取 DTC
    QTreeWidgetItem *dtcItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("19 读取诊断故障码"));
    new QTreeWidgetItem(dtcItem, QStringList() << QString::fromUtf8("按状态掩码读取 DTC (19 02 FF)") << "1902FF");
    new QTreeWidgetItem(dtcItem, QStringList() << QString::fromUtf8("读取支持的 DTC (19 0A)") << "190A");

    // 22 读数据
    QTreeWidgetItem *readItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("22 按标识符读取数据"));
    new QTreeWidgetItem(readItem, QStringList() << QString::fromUtf8("读取软件版本 (22 F1 89)") << "22F189");
    new QTreeWidgetItem(readItem, QStringList() << QString::fromUtf8("读取硬件版本 (22 F1 91)") << "22F191");
    new QTreeWidgetItem(readItem, QStringList() << QString::fromUtf8("读取零件号 (22 F1 87)") << "22F187");

    // 27 安全访问
    QTreeWidgetItem *secItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("27 安全访问"));
    new QTreeWidgetItem(secItem, QStringList() << QString::fromUtf8("请求种子 (27 01)") << "2701");
    new QTreeWidgetItem(secItem, QStringList() << QString::fromUtf8("发送密钥 (27 02 示例)") << "270255AA1234");

    // 28 通信控制
    QTreeWidgetItem *commItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("28 通信控制"));
    new QTreeWidgetItem(commItem, QStringList() << QString::fromUtf8("关闭 Rx/Tx 应用报文 (28 01 01)") << "280101");

    // 2E 写数据
    QTreeWidgetItem *writeItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("2E 按标识符写数据"));
    new QTreeWidgetItem(writeItem, QStringList() << QString::fromUtf8("参数写入模版 (2E F1 90 41 42)") << "2EF1904142");

    // 2F IO 控制
    QTreeWidgetItem *ioItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("2F IO 控制"));
    new QTreeWidgetItem(ioItem, QStringList() << QString::fromUtf8("IO 控制模板 (2F F1 90 03)") << "2FF19003");

    // 31 例程控制
    QTreeWidgetItem *rtItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("31 例程控制"));
    new QTreeWidgetItem(rtItem, QStringList() << QString::fromUtf8("启动校验 (31 01 02 02)") << "31010202");

    // 34/36/37 下载传输
    QTreeWidgetItem *downloadItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("34/36/37 下载传输"));
    new QTreeWidgetItem(downloadItem, QStringList() << QString::fromUtf8("请求下载模板 (34 00 44 08 00 80 00 00 00 10 00)") << "3400440800800000001000");
    new QTreeWidgetItem(downloadItem, QStringList() << QString::fromUtf8("传输数据模板 (36 01)") << "3601");
    new QTreeWidgetItem(downloadItem, QStringList() << QString::fromUtf8("退出传输 (37)") << "37");

    // 3E 在线保持
    QTreeWidgetItem *tpItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("3E 在线保持"));
    new QTreeWidgetItem(tpItem, QStringList() << QString::fromUtf8("发送保持 (3E 80)") << "3E80");

    // 85 DTC 设置
    QTreeWidgetItem *dtcSettingItem = new QTreeWidgetItem(m_serviceTree, QStringList() << QString::fromUtf8("85 DTC 设置"));
    new QTreeWidgetItem(dtcSettingItem, QStringList() << QString::fromUtf8("开启 DTC 设置 (85 01)") << "8501");
    new QTreeWidgetItem(dtcSettingItem, QStringList() << QString::fromUtf8("关闭 DTC 设置 (85 02)") << "8502");

    m_serviceTree->expandAll();
}

void UdsWidget::updateStats()
{
    m_testCountLabel->setText(QString::fromUtf8("测试次数: %1").arg(m_testCount));
    m_passCountLabel->setText(QString::fromUtf8("通过: %1").arg(m_passCount));
    m_failCountLabel->setText(QString::fromUtf8("未通过: %1").arg(m_failCount));
}

QString UdsWidget::normalizeHexInput(const QString &input) const
{
    QString normalized;
    for (const QChar &ch : input) {
        if (!ch.isSpace()) {
            normalized.append(ch);
        }
    }
    return normalized.toUpper();
}

bool UdsWidget::isValidHexInput(const QString &input, QString *errorMessage) const
{
    QString normalized = normalizeHexInput(input);
    if (normalized.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("请求 PDU 不能为空");
        }
        return false;
    }

    if ((normalized.size() % 2) != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("请求 PDU 长度必须为偶数字符");
        }
        return false;
    }

    for (const QChar &ch : normalized) {
        bool isHexChar = (ch >= QChar('0') && ch <= QChar('9')) ||
                         (ch >= QChar('A') && ch <= QChar('F'));
        if (!isHexChar) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("请求 PDU 包含非十六进制字符: %1").arg(ch);
            }
            return false;
        }
    }

    return true;
}

QString UdsWidget::formatHexWithSpaces(const QString &hex) const
{
    QString normalized = normalizeHexInput(hex);
    QString formatted;
    for (int i = 0; i < normalized.size(); i += 2) {
        if (!formatted.isEmpty()) {
            formatted.append(' ');
        }
        formatted.append(normalized.mid(i, 2));
    }
    return formatted;
}

QString UdsWidget::flowCellText(int row, int column, const QString &defaultValue) const
{
    QTableWidgetItem *item = m_flowTable->item(row, column);
    if (!item) {
        return defaultValue;
    }
    QString text = item->text().trimmed();
    return text.isEmpty() ? defaultValue : text;
}

bool UdsWidget::matchExpectedResponse(const QString &actualResponse, const QString &expectedResponse, const QString &matchMode) const
{
    QString actual = normalizeHexInput(actualResponse);
    QString expected = normalizeHexInput(expectedResponse);
    QString mode = matchMode.trimmed().toLower();

    if (expected.isEmpty()) {
        return true;
    }
    if (expected == "TIMEOUT") {
        return actual == "TIMEOUT";
    }
    if (mode.contains("exact") || mode.contains(QString::fromUtf8("完全"))) {
        return actual == expected;
    }
    if (mode.contains("contains") || mode.contains("contain") || mode.contains(QString::fromUtf8("包含"))) {
        return actual.contains(expected);
    }
    return actual.startsWith(expected);
}

bool UdsWidget::shouldStopOnFlowFailure(int row) const
{
    QString failPolicy = flowCellText(row, 8, QString::fromUtf8("继续")).trimmed().toLower();
    return failPolicy.contains("stop") || failPolicy.contains(QString::fromUtf8("停止"));
}

void UdsWidget::finishFlowRun(const QString &message, int logType)
{
    m_flowRunning = false;
    m_flowTimer->stop();
    m_runFlowBtn->setText(QString::fromUtf8(" 列表发送 "));
    m_runFlowBtn->setStyleSheet(getButtonStyleSheet());
    m_addDelayBtn->setEnabled(true);
    m_deleteBtn->setEnabled(true);
    m_clearListBtn->setEnabled(true);
    m_moveUpBtn->setEnabled(true);
    m_moveDownBtn->setEnabled(true);
    m_importFlowBtn->setEnabled(true);
    onLogMessage(message, logType);
}

// 应用配置变更
void UdsWidget::filterServiceTree(const QString &keyword)
{
    const QString filter = keyword.trimmed();
    for (int i = 0; i < m_serviceTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parent = m_serviceTree->topLevelItem(i);
        bool parentMatches = parent->text(0).contains(filter, Qt::CaseInsensitive) ||
                             parent->text(1).contains(filter, Qt::CaseInsensitive);
        bool hasVisibleChild = false;

        for (int childIndex = 0; childIndex < parent->childCount(); ++childIndex) {
            QTreeWidgetItem *child = parent->child(childIndex);
            bool childMatches = filter.isEmpty() || parentMatches ||
                                child->text(0).contains(filter, Qt::CaseInsensitive) ||
                                child->text(1).contains(filter, Qt::CaseInsensitive);
            child->setHidden(!childMatches);
            hasVisibleChild = hasVisibleChild || childMatches;
        }

        parent->setHidden(!filter.isEmpty() && !parentMatches && !hasVisibleChild);
        parent->setExpanded(!filter.isEmpty() && (parentMatches || hasVisibleChild));
    }
}

void UdsWidget::setManualResponseStatus(const QString &text, const QColor &color)
{
    if (!m_pduStatusLabel) {
        return;
    }
    m_pduStatusLabel->setText(text);
    m_pduStatusLabel->setStyleSheet(QString("QLabel { color: %1; padding-left: 4px; }").arg(color.name()));
}

QString UdsWidget::logTypeLabel(int type) const
{
    switch (type) {
        case 1:
            return "TX";
        case 2:
            return "RX";
        case 3:
            return "ERR";
        default:
            return "SYS";
    }
}

QString UdsWidget::formatLogLine(const QString &time, const QString &message, int type) const
{
    QString color = "#d8dee9";
    if (type == 1) {
        color = "#a3be8c";
    } else if (type == 2) {
        color = "#88c0d0";
    } else if (type == 3) {
        color = "#bf616a";
    }

    return QString("<font color=\"%1\">[%2] <b>[%3]</b> %4</font>")
        .arg(color)
        .arg(time)
        .arg(logTypeLabel(type))
        .arg(message.toHtmlEscaped());
}

void UdsWidget::refreshLogView()
{
    int typeFilter = m_logFilterCombo ? m_logFilterCombo->currentData().toInt() : -1;
    QString keyword = m_logSearchEdit ? m_logSearchEdit->text().trimmed() : QString();

    m_consoleLog->clear();
    for (const LogEntry &entry : m_logEntries) {
        if (typeFilter >= 0 && entry.type != typeFilter) {
            continue;
        }
        if (!keyword.isEmpty() &&
            !entry.message.contains(keyword, Qt::CaseInsensitive) &&
            !entry.time.contains(keyword, Qt::CaseInsensitive) &&
            !logTypeLabel(entry.type).contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }
        m_consoleLog->appendHtml(formatLogLine(entry.time, entry.message, entry.type));
    }
    m_consoleLog->verticalScrollBar()->setValue(m_consoleLog->verticalScrollBar()->maximum());
}

void UdsWidget::onApplyConfig()
{
    bool ok;
    uint32_t reqId = m_reqIdEdit->text().toUInt(&ok, 16);
    if (!ok) {
        return;
    }
    uint32_t resId = m_resIdEdit->text().toUInt(&ok, 16);
    if (!ok) {
        return;
    }

    int channel = m_channelCombo->currentIndex();
    int protocol = m_protocolCombo->currentIndex(); // 0-CAN, 1-CANFD
    bool isExtended = (m_frameTypeCombo->currentIndex() == 1);

    // 智能容错与边界检查：
    // 1. 如果用户输入了大于 0x7FF 的 ID 但当前为标准帧模式，自动切换为扩展帧
    if (!isExtended && (reqId > 0x7FF || resId > 0x7FF)) {
        isExtended = true;
        m_frameTypeCombo->blockSignals(true);
        m_frameTypeCombo->setCurrentIndex(1);
        m_frameTypeCombo->blockSignals(false);
        onLogMessage(QString("ID超出标准帧范围(0~0x7FF)，已自动切换为扩展帧模式(29-bit)"), 1);
    }

    // 2. 检查扩展帧是否超出 29 位有效范围 (0~0x1FFFFFFF)
    if (isExtended) {
        if (reqId > 0x1FFFFFFF) {
            onLogMessage(QString("请求 ID 0x%1 超出 29 位扩展帧范围(0~0x1FFFFFFF)").arg(reqId, 0, 16).toUpper(), 3);
            return;
        }
        if (resId > 0x1FFFFFFF) {
            onLogMessage(QString("响应 ID 0x%1 超出 29 位扩展帧范围(0~0x1FFFFFFF)").arg(resId, 0, 16).toUpper(), 3);
            return;
        }
    }

    m_udsClient->setConfig(reqId, resId, isExtended, channel, protocol);
    saveSettings();
}

// 自动在线保持状态改变
void UdsWidget::onTesterPresentStateChanged(int state)
{
    m_udsClient->setTesterPresentEnabled(state == Qt::Checked, m_testerPresentIntervalSpin->value());
    saveSettings();
}

uint16_t UdsWidget::getParameterWriteDid() const
{
    bool ok;
    uint16_t did = m_paramDidEdit->text().toUInt(&ok, 16);
    if (!ok) {
        return 0xF1A0;
    }
    return did;
}

// 左侧树双击填充模板 PDU
void UdsWidget::onServiceTreeDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (item->childCount() > 0) return; // 忽略父节点双击
    
    QString rawPdu = item->text(1);
    if (rawPdu.isEmpty()) return;

    m_pduReqEdit->setText(formatHexWithSpaces(rawPdu));
    m_pduReqEdit->setFocus();
    setManualResponseStatus(QString::fromUtf8("已填入服务模板"), QColor("#88c0d0"));
}

// 单次诊断发送
void UdsWidget::onSendImmediateClicked()
{
    onApplyConfig();

    QString errorMessage;
    if (!isValidHexInput(m_pduReqEdit->text(), &errorMessage)) {
        setManualResponseStatus(errorMessage, QColor("#bf616a"));
        QMessageBox::warning(this, "提示", errorMessage);
        return;
    }

    QString reqStr = normalizeHexInput(m_pduReqEdit->text());
    m_pduReqEdit->setText(formatHexWithSpaces(reqStr));
    QByteArray pdu = QByteArray::fromHex(reqStr.toUtf8());
    if (pdu.isEmpty()) {
        setManualResponseStatus(QString::fromUtf8("请求 PDU 转换失败"), QColor("#bf616a"));
        QMessageBox::warning(this, "提示", "请求 PDU 转换失败，请输入正确的十六进制报文！");
        return;
    }

    m_pduResEdit->clear();
    setManualResponseStatus(QString::fromUtf8("等待响应..."), QColor("#ebcb8b"));
    uint8_t service = pdu.at(0);
    m_udsClient->sendUdsRequest(service, pdu.mid(1));
}

// 将 PDU 添加到自动化执行列表
void UdsWidget::onAddToListClicked()
{
    QString errorMessage;
    if (!isValidHexInput(m_pduReqEdit->text(), &errorMessage)) {
        QMessageBox::warning(this, "提示", errorMessage);
        return;
    }
    QString pdu = formatHexWithSpaces(m_pduReqEdit->text());
    m_pduReqEdit->setText(pdu);

    int row = m_flowTable->rowCount();
    m_flowTable->insertRow(row);

    // 1. 选择框
    QTableWidgetItem *chkItem = new QTableWidgetItem();
    chkItem->setCheckState(Qt::Checked);
    m_flowTable->setItem(row, 0, chkItem);

    // 2. 名称
    QString name = QString("UDS 诊断步骤 %1").arg(row + 1);
    m_flowTable->setItem(row, 1, new QTableWidgetItem(name));

    // 3. 请求 PDU
    m_flowTable->setItem(row, 2, new QTableWidgetItem(pdu));

    // 4. 响应 PDU
    m_flowTable->setItem(row, 3, new QTableWidgetItem(""));

    // 5. 状态
    QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromUtf8("等待"));
    statusItem->setForeground(QBrush(QColor("#d8dee9")));
    m_flowTable->setItem(row, 4, statusItem);

    // 6. 耗时
    m_flowTable->setItem(row, 5, new QTableWidgetItem(""));

    m_flowTable->setItem(row, 6, new QTableWidgetItem(""));
    m_flowTable->setItem(row, 7, new QTableWidgetItem(QString::fromUtf8("前缀")));
    m_flowTable->setItem(row, 8, new QTableWidgetItem(QString::fromUtf8("继续")));
}

// 添加延时步骤
void UdsWidget::onAddDelayClicked()
{
    int row = m_flowTable->rowCount();
    m_flowTable->insertRow(row);

    // 1. 选择框
    QTableWidgetItem *chkItem = new QTableWidgetItem();
    chkItem->setCheckState(Qt::Checked);
    m_flowTable->setItem(row, 0, chkItem);

    // 2. 名称
    m_flowTable->setItem(row, 1, new QTableWidgetItem(QString::fromUtf8("延时等待")));

    // 3. 延时数值 (PDU 框填入时间毫秒值)
    m_flowTable->setItem(row, 2, new QTableWidgetItem("1000"));

    m_flowTable->setItem(row, 3, new QTableWidgetItem(""));
    m_flowTable->setItem(row, 4, new QTableWidgetItem(QString::fromUtf8("等待")));
    m_flowTable->setItem(row, 5, new QTableWidgetItem(""));
    m_flowTable->setItem(row, 6, new QTableWidgetItem(""));
    m_flowTable->setItem(row, 7, new QTableWidgetItem(QString::fromUtf8("前缀")));
    m_flowTable->setItem(row, 8, new QTableWidgetItem(QString::fromUtf8("继续")));
}

// 删除选中行
void UdsWidget::onDeleteStepClicked()
{
    if (m_flowRunning) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("流程运行中不能删除步骤。"));
        return;
    }

    int currentRow = m_flowTable->currentRow();
    if (currentRow >= 0) {
        m_flowTable->removeRow(currentRow);
    }
}

// 清空列表
void UdsWidget::onClearListClicked()
{
    if (m_flowRunning) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("流程运行中不能清空列表。"));
        return;
    }
    if (m_flowTable->rowCount() == 0) {
        return;
    }
    if (QMessageBox::question(this, QString::fromUtf8("确认"), QString::fromUtf8("确定要清空当前自动化流程列表吗？")) != QMessageBox::Yes) {
        return;
    }
    m_flowTable->setRowCount(0);
}

// 上移
void UdsWidget::onMoveUpClicked()
{
    if (m_flowRunning) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("流程运行中不能调整步骤顺序。"));
        return;
    }

    int row = m_flowTable->currentRow();
    if (row > 0) {
        m_flowTable->insertRow(row - 1);
        for (int i = 0; i < m_flowTable->columnCount(); ++i) {
            m_flowTable->setItem(row - 1, i, m_flowTable->takeItem(row + 1, i));
        }
        m_flowTable->removeRow(row + 1);
        m_flowTable->setCurrentCell(row - 1, 0);
    }
}

// 下移
void UdsWidget::onMoveDownClicked()
{
    if (m_flowRunning) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("流程运行中不能调整步骤顺序。"));
        return;
    }

    int row = m_flowTable->currentRow();
    if (row >= 0 && row < m_flowTable->rowCount() - 1) {
        m_flowTable->insertRow(row + 2);
        for (int i = 0; i < m_flowTable->columnCount(); ++i) {
            m_flowTable->setItem(row + 2, i, m_flowTable->takeItem(row, i));
        }
        m_flowTable->removeRow(row);
        m_flowTable->setCurrentCell(row + 1, 0);
    }
}

void UdsWidget::onExportFlowClicked()
{
    if (m_flowTable->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "流程列表为空，无法导出。");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出流程"), "", "JSON Files (*.json);;All Files (*)");
    if (filePath.isEmpty()) {
        return;
    }

    QJsonArray steps;
    for (int row = 0; row < m_flowTable->rowCount(); ++row) {
        QTableWidgetItem *checkItem = m_flowTable->item(row, 0);
        QJsonObject step;
        step["enabled"] = !checkItem || checkItem->checkState() == Qt::Checked;
        step["name"] = flowCellText(row, 1);
        step["request"] = flowCellText(row, 2);
        step["response"] = flowCellText(row, 3);
        step["status"] = flowCellText(row, 4);
        step["elapsed"] = flowCellText(row, 5);
        step["expected"] = flowCellText(row, 6);
        step["matchMode"] = flowCellText(row, 7, QString::fromUtf8("前缀"));
        step["failPolicy"] = flowCellText(row, 8, QString::fromUtf8("继续"));
        steps.append(step);
    }

    QJsonObject root;
    root["version"] = 1;
    root["steps"] = steps;

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "错误", QString("无法写入流程文件: %1").arg(filePath));
        onLogMessage(QString("导出流程失败: 无法写入文件 %1").arg(filePath), 3);
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        QMessageBox::critical(this, "错误", QString("保存流程文件失败: %1").arg(filePath));
        onLogMessage(QString("导出流程失败: 保存文件失败 %1").arg(filePath), 3);
        return;
    }

    onLogMessage(QString("已导出自动化流程: %1").arg(filePath), 0);
}

void UdsWidget::onImportFlowClicked()
{
    if (m_flowRunning) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("流程运行中不能导入新流程。"));
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(this, QString::fromUtf8("导入流程"), "", "JSON Files (*.json);;All Files (*)");
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "错误", QString("无法打开流程文件: %1").arg(filePath));
        onLogMessage(QString("导入流程失败: 无法打开文件 %1").arg(filePath), 3);
        return;
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::critical(this, "错误", QString("流程 JSON 格式错误: %1").arg(parseError.errorString()));
        onLogMessage(QString("导入流程失败: JSON 格式错误 %1").arg(filePath), 3);
        return;
    }

    QJsonObject root = document.object();
    if (root.value("version").toInt() != 1 || !root.value("steps").isArray()) {
        QMessageBox::critical(this, "错误", "流程文件版本或 steps 字段无效。");
        onLogMessage(QString("导入流程失败: 版本或字段无效 %1").arg(filePath), 3);
        return;
    }

    struct FlowStep {
        bool enabled;
        QString name;
        QString request;
        QString expected;
        QString matchMode;
        QString failPolicy;
    };

    QVector<FlowStep> importedSteps;
    int invalidCount = 0;
    QJsonArray steps = root.value("steps").toArray();
    for (const QJsonValue &value : steps) {
        if (!value.isObject()) {
            invalidCount++;
            continue;
        }

        QJsonObject stepObject = value.toObject();
        QString name = stepObject.value("name").toString(QString::fromUtf8("导入步骤"));
        QString request = stepObject.value("request").toString();
        QString expected = stepObject.value("expected").toString();
        QString matchMode = stepObject.value("matchMode").toString(QString::fromUtf8("前缀"));
        QString failPolicy = stepObject.value("failPolicy").toString(QString::fromUtf8("继续"));

        bool isDelay = name.contains(QString::fromUtf8("延时")) || name.contains("Delay", Qt::CaseInsensitive);
        QString errorMessage;
        if (!isDelay && !isValidHexInput(request, &errorMessage)) {
            invalidCount++;
            continue;
        }
        if (!expected.isEmpty() &&
            expected.trimmed().compare("TIMEOUT", Qt::CaseInsensitive) != 0 &&
            !isValidHexInput(expected, &errorMessage)) {
            invalidCount++;
            continue;
        }

        FlowStep step;
        step.enabled = stepObject.value("enabled").toBool(true);
        step.name = name;
        step.request = isDelay ? request.trimmed() : formatHexWithSpaces(request);
        step.expected = expected.trimmed().compare("TIMEOUT", Qt::CaseInsensitive) == 0 ? "TIMEOUT" : formatHexWithSpaces(expected);
        step.matchMode = matchMode.trimmed().isEmpty() ? QString::fromUtf8("前缀") : matchMode;
        step.failPolicy = failPolicy.trimmed().isEmpty() ? QString::fromUtf8("继续") : failPolicy;
        importedSteps.append(step);
    }

    if (importedSteps.isEmpty()) {
        QMessageBox::critical(this, "错误", "流程文件没有可导入的合法步骤，当前流程未改变。");
        onLogMessage(QString("导入流程失败: 无合法步骤，非法步骤数=%1").arg(invalidCount), 3);
        return;
    }

    m_flowTable->setRowCount(0);
    for (const FlowStep &step : importedSteps) {
        int row = m_flowTable->rowCount();
        m_flowTable->insertRow(row);

        QTableWidgetItem *checkItem = new QTableWidgetItem();
        checkItem->setCheckState(step.enabled ? Qt::Checked : Qt::Unchecked);
        m_flowTable->setItem(row, 0, checkItem);
        m_flowTable->setItem(row, 1, new QTableWidgetItem(step.name));
        m_flowTable->setItem(row, 2, new QTableWidgetItem(step.request));
        m_flowTable->setItem(row, 3, new QTableWidgetItem(""));

        QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromUtf8("等待"));
        statusItem->setForeground(QBrush(QColor("#d8dee9")));
        m_flowTable->setItem(row, 4, statusItem);
        m_flowTable->setItem(row, 5, new QTableWidgetItem(""));
        m_flowTable->setItem(row, 6, new QTableWidgetItem(step.expected));
        m_flowTable->setItem(row, 7, new QTableWidgetItem(step.matchMode));
        m_flowTable->setItem(row, 8, new QTableWidgetItem(step.failPolicy));
    }

    QString message = QString("已导入自动化流程: %1，步骤=%2，跳过非法=%3")
                          .arg(filePath)
                          .arg(importedSteps.size())
                          .arg(invalidCount);
    onLogMessage(message, invalidCount > 0 ? 3 : 0);
}

void UdsWidget::onExportLogClicked()
{
    if (m_consoleLog->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "日志为空，无法导出。");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出日志"), "", "Text Files (*.txt);;All Files (*)");
    if (filePath.isEmpty()) {
        return;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", QString("无法写入日志文件: %1").arg(filePath));
        onLogMessage(QString("导出日志失败: 无法写入文件 %1").arg(filePath), 3);
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << m_consoleLog->toPlainText();
    if (!file.commit()) {
        QMessageBox::critical(this, "错误", QString("保存日志文件失败: %1").arg(filePath));
        onLogMessage(QString("导出日志失败: 保存文件失败 %1").arg(filePath), 3);
        return;
    }

    onLogMessage(QString("已导出日志: %1").arg(filePath), 0);
}

void UdsWidget::onReadDtcClicked()
{
    onApplyConfig();

    QString errorMessage;
    if (!isValidHexInput(m_dtcStatusMaskEdit->text(), &errorMessage) ||
        normalizeHexInput(m_dtcStatusMaskEdit->text()).size() != 2) {
        QMessageBox::warning(this, "提示", "DTC 状态掩码必须是 1 字节十六进制值。");
        return;
    }

    QString mask = normalizeHexInput(m_dtcStatusMaskEdit->text());
    m_dtcStatusMaskEdit->setText(mask);
    m_dtcTable->setRowCount(0);
    m_udsClient->sendUdsRequest(0x19, QByteArray::fromHex(QString("02%1").arg(mask).toUtf8()));
}

void UdsWidget::onClearDtcClicked()
{
    if (QMessageBox::question(this, "确认", "确定要清除全部 DTC 吗？") != QMessageBox::Yes) {
        return;
    }

    onApplyConfig();
    m_udsClient->sendUdsRequest(0x14, QByteArray::fromHex("FFFFFF"));
}

void UdsWidget::updateDtcTable(const QByteArray &payload)
{
    if (payload.size() < 2 || static_cast<uint8_t>(payload.at(0)) != 0x02) {
        return;
    }

    m_dtcTable->setRowCount(0);
    int recordOffset = 2; // sub-function + status availability mask
    int invalidBytes = (payload.size() - recordOffset) % 4;
    for (int offset = recordOffset; offset + 3 < payload.size(); offset += 4) {
        uint32_t dtc = (static_cast<uint8_t>(payload.at(offset)) << 16) |
                       (static_cast<uint8_t>(payload.at(offset + 1)) << 8) |
                        static_cast<uint8_t>(payload.at(offset + 2));
        uint8_t status = static_cast<uint8_t>(payload.at(offset + 3));

        QString dtcText = QString("%1").arg(dtc, 6, 16, QChar('0')).toUpper();
        QString statusText = QString("0x%1").arg(status, 2, 16, QChar('0')).toUpper();
        QString rawText = QString("%1 %2 %3 %4")
                              .arg(static_cast<uint8_t>(payload.at(offset)), 2, 16, QChar('0'))
                              .arg(static_cast<uint8_t>(payload.at(offset + 1)), 2, 16, QChar('0'))
                              .arg(static_cast<uint8_t>(payload.at(offset + 2)), 2, 16, QChar('0'))
                              .arg(status, 2, 16, QChar('0'))
                              .toUpper();

        int row = m_dtcTable->rowCount();
        m_dtcTable->insertRow(row);
        m_dtcTable->setItem(row, 0, new QTableWidgetItem(dtcText));
        m_dtcTable->setItem(row, 1, new QTableWidgetItem(statusText));
        m_dtcTable->setItem(row, 2, new QTableWidgetItem(rawText));
    }

    if (m_dtcTable->rowCount() == 0) {
        onLogMessage("DTC 读取完成: 未返回故障码记录", 0);
    } else {
        onLogMessage(QString("DTC 读取完成: 共 %1 条记录").arg(m_dtcTable->rowCount()), 0);
    }
    if (invalidBytes > 0) {
        onLogMessage(QString("DTC 响应提示: 尾部存在 %1 个未解析字节").arg(invalidBytes), 3);
    }
}

bool UdsWidget::buildUpgradeConfig(UdsClient::UpgradeConfig *config, QString *errorMessage) const
{
    auto parseHexByte = [this, errorMessage](QLineEdit *edit, const QString &fieldName, uint8_t *value) -> bool {
        QString error;
        QString text = edit->text();
        if (!isValidHexInput(text, &error) || normalizeHexInput(text).size() != 2) {
            if (errorMessage) {
                *errorMessage = QString("%1 必须是 1 字节十六进制值").arg(fieldName);
            }
            return false;
        }
        *value = static_cast<uint8_t>(normalizeHexInput(text).toUInt(nullptr, 16));
        return true;
    };

    auto parseHexWord = [this, errorMessage](QLineEdit *edit, const QString &fieldName, uint16_t *value) -> bool {
        QString error;
        QString text = edit->text();
        if (!isValidHexInput(text, &error) || normalizeHexInput(text).size() != 4) {
            if (errorMessage) {
                *errorMessage = QString("%1 必须是 2 字节十六进制值").arg(fieldName);
            }
            return false;
        }
        *value = static_cast<uint16_t>(normalizeHexInput(text).toUInt(nullptr, 16));
        return true;
    };

    UdsClient::UpgradeConfig nextConfig;
    if (!parseHexByte(m_seedSubFuncEdit, QString::fromUtf8("Seed 子功能"), &nextConfig.seedSubFunction) ||
        !parseHexByte(m_keySubFuncEdit, QString::fromUtf8("Key 子功能"), &nextConfig.keySubFunction) ||
        !parseHexByte(m_dataFormatEdit, QString::fromUtf8("DFI"), &nextConfig.dataFormatIdentifier) ||
        !parseHexByte(m_addressLengthFormatEdit, QString::fromUtf8("ALFI"), &nextConfig.addressAndLengthFormat) ||
        !parseHexWord(m_routineIdEdit, QString::fromUtf8("Routine ID"), &nextConfig.checksumRoutineId)) {
        return false;
    }

    if (nextConfig.keySubFunction != static_cast<uint8_t>(nextConfig.seedSubFunction + 1)) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("Key 子功能必须等于 Seed 子功能 + 1");
        }
        return false;
    }

    int addressLen = (nextConfig.addressAndLengthFormat >> 4) & 0x0F;
    int sizeLen = nextConfig.addressAndLengthFormat & 0x0F;
    if (addressLen < 1 || addressLen > 4 || sizeLen < 1 || sizeLen > 4) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("ALFI 的地址长度和数据长度字段必须在 1 到 4 字节之间");
        }
        return false;
    }

    nextConfig.defaultBlockSize = m_defaultBlockSizeSpin->value();
    nextConfig.useEcuBlockSize = m_useEcuBlockSizeCheck->isChecked();
    nextConfig.appendCrc32 = m_appendCrcCheck->isChecked();
    nextConfig.crcBigEndian = (m_crcEndianCombo->currentIndex() == 0);
    nextConfig.resetType = static_cast<uint8_t>(m_resetTypeCombo->currentData().toUInt());

    if (config) {
        *config = nextConfig;
    }
    return true;
}

void UdsWidget::setUpgradeConfigControlsEnabled(bool enabled)
{
    m_seedSubFuncEdit->setEnabled(enabled);
    m_keySubFuncEdit->setEnabled(enabled);
    m_dataFormatEdit->setEnabled(enabled);
    m_addressLengthFormatEdit->setEnabled(enabled);
    m_defaultBlockSizeSpin->setEnabled(enabled);
    m_useEcuBlockSizeCheck->setEnabled(enabled);
    m_routineIdEdit->setEnabled(enabled);
    m_appendCrcCheck->setEnabled(enabled);
    m_crcEndianCombo->setEnabled(enabled);
    m_resetTypeCombo->setEnabled(enabled);
}

// 点击运行自动化列表流程
void UdsWidget::onRunFlowClicked()
{
    if (m_flowRunning) {
        // 主动停止
        finishFlowRun("自动化流程测试被用户中止！", 3);
        return;
    }

    if (m_flowTable->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "测试流程列表为空，请先添加步骤！");
        return;
    }

    onApplyConfig();

    m_flowRunning = true;
    m_flowCurrentIndex = 0;
    m_flowCurrentLoop = 1;
    m_flowTotalLoops = m_loopSpin->value();

    m_runFlowBtn->setText(QString::fromUtf8(" 中止发送 "));
    m_runFlowBtn->setStyleSheet(getButtonStyleSheet() + "QPushButton { background-color: #bf616a; color: #eceff4; }");
    m_addDelayBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_clearListBtn->setEnabled(false);
    m_moveUpBtn->setEnabled(false);
    m_moveDownBtn->setEnabled(false);
    m_importFlowBtn->setEnabled(false);

    // 重置所有步骤状态显示
    for (int i = 0; i < m_flowTable->rowCount(); ++i) {
        if (m_flowTable->item(i, 3)) {
            m_flowTable->item(i, 3)->setText("");
        }
        if (m_flowTable->item(i, 4)) {
            m_flowTable->item(i, 4)->setText(QString::fromUtf8("等待"));
            m_flowTable->item(i, 4)->setForeground(QBrush(QColor("#d8dee9")));
        }
        if (m_flowTable->item(i, 5)) {
            m_flowTable->item(i, 5)->setText("");
        }
    }

    onLogMessage(QString("=== 开始执行自动化测试流程 (第 1 轮/共 %1 轮) ===").arg(m_flowTotalLoops), 0);
    
    // 启动首个步骤
    m_flowTimer->start(10);
}

// 定时触发下一步骤执行
void UdsWidget::onFlowTimerTimeout()
{
    if (!m_flowRunning) return;

    // 寻找下一个选中的步骤
    while (m_flowCurrentIndex < m_flowTable->rowCount()) {
        QTableWidgetItem *chk = m_flowTable->item(m_flowCurrentIndex, 0);
        if (chk && chk->checkState() == Qt::Checked) {
            break;
        }
        m_flowCurrentIndex++;
    }

    // 判断是否一轮结束
    if (m_flowCurrentIndex >= m_flowTable->rowCount()) {
        m_flowCurrentLoop++;
        if (m_flowCurrentLoop <= m_flowTotalLoops) {
            m_flowCurrentIndex = 0;
            onLogMessage(QString("=== 开始执行自动化测试流程 (第 %1 轮/共 %2 轮) ===").arg(m_flowCurrentLoop).arg(m_flowTotalLoops), 0);
            m_flowTimer->start(m_intervalSpin->value());
        } else {
            // 全部结束
            finishFlowRun("=== 自动化流程测试全部完成！ ===", 0);
        }
        return;
    }

    // 执行当前步骤
    m_flowTable->selectRow(m_flowCurrentIndex);
    QTableWidgetItem *nameItem = m_flowTable->item(m_flowCurrentIndex, 1);
    QTableWidgetItem *pduItem = m_flowTable->item(m_flowCurrentIndex, 2);
    QTableWidgetItem *statusItem = m_flowTable->item(m_flowCurrentIndex, 4);
    if (!nameItem || !pduItem || !statusItem) {
        onLogMessage(QString("自动化流程错误: 步骤 %1 数据单元损坏").arg(m_flowCurrentIndex + 1), 3);
        m_flowCurrentIndex++;
        m_flowTimer->start(m_intervalSpin->value());
        return;
    }
    QString name = nameItem->text();
    QString pduStr = pduItem->text().replace(" ", "");

    if (name.contains(QString::fromUtf8("延时")) || name.contains("Delay")) {
        int delayVal = pduStr.toInt();
        if (delayVal <= 0) delayVal = 1000;
        
        statusItem->setText(QString::fromUtf8("延时中"));
        statusItem->setForeground(QBrush(QColor("#ebcb8b")));
        
        onLogMessage(QString("步骤 %1: 延时等待 %2 ms...").arg(m_flowCurrentIndex + 1).arg(delayVal), 0);
        
        m_flowCurrentIndex++;
        m_flowTimer->start(delayVal);
    } else {
        // UDS 诊断指令发送
        QString errorMessage;
        if (!isValidHexInput(pduStr, &errorMessage)) {
            statusItem->setText(QString::fromUtf8("错误"));
            statusItem->setForeground(QBrush(QColor("#bf616a")));
            onLogMessage(QString("自动化流程错误: 步骤 %1 请求 PDU 无效 - %2").arg(m_flowCurrentIndex + 1).arg(errorMessage), 3);
            m_flowCurrentIndex++;
            m_flowTimer->start(m_intervalSpin->value());
            return;
        }

        pduStr = normalizeHexInput(pduStr);
        pduItem->setText(formatHexWithSpaces(pduStr));
        QByteArray pdu = QByteArray::fromHex(pduStr.toUtf8());
        if (pdu.isEmpty()) {
            statusItem->setText(QString::fromUtf8("错误"));
            statusItem->setForeground(QBrush(QColor("#bf616a")));
            onLogMessage(QString("自动化流程错误: 步骤 %1 请求 PDU 转换失败").arg(m_flowCurrentIndex + 1), 3);
            m_flowCurrentIndex++;
            m_flowTimer->start(m_intervalSpin->value());
            return;
        }

        statusItem->setText(QString::fromUtf8("发送中"));
        statusItem->setForeground(QBrush(QColor("#ebcb8b")));

        uint8_t service = pdu.at(0);
        m_udsClient->sendUdsRequest(service, pdu.mid(1));
    }
}

// 接收底层的 UDS 响应反馈
void UdsWidget::onUdsResponseReceived(uint8_t serviceId, bool isPositive, const QByteArray &payload, uint8_t nrc)
{
    // 组装十六进制显示
    QString hexStr;
    if (isPositive) {
        hexStr += QString("%1 ").arg(serviceId, 2, 16, QChar('0')).toUpper();
        for (int i = 0; i < payload.size(); ++i) {
            hexStr += QString("%1 ").arg(static_cast<uint8_t>(payload.at(i)), 2, 16, QChar('0')).toUpper();
        }
    } else {
        hexStr += QString("7F %1 %2")
                    .arg(serviceId, 2, 16, QChar('0')).arg(nrc, 2, 16, QChar('0')).toUpper();
    }
    hexStr = hexStr.trimmed();

    m_pduResEdit->setText(hexStr);
    setManualResponseStatus(isPositive ? QString::fromUtf8("收到正响应") : QString::fromUtf8("收到负响应"),
                            isPositive ? QColor("#a3be8c") : QColor("#bf616a"));
    if (isPositive && serviceId == 0x59) {
        updateDtcTable(payload);
    } else if (isPositive && serviceId == 0x54) {
        m_dtcTable->setRowCount(0);
        onLogMessage("DTC 清除成功，已清空 DTC 表格", 0);
    }

    bool stepPassed = isPositive;
    bool expectationValid = true;
    QString expectationError;
    QString expectedResponse;
    QString matchMode;
    if (m_flowRunning && m_flowCurrentIndex < m_flowTable->rowCount()) {
        expectedResponse = flowCellText(m_flowCurrentIndex, 6);
        matchMode = flowCellText(m_flowCurrentIndex, 7, QString::fromUtf8("前缀"));
        if (!expectedResponse.isEmpty()) {
            if (expectedResponse.trimmed().compare("TIMEOUT", Qt::CaseInsensitive) != 0 &&
                !isValidHexInput(expectedResponse, &expectationError)) {
                expectationValid = false;
                stepPassed = false;
            } else {
                stepPassed = matchExpectedResponse(hexStr, expectedResponse, matchMode);
            }
        }
    }

    m_testCount++;
    if (stepPassed) m_passCount++;
    else m_failCount++;
    updateStats();

    // 如果是自动化执行列表中
    if (m_flowRunning && m_flowCurrentIndex < m_flowTable->rowCount()) {
        QTableWidgetItem *resItem = m_flowTable->item(m_flowCurrentIndex, 3);
        if (resItem) {
            resItem->setText(hexStr);
        }
        QTableWidgetItem *statusItem = m_flowTable->item(m_flowCurrentIndex, 4);
        if (statusItem) {
            if (stepPassed) {
                statusItem->setText(QString::fromUtf8("成功"));
                statusItem->setForeground(QBrush(QColor("#a3be8c")));
            } else if (!expectationValid) {
                statusItem->setText(QString::fromUtf8("期望错误"));
                statusItem->setForeground(QBrush(QColor("#bf616a")));
                onLogMessage(QString("自动化流程错误: 步骤 %1 期望响应无效 - %2").arg(m_flowCurrentIndex + 1).arg(expectationError), 3);
            } else if (!expectedResponse.isEmpty()) {
                statusItem->setText(QString::fromUtf8("断言失败"));
                statusItem->setForeground(QBrush(QColor("#bf616a")));
                onLogMessage(QString("自动化流程断言失败: 步骤 %1，期望=%2，实际=%3，模式=%4")
                             .arg(m_flowCurrentIndex + 1)
                             .arg(formatHexWithSpaces(expectedResponse))
                             .arg(hexStr)
                             .arg(matchMode), 3);
            } else {
                statusItem->setText(QString("错误 (0x%1)").arg(nrc, 2, 16, QChar('0')).toUpper());
                statusItem->setForeground(QBrush(QColor("#bf616a")));
            }
        }

        if (!stepPassed && shouldStopOnFlowFailure(m_flowCurrentIndex)) {
            finishFlowRun(QString("=== 自动化流程因步骤 %1 失败而停止 ===").arg(m_flowCurrentIndex + 1), 3);
            return;
        }

        m_flowCurrentIndex++;
        m_flowTimer->start(m_intervalSpin->value()); // 间隔触发下一步
    }
}

// 响应超时
void UdsWidget::onUdsResponseTimeout()
{
    m_pduResEdit->setText("TIMEOUT");
    setManualResponseStatus(QString::fromUtf8("响应超时"), QColor("#bf616a"));
    bool stepPassed = false;
    if (m_flowRunning && m_flowCurrentIndex < m_flowTable->rowCount()) {
        QString expectedResponse = flowCellText(m_flowCurrentIndex, 6);
        if (!expectedResponse.isEmpty()) {
            stepPassed = matchExpectedResponse("TIMEOUT", expectedResponse, flowCellText(m_flowCurrentIndex, 7, QString::fromUtf8("前缀")));
        }
    }

    m_testCount++;
    if (stepPassed) m_passCount++;
    else m_failCount++;
    updateStats();

    if (m_flowRunning && m_flowCurrentIndex < m_flowTable->rowCount()) {
        QTableWidgetItem *resItem = m_flowTable->item(m_flowCurrentIndex, 3);
        if (resItem) {
            resItem->setText("TIMEOUT");
        }
        QTableWidgetItem *statusItem = m_flowTable->item(m_flowCurrentIndex, 4);
        if (statusItem) {
            if (stepPassed) {
                statusItem->setText(QString::fromUtf8("成功"));
                statusItem->setForeground(QBrush(QColor("#a3be8c")));
            } else {
                statusItem->setText(QString::fromUtf8("超时"));
                statusItem->setForeground(QBrush(QColor("#bf616a")));
            }
        }

        if (!stepPassed && shouldStopOnFlowFailure(m_flowCurrentIndex)) {
            finishFlowRun(QString("=== 自动化流程因步骤 %1 超时而停止 ===").arg(m_flowCurrentIndex + 1), 3);
            return;
        }

        m_flowCurrentIndex++;
        m_flowTimer->start(m_intervalSpin->value());
    }
}

// ---------------------- 软件升级相关交互 ----------------------

// 游览并选择固件文件
void UdsWidget::onBrowseFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择固件文件"), "", "Binary Files (*.bin);;All Files (*)");
    if (!filePath.isEmpty()) {
        m_filePathEdit->setText(filePath);
        onLogMessage(QString("已选择固件文件: %1").arg(filePath), 0);
    }
}

// 开始升级
void UdsWidget::onStartUpgradeClicked()
{
    onApplyConfig();

    QString filePath = m_filePathEdit->text();
    if (filePath.isEmpty()) {
        QMessageBox::critical(this, "错误", "请先选择一个有效的固件文件！");
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "错误", QString("无法打开文件: %1").arg(filePath));
        return;
    }

    QByteArray firmwareData = file.readAll();
    file.close();

    bool ok;
    uint32_t flashAddress = m_flashAddrEdit->text().toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::critical(this, "错误", "起始 Flash 物理地址必须是有效的十六进制数值！");
        return;
    }

    UdsClient::UpgradeConfig upgradeConfig;
    QString configError;
    if (!buildUpgradeConfig(&upgradeConfig, &configError)) {
        QMessageBox::critical(this, "错误", QString("刷写参数配置无效: %1").arg(configError));
        return;
    }
    m_udsClient->setUpgradeConfig(upgradeConfig);

    // 禁用交互控件，升级期间防止误操作
    m_startUpgradeBtn->setEnabled(false);
    m_browseFileBtn->setEnabled(false);
    m_flashAddrEdit->setEnabled(false);
    setUpgradeConfigControlsEnabled(false);
    m_channelCombo->setEnabled(false);
    m_protocolCombo->setEnabled(false);
    m_frameTypeCombo->setEnabled(false);
    m_reqIdEdit->setEnabled(false);
    m_resIdEdit->setEnabled(false);
    m_funcIdEdit->setEnabled(false);
    m_paramDidEdit->setEnabled(false);
    m_testerPresentCheck->setEnabled(false);
    m_testerPresentIntervalSpin->setEnabled(false);
    m_abortUpgradeBtn->setEnabled(true);

    m_upgradeProgressBar->setValue(0);
    m_upgradeStatusLabel->setText("当前状态：启动中...");

    if (!m_udsClient->startUpgrade(firmwareData, flashAddress)) {
        onUpgradeCompleted(false, "无法开启升级状态机");
    }
}

// 中止升级
void UdsWidget::onAbortUpgradeClicked()
{
    m_udsClient->abortUpgrade();
}

// 升级状态文字变化槽
void UdsWidget::onUpgradeStateChanged(const QString &stateName)
{
    m_upgradeStatusLabel->setText(QString("当前状态：%1").arg(stateName));
}

// 升级进度平滑更新槽
void UdsWidget::onUpgradeProgress(int currentBytes, int totalBytes, int percent)
{
    m_upgradeProgressBar->setValue(percent);
    m_upgradeStatusLabel->setText(QString("数据块传输进度: %1 / %2 字节 (%3%)")
                                  .arg(currentBytes).arg(totalBytes).arg(percent));
}

// 升级完毕结果呈现槽
void UdsWidget::onUpgradeCompleted(bool success, const QString &errorMsg)
{
    if (success) {
        m_upgradeProgressBar->setValue(100);
        m_upgradeStatusLabel->setText("当前状态：固件写入并校验成功！ECU 已重启！");
        QMessageBox::information(this, "成功", "ECU 固件写入并校验成功，系统已重启！");
    } else {
        m_upgradeStatusLabel->setText(QString("当前状态：升级失败 - %1").arg(errorMsg));
        QMessageBox::critical(this, "错误", QString("固件写入升级失败：\n%1").arg(errorMsg));
    }

    // 重新启用配置交互控件
    m_startUpgradeBtn->setEnabled(true);
    m_browseFileBtn->setEnabled(true);
    m_flashAddrEdit->setEnabled(true);
    setUpgradeConfigControlsEnabled(true);
    m_channelCombo->setEnabled(true);
    m_protocolCombo->setEnabled(true);
    m_frameTypeCombo->setEnabled(true);
    m_reqIdEdit->setEnabled(true);
    m_resIdEdit->setEnabled(true);
    m_funcIdEdit->setEnabled(true);
    m_paramDidEdit->setEnabled(true);
    m_testerPresentCheck->setEnabled(true);
    m_testerPresentIntervalSpin->setEnabled(true);
    m_abortUpgradeBtn->setEnabled(false);
}

// 控制台日志呈现分发槽
void UdsWidget::onLogMessage(const QString &msg, int type)
{
    QString timeStr = QTime::currentTime().toString("hh:mm:ss.zzz");
    LogEntry entry;
    entry.time = timeStr;
    entry.message = msg;
    entry.type = type;
    m_logEntries.append(entry);

    const int maxLogEntries = 5000;
    while (m_logEntries.size() > maxLogEntries) {
        m_logEntries.removeFirst();
    }

    refreshLogView();
    return;

    QString logLine;
    
    // 使用富文本进行终端配色渲染
    switch(type) {
        case 1: // TX
            logLine = QString("<font color=\"#a3be8c\">[%1] <b>[TX]</b> %2</font>").arg(timeStr).arg(msg.toHtmlEscaped());
            break;
        case 2: // RX Positive
            logLine = QString("<font color=\"#88c0d0\">[%1] <b>[RX]</b> %2</font>").arg(timeStr).arg(msg.toHtmlEscaped());
            break;
        case 3: // RX Negative / Error
            logLine = QString("<font color=\"#bf616a\">[%1] <b>[ERR]</b> %2</font>").arg(timeStr).arg(msg.toHtmlEscaped());
            break;
        default: // Info / Notice
            logLine = QString("<font color=\"#d8dee9\">[%1] <b>[SYS]</b> %2</font>").arg(timeStr).arg(msg.toHtmlEscaped());
            break;
    }
    
    m_consoleLog->appendHtml(logLine);
    m_consoleLog->verticalScrollBar()->setValue(m_consoleLog->verticalScrollBar()->maximum());
}

// ---------------------- 精美样式表 (Stylesheet Helpers) ----------------------

QString UdsWidget::getButtonStyleSheet()
{
    return "QPushButton {"
           "  background-color: #3b4252; color: #d8dee9;"
           "  border: 1px solid #4c566a; border-radius: 4px;"
           "  padding: 5px 10px; font-size: 12px; font-family: 'Segoe UI', Arial;"
           "}"
           "QPushButton:hover {"
           "  background-color: #88c0d0; color: #2e3440;"
           "}"
           "QPushButton:pressed {"
           "  background-color: #81a1c1;"
           "}"
           "QPushButton:disabled {"
           "  background-color: #2e3440; color: #4c566a;"
           "}";
}

QString UdsWidget::getLineEditStyleSheet()
{
    return "QLineEdit, QSpinBox {"
           "  background-color: #3b4252; color: #eceff4;"
           "  border: 1px solid #4c566a; border-radius: 4px;"
           "  padding: 3px 6px; font-size: 12px; font-family: 'Segoe UI', Arial;"
           "}"
           "QLineEdit:focus, QSpinBox:focus {"
           "  border: 1px solid #88c0d0;"
           "}";
}

QString UdsWidget::getComboBoxStyleSheet()
{
    return "QComboBox {"
           "  background-color: #3b4252; color: #eceff4;"
           "  border: 1px solid #4c566a; border-radius: 4px;"
           "  padding: 3px 20px 3px 6px; font-size: 12px;"
           "}"
           "QComboBox::drop-down {"
           "  subcontrol-origin: padding;"
           "  subcontrol-position: top right; width: 15px;"
           "  border-left-width: 1px; border-left-color: #4c566a; border-left-style: solid;"
           "}"
           "QComboBox QAbstractItemView {"
           "  background-color: #2e3440; color: #d8dee9; selection-background-color: #434c5e; selection-color: #88c0d0;"
           "}";
}

QString UdsWidget::getTableStyleSheet()
{
    return "QTableWidget {"
           "  background-color: #2e3440; gridline-color: #3b4252; color: #eceff4;"
           "  border: 1px solid #3b4252; font-size: 12px;"
           "}"
           "QHeaderView::section {"
           "  background-color: #3b4252; color: #d8dee9; padding: 4px;"
           "  border: 1px solid #2e3440; font-weight: bold;"
           "}"
           "QTableWidget::item:selected {"
           "  background-color: #434c5e; color: #88c0d0;"
           "}";
}

QString UdsWidget::getTreeStyleSheet()
{
    return "QTreeWidget {"
           "  background-color: #2e3440; color: #eceff4;"
           "  border: 1px solid #3b4252; font-size: 12px;"
           "}"
           "QHeaderView::section {"
           "  background-color: #3b4252; color: #d8dee9;"
           "  border: 1px solid #2e3440; font-weight: bold;"
           "}"
           "QTreeView::item:hover {"
           "  background-color: #3b4252;"
           "}"
           "QTreeView::item:selected {"
           "  background-color: #434c5e; color: #88c0d0; font-weight: bold;"
           "}";
}

void UdsWidget::loadSettings()
{
    QSettings settings("SQ_CAN_APP", "UdsConfig");
    m_channelCombo->setCurrentIndex(settings.value("channel", 0).toInt());
    m_protocolCombo->setCurrentIndex(settings.value("protocol", 0).toInt());
    m_frameTypeCombo->setCurrentIndex(settings.value("frameType", 0).toInt());
    m_reqIdEdit->setText(settings.value("reqId", "7E0").toString());
    m_resIdEdit->setText(settings.value("resId", "7E8").toString());
    m_funcIdEdit->setText(settings.value("funcId", "7DF").toString());
    m_paramDidEdit->setText(settings.value("paramDid", "F1A0").toString());
    m_testerPresentCheck->setChecked(settings.value("testerPresent", false).toBool());
    m_testerPresentIntervalSpin->setValue(settings.value("testerPresentInterval", 2000).toInt());
}

void UdsWidget::saveSettings()
{
    QSettings settings("SQ_CAN_APP", "UdsConfig");
    settings.setValue("channel", m_channelCombo->currentIndex());
    settings.setValue("protocol", m_protocolCombo->currentIndex());
    settings.setValue("frameType", m_frameTypeCombo->currentIndex());
    settings.setValue("reqId", m_reqIdEdit->text());
    settings.setValue("resId", m_resIdEdit->text());
    settings.setValue("funcId", m_funcIdEdit->text());
    settings.setValue("paramDid", m_paramDidEdit->text());
    settings.setValue("testerPresent", m_testerPresentCheck->isChecked());
    settings.setValue("testerPresentInterval", m_testerPresentIntervalSpin->value());
}
