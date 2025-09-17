#ifndef DRONEMANAGER_H
#define DRONEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QPointF>
#include <QRandomGenerator>
#include "Drone.h"

class DroneManager : public QObject
{
    Q_OBJECT

public:
    explicit DroneManager(double squareSize = 2000.0, QObject *parent = nullptr);
    ~DroneManager();
    
    // 无人机管理
    void addDrone(int id, QPointF initialPos, double vx, double vy, DroneType type = DroneType::Standard);
    void addDroneWithTrajectory(int id, QPointF startPos, QPointF endPos, 
                               TrajectoryType trajectory = TrajectoryType::Linear,
                               SpeedType speedType = SpeedType::Constant,
                               double startSpeed = 50.0, double endSpeed = -1,
                               DroneType type = DroneType::Standard);
    void removeDrone(int id);
    void removeAllDrones();
    
    // 自动生成无人机
    void generateRandomDrone();
    void startAutoGeneration(int intervalMs = 5000); // 每5秒生成一个
    void stopAutoGeneration();
    bool isAutoGenerationActive() const;
    int getGenerationInterval() const;
    
    // 威胁评估和打击
    QList<Drone*> getThreatSortedDrones() const;
    QList<Drone*> getThreatSortedDronesInRadar(QPointF radarCenter, double radarRadius) const;
    QList<Drone*> getDronesInStrikeRange(QPointF center, double radius) const;
    void strikeTarget(QPointF center, double radius);
    double calculateTotalThreat(const QList<Drone*>& drones) const;
    
    // 智能打击算法
    QPointF findOptimalStrikePoint(double strikeRadius, double searchRadius) const;
    QList<Drone*> getDronesInRadarRange(QPointF radarCenter, double radarRadius) const;
    
    // 新增：高级威胁评估和智能拦截
    double calculateAdvancedThreatScore(const Drone* drone, QPointF radarCenter = QPointF(0, 0)) const;
    QList<Drone*> getAdvancedThreatSortedDrones(QPointF radarCenter = QPointF(0, 0)) const;
    QPointF findOptimalInterceptPoint(const Drone* targetDrone, double interceptorSpeed = 200.0) const;
    QList<Drone*> getPriorityTargets(QPointF radarCenter, double radarRadius, int maxTargets = 5) const;
    bool shouldEngageTarget(const Drone* drone, QPointF radarCenter, double radarRadius) const;
    
    // 获取无人机信息
    QList<Drone*> getAllDrones() const { return m_drones; }
    QList<Drone*> getActiveDrones() const;
    Drone* getDroneById(int id) const;
    
    // 区域设置
    void setSquareSize(double size) { m_squareSize = size; }
    double getSquareSize() const { return m_squareSize; }
    
    // 更新循环
    void startUpdateLoop(int intervalMs = 100); // 每100ms更新一次
    void stopUpdateLoop();

signals:
    void droneAdded(int droneId);
    void droneRemoved(int droneId);
    void dronePositionUpdated(int droneId, QPointF position);
    void droneDestroyed(int droneId);
    void droneEscaped(int droneId);  // 新增：无人机逃脱信号
    void strikeExecuted(QPointF center, double radius, int destroyedCount);
    
    // 新增：高级警报信号
    void highPriorityThreatDetected(int droneId, double threatScore);
    void interceptRecommendation(int droneId, QPointF interceptPoint, double timeToIntercept);

private:
    QTimer* m_updateTimer;
    QTimer* m_generationTimer;
    QList<Drone*> m_drones;
    double m_squareSize;
    int m_nextDroneId;
    int m_generationInterval;
    QRandomGenerator* m_randomGenerator;
    
    // 私有辅助方法
    DroneType generateRandomDroneType();
    QPointF generateRandomEdgePosition();
    QPointF generateRandomTargetPosition();
    QPointF generateRandomVelocity(double minSpeed, double maxSpeed);
    QPointF generateRandomVelocityTowardRadar(const QPointF& fromPosition, double minSpeed, double maxSpeed);
    int generateUniqueId();
    // 在DroneManager类中添加以下声明
private:
    QPointF generateRandomVelocityWithVariation(double minSpeed, double maxSpeed);
    void applyRandomVelocityChange(Drone* drone);

private:
    QPointF m_radarCenter; // 雷达中心位置
private slots:
    void updateAllDrones();
    void onDroneOutOfBounds(int droneId);
    void onDronePositionUpdated(int droneId, QPointF position);
    void onDroneDestroyed(int droneId);
};

#endif // DRONEMANAGER_H
