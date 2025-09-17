#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QSplitter>
#include <QScrollArea>
#include <QGridLayout>
#include <QTimer>
#include <QDebug>
#include <QTextEdit>
#include <QFont>
#include <QListWidget>
#include <QSet>
#include <algorithm>
#include <exception>

#include "DroneManager.h"
#include "RadarSimulator.h"
#include "RadarDisplay.h"
#include "StatisticsManager.h"
#include "WeaponStrategy.h"

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr) : QWidget(parent)
    {
        qDebug() << "开始初始化主窗口...";

        // 首先初始化系统组件
        qDebug() << "初始化无人机管理器...";
        m_droneManager = new DroneManager(1600.0, this);

        qDebug() << "初始化雷达仿真器...";
        m_radarSimulator = new RadarSimulator(m_droneManager, this);

        qDebug() << "初始化雷达显示器...";
        m_radarDisplay = new RadarDisplay(this); // 将雷达显示器作为子控件

        qDebug() << "初始化统计管理器...";
        m_statisticsManager = new StatisticsManager(this);

        qDebug() << "初始化武器策略系统...";
        m_weaponStrategy = new WeaponStrategy(m_droneManager, this);

        // 配置雷达
        qDebug() << "配置雷达参数...";
        m_radarSimulator->setRadarCenter(QPointF(0, 0));
        m_radarSimulator->setRadarRadius(800.0);
        m_radarSimulator->setScanInterval(100);  // 默认扫描间隔改为0.1秒(100毫秒)

        // 然后设置UI
        qDebug() << "设置UI界面...";
        setupUI();

        qDebug() << "应用样式...";
        setupStyles(); // 添加样式设置

        // 连接信号
        qDebug() << "连接信号...";
        // 【修改2】: 将所有信号连接操作集中到一个独立的函数中，使构造函数更整洁
        setupConnections();

        // 启动系统
        qDebug() << "启动系统组件...";
        m_droneManager->startUpdateLoop(100);
        m_droneManager->startAutoGeneration(1000);  // 默认生成间隔改为1秒

        // 初始生成一个无人机供演示
        QTimer::singleShot(500, this, [this]() {
            m_droneManager->generateRandomDrone();
            qDebug() << "初始生成演示无人机";
        });

        // 启动雷达服务器和扫描
        qDebug() << "启动雷达服务器...";
        m_radarSimulator->startServer(12345);
        m_radarSimulator->startConfigServer(12347); // 启动配置服务器
        m_radarSimulator->startRadar();

        // 连接显示器到雷达并注册客户端
        qDebug() << "连接雷达显示器...";
        m_radarDisplay->connectToRadar("127.0.0.1", 12345);

        // 立即注册客户端（不依赖信号）
        QTimer::singleShot(200, this, [this]() {
            m_radarSimulator->addClient(QHostAddress::LocalHost, 12346);
            qDebug() << "强制注册UDP客户端端口 12346";
        });

        setWindowTitle("无人机雷达系统 - 威胁评估与打击");
        resize(1400, 900);

        // 延迟更新初始状态，确保所有组件都已初始化
        QTimer::singleShot(1000, this, [this]() {
            if (m_droneManager) {
                updateDroneCount();
            }
        });
        QTimer::singleShot(1200, this, [this]() {
            if (m_droneManager && m_radarDisplay) {
                updateRadarStatusInfo();
            }
        });

        qDebug() << "主窗口初始化完成!";
    }

private slots:
    void onStartStopDroneManager();
    void onStartStopRadar();
    void onGenerationIntervalChanged(double interval);
    void onStrike();
    void onGroupStrike();
    void onStrategicStrike();
    void onPriorityTargets();
    void onHighPriorityThreatDetected(int droneId, double threatScore);
    void onStrikeRequested(QPointF position, double radius);
    void onDroneClicked(int droneId, QPointF position);
    void onStrikeModeToggled(bool enabled);
    void onDroneAddedForStats(int droneId);
    void onDroneDestroyedForStats(int droneId);
    void onDroneEscapedForStats(int droneId); // 【新增3】: 添加槽函数声明
    void onStrikeExecutedForStats(QPointF center, double radius, int destroyedCount);
    void onStatisticsUpdated(const DefenseStatistics& stats);
    void onGenerateReport();
    void onExportData();
    void updateDroneCount();
    void updateRadarStatusInfo();
    void updateThreatList(const QList<RadarDetection>& detections);
    void onRadarScanCompleted(const QList<RadarDetection>& detections);
    void onAutoFireToggled(bool enabled);
    void onStrategyChanged(const WeaponConfig& config);
    void onWeaponFired(QPointF target, double radius, WeaponType type);
    void onCooldownComplete();
    void updateWeaponStatus();

private:
    void setupUI();
    void setupStyles();
    void setupConnections();
    void addLogMessage(const QString& message, const QColor& color); // 【新增4】: 添加辅助函数声明

private:
    DroneManager* m_droneManager;
    RadarSimulator* m_radarSimulator;
    RadarDisplay* m_radarDisplay;
    StatisticsManager* m_statisticsManager;
    WeaponStrategy* m_weaponStrategy;

    // UI控件
    QPushButton* m_startStopDroneButton;
    QPushButton* m_startStopRadarButton;
    QDoubleSpinBox* m_generationInterval;
    QDoubleSpinBox* m_scanInterval;
    QDoubleSpinBox* m_radarRadius;
    QLabel* m_strikeStatusLabel;
    QLabel* m_defenseEfficiencyLabel;
    QLabel* m_totalEventsLabel;
    QCheckBox* m_strikeModeToggle;
    QListWidget* m_threatListWidget;
    QListWidget* m_eventLogWidget; // 【新增5】: 添加UI控件指针

    // 武器策略系统UI
    QLabel* m_weaponStatusLabel;
    QPushButton* m_laserThreatButton;
    QPushButton* m_missileThreatButton;
    QPushButton* m_autoFireButton;

    // 状态跟踪
    QSet<int> m_dronesInRadar; // 【新增6】: 添加状态跟踪集合
};

// ===================================================================
//              函数实现
// ===================================================================

void MainWindow::onStartStopDroneManager()
{
    static bool isGenerating = true;
    if (!isGenerating) {
        m_droneManager->startAutoGeneration(m_generationInterval->value() * 1000);
        m_startStopDroneButton->setText("停止无人机生成");
        isGenerating = true;
    } else {
        m_droneManager->stopAutoGeneration();
        m_startStopDroneButton->setText("开始无人机生成");
        isGenerating = false;
    }
}

void MainWindow::onStartStopRadar()
{
    if (m_radarSimulator->isRunning()) {
        m_radarSimulator->stopRadar();
        m_radarDisplay->setRadarRunning(false);
        m_startStopRadarButton->setText("启动雷达");
    } else {
        m_radarSimulator->setScanInterval(m_scanInterval->value() * 1000);
        m_radarSimulator->setRadarRadius(m_radarRadius->value());
        m_radarSimulator->startRadar();
        m_radarDisplay->setRadarRunning(true);
        m_startStopRadarButton->setText("停止雷达");
    }
}

void MainWindow::onGenerationIntervalChanged(double interval)
{
    if (m_droneManager && !m_droneManager->getActiveDrones().isEmpty()) {
        m_droneManager->stopAutoGeneration();
        m_droneManager->startAutoGeneration(interval * 1000);
    }
}

void MainWindow::onStrike()
{
    double strikeRadius = 80.0;
    double radarRadius = m_radarSimulator->getRadarRadius();
    QList<Drone*> threatSortedDrones = m_droneManager->getThreatSortedDrones();
    if (threatSortedDrones.isEmpty()) {
        m_strikeStatusLabel->setText("单体打击: 雷达范围内无目标");
        return;
    }
    Drone* highestThreatDrone = nullptr;
    for (Drone* drone : threatSortedDrones) {
        QPointF dronePos = drone->getCurrentPosition();
        double distance = qSqrt(dronePos.x() * dronePos.x() + dronePos.y() * dronePos.y());
        if (distance <= radarRadius) {
            highestThreatDrone = drone;
            break;
        }
    }
    if (!highestThreatDrone) {
        m_strikeStatusLabel->setText("单体打击: 雷达范围内无目标");
        return;
    }
    QPointF targetPos = highestThreatDrone->getCurrentPosition();
    double targetThreat = highestThreatDrone->getThreatScore();
    m_droneManager->strikeTarget(targetPos, strikeRadius);
    m_radarDisplay->addStrikeEffect(targetPos, strikeRadius);
    m_strikeStatusLabel->setText(QString("单体打击: 摧毁目标ID%1(类型%2)，位置(%3,%4)，威胁值%5")
                                     .arg(highestThreatDrone->getId())
                                     .arg("Standard")
                                     .arg(targetPos.x(), 0, 'f', 1)
                                     .arg(targetPos.y(), 0, 'f', 1)
                                     .arg(targetThreat, 0, 'f', 1));
}

void MainWindow::onGroupStrike()
{
    double strikeRadius = 150.0;
    double radarRadius = m_radarSimulator->getRadarRadius();
    QPointF optimalPoint = m_droneManager->findOptimalStrikePoint(strikeRadius, radarRadius);
    QList<Drone*> targetsInRange = m_droneManager->getDronesInStrikeRange(optimalPoint, strikeRadius);
    QList<Drone*> targetsInRadar;
    for (Drone* drone : targetsInRange) {
        QPointF dronePos = drone->getCurrentPosition();
        double distance = qSqrt(dronePos.x() * dronePos.x() + dronePos.y() * dronePos.y());
        if (distance <= radarRadius) {
            targetsInRadar.append(drone);
        }
    }
    if (targetsInRadar.isEmpty()) {
        m_strikeStatusLabel->setText("群体打击: 雷达范围内无合适群体目标");
        return;
    }
    double totalThreat = 0;
    for (Drone* drone : targetsInRadar) {
        totalThreat += drone->getThreatScore();
    }
    m_droneManager->strikeTarget(optimalPoint, strikeRadius);
    m_radarDisplay->addStrikeEffect(optimalPoint, strikeRadius);
    m_strikeStatusLabel->setText(QString("群体打击: 最优点(%1,%2)，摧毁%3个目标，总威胁值%4")
                                     .arg(optimalPoint.x(), 0, 'f', 1)
                                     .arg(optimalPoint.y(), 0, 'f', 1)
                                     .arg(targetsInRadar.size())
                                     .arg(totalThreat, 0, 'f', 1));
}

void MainWindow::onStrategicStrike()
{
    double radarRadius = m_radarSimulator->getRadarRadius();
    QPointF radarCenter(0, 0);
    bool success = m_weaponStrategy->executeStrike(radarCenter, radarRadius);
    if (!success) {
        m_strikeStatusLabel->setText(QString("策略打击失败: %1 - 冷却中").arg(m_weaponStrategy->getStatusText()));
    }
}

void MainWindow::onPriorityTargets()
{
    double radarRadius = m_radarSimulator->getRadarRadius();
    QPointF radarCenter(0, 0);
    QList<Drone*> priorityTargets = m_droneManager->getPriorityTargets(radarCenter, radarRadius, 5);
    for (Drone* drone : priorityTargets) {
        QPointF pos = drone->getCurrentPosition();
        m_radarDisplay->addStrikeEffect(pos, 50.0);
    }
}

void MainWindow::onHighPriorityThreatDetected(int droneId, double threatScore)
{
    m_statisticsManager->recordHighThreatDetected(droneId, threatScore);
    qDebug() << "High priority threat detected: Drone" << droneId << "threat score:" << threatScore;
}

void MainWindow::onStrikeRequested(QPointF position, double radius)
{
    double distanceFromCenter = qSqrt(position.x() * position.x() + position.y() * position.y());
    if (distanceFromCenter > m_radarSimulator->getRadarRadius()) {
        m_strikeStatusLabel->setText("交互打击: 目标超出雷达范围");
        return;
    }
    QList<Drone*> dronesInRange = m_droneManager->getDronesInStrikeRange(position, radius);
    int droneCount = dronesInRange.size();
    double totalThreat = 0;
    for (Drone* drone : dronesInRange) {
        totalThreat += drone->getThreatScore();
    }
    m_droneManager->strikeTarget(position, radius);
    m_strikeStatusLabel->setText(QString("交互打击: 位置(%1,%2) 目标%3个 威胁值%4")
                                     .arg(position.x(), 0, 'f', 1)
                                     .arg(position.y(), 0, 'f', 1)
                                     .arg(droneCount)
                                     .arg(totalThreat, 0, 'f', 1));
    qDebug() << "Interactive strike requested at" << position << "targeting" << droneCount << "drones";
}

void MainWindow::onDroneClicked(int droneId, QPointF position)
{
    Drone* drone = m_droneManager->getDroneById(droneId);
    if (drone) {
        // ...
    }
    qDebug() << "Drone clicked:" << droneId << "at position" << position;
}

void MainWindow::onStrikeModeToggled(bool enabled)
{
    qDebug() << "Strike mode toggled:" << enabled;
}

void MainWindow::onDroneAddedForStats(int droneId)
{
    Drone* drone = m_droneManager->getDroneById(droneId);
    if (drone) {
        m_statisticsManager->recordDroneSpawned(droneId, drone->getType(), drone->getCurrentPosition());
    }
}

void MainWindow::onDroneDestroyedForStats(int droneId)
{
    // 统计逻辑
    Drone* drone = m_droneManager->getDroneById(droneId);
    if (drone) {
        double threatValue = m_droneManager->calculateAdvancedThreatScore(drone, QPointF(0, 0));
        m_statisticsManager->recordDroneDestroyed(droneId, drone->getType(),
                                                  drone->getCurrentPosition(), threatValue);
    }
    // 【修改8】: 添加日志记录
    addLogMessage(QString("无人机 %1 已被击毁！").arg(droneId), QColor("#f44336")); // 红色
}

void MainWindow::onDroneEscapedForStats(int droneId)
{
    // 统计逻辑
    Drone* drone = m_droneManager->getDroneById(droneId);
    if (drone) {
        m_statisticsManager->recordDroneEscaped(droneId, drone->getType(),
                                                drone->getCurrentPosition());
    }
    // 【新增9】: 添加日志记录
    addLogMessage(QString("无人机 %1 飞离区域。").arg(droneId), QColor("#ffab40")); // 橙黄色
}

void MainWindow::onStrikeExecutedForStats(QPointF center, double radius, int destroyedCount)
{
    m_statisticsManager->recordStrikeExecuted(center, radius, destroyedCount);
}

void MainWindow::onStatisticsUpdated(const DefenseStatistics& stats)
{
    m_defenseEfficiencyLabel->setText(QString("防御效率: %1%").arg(stats.defenseEfficiency, 0, 'f', 1));
    int totalEvents = stats.totalDronesSpawned + stats.totalStrikesExecuted +
                      stats.totalInterceptsExecuted + stats.highThreatEvents;
    m_totalEventsLabel->setText(QString("总事件数: %1").arg(totalEvents));
}

void MainWindow::onGenerateReport()
{
    QString report = m_statisticsManager->generateReport();
    QWidget* reportWindow = new QWidget();
    reportWindow->setWindowTitle("防御统计报告");
    reportWindow->resize(600, 400);
    QVBoxLayout* layout = new QVBoxLayout(reportWindow);
    QTextEdit* reportText = new QTextEdit();
    reportText->setPlainText(report);
    reportText->setReadOnly(true);
    reportText->setFont(QFont("Consolas", 10));
    QPushButton* closeButton = new QPushButton("关闭");
    connect(closeButton, &QPushButton::clicked, reportWindow, &QWidget::close);
    layout->addWidget(reportText);
    layout->addWidget(closeButton);
    reportWindow->show();
}

void MainWindow::onExportData()
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString jsonFilename = QString("defense_stats_%1.json").arg(timestamp);
    QString csvFilename = QString("defense_events_%1.csv").arg(timestamp);
    bool jsonSuccess = m_statisticsManager->exportToJson(jsonFilename);
    bool csvSuccess = m_statisticsManager->exportToCsv(csvFilename);
    // ... (status message can be shown in a label if needed)
}

void MainWindow::updateDroneCount()
{
    if (!m_droneManager) {
        qWarning() << "DroneManager not initialized!";
        return;
    }
}

void MainWindow::updateRadarStatusInfo()
{
    try {
        if (!m_droneManager || !m_radarDisplay) return;
        QList<Drone*> activeDrones = m_droneManager->getActiveDrones();
        int droneCount = activeDrones.size();
        static int totalDetected = 0;
        if (droneCount > totalDetected) totalDetected = droneCount;
        m_radarDisplay->setStatusInfo(
            "已连接", droneCount, totalDetected, "刚刚", "系统运行中",
            QString("无人机数量: %1").arg(droneCount), "雷达状态: 运行中"
            );
    } catch (...) { /* ... */ }
}

void MainWindow::updateThreatList(const QList<RadarDetection>& detections)
{
    try {
        if (!m_threatListWidget || !m_droneManager) return;
        m_threatListWidget->clear();
        if (detections.isEmpty()) {
            QListWidgetItem* item = new QListWidgetItem("无威胁目标");
            item->setTextAlignment(Qt::AlignCenter);
            m_threatListWidget->addItem(item);
            m_radarDisplay->clearLaserTarget();
            return;
        }
        QList<QPair<Drone*, double>> droneThreats;
        for (const RadarDetection& detection : detections) {
            Drone* drone = m_droneManager->getDroneById(detection.droneId);
            if (drone) {
                double pixelDistance = qSqrt(detection.position.x() * detection.position.x() +
                                             detection.position.y() * detection.position.y());
                double scaleFactor = m_radarDisplay->getScaleFactor();
                double distanceToCenter = pixelDistance * scaleFactor;
                double threatScore = 1000.0 / qMax(10.0, distanceToCenter);
                droneThreats.append(qMakePair(drone, threatScore));
            }
        }
        std::sort(droneThreats.begin(), droneThreats.end(),
                  [](const QPair<Drone*, double>& a, const QPair<Drone*, double>& b) {
                      if (!a.first || !b.first) return false;
                      return a.second > b.second;
                  });
        for (int i = 0; i < droneThreats.size(); ++i) {
            const auto& pair = droneThreats[i];
            Drone* drone = pair.first;
            double threatScore = pair.second;
            if (!drone) continue;
            QString itemText = QString("ID:%1 威胁:%2").arg(drone->getId()).arg(threatScore, 0, 'f', 1);
            if (i == 0) itemText += " 已锁定";
            QListWidgetItem* item = new QListWidgetItem(itemText);
            QFont font = item->font();
            font.setPointSize(10);
            font.setBold(true);
            item->setFont(font);
            if (threatScore >= 10.0) item->setBackground(QColor(255, 0, 0, 100));
            else if (threatScore >= 8.0) item->setBackground(QColor(255, 50, 50, 100));
            else if (threatScore >= 6.0) item->setBackground(QColor(255, 100, 0, 100));
            else if (threatScore >= 4.0) item->setBackground(QColor(255, 200, 0, 100));
            else if (threatScore >= 2.0) item->setBackground(QColor(100, 255, 100, 100));
            else item->setBackground(QColor(50, 200, 50, 100));
            m_threatListWidget->addItem(item);
        }
        if (!droneThreats.isEmpty()) {
            m_radarDisplay->setLaserTarget(droneThreats.first().first->getId());
        } else {
            m_radarDisplay->clearLaserTarget();
        }
    } catch (...) { /* ... */ }
}

void MainWindow::onRadarScanCompleted(const QList<RadarDetection>& detections)
{
    static qint64 lastUpdateTime = 0;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (currentTime - lastUpdateTime < 100) {
        return;
    }
    lastUpdateTime = currentTime;

    // 【新增10】: 检测无人机进入的逻辑
    QSet<int> currentDroneIds;
    for (const auto& detection : detections) {
        currentDroneIds.insert(detection.droneId);
    }
    for (int id : currentDroneIds) {
        if (!m_dronesInRadar.contains(id)) {
            addLogMessage(QString("无人机 %1 进入区域。").arg(id), QColor("#4caf50")); // 绿色
        }
    }
    m_dronesInRadar = currentDroneIds; // 更新状态以供下次比较

    // 你原来的函数内容
    updateDroneCount();
    updateRadarStatusInfo();
    updateThreatList(detections);
}

void MainWindow::onAutoFireToggled(bool enabled)
{
    if (enabled) {
        m_autoFireButton->setText("停止自动开火");
        m_weaponStrategy->setAutoFire(true);
    } else {
        m_autoFireButton->setText("开始自动开火");
        m_weaponStrategy->setAutoFire(false);
    }
}

void MainWindow::onStrategyChanged(const WeaponConfig& config)
{
    m_weaponStatusLabel->setText(QString("当前策略: %1").arg(m_weaponStrategy->getStatusText()));
}

void MainWindow::onWeaponFired(QPointF target, double radius, WeaponType type)
{
    m_radarDisplay->addStrikeEffect(target, radius);
    QString weaponTypeName = (type == WeaponType::Laser) ? "激光" : "导弹";
    m_strikeStatusLabel->setText(QString("%1打击: 位置(%2,%3) 半径%4")
                                     .arg(weaponTypeName)
                                     .arg(target.x(), 0, 'f', 1)
                                     .arg(target.y(), 0, 'f', 1)
                                     .arg(radius, 0, 'f', 1));
    updateWeaponStatus();
}

void MainWindow::onCooldownComplete()
{
    updateWeaponStatus();
}

void MainWindow::updateWeaponStatus()
{
    m_weaponStatusLabel->setText(QString("当前策略: %1").arg(m_weaponStrategy->getStatusText()));
}

void MainWindow::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setHandleWidth(8);

    // --- 左侧：雷达显示 和 信息面板 ---
    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(2, 2, 2, 2);
    leftLayout->setSpacing(0);

    QHBoxLayout* radarAreaLayout = new QHBoxLayout();
    radarAreaLayout->setContentsMargins(0, 0, 0, 0); // 移除边距
    radarAreaLayout->setSpacing(5);
    radarAreaLayout->addWidget(m_radarDisplay, 1);

    // --- 【修改6】: 将原来的 threatSortWidget 扩展为包含日志的 infoPanelWidget ---
    QWidget* infoPanelWidget = new QWidget(); // 原来的 threatSortWidget
    infoPanelWidget->setMaximumWidth(200);
    infoPanelWidget->setMinimumWidth(200);
    // 移除固定高度限制，让布局自然拉伸
    QVBoxLayout* infoPanelLayout = new QVBoxLayout(infoPanelWidget); // 原来的 threatSortLayout
    infoPanelLayout->setSpacing(8);
    infoPanelLayout->setContentsMargins(8, 8, 8, 8);

    QLabel* threatSortTitle = new QLabel("威胁排序");
    threatSortTitle->setObjectName("statusHeaderLabel");
    threatSortTitle->setMinimumHeight(25);
    infoPanelLayout->addWidget(threatSortTitle);

    m_threatListWidget = new QListWidget();
    m_threatListWidget->setObjectName("threatList");
    infoPanelLayout->addWidget(m_threatListWidget, 1); // 拉伸因子1，占上半部分空间

    // 【新增7】: 在威胁排序列表下方，创建并添加实时事件日志
    QLabel* eventLogTitle = new QLabel("实时事件日志");
    eventLogTitle->setObjectName("statusHeaderLabel");
    eventLogTitle->setMinimumHeight(25);
    infoPanelLayout->addWidget(eventLogTitle);

    m_eventLogWidget = new QListWidget();
    m_eventLogWidget->setObjectName("threatList"); // 复用样式
    infoPanelLayout->addWidget(m_eventLogWidget, 1); // 拉伸因子1，占下半部分空间

    radarAreaLayout->addWidget(infoPanelWidget, 0); // 拉伸因子0，固定大小，不影响雷达区域
    leftLayout->addLayout(radarAreaLayout, 1); // 雷达区域占满所有垂直空间

    // --- 右侧：控制面板 (这部分完全不变) ---
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* rightWidget = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(15, 10, 15, 10);
    rightLayout->setSpacing(20);

    // (右侧所有Group Box的创建和布局都和你原来的一样)
    QGroupBox* droneGroup = new QGroupBox("无人机管理");
    droneGroup->setObjectName("controlGroup");
    droneGroup->setMinimumHeight(120);
    QVBoxLayout* droneLayout = new QVBoxLayout(droneGroup);
    droneLayout->setSpacing(15);
    droneLayout->setContentsMargins(15, 20, 15, 15);
    QHBoxLayout* genLayout = new QHBoxLayout();
    genLayout->setSpacing(10);
    QLabel* genLabel = new QLabel("生成间隔(秒):");
    genLabel->setMinimumWidth(100);
    genLayout->addWidget(genLabel);
    m_generationInterval = new QDoubleSpinBox();
    m_generationInterval->setRange(0.1, 60.0); m_generationInterval->setValue(1.0); m_generationInterval->setDecimals(1);
    m_generationInterval->setSingleStep(0.1); m_generationInterval->setMinimumHeight(35); m_generationInterval->setButtonSymbols(QAbstractSpinBox::NoButtons);
    genLayout->addWidget(m_generationInterval);
    genLayout->addStretch();
    m_startStopDroneButton = new QPushButton("停止无人机生成");
    m_startStopDroneButton->setObjectName("primaryButton");
    m_startStopDroneButton->setMinimumHeight(40);
    droneLayout->addLayout(genLayout);
    droneLayout->addWidget(m_startStopDroneButton);

    QGroupBox* radarGroup = new QGroupBox("雷达控制");
    radarGroup->setObjectName("controlGroup");
    radarGroup->setMinimumHeight(150);
    QVBoxLayout* radarLayout = new QVBoxLayout(radarGroup);
    radarLayout->setSpacing(15);
    radarLayout->setContentsMargins(15, 20, 15, 15);
    QHBoxLayout* scanLayout = new QHBoxLayout();
    scanLayout->setSpacing(10);
    QLabel* scanLabel = new QLabel("扫描间隔(秒):");
    scanLabel->setMinimumWidth(100);
    scanLayout->addWidget(scanLabel);
    m_scanInterval = new QDoubleSpinBox();
    m_scanInterval->setRange(0.1, 10.0); m_scanInterval->setValue(0.1); m_scanInterval->setDecimals(1);
    m_scanInterval->setSingleStep(0.1); m_scanInterval->setMinimumHeight(35); m_scanInterval->setButtonSymbols(QAbstractSpinBox::NoButtons);
    scanLayout->addWidget(m_scanInterval);
    scanLayout->addStretch();
    QHBoxLayout* radiusLayout = new QHBoxLayout();
    radiusLayout->setSpacing(10);
    QLabel* radiusLabel = new QLabel("雷达半径:");
    radiusLabel->setMinimumWidth(100);
    radiusLayout->addWidget(radiusLabel);
    m_radarRadius = new QDoubleSpinBox();
    m_radarRadius->setRange(100, 2000); m_radarRadius->setValue(800); m_radarRadius->setMinimumHeight(35);
    m_radarRadius->setButtonSymbols(QAbstractSpinBox::NoButtons);
    radiusLayout->addWidget(m_radarRadius);
    radiusLayout->addStretch();
    m_startStopRadarButton = new QPushButton("停止雷达");
    m_startStopRadarButton->setObjectName("primaryButton");
    m_startStopRadarButton->setMinimumHeight(40);
    radarLayout->addLayout(scanLayout);
    radarLayout->addLayout(radiusLayout);
    radarLayout->addWidget(m_startStopRadarButton);

    QVBoxLayout* basicControlLayout = new QVBoxLayout();
    basicControlLayout->addWidget(droneGroup);
    basicControlLayout->addWidget(radarGroup);

    QVBoxLayout* threatPanelLayout = new QVBoxLayout();

    QGroupBox* strikeGroup = new QGroupBox("智能武器系统");
    strikeGroup->setObjectName("controlGroup");
    strikeGroup->setMinimumHeight(280);
    QVBoxLayout* strikeLayout = new QVBoxLayout(strikeGroup);
    strikeLayout->setSpacing(15);
    strikeLayout->setContentsMargins(15, 20, 15, 15);
    m_weaponStatusLabel = new QLabel("当前策略: 激光单体打击 - 就绪");
    m_weaponStatusLabel->setObjectName("statusLabel");
    m_weaponStatusLabel->setWordWrap(true);
    m_weaponStatusLabel->setMinimumHeight(35);
    QGridLayout* strategyButtonLayout = new QGridLayout();
    strategyButtonLayout->setSpacing(8);
    strategyButtonLayout->setVerticalSpacing(8);
    m_laserThreatButton = new QPushButton("激光单体打击");
    m_laserThreatButton->setObjectName("primaryButton");
    m_laserThreatButton->setMinimumHeight(45);
    m_missileThreatButton = new QPushButton("导弹范围打击");
    m_missileThreatButton->setObjectName("warningButton");
    m_missileThreatButton->setMinimumHeight(45);
    strategyButtonLayout->addWidget(m_laserThreatButton, 0, 0);
    strategyButtonLayout->addWidget(m_missileThreatButton, 0, 1);
    QHBoxLayout* autoFireLayout = new QHBoxLayout();
    autoFireLayout->setSpacing(10);
    m_autoFireButton = new QPushButton("开始自动开火");
    m_autoFireButton->setObjectName("successButton");
    m_autoFireButton->setMinimumHeight(40);
    m_autoFireButton->setCheckable(true);
    autoFireLayout->addWidget(m_autoFireButton);
    autoFireLayout->addStretch();
    QHBoxLayout* interactiveStrikeLayout = new QHBoxLayout();
    interactiveStrikeLayout->setSpacing(10);
    m_strikeModeToggle = new QCheckBox("交互打击模式");
    m_strikeModeToggle->setObjectName("strikeModeToggle");
    m_strikeModeToggle->setMinimumHeight(30);
    interactiveStrikeLayout->addWidget(m_strikeModeToggle);
    interactiveStrikeLayout->addStretch();
    QVBoxLayout* strikeStatusLayout = new QVBoxLayout();
    strikeStatusLayout->setSpacing(8);
    m_strikeStatusLabel = new QLabel("打击状态: 待命");
    m_strikeStatusLabel->setObjectName("statusLabel");
    m_strikeStatusLabel->setMinimumHeight(30);
    strikeStatusLayout->addWidget(m_strikeStatusLabel);
    strikeLayout->addWidget(m_weaponStatusLabel);
    strikeLayout->addLayout(strategyButtonLayout);
    strikeLayout->addLayout(autoFireLayout);
    strikeLayout->addLayout(interactiveStrikeLayout);
    strikeLayout->addLayout(strikeStatusLayout);
    threatPanelLayout->addWidget(strikeGroup);

    QGroupBox* statisticsGroup = new QGroupBox("统计分析");
    statisticsGroup->setObjectName("controlGroup");
    statisticsGroup->setMinimumHeight(130);
    QVBoxLayout* statisticsLayout = new QVBoxLayout(statisticsGroup);
    statisticsLayout->setSpacing(12);
    statisticsLayout->setContentsMargins(15, 20, 15, 15);
    m_defenseEfficiencyLabel = new QLabel("防御效率: 0%");
    m_defenseEfficiencyLabel->setObjectName("statusLabel");
    m_defenseEfficiencyLabel->setMinimumHeight(25);
    m_totalEventsLabel = new QLabel("总事件数: 0");
    m_totalEventsLabel->setObjectName("statusLabel");
    m_totalEventsLabel->setMinimumHeight(25);
    QHBoxLayout* reportButtonLayout = new QHBoxLayout();
    reportButtonLayout->setSpacing(8);
    QPushButton* generateReportButton = new QPushButton("生成报告");
    generateReportButton->setObjectName("secondaryButton");
    generateReportButton->setMinimumHeight(35);
    QPushButton* exportDataButton = new QPushButton("导出数据");
    exportDataButton->setObjectName("warningButton");
    exportDataButton->setMinimumHeight(35);
    reportButtonLayout->addWidget(generateReportButton);
    reportButtonLayout->addWidget(exportDataButton);
    statisticsLayout->addWidget(m_defenseEfficiencyLabel);
    statisticsLayout->addWidget(m_totalEventsLabel);
    statisticsLayout->addLayout(reportButtonLayout);
    threatPanelLayout->addWidget(statisticsGroup);

    rightLayout->addLayout(basicControlLayout);
    rightLayout->addLayout(threatPanelLayout);
    rightLayout->addStretch();

    scrollArea->setWidget(rightWidget);
    scrollArea->setMinimumWidth(400);

    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(scrollArea);
    mainSplitter->setStretchFactor(0, 6);
    mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(mainSplitter, 1);
}

void MainWindow::setupStyles()
{
    QString styleSheet = R"(
        QWidget {
            background-color: #0a1219;
            color: #00bfff;
            font-family: "Segoe UI", "Microsoft YaHei", "Arial", sans-serif;
            font-size: 13px;
            line-height: 1.4;
        }
        #titleLabel {
            font-size: 26px;
            font-weight: bold;
            color: #00ffff;
            margin: 5px;
            padding: 20px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 rgba(0, 191, 255, 0.15), stop:0.5 rgba(0, 255, 255, 0.2), stop:1 rgba(0, 191, 255, 0.15));
            border: 2px solid #00bfff;
            border-radius: 12px;
            text-align: center;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #1e3a5f;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(30, 58, 95, 0.3), stop:1 rgba(10, 18, 25, 0.8));
        }
        QGroupBox::title {
            color: #00ffff;
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
            background-color: #0a1219;
        }
        #controlGroup {
            border-color: #2196f3;
        }
        #statusGroup {
            border-color: #4caf50;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(76, 175, 80, 0.2), stop:1 rgba(10, 18, 25, 0.8));
        }
        
        /* 专业按钮样式 - 现代化设计 */
        QPushButton {
            border: 1px solid #2c3e50;
            border-radius: 6px;
            padding: 12px 20px;
            font-weight: 600;
            font-size: 13px;
            min-width: 120px;
            min-height: 40px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #34495e, stop:1 #2c3e50);
            color: #ecf0f1;
            text-align: center;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3498db, stop:1 #2980b9);
            border-color: #3498db;
            color: #ffffff;
            transform: translateY(-1px);
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2980b9, stop:1 #1f4e79);
            border-color: #2980b9;
            padding: 13px 20px 11px 20px;
        }
        
        /* 主要操作按钮 - 绿色系 */
        #primaryButton {
            border: 1px solid #27ae60;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2ecc71, stop:1 #27ae60);
            color: #ffffff;
            font-weight: 700;
        }
        #primaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #58d68d, stop:1 #2ecc71);
            border-color: #2ecc71;
            box-shadow: 0 4px 8px rgba(46, 204, 113, 0.3);
        }
        
        /* 次要操作按钮 - 蓝色系 */
        #secondaryButton {
            border: 1px solid #3498db;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5dade2, stop:1 #3498db);
            color: #ffffff;
            font-weight: 600;
        }
        #secondaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #85c1e9, stop:1 #5dade2);
            border-color: #5dade2;
        }
        
        /* 警告按钮 - 橙色系 */
        #warningButton {
            border: 1px solid #e67e22;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #f39c12, stop:1 #e67e22);
            color: #ffffff;
            font-weight: 700;
        }
        #warningButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #f7dc6f, stop:1 #f39c12);
            border-color: #f39c12;
            box-shadow: 0 4px 8px rgba(243, 156, 18, 0.3);
        }
        
        /* 危险按钮 - 红色系 */
        #dangerButton {
            border: 1px solid #c0392b;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #e74c3c, stop:1 #c0392b);
            color: #ffffff;
            font-size: 14px;
            font-weight: 700;
        }
        #dangerButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #ec7063, stop:1 #e74c3c);
            border-color: #e74c3c;
            box-shadow: 0 4px 8px rgba(231, 76, 60, 0.3);
        }
        
        /* 成功按钮 - 深绿色系 */
        #successButton {
            border: 1px solid #1e8449;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #28b463, stop:1 #1e8449);
            color: #ffffff;
            font-size: 14px;
            font-weight: 700;
        }
        #successButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #52c882, stop:1 #28b463);
            border-color: #28b463;
            box-shadow: 0 4px 8px rgba(40, 180, 99, 0.3);
        }
        QSpinBox, QDoubleSpinBox {
            border: 2px solid #1e88e5;
            border-radius: 6px;
            padding: 8px 12px;
            background: rgba(30, 136, 229, 0.15);
            selection-background-color: #42a5f5;
            min-width: 100px;
            min-height: 32px;
            font-size: 13px;
        }
        QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #00ffff;
            background: rgba(0, 255, 255, 0.15);
        }
        QSpinBox:hover, QDoubleSpinBox:hover {
            border-color: #42a5f5;
            background: rgba(30, 136, 229, 0.2);
        }
        QLabel {
            color: #b3e5fc;
            font-size: 13px;
        }
        QCheckBox {
            color: #00bfff;
            font-size: 13px;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 2px solid #1e88e5;
            border-radius: 4px;
            background: rgba(30, 136, 229, 0.1);
        }
        QCheckBox::indicator:hover {
            border-color: #42a5f5;
            background: rgba(30, 136, 229, 0.2);
        }
        QCheckBox::indicator:checked {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(76, 175, 80, 0.8), stop:1 rgba(27, 94, 32, 0.8));
            border-color: #4caf50;
        }
        QCheckBox::indicator:checked:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(76, 175, 80, 1.0), stop:1 rgba(27, 94, 32, 1.0));
        }
        #systemStatus {
            font-size: 14px;
            font-weight: bold;
            color: #4caf50;
        }
        #statusLabel {
            font-size: 13px;
            color: #81c784;
        }
        #sectionTitle {
            font-size: 16px;
            font-weight: bold;
            color: #00ffff;
            margin: 5px;
        }
        QSplitter::handle {
            background-color: #1e88e5;
            width: 2px;
            height: 2px;
        }
        QSplitter::handle:hover {
            background-color: #42a5f5;
        }
        QDoubleSpinBox::up-button, QSpinBox::up-button {
            background-color: rgba(30, 136, 229, 0.3);
            border: 1px solid #1e88e5;
        }
        QDoubleSpinBox::down-button, QSpinBox::down-button {
            background-color: rgba(30, 136, 229, 0.3);
            border: 1px solid #1e88e5;
        }
        #threatLabel {
            font-size: 11px;
            color: #ffab40;
            background: rgba(255, 171, 64, 0.1);
            border: 1px solid rgba(255, 171, 64, 0.3);
            border-radius: 4px;
            padding: 8px;
            margin: 4px;
        }
        #strikeModeToggle {
            color: #ff9800;
            font-weight: bold;
            spacing: 10px;
        }
        #strikeModeToggle::indicator {
            width: 18px;
            height: 18px;
            border: 2px solid #ff9800;
            border-radius: 4px;
            background: rgba(255, 152, 0, 0.1);
        }
        #strikeModeToggle::indicator:checked {
            background: rgba(255, 152, 0, 0.6);
            border-color: #ffb74d;
        }
        #strikeModeToggle::indicator:hover {
            border-color: #ffcc02;
            background: rgba(255, 152, 0, 0.3);
        }
        QScrollArea {
            border: none;
            background: transparent;
        }
        QScrollBar:vertical {
            background: rgba(30, 58, 95, 0.3);
            width: 12px;
            border-radius: 6px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: rgba(0, 191, 255, 0.5);
            border-radius: 6px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(0, 191, 255, 0.7);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
        #statusHeaderLabel {
            font-size: 14px;
            font-weight: bold;
            color: #00ffff;
            background: rgba(0, 191, 255, 0.1);
            border: 1px solid rgba(0, 191, 255, 0.3);
            border-radius: 4px;
            padding: 5px;
            margin: 2px;
        }
        #threatList {
            background: rgba(30, 58, 95, 0.2);
            border: 1px solid #1e3a5f;
            border-radius: 6px;
            font-size: 12px;
            alternate-background-color: rgba(0, 191, 255, 0.05);
            selection-background-color: rgba(0, 191, 255, 0.3);
            padding: 2px;
        }
        #threatList::item, #eventLogWidget::item {
            padding: 4px;
            margin: 1px;
            border-radius: 3px;
        }
        #threatList::item:hover, #eventLogWidget::item:hover {
            background: rgba(0, 191, 255, 0.2);
        }
        #threatList::item:selected, #eventLogWidget::item:selected {
            background: rgba(0, 191, 255, 0.4);
        }
    )";

    setStyleSheet(styleSheet);
}

// 【新增12】: 添加一个集中的函数来管理所有信号槽连接
void MainWindow::setupConnections()
{
    // 你原来的信号连接
    connect(m_droneManager, &DroneManager::droneAdded, this, &MainWindow::updateDroneCount);
    connect(m_droneManager, &DroneManager::droneRemoved, this, &MainWindow::updateDroneCount);
    connect(m_radarSimulator, &RadarSimulator::radarScanCompleted, this, &MainWindow::onRadarScanCompleted);
    connect(m_radarDisplay, &RadarDisplay::connectionStatusChanged, this, [this](bool connected) {
        if (connected) {
            m_radarSimulator->addClient(QHostAddress::LocalHost, 12346);
        }
    });
    connect(m_startStopDroneButton, &QPushButton::clicked, this, &MainWindow::onStartStopDroneManager);
    connect(m_startStopRadarButton, &QPushButton::clicked, this, &MainWindow::onStartStopRadar);
    connect(m_generationInterval, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onGenerationIntervalChanged);
    connect(m_droneManager, &DroneManager::highPriorityThreatDetected, this, &MainWindow::onHighPriorityThreatDetected);
    connect(m_droneManager, &DroneManager::strikeExecuted, this, &MainWindow::onStrikeExecutedForStats);
    connect(m_statisticsManager, &StatisticsManager::statisticsUpdated, this, &MainWindow::onStatisticsUpdated);
    connect(m_radarDisplay, &RadarDisplay::strikeRequested, this, &MainWindow::onStrikeRequested);
    connect(m_radarDisplay, &RadarDisplay::droneClicked, this, &MainWindow::onDroneClicked);
    connect(m_strikeModeToggle, &QCheckBox::toggled, this, [this](bool enabled) {
        m_radarDisplay->setStrikeMode(enabled);
        m_radarDisplay->setStrikeRadius(120.0);
        onStrikeModeToggled(enabled);
    });
    connect(m_laserThreatButton, &QPushButton::clicked, this, [this]() {
        m_weaponStrategy->setCurrentStrategy(WeaponType::Laser, TargetingStrategy::ThreatPriority);
        onStrategicStrike();
    });
    connect(m_missileThreatButton, &QPushButton::clicked, this, [this]() {
        m_weaponStrategy->setCurrentStrategy(WeaponType::Missile, TargetingStrategy::ThreatPriority);
        onStrategicStrike();
    });
    connect(m_autoFireButton, &QPushButton::toggled, this, &MainWindow::onAutoFireToggled);
    connect(m_weaponStrategy, &WeaponStrategy::strategyChanged, this, &MainWindow::onStrategyChanged);
    connect(m_weaponStrategy, &WeaponStrategy::weaponFired, this, &MainWindow::onWeaponFired);
    connect(m_weaponStrategy, &WeaponStrategy::cooldownComplete, this, &MainWindow::onCooldownComplete);
    connect(m_droneManager, &DroneManager::droneAdded, this, &MainWindow::onDroneAddedForStats);
    connect(m_droneManager, &DroneManager::droneDestroyed, this, &MainWindow::onDroneDestroyedForStats);

    // 【新增13】: 添加对无人机逃逸信号的连接，这是日志功能的核心之一
    connect(m_droneManager, &DroneManager::droneEscaped, this, &MainWindow::onDroneEscapedForStats);
}

// 【新增14】: 添加这个全新的辅助函数，用于向日志列表添加信息
void MainWindow::addLogMessage(const QString& message, const QColor& color)
{
    if (!m_eventLogWidget) return;

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QListWidgetItem* item = new QListWidgetItem(QString("%1").arg(message));
    item->setForeground(color);

    m_eventLogWidget->insertItem(0, item);

    if (m_eventLogWidget->count() > 100) { // 限制日志数量
        delete m_eventLogWidget->takeItem(m_eventLogWidget->count() - 1);
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
