#ifndef STATISTICSMANAGER_H
#define STATISTICSMANAGER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QPointF>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include "Drone.h"

// 统计事件类型
enum class EventType {
    DroneSpawned,
    DroneDestroyed,
    DroneEscaped,        // 新增：无人机逃脱（出界）
    StrikeExecuted,
    InterceptExecuted,
    HighThreatDetected
};

// 统计事件记录
struct StatisticsEvent {
    qint64 timestamp;
    EventType type;
    int droneId;
    QPointF position;
    double value; // 威胁值、爆炸半径等
    QString details;
    
    StatisticsEvent(EventType t, int id = -1, QPointF pos = QPointF(), double val = 0.0, const QString& det = "")
        : timestamp(QDateTime::currentMSecsSinceEpoch()), type(t), droneId(id), position(pos), value(val), details(det) {}
};

// 统计数据结构
struct DefenseStatistics {
    int totalDronesSpawned = 0;
    int totalDronesDestroyed = 0;
    int totalDronesEscaped = 0;  // 新增：逃脱无人机数量
    int totalStrikesExecuted = 0;
    int totalInterceptsExecuted = 0;
    double totalThreatNeutralized = 0.0;
    double averageResponseTime = 0.0;
    double defenseEfficiency = 0.0; // 摧毁率
    
    // 按无人机类型统计（已废弃，现在统一类型）
    // int typeADestroyed = 0;
    // int typeBDestroyed = 0;
    // int typeCDestroyed = 0;
    
    // 威胁等级统计
    int highThreatEvents = 0;
    double maxThreatLevel = 0.0;
};

class StatisticsManager : public QObject
{
    Q_OBJECT

public:
    explicit StatisticsManager(QObject *parent = nullptr);
    
    // 事件记录
    void recordDroneSpawned(int droneId, DroneType type, QPointF position);
    void recordDroneDestroyed(int droneId, DroneType type, QPointF position, double threatValue);
    void recordDroneEscaped(int droneId, DroneType type, QPointF position);  // 新增：记录逃脱
    void recordStrikeExecuted(QPointF position, double radius, int dronesDestroyed);
    void recordInterceptExecuted(int droneId, QPointF interceptPoint, double threatValue);
    void recordHighThreatDetected(int droneId, double threatLevel);
    
    // 统计数据获取
    DefenseStatistics getCurrentStatistics() const;
    QList<StatisticsEvent> getRecentEvents(int minutes = 10) const;
    QList<StatisticsEvent> getEventsByType(EventType type) const;
    
    // 数据导出
    QString generateReport() const;
    bool exportToJson(const QString& filename) const;
    bool exportToCsv(const QString& filename) const;
    
    // 实时分析
    double calculateCurrentThreatLevel() const;
    double getDefenseEfficiency() const;
    QString getBestPerformanceMetrics() const;
    
    // 数据清理
    void clearOldEvents(int daysOld = 7);
    void resetStatistics();

signals:
    void statisticsUpdated(const DefenseStatistics& stats);
    void reportGenerated(const QString& report);
    void highActivityDetected(int eventsPerMinute);

private slots:
    void updateRealTimeStats();

private:
    QList<StatisticsEvent> m_events;
    DefenseStatistics m_statistics;
    QTimer* m_updateTimer;
    qint64 m_sessionStartTime;
    
    void updateStatistics();
    QJsonObject eventToJson(const StatisticsEvent& event) const;
    StatisticsEvent jsonToEvent(const QJsonObject& json) const;
};

#endif // STATISTICSMANAGER_H 