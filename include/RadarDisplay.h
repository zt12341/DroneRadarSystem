#ifndef RADARDISPLAY_H
#define RADARDISPLAY_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QUdpSocket>
#include <QHostAddress>
#include <QPointF>
#include <QList>
#include <QColor>
#include "RadarSimulator.h"
#include "Drone.h"

struct DisplayDrone {
    int id;
    QPointF position;
    QPointF velocity;
    qint64 lastUpdateTime;
    double distance;
    double azimuth;
    QColor color;
    QList<QPointF> trail; // 轨迹点
    DroneType type;        // 无人机类型
    int threatLevel;       // 威胁等级
    double threatScore;    // 威胁评分
    
    // 新轨迹系统字段
    TrajectoryType trajectoryType = TrajectoryType::Linear;
    SpeedType speedType = SpeedType::Constant;
    double currentDirection = 0.0; // 当前方向角度（弧度）
    double currentSpeed = 0.0;     // 新轨迹系统的实际速度（米/秒）
    bool useNewTrajectory = false; // 是否使用新轨迹系统
};

class RadarDisplay : public QWidget
{
    Q_OBJECT

public:
    explicit RadarDisplay(QWidget *parent = nullptr);
    ~RadarDisplay();
    
    // 连接设置
    void connectToRadar(const QString& host = "127.0.0.1", quint16 port = 12345);
    void disconnectFromRadar();
    bool isConnected() const;
    
    // 显示设置
    void setRadarRadius(double radius) { m_radarRadius = radius; update(); }
    void setShowTrails(bool show) { m_showTrails = show; update(); }
    void setShowInfo(bool show) { m_showInfo = show; update(); }
    void setTrailLength(int length) { m_trailLength = length; }
    
    double getRadarRadius() const { return m_radarRadius; }
    bool getShowTrails() const { return m_showTrails; }
    bool getShowInfo() const { return m_showInfo; }
    int getTrailLength() const { return m_trailLength; }
    double getScaleFactor() const { return m_scaleFactor; }
    
    // 激光锁定功能
    void setLaserTarget(int droneId) { m_laserTargetId = droneId; update(); }
    void clearLaserTarget() { m_laserTargetId = -1; update(); }
    
    // 清除显示
    void clearDisplay();
    
    // 打击高亮
    void highlightStrikeArea(QPointF center, double radius);
    void highlightStrikeAreaWithAnimation(QPointF center, double radius, int duration = 2000);
    
    // 新增：鼠标交互打击功能
    void setStrikeMode(bool enabled);
    bool getStrikeMode() const { return m_strikeMode; }
    
    // 新增：控制雷达扫描线
    void setRadarRunning(bool running);
    void setStrikeRadius(double radius) { m_currentStrikeRadius = radius; }
    double getStrikeRadius() const { return m_currentStrikeRadius; }
    
    // 新增：多重打击效果
    void addStrikeEffect(QPointF center, double radius);
    void clearStrikeEffects();
    
    // 新增：状态信息设置
    void setStatusInfo(const QString& connectionStatus, int currentDetections, 
                      int totalDetections, const QString& lastUpdate,
                      const QString& systemStatus, const QString& droneCount, 
                      const QString& radarStatus);

signals:
    void connectionStatusChanged(bool connected);
    void droneDataReceived(int droneCount);
    void strikeRequested(QPointF center, double radius); // 新增：鼠标点击打击信号
    void droneClicked(int droneId, QPointF position); // 新增：无人机点击信号

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override; // 新增：鼠标移动事件
    void leaveEvent(QEvent* event) override; // 新增：鼠标离开事件

private slots:
    void handleDataReceived();
    void updateDisplay();
    void cleanupOldDrones();

private:
    void parseRadarData(const QByteArray& data);
    void processRadarData(const QList<RadarDetection>& detections);
    QPointF worldToScreen(QPointF worldPos);
    QPointF screenToWorld(QPointF screenPos);
    void drawRadarGrid(QPainter& painter);
    void drawDrones(QPainter& painter);
    void drawDroneInfo(QPainter& painter, const DisplayDrone& drone);
    void drawDroneTrail(QPainter& painter, const DisplayDrone& drone);
    void drawStrikeHighlight(QPainter& painter);
    void drawStrikeEffects(QPainter& painter);
    void drawStrikeCursor(QPainter& painter);
    void drawHoverEffect(QPainter& painter);
    QColor getDroneColor(int droneId);
    QColor getThreatBasedColor(double threatScore); // 根据威胁值获取颜色
    
    // 网络连接
    QUdpSocket* m_udpSocket;
    QHostAddress m_serverAddress;
    quint16 m_serverPort;
    
    // 显示参数
    double m_radarRadius;
    QPointF m_radarCenter;
    double m_scaleFactor;
    bool m_showTrails;
    bool m_showInfo;
    int m_trailLength;
    
    // 无人机数据
    QList<DisplayDrone> m_drones;
    QTimer* m_updateTimer;
    QTimer* m_cleanupTimer;
    
    // 颜色列表
    QList<QColor> m_droneColors;
    
    // 统计信息
    int m_totalDronesDetected;
    qint64 m_lastDataTime;
    
    // 打击高亮
    bool m_showStrikeHighlight;
    QPointF m_strikeCenter;
    double m_strikeRadius;
    QTimer* m_strikeHighlightTimer;
    
    // 新增：打击相关结构
    struct StrikeEffect {
        QPointF center;
        double radius;
        qint64 startTime;
        int duration;
        double currentRadius;
        QColor color;
        int pulsePhase;
    };
    
    // 新增：交互状态
    bool m_strikeMode;
    double m_currentStrikeRadius;
    QPointF m_mousePosition;
    bool m_showMouseCursor;
    
    // 激光锁定状态
    int m_laserTargetId;
    
    // 新增：多重打击效果
    QList<StrikeEffect> m_strikeEffects;
    QTimer* m_strikeEffectTimer;
    
    // 新增：动画参数
    int m_animationPhase;
    QTimer* m_animationTimer;
    
    // 新增：扫描线动画
    double m_scanAngle;
    QTimer* m_scanTimer;
    bool m_radarRunning; // 雷达运行状态
    
    // 新增：hover状态
    int m_hoveredDroneId;
    QPointF m_hoveredDronePosition;
    
    // 新增：状态信息显示
    QString m_connectionStatus;
    int m_currentDetections;
    int m_totalDetections;
    QString m_lastUpdate;
    QString m_systemStatus;
    QString m_droneCount;
    QString m_radarStatus;
};

#endif // RADARDISPLAY_H
