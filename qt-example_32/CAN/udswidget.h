#ifndef UDSWIDGET_H
#define UDSWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QColor>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QSplitter>
#include <QTimer>
#include <QVector>
#include "udsclient.h"

class UdsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit UdsWidget(QWidget *parent = nullptr);
    ~UdsWidget();

    void setCanThread(CANThread *thread);
    UdsClient* udsClient() const { return m_udsClient; }
    uint16_t getParameterWriteDid() const;

private slots:
    // 配置变更槽
    void onApplyConfig();
    void onTesterPresentStateChanged(int state);

    // 服务树双击填充
    void onServiceTreeDoubleClicked(QTreeWidgetItem *item, int column);

    // 诊断调试槽
    void onSendImmediateClicked();
    void onAddToListClicked();

    // 自动化列表操作槽
    void onAddDelayClicked();
    void onDeleteStepClicked();
    void onClearListClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onRunFlowClicked();
    void onFlowTimerTimeout();
    void onImportFlowClicked();
    void onExportFlowClicked();
    void onExportLogClicked();
    void onReadDtcClicked();
    void onClearDtcClicked();
    void filterServiceTree(const QString &keyword);
    void refreshLogView();

    // 软件升级槽
    void onBrowseFileClicked();
    void onStartUpgradeClicked();
    void onAbortUpgradeClicked();

    // 协议信号槽
    void onUdsResponseReceived(uint8_t serviceId, bool isPositive, const QByteArray &payload, uint8_t nrc);
    void onUdsResponseTimeout();
    void onUpgradeStateChanged(const QString &stateName);
    void onUpgradeProgress(int currentBytes, int totalBytes, int percent);
    void onUpgradeCompleted(bool success, const QString &errorMsg);
    void onLogMessage(const QString &msg, int type);

private:
    void setupUi();
    void initServiceTree();
    void updateStats();
    void loadSettings();
    void saveSettings();
    QString normalizeHexInput(const QString &input) const;
    bool isValidHexInput(const QString &input, QString *errorMessage) const;
    QString formatHexWithSpaces(const QString &hex) const;
    QString flowCellText(int row, int column, const QString &defaultValue = QString()) const;
    bool matchExpectedResponse(const QString &actualResponse, const QString &expectedResponse, const QString &matchMode) const;
    bool shouldStopOnFlowFailure(int row) const;
    void finishFlowRun(const QString &message, int logType);
    void updateDtcTable(const QByteArray &payload);
    bool buildUpgradeConfig(UdsClient::UpgradeConfig *config, QString *errorMessage) const;
    void setUpgradeConfigControlsEnabled(bool enabled);
    void setManualResponseStatus(const QString &text, const QColor &color);
    QString formatLogLine(const QString &time, const QString &message, int type) const;
    QString logTypeLabel(int type) const;

    struct LogEntry {
        QString time;
        QString message;
        int type;
    };
    
    // UI 样式表定义
    QString getButtonStyleSheet();
    QString getLineEditStyleSheet();
    QString getComboBoxStyleSheet();
    QString getTableStyleSheet();
    QString getTreeStyleSheet();

    // 协议层成员
    UdsClient *m_udsClient;
    CANThread *m_canThread;

    // 1. 顶部配置控件
    QComboBox *m_channelCombo;
    QComboBox *m_protocolCombo;
    QComboBox *m_frameTypeCombo;
    QLineEdit *m_reqIdEdit;
    QLineEdit *m_funcIdEdit;
    QLineEdit *m_resIdEdit;
    QCheckBox *m_testerPresentCheck;
    QSpinBox *m_testerPresentIntervalSpin;
    QLineEdit *m_paramDidEdit;

    // 2. 左侧服务树
    QLineEdit *m_serviceSearchEdit;
    QTreeWidget *m_serviceTree;

    // 3. 右侧 Tab 工作区
    QTabWidget *m_workTabWidget;
    QWidget *m_diagTab;
    QWidget *m_dtcTab;
    QWidget *m_upgradeTab;

    // 3.1 诊断调试 Tab 控件
    QLineEdit *m_pduReqEdit;
    QLineEdit *m_pduResEdit;
    QLabel *m_pduStatusLabel;
    QPushButton *m_sendBtn;
    QPushButton *m_addToListBtn;
    
    QTableWidget *m_flowTable;
    QPushButton *m_addDelayBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_clearListBtn;
    QPushButton *m_moveUpBtn;
    QPushButton *m_moveDownBtn;
    QPushButton *m_runFlowBtn;
    QPushButton *m_importFlowBtn;
    QPushButton *m_exportFlowBtn;
    QPushButton *m_exportLogBtn;

    QLineEdit *m_dtcStatusMaskEdit;
    QPushButton *m_readDtcBtn;
    QPushButton *m_clearDtcBtn;
    QTableWidget *m_dtcTable;
    QSpinBox *m_loopSpin;
    QSpinBox *m_intervalSpin;

    // 3.2 软件升级 Tab 控件
    QLineEdit *m_filePathEdit;
    QPushButton *m_browseFileBtn;
    QLineEdit *m_flashAddrEdit;
    QLineEdit *m_seedSubFuncEdit;
    QLineEdit *m_keySubFuncEdit;
    QLineEdit *m_dataFormatEdit;
    QLineEdit *m_addressLengthFormatEdit;
    QSpinBox *m_defaultBlockSizeSpin;
    QCheckBox *m_useEcuBlockSizeCheck;
    QLineEdit *m_routineIdEdit;
    QCheckBox *m_appendCrcCheck;
    QComboBox *m_crcEndianCombo;
    QComboBox *m_resetTypeCombo;
    QProgressBar *m_upgradeProgressBar;
    QLabel *m_upgradeStatusLabel;
    QPushButton *m_startUpgradeBtn;
    QPushButton *m_abortUpgradeBtn;

    // 4. 底部日志与统计控件
    QPlainTextEdit *m_consoleLog;
    QComboBox *m_logFilterCombo;
    QLineEdit *m_logSearchEdit;
    QLabel *m_testCountLabel;
    QLabel *m_passCountLabel;
    QLabel *m_failCountLabel;
    QPushButton *m_resetStatsBtn;
    QPushButton *m_clearLogBtn;
    QVector<LogEntry> m_logEntries;

    // 状态统计变量
    int m_testCount;
    int m_passCount;
    int m_failCount;

    // 自动化执行管理变量
    bool m_flowRunning;
    int m_flowCurrentIndex;
    int m_flowCurrentLoop;
    int m_flowTotalLoops;
    QTimer *m_flowTimer;
};

#endif // UDSWIDGET_H
