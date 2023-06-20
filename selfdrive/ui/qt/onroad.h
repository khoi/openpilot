#pragma once

#include <QPushButton>
#include <QStackedLayout>
#include <QWidget>

#include "common/util.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"


const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;


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

class ExperimentalButton : public QPushButton {
  Q_OBJECT

public:
  explicit ExperimentalButton(QWidget *parent = 0);
  void updateState(const UIState &s);

private:
  void paintEvent(QPaintEvent *event) override;

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;
};

// container window for the NVG UI
class AnnotatedCameraWidget : public CameraWidget {
  Q_OBJECT
  Q_PROPERTY(float speed MEMBER speed);
  Q_PROPERTY(QString speedUnit MEMBER speedUnit);
  Q_PROPERTY(float setSpeed MEMBER setSpeed);
  Q_PROPERTY(float speedLimit MEMBER speedLimit);
  Q_PROPERTY(bool is_cruise_set MEMBER is_cruise_set);
  Q_PROPERTY(bool has_eu_speed_limit MEMBER has_eu_speed_limit);
  Q_PROPERTY(bool has_us_speed_limit MEMBER has_us_speed_limit);
  Q_PROPERTY(bool is_metric MEMBER is_metric);

  Q_PROPERTY(bool dmActive MEMBER dmActive);
  Q_PROPERTY(bool hideDM MEMBER hideDM);
  Q_PROPERTY(bool rightHandDM MEMBER rightHandDM);
  Q_PROPERTY(int status MEMBER status);
  Q_PROPERTY(int is_braking MEMBER is_braking);
  Q_PROPERTY(float gpsAccuracy MEMBER gpsAccuracy)
  Q_PROPERTY(bool gpsOk MEMBER gpsOk);
  Q_PROPERTY(float altitude MEMBER altitude);
  Q_PROPERTY(int gpsSatCount MEMBER gpsSatCount)
  Q_PROPERTY(float lead_d_rel MEMBER lead_d_rel);
  Q_PROPERTY(float angleSteers MEMBER angleSteers);
  Q_PROPERTY(float steerAngleDesired MEMBER steerAngleDesired);
  Q_PROPERTY(float bearingAccuracyDeg MEMBER bearingAccuracyDeg);
  Q_PROPERTY(float bearingDeg MEMBER bearingDeg);
  Q_PROPERTY(float torqueLatAccel MEMBER torqueLatAccel);
  Q_PROPERTY(float torqueFriction MEMBER torqueFriction);
  Q_PROPERTY(float cpuPerc MEMBER cpuPerc);
  Q_PROPERTY(float cpuTemp MEMBER cpuTemp);
  Q_PROPERTY(float fanSpeed MEMBER fanSpeed);
  Q_PROPERTY(bool left_bsm MEMBER left_bsm);
  Q_PROPERTY(bool right_bsm MEMBER right_bsm);
  Q_PROPERTY(bool left_blinker MEMBER left_blinker);
  Q_PROPERTY(bool right_blinker MEMBER right_blinker);

public:
  explicit AnnotatedCameraWidget(VisionStreamType type, QWidget* parent = 0);
  void updateState(const UIState &s);

private:
  void drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity);
  void drawIconRotate(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity, float angle);
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);
  void drawText2(QPainter &p, int x, int y, int flags, const QString &text, const QColor& color);
  void drawTextWithColor(QPainter &p, int x, int y, const QString &text, QColor& color);

  ExperimentalButton *experimental_btn;
  const int radius = 192;
  QPixmap dm_img;
  float speed;
  QString speedUnit;
  float setSpeed;
  float speedLimit;
  bool is_cruise_set = false;
  bool is_metric = false;
  bool dmActive = false;
  bool hideDM = false;
  bool rightHandDM = false;
  float dm_fade_state = 1.0;
  bool has_us_speed_limit = false;
  bool has_eu_speed_limit = false;
  bool v_ego_cluster_seen = false;
  int status = STATUS_DISENGAGED;
  std::unique_ptr<PubMaster> pm;
  bool is_braking = false;
  float gpsAccuracy = 0;
  bool gpsOk = false;
  float altitude = 0;
  int gpsSatCount = 0;
  float lead_d_rel = 0;
  float angleSteers = 0;
  float steerAngleDesired = 0;
  float bearingAccuracyDeg = 0;
  float bearingDeg = 0;
  float torqueLatAccel = 0;
  float torqueFriction = 0;
  int cpuPerc = 0;
  float cpuTemp = 0;
  int fanSpeed = 0;
  bool left_bsm = false;
  bool right_bsm = false;
  bool left_blinker = false;
  bool right_blinker = false;

  int skip_frame_count = 0;
  bool wide_cam_requested = false;

protected:
  void paintGL() override;
  void initializeGL() override;
  void showEvent(QShowEvent *event) override;
  void updateFrameMat() override;
  void drawLaneLines(QPainter &painter, const UIState *s);
  void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd);
  void drawHud(QPainter &p);
  void drawDriverState(QPainter &painter, const UIState *s);
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }

  double prev_draw_t = 0;
  FirstOrderFilter fps_filter;

  // DEV UI
  QPixmap direction_img;
  QPixmap turnsignal_l_img;
  QPixmap turnsignal_r_img;
 
  void drawGpsStatus(QPainter &p);
  void drawRightDevUi(QPainter &p, int x, int y);
  void drawRightDevUi2(QPainter &p, int x, int y);
  void drawRightDevUiBorder(QPainter &p, int x, int y);
  int drawDevUiElementRight(QPainter &p, int x, int y, const char* value, const char* label, const char* units, QColor &color);
  int drawDevUiElementLeft(QPainter &p, int x, int y, const char* value, const char* label, const char* units, QColor &color);
  void drawTurnSignals(QPainter &p);
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
  AnnotatedCameraWidget *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QWidget *map = nullptr;
  QHBoxLayout* split;

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};
