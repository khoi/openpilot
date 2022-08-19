#pragma once

#include <QStackedLayout>
#include <QWidget>

#include "common/util.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"
#include "selfdrive/ui/ui.h"


// ***** onroad widgets *****
class OnroadAlerts : public QWidget {
  Q_OBJECT

public:
  OnroadAlerts(QWidget *parent = 0) : QWidget(parent) {};
  void updateAlert(const Alert &a, const QColor &color);

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QColor bg;
  Alert alert = {};
};

// container window for the NVG UI
class NvgWindow : public CameraViewWidget {
  Q_OBJECT
  Q_PROPERTY(float speed MEMBER speed);
  Q_PROPERTY(QString speedUnit MEMBER speedUnit);
  Q_PROPERTY(float setSpeed MEMBER setSpeed);
  Q_PROPERTY(float speedLimit MEMBER speedLimit);
  Q_PROPERTY(bool is_cruise_set MEMBER is_cruise_set);
  Q_PROPERTY(bool has_eu_speed_limit MEMBER has_eu_speed_limit);
  Q_PROPERTY(bool has_us_speed_limit MEMBER has_us_speed_limit);
  Q_PROPERTY(bool is_metric MEMBER is_metric);

  Q_PROPERTY(bool engageable MEMBER engageable);
  Q_PROPERTY(bool dmActive MEMBER dmActive);
  Q_PROPERTY(bool hideDM MEMBER hideDM);
  Q_PROPERTY(bool rightHandDM MEMBER rightHandDM);
  Q_PROPERTY(int status MEMBER status);
  Q_PROPERTY(int is_braking MEMBER is_braking);
  Q_PROPERTY(float tpms_fl MEMBER tpms_fl);
  Q_PROPERTY(float tpms_fr MEMBER tpms_fr);
  Q_PROPERTY(float tpms_rl MEMBER tpms_rl);
  Q_PROPERTY(float tpms_rr MEMBER tpms_rr);
  Q_PROPERTY(float gpsAccuracy MEMBER gpsAccuracy);
  Q_PROPERTY(int gpsSatCount MEMBER gpsSatCount);
  Q_PROPERTY(int lead_status MEMBER lead_status);
  Q_PROPERTY(float lead_d_rel MEMBER lead_d_rel);
  Q_PROPERTY(float lead_v_rel MEMBER lead_v_rel);
  Q_PROPERTY(float angleSteers MEMBER angleSteers);
  Q_PROPERTY(float steerAngleDesired MEMBER steerAngleDesired);
  Q_PROPERTY(int engineRPM MEMBER engineRPM);
  
public:
  explicit NvgWindow(VisionStreamType type, QWidget* parent = 0);
  void updateState(const UIState &s);

private:
  void drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity);
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);
  void drawText2(QPainter &p, int x, int y, int flags, const QString &text, const QColor& color);
  int devUiDrawElement(QPainter &p, int x, int y, const char* value, const char* label, const char* units, QColor &color);
  void drawTextWithColor(QPainter &p, int x, int y, const QString &text, QColor& color);

  QPixmap engage_img;
  QPixmap dm_img;
  const int radius = 192;
  const int img_size = (radius / 2) * 1.5;
  float speed;
  QString speedUnit;
  float setSpeed;
  float speedLimit;
  bool is_cruise_set = false;
  bool is_metric = false;
  bool engageable = false;
  bool dmActive = false;
  bool hideDM = false;
  bool rightHandDM = false;
  bool has_us_speed_limit = false;
  bool has_eu_speed_limit = false;
  bool v_ego_cluster_seen = false;
  int status = STATUS_DISENGAGED;
  bool is_braking = false;
  float tpms_fl = 0;
  float tpms_fr = 0;
  float tpms_rl = 0;
  float tpms_rr = 0;
  float gpsAccuracy = 0;
  int gpsSatCount = 0;
  int lead_status;
  float lead_d_rel = 0;
  float lead_v_rel = 0;
  float angleSteers = 0;
  float steerAngleDesired = 0;
  int engineRPM = 0;

protected:
  void paintGL() override;
  void initializeGL() override;
  void showEvent(QShowEvent *event) override;
  void updateFrameMat() override;
  void drawLaneLines(QPainter &painter, const UIState *s);
  void drawLead(QPainter &painter, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const QPointF &vd);
  void drawHud(QPainter &p);
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }

  double prev_draw_t = 0;
  FirstOrderFilter fps_filter;

  // DEV UI
  QPixmap ic_brake;
  QPixmap ic_tire_pressure;
  QPixmap ic_autohold_warning;
  QPixmap ic_autohold_active;
  QPixmap ic_satellite;

  void drawBottomIcons(QPainter &p);
  void drawGpsStatus(QPainter &p);
  void drawLeftDevUI(QPainter &p, int x, int y);
};

// container for all onroad widgets
class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget* parent = 0);
  bool isMapVisible() const { return map && map->isVisible(); }

private:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent* e) override;
  OnroadAlerts *alerts;
  NvgWindow *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QWidget *map = nullptr;
  QHBoxLayout* split;

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};
