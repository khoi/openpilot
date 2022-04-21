#include "selfdrive/ui/qt/sidebar.h"

#include <QMouseEvent>

#include "selfdrive/ui/qt/util.h"

void Sidebar::drawMetric(QPainter &p, const QString &label, QColor c, int y) {
  const QRect rect = {30, y, 240, label.contains("\n") ? 124 : 100};

  p.setPen(Qt::NoPen);
  p.setBrush(QBrush(c));
  p.setClipRect(rect.x() + 6, rect.y(), 18, rect.height(), Qt::ClipOperation::ReplaceClip);
  p.drawRoundedRect(QRect(rect.x() + 6, rect.y() + 6, 100, rect.height() - 12), 10, 10);
  p.setClipping(false);

  QPen pen = QPen(QColor(0xff, 0xff, 0xff, 0x55));
  pen.setWidth(2);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(rect, 20, 20);

  p.setPen(QColor(0xff, 0xff, 0xff));
  configFont(p, "Open Sans", 35, "Bold");
  const QRect r = QRect(rect.x() + 30, rect.y(), rect.width() - 40, rect.height());
  p.drawText(r, Qt::AlignCenter, label);
}

Sidebar::Sidebar(QWidget *parent) : QFrame(parent) {
  settings_img = loadPixmap("../assets/images/button_settings.png", settings_btn.size(), Qt::IgnoreAspectRatio);

  connect(this, &Sidebar::valueChanged, [=] { update(); });

  setAttribute(Qt::WA_OpaquePaintEvent);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(300);

  QObject::connect(uiState(), &UIState::uiUpdate, this, &Sidebar::updateState);
}

void Sidebar::mouseReleaseEvent(QMouseEvent *event) {
  if (settings_btn.contains(event->pos())) {
    emit openSettings();
  }
}

void Sidebar::updateState(const UIState &s) {
  if (!isVisible()) return;

  auto &sm = *(s.sm);

  auto deviceState = sm["deviceState"].getDeviceState();
  setProperty("netType", network_type[deviceState.getNetworkType()]);
  int strength = (int)deviceState.getNetworkStrength();
  setProperty("netStrength", strength > 0 ? strength + 1 : 0);

  auto fanSpeedPercentDesired = deviceState.getFanSpeedPercentDesired();
  QString fanSpeedStr = QString::fromUtf8((util::string_format("FAN\n%d%%", fanSpeedPercentDesired).c_str()));

  ItemStatus fanSpeedStatus = {fanSpeedStr, good_color};
  if (fanSpeedPercentDesired > 80) {
    fanSpeedStatus = {fanSpeedStr, danger_color};
  } else if (fanSpeedPercentDesired > 50) {
    fanSpeedStatus = {fanSpeedStr, warning_color};
  }
  setProperty("fanSpeedStatus", QVariant::fromValue(fanSpeedStatus));

  auto cpuList = deviceState.getCpuTempC();
  float cpuTemp = 0;
  if (cpuList.size() > 0) {
    for(int i = 0; i < cpuList.size(); i++)
        cpuTemp += cpuList[i];

    cpuTemp /= cpuList.size();
  }
  QString cpuTempStr = QString::fromUtf8((util::string_format("CPU\n%.1f°", cpuTemp).c_str()));

  ItemStatus cpuTempStatus = {cpuTempStr, danger_color};
  auto ts = deviceState.getThermalStatus();
  if (ts == cereal::DeviceState::ThermalStatus::GREEN) {
    cpuTempStatus = {cpuTempStr, good_color};
  } else if (ts == cereal::DeviceState::ThermalStatus::YELLOW) {
    cpuTempStatus = {cpuTempStr, warning_color};
  }
  setProperty("cpuTempStatus", QVariant::fromValue(cpuTempStatus));

  auto usageList = deviceState.getCpuUsagePercent();
  float cpuUsageAvg = 0;
  if (usageList.size() > 0) {
    for(int i = 0; i < usageList.size(); i++)
        cpuUsageAvg += usageList[i];

    cpuUsageAvg /= usageList.size();
  }
  QString cpuUsageTempStr = QString::fromUtf8((util::string_format("LOAD\n%.0f%%", cpuUsageAvg).c_str()));

  ItemStatus cpuUsageStatus = {cpuUsageTempStr, good_color};
  if (cpuUsageAvg > 90) {
    cpuUsageStatus = {cpuUsageTempStr, danger_color};
  } else if (cpuUsageAvg > 80) {
    cpuUsageStatus = {cpuUsageTempStr, warning_color};
  }
  setProperty("cpuUsageStatus", QVariant::fromValue(cpuUsageStatus));

  float powerDrawW = deviceState.getPowerDrawW();
  QString powerDrawWStr = QString::fromUtf8((util::string_format("Watt\n%.1f°", powerDrawW).c_str()));
  ItemStatus powerDrawStatus = {powerDrawWStr, danger_color};
  if (ts == cereal::DeviceState::ThermalStatus::GREEN) {
    powerDrawStatus = {powerDrawWStr, good_color};
  } else if (ts == cereal::DeviceState::ThermalStatus::YELLOW) {
    powerDrawStatus = {powerDrawWStr, warning_color};
  }
  setProperty("powerDrawStatus", QVariant::fromValue(powerDrawStatus));
}

void Sidebar::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setPen(Qt::NoPen);
  p.setRenderHint(QPainter::Antialiasing);

  p.fillRect(rect(), QColor(57, 57, 57));

  // static imgs
  p.setOpacity(0.65);
  p.drawPixmap(settings_btn.x(), settings_btn.y(), settings_img);

  p.setOpacity(1);
  // network
  int x = 58;
  const QColor gray(0x54, 0x54, 0x54);
  for (int i = 0; i < 5; ++i) {
    p.setBrush(i < net_strength ? Qt::white : gray);
    p.drawEllipse(x, 196, 27, 27);
    x += 37;
  }

  configFont(p, "Open Sans", 35, "Regular");
  p.setPen(QColor(0xff, 0xff, 0xff));
  const QRect r = QRect(50, 247, 100, 50);
  p.drawText(r, Qt::AlignCenter, net_type);

  // metrics
  drawMetric(p, power_draw_status.first, power_draw_status.second, 338);
  drawMetric(p, cpu_temp_status.first, cpu_temp_status.second, 496);
  drawMetric(p, cpu_usage_status.first, cpu_usage_status.second, 654);
  drawMetric(p, fan_speed_status.first, fan_speed_status.second, 812);
}
