#include "RadarConfig.h"
#include <QApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHostAddress>
#include <QNetworkDatagram>

RadarConfig::RadarConfig(QWidget *parent)
    : QWidget(parent)
    , m_udpSocket(nullptr)
    , m_radarHost("127.0.0.1")
    , m_radarPort(12345)
    , m_configPort(12347)
    , m_isConnected(false)
    , m_heartbeatTimer(new QTimer(this))
{
    setupUI();
    setupStyles();
    
    // 初始化UDP套接字
    m_udpSocket = new QUdpSocket(this);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &RadarConfig::onConfigResponse);
    
    // 心跳定时器
    connect(m_heartbeatTimer, &QTimer::timeout, this, &RadarConfig::requestCurrentSettings);
    
    setWindowTitle("雷达配置程序");
    resize(500, 700);
}

RadarConfig::~RadarConfig()
{
    if (m_isConnected) {
        disconnectFromRadar();
    }
}

void RadarConfig::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 标题
    QLabel* titleLabel = new QLabel("雷达仿真器配置程序");
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // 连接设置
    QGroupBox* connectionGroup = new QGroupBox("连接设置");
    connectionGroup->setObjectName("controlGroup");
    QVBoxLayout* connectionLayout = new QVBoxLayout(connectionGroup);
    
    QHBoxLayout* hostLayout = new QHBoxLayout();
    hostLayout->addWidget(new QLabel("雷达主机:"));
    m_hostEdit = new QLineEdit("127.0.0.1");
    hostLayout->addWidget(m_hostEdit);
    
    QHBoxLayout* portLayout = new QHBoxLayout();
    portLayout->addWidget(new QLabel("雷达端口:"));
    m_portEdit = new QSpinBox();
    m_portEdit->setRange(1000, 65535);
    m_portEdit->setValue(12345);
    portLayout->addWidget(m_portEdit);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton("连接雷达");
    m_connectButton->setObjectName("primaryButton");
    m_disconnectButton = new QPushButton("断开连接");
    m_disconnectButton->setObjectName("secondaryButton");
    m_disconnectButton->setEnabled(false);
    
    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    
    m_statusLabel = new QLabel("状态: 未连接");
    m_statusLabel->setObjectName("statusLabel");
    
    connectionLayout->addLayout(hostLayout);
    connectionLayout->addLayout(portLayout);
    connectionLayout->addLayout(buttonLayout);
    connectionLayout->addWidget(m_statusLabel);
    
    // 无人机生成配置
    QGroupBox* droneGroup = new QGroupBox("无人机生成配置");
    droneGroup->setObjectName("controlGroup");
    QVBoxLayout* droneLayout = new QVBoxLayout(droneGroup);
    
    QHBoxLayout* genIntervalLayout = new QHBoxLayout();
    genIntervalLayout->addWidget(new QLabel("生成间隔(秒):"));
    m_generationInterval = new QSpinBox();
    m_generationInterval->setRange(1, 60);
    m_generationInterval->setValue(3);
    genIntervalLayout->addWidget(m_generationInterval);
    
    QHBoxLayout* maxDronesLayout = new QHBoxLayout();
    maxDronesLayout->addWidget(new QLabel("最大无人机数:"));
    m_maxDrones = new QSpinBox();
    m_maxDrones->setRange(1, 50);
    m_maxDrones->setValue(10);
    maxDronesLayout->addWidget(m_maxDrones);
    
    QHBoxLayout* speedRangeLayout1 = new QHBoxLayout();
    speedRangeLayout1->addWidget(new QLabel("最小速度:"));
    m_minSpeed = new QDoubleSpinBox();
    m_minSpeed->setRange(5.0, 100.0);
    m_minSpeed->setValue(10.0);
    m_minSpeed->setSuffix(" px/s");
    speedRangeLayout1->addWidget(m_minSpeed);
    
    QHBoxLayout* speedRangeLayout2 = new QHBoxLayout();
    speedRangeLayout2->addWidget(new QLabel("最大速度:"));
    m_maxSpeed = new QDoubleSpinBox();
    m_maxSpeed->setRange(10.0, 200.0);
    m_maxSpeed->setValue(50.0);
    m_maxSpeed->setSuffix(" px/s");
    speedRangeLayout2->addWidget(m_maxSpeed);
    
    m_applyGenButton = new QPushButton("应用无人机配置");
    m_applyGenButton->setObjectName("primaryButton");
    m_applyGenButton->setEnabled(false);
    
    droneLayout->addLayout(genIntervalLayout);
    droneLayout->addLayout(maxDronesLayout);
    droneLayout->addLayout(speedRangeLayout1);
    droneLayout->addLayout(speedRangeLayout2);
    droneLayout->addWidget(m_applyGenButton);
    
    // 雷达配置
    QGroupBox* radarGroup = new QGroupBox("雷达配置");
    radarGroup->setObjectName("controlGroup");
    QVBoxLayout* radarLayout = new QVBoxLayout(radarGroup);
    
    QHBoxLayout* scanIntervalLayout = new QHBoxLayout();
    scanIntervalLayout->addWidget(new QLabel("扫描间隔(秒):"));
    m_scanInterval = new QSpinBox();
    m_scanInterval->setRange(1, 10);
    m_scanInterval->setValue(1);
    scanIntervalLayout->addWidget(m_scanInterval);
    
    QHBoxLayout* radiusLayout = new QHBoxLayout();
    radiusLayout->addWidget(new QLabel("雷达半径:"));
    m_radarRadius = new QDoubleSpinBox();
    m_radarRadius->setRange(100.0, 2000.0);
    m_radarRadius->setValue(800.0);
    m_radarRadius->setSuffix(" px");
    radiusLayout->addWidget(m_radarRadius);
    
    QHBoxLayout* centerXLayout = new QHBoxLayout();
    centerXLayout->addWidget(new QLabel("雷达中心X:"));
    m_radarCenterX = new QDoubleSpinBox();
    m_radarCenterX->setRange(-1000.0, 1000.0);
    m_radarCenterX->setValue(0.0);
    m_radarCenterX->setSuffix(" px");
    centerXLayout->addWidget(m_radarCenterX);
    
    QHBoxLayout* centerYLayout = new QHBoxLayout();
    centerYLayout->addWidget(new QLabel("雷达中心Y:"));
    m_radarCenterY = new QDoubleSpinBox();
    m_radarCenterY->setRange(-1000.0, 1000.0);
    m_radarCenterY->setValue(0.0);
    m_radarCenterY->setSuffix(" px");
    centerYLayout->addWidget(m_radarCenterY);
    
    m_applyRadarButton = new QPushButton("应用雷达配置");
    m_applyRadarButton->setObjectName("primaryButton");
    m_applyRadarButton->setEnabled(false);
    
    radarLayout->addLayout(scanIntervalLayout);
    radarLayout->addLayout(radiusLayout);
    radarLayout->addLayout(centerXLayout);
    radarLayout->addLayout(centerYLayout);
    radarLayout->addWidget(m_applyRadarButton);
    
    // 操作按钮
    QGroupBox* operationGroup = new QGroupBox("操作");
    operationGroup->setObjectName("controlGroup");
    QVBoxLayout* operationLayout = new QVBoxLayout(operationGroup);
    
    QHBoxLayout* opButtonLayout = new QHBoxLayout();
    m_refreshButton = new QPushButton("刷新当前设置");
    m_refreshButton->setObjectName("secondaryButton");
    m_refreshButton->setEnabled(false);
    
    m_resetButton = new QPushButton("重置为默认值");
    m_resetButton->setObjectName("secondaryButton");
    
    opButtonLayout->addWidget(m_refreshButton);
    opButtonLayout->addWidget(m_resetButton);
    
    // 当前设置显示
    m_currentSettingsLabel = new QLabel("当前设置: 未连接");
    m_currentSettingsLabel->setObjectName("infoLabel");
    m_currentSettingsLabel->setWordWrap(true);
    
    operationLayout->addLayout(opButtonLayout);
    operationLayout->addWidget(m_currentSettingsLabel);
    
    // 添加到主布局
    mainLayout->addWidget(connectionGroup);
    mainLayout->addWidget(droneGroup);
    mainLayout->addWidget(radarGroup);
    mainLayout->addWidget(operationGroup);
    mainLayout->addStretch();
    
    // 连接信号
    connect(m_connectButton, &QPushButton::clicked, this, &RadarConfig::connectToRadar);
    connect(m_disconnectButton, &QPushButton::clicked, this, &RadarConfig::disconnectFromRadar);
    connect(m_applyGenButton, &QPushButton::clicked, this, &RadarConfig::applyGenerationInterval);
    connect(m_applyRadarButton, &QPushButton::clicked, this, &RadarConfig::applyRadarSettings);
    connect(m_refreshButton, &QPushButton::clicked, this, &RadarConfig::requestCurrentSettings);
    connect(m_resetButton, &QPushButton::clicked, this, [this]() {
        m_generationInterval->setValue(3);
        m_scanInterval->setValue(1);
        m_radarRadius->setValue(800.0);
        m_radarCenterX->setValue(0.0);
        m_radarCenterY->setValue(0.0);
        m_maxDrones->setValue(10);
        m_minSpeed->setValue(10.0);
        m_maxSpeed->setValue(50.0);
    });
}

void RadarConfig::setupStyles()
{
    QString styleSheet = R"(
        QWidget {
            background-color: #0a1219;
            color: #00bfff;
            font-family: "Consolas", "Monaco", monospace;
            font-size: 12px;
        }
        
        #titleLabel {
            font-size: 20px;
            font-weight: bold;
            color: #00ffff;
            margin: 10px;
            padding: 10px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 rgba(0, 191, 255, 0.1), stop:1 rgba(0, 255, 255, 0.1));
            border: 2px solid rgba(0, 255, 255, 0.3);
            border-radius: 5px;
        }
        
        #controlGroup {
            background: rgba(0, 191, 255, 0.05);
            border: 2px solid rgba(0, 191, 255, 0.3);
            border-radius: 8px;
            margin: 5px;
            font-weight: bold;
            color: #00ffff;
        }
        
        #primaryButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                stop:0 rgba(0, 255, 0, 0.8), stop:1 rgba(0, 200, 0, 0.6));
            color: white;
            border: 2px solid #00ff00;
            border-radius: 5px;
            padding: 8px 15px;
            font-weight: bold;
            min-height: 25px;
        }
        
        #primaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                stop:0 rgba(0, 255, 0, 0.9), stop:1 rgba(0, 220, 0, 0.7));
        }
        
        #primaryButton:disabled {
            background: rgba(100, 100, 100, 0.3);
            color: rgba(255, 255, 255, 0.5);
            border: 2px solid rgba(100, 100, 100, 0.5);
        }
        
        #secondaryButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                stop:0 rgba(0, 191, 255, 0.8), stop:1 rgba(0, 150, 200, 0.6));
            color: white;
            border: 2px solid #00bfff;
            border-radius: 5px;
            padding: 8px 15px;
            font-weight: bold;
            min-height: 25px;
        }
        
        #secondaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                stop:0 rgba(0, 191, 255, 0.9), stop:1 rgba(0, 170, 220, 0.7));
        }
        
        #secondaryButton:disabled {
            background: rgba(100, 100, 100, 0.3);
            color: rgba(255, 255, 255, 0.5);
            border: 2px solid rgba(100, 100, 100, 0.5);
        }
        
        #statusLabel {
            color: #ffff00;
            font-weight: bold;
            padding: 5px;
            background: rgba(255, 255, 0, 0.1);
            border: 1px solid rgba(255, 255, 0, 0.3);
            border-radius: 3px;
        }
        
        #infoLabel {
            color: #00ff00;
            font-weight: bold;
            padding: 5px;
            background: rgba(0, 255, 0, 0.1);
            border: 1px solid rgba(0, 255, 0, 0.3);
            border-radius: 3px;
            font-size: 11px;
        }
        
        QSpinBox, QDoubleSpinBox, QLineEdit {
            background: rgba(0, 191, 255, 0.1);
            border: 2px solid rgba(0, 191, 255, 0.3);
            border-radius: 4px;
            padding: 5px;
            color: #00bfff;
            min-height: 20px;
        }
        
        QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {
            border: 2px solid rgba(0, 255, 255, 0.8);
        }
        
        QLabel {
            color: #00bfff;
            font-weight: bold;
        }
    )";
    
    setStyleSheet(styleSheet);
}

void RadarConfig::connectToRadar()
{
    m_radarHost = m_hostEdit->text();
    m_radarPort = m_portEdit->value();
    
    if (m_udpSocket->bind(QHostAddress::Any, m_configPort)) {
        updateConnectionStatus(true);
        requestCurrentSettings();
        m_heartbeatTimer->start(5000); // 5秒心跳
        
        QMessageBox::information(this, "连接成功", 
            QString("已连接到雷达仿真器\n主机: %1\n端口: %2").arg(m_radarHost).arg(m_radarPort));
    } else {
        QMessageBox::critical(this, "连接失败", "无法绑定配置端口 " + QString::number(m_configPort));
    }
}

void RadarConfig::disconnectFromRadar()
{
    m_heartbeatTimer->stop();
    m_udpSocket->close();
    updateConnectionStatus(false);
    m_currentSettingsLabel->setText("当前设置: 未连接");
}

void RadarConfig::sendConfigCommand(const QJsonObject& command)
{
    if (!m_isConnected) return;
    
    QJsonDocument doc(command);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    qint64 sent = m_udpSocket->writeDatagram(data, QHostAddress(m_radarHost), m_radarPort);
    
    if (sent == -1) {
        QMessageBox::warning(this, "发送失败", "无法发送配置命令到雷达仿真器");
    }
}

void RadarConfig::applyGenerationInterval()
{
    QJsonObject command;
    command["type"] = "config";
    command["category"] = "drone";
    command["generationInterval"] = m_generationInterval->value() * 1000;
    command["maxDrones"] = m_maxDrones->value();
    command["minSpeed"] = m_minSpeed->value();
    command["maxSpeed"] = m_maxSpeed->value();
    
    sendConfigCommand(command);
}

void RadarConfig::applyRadarSettings()
{
    QJsonObject command;
    command["type"] = "config";
    command["category"] = "radar";
    command["scanInterval"] = m_scanInterval->value() * 1000;
    command["radarRadius"] = m_radarRadius->value();
    command["centerX"] = m_radarCenterX->value();
    command["centerY"] = m_radarCenterY->value();
    
    sendConfigCommand(command);
}

void RadarConfig::requestCurrentSettings()
{
    QJsonObject command;
    command["type"] = "query";
    command["request"] = "current_settings";
    
    sendConfigCommand(command);
}

void RadarConfig::onConfigResponse()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject response = doc.object();
            QString type = response["type"].toString();
            
            if (type == "settings") {
                QString settingsText = QString(
                    "当前雷达设置:\n"
                    "扫描间隔: %1秒\n"
                    "雷达半径: %2px\n"
                    "中心位置: (%3, %4)\n"
                    "无人机生成间隔: %5秒\n"
                    "最大无人机数: %6\n"
                    "速度范围: %7-%8 px/s"
                ).arg(response["scanInterval"].toInt() / 1000)
                 .arg(response["radarRadius"].toDouble())
                 .arg(response["centerX"].toDouble())
                 .arg(response["centerY"].toDouble())
                 .arg(response["generationInterval"].toInt() / 1000)
                 .arg(response["maxDrones"].toInt())
                 .arg(response["minSpeed"].toDouble())
                 .arg(response["maxSpeed"].toDouble());
                
                m_currentSettingsLabel->setText(settingsText);
            } else if (type == "config_result") {
                QString category = response["category"].toString();
                bool success = response["success"].toBool();
                QString message = response["message"].toString();
                
                if (success) {
                    QMessageBox::information(this, "配置成功", 
                        QString("%1配置已更新: %2").arg(category).arg(message));
                } else {
                    QMessageBox::warning(this, "配置失败", 
                        QString("%1配置失败: %2").arg(category).arg(message));
                }
                
                // 刷新设置
                requestCurrentSettings();
            }
        }
    }
}

void RadarConfig::updateConnectionStatus(bool connected)
{
    m_isConnected = connected;
    
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    enableControls(connected);
    
    if (connected) {
        m_statusLabel->setText("状态: 已连接");
        m_statusLabel->setStyleSheet("#statusLabel { color: #00ff00; }");
    } else {
        m_statusLabel->setText("状态: 未连接");
        m_statusLabel->setStyleSheet("#statusLabel { color: #ff0000; }");
    }
}

void RadarConfig::enableControls(bool enabled)
{
    m_applyGenButton->setEnabled(enabled);
    m_applyRadarButton->setEnabled(enabled);
    m_refreshButton->setEnabled(enabled);
}
