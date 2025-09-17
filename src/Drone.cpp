#include "Drone.h"
#include <QDateTime>
#include <QDataStream>
#include <QIODevice>
#include <QtMath>
#include <QRandomGenerator>
#include <cmath> // Required for std::numeric_limits

Drone::Drone(int id, QPointF initialPos, double vx, double vy, DroneType type, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_initialPosition(initialPos)
    , m_currentPosition(initialPos)
    , m_velocityX(vx)
    , m_velocityY(vy)
    , m_startTime(QDateTime::currentMSecsSinceEpoch())
    , m_active(true)
    , m_destroyed(false)
    , m_type(type)
    , m_useNewTrajectorySystem(false)
{
}

Drone::Drone(int id, QPointF startPos, QPointF endPos, TrajectoryType trajectory, 
             SpeedType speedType, double startSpeed, double endSpeed, 
             DroneType type, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_initialPosition(startPos)
    , m_currentPosition(startPos)
    , m_startTime(QDateTime::currentMSecsSinceEpoch())
    , m_active(true)
    , m_destroyed(false)
    , m_type(type)
    , m_trajectoryType(trajectory)
    , m_speedType(speedType)
    , m_startPos(startPos)
    , m_targetPos(endPos)
    , m_startSpeed(startSpeed)
    , m_endSpeed(endSpeed < 0 ? startSpeed : endSpeed)
    , m_currentSpeed(startSpeed)
    , m_trajectoryProgress(0.0)
    , m_direction(0.0)
    , m_useNewTrajectorySystem(true)
{
    initializeTrajectory();
}

qint64 Drone::getCurrentTime() const
{
    return QDateTime::currentMSecsSinceEpoch();
}

QPointF Drone::calculatePositionAtTime(qint64 timeMs) const
{
    if (!m_active) {
        return m_currentPosition;
    }
    
    if (!m_useNewTrajectorySystem) {
        // 使用旧的线性计算方法
        double timeSeconds = (timeMs - m_startTime) / 1000.0;
        double x = m_initialPosition.x() + m_velocityX * timeSeconds;
        double y = m_initialPosition.y() + m_velocityY * timeSeconds;
        return QPointF(x, y);
    }
    
    // 使用新的轨迹系统
    double elapsedSeconds = (timeMs - m_startTime) / 1000.0;
    
    // 根据速度类型计算当前应该走过的距离
    double totalTime = 0;
    if (m_speedType == SpeedType::Constant) {
        totalTime = m_totalDistance / qMax(1.0, m_startSpeed); // 确保速度不为0
    } else {
        // 均匀变速：使用平均速度计算总时间
        double avgSpeed = (m_startSpeed + m_endSpeed) / 2.0;
        avgSpeed = qMax(10.0, avgSpeed); // 确保平均速度至少10m/s
        totalTime = m_totalDistance / avgSpeed;
    }
    
    // 确保总时间合理，防止过短或过长
    totalTime = qMax(5.0, qMin(120.0, totalTime)); // 5-120秒之间
    
    double progress = qMin(1.0, elapsedSeconds / totalTime);
    
    // 如果进度达到1.0，让无人机继续运动到边界外
    if (progress >= 1.0) {
        // 继续延伸轨迹，确保无人机飞出边界
        double extraProgress = (elapsedSeconds - totalTime) / totalTime;
        progress = 1.0 + extraProgress * 0.5; // 继续飞行，但减速
    }
    
    QPointF position;
    if (m_trajectoryType == TrajectoryType::Linear) {
        // 直线轨迹
        position = m_startPos + progress * (m_targetPos - m_startPos);
    } else {
        // 弧形轨迹（贝塞尔曲线）
        position = calculateBezierPoint(progress);
    }
    
    return position;
}

void Drone::destroy()
{
    if (!m_destroyed) {
        m_destroyed = true;
        m_active = false;
        emit droneDestroyed(m_id);
    }
}

void Drone::updatePosition()
{
    if (!m_active) {
        return;
    }
    
    qint64 currentTime = getCurrentTime();
    QPointF oldPosition = m_currentPosition;
    QPointF newPosition = calculatePositionAtTime(currentTime);
    
    if (newPosition != m_currentPosition) {
        m_currentPosition = newPosition;
        
        // 更新方向和轨迹进度（仅新轨迹系统）
        if (m_useNewTrajectorySystem) {
            double elapsedSeconds = (currentTime - m_startTime) / 1000.0;
            
            // 计算轨迹进度
            double totalTime = 0;
            if (m_speedType == SpeedType::Constant) {
                totalTime = m_totalDistance / m_startSpeed;
            } else {
                double avgSpeed = (m_startSpeed + m_endSpeed) / 2.0;
                totalTime = m_totalDistance / avgSpeed;
            }
            
            m_trajectoryProgress = qMin(1.0, elapsedSeconds / totalTime);
            
            // 计算当前运动方向
            if (m_trajectoryType == TrajectoryType::Linear) {
                // 直线轨迹：方向就是起点到终点的方向
                QPointF direction = m_targetPos - m_startPos;
                if (direction.x() != 0 || direction.y() != 0) {
                    m_direction = qAtan2(direction.y(), direction.x());
                }
            } else {
                // 弧形轨迹：使用切线方向
                QPointF tangent = calculateBezierTangent(m_trajectoryProgress);
                if (tangent.x() != 0 || tangent.y() != 0) {
                    m_direction = qAtan2(tangent.y(), tangent.x());
                }
            }
            
            // 更新当前速度
            m_currentSpeed = calculateCurrentSpeedForProgress(m_trajectoryProgress);
            
            // 更新速度向量（用于兼容性）
            if (m_currentSpeed > 0) {
                m_velocityX = m_currentSpeed * qCos(m_direction);
                m_velocityY = m_currentSpeed * qSin(m_direction);
            }
            
            // 调试信息：输出轨迹进度
            static int debugCounter = 0;
            if (++debugCounter % 100 == 0) {  // 每100次输出一次
                qDebug() << "Drone" << m_id << "progress:" << m_trajectoryProgress 
                         << "speed:" << m_currentSpeed << "pos:" << m_currentPosition;
            }
        }
        
        emit positionUpdated(m_id, m_currentPosition);
    }
}

bool Drone::isInSquareArea(double squareSize) const
{
    double halfSize = squareSize / 2.0;
    return (m_currentPosition.x() >= -halfSize && m_currentPosition.x() <= halfSize &&
            m_currentPosition.y() >= -halfSize && m_currentPosition.y() <= halfSize);
}

bool Drone::isInRadarRange(QPointF radarCenter, double radarRadius) const
{
    double dx = m_currentPosition.x() - radarCenter.x();
    double dy = m_currentPosition.y() - radarCenter.y();
    double distance = qSqrt(dx * dx + dy * dy);
    return distance <= radarRadius;
}

bool Drone::isInStrikeRange(QPointF strikeCenter, double strikeRadius) const
{
    double dx = m_currentPosition.x() - strikeCenter.x();
    double dy = m_currentPosition.y() - strikeCenter.y();
    double distance = qSqrt(dx * dx + dy * dy);
    return distance <= strikeRadius;
}

double Drone::getBaseWeight() const
{
    // 统一类型，基础权重为1.0，威胁值主要由距离决定
    return 1.0;
}

// 在Drone.cpp中添加以下实现
void Drone::setVelocity(double vx, double vy)
{
    m_velocityX = vx;
    m_velocityY = vy;

    // 确保速度不超过最大值
    double currentSpeed = getSpeed();
    if (currentSpeed > m_maxSpeed && m_maxSpeed > 0) {
        double ratio = m_maxSpeed / currentSpeed;
        m_velocityX *= ratio;
        m_velocityY *= ratio;
    }
}

void Drone::applyVelocityChange(double deltaVx, double deltaVy, double maxSpeed)
{
    m_velocityX += deltaVx;
    m_velocityY += deltaVy;

    // 确保速度不超过最大值
    double currentSpeed = getSpeed();
    if (currentSpeed > maxSpeed && maxSpeed > 0) {
        double ratio = maxSpeed / currentSpeed;
        m_velocityX *= ratio;
        m_velocityY *= ratio;
    }
}

void Drone::setMaxSpeed(double maxSpeed)
{
    m_maxSpeed = maxSpeed;
}

double Drone::getMaxSpeed() const
{
    return m_maxSpeed;
}

double Drone::getSpeed() const
{
    return qSqrt(m_velocityX * m_velocityX + m_velocityY * m_velocityY);
}

int Drone::getThreatLevel() const
{
    // 根据距离雷达中心的距离计算威胁等级（距离越近威胁越大）
    double distance = qSqrt(m_currentPosition.x() * m_currentPosition.x() + 
                           m_currentPosition.y() * m_currentPosition.y());
    
    // 威胁等级：距离雷达中心越近威胁越高
    if (distance < 100) return 10;      // 极高威胁
    else if (distance < 200) return 8;  // 高威胁  
    else if (distance < 400) return 6;  // 中等威胁
    else if (distance < 600) return 4;  // 低威胁
    else if (distance < 800) return 2;  // 很低威胁
    else return 1;                      // 最低威胁
}

double Drone::getThreatScore() const
{
    // 威胁评分 = 距离雷达中心的距离倒数 * 1000（距离越近分数越高）
    double distance = qSqrt(m_currentPosition.x() * m_currentPosition.x() + 
                           m_currentPosition.y() * m_currentPosition.y());
    
    // 防止除零，最小距离设为1
    distance = qMax(1.0, distance);
    
    // 威胁评分：距离的倒数乘以系数，距离越近分数越高
    return 1000.0 / distance;
}

QByteArray Drone::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    
    stream << m_id;
    stream << m_initialPosition;
    stream << m_currentPosition;
    stream << m_velocityX;
    stream << m_velocityY;
    stream << m_startTime;
    stream << m_active;
    stream << m_destroyed;
    stream << static_cast<int>(m_type);
    
    return data;
}

Drone* Drone::deserialize(const QByteArray& data, QObject* parent)
{
    QDataStream stream(data);
    
    int id;
    QPointF initialPos, currentPos;
    double vx, vy;
    qint64 startTime;
    bool active, destroyed;
    int typeInt;
    
    stream >> id >> initialPos >> currentPos >> vx >> vy >> startTime >> active >> destroyed >> typeInt;
    
    DroneType type = static_cast<DroneType>(typeInt);
    Drone* drone = new Drone(id, initialPos, vx, vy, type, parent);
    drone->m_currentPosition = currentPos;
    drone->m_startTime = startTime;
    drone->m_active = active;
    drone->m_destroyed = destroyed;
    
    return drone;
}

// 新增：轨迹预测方法
QPointF Drone::predictPositionAtTime(qint64 futureTimeMs) const
{
    if (!m_active || m_destroyed) {
        return m_currentPosition;
    }
    
    double timeSeconds = (futureTimeMs - m_startTime) / 1000.0;
    
    double x = m_initialPosition.x() + m_velocityX * timeSeconds;
    double y = m_initialPosition.y() + m_velocityY * timeSeconds;
    
    return QPointF(x, y);
}

QPointF Drone::calculateInterceptPoint(QPointF interceptorPos, double interceptorSpeed) const
{
    if (!m_active || m_destroyed || interceptorSpeed <= 0) {
        return m_currentPosition;
    }
    
    // 当前时间
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    QPointF currentDronePos = calculatePositionAtTime(currentTime);
    
    // 使用迭代法求解拦截点
    double bestTime = 0;
    double minError = std::numeric_limits<double>::max();
    
    // 搜索0-30秒内的最佳拦截时间
    for (double t = 0.1; t <= 30.0; t += 0.1) {
        qint64 interceptTime = currentTime + (qint64)(t * 1000);
        QPointF predictedDronePos = predictPositionAtTime(interceptTime);
        
        // 计算拦截器需要的距离和时间
        double dx = predictedDronePos.x() - interceptorPos.x();
        double dy = predictedDronePos.y() - interceptorPos.y();
        double requiredDistance = qSqrt(dx * dx + dy * dy);
        double requiredTime = requiredDistance / interceptorSpeed;
        
        // 计算误差
        double error = qAbs(requiredTime - t);
        if (error < minError) {
            minError = error;
            bestTime = t;
        }
    }
    
    // 返回最佳拦截点
    qint64 interceptTime = currentTime + (qint64)(bestTime * 1000);
    return predictPositionAtTime(interceptTime);
}

double Drone::getTimeToReachRadarCenter() const
{
    if (!m_active || m_destroyed) {
        return -1;
    }
    
    QPointF currentPos = m_currentPosition;
    
    // 如果无人机不朝向雷达中心移动，返回-1
    if (m_velocityX * (-currentPos.x()) + m_velocityY * (-currentPos.y()) <= 0) {
        return -1;
    }
    
    // 计算到达雷达中心的时间
    double speed = getSpeed();
    if (speed <= 0) {
        return -1;
    }
    
    double distance = qSqrt(currentPos.x() * currentPos.x() + currentPos.y() * currentPos.y());
    return distance / speed;
}

double Drone::getMinDistanceToRadarCenter() const
{
    if (!m_active || m_destroyed) {
        return -1;
    }
    
    // 计算无人机轨迹与雷达中心的最小距离
    QPointF currentPos = m_currentPosition;
    double vx = m_velocityX;
    double vy = m_velocityY;
    
    // 点到直线的距离公式的应用
    // 直线方程：r(t) = currentPos + t * velocity
    // 求导数为0的点，即最近距离点
    
    double denominator = vx * vx + vy * vy;
    if (denominator <= 0) {
        return qSqrt(currentPos.x() * currentPos.x() + currentPos.y() * currentPos.y());
    }
    
    double t = -(currentPos.x() * vx + currentPos.y() * vy) / denominator;
    
    // 如果t < 0，说明最近点在过去，返回当前距离
    if (t < 0) {
        return qSqrt(currentPos.x() * currentPos.x() + currentPos.y() * currentPos.y());
    }
    
    // 计算最近点的坐标
    double minX = currentPos.x() + t * vx;
    double minY = currentPos.y() + t * vy;
    
    return qSqrt(minX * minX + minY * minY);
}

bool Drone::willEnterRadarZone(QPointF radarCenter, double radarRadius, qint64 timeWindowMs) const
{
    if (!m_active || m_destroyed) {
        return false;
    }
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 endTime = currentTime + timeWindowMs;
    
    // 检查在时间窗口内是否会进入雷达区域
    for (qint64 t = currentTime; t <= endTime; t += 1000) { // 每秒检查一次
        QPointF futurePos = predictPositionAtTime(t);
        double dx = futurePos.x() - radarCenter.x();
        double dy = futurePos.y() - radarCenter.y();
        double distance = qSqrt(dx * dx + dy * dy);
        if (distance <= radarRadius) {
            return true;
        }
    }
    
    return false;
}

// 新轨迹系统实现
void Drone::initializeTrajectory()
{
    // 计算总距离
    QPointF direction = m_targetPos - m_startPos;
    m_totalDistance = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
    
    // 初始化速度向量（用于兼容旧系统）
    if (m_totalDistance > 0) {
        m_velocityX = (direction.x() / m_totalDistance) * m_startSpeed;
        m_velocityY = (direction.y() / m_totalDistance) * m_startSpeed;
    }
    
    // 为弧形轨迹计算控制点
    if (m_trajectoryType == TrajectoryType::Curved) {
        // 在起点和终点之间创建一个偏移的控制点，形成弧形
        QPointF midPoint = (m_startPos + m_targetPos) / 2;
        QPointF perpendicular(-direction.y(), direction.x()); // 垂直向量
        perpendicular = perpendicular / qSqrt(perpendicular.x() * perpendicular.x() + perpendicular.y() * perpendicular.y());

        // 偏移距离为总距离的1.2到1.8倍，随机选择方向
        double offset = m_totalDistance * (1.2 + QRandomGenerator::global()->generateDouble() * 0.6);
        int direction_sign = QRandomGenerator::global()->bounded(2) ? 1 : -1;
        m_controlPoint = midPoint + perpendicular * offset * direction_sign;
    }
}

QPointF Drone::calculateBezierPoint(double t) const
{
    // 二次贝塞尔曲线：B(t) = (1-t)²P0 + 2(1-t)tP1 + t²P2
    double oneMinusT = 1.0 - t;
    return oneMinusT * oneMinusT * m_startPos + 
           2 * oneMinusT * t * m_controlPoint + 
           t * t * m_targetPos;
}

QPointF Drone::calculateBezierTangent(double t) const
{
    // 贝塞尔曲线的切线：B'(t) = 2(1-t)(P1-P0) + 2t(P2-P1)
    double oneMinusT = 1.0 - t;
    return 2 * oneMinusT * (m_controlPoint - m_startPos) + 
           2 * t * (m_targetPos - m_controlPoint);
}

double Drone::calculateCurrentSpeedForProgress(double progress) const
{
    if (m_speedType == SpeedType::Constant) {
        return m_startSpeed;
    } else {
        // 均匀变速：线性插值，确保最小速度不低于10m/s
        double currentSpeed = m_startSpeed + (m_endSpeed - m_startSpeed) * progress;
        return qMax(10.0, currentSpeed); // 保证最小速度10m/s
    }
}
