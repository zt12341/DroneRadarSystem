#include "StatisticsManager.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

StatisticsManager::StatisticsManager(QObject *parent)
    : QObject(parent)
    , m_sessionStartTime(QDateTime::currentMSecsSinceEpoch())
{
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &StatisticsManager::updateRealTimeStats);
    m_updateTimer->start(5000); // 每5秒更新一次统计
}

void StatisticsManager::recordDroneSpawned(int droneId, DroneType type, QPointF position)
{
    QString typeStr = "Standard"; // 统一类型
    StatisticsEvent event(EventType::DroneSpawned, droneId, position, QString("Type: %1 spawned").arg(typeStr));
    m_events.append(event);

    m_statistics.totalDronesSpawned++;
    updateStatistics();

    qDebug() << "Statistics: Drone spawned - ID:" << droneId << "Type:" << typeStr;
}

void StatisticsManager::recordDroneDestroyed(int droneId, DroneType type, QPointF position, double threatValue)
{
    QString typeStr = "Standard"; // 统一类型
    StatisticsEvent event(EventType::DroneDestroyed, droneId, position,
                          QString("Type: %1 destroyed").arg(typeStr));
    m_events.append(event);

    m_statistics.totalDronesDestroyed++;
    m_statistics.totalThreatNeutralized += threatValue;

    // 统一类型，不再按类型分类统计
    // switch (type) 代码已删除，因为现在只有Standard类型

    updateStatistics();
    qDebug() << "Statistics: Drone destroyed - ID:" << droneId << "Threat:" << threatValue;
}

void StatisticsManager::recordDroneEscaped(int droneId, DroneType type, QPointF position)
{
    QString typeStr = "Standard"; // 统一类型
    StatisticsEvent event(EventType::DroneEscaped, droneId, position,
                          QString("Type: %1 escaped").arg(typeStr));
    m_events.append(event);

    m_statistics.totalDronesEscaped++;
    updateStatistics();

    qDebug() << "Statistics: Drone escaped - ID:" << droneId << "Type:" << typeStr;
}

void StatisticsManager::recordStrikeExecuted(QPointF position, double radius, int dronesDestroyed)
{

    m_statistics.totalStrikesExecuted++;
    updateStatistics();

    qDebug() << "Statistics: Strike executed at" << position << "destroyed" << dronesDestroyed;
}

void StatisticsManager::recordInterceptExecuted(int droneId, QPointF interceptPoint, double threatValue)
{
    StatisticsEvent event(EventType::InterceptExecuted, droneId, interceptPoint,
                          QString("Target ID: %1, Threat: %2").arg(droneId));
    m_events.append(event);

    m_statistics.totalInterceptsExecuted++;
    updateStatistics();

    qDebug() << "Statistics: Intercept executed for drone" << droneId << "threat:" << threatValue;
}

void StatisticsManager::recordHighThreatDetected(int droneId, double threatLevel)
{
    StatisticsEvent event(EventType::HighThreatDetected, droneId, QPointF(),
                          QString("High threat level: %1"));
    m_events.append(event);

    m_statistics.highThreatEvents++;
    if (threatLevel > m_statistics.maxThreatLevel) {
        m_statistics.maxThreatLevel = threatLevel;
    }

    updateStatistics();
    qDebug() << "Statistics: High threat detected - Drone" << droneId << "level:" << threatLevel;
}

DefenseStatistics StatisticsManager::getCurrentStatistics() const
{
    return m_statistics;
}

QList<StatisticsEvent> StatisticsManager::getRecentEvents(int minutes) const
{
    QList<StatisticsEvent> recentEvents;
    qint64 cutoffTime = QDateTime::currentMSecsSinceEpoch() - (minutes * 60 * 1000);

    for (const StatisticsEvent& event : m_events) {
        if (event.timestamp >= cutoffTime) {
            recentEvents.append(event);
        }
    }

    return recentEvents;
}

QList<StatisticsEvent> StatisticsManager::getEventsByType(EventType type) const
{
    QList<StatisticsEvent> filteredEvents;

    for (const StatisticsEvent& event : m_events) {
        if (event.type == type) {
            filteredEvents.append(event);
        }
    }

    return filteredEvents;
}

QString StatisticsManager::generateReport() const
{
    QString report;
    QTextStream stream(&report);

    qint64 sessionDuration = QDateTime::currentMSecsSinceEpoch() - m_sessionStartTime;
    double sessionHours = sessionDuration / (1000.0 * 60.0 * 60.0);
    double sessionMinutes = sessionDuration / (1000.0 * 60.0);

    stream << "===== 无人机雷达仿真系统 - 防御统计报告 =====\n";
    stream << QString("报告生成时间: %1\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    stream << QString("会话时长: %1 小时 (%2 分钟)\n\n").arg(sessionHours, 0, 'f', 2).arg(sessionMinutes, 0, 'f', 1);

    stream << "=== 总体统计 ===\n";
    stream << QString("无人机生成总数: %1\n").arg(m_statistics.totalDronesSpawned);
    stream << QString("无人机摧毁总数: %1\n").arg(m_statistics.totalDronesDestroyed);

    // 计算实时防御效率
    double realEfficiency = 0.0;
    if (m_statistics.totalDronesSpawned > 0) {
        realEfficiency = (double(m_statistics.totalDronesDestroyed) / m_statistics.totalDronesSpawned) * 100.0;
    }
    stream << QString("防御效率: %1% (摧毁/生成比例)\n").arg(realEfficiency, 0, 'f', 1);

    // 计算存活无人机数量
    int survivedDrones = m_statistics.totalDronesSpawned - m_statistics.totalDronesDestroyed - m_statistics.totalDronesEscaped;
    stream << QString("逃脱无人机: %1\n").arg(m_statistics.totalDronesEscaped);
    stream << QString("当前存活无人机: %1\n").arg(survivedDrones > 0 ? survivedDrones : 0);

    stream << QString("执行打击次数: %1\n").arg(m_statistics.totalStrikesExecuted);
    stream << QString("智能拦截次数: %1\n").arg(m_statistics.totalInterceptsExecuted);
    stream << QString("中和威胁总值: %1\n").arg(m_statistics.totalThreatNeutralized, 0, 'f', 1);

    // 计算平均效率指标
    if (m_statistics.totalStrikesExecuted > 0) {
        double avgDestroysPerStrike = double(m_statistics.totalDronesDestroyed) / m_statistics.totalStrikesExecuted;
        stream << QString("平均每次打击摧毁: %1 架\n").arg(avgDestroysPerStrike, 0, 'f', 1);
    }

    if (sessionMinutes > 0) {
        double dronesPerMinute = double(m_statistics.totalDronesSpawned) / sessionMinutes;
        double destroysPerMinute = double(m_statistics.totalDronesDestroyed) / sessionMinutes;
        stream << QString("平均生成速率: %1 架/分钟\n").arg(dronesPerMinute, 0, 'f', 1);
        stream << QString("平均摧毁速率: %1 架/分钟\n").arg(destroysPerMinute, 0, 'f', 1);
    }
    stream << "\n";

    stream << "=== 无人机统计 ===\n";
    stream << QString("标准类型无人机: %1\n").arg(m_statistics.totalDronesDestroyed);
    stream << "注：所有无人机现在统一为标准类型，威胁值基于距离计算\n";
    stream << "\n";

    // 威胁处理效率
    if (m_statistics.highThreatEvents > 0) {
        double threatHandlingEfficiency = (double(m_statistics.totalInterceptsExecuted) / m_statistics.highThreatEvents) * 100.0;
        stream << QString("威胁处理效率: %1% (拦截/检测比例)\n").arg(threatHandlingEfficiency, 0, 'f', 1);
    }

    // 平均响应时间（转换为更易读的格式）
    if (m_statistics.averageResponseTime > 0) {
        double responseTimeSeconds = m_statistics.averageResponseTime / 1000.0;
        stream << QString("平均响应时间: %1 秒\n").arg(responseTimeSeconds, 0, 'f', 2);
    }
    stream << "\n";

    // 最近事件摘要
    QList<StatisticsEvent> recentEvents = getRecentEvents(10);
    stream << QString("=== 最近10分钟事件摘要 (共%1个事件) ===\n").arg(recentEvents.size());
    if (recentEvents.isEmpty()) {
        stream << "最近10分钟内无事件记录\n";
    } else {
        for (const StatisticsEvent& event : recentEvents) {
            QString eventTypeStr;
            switch (event.type) {
            case EventType::DroneSpawned: eventTypeStr = "无人机生成"; break;
            case EventType::DroneDestroyed: eventTypeStr = "无人机摧毁"; break;
            case EventType::DroneEscaped: eventTypeStr = "无人机逃脱"; break;
            case EventType::StrikeExecuted: eventTypeStr = "执行打击"; break;
            case EventType::InterceptExecuted: eventTypeStr = "智能拦截"; break;
            case EventType::HighThreatDetected: eventTypeStr = "高威胁检测"; break;
            }

            QDateTime eventTime = QDateTime::fromMSecsSinceEpoch(event.timestamp);
            stream << QString("%1 - %2: %3\n")
                          .arg(eventTime.toString("hh:mm:ss"))
                          .arg(eventTypeStr)
                          .arg(event.details);
        }
    }

    stream << "\n=== 报告结束 ===\n";
    stream << QString("数据统计时间: %1 到 %2\n")
                  .arg(QDateTime::fromMSecsSinceEpoch(m_sessionStartTime).toString("yyyy-MM-dd hh:mm:ss"))
                  .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    emit const_cast<StatisticsManager*>(this)->reportGenerated(report);
    return report;
}

bool StatisticsManager::exportToJson(const QString& filename) const
{
    QJsonArray eventsArray;
    for (const StatisticsEvent& event : m_events) {
        eventsArray.append(eventToJson(event));
    }

    QJsonObject statsObj;
    statsObj["totalDronesSpawned"] = m_statistics.totalDronesSpawned;
    statsObj["totalDronesDestroyed"] = m_statistics.totalDronesDestroyed;
    statsObj["defenseEfficiency"] = m_statistics.defenseEfficiency;
    statsObj["totalStrikesExecuted"] = m_statistics.totalStrikesExecuted;
    statsObj["totalInterceptsExecuted"] = m_statistics.totalInterceptsExecuted;
    statsObj["totalThreatNeutralized"] = m_statistics.totalThreatNeutralized;

    QJsonObject rootObj;
    rootObj["sessionStartTime"] = m_sessionStartTime;
    rootObj["statistics"] = statsObj;
    rootObj["events"] = eventsArray;

    QJsonDocument doc(rootObj);

    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        return true;
    }
    return false;
}

bool StatisticsManager::exportToCsv(const QString& filename) const
{
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);

        // CSV 头
        stream << "Timestamp,EventType,DroneID,PositionX,PositionY,Details\n";

        // 数据行
        for (const StatisticsEvent& event : m_events) {
            QString eventTypeStr;
            switch (event.type) {
            case EventType::DroneSpawned: eventTypeStr = "DroneSpawned"; break;
            case EventType::DroneDestroyed: eventTypeStr = "DroneDestroyed"; break;
            case EventType::DroneEscaped: eventTypeStr = "DroneEscaped"; break;
            case EventType::StrikeExecuted: eventTypeStr = "StrikeExecuted"; break;
            case EventType::InterceptExecuted: eventTypeStr = "InterceptExecuted"; break;
            case EventType::HighThreatDetected: eventTypeStr = "HighThreatDetected"; break;
            }

            stream << QString("%1,%2,%3,%4,%5,\"%7\"\n")
                          .arg(event.timestamp)
                          .arg(eventTypeStr)
                          .arg(event.droneId)
                          .arg(event.position.x())
                          .arg(event.position.y())
                          .arg(event.details);
        }
        return true;
    }
    return false;
}

double StatisticsManager::calculateCurrentThreatLevel() const
{
    QList<StatisticsEvent> recentThreats = getRecentEvents(5); // 最近5分钟
    double totalThreat = 0.0;
    int threatEvents = 0;

    for (const StatisticsEvent& event : recentThreats) {
        if (event.type == EventType::HighThreatDetected) {
            totalThreat += event.value;
            threatEvents++;
        }
    }

    return threatEvents > 0 ? totalThreat / threatEvents : 0.0;
}

double StatisticsManager::getDefenseEfficiency() const
{
    return m_statistics.defenseEfficiency;
}

QString StatisticsManager::getBestPerformanceMetrics() const
{
    QString metrics;

    // 找出效率最高的时间段
    double bestEfficiency = m_statistics.defenseEfficiency;
    // int bestStreak = 0; // 连续成功拦截次数 (暂未实现)

    // 计算平均响应时间（从威胁检测到摧毁的时间）
    QList<StatisticsEvent> threats = getEventsByType(EventType::HighThreatDetected);
    QList<StatisticsEvent> destructions = getEventsByType(EventType::DroneDestroyed);

    double totalResponseTime = 0.0;
    int responsePairs = 0;

    for (const StatisticsEvent& threat : threats) {
        for (const StatisticsEvent& destruction : destructions) {
            if (destruction.droneId == threat.droneId && destruction.timestamp > threat.timestamp) {
                totalResponseTime += (destruction.timestamp - threat.timestamp);
                responsePairs++;
                break;
            }
        }
    }

    double avgResponseTime = responsePairs > 0 ? totalResponseTime / (responsePairs * 1000.0) : 0.0; // 转换为秒

    metrics = QString("最佳防御效率: %1%\n平均响应时间: %2秒\n总事件数: %3")
                  .arg(bestEfficiency, 0, 'f', 1)
                  .arg(avgResponseTime, 0, 'f', 2)
                  .arg(m_events.size());

    return metrics;
}

void StatisticsManager::clearOldEvents(int daysOld)
{
    qint64 cutoffTime = QDateTime::currentMSecsSinceEpoch() - (daysOld * 24 * 60 * 60 * 1000);

    m_events.erase(
        std::remove_if(m_events.begin(), m_events.end(),
                       [cutoffTime](const StatisticsEvent& event) {
                           return event.timestamp < cutoffTime;
                       }),
        m_events.end());
}

void StatisticsManager::resetStatistics()
{
    m_events.clear();
    m_statistics = DefenseStatistics();
    m_sessionStartTime = QDateTime::currentMSecsSinceEpoch();
    updateStatistics();
}

void StatisticsManager::updateRealTimeStats()
{
    updateStatistics();

    // 检查高活动度
    QList<StatisticsEvent> recentEvents = getRecentEvents(1); // 最近1分钟
    if (recentEvents.size() > 10) { // 每分钟超过10个事件认为是高活动度
        emit highActivityDetected(recentEvents.size());
    }
}

void StatisticsManager::updateStatistics()
{
    // 计算防御效率 - 考虑逃脱的无人机
    if (m_statistics.totalDronesSpawned > 0) {
        // 防御效率 = 摧毁数量 / (摧毁数量 + 逃脱数量) * 100%
        int completedThreats = m_statistics.totalDronesDestroyed + m_statistics.totalDronesEscaped;
        if (completedThreats > 0) {
            m_statistics.defenseEfficiency = (double(m_statistics.totalDronesDestroyed) / completedThreats) * 100.0;
        } else {
            m_statistics.defenseEfficiency = 0.0;
        }
    }

    // 计算平均响应时间 - 修正为更准确的计算
    QList<StatisticsEvent> threats = getEventsByType(EventType::HighThreatDetected);
    QList<StatisticsEvent> destructions = getEventsByType(EventType::DroneDestroyed);

    if (!threats.isEmpty() && !destructions.isEmpty()) {
        double totalResponseTime = 0.0;
        int validPairs = 0;

        // 对每个高威胁检测事件，找到对应的摧毁事件
        for (const StatisticsEvent& threat : threats) {
            for (const StatisticsEvent& destruction : destructions) {
                if (destruction.droneId == threat.droneId &&
                    destruction.timestamp > threat.timestamp &&
                    (destruction.timestamp - threat.timestamp) < 60000) { // 60秒内有效
                    totalResponseTime += (destruction.timestamp - threat.timestamp);
                    validPairs++;
                    break; // 找到对应的摧毁事件后退出内循环
                }
            }
        }

        if (validPairs > 0) {
            m_statistics.averageResponseTime = totalResponseTime / validPairs; // 毫秒
        } else {
            // 如果没有威胁-摧毁配对，使用简单的平均生存时间
            double totalLifetime = 0.0;
            for (const StatisticsEvent& destruction : destructions) {
                // 找到对应的生成事件
                QList<StatisticsEvent> spawns = getEventsByType(EventType::DroneSpawned);
                for (const StatisticsEvent& spawn : spawns) {
                    if (spawn.droneId == destruction.droneId && spawn.timestamp <= destruction.timestamp) {
                        totalLifetime += (destruction.timestamp - spawn.timestamp);
                        break;
                    }
                }
            }
            if (destructions.size() > 0) {
                m_statistics.averageResponseTime = totalLifetime / destructions.size();
            }
        }
    }

    emit statisticsUpdated(m_statistics);
}

QJsonObject StatisticsManager::eventToJson(const StatisticsEvent& event) const
{
    QJsonObject obj;
    obj["timestamp"] = event.timestamp;
    obj["type"] = static_cast<int>(event.type);
    obj["droneId"] = event.droneId;
    obj["positionX"] = event.position.x();
    obj["positionY"] = event.position.y();
    obj["details"] = event.details;
    return obj;
}

StatisticsEvent StatisticsManager::jsonToEvent(const QJsonObject& json) const
{
    StatisticsEvent event(static_cast<EventType>(json["type"].toInt()),
                          json["droneId"].toInt(),
                          QPointF(json["positionX"].toDouble(), json["positionY"].toDouble()),

                          json["details"].toString());
    event.timestamp = json["timestamp"].toVariant().toLongLong();
    return event;
}
