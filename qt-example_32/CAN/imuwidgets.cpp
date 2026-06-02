#include "imuwidgets.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QPen>
#include <QBrush>

// ============================================================================
// 1. 动态罗盘监控控件 (CompassWidget)
// ============================================================================
CompassWidget::CompassWidget(QWidget *parent)
    : QWidget(parent), m_yaw(0.0), m_useRange180(false)
{
    // 保证比例为 1:1，维持正圆形
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CompassWidget::setYaw(double yaw)
{
    // 规范偏航角到 0 ~ 360 度范围以简化绘制
    m_yaw = yaw;
    while (m_yaw < 0.0) m_yaw += 360.0;
    while (m_yaw >= 360.0) m_yaw -= 360.0;
    update(); // 触发重绘
}

void CompassWidget::setRange180(bool enable)
{
    m_useRange180 = enable;
    update();
}

void CompassWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    int size = qMin(width(), height());
    QPointF center(width() / 2.0, height() / 2.0);
    double radius = size / 2.0 - 15.0; // 留白以画指示指针

    if (radius <= 0) return;

    // 1. 绘制背景圆盘
    painter.setPen(QPen(QColor("#3b4252"), 3));
    painter.setBrush(QBrush(QColor("#242933"))); // 更深的表盘灰
    painter.drawEllipse(center, radius, radius);

    // 2. 保存绘图器状态，平移并旋转坐标系以绘制旋转刻度和方位汉字
    painter.save();
    painter.translate(center);
    painter.rotate(-m_yaw); // 罗盘反方向旋转

    // 绘制 30° 递增刻度线与主要方位字
    QPen scalePen(QColor("#4c566a"), 1.5);
    QPen mainScalePen(QColor("#88c0d0"), 2);
    QFont font("Microsoft YaHei", 12, QFont::Bold);
    painter.setFont(font);

    for (int i = 0; i < 360; i += 10) {
        if (i % 90 == 0) {
            painter.setPen(mainScalePen);
            painter.drawLine(0, -radius, 0, -radius + 10);
            
            // 绘制主方位字
            painter.save();
            QString text;
            if (i == 0) text = "北";
            else if (i == 90) text = "东";
            else if (i == 180) text = "南";
            else if (i == 270) text = "西";
            
            // 保持汉字方向竖直向上，不跟着倒立
            painter.translate(0, -radius + 20);
            painter.rotate(m_yaw); 
            
            // 区分主北方向（红色）与其他方向（白色）
            if (i == 0) painter.setPen(QColor("#bf616a"));
            else painter.setPen(QColor("#eceff4"));
            
            QRectF textRect(-15, -15, 30, 30);
            painter.drawText(textRect, Qt::AlignCenter, text);
            painter.restore();
        } else if (i % 30 == 0) {
            painter.setPen(scalePen);
            painter.drawLine(0, -radius, 0, -radius + 8);
        } else {
            painter.setPen(scalePen);
            painter.drawLine(0, -radius, 0, -radius + 5);
        }
        painter.rotate(10);
    }
    painter.restore(); // 恢复无倾斜坐标系

    // 3. 绘制外圈顶部红色的绝对指向箭头（车头指向）
    painter.save();
    painter.translate(center);
    QPolygonF pointer;
    pointer << QPointF(0, -radius - 12)
            << QPointF(-7, -radius - 2)
            << QPointF(7, -radius - 2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(QColor("#bf616a"))); // 北极红
    painter.drawPolygon(pointer);
    painter.restore();

    // 4. 在中心绘制航向度数与方位短语
    QFont valFont("Consolas", 24, QFont::Bold);
    painter.setFont(valFont);
    painter.setPen(QColor("#88c0d0"));
    
    double showYaw = m_yaw;
    if (m_useRange180) {
        if (showYaw > 180.0) {
            showYaw -= 360.0;
        }
    }
    QString yawStr = QString::number(showYaw, 'f', 0) + "°";
    QRectF centerValRect(center.x() - 50, center.y() - 25, 100, 30);
    painter.drawText(centerValRect, Qt::AlignCenter, yawStr);

    QFont subFont("Microsoft YaHei", 10, QFont::Bold);
    painter.setFont(subFont);
    painter.setPen(QColor("#abb2bf"));
    
    QString textDir = "正北";
    if (m_yaw >= 337.5 || m_yaw < 22.5) textDir = "正北";
    else if (m_yaw >= 22.5 && m_yaw < 67.5) textDir = "东北";
    else if (m_yaw >= 67.5 && m_yaw < 112.5) textDir = "正东";
    else if (m_yaw >= 112.5 && m_yaw < 157.5) textDir = "东南";
    else if (m_yaw >= 157.5 && m_yaw < 202.5) textDir = "正南";
    else if (m_yaw >= 202.5 && m_yaw < 247.5) textDir = "西南";
    else if (m_yaw >= 247.5 && m_yaw < 292.5) textDir = "正西";
    else if (m_yaw >= 292.5 && m_yaw < 337.5) textDir = "西北";
    
    QRectF subRect(center.x() - 50, center.y() + 8, 100, 20);
    painter.drawText(subRect, Qt::AlignCenter, textDir);
}

// ============================================================================
// 2. 动态姿态仪/水平仪控件 (AttitudeIndicatorWidget)
// ============================================================================
AttitudeIndicatorWidget::AttitudeIndicatorWidget(QWidget *parent)
    : QWidget(parent), m_roll(0.0), m_pitch(0.0)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void AttitudeIndicatorWidget::setRollPitch(double roll, double pitch)
{
    m_roll = roll;
    m_pitch = pitch;
    // 裁剪边界
    if (m_pitch > 90.0) m_pitch = 90.0;
    if (m_pitch < -90.0) m_pitch = -90.0;
    if (m_roll > 180.0) m_roll -= 360.0;
    if (m_roll < -180.0) m_roll += 360.0;
    update();
}

void AttitudeIndicatorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    int size = qMin(width(), height());
    QPointF center(width() / 2.0, height() / 2.0);
    double radius = size / 2.0 - 10.0;

    if (radius <= 0) return;

    // 1. 设置圆形裁剪区，使得绘制的天地分界线被限制在圆形框内
    QPainterPath clipPath;
    clipPath.addEllipse(center, radius, radius);
    painter.setClipPath(clipPath);

    // 2. 绘制移动的天地背景 (受 Roll 旋转与 Pitch 上下偏移共同控制)
    painter.save();
    painter.translate(center);
    painter.rotate(-m_roll); // 倾斜角控制旋转

    // 俯仰高度系数：假设俯仰45°时平移达到半径尺寸
    double pitchOffsetY = (m_pitch / 45.0) * radius;
    painter.translate(0, pitchOffsetY); // 俯仰角控制上下位移

    // 填充天空（天蓝色）和大地（焦糖棕色）
    QRectF skyRect(-radius * 3.0, -radius * 3.0, radius * 6.0, radius * 3.0);
    QRectF groundRect(-radius * 3.0, 0, radius * 6.0, radius * 3.0);
    painter.fillRect(skyRect, QBrush(QColor("#4c729f")));    // 航空天蓝
    painter.fillRect(groundRect, QBrush(QColor("#8f5e3b"))); // 泥土棕色

    // 绘制白色的天地分界水平基准线
    painter.setPen(QPen(Qt::white, 3));
    painter.drawLine(-radius * 1.5, 0, radius * 1.5, 0);

    // 绘制俯仰刻度尺 (每 5 度一个小刻度，每 10 度大刻度并标数字)
    QFont font("Consolas", 8, QFont::Bold);
    painter.setFont(font);
    painter.setPen(QPen(QColor("#eceff4"), 1.5));
    
    for (int p = -80; p <= 80; p += 5) {
        if (p == 0) continue;
        double lineY = - (p / 45.0) * radius;
        
        if (p % 10 == 0) {
            // 大刻度
            painter.drawLine(-25, lineY, 25, lineY);
            // 绘制左右刻度数字
            painter.drawText(-42, lineY + 4, QString::number(qAbs(p)));
            painter.drawText(29, lineY + 4, QString::number(qAbs(p)));
        } else {
            // 小刻度
            painter.drawLine(-12, lineY, 12, lineY);
        }
    }
    painter.restore(); // 退出天地背景旋转平移的坐标系

    // 3. 取消圆形裁剪以绘制外层装饰表圈和固定车辆指针
    painter.setClipping(false);

    // 绘制外圈厚实的保护环
    painter.setPen(QPen(QColor("#4c566a"), 6));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius + 3.0, radius + 3.0);

    // 4. 绘制固定在窗口中心的黄色飞机/车身轮廓标记
    painter.save();
    painter.translate(center);
    QPen craftPen(QColor("#ebcb8b"), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(craftPen);
    painter.setBrush(QBrush(QColor("#ebcb8b")));
    
    // 中心小圆点
    painter.drawEllipse(QPointF(0, 0), 3.5, 3.5);
    // 左翼及折角
    painter.drawLine(-35, 0, -12, 0);
    painter.drawLine(-12, 0, -12, 6);
    // 右翼及折角
    painter.drawLine(12, 0, 35, 0);
    painter.drawLine(12, 0, 12, 6);
    
    // 底部固定梯形支架 (代表车辆俯仰的托架)
    QPainterPath supportPath;
    supportPath.moveTo(-8, 12);
    supportPath.lineTo(-12, 28);
    supportPath.lineTo(12, 28);
    supportPath.lineTo(8, 12);
    painter.setPen(QPen(QColor("#ebcb8b"), 2));
    painter.setBrush(QBrush(QColor("#ebcb8b"), Qt::Dense6Pattern));
    painter.drawPath(supportPath);
    
    painter.restore();

    // 5. 绘制外表盘顶部的 Roll 倾斜角度刻度标记 (0, 10, 20, 30, 45, 60 度)
    painter.save();
    painter.translate(center);
    painter.setPen(QPen(QColor("#eceff4"), 1.5));
    
    double angles[] = { -60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60 };
    for (double ang : angles) {
        painter.save();
        painter.rotate(ang);
        int len = (qRound(ang) % 30 == 0) ? 8 : 5;
        painter.drawLine(0, -radius - 3, 0, -radius - 3 - len);
        painter.restore();
    }
    
    // 绘制指示当前 Roll 值的白色三角指针 (指示车辆左/右倾斜的角度)
    painter.rotate(-m_roll);
    QPolygonF rollPointer;
    rollPointer << QPointF(0, -radius + 2)
                << QPointF(-5, -radius + 8)
                << QPointF(5, -radius + 8);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(QColor("#88c0d0"))); // 冰蓝色倾斜指针
    painter.drawPolygon(rollPointer);
    
    painter.restore();
}
