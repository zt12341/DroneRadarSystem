#include "RadarDisplay.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QDateTime>
#include <QDataStream>
#include <QtMath>
#include <QFont>
#include <random>

RadarDisplay::RadarDisplay(QWidget *parent)
    : QWidget(parent)
    , m_radarRadius(800.0)
    , m_radarCenter(0, 0)
    , m_scaleFactor(1.0)
    , m_showTrails(true)
    , m_showInfo(true)
    , m_trailLength(15) // 适中的轨迹长度，保持轨迹可见
    , m_totalDronesDetected(0)
    , m_lastDataTime(0)
    , m_showStrikeHighlight(false)
    , m_strikeRadius(0.0)
    , m_strikeMode(false)
    , m_currentStrikeRadius(120.0)
    , m_showMouseCursor(false)
    , m_animationPhase(0)
    , m_hoveredDroneId(-1)
    , m_scanAngle(0)
    , m_radarRunning(true) // 默认雷达运行
    , m_connectionStatus("已连接")
    , m_currentDetections(0)
    , m_totalDetections(0)
    , m_lastUpdate("0 秒前")
    , m_systemStatus("系统运行中")
    , m_droneCount("无人机数量: 0")
    , m_laserTargetId(-1) // 初始化激光锁定目标ID
    , m_radarStatus("雷达状态: 运行中")
{
    setMinimumSize(600, 600);
    setWindowTitle("雷达显示器");
    setMouseTracking(true); // 启用鼠标跟踪

    // 网络连接
    m_udpSocket = new QUdpSocket(this);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &RadarDisplay::handleDataReceived);

    // 定时器
    m_updateTimer = new QTimer(this);
    m_cleanupTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &RadarDisplay::updateDisplay);
    connect(m_cleanupTimer, &QTimer::timeout, this, &RadarDisplay::cleanupOldDrones);

    m_updateTimer->start(50);  // 20fps
    m_cleanupTimer->start(500); // 每0.5秒清理一次过期数据，更频繁清理

    // 新增：独立的扫描线角度更新定时器（不触发重绘）
    m_scanTimer = new QTimer(this);
    connect(m_scanTimer, &QTimer::timeout, this, [this]() {
        if (m_radarRunning) { // 只在雷达运行时更新扫描角度
            m_scanAngle += 2; // 固定频率：2度/帧
            if (m_scanAngle >= 360) m_scanAngle = 0;
        }
        // 注意：这里不调用update()，让正常的更新定时器处理重绘
    });
    m_scanTimer->start(50); // 固定20fps，确保扫描线速度恒定

    // 打击高亮定时器
    m_strikeHighlightTimer = new QTimer(this);
    m_strikeHighlightTimer->setSingleShot(true);
    connect(m_strikeHighlightTimer, &QTimer::timeout, this, [this]() {
        m_showStrikeHighlight = false;
        update();
    });

    // 新增：打击效果定时器
    m_strikeEffectTimer = new QTimer(this);
    connect(m_strikeEffectTimer, &QTimer::timeout, this, [this]() {
        // 更新打击效果动画
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_strikeEffects.begin(); it != m_strikeEffects.end();) {
            qint64 elapsed = currentTime - it->startTime;
            if (elapsed >= it->duration) {
                it = m_strikeEffects.erase(it);
            } else {
                // 更新动画参数
                double progress = double(elapsed) / it->duration;
                it->currentRadius = it->radius * (1.0 + progress * 0.5); // 扩张效果
                it->pulsePhase = (it->pulsePhase + 1) % 360;

                // 颜色渐变效果
                int alpha = qMax(0, int(255 * (1.0 - progress)));
                it->color.setAlpha(alpha);
                ++it;
            }
        }

        if (m_strikeEffects.isEmpty()) {
            m_strikeEffectTimer->stop();
        }
        update();
    });

    // 新增：动画定时器
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, [this]() {
        m_animationPhase = (m_animationPhase + 1) % 360;
        update();
    });
    m_animationTimer->start(100); // 10fps动画

    // 初始化颜色列表
    m_droneColors << QColor(Qt::red) << QColor(Qt::green) << QColor(Qt::blue)
                  << QColor(Qt::yellow) << QColor(Qt::magenta) << QColor(Qt::cyan)
                  << QColor(Qt::white) << QColor(255, 165, 0) << QColor(255, 192, 203)
                  << QColor(128, 0, 128);
}

RadarDisplay::~RadarDisplay()
{
    disconnectFromRadar();
}

void RadarDisplay::connectToRadar(const QString& host, quint16 port)
{
    m_serverAddress = QHostAddress(host);
    m_serverPort = port;

    // UDP客户端绑定到固定端口12346
    quint16 clientPort = 12346;
    if (m_udpSocket->bind(QHostAddress::LocalHost, clientPort)) {
        qDebug() << "UDP client bound successfully to port" << clientPort
                 << "to communicate with" << host << ":" << port;

        emit connectionStatusChanged(true);
    } else {
        qDebug() << "Failed to bind UDP socket to port" << clientPort << ":" << m_udpSocket->errorString();
        emit connectionStatusChanged(false);
    }
}

void RadarDisplay::disconnectFromRadar()
{
    if (m_udpSocket) {
        m_udpSocket->close();
        qDebug() << "UDP socket closed";
    }
}

bool RadarDisplay::isConnected() const
{
    return m_udpSocket && m_udpSocket->state() == QAbstractSocket::BoundState;
}

void RadarDisplay::clearDisplay()
{
    m_drones.clear();
    m_totalDronesDetected = 0;
    update();
}

void RadarDisplay::highlightStrikeArea(QPointF center, double radius)
{
    m_strikeCenter = center;
    m_strikeRadius = radius;
    m_showStrikeHighlight = true;

    // 3秒后自动隐藏高亮
    m_strikeHighlightTimer->start(3000);
    update();
}

// 新增：带动画的打击高亮
void RadarDisplay::highlightStrikeAreaWithAnimation(QPointF center, double radius, int /* duration */)
{
    // 只使用新的动画效果系统
    addStrikeEffect(center, radius);
}

// 新增：添加打击效果
void RadarDisplay::addStrikeEffect(QPointF center, double radius)
{
    StrikeEffect effect;
    effect.center = center;
    effect.radius = radius;
    effect.startTime = QDateTime::currentMSecsSinceEpoch();
    effect.duration = 1200; // 1.2秒动画，更快响应
    effect.currentRadius = radius;
    effect.color = QColor(255, 100, 0, 255); // 橙红色
    effect.pulsePhase = 0;

    m_strikeEffects.append(effect);

    if (!m_strikeEffectTimer->isActive()) {
        m_strikeEffectTimer->start(50); // 恢复原来的频率
    }

    // 立即触发一次绘制以显示即时效果
    update();
}

// 新增：清除打击效果
void RadarDisplay::clearStrikeEffects()
{
    m_strikeEffects.clear();
    m_strikeEffectTimer->stop();
    update();
}

// 新增：设置状态信息
void RadarDisplay::setStatusInfo(const QString& connectionStatus, int currentDetections, 
                                int totalDetections, const QString& lastUpdate,
                                const QString& systemStatus, const QString& droneCount, 
                                const QString& radarStatus)
{
    m_connectionStatus = connectionStatus;
    m_currentDetections = currentDetections;
    m_totalDetections = totalDetections;
    m_lastUpdate = lastUpdate;
    m_systemStatus = systemStatus;
    m_droneCount = droneCount;
    m_radarStatus = radarStatus;
    update(); // 触发重绘
}

// 新增：设置打击模式
void RadarDisplay::setStrikeMode(bool enabled)
{
    m_strikeMode = enabled;
    if (enabled) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
        m_showMouseCursor = false;
    }
    update();
}

void RadarDisplay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 设置背景
    painter.fillRect(rect(), QColor(0, 20, 0));

    // 计算缩放因子
    int minDimension = qMin(width(), height()) - 20;
    m_scaleFactor = minDimension / (2.0 * m_radarRadius);

    // 设置坐标系原点到中心
    painter.translate(width() / 2, height() / 2);

    // 绘制雷达网格
    drawRadarGrid(painter);

    // 调试信息：显示无人机数量
    static int paintCount = 0;
    if ((++paintCount % 50) == 0) { // 每50次绘制输出一次调试信息
        qDebug() << "PAINT EVENT: Drawing" << m_drones.size() << "drones. Scale factor:" << m_scaleFactor;
    }

    // 绘制无人机
    drawDrones(painter);

    // 绘制打击高亮
    if (m_showStrikeHighlight) {
        drawStrikeHighlight(painter);
    }

    // 绘制动画打击效果
    drawStrikeEffects(painter);

    // 绘制鼠标光标（打击模式下）
    if (m_strikeMode && m_showMouseCursor) {
        drawStrikeCursor(painter);
    }

    // 绘制hover效果
    if (m_hoveredDroneId != -1) {
        drawHoverEffect(painter);
    }

    // 重置坐标系绘制信息面板
    painter.resetTransform();

    // 绘制状态信息（左上角）
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);

    int y = 20;
    painter.drawText(10, y, QString("连接状态: %1").arg(m_connectionStatus));
    y += 20;
    painter.drawText(10, y, QString("当前检测: %1 个目标").arg(m_currentDetections));
    y += 20;
    painter.drawText(10, y, QString("累计检测: %1 个目标").arg(m_totalDetections));
    y += 20;
    painter.drawText(10, y, QString("最后更新: %1").arg(m_lastUpdate));
    y += 20;
    painter.drawText(10, y, m_systemStatus);
    y += 20;
    painter.drawText(10, y, m_droneCount);
    y += 20;
    painter.drawText(10, y, m_radarStatus);
    y += 20;
    
    // 添加轨迹系统统计
    int linearCount = 0, curvedCount = 0, constantSpeedCount = 0, variableSpeedCount = 0, newTrajectoryCount = 0;
    for (const DisplayDrone& drone : m_drones) {
        if (drone.useNewTrajectory) {
            newTrajectoryCount++;
            if (drone.trajectoryType == TrajectoryType::Linear) {
                linearCount++;
            } else {
                curvedCount++;
            }
            
            if (drone.speedType == SpeedType::Constant) {
                constantSpeedCount++;
            } else {
                variableSpeedCount++;
            }
        }
    }

    if (m_lastDataTime > 0) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_lastDataTime;
        painter.drawText(10, y, QString("最后更新: %1 秒前").arg(elapsed / 1000.0, 0, 'f', 1));
    }
}

void RadarDisplay::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    update();
}

void RadarDisplay::mousePressEvent(QMouseEvent* event)
{
    QPointF screenPos = event->pos();
    QPointF worldPos = screenToWorld(screenPos);

    qDebug() << "Mouse clicked at screen:" << screenPos << "world:" << worldPos;

    // 如果是打击模式
    if (m_strikeMode && event->button() == Qt::LeftButton) {
        // 检查是否在雷达范围内
        double distanceFromCenter = qSqrt(worldPos.x() * worldPos.x() + worldPos.y() * worldPos.y());
        if (distanceFromCenter <= m_radarRadius) {
            // 发射打击请求信号
            emit strikeRequested(worldPos, m_currentStrikeRadius);

            // 立即显示打击效果
            highlightStrikeAreaWithAnimation(worldPos, m_currentStrikeRadius);

            qDebug() << "Strike requested at:" << worldPos << "radius:" << m_currentStrikeRadius;
        } else {
            qDebug() << "Strike request outside radar range";
        }
        return;
    }

    // 查找点击的无人机
    for (const DisplayDrone& drone : m_drones) {
        QPointF droneScreenPos = worldToScreen(drone.position);
        double distance = QPointF(screenPos - droneScreenPos).manhattanLength();
        if (distance < 20) { // 20像素容忍度
            qDebug() << "Clicked on drone" << drone.id << "at position" << drone.position;
            emit droneClicked(drone.id, drone.position);

            // 高亮选中的无人机
            highlightStrikeAreaWithAnimation(drone.position, 30.0);
            break;
        }
    }
}

// 新增：鼠标移动事件
void RadarDisplay::mouseMoveEvent(QMouseEvent* event)
{
    QPointF screenPos = event->pos();
    QPointF worldPos = screenToWorld(screenPos);
    m_mousePosition = worldPos;

    if (m_strikeMode) {
        // 检查是否在雷达范围内
        double distanceFromCenter = qSqrt(worldPos.x() * worldPos.x() + worldPos.y() * worldPos.y());
        m_showMouseCursor = (distanceFromCenter <= m_radarRadius);
        update();
    } else {
        // 检查是否hover在无人机上
        int previousHovered = m_hoveredDroneId;
        m_hoveredDroneId = -1;

        for (const DisplayDrone& drone : m_drones) {
            QPointF droneScreenPos = worldToScreen(drone.position);
            double distance = QPointF(screenPos - droneScreenPos).manhattanLength();
            if (distance < 25) { // 25像素hover容忍度
                m_hoveredDroneId = drone.id;
                m_hoveredDronePosition = drone.position;
                break;
            }
        }

        if (previousHovered != m_hoveredDroneId) {
            update(); // 只在hover状态改变时重绘
        }
    }
}

// 新增：鼠标离开事件
void RadarDisplay::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    m_showMouseCursor = false;
    m_hoveredDroneId = -1;
    update();
}

void RadarDisplay::handleDataReceived()
{
    qDebug() << "=== UDP DATA RECEIVED ===";
    qDebug() << "Pending datagrams:" << m_udpSocket->pendingDatagramSize();

    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        qint64 bytesRead = m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        qDebug() << "Received" << bytesRead << "bytes from" << sender.toString() << ":" << senderPort;

        // 处理接收到的数据
        QDataStream stream(datagram);
        stream.setVersion(QDataStream::Qt_5_15);

        quint32 magic, version;
        stream >> magic >> version;

        qDebug() << "Magic:" << QString("0x%1").arg(magic, 8, 16, QChar('0'))
                 << "Version:" << version;

        if (magic != 0x52444152) { // "RDAR"
            qWarning() << "Invalid magic number, discarding datagram";
            continue;
        }

        if (version != 1) {
            qWarning() << "Unsupported version:" << version;
            continue;
        }

        qint64 timestamp;
        quint32 droneCount;
        stream >> timestamp >> droneCount;

        qDebug() << "Timestamp:" << timestamp << "DroneCount:" << droneCount;

        // 解析无人机数据
        QList<RadarDetection> detections;
        for (quint32 i = 0; i < droneCount; ++i) {
            RadarDetection detection;
            stream >> detection.droneId >> detection.position >> detection.velocity
                   >> detection.detectionTime >> detection.distance >> detection.azimuth;
            
            // 读取轨迹系统信息
            quint32 trajectoryTypeInt, speedTypeInt;
            stream >> trajectoryTypeInt >> speedTypeInt >> detection.currentDirection >> detection.currentSpeed >> detection.useNewTrajectory;
            detection.trajectoryType = static_cast<TrajectoryType>(trajectoryTypeInt);
            detection.speedType = static_cast<SpeedType>(speedTypeInt);
            
            detections.append(detection);

            qDebug() << "Parsed drone" << detection.droneId
                     << "at" << detection.position
                     << "distance" << detection.distance;
        }

        qDebug() << "*** Processing" << detections.size() << "detections ***";
        // 处理接收到的数据
        processRadarData(detections);
    }
}

void RadarDisplay::processRadarData(const QList<RadarDetection>& detections)
{
    qDebug() << "=== PROCESSING RADAR DATA ===";
    qDebug() << "Input detections:" << detections.size();
    qDebug() << "Current drones in display:" << m_drones.size();

    // 更新无人机数据
    for (const RadarDetection& detection : detections) {
        bool found = false;
        for (DisplayDrone& drone : m_drones) {
            if (drone.id == detection.droneId) {
                // 更新现有无人机
                QPointF oldPosition = drone.position;
                drone.position = detection.position;
                 drone.velocity = detection.velocity;
                if (m_showTrails && oldPosition != detection.position) {
                    // 只在位置有足够变化时才添加轨迹点
                    bool shouldAddTrail = false;
                    if (drone.trail.isEmpty()) {
                        shouldAddTrail = true;
                        drone.trail.append(oldPosition); // 添加起始位置
                    } else {
                        QPointF lastPos = drone.trail.last();
                        double distance = qSqrt(qPow(oldPosition.x() - lastPos.x(), 2) +
                                              qPow(oldPosition.y() - lastPos.y(), 2));
                        shouldAddTrail = distance > 5.0; // 降低移动距离阈值，让轨迹更容易添加
                    }

                    if (shouldAddTrail) {
                        drone.trail.append(oldPosition);
                        if (drone.trail.size() > m_trailLength) {
                            drone.trail.removeFirst();
                        }
                    }
                }
                drone.velocity = detection.velocity;
                drone.lastUpdateTime = detection.detectionTime;
                drone.distance = detection.distance;
                drone.azimuth = detection.azimuth;
                
                // 根据威胁值更新颜色（将像素坐标转换为实际距离）
                double pixelDistance = qSqrt(detection.position.x() * detection.position.x() + 
                                           detection.position.y() * detection.position.y());
                double distanceToCenter = pixelDistance * m_scaleFactor; // 转换为实际距离（米）
                double threatScore = 1000.0 / qMax(1.0, distanceToCenter);
                drone.color = getThreatBasedColor(threatScore);
                
                // 更新轨迹系统信息
                drone.trajectoryType = detection.trajectoryType;
                drone.speedType = detection.speedType;
                drone.currentDirection = detection.currentDirection;
                drone.currentSpeed = detection.currentSpeed;
                drone.useNewTrajectory = detection.useNewTrajectory;
                
                found = true;
                qDebug() << "Updated existing drone" << drone.id << "at" << drone.position;
                break;
            }
        }

        if (!found) {
            // 新的无人机
            DisplayDrone drone;
            drone.id = detection.droneId;
            drone.position = detection.position;
            drone.velocity = detection.velocity;
            drone.lastUpdateTime = detection.detectionTime;
            drone.distance = detection.distance;
            drone.azimuth = detection.azimuth;
            
            // 根据威胁值设置颜色（将像素坐标转换为实际距离）
            double pixelDistance = qSqrt(detection.position.x() * detection.position.x() + 
                                       detection.position.y() * detection.position.y());
            double distanceToCenter = pixelDistance * m_scaleFactor; // 转换为实际距离（米）
            double threatScore = 1000.0 / qMax(1.0, distanceToCenter);
            drone.color = getThreatBasedColor(threatScore);
            
            // 设置轨迹系统信息
            drone.trajectoryType = detection.trajectoryType;
            drone.speedType = detection.speedType;
            drone.currentDirection = detection.currentDirection;
            drone.currentSpeed = detection.currentSpeed;
            drone.useNewTrajectory = detection.useNewTrajectory;

            // 为新无人机初始化轨迹
            if (m_showTrails) {
                drone.trail.append(detection.position);
            }

            m_drones.append(drone);
            m_totalDronesDetected++;
            qDebug() << "Added NEW drone" << drone.id << "at" << drone.position << "color" << drone.color.name();
        }
    }

    qDebug() << "Total drones after processing:" << m_drones.size();

    m_lastDataTime = QDateTime::currentMSecsSinceEpoch();
    emit droneDataReceived(detections.size());

    // 强制重绘显示
    update();
    qDebug() << "=== RADAR DATA PROCESSING COMPLETE ===";
}

void RadarDisplay::updateDisplay()
{
    update();
}

void RadarDisplay::cleanupOldDrones()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    const qint64 timeoutMs = 2000; // 2秒超时，给目标足够的显示时间

    for (int i = m_drones.size() - 1; i >= 0; --i) {
        if (currentTime - m_drones[i].lastUpdateTime > timeoutMs) {
            qDebug() << "Removing expired drone" << m_drones[i].id;
            // 清除轨迹
            m_drones[i].trail.clear();
            m_drones.removeAt(i);
        } else {
            // 即使无人机还在，也要让轨迹逐渐缩短
            DisplayDrone& drone = m_drones[i];
            if (!drone.trail.isEmpty() && drone.trail.size() > 8) {
                // 只有当轨迹很长时才偶尔清理
                static std::random_device rd;
                static std::mt19937 gen(rd());
                static std::uniform_int_distribution<> dis(0, 9);
                if (dis(gen) == 0) { // 10%的概率移除最老的轨迹点
                    drone.trail.removeFirst();
                }
            }
        }
    }
}

void RadarDisplay::parseRadarData(const QByteArray& data)
{
    Q_UNUSED(data)
    // 数据已在 handleDataReceived 中解析
}

QPointF RadarDisplay::worldToScreen(QPointF worldPos)
{
    return QPointF(worldPos.x() * m_scaleFactor, worldPos.y() * m_scaleFactor);
}

QPointF RadarDisplay::screenToWorld(QPointF screenPos)
{
    // 转换为以中心为原点的坐标
    QPointF centeredPos = screenPos - QPointF(width() / 2, height() / 2);
    return QPointF(centeredPos.x() / m_scaleFactor, centeredPos.y() / m_scaleFactor);
}

void RadarDisplay::drawRadarGrid(QPainter& painter)
{
    double screenRadius = m_radarRadius * m_scaleFactor;
    
    // 绘制背景渐变圆形
    QRadialGradient backgroundGradient(0, 0, screenRadius);
    backgroundGradient.setColorAt(0, QColor(0, 20, 40, 30));    // 中心稍亮
    backgroundGradient.setColorAt(0.7, QColor(0, 15, 30, 20));  // 中间
    backgroundGradient.setColorAt(1, QColor(0, 10, 20, 10));    // 边缘最暗
    painter.setBrush(QBrush(backgroundGradient));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), screenRadius, screenRadius);
    
    // 绘制同心圆网格 - 更精细的网格
    painter.setPen(QPen(QColor(0, 255, 100, 80), 1.5)); // 半透明绿色
    for (int i = 1; i <= 8; ++i) {
        double r = screenRadius * i / 8.0;
        painter.drawEllipse(QPointF(0, 0), r, r);
    }
    
    // 绘制主要距离圈（更粗的线）
    painter.setPen(QPen(QColor(0, 255, 100, 150), 2.5));
    for (int i = 1; i <= 4; ++i) {
        double r = screenRadius * i / 4.0;
        painter.drawEllipse(QPointF(0, 0), r, r);
        
        // 绘制距离标签 - 使用更专业的字体
        painter.setPen(QColor(0, 255, 255, 200)); // 青色标签
        QFont labelFont = painter.font();
        labelFont.setPointSize(12);
        labelFont.setBold(true);
        painter.setFont(labelFont);
        
        // 在多个位置显示距离标签，避免重叠
        QList<double> labelAngles = {45.0, 135.0, 225.0, 315.0};
        for (double labelAngle : labelAngles) {
            double labelRad = qDegreesToRadians(labelAngle);
            QPointF labelPos(r * qCos(labelRad), -r * qSin(labelRad));
            
            // 计算文本边界，调整位置
            QFontMetrics fm(labelFont);
            QString distanceText = QString("%1km").arg(m_radarRadius * i / 4000.0, 0, 'f', 1);
            QRect textRect = fm.boundingRect(distanceText);
            
            // 根据角度调整标签位置，使其更靠近圆圈
            QPointF adjustedLabelPos = labelPos;
            if (labelAngle == 45.0) {
                adjustedLabelPos.setX(labelPos.x() - textRect.width() / 2);
                adjustedLabelPos.setY(labelPos.y() - 5);
            } else if (labelAngle == 135.0) {
                adjustedLabelPos.setX(labelPos.x() - textRect.width());
                adjustedLabelPos.setY(labelPos.y() - 5);
            } else if (labelAngle == 225.0) {
                adjustedLabelPos.setX(labelPos.x() - textRect.width());
                adjustedLabelPos.setY(labelPos.y() + textRect.height() + 5);
            } else if (labelAngle == 315.0) {
                adjustedLabelPos.setX(labelPos.x() - textRect.width() / 2);
                adjustedLabelPos.setY(labelPos.y() + textRect.height() + 5);
            }
            
            painter.drawText(adjustedLabelPos, distanceText);
        }
        painter.setPen(QPen(QColor(0, 255, 100, 150), 2.5));
    }
    
    // 绘制方位线 - 更精细的网格
    // 主要方位线（每30度）
    painter.setPen(QPen(QColor(0, 255, 100, 120), 2));
    for (int angle = 0; angle < 360; angle += 30) {
        double rad = qDegreesToRadians(double(angle));
        QPointF end(screenRadius * qSin(rad), -screenRadius * qCos(rad));
        painter.drawLine(QPointF(0, 0), end);
        
        // 绘制主要角度标签 - 放在方位线外侧一点点
        QPointF labelPos = end * 1.05; // 放在方位线外侧，但不会太远
        painter.setPen(QColor(0, 255, 255, 200)); // 稍微提高透明度
        QFont angleFont = painter.font();
        angleFont.setPointSize(10); // 稍微减小字体
        angleFont.setBold(true);
        painter.setFont(angleFont);
        
        // 计算文本边界，确保标签不会超出雷达范围
        QFontMetrics fm(angleFont);
        QString angleText = QString("%1°").arg(angle);
        QRect textRect = fm.boundingRect(angleText);
        
        // 调整标签位置，考虑文本大小，确保不超出雷达圆
        QPointF adjustedLabelPos = labelPos;
        
        // 检查标签是否会超出雷达圆边界
        double maxRadius = screenRadius * 1.1; // 允许超出雷达圆10%
        double labelDistance = qSqrt(adjustedLabelPos.x() * adjustedLabelPos.x() + adjustedLabelPos.y() * adjustedLabelPos.y());
        
        if (angle == 0 || angle == 180) {
            // 南北方向：水平居中
            adjustedLabelPos.setX(labelPos.x() - textRect.width() / 2);
            // 检查是否超出边界
            if (qAbs(adjustedLabelPos.x()) + textRect.width() / 2 > maxRadius) {
                adjustedLabelPos.setX((adjustedLabelPos.x() > 0) ? maxRadius - textRect.width() / 2 : -maxRadius + textRect.width() / 2);
            }
        } else if (angle == 90 || angle == 270) {
            // 东西方向：垂直居中
            adjustedLabelPos.setY(labelPos.y() + textRect.height() / 2);
            // 检查是否超出边界
            if (qAbs(adjustedLabelPos.y()) + textRect.height() / 2 > maxRadius) {
                adjustedLabelPos.setY((adjustedLabelPos.y() > 0) ? maxRadius - textRect.height() / 2 : -maxRadius + textRect.height() / 2);
            }
        } else {
            // 其他方向：稍微偏移以避免与线重叠
            adjustedLabelPos.setX(labelPos.x() - textRect.width() / 2);
            adjustedLabelPos.setY(labelPos.y() + textRect.height() / 2);
            
            // 检查是否超出边界，如果超出则调整到边界内
            double currentDistance = qSqrt(adjustedLabelPos.x() * adjustedLabelPos.x() + adjustedLabelPos.y() * adjustedLabelPos.y());
            if (currentDistance > maxRadius) {
                double scale = maxRadius / currentDistance;
                adjustedLabelPos *= scale;
            }
        }
        
        painter.drawText(adjustedLabelPos, angleText);
        painter.setPen(QPen(QColor(0, 255, 100, 120), 2));
    }
    
    // 次要方位线（每10度，更细更透明）
    painter.setPen(QPen(QColor(0, 255, 100, 40), 0.8));
    for (int angle = 0; angle < 360; angle += 10) {
        if (angle % 30 != 0) { // 跳过主要方位线
            double rad = qDegreesToRadians(double(angle));
            QPointF end(screenRadius * qSin(rad), -screenRadius * qCos(rad));
            painter.drawLine(QPointF(0, 0), end);
        }
    }
    
    // 绘制中心十字线
    painter.setPen(QPen(QColor(0, 255, 255, 100), 1));
    painter.drawLine(QPointF(-screenRadius * 0.1, 0), QPointF(screenRadius * 0.1, 0));
    painter.drawLine(QPointF(0, -screenRadius * 0.1), QPointF(0, screenRadius * 0.1));
    
    // 绘制扫描线和拖影效果
    if (m_radarRunning) {
        // 增强的拖影效果 - 更平滑的渐变
        double trailSpan = 60.0; // 增加拖影跨度
        int steps = 60; // 更多步骤，更平滑
        
        // 绘制多层拖影，创造更真实的雷达效果
        for (int layer = 0; layer < 3; ++layer) {
            double layerSpan = trailSpan * (1.0 - layer * 0.2); // 每层递减
            int layerSteps = steps - layer * 10;
            
            for (int i = 0; i < layerSteps; ++i) {
                double stepAngle = layerSpan / layerSteps;
                double currentAngle = m_scanAngle - (i + 1) * stepAngle;
                double nextAngle = m_scanAngle - i * stepAngle;
                
                // 多层衰减效果
                double fadeRatio = 1.0 - (double(i) / layerSteps);
                fadeRatio = fadeRatio * fadeRatio; // 二次衰减
                int alpha = int((80 - layer * 20) * fadeRatio);
                
                if (alpha > 5) {
                    double qtStartAngle = (90 - nextAngle) * 16;
                    double qtSpanAngle = stepAngle * 16;
                    
                    // 不同层的颜色
                    QColor layerColor;
                    if (layer == 0) layerColor = QColor(0, 255, 0, alpha);      // 最亮绿色
                    else if (layer == 1) layerColor = QColor(0, 200, 100, alpha); // 中等绿色
                    else layerColor = QColor(0, 150, 150, alpha);                // 青绿色
                    
                    painter.setBrush(QBrush(layerColor));
                    painter.setPen(Qt::NoPen);
                    
                    painter.drawPie(QRectF(-screenRadius, -screenRadius, 
                                          2*screenRadius, 2*screenRadius), 
                                   qtStartAngle, qtSpanAngle);
                }
            }
        }
        
        // 绘制主扫描线 - 更亮更粗
        double scanRad = qDegreesToRadians(m_scanAngle);
        QPointF scanEnd(screenRadius * qSin(scanRad), -screenRadius * qCos(scanRad));
        
        // 扫描线发光效果
        painter.setPen(QPen(QColor(0, 255, 0, 100), 8)); // 外发光
        painter.drawLine(QPointF(0, 0), scanEnd);
        painter.setPen(QPen(QColor(0, 255, 0, 255), 4)); // 主扫描线
        painter.drawLine(QPointF(0, 0), scanEnd);
        painter.setPen(QPen(QColor(255, 255, 255, 200), 2)); // 内核心
        painter.drawLine(QPointF(0, 0), scanEnd);
        
        // 扫描线端点光点
        painter.setBrush(QBrush(QColor(0, 255, 0, 200)));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(scanEnd, 4, 4);
    }
    
    // 绘制外边框
    painter.setPen(QPen(QColor(0, 255, 100, 200), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(0, 0), screenRadius, screenRadius);
}

void RadarDisplay::drawDrones(QPainter& painter)
{
    static int drawCount = 0;
    if ((++drawCount % 50) == 0) { // 每50次绘制输出一次调试信息
        qDebug() << "DRAW DRONES: Drawing" << m_drones.size() << "drones";
        for (const DisplayDrone& drone : m_drones) {
            qDebug() << "  Drone" << drone.id << "at world position" << drone.position
                     << "screen position" << worldToScreen(drone.position);
        }
    }

    for (const DisplayDrone& drone : m_drones) {
        // 绘制轨迹
        if (m_showTrails && !drone.trail.isEmpty()) {
            drawDroneTrail(painter, drone);
        }

        // 绘制无人机
        QPointF screenPos = worldToScreen(drone.position);

        // 激光锁定高亮效果
        if (drone.id == m_laserTargetId) {
            // 绘制闪烁的锁定圆圈
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            double blinkPhase = (currentTime % 1000) / 1000.0; // 1秒周期
            int alpha = (int)(128 + 127 * qSin(blinkPhase * 2 * M_PI)); // 闪烁透明度
            
            painter.setPen(QPen(QColor(255, 255, 255, alpha), 3)); // 白色闪烁圆圈
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(screenPos, 15, 15); // 锁定圆圈
            
            // 绘制锁定十字线
            painter.setPen(QPen(QColor(255, 255, 255, alpha), 2)); // 白色十字线
            painter.drawLine(screenPos - QPointF(20, 0), screenPos + QPointF(20, 0));
            painter.drawLine(screenPos - QPointF(0, 20), screenPos + QPointF(0, 20));
        }

        painter.setPen(QPen(drone.color, 2));
        painter.setBrush(drone.color);

        // 绘制无人机图标 - 更专业的显示
        QPolygonF triangle;
        double size = 10; // 增大箭头尺寸
        triangle << QPointF(0, -size) << QPointF(-size/2, size/2) << QPointF(size/2, size/2);

        painter.save();
        painter.translate(screenPos);

        // 根据方向旋转三角形
        if (drone.useNewTrajectory) {
            // 新轨迹系统：使用实际运动方向
            double angle = drone.currentDirection;
            painter.rotate(qRadiansToDegrees(angle) + 90); // +90度因为三角形默认向上
        } else {
            // 旧系统：根据速度方向
            if (drone.velocity.manhattanLength() > 0) {
                double angle = qAtan2(drone.velocity.x(), -drone.velocity.y());
                painter.rotate(qRadiansToDegrees(angle));
            }
        }

        // 绘制无人机发光效果
        painter.setPen(QPen(QColor(drone.color.red(), drone.color.green(), drone.color.blue(), 100), 6));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(triangle);
        
        // 绘制无人机主体
        painter.setPen(QPen(drone.color, 2));
        painter.setBrush(QBrush(drone.color));
        painter.drawPolygon(triangle);
        
        // 绘制内部高光
        painter.setPen(QPen(QColor(255, 255, 255, 150), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(triangle);

        painter.restore();

        // 绘制详细信息（包含ID，去掉白色ID）
        if (m_showInfo) {
            drawDroneInfo(painter, drone);
        }
    }
}

void RadarDisplay::drawDroneInfo(QPainter& painter, const DisplayDrone& drone)
{
    QPointF screenPos = worldToScreen(drone.position);
    
    // 分别显示ID和速度，两行显示
    double speed;
    if (drone.useNewTrajectory && drone.currentSpeed > 0) {
        // 新轨迹系统：使用实际物理速度
        speed = drone.currentSpeed;
    } else {
        // 旧系统：计算速度向量大小
        speed = qSqrt(drone.velocity.x() * drone.velocity.x() + drone.velocity.y() * drone.velocity.y());
    }
    
    QString idText = QString("%1").arg(drone.id);  // 去掉"ID:"
    QString speedText = QString("%1m/s").arg(speed, 0, 'f', 1);
    // 直接使用 drone.distance，这是从 RadarSimulator 正确计算的距离
    QString distanceText = QString("%1m").arg(drone.distance, 0, 'f', 0);

    QPointF textPos = screenPos + QPointF(15, 15);
    
    // 绘制ID号 - 白色加大加粗
    painter.setPen(Qt::white);
    QFont idFont = painter.font();
    idFont.setPointSize(12);  // 较大字体
    idFont.setBold(true);     // 加粗
    painter.setFont(idFont);
    painter.drawText(textPos, idText);
    
    // 绘制速度和距离 - 与三角形同色，字体更大
    painter.setPen(drone.color);  // 使用与三角形相同的颜色
    QFont infoFont = painter.font();
    infoFont.setPointSize(10);   // 增大字体
    infoFont.setBold(false);     // 不加粗
    painter.setFont(infoFont);
    painter.drawText(textPos + QPointF(0, 16), speedText);     // 速度在第二行
    painter.drawText(textPos + QPointF(0, 28), distanceText);  // 距离在第三行
}

void RadarDisplay::drawDroneTrail(QPainter& painter, const DisplayDrone& drone)
{
    if (drone.trail.isEmpty()) return;

    // 绘制带渐变透明度的轨迹，越老的轨迹越透明
    for (int i = 1; i < drone.trail.size(); ++i) {
        double alpha = double(i) / drone.trail.size(); // 0到1的透明度
        QColor trailColor = drone.color;
        trailColor.setAlpha(int(255 * alpha * 0.8)); // 最大透明度为80%

        // 使用更平滑的线条样式
        painter.setPen(QPen(trailColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        QPointF p1 = worldToScreen(drone.trail[i-1]);
        QPointF p2 = worldToScreen(drone.trail[i]);
        painter.drawLine(p1, p2);
        
        // 添加轨迹点标记
        if (i % 3 == 0) { // 每3个点画一个小圆点
            painter.setBrush(QBrush(trailColor));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(p1, 2, 2);
        }
    }

    // 连接最后一个轨迹点到当前位置（最亮）
    if (!drone.trail.isEmpty()) {
        QColor trailColor = drone.color;
        trailColor.setAlpha(220); // 更亮的连接线
        painter.setPen(QPen(trailColor, 4, Qt::DotLine)); // 增加线条粗细

        QPointF lastTrail = worldToScreen(drone.trail.last());
        QPointF currentPos = worldToScreen(drone.position);
        painter.drawLine(lastTrail, currentPos);
    }
}

QColor RadarDisplay::getDroneColor(int droneId)
{
    return m_droneColors[droneId % m_droneColors.size()];
}

QColor RadarDisplay::getThreatBasedColor(double threatScore)
{
    // 根据威胁值动态生成颜色
    if (threatScore >= 10.0) {
        return QColor(255, 0, 0);      // 极高威胁：鲜红色
    } else if (threatScore >= 8.0) {
        return QColor(255, 50, 50);    // 高威胁：红色
    } else if (threatScore >= 6.0) {
        return QColor(255, 100, 0);    // 中等威胁：橙色
    } else if (threatScore >= 3.5) {
        return QColor(255, 200, 0);    // 低威胁：黄色
    } else if (threatScore >= 2.0) {
        return QColor(100, 255, 100);  // 很低威胁：浅绿色
    } else {
        return QColor(50, 200, 50);    // 最低威胁：绿色
    }
}

void RadarDisplay::drawStrikeHighlight(QPainter& painter)
{
    // 设置画笔和笔刷为打击高亮效果
    QPen pen(QColor(255, 0, 0, 200), 4); // 红色，半透明，粗线条
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);

    QBrush brush(QColor(255, 0, 0, 50)); // 红色填充，更透明
    painter.setBrush(brush);

    // 转换打击中心到屏幕坐标
    QPointF centerScreen = worldToScreen(m_strikeCenter);

    // 将世界坐标的半径转换为屏幕坐标的半径
    double radiusScreen = m_strikeRadius * m_scaleFactor;

    // 绘制打击区域圆圈
    painter.drawEllipse(centerScreen, radiusScreen, radiusScreen);

    // 在中心绘制一个小十字标记
    painter.setPen(QPen(QColor(255, 255, 0, 255), 2)); // 黄色十字
    double crossSize = 10;
    painter.drawLine(centerScreen.x() - crossSize, centerScreen.y(),
                    centerScreen.x() + crossSize, centerScreen.y());
    painter.drawLine(centerScreen.x(), centerScreen.y() - crossSize,
                    centerScreen.x(), centerScreen.y() + crossSize);
}

// 新增：绘制动画打击效果
void RadarDisplay::drawStrikeEffects(QPainter& painter)
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    // 绘制所有活跃的打击效果
    for (auto it = m_strikeEffects.begin(); it != m_strikeEffects.end(); ) {
        StrikeEffect& effect = *it;
        qint64 elapsed = currentTime - effect.startTime;

        if (elapsed >= effect.duration) {
            // 动画结束，移除效果
            it = m_strikeEffects.erase(it);
            continue;
        }

        // 计算动画进度 (0.0 到 1.0)
        double progress = static_cast<double>(elapsed) / effect.duration;

        // 创建脉冲效果
        effect.pulsePhase += 0.3; // 恢复原来的脉冲速度
        double pulseScale = 1.0 + 0.5 * qSin(effect.pulsePhase); // 恢复原来的脉冲幅度

        // 随时间增长的外圈
        double outerRadius = effect.radius * (1.0 + progress * 2.0) * pulseScale;
        double innerRadius = effect.radius * pulseScale;

        // 渐变透明度
        int alpha = static_cast<int>(255 * (1.0 - progress));

        // 绘制外圈（冲击波效果）
        QPen outerPen(QColor(255, 100, 0, alpha / 2), 4); // 更粗的线条和更高的透明度
        painter.setPen(outerPen);
        painter.setBrush(Qt::NoBrush);

        QPointF centerScreen = worldToScreen(effect.center);
        double outerRadiusScreen = outerRadius * m_scaleFactor;
        painter.drawEllipse(centerScreen, outerRadiusScreen, outerRadiusScreen);

        // 绘制内圈（核心爆炸效果）
        QPen innerPen(QColor(255, 200, 0, alpha), 3); // 更粗的线条
        painter.setPen(innerPen);
        QBrush innerBrush(QColor(255, 150, 0, alpha / 3)); // 更明显的填充
        painter.setBrush(innerBrush);

        double innerRadiusScreen = innerRadius * m_scaleFactor;
        painter.drawEllipse(centerScreen, innerRadiusScreen, innerRadiusScreen);

        // 绘制火花效果
        if (progress < 0.5) {
            painter.setPen(QPen(QColor(255, 255, 100, alpha), 1));
            for (int i = 0; i < 8; ++i) {
                double angle = i * M_PI / 4.0 + effect.pulsePhase * 0.1;
                double sparkLength = innerRadiusScreen * (1.5 + 0.5 * qSin(effect.pulsePhase + i));
                QPointF sparkEnd = centerScreen + QPointF(
                    qCos(angle) * sparkLength,
                    qSin(angle) * sparkLength
                );
                painter.drawLine(centerScreen, sparkEnd);
            }
        }

        ++it;
    }

    // 如果有活跃效果，继续更新
    if (!m_strikeEffects.isEmpty()) {
        update();
    }
}

// 新增：绘制打击模式鼠标光标
void RadarDisplay::drawStrikeCursor(QPainter& painter)
{
    QPointF cursorScreen = worldToScreen(m_mousePosition);
    double cursorRadiusScreen = m_currentStrikeRadius * m_scaleFactor;

    // 绘制瞄准圆圈
    QPen aimPen(QColor(255, 255, 0, 180), 2);
    aimPen.setStyle(Qt::DashLine);
    painter.setPen(aimPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(cursorScreen, cursorRadiusScreen, cursorRadiusScreen);

    // 绘制十字准星
    QPen crossPen(QColor(255, 255, 0, 220), 2);
    painter.setPen(crossPen);
    double crossSize = 15;

    // 水平线
    painter.drawLine(cursorScreen.x() - crossSize, cursorScreen.y(),
                    cursorScreen.x() + crossSize, cursorScreen.y());
    // 垂直线
    painter.drawLine(cursorScreen.x(), cursorScreen.y() - crossSize,
                    cursorScreen.x(), cursorScreen.y() + crossSize);

    // 绘制中心点
    painter.setPen(QPen(QColor(255, 100, 100, 255), 3));
    painter.drawPoint(cursorScreen);

    // 绘制距离信息
    double distanceFromCenter = qSqrt(m_mousePosition.x() * m_mousePosition.x() +
                                     m_mousePosition.y() * m_mousePosition.y());

    painter.setPen(QColor(255, 255, 255, 200));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    QString distanceText = QString("距离: %1m").arg(distanceFromCenter, 0, 'f', 0);
    painter.drawText(cursorScreen + QPointF(20, -10), distanceText);

    QString radiusText = QString("范围: %1m").arg(m_currentStrikeRadius, 0, 'f', 0);
    painter.drawText(cursorScreen + QPointF(20, 5), radiusText);
}

// 新增：绘制hover效果
void RadarDisplay::drawHoverEffect(QPainter& painter)
{
    QPointF hoverScreen = worldToScreen(m_hoveredDronePosition);

    // 创建脉冲效果
    static qint64 lastTime = 0;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (lastTime == 0) lastTime = currentTime;

    static double pulsePhase = 0;
    pulsePhase += (currentTime - lastTime) * 0.01;
    lastTime = currentTime;

    double pulseScale = 1.0 + 0.4 * qSin(pulsePhase);

    // 绘制hover圆圈
    QPen hoverPen(QColor(100, 200, 255, 180), 3);
    painter.setPen(hoverPen);
    painter.setBrush(QBrush(QColor(100, 200, 255, 30)));

    double hoverRadius = 30 * pulseScale;
    painter.drawEllipse(hoverScreen, hoverRadius, hoverRadius);

    // 绘制选择指示器
    painter.setPen(QPen(QColor(255, 255, 255, 200), 2));
    double indicatorSize = 20;

    // 绘制四个角的指示器
    for (int i = 0; i < 4; ++i) {
        double angle = i * M_PI / 2.0;
        QPointF direction(qCos(angle), qSin(angle));
        QPointF start = hoverScreen + direction * (hoverRadius + 5);
        QPointF end = start + direction * indicatorSize;
        painter.drawLine(start, end);
    }

    // 继续更新动画
    update();
}

// 新增：控制雷达扫描线
void RadarDisplay::setRadarRunning(bool running)
{
    m_radarRunning = running;
    qDebug() << "Radar display running state set to:" << running;
}
