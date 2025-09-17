#include "WeaponStrategy.h"
#include "DroneManager.h"
#include <QDebug>
#include <QtMath>
#include <algorithm>
#include <limits>

WeaponStrategy::WeaponStrategy(DroneManager* droneManager, QObject *parent)
    : QObject(parent)
    , m_droneManager(droneManager)
    , m_lastFireTime(0)
    , m_autoFireEnabled(false)
{
    initializeStrategies();
    
    // 默认策略：激光威胁优先
    setCurrentStrategy(WeaponType::Laser, TargetingStrategy::ThreatPriority);
    
    // 冷却计时器
    m_cooldownTimer = new QTimer(this);
    m_cooldownTimer->setSingleShot(true);
    connect(m_cooldownTimer, &QTimer::timeout, this, &WeaponStrategy::onCooldownComplete);
    
    // 自动开火计时器
    m_autoFireTimer = new QTimer(this);
    connect(m_autoFireTimer, &QTimer::timeout, this, &WeaponStrategy::onAutoFireTimer);
}

void WeaponStrategy::initializeStrategies()
{
    m_strategies.clear();
    
    // 策略1：激光武器 - 威胁优先
    WeaponConfig laserThreat;
    laserThreat.type = WeaponType::Laser;
    laserThreat.strategy = TargetingStrategy::ThreatPriority;
    laserThreat.cooldownTime = 1.5;  // 减少激光冷却时间
    laserThreat.range = 800.0;
    laserThreat.radius = 35.0;       // 激光单体打击半径（更小，通常只能击中一个无人机）
    laserThreat.name = "激光单体打击";
    m_strategies.append(laserThreat);
    
    // 策略2：导弹武器 - 威胁优先  
    WeaponConfig missileThreat;
    missileThreat.type = WeaponType::Missile;
    missileThreat.strategy = TargetingStrategy::ThreatPriority;
    missileThreat.cooldownTime = 0.8;  // 进一步减少导弹冷却时间：从1.2秒改为0.8秒
    missileThreat.range = 800.0;
    missileThreat.radius = 150.0;      // 导弹爆炸半径
    missileThreat.name = "导弹范围打击";
    m_strategies.append(missileThreat);
    
    // 策略3：激光武器 - 时间优先
    WeaponConfig laserTime;
    laserTime.type = WeaponType::Laser;
    laserTime.strategy = TargetingStrategy::TimePriority;
    laserTime.cooldownTime = 1.5;  // 减少激光冷却时间
    laserTime.range = 800.0;
    laserTime.radius = 80.0;
    laserTime.name = "激光-时间优先";
    m_strategies.append(laserTime);
    
    // 策略4：导弹武器 - 时间优先
    WeaponConfig missileTime;
    missileTime.type = WeaponType::Missile;
    missileTime.strategy = TargetingStrategy::TimePriority;
    missileTime.cooldownTime = 0.8;  // 进一步减少导弹冷却时间：从1.2秒改为0.8秒
    missileTime.range = 800.0;
    missileTime.radius = 150.0;
    missileTime.name = "导弹-时间优先";
    m_strategies.append(missileTime);
}

void WeaponStrategy::setCurrentStrategy(WeaponType type, TargetingStrategy strategy)
{
    for (const WeaponConfig& config : m_strategies) {
        if (config.type == type && config.strategy == strategy) {
            m_currentConfig = config;
            emit strategyChanged(m_currentConfig);
            qDebug() << "切换武器策略:" << m_currentConfig.name;
            return;
        }
    }
}

bool WeaponStrategy::canFire() const
{
    if (m_lastFireTime == 0) return true;
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    double elapsedSeconds = (currentTime - m_lastFireTime) / 1000.0;
    return elapsedSeconds >= m_currentConfig.cooldownTime;
}

double WeaponStrategy::getTimeUntilReady() const
{
    if (canFire()) return 0.0;
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    double elapsedSeconds = (currentTime - m_lastFireTime) / 1000.0;
    double remaining = m_currentConfig.cooldownTime - elapsedSeconds;
    return qMax(0.0, remaining); // 确保不返回负数
}

QString WeaponStrategy::getStatusText() const
{
    return m_currentConfig.name;  // 只返回策略名称，不显示状态
}

bool WeaponStrategy::executeStrike(QPointF radarCenter, double radarRadius)
{
    if (!canFire()) {
        qDebug() << "武器冷却中，无法开火";
        return false;
    }
    
    QPointF target;
    
    // 根据策略选择目标
    if (m_currentConfig.strategy == TargetingStrategy::ThreatPriority) {
        target = findThreatPriorityTarget(m_currentConfig.type, radarCenter, radarRadius);
    } else {
        target = findTimePriorityTarget(m_currentConfig.type, radarCenter, radarRadius);
    }
    
    // 检查目标是否有效
    if (target.isNull()) {
        qDebug() << "未找到有效目标";
        return false;
    }
    
    // 执行打击
    m_droneManager->strikeTarget(target, m_currentConfig.radius);
    
    // 开始冷却
    m_lastFireTime = QDateTime::currentMSecsSinceEpoch();
    m_cooldownTimer->start(static_cast<int>(m_currentConfig.cooldownTime * 1000));
    
    emit weaponFired(target, m_currentConfig.radius, m_currentConfig.type);
    
    qDebug() << "执行" << m_currentConfig.name << "打击，目标:" << target;
    return true;
}

QPointF WeaponStrategy::findThreatPriorityTarget(WeaponType type, QPointF radarCenter, double radarRadius)
{
    if (type == WeaponType::Laser) {
        return findLaserThreatTarget(radarCenter, radarRadius);
    } else {
        return findMissileThreatTarget(radarCenter, radarRadius);
    }
}

QPointF WeaponStrategy::findTimePriorityTarget(WeaponType type, QPointF radarCenter, double radarRadius)
{
    if (type == WeaponType::Laser) {
        return findLaserTimeTarget(radarCenter, radarRadius);
    } else {
        return findMissileTimeTarget(radarCenter, radarRadius);
    }
}

QPointF WeaponStrategy::findLaserThreatTarget(QPointF radarCenter, double radarRadius)
{
    // 激光威胁优先：选择威胁值最高的单个目标
    QList<Drone*> radarDrones = m_droneManager->getDronesInRadarRange(radarCenter, radarRadius);
    
    if (radarDrones.isEmpty()) {
        return QPointF();
    }
    
    // 按威胁值排序
    std::sort(radarDrones.begin(), radarDrones.end(), 
        [](const Drone* a, const Drone* b) {
            return a->getThreatScore() > b->getThreatScore();
        });
    
    return radarDrones.first()->getCurrentPosition();
}

QPointF WeaponStrategy::findLaserTimeTarget(QPointF radarCenter, double radarRadius)
{
    // 激光时间优先：选择最快离开雷达区域的高威胁目标
    QList<Drone*> radarDrones = m_droneManager->getDronesInRadarRange(radarCenter, radarRadius);
    
    if (radarDrones.isEmpty()) {
        return QPointF();
    }
    
    Drone* bestTarget = nullptr;
    double minTimeToLeave = std::numeric_limits<double>::max();
    double minThreatThreshold = 3.0; // 降低最小威胁阈值：从5.0降到3.0
    
    for (Drone* drone : radarDrones) {
        if (drone->getThreatScore() < minThreatThreshold) continue;
        
        double timeToLeave = calculateTimeToLeaveRadar(drone, radarCenter, radarRadius);
        if (timeToLeave > 0 && timeToLeave < minTimeToLeave) {
            minTimeToLeave = timeToLeave;
            bestTarget = drone;
        }
    }
    
    if (bestTarget) {
        return bestTarget->getCurrentPosition();
    }
    
    // 如果没有找到即将离开的目标，回退到威胁优先
    return findLaserThreatTarget(radarCenter, radarRadius);
}

QPointF WeaponStrategy::findMissileThreatTarget(QPointF radarCenter, double radarRadius)
{
    // 导弹威胁优先：更激进的目标选择策略 - 优化版
    QList<Drone*> radarDrones = m_droneManager->getDronesInRadarRange(radarCenter, radarRadius);
    
    if (radarDrones.isEmpty()) {
        return QPointF();
    }
    
    // 策略1：优先使用当前位置的优化群体打击点（速度最快）
    QPointF optimalPoint = m_droneManager->findOptimalStrikePoint(m_currentConfig.radius, radarRadius);
    
    // 验证优化点是否有效（不为原点或有实际目标）
    QList<Drone*> targetsAtOptimal = m_droneManager->getDronesInStrikeRange(optimalPoint, m_currentConfig.radius);
    if (!targetsAtOptimal.isEmpty()) {
        qDebug() << "导弹威胁优先: 使用优化群体打击点" << optimalPoint << "目标数" << targetsAtOptimal.size();
        return optimalPoint;
    }
    
    // 策略2：快速直接打击威胁值最高的目标（无需复杂计算）
    Drone* highestThreatDrone = nullptr;
    double maxThreat = 0;
    
    for (Drone* drone : radarDrones) {
        double threat = drone->getThreatScore();
        if (threat > maxThreat) {
            maxThreat = threat;
            highestThreatDrone = drone;
        }
    }
    
    if (highestThreatDrone && maxThreat >= 0.5) { // 降低威胁阈值
        QPointF targetPos = highestThreatDrone->getCurrentPosition();
        qDebug() << "导弹威胁优先: 直接打击最高威胁目标" << targetPos << "威胁值" << maxThreat;
        return targetPos;
    }
    
    // 策略3：如果没有高威胁目标，直接打击第一个目标（确保开火）
    if (!radarDrones.isEmpty()) {
        QPointF targetPos = radarDrones.first()->getCurrentPosition();
        qDebug() << "导弹威胁优先: 打击第一个有效目标" << targetPos;
        return targetPos;
    }
    
    return QPointF();
}

QPointF WeaponStrategy::findMissileTimeTarget(QPointF radarCenter, double radarRadius)
{
    // 导弹时间优先：简化优化版本 - 大幅提升响应速度
    
    // 1. 快速获取紧急目标（扩展时间窗口）
    QList<Drone*> urgentDrones = getDronesWithTimeToLeave(radarCenter, radarRadius, 20.0); // 扩展到20秒
    
    if (urgentDrones.isEmpty()) {
        // 没有紧急目标，快速回退到威胁优先
        return findMissileThreatTarget(radarCenter, radarRadius);
    }
    
    // 2. 简化版：直接选择威胁最高的紧急目标并预测位置
    Drone* primaryTarget = nullptr;
    double maxThreat = 0;
    for (Drone* drone : urgentDrones) {
        if (drone->getThreatScore() > maxThreat) {
            maxThreat = drone->getThreatScore();
            primaryTarget = drone;
        }
    }
    
    if (!primaryTarget) {
        return findMissileThreatTarget(radarCenter, radarRadius);
    }
    
    // 3. 简化预测：使用0.5秒后的预测位置（减少计算延迟）
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 predictTime = currentTime + 500; // 0.5秒后预测（减少计算量）
    QPointF predictedPos = primaryTarget->predictPositionAtTime(predictTime);
            
    // 4. 快速验证：检查预测位置是否仍在雷达范围内
    double distFromCenter = qSqrt(predictedPos.x() * predictedPos.x() + predictedPos.y() * predictedPos.y());
    if (distFromCenter > radarRadius) {
        // 如果预测位置超出雷达范围，使用当前位置
        predictedPos = primaryTarget->getCurrentPosition();
    }
    
    // 5. 快速群体检查：在预测位置附近查找其他目标
    QList<Drone*> nearbyTargets;
            for (Drone* drone : urgentDrones) {
        QPointF dronePos = drone->predictPositionAtTime(predictTime);
        double distToTarget = qSqrt(qPow(dronePos.x() - predictedPos.x(), 2) + 
                                   qPow(dronePos.y() - predictedPos.y(), 2));
        if (distToTarget <= m_currentConfig.radius) {
            nearbyTargets.append(drone);
                }
            }
            
    qDebug() << "时间优先导弹打击: 预测打击点" << predictedPos 
             << "主要目标威胁值" << maxThreat 
             << "附近目标数" << nearbyTargets.size();
    
    return predictedPos;
}

double WeaponStrategy::calculateTimeToLeaveRadar(Drone* drone, QPointF radarCenter, double radarRadius)
{
    QPointF currentPos = drone->getCurrentPosition();
    double vx = drone->getVelocityX();
    double vy = drone->getVelocityY();
    
    // 如果无人机静止或速度很小
    if (qAbs(vx) < 0.1 && qAbs(vy) < 0.1) {
        return std::numeric_limits<double>::max();
    }
    
    // 计算无人机何时离开雷达圆形区域
    // 使用二次方程求解
    double dx = currentPos.x() - radarCenter.x();
    double dy = currentPos.y() - radarCenter.y();
    
    double a = vx * vx + vy * vy;
    double b = 2 * (dx * vx + dy * vy);
    double c = dx * dx + dy * dy - radarRadius * radarRadius;
    
    double discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        // 无解，无人机不会离开雷达区域
        return std::numeric_limits<double>::max();
    }
    
    double t1 = (-b + qSqrt(discriminant)) / (2 * a);
    double t2 = (-b - qSqrt(discriminant)) / (2 * a);
    
    // 选择正数且较小的解
    double timeToLeave = std::numeric_limits<double>::max();
    if (t1 > 0) timeToLeave = qMin(timeToLeave, t1);
    if (t2 > 0) timeToLeave = qMin(timeToLeave, t2);
    
    return timeToLeave;
}

QList<Drone*> WeaponStrategy::getDronesWithTimeToLeave(QPointF radarCenter, double radarRadius, double maxTime)
{
    QList<Drone*> urgentDrones;
    QList<Drone*> allDrones = m_droneManager->getDronesInRadarRange(radarCenter, radarRadius);
    
    for (Drone* drone : allDrones) {
        double timeToLeave = calculateTimeToLeaveRadar(drone, radarCenter, radarRadius);
        if (timeToLeave <= maxTime && timeToLeave > 0) {
            urgentDrones.append(drone);
        }
    }
    
    return urgentDrones;
}

QList<WeaponConfig> WeaponStrategy::getAllStrategies() const
{
    return m_strategies;
}

void WeaponStrategy::setAutoFire(bool enabled)
{
    m_autoFireEnabled = enabled;
    if (enabled) {
        m_autoFireTimer->start(100); // 适中的频率：100ms检查间隔
        qDebug() << "自动开火模式启用 - 优化响应模式 (100ms检查间隔)";
    } else {
        m_autoFireTimer->stop();
        qDebug() << "自动开火模式关闭";
    }
}

void WeaponStrategy::onAutoFireTimer()
{
    if (!m_autoFireEnabled) return;
    
    // 如果武器就绪，尝试自动开火
    if (canFire()) {
        // 检查是否有目标 - 使用更积极的开火条件
        QList<Drone*> activeDrones = m_droneManager->getActiveDrones();
        if (!activeDrones.isEmpty()) {
            double radarRadius = 800.0; // 可以从外部设置
            QPointF radarCenter(0, 0);
            
            // 检查雷达范围内是否有目标
            QList<Drone*> radarDrones = m_droneManager->getDronesInRadarRange(radarCenter, radarRadius);
            
            if (!radarDrones.isEmpty()) {
                // 极低威胁阈值 - 几乎任何目标都开火
                double autoFireThreshold = 0.1; // 威胁值超过0.1就开火（极极低阈值）
                bool hasValidTarget = false;
                
                for (Drone* drone : radarDrones) {
                    if (drone->getThreatScore() >= autoFireThreshold) {
                        hasValidTarget = true;
                        break;
                    }
                }
                
                // 如果没有达到威胁阈值的目标，仍然对任何存在的目标开火
                if (!hasValidTarget && !radarDrones.isEmpty()) {
                    hasValidTarget = true; // 强制开火
                    qDebug() << "强制自动开火 - 雷达范围内有" << radarDrones.size() << "个目标";
                }
                
                if (hasValidTarget) {
                    bool success = executeStrike(radarCenter, radarRadius);
                    if (success) {
                        qDebug() << "自动开火成功 - 策略:" << m_currentConfig.name;
                    } else {
                        qDebug() << "自动开火失败 - 未找到有效目标位置";
                    }
                }
            }
        }
    } else {
        // 添加调试信息显示武器状态
        double remainingTime = getTimeUntilReady();
        if (remainingTime > 0) {
            qDebug() << "武器冷却中，剩余时间:" << remainingTime << "秒";
        }
    }
}

void WeaponStrategy::onCooldownComplete()
{
    emit cooldownComplete();
    qDebug() << "武器冷却完成:" << m_currentConfig.name;
} 