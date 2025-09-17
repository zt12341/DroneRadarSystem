#ifndef RADARCONFIG_H
#define RADARCONFIG_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QUdpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageBox>

class RadarConfig : public QWidget
{
    Q_OBJECT

public:
    explicit RadarConfig(QWidget *parent = nullptr);
    ~RadarConfig();

private slots:
    void connectToRadar();
    void disconnectFromRadar();
    void sendConfiguration();
    void applyGenerationInterval();
    void applyRadarSettings();
    void requestCurrentSettings();
    void onConfigResponse();

private:
    void setupUI();
    void setupStyles();
    void sendConfigCommand(const QJsonObject& command);
    void updateConnectionStatus(bool connected);
    void enableControls(bool enabled);

    // 网络
    QUdpSocket* m_udpSocket;
    QString m_radarHost;
    quint16 m_radarPort;
    quint16 m_configPort;
    bool m_isConnected;
    QTimer* m_heartbeatTimer;

    // UI控件
    QLineEdit* m_hostEdit;
    QSpinBox* m_portEdit;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QLabel* m_statusLabel;

    // 配置控件
    QSpinBox* m_generationInterval;
    QSpinBox* m_scanInterval;
    QDoubleSpinBox* m_radarRadius;
    QDoubleSpinBox* m_radarCenterX;
    QDoubleSpinBox* m_radarCenterY;
    QSpinBox* m_maxDrones;
    QDoubleSpinBox* m_minSpeed;
    QDoubleSpinBox* m_maxSpeed;

    QPushButton* m_applyGenButton;
    QPushButton* m_applyRadarButton;
    QPushButton* m_refreshButton;
    QPushButton* m_resetButton;

    QLabel* m_currentSettingsLabel;
};

#endif // RADARCONFIG_H
