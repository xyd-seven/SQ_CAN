#ifndef IMUWIDGETS_H
#define IMUWIDGETS_H

#include <QWidget>

// 1. 动态罗盘监控控件 (指示 Yaw 偏航角)
class CompassWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CompassWidget(QWidget *parent = nullptr);
    void setYaw(double yaw);
    void setRange180(bool enable); // 设置是否以 -180 ~ 180° 格式显示中心度数

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_yaw; // 偏航角
    bool m_useRange180; // 是否显示为 -180 ~ 180 格式
};

// 2. 陀螺姿态仪监控控件 (指示 Roll 翻滚角 与 Pitch 俯仰角)
class AttitudeIndicatorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AttitudeIndicatorWidget(QWidget *parent = nullptr);
    void setRollPitch(double roll, double pitch);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_roll;  // 翻滚角 (-180 ~ 180 度)
    double m_pitch; // 俯仰角 (-90 ~ 90 度)
};

#endif // IMUWIDGETS_H
