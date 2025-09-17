#ifndef RADARSIMULATOR_H
#define RADARSIMULATOR_H

#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QHostAddress>
#include <QPointF>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>
#include "DroneManager.h"
#include "Drone.h"

struct RadarDetection {
    int droneId;
    QPointF position;
    QPointF velocity;
    qint64 detectionTime;
    double distance;
    double azimuth; // 方位角
    
    // 新轨迹系统字段
    TrajectoryType trajectoryType = TrajectoryType::Linear;
    SpeedType speedType = SpeedType::Constant;
    double currentDirection = 0.0; // 当前方向角度（弧度）
    double currentSpeed = 0.0;     // 新轨迹系统的实际速度（米/秒）
    bool useNewTrajectory = false; // 是否使用新轨迹系统
};

class RadarSimulator : public QObject
{
    Q_OBJECT

public:
    explicit RadarSimulator(DroneManager* droneManager, QObject *parent = nullptr);
    ~RadarSimulator();
    
    // 雷达配置
    void setRadarCenter(QPointF center) { m_radarCenter = center; }
    void setRadarRadius(double radius) { m_radarRadius = radius; }
    void setScanInterval(int intervalMs) { m_scanInterval = intervalMs; }
    
    QPointF getRadarCenter() const { return m_radarCenter; }
    double getRadarRadius() const { return m_radarRadius; }
    int getScanInterval() const { return m_scanInterval; }
    
    // 雷达控制
    void startRadar();
    void stopRadar();
    bool isRunning() const { return m_scanTimer->isActive(); }
    
    // 网络UDP服务器
    void startServer(quint16 port = 12345);
    void stopServer();
    bool isServerRunning() const;
    void addClient(const QHostAddress& address, quint16 port);
    
    // 配置管理
    void startConfigServer(quint16 configPort = 12347);
    void processConfigCommand(const QJsonObject& command, const QHostAddress& sender, quint16 senderPort);
    QJsonObject getCurrentSettings() const;
    
    // 手动扫描
    QList<RadarDetection> performScan();
    
    // 获取最新检测结果
    QList<RadarDetection> getLatestDetections() const { return m_latestDetections; }

signals:
    void radarScanCompleted(QList<RadarDetection> detections);
    void clientAdded(QString clientAddress);
    void dataSent(QByteArray data);

private slots:
    void performRadarScan();
    void handleConfigMessage();

private:
    void sendDataToClients(const QByteArray& data);
    void sendConfigResponse(const QJsonObject& response, const QHostAddress& address, quint16 port);
    QByteArray serializeDetections(const QList<RadarDetection>& detections);
    double calculateDistance(QPointF pos1, QPointF pos2);
    double calculateAzimuth(QPointF center, QPointF target);
    
    DroneManager* m_droneManager;
    QTimer* m_scanTimer;
    QUdpSocket* m_udpSocket;
    QUdpSocket* m_configSocket;
    QList<QPair<QHostAddress, quint16>> m_clients;
    
    // 雷达参数
    QPointF m_radarCenter;
    double m_radarRadius;
    int m_scanInterval;
    
    // 检测结果
    QList<RadarDetection> m_latestDetections;
};

#endif // RADARSIMULATOR_H
