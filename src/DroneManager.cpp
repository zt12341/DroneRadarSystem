#include "DroneManager.h"
#include <QDebug>
#include <QtMath>
#include <algorithm>

DroneManager::DroneManager(double squareSize, QObject *parent)
    : QObject(parent)
    , m_squareSize(squareSize)
    , m_nextDroneId(1)
    , m_generationInterval(3000)
    , m_radarCenter(0, 0)// 默认3秒
{
    m_updateTimer = new QTimer(this);
    m_generationTimer = new QTimer(this);
    m_randomGenerator = QRandomGenerator::global();
    
    connect(m_updateTimer, &QTimer::timeout, this, &DroneManager::updateAllDrones);
    connect(m_generationTimer, &QTimer::timeout, this, &DroneManager::generateRandomDrone);
}

DroneManager::~DroneManager()
{
    removeAllDrones();
}

void DroneManager::addDrone(int id, QPointF initialPos, double vx, double vy, DroneType type)
{
    // 检查ID是否已存在
    if (getDroneById(id) != nullptr) {
        qWarning() << "Drone with ID" << id << "already exists";
        return;
    }
    
    Drone* drone = new Drone(id, initialPos, vx, vy, type, this);
    
    connect(drone, &Drone::positionUpdated, 
            this, &DroneManager::onDronePositionUpdated);
    connect(drone, &Drone::droneOutOfBounds, 
            this, &DroneManager::onDroneOutOfBounds);
    connect(drone, &Drone::droneDestroyed, 
            this, &DroneManager::onDroneDestroyed);
    
    m_drones.append(drone);
    emit droneAdded(id);
    
    QString typeStr = "Standard"; // 统一类型
    qDebug() << "Added drone" << id << "type" << typeStr << "at position" << initialPos 
             << "with velocity (" << vx << "," << vy << ")" << "threat level:" << drone->getThreatLevel();
}

void DroneManager::addDroneWithTrajectory(int id, QPointF startPos, QPointF endPos, 
                                         TrajectoryType trajectory, SpeedType speedType,
                                         double startSpeed, double endSpeed, DroneType type)
{
    // 检查ID是否已存在
    if (getDroneById(id) != nullptr) {
        qWarning() << "Drone with ID" << id << "already exists";
        return;
    }
    
    Drone* drone = new Drone(id, startPos, endPos, trajectory, speedType, startSpeed, endSpeed, type, this);
    
    connect(drone, &Drone::positionUpdated, 
            this, &DroneManager::onDronePositionUpdated);
    connect(drone, &Drone::droneOutOfBounds, 
            this, &DroneManager::onDroneOutOfBounds);
    connect(drone, &Drone::droneDestroyed, 
            this, &DroneManager::onDroneDestroyed);
    
    m_drones.append(drone);
    emit droneAdded(id);
    
    QString typeStr = "Standard"; // 统一类型
    QString trajStr = (trajectory == TrajectoryType::Linear) ? "Linear" : "Curved";
    QString speedStr = (speedType == SpeedType::Constant) ? "Constant" : "Variable";
    
    qDebug() << "Added trajectory drone" << id << "type" << typeStr << "from" << startPos 
             << "to" << endPos << "trajectory:" << trajStr << "speed:" << speedStr 
             << "(" << startSpeed << "->" << endSpeed << ")" << "threat level:" << drone->getThreatLevel();
}

void DroneManager::removeDrone(int id)
{
    for (int i = 0; i < m_drones.size(); ++i) {
        if (m_drones[i]->getId() == id) {
            Drone* drone = m_drones.takeAt(i);
            drone->deleteLater();
            emit droneRemoved(id);
            qDebug() << "Removed drone" << id;
            return;
        }
    }
}

void DroneManager::removeAllDrones()
{
    while (!m_drones.isEmpty()) {
        Drone* drone = m_drones.takeFirst();
        emit droneRemoved(drone->getId());
        drone->deleteLater();
    }
}

// 修改generateRandomDrone方法
void DroneManager::generateRandomDrone()
{
    int id = generateUniqueId();
    QPointF startPos = generateRandomEdgePosition();
    
    // 随机选择轨迹类型（80%弧形，20%直线）
    TrajectoryType trajectory = (m_randomGenerator->generateDouble() < 0.8) ?
                               TrajectoryType::Curved : TrajectoryType::Linear;
    
    // 随机选择速度类型（60%变速，40%匀速）
    SpeedType speedType = (m_randomGenerator->generateDouble() < 0.6) ?
                         SpeedType::Accelerating : SpeedType::Constant;
    
    // 生成目标位置（倾向于朝向雷达中心附近）
    QPointF targetPos = generateRandomTargetPosition();
    
    // 统一类型，随机设置速度参数
    DroneType type = DroneType::Standard;
    double startSpeed, endSpeed;
    
    // 所有无人机使用相同的速度范围：30-100 m/s
    startSpeed = 30.0 + m_randomGenerator->generateDouble() * 70.0; // 30-100
    
    if (speedType == SpeedType::Accelerating) {
        // 变速：结束速度为起始速度的0.3-3.0倍，增大变速范围
        double speedMultiplier = 0.3 + m_randomGenerator->generateDouble() * 2.7;
        endSpeed = startSpeed * speedMultiplier;
        endSpeed = qMax(10.0, qMin(endSpeed, 150.0)); // 限制最小10m/s，最大150m/s
    } else {
        endSpeed = startSpeed; // 匀速
    }
    
    addDroneWithTrajectory(id, startPos, targetPos, trajectory, speedType, 
                          startSpeed, endSpeed, type);
}

// 添加新的速度生成方法
QPointF DroneManager::generateRandomVelocityWithVariation(double minSpeed, double maxSpeed)
{
    // 生成随机角度，但偏向朝向雷达中心
    double baseAngle = 0;
    QPointF radarCenter(0, 0);

    // 计算指向雷达中心的角度
    double toCenterAngle = qAtan2(-m_radarCenter.y(), -m_radarCenter.x());

    // 生成偏向雷达中心的角度（±45度范围内）
    double angleVariation = (m_randomGenerator->generateDouble() - 0.5) * M_PI / 2; // ±45度
    double angle = toCenterAngle + angleVariation;

    // 生成随机速度大小
    double speed = minSpeed + m_randomGenerator->generateDouble() * (maxSpeed - minSpeed);

    // 添加小幅随机变化
    double speedVariation = (m_randomGenerator->generateDouble() - 0.5) * 100.0; // ±5单位变化
    speed += speedVariation;

    // 转换为x,y分量
    double vx = speed * qCos(angle);
    double vy = speed * qSin(angle);

    return QPointF(vx, vy);
}

void DroneManager::startAutoGeneration(int intervalMs)
{
    m_generationInterval = intervalMs;
    m_generationTimer->start(intervalMs);
    qDebug() << "Started auto generation with interval" << intervalMs << "ms";
}

void DroneManager::stopAutoGeneration()
{
    m_generationTimer->stop();
    qDebug() << "Stopped auto generation";
}

QList<Drone*> DroneManager::getActiveDrones() const
{
    QList<Drone*> activeDrones;
    for (Drone* drone : m_drones) {
        if (drone->isActive()) {
            activeDrones.append(drone);
        }
    }
    return activeDrones;
}

Drone* DroneManager::getDroneById(int id) const
{
    for (Drone* drone : m_drones) {
        if (drone->getId() == id) {
            return drone;
        }
    }
    return nullptr;
}

void DroneManager::startUpdateLoop(int intervalMs)
{
    m_updateTimer->start(intervalMs);
    qDebug() << "Started update loop with interval" << intervalMs << "ms";
}

void DroneManager::stopUpdateLoop()
{
    m_updateTimer->stop();
    qDebug() << "Stopped update loop";
}

// 修改updateAllDrones方法，添加速度变化逻辑
void DroneManager::updateAllDrones()
{
    QList<int> dronesOutOfBounds;

    for (Drone* drone : m_drones) {
        if (drone->isActive()) {
            // 随机改变速度（有一定概率）
            if (m_randomGenerator->generateDouble() < 0.3) { // 2%的概率每次更新改变速度
                applyRandomVelocityChange(drone);
            }

            drone->updatePosition();

            // 检查是否超出正方形区域
            if (!drone->isInSquareArea(m_squareSize)) {
                dronesOutOfBounds.append(drone->getId());
            }
        }
    }

    // 移除超出边界的无人机
    for (int id : dronesOutOfBounds) {
        onDroneOutOfBounds(id);
    }
}

// 修改applyRandomVelocityChange方法，增加变化频率和幅度
void DroneManager::applyRandomVelocityChange(Drone* drone)
{
    if (!drone) return;

    // 获取当前速度
    double vx = drone->getVelocityX();
    double vy = drone->getVelocityY();
    double currentSpeed = drone->getSpeed();
    double maxSpeed = drone->getMaxSpeed();

    // 增加变化概率到5%
    if (m_randomGenerator->generateDouble() < 0.15) {
        // 增加角度变化幅度到±0.3弧度（约±17度）
        double angleChange = (m_randomGenerator->generateDouble() - 0.5) * 0.3;
        // 增加速度变化幅度到±10单位
        double speedChange = (m_randomGenerator->generateDouble() - 0.5) * 10.0;

        // 计算新角度
        double currentAngle = qAtan2(vy, vx);
        double newAngle = currentAngle + angleChange;

        // 计算新速度，确保在合理范围内
        double newSpeed = qMax(10.0, qMin(maxSpeed, currentSpeed + speedChange));

        // 转换为x,y分量
        double newVx = newSpeed * qCos(newAngle);
        double newVy = newSpeed * qSin(newAngle);

        // 应用变化
        drone->setVelocity(newVx, newVy);

        // 输出调试信息
        qDebug() << "Drone" << drone->getId() << "velocity changed: ("
                 << vx << "," << vy << ") -> (" << newVx << "," << newVy << ")";
    }
}

void DroneManager::onDroneOutOfBounds(int droneId)
{
    qDebug() << "Drone" << droneId << "is out of bounds, escaping...";
    emit droneEscaped(droneId);  // 发送逃脱信号而不是摧毁信号
    removeDrone(droneId);
}

void DroneManager::onDronePositionUpdated(int droneId, QPointF position)
{
    emit dronePositionUpdated(droneId, position);
}

void DroneManager::onDroneDestroyed(int droneId)
{
    emit droneDestroyed(droneId);
    removeDrone(droneId);
}

QList<Drone*> DroneManager::getThreatSortedDrones() const
{
    QList<Drone*> activeDrones = getActiveDrones();
    
    // 按威胁评分排序（从高到低）
    std::sort(activeDrones.begin(), activeDrones.end(), 
              [](const Drone* a, const Drone* b) {
                  return a->getThreatScore() > b->getThreatScore();
              });
    
    return activeDrones;
}

QList<Drone*> DroneManager::getThreatSortedDronesInRadar(QPointF radarCenter, double radarRadius) const
{
    // 只获取在雷达范围内的活跃无人机
    QList<Drone*> radarDrones = getDronesInRadarRange(radarCenter, radarRadius);
    
    // 按威胁值从高到低排序
    std::sort(radarDrones.begin(), radarDrones.end(),
              [](const Drone* a, const Drone* b) {
                  return a->getThreatScore() > b->getThreatScore();
              });
    
    return radarDrones;
}

QList<Drone*> DroneManager::getDronesInStrikeRange(QPointF center, double radius) const
{
    QList<Drone*> dronesInRange;
    
    for (Drone* drone : getActiveDrones()) {
        if (drone->isInStrikeRange(center, radius)) {
            dronesInRange.append(drone);
        }
    }
    
    return dronesInRange;
}

void DroneManager::strikeTarget(QPointF center, double radius)
{
    QList<Drone*> targetDrones = getDronesInStrikeRange(center, radius);
    int destroyedCount = 0;
    
    for (Drone* drone : targetDrones) {
        drone->destroy();
        destroyedCount++;
        qDebug() << "Drone" << drone->getId() << "destroyed by strike";
    }
    
    emit strikeExecuted(center, radius, destroyedCount);
    qDebug() << "Strike executed at" << center << "radius" << radius << "destroyed" << destroyedCount << "drones";
}

double DroneManager::calculateTotalThreat(const QList<Drone*>& drones) const
{
    double totalThreat = 0;
    for (const Drone* drone : drones) {
        totalThreat += drone->getThreatScore();
    }
    return totalThreat;
}

DroneType DroneManager::generateRandomDroneType()
{
    // 所有无人机统一使用Standard类型
    return DroneType::Standard;
}

QList<Drone*> DroneManager::getDronesInRadarRange(QPointF radarCenter, double radarRadius) const
{
    QList<Drone*> dronesInRange;
    
    for (Drone* drone : getActiveDrones()) {
        if (drone->isInRadarRange(radarCenter, radarRadius)) {
            dronesInRange.append(drone);
        }
    }
    
    return dronesInRange;
}

QPointF DroneManager::findOptimalStrikePoint(double strikeRadius, double searchRadius) const
{
    QList<Drone*> radarDrones = getDronesInRadarRange(QPointF(0, 0), searchRadius);
    
    if (radarDrones.isEmpty()) {
        return QPointF(0, 0);
    }
    
    QPointF bestStrikePoint(0, 0);
    double maxTotalThreat = 0;
    
    // 遍历搜索网格，寻找最优打击点
    int gridSize = 20; // 网格密度
    double step = searchRadius * 2 / gridSize;
    
    for (int x = -gridSize/2; x <= gridSize/2; ++x) {
        for (int y = -gridSize/2; y <= gridSize/2; ++y) {
            QPointF testPoint(x * step, y * step);
            
            // 检查是否在雷达范围内
            if (QPointF(testPoint).manhattanLength() > searchRadius) {
                continue;
            }
            
            // 计算此点打击范围内的总威胁值
            QList<Drone*> targetsInRange = getDronesInStrikeRange(testPoint, strikeRadius);
            double totalThreat = calculateTotalThreat(targetsInRange);
            
            if (totalThreat > maxTotalThreat) {
                maxTotalThreat = totalThreat;
                bestStrikePoint = testPoint;
            }
        }
    }
    
    return bestStrikePoint;
}

QPointF DroneManager::generateRandomEdgePosition()
{
    double halfSize = m_squareSize / 2.0;
    
    // 生成位置应该在正方形边缘，但在边界内
    double distance = halfSize;
    
    // 选择四个边中的一个
    int edge = m_randomGenerator->bounded(4);
    double x, y;
    
    int rangeMin = int(-distance);
    int rangeMax = int(distance);
    
    switch (edge) {
        case 0: // 上边
            x = m_randomGenerator->bounded(rangeMin, rangeMax);
            y = -distance;
            break;
        case 1: // 右边
            x = distance;
            y = m_randomGenerator->bounded(rangeMin, rangeMax);
            break;
        case 2: // 下边
            x = m_randomGenerator->bounded(rangeMin, rangeMax);
            y = distance;
            break;
        case 3: // 左边
            x = -distance;
            y = m_randomGenerator->bounded(rangeMin, rangeMax);
            break;
    }
    
    return QPointF(x, y);
}

QPointF DroneManager::generateRandomTargetPosition()
{
    // 确保无人机有合理的穿越轨迹：80%概率朝向对面边界，20%朝向雷达中心附近
    if (m_randomGenerator->generateDouble() < 0.8) {
        // 朝向对面边界，确保穿越运动
        double halfSize = m_squareSize / 2.0;
        
        // 随机选择对面区域的边界
        int targetEdge = m_randomGenerator->bounded(4);
        double x, y;
        
        switch (targetEdge) {
            case 0: // 朝向上边
                x = (m_randomGenerator->generateDouble() - 0.5) * m_squareSize;
                y = -halfSize + m_randomGenerator->generateDouble() * halfSize * 0.3; // 靠近上边界
                break;
            case 1: // 朝向右边
                x = halfSize - m_randomGenerator->generateDouble() * halfSize * 0.3; // 靠近右边界
                y = (m_randomGenerator->generateDouble() - 0.5) * m_squareSize;
                break;
            case 2: // 朝向下边
                x = (m_randomGenerator->generateDouble() - 0.5) * m_squareSize;
                y = halfSize - m_randomGenerator->generateDouble() * halfSize * 0.3; // 靠近下边界
                break;
            case 3: // 朝向左边
                x = -halfSize + m_randomGenerator->generateDouble() * halfSize * 0.3; // 靠近左边界
                y = (m_randomGenerator->generateDouble() - 0.5) * m_squareSize;
                break;
        }
        
        return QPointF(x, y);
    } else {
        // 朝向雷达中心附近（雷达中心±300像素范围内）
        double offsetRange = 300.0;
        double offsetX = (m_randomGenerator->generateDouble() - 0.5) * offsetRange * 2;
        double offsetY = (m_randomGenerator->generateDouble() - 0.5) * offsetRange * 2;
        return QPointF(offsetX, offsetY);
    }
}

QPointF DroneManager::generateRandomVelocity(double minSpeed, double maxSpeed)
{
    // 生成随机角度
    double angle = m_randomGenerator->generateDouble() * 2 * M_PI;

    // 生成随机速度大小
    double speed = minSpeed + m_randomGenerator->generateDouble() * (maxSpeed - minSpeed);

    // 转换为x,y分量
    double vx = speed * qCos(angle);
    double vy = speed * qSin(angle);

    return QPointF(vx, vy);
}

QPointF DroneManager::generateRandomVelocityTowardRadar(const QPointF& fromPosition, double minSpeed, double maxSpeed)
{
    // 生成随机速度大小
    double speed = minSpeed + m_randomGenerator->generateDouble() * (maxSpeed - minSpeed);
    
    // 计算从边缘位置朝向雷达圆形区域内任意点的方向
    // 雷达中心在(0,0)，半径通常为800
    double radarRadius = 800.0; // 假设雷达半径
    
    // 在雷达圆内生成一个随机目标点
    double targetAngle = m_randomGenerator->generateDouble() * 2 * M_PI;
    double targetDistance = m_randomGenerator->generateDouble() * radarRadius * 0.8; // 80%雷达范围内
    QPointF targetPoint(targetDistance * qCos(targetAngle), targetDistance * qSin(targetAngle));
    
    // 计算从起始位置到目标点的方向
    QPointF direction = targetPoint - fromPosition;
    double distance = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
    
    if (distance > 0) {
        // 标准化并应用速度
        direction = QPointF(direction.x() / distance, direction.y() / distance);
        
        // 添加一些随机偏移（±30度）让路径不那么直接
        double currentAngle = qAtan2(direction.y(), direction.x());
        double angleOffset = (m_randomGenerator->generateDouble() - 0.5) * M_PI / 3; // ±30度
        double finalAngle = currentAngle + angleOffset;
        
        return QPointF(speed * qCos(finalAngle), speed * qSin(finalAngle));
    } else {
        // 如果距离为0（不太可能），返回随机速度
        return generateRandomVelocity(minSpeed, maxSpeed);
    }
}

int DroneManager::generateUniqueId()
{
    return m_nextDroneId++;
}

bool DroneManager::isAutoGenerationActive() const
{
    return m_generationTimer->isActive();
}

int DroneManager::getGenerationInterval() const
{
    return m_generationInterval;
}

// 新增：高级威胁评估算法
double DroneManager::calculateAdvancedThreatScore(const Drone* drone, QPointF radarCenter) const
{
    if (!drone || !drone->isActive() || drone->isDestroyed()) {
        return 0.0;
    }
    
    // 基础威胁值
    double baseThreat = drone->getThreatScore();
    
    // 距离因子 (距离越近威胁越大)
    QPointF dronePos = drone->getCurrentPosition();
    double distance = qSqrt((dronePos.x() - radarCenter.x()) * (dronePos.x() - radarCenter.x()) + 
                           (dronePos.y() - radarCenter.y()) * (dronePos.y() - radarCenter.y()));
    double distanceFactor = 1000.0 / (distance + 100.0); // 避免除零
    
    // 速度因子 (速度越快威胁越大)
    double speed = drone->getSpeed();
    double speedFactor = 1.0 + speed / 100.0;
    
    // 轨迹因子 (朝向雷达中心的威胁更大)
    double minDistance = drone->getMinDistanceToRadarCenter();
    double trajectoryFactor = 1.0;
    if (minDistance >= 0 && minDistance < 800) { // 如果会接近雷达中心800像素内
        trajectoryFactor = 2.0 - (minDistance / 800.0); // 1.0-2.0倍
    }
    
    // 时间紧急度因子
    double timeToCenter = drone->getTimeToReachRadarCenter();
    double urgencyFactor = 1.0;
    if (timeToCenter > 0 && timeToCenter < 30.0) { // 30秒内到达
        urgencyFactor = 2.0 - (timeToCenter / 30.0); // 1.0-2.0倍
    }
    
    // 综合威胁评分
    double advancedScore = baseThreat * distanceFactor * speedFactor * trajectoryFactor * urgencyFactor;
    
    return advancedScore;
}

QList<Drone*> DroneManager::getAdvancedThreatSortedDrones(QPointF radarCenter) const
{
    QList<Drone*> activeDrones = getActiveDrones();
    
    // 按高级威胁评分排序（从高到低）
    std::sort(activeDrones.begin(), activeDrones.end(), 
              [this, radarCenter](const Drone* a, const Drone* b) {
                  return calculateAdvancedThreatScore(a, radarCenter) > 
                         calculateAdvancedThreatScore(b, radarCenter);
              });
    
    return activeDrones;
}

QPointF DroneManager::findOptimalInterceptPoint(const Drone* targetDrone, double interceptorSpeed) const
{
    if (!targetDrone || !targetDrone->isActive() || targetDrone->isDestroyed()) {
        return QPointF(0, 0);
    }
    
    // 假设拦截器从雷达中心发射
    QPointF interceptorPos(0, 0);
    
    return targetDrone->calculateInterceptPoint(interceptorPos, interceptorSpeed);
}

QList<Drone*> DroneManager::getPriorityTargets(QPointF radarCenter, double radarRadius, int maxTargets) const
{
    QList<Drone*> candidates = getDronesInRadarRange(radarCenter, radarRadius);
    QList<Drone*> priorityTargets;
    
    // 使用高级威胁评估排序
    std::sort(candidates.begin(), candidates.end(),
              [this, radarCenter](const Drone* a, const Drone* b) {
                  return calculateAdvancedThreatScore(a, radarCenter) > 
                         calculateAdvancedThreatScore(b, radarCenter);
              });
    
    // 选择前maxTargets个目标，并应用优先级过滤
    for (Drone* drone : candidates) {
        if (priorityTargets.size() >= maxTargets) {
            break;
        }
        
        if (shouldEngageTarget(drone, radarCenter, radarRadius)) {
            priorityTargets.append(drone);
            
            // 发送高优先级威胁检测信号
            double threatScore = calculateAdvancedThreatScore(drone, radarCenter);
            if (threatScore > 1000.0) { // 高威胁阈值
                emit const_cast<DroneManager*>(this)->highPriorityThreatDetected(drone->getId(), threatScore);
            }
        }
    }
    
    return priorityTargets;
}

bool DroneManager::shouldEngageTarget(const Drone* drone, QPointF radarCenter, double radarRadius) const
{
    if (!drone || !drone->isActive() || drone->isDestroyed()) {
        return false;
    }
    
    // 基本条件：必须在雷达范围内
    if (!drone->isInRadarRange(radarCenter, radarRadius)) {
        return false;
    }
    
    // 威胁评分必须达到最低阈值
    double threatScore = calculateAdvancedThreatScore(drone, radarCenter);
    if (threatScore < 50.0) { // 最低威胁阈值
        return false;
    }
    
    // 检查是否会进入核心防御区域（雷达半径的50%）
    double coreDefenseRadius = radarRadius * 0.5;
    if (drone->willEnterRadarZone(radarCenter, coreDefenseRadius, 10000)) { // 10秒内
        return true;
    }
    
    // 对于高威胁目标，无论位置都应该拦截
    if (threatScore > 500.0) {
        return true;
    }
    
    // 检查最小接近距离
    double minDistance = drone->getMinDistanceToRadarCenter();
    if (minDistance >= 0 && minDistance < radarRadius * 0.7) {
        return true;
    }
    
    return false;
}
