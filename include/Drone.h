#ifndef DRONE_H
#define DRONE_H

#include <QObject>
#include <QPointF>
#include <QTimer>

enum class DroneType {
    Standard = 0  // 统一类型，威胁值只按距离计算
};

enum class TrajectoryType {
    Linear = 0,    // 直线轨迹
    Curved = 1     // 弧形轨迹
};

enum class SpeedType {
    Constant = 0,    // 匀速
    Accelerating = 1 // 均匀变速
};

class Drone : public QObject
{
    Q_OBJECT

public:
    explicit Drone(int id, QPointF initialPos, double vx, double vy, DroneType type = DroneType::Standard, QObject *parent = nullptr);
    explicit Drone(int id, QPointF startPos, QPointF endPos, TrajectoryType trajectory, 
                  SpeedType speedType, double startSpeed, double endSpeed = -1, 
                  DroneType type = DroneType::Standard, QObject *parent = nullptr);
    
    // 基本属性获取
    int getId() const { return m_id; }
    QPointF getCurrentPosition() const { return m_currentPosition; }
    QPointF getInitialPosition() const { return m_initialPosition; }
    double getVelocityX() const { return m_velocityX; }
    double getVelocityY() const { return m_velocityY; }
    qint64 getStartTime() const { return m_startTime; }
    qint64 getCurrentTime() const;
    bool isActive() const { return m_active; }
    void setVelocity(double vx, double vy);
    
    // 新轨迹系统属性
    TrajectoryType getTrajectoryType() const { return m_trajectoryType; }
    SpeedType getSpeedType() const { return m_speedType; }
    double getCurrentDirection() const { return m_direction; }
    double getCurrentSpeed() const { return m_currentSpeed; }
    double getTrajectoryProgress() const { return m_trajectoryProgress; }
    QPointF getStartPosition() const { return m_startPos; }
    QPointF getTargetPosition() const { return m_targetPos; }
    void applyVelocityChange(double deltaVx, double deltaVy, double maxSpeed);
    void setMaxSpeed(double maxSpeed);
    double getMaxSpeed() const;
    // 威胁相关
    DroneType getType() const { return m_type; }
    double getBaseWeight() const;
    double getSpeed() const;
    int getThreatLevel() const;
    double getThreatScore() const;
    bool isDestroyed() const { return m_destroyed; }
    
    // 位置计算
    QPointF calculatePositionAtTime(qint64 timeMs) const;
    void updatePosition();
    
    // 区域检测
    bool isInSquareArea(double squareSize) const;
    bool isInRadarRange(QPointF radarCenter, double radarRadius) const;
    bool isInStrikeRange(QPointF strikeCenter, double strikeRadius) const;
    
    // 状态控制
    void setActive(bool active) { m_active = active; }
    void destroy();
    
    // 序列化/反序列化（用于网络传输）
    QByteArray serialize() const;
    static Drone* deserialize(const QByteArray& data, QObject* parent = nullptr);
    
    // 新增：轨迹预测和拦截计算
    QPointF predictPositionAtTime(qint64 futureTimeMs) const;
    QPointF calculateInterceptPoint(QPointF interceptorPos, double interceptorSpeed) const;
    double getTimeToReachRadarCenter() const;
    double getMinDistanceToRadarCenter() const;
    bool willEnterRadarZone(QPointF radarCenter, double radarRadius, qint64 timeWindowMs = 10000) const;

signals:
    void positionUpdated(int droneId, QPointF position);
    void droneOutOfBounds(int droneId);
    void droneDestroyed(int droneId);

private:
    int m_id;
    QPointF m_initialPosition;
    QPointF m_currentPosition;
    double m_velocityX;
    double m_velocityY;
    double m_maxSpeed;
    qint64 m_startTime;
    bool m_active;
    bool m_destroyed;
    DroneType m_type;
    
    // 新轨迹系统成员
    TrajectoryType m_trajectoryType;   // 轨迹类型
    SpeedType m_speedType;             // 速度类型
    QPointF m_startPos;                // 起始位置
    QPointF m_targetPos;               // 目标位置
    QPointF m_controlPoint;            // 贝塞尔曲线控制点（弧形轨迹用）
    double m_startSpeed;               // 起始速度
    double m_endSpeed;                 // 结束速度
    double m_currentSpeed;             // 当前速度
    double m_trajectoryProgress;       // 轨迹进度(0.0-1.0)
    double m_totalDistance;            // 总距离
    double m_direction;                // 当前运动方向（弧度）
    bool m_useNewTrajectorySystem;     // 是否使用新轨迹系统
    
    // 辅助方法
    void initializeTrajectory();
    QPointF calculateBezierPoint(double t) const;
    QPointF calculateBezierTangent(double t) const;
    double calculateCurrentSpeedForProgress(double progress) const;
};

#endif // DRONE_H
