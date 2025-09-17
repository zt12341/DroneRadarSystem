#ifndef WEAPONSTRATEGY_H
#define WEAPONSTRATEGY_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QPointF>
#include "Drone.h"

class DroneManager;

enum class WeaponType {
    Laser,     // 激光武器 - 单体打击
    Missile    // 导弹武器 - 群体打击
};

enum class TargetingStrategy {
    ThreatPriority,  // 威胁优先算法
    TimePriority     // 时间优先算法 
};

struct WeaponConfig {
    WeaponType type;
    TargetingStrategy strategy;
    double cooldownTime;    // 冷却时间（秒）
    double range;          // 武器射程
    double radius;         // 爆炸半径（导弹）或精确度（激光）
    QString name;          // 策略名称
};

class WeaponStrategy : public QObject
{
    Q_OBJECT

public:
    explicit WeaponStrategy(DroneManager* droneManager, QObject *parent = nullptr);
    
    // 策略选择
    void setCurrentStrategy(WeaponType type, TargetingStrategy strategy);
    WeaponConfig getCurrentConfig() const { return m_currentConfig; }
    
    // 武器状态
    bool canFire() const;
    double getTimeUntilReady() const;
    QString getStatusText() const;
    
    // 执行打击
    bool executeStrike(QPointF radarCenter, double radarRadius);
    
    // 自动开火控制
    void setAutoFire(bool enabled);
    bool isAutoFireEnabled() const { return m_autoFireEnabled; }
    
    // 获取所有可用策略
    QList<WeaponConfig> getAllStrategies() const;

signals:
    void weaponFired(QPointF target, double radius, WeaponType type);
    void cooldownComplete();
    void strategyChanged(WeaponConfig config);

private slots:
    void onCooldownComplete();
    void onAutoFireTimer();

private:
    // 策略实现
    QPointF findThreatPriorityTarget(WeaponType type, QPointF radarCenter, double radarRadius);
    QPointF findTimePriorityTarget(WeaponType type, QPointF radarCenter, double radarRadius);
    
    // 激光单体打击
    QPointF findLaserThreatTarget(QPointF radarCenter, double radarRadius);
    QPointF findLaserTimeTarget(QPointF radarCenter, double radarRadius);
    
    // 导弹群体打击
    QPointF findMissileThreatTarget(QPointF radarCenter, double radarRadius);
    QPointF findMissileTimeTarget(QPointF radarCenter, double radarRadius);
    
    // 时间计算辅助函数
    double calculateTimeToLeaveRadar(Drone* drone, QPointF radarCenter, double radarRadius);
    QList<Drone*> getDronesWithTimeToLeave(QPointF radarCenter, double radarRadius, double maxTime);
    
    DroneManager* m_droneManager;
    WeaponConfig m_currentConfig;
    QTimer* m_cooldownTimer;
    QTimer* m_autoFireTimer;
    qint64 m_lastFireTime;
    bool m_autoFireEnabled;
    
    // 预定义的4种策略
    QList<WeaponConfig> m_strategies;
    
    void initializeStrategies();
};

#endif // WEAPONSTRATEGY_H 