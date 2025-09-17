#include "RadarSimulator.h"
#include <QDebug>
#include <QDataStream>
#include <QIODevice>
#include <QDateTime>
#include <QtMath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>

RadarSimulator::RadarSimulator(DroneManager* droneManager, QObject *parent)
    : QObject(parent)
    , m_droneManager(droneManager)
    , m_radarCenter(0, 0)
    , m_radarRadius(800.0)
    , m_scanInterval(1000)
{
    m_scanTimer = new QTimer(this);
    m_udpSocket = new QUdpSocket(this);
    m_configSocket = new QUdpSocket(this);
    
    connect(m_scanTimer, &QTimer::timeout, this, &RadarSimulator::performRadarScan);
    connect(m_configSocket, &QUdpSocket::readyRead, this, &RadarSimulator::handleConfigMessage);
}

RadarSimulator::~RadarSimulator()
{
    stopRadar();
    stopServer();
}

void RadarSimulator::startRadar()
{
    if (!m_scanTimer->isActive()) {
        m_scanTimer->start(m_scanInterval);
        qDebug() << "Radar started with scan interval:" << m_scanInterval << "ms";
        qDebug() << "Radar center:" << m_radarCenter << "radius:" << m_radarRadius;
    }
}

void RadarSimulator::stopRadar()
{
    if (m_scanTimer->isActive()) {
        m_scanTimer->stop();
        qDebug() << "Radar stopped";
    }
}

void RadarSimulator::startServer(quint16 port)
{
    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        qWarning() << "UDP socket is already bound";
        return;
    }
    
    if (m_udpSocket->bind(port)) {
        qDebug() << "UDP server started on port" << port;
    } else {
        qWarning() << "Failed to bind UDP socket to port" << port << ":" << m_udpSocket->errorString();
    }
}

void RadarSimulator::stopServer()
{
    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_clients.clear();
        m_udpSocket->close();
        qDebug() << "UDP server stopped";
    }
}

bool RadarSimulator::isServerRunning() const
{
    return m_udpSocket->state() == QAbstractSocket::BoundState;
}

void RadarSimulator::addClient(const QHostAddress& address, quint16 port)
{
    QPair<QHostAddress, quint16> client(address, port);
    if (!m_clients.contains(client)) {
        m_clients.append(client);
        qDebug() << "Added UDP client:" << address.toString() << ":" << port;
        emit clientAdded(QString("%1:%2").arg(address.toString()).arg(port));
    }
}

QList<RadarDetection> RadarSimulator::performScan()
{
    QList<RadarDetection> detections;
    QList<Drone*> activeDrones = m_droneManager->getActiveDrones();
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    qDebug() << "=== RADAR SCAN START ===";
    qDebug() << "Active drones:" << activeDrones.size();
    qDebug() << "Radar center:" << m_radarCenter << "radius:" << m_radarRadius;
    
    for (Drone* drone : activeDrones) {
        QPointF dronePos = drone->getCurrentPosition();
        double distance = calculateDistance(m_radarCenter, dronePos);
        
        qDebug() << "Checking drone" << drone->getId() 
                 << "at position" << dronePos 
                 << "distance from radar:" << distance;
        
        // 检查无人机是否在雷达探测范围内
        if (drone->isInRadarRange(m_radarCenter, m_radarRadius)) {
            RadarDetection detection;
            detection.droneId = drone->getId();
            detection.position = dronePos;
            detection.velocity = QPointF(drone->getVelocityX(), drone->getVelocityY());
            detection.detectionTime = currentTime;
            detection.distance = distance;
            detection.azimuth = calculateAzimuth(m_radarCenter, detection.position);
            
            // 填充轨迹系统信息
            detection.trajectoryType = drone->getTrajectoryType();
            detection.speedType = drone->getSpeedType();
            detection.currentDirection = drone->getCurrentDirection();
            detection.currentSpeed = drone->getCurrentSpeed();
            detection.useNewTrajectory = true; // 新生成的无人机都使用新轨迹系统
            
            detections.append(detection);
            
            qDebug() << "*** DETECTED drone" << detection.droneId 
                     << "at position" << detection.position
                     << "distance" << detection.distance
                     << "azimuth" << qRadiansToDegrees(detection.azimuth) << "degrees";
        } else {
            qDebug() << "Drone" << drone->getId() << "is OUT OF RANGE (distance:" << distance << ")";
        }
    }
    
    qDebug() << "=== RADAR SCAN COMPLETE: " << detections.size() << "detections ===";
    return detections;
}

void RadarSimulator::performRadarScan()
{
    m_latestDetections = performScan();
    
    qDebug() << "Radar scan completed. Detections:" << m_latestDetections.size() 
             << "Clients:" << m_clients.size();
    
    // 发送数据到所有连接的客户端
    if (!m_latestDetections.isEmpty() && !m_clients.isEmpty()) {
        QByteArray data = serializeDetections(m_latestDetections);
        qDebug() << "Sending data to clients. Data size:" << data.size() << "bytes";
        sendDataToClients(data);
    } else if (m_clients.isEmpty()) {
        qDebug() << "No clients connected, not sending data";
    } else {
        qDebug() << "No detections, not sending data";
    }
    
    emit radarScanCompleted(m_latestDetections);
}

void RadarSimulator::sendDataToClients(const QByteArray& data)
{
    qDebug() << "Sending UDP data to" << m_clients.size() << "clients";
    
    for (const QPair<QHostAddress, quint16>& client : m_clients) {
        qDebug() << "Sending to client:" << client.first.toString() << ":" << client.second;
        qint64 bytesWritten = m_udpSocket->writeDatagram(data, client.first, client.second);
        if (bytesWritten == -1) {
            qWarning() << "Failed to send UDP data to" << client.first.toString() << ":" << client.second
                       << m_udpSocket->errorString();
        } else {
            qDebug() << "Successfully sent" << bytesWritten << "bytes to" 
                     << client.first.toString() << ":" << client.second;
        }
    }
    
    if (!m_clients.isEmpty()) {
        emit dataSent(data);
    }
}

QByteArray RadarSimulator::serializeDetections(const QList<RadarDetection>& detections)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    
    // 写入魔术数字和版本
    stream << quint32(0x52444152); // "RDAR"
    stream << quint32(1);          // Version
    
    // 写入时间戳
    stream << QDateTime::currentMSecsSinceEpoch();
    
    // 写入检测数量
    stream << quint32(detections.size());
    
    // 写入每个检测结果
    for (const RadarDetection& detection : detections) {
        stream << detection.droneId;
        stream << detection.position;
        stream << detection.velocity;
        stream << detection.detectionTime;
        stream << detection.distance;
        stream << detection.azimuth;
        
        // 写入轨迹系统信息
        stream << quint32(static_cast<uint32_t>(detection.trajectoryType));
        stream << quint32(static_cast<uint32_t>(detection.speedType));
        stream << detection.currentDirection;
        stream << detection.currentSpeed;
        stream << detection.useNewTrajectory;
    }
    
    return data;
}

double RadarSimulator::calculateDistance(QPointF pos1, QPointF pos2)
{
    double dx = pos2.x() - pos1.x();
    double dy = pos2.y() - pos1.y();
    return qSqrt(dx * dx + dy * dy);
}

double RadarSimulator::calculateAzimuth(QPointF center, QPointF target)
{
    double dx = target.x() - center.x();
    double dy = target.y() - center.y();
    
    // 计算相对于正北方向的角度（数学角度系统，0度为东，90度为北）
    // 转换为雷达角度系统（0度为北，顺时针增加）
    double angle = qAtan2(dx, -dy); // 注意y轴方向
    
    // 确保角度在0-2π范围内
    if (angle < 0) {
        angle += 2 * M_PI;
    }
    
    return angle;
}

void RadarSimulator::startConfigServer(quint16 configPort)
{
    if (m_configSocket->state() != QAbstractSocket::UnconnectedState) {
        qWarning() << "Config UDP socket is already bound";
        return;
    }
    
    if (m_configSocket->bind(QHostAddress::Any, configPort)) {
        qDebug() << "Config server started on port:" << configPort;
    } else {
        qWarning() << "Failed to start config server on port:" << configPort;
    }
}

void RadarSimulator::handleConfigMessage()
{
    while (m_configSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_configSocket->receiveDatagram();
        QByteArray data = datagram.data();
        QHostAddress sender = datagram.senderAddress();
        quint16 senderPort = datagram.senderPort();
        
        qDebug() << "Received config message from" << sender.toString() << ":" << senderPort;
        qDebug() << "Data:" << data;
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject command = doc.object();
            processConfigCommand(command, sender, senderPort);
        } else {
            qWarning() << "Invalid JSON in config message:" << error.errorString();
        }
    }
}

void RadarSimulator::processConfigCommand(const QJsonObject& command, const QHostAddress& sender, quint16 senderPort)
{
    QString type = command["type"].toString();
    QJsonObject response;
    response["type"] = "config_result";
    
    if (type == "config") {
        QString category = command["category"].toString();
        response["category"] = category;
        
        if (category == "radar") {
            bool changed = false;
            QString changes;
            
            if (command.contains("scanInterval")) {
                int newInterval = command["scanInterval"].toInt();
                if (newInterval != m_scanInterval) {
                    m_scanInterval = newInterval;
                    if (m_scanTimer->isActive()) {
                        m_scanTimer->stop();
                        m_scanTimer->start(m_scanInterval);
                    }
                    changes += QString("扫描间隔: %1ms ").arg(newInterval);
                    changed = true;
                }
            }
            
            if (command.contains("radarRadius")) {
                double newRadius = command["radarRadius"].toDouble();
                if (newRadius != m_radarRadius) {
                    m_radarRadius = newRadius;
                    changes += QString("雷达半径: %1px ").arg(newRadius);
                    changed = true;
                }
            }
            
            if (command.contains("centerX") && command.contains("centerY")) {
                double newX = command["centerX"].toDouble();
                double newY = command["centerY"].toDouble();
                QPointF newCenter(newX, newY);
                if (newCenter != m_radarCenter) {
                    m_radarCenter = newCenter;
                    changes += QString("中心位置: (%1,%2) ").arg(newX).arg(newY);
                    changed = true;
                }
            }
            
            response["success"] = changed;
            response["message"] = changed ? changes.trimmed() : "没有参数需要更新";
            
        } else if (category == "drone") {
            bool changed = false;
            QString changes;
            
            if (command.contains("generationInterval")) {
                int newInterval = command["generationInterval"].toInt();
                if (m_droneManager->getGenerationInterval() != newInterval) {
                    if (m_droneManager->isAutoGenerationActive()) {
                        m_droneManager->stopAutoGeneration();
                        m_droneManager->startAutoGeneration(newInterval);
                    }
                    changes += QString("生成间隔: %1ms ").arg(newInterval);
                    changed = true;
                }
            }
            
            // 注意：maxDrones, minSpeed, maxSpeed等参数需要在DroneManager中实现相应的设置方法
            // 这里先记录日志
            if (command.contains("maxDrones")) {
                qDebug() << "Max drones setting:" << command["maxDrones"].toInt();
                changes += QString("最大无人机数: %1 ").arg(command["maxDrones"].toInt());
                changed = true;
            }
            
            if (command.contains("minSpeed") || command.contains("maxSpeed")) {
                qDebug() << "Speed range setting:" << command["minSpeed"].toDouble() 
                         << "-" << command["maxSpeed"].toDouble();
                changes += QString("速度范围: %1-%2 ")
                          .arg(command["minSpeed"].toDouble())
                          .arg(command["maxSpeed"].toDouble());
                changed = true;
            }
            
            response["success"] = changed;
            response["message"] = changed ? changes.trimmed() : "没有参数需要更新";
        } else {
            response["success"] = false;
            response["message"] = "未知的配置类别: " + category;
        }
        
    } else if (type == "query") {
        QString request = command["request"].toString();
        
        if (request == "current_settings") {
            response["type"] = "settings";
            response["scanInterval"] = m_scanInterval;
            response["radarRadius"] = m_radarRadius;
            response["centerX"] = m_radarCenter.x();
            response["centerY"] = m_radarCenter.y();
            response["generationInterval"] = m_droneManager ? m_droneManager->getGenerationInterval() : 3000;
            response["maxDrones"] = 10; // 硬编码值，需要从DroneManager获取
            response["minSpeed"] = 10.0;
            response["maxSpeed"] = 50.0;
        }
    }
    
    sendConfigResponse(response, sender, senderPort);
}

QJsonObject RadarSimulator::getCurrentSettings() const
{
    QJsonObject settings;
    settings["scanInterval"] = m_scanInterval;
    settings["radarRadius"] = m_radarRadius;
    settings["centerX"] = m_radarCenter.x();
    settings["centerY"] = m_radarCenter.y();
    settings["generationInterval"] = m_droneManager ? m_droneManager->getGenerationInterval() : 3000;
    return settings;
}

void RadarSimulator::sendConfigResponse(const QJsonObject& response, const QHostAddress& address, quint16 port)
{
    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    qint64 sent = m_configSocket->writeDatagram(data, address, port);
    qDebug() << "Sent config response to" << address.toString() << ":" << port << "bytes:" << sent;
}
