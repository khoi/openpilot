#include "selfdrive/ui/qt/onroad.h"

#include <cmath>

#include <QDebug>

#include "common/timing.h"
#include "selfdrive/ui/qt/util.h"
#ifdef ENABLE_MAPS
#include "selfdrive/ui/qt/maps/map.h"
#include "selfdrive/ui/qt/maps/map_helpers.h"
#endif

static const QColor get_tpms_color(float tpms) {
    if(tpms < 5 || tpms > 60) // N/A
        return QColor(255, 255, 255, 220);
    if(tpms < 31)
        return QColor(255, 90, 90, 220);
    return QColor(255, 255, 255, 220);
}

static const QString get_tpms_text(float tpms) {
    if(tpms < 5 || tpms > 60)
        return "";

    char str[32];
    snprintf(str, sizeof(str), "%.0f", round(tpms));
    return QString(str);
}

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(bdr_s);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  nvg = new NvgWindow(VISION_STREAM_ROAD, this);

  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addWidget(nvg);

  stacked_layout->addWidget(split_wrapper);

  alerts = new OnroadAlerts(this);
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
}

void OnroadWindow::updateState(const UIState &s) {
  QColor bgColor = bg_colors[s.status];
  Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
  if (s.sm->updated("controlsState") || !alert.equal({})) {
    if (alert.type == "controlsUnresponsive") {
      bgColor = bg_colors[STATUS_ALERT];
    } else if (alert.type == "controlsUnresponsivePermanent") {
      bgColor = bg_colors[STATUS_DISENGAGED];
    }
    alerts->updateAlert(alert, bgColor);
  }

  if (s.scene.map_on_left) {
    split->setDirection(QBoxLayout::LeftToRight);
  } else {
    split->setDirection(QBoxLayout::RightToLeft);
  }

  nvg->updateState(s);

  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent* e) {
  if (map != nullptr) {
    bool sidebarVisible = geometry().x() > 0;
    map->setVisible(!sidebarVisible && !map->isVisible());
  }
  // propagation event to parent(HomeWindow)
  QWidget::mousePressEvent(e);
}

void OnroadWindow::offroadTransition(bool offroad) {
#ifdef ENABLE_MAPS
  if (!offroad) {
    if (map == nullptr && (uiState()->prime_type || !MAPBOX_TOKEN.isEmpty())) {
      MapWindow * m = new MapWindow(get_mapbox_settings());
      map = m;

      QObject::connect(uiState(), &UIState::offroadTransition, m, &MapWindow::offroadTransition);

      m->setFixedWidth(topWidget(this)->width() / 2);
      split->insertWidget(0, m);

      // Make map visible after adding to split
      m->offroadTransition(offroad);
    }
  }
#endif

  alerts->updateAlert({}, bg);

  // update stream type
  bool wide_cam = Params().getBool("WideCameraOnly");
  nvg->setStreamType(wide_cam ? VISION_STREAM_WIDE_ROAD : VISION_STREAM_ROAD);
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));
}

// ***** onroad widgets *****

// OnroadAlerts
void OnroadAlerts::updateAlert(const Alert &a, const QColor &color) {
  if (!alert.equal(a) || color != bg) {
    alert = a;
    bg = color;
    update();
  }
}

void OnroadAlerts::paintEvent(QPaintEvent *event) {
  if (alert.size == cereal::ControlsState::AlertSize::NONE) {
    return;
  }
  static std::map<cereal::ControlsState::AlertSize, const int> alert_sizes = {
    {cereal::ControlsState::AlertSize::SMALL, 271},
    {cereal::ControlsState::AlertSize::MID, 420},
    {cereal::ControlsState::AlertSize::FULL, height()},
  };
  int h = alert_sizes[alert.size];
  QRect r = QRect(0, height() - h, width(), h);

  QPainter p(this);

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  p.setBrush(QBrush(bg));
  p.drawRect(r);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
  p.fillRect(r, g);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    configFont(p, "Inter", 74, "SemiBold");
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    configFont(p, "Inter", 88, "Bold");
    p.drawText(QRect(0, c.y() - 125, width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    configFont(p, "Inter", 66, "Regular");
    p.drawText(QRect(0, c.y() + 21, width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    configFont(p, "Inter", l ? 132 : 177, "Bold");
    p.drawText(QRect(0, r.y() + (l ? 240 : 270), width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    configFont(p, "Inter", 88, "Regular");
    p.drawText(QRect(0, r.height() - (l ? 361 : 420), width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}

// NvgWindow

NvgWindow::NvgWindow(VisionStreamType type, QWidget* parent) : fps_filter(UI_FREQ, 3, 1. / UI_FREQ), CameraViewWidget("camerad", type, true, parent) {
  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size, img_size});
  ic_tire_pressure = QPixmap("../assets/images/img_tire_pressure.png");
  direction_img = loadPixmap("../assets/img_direction.png", {img_size, img_size});
  turnsignal_l_img = loadPixmap("../assets/img_turnsignal_l.png", {img_size, img_size});
  turnsignal_r_img = loadPixmap("../assets/img_turnsignal_r.png", {img_size, img_size});
}

void NvgWindow::updateState(const UIState &s) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);

  const bool cs_alive = sm.alive("controlsState");
  const bool nav_alive = sm.alive("navInstruction") && sm["navInstruction"].getValid();
  const auto cs = sm["controlsState"].getControlsState();
  const auto ubloxGnss = sm["ubloxGnss"].getUbloxGnss();

  // Handle older routes where vCruiseCluster is not set
  float v_cruise =  cs.getVCruiseCluster() == 0.0 ? cs.getVCruise() : cs.getVCruiseCluster();
  float set_speed = cs_alive ? v_cruise : SET_SPEED_NA;
  bool cruise_set = set_speed > 0 && (int)set_speed != SET_SPEED_NA;
  if (cruise_set && !s.scene.is_metric) {
    set_speed *= KM_TO_MILE;
  }

  // Handle older routes where vEgoCluster is not set
  float v_ego;
  if (sm["carState"].getCarState().getVEgoCluster() == 0.0 && !v_ego_cluster_seen) {
    v_ego = sm["carState"].getCarState().getVEgo();
  } else {
    v_ego = sm["carState"].getCarState().getVEgoCluster();
    v_ego_cluster_seen = true;
  }
  float cur_speed = cs_alive ? std::max<float>(0.0, v_ego) : 0.0;
  cur_speed  *= s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH;

  auto speed_limit_sign = sm["navInstruction"].getNavInstruction().getSpeedLimitSign();
  float speed_limit = nav_alive ? sm["navInstruction"].getNavInstruction().getSpeedLimit() : 0.0;
  speed_limit *= (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH);

  setProperty("speedLimit", speed_limit);
  setProperty("has_us_speed_limit", nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::MUTCD);
  setProperty("has_eu_speed_limit", nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::VIENNA);

  setProperty("is_cruise_set", cruise_set);
  setProperty("is_metric", s.scene.is_metric);
  setProperty("speed", cur_speed);
  setProperty("setSpeed", set_speed);
  setProperty("speedUnit", s.scene.is_metric ? tr("km/h") : tr("mph"));
  setProperty("hideDM", cs.getAlertSize() != cereal::ControlsState::AlertSize::NONE);
  setProperty("status", s.status);
  setProperty("is_braking", sm["carState"].getCarState().getBrakeLights());
  
  // dev ui
  const auto leadOne = sm["radarState"].getRadarState().getLeadOne();
  const auto gpsLocationExternal = sm["gpsLocationExternal"].getGpsLocationExternal();
  const auto carState = sm["carState"].getCarState();

  setProperty("lead_d_rel", leadOne.getDRel());
  setProperty("lead_v_rel", leadOne.getVRel());
  setProperty("lead_status", leadOne.getStatus());
  setProperty("angleSteers", carState.getSteeringAngleDeg());
  setProperty("steerAngleDesired", cs.getLateralControlState().getPidState().getSteeringAngleDesiredDeg());
  setProperty("gpsAccuracy", gpsLocationExternal.getAccuracy());
  setProperty("altitude", gpsLocationExternal.getAltitude());
  setProperty("vEgo", carState.getVEgo());
  setProperty("aEgo", carState.getAEgo());
  setProperty("bearingAccuracyDeg", gpsLocationExternal.getBearingAccuracyDeg());
  setProperty("bearingDeg", gpsLocationExternal.getBearingDeg());
  setProperty("left_bsm", carState.getLeftBlindspot());
  setProperty("right_bsm", carState.getRightBlindspot());
  setProperty("left_blinker", carState.getLeftBlinker());
  setProperty("right_blinker", carState.getRightBlinker());

  // update engageability and DM icons at 2Hz
  if (sm.frame % (UI_FREQ / 2) == 0) {
    setProperty("engageable", cs.getEngageable() || cs.getEnabled());
    setProperty("dmActive", sm["driverMonitoringState"].getDriverMonitoringState().getIsActiveMode());
    setProperty("rightHandDM", sm["driverMonitoringState"].getDriverMonitoringState().getIsRHD());

    setProperty("tpms_fl", sm["carState"].getCarState().getTpms().getFl());
    setProperty("tpms_fr", sm["carState"].getCarState().getTpms().getFr());
    setProperty("tpms_rl", sm["carState"].getCarState().getTpms().getRl());
    setProperty("tpms_rr", sm["carState"].getCarState().getTpms().getRr());
    setProperty("gpsOk", sm["liveLocationKalman"].getLiveLocationKalman().getGpsOK());
    auto numMeas = ubloxGnss.getMeasurementReport().getNumMeas();
    if (numMeas > 0) setProperty("gpsSatCount", numMeas);
  }

  if (s.scene.calibration_valid) {
    CameraViewWidget::updateCalibration(s.scene.view_from_calib);
  } else {
    CameraViewWidget::updateCalibration(DEFAULT_CALIBRATION);
  }
}

void NvgWindow::drawHud(QPainter &p) {
  p.save();

  QString speedLimitStr = (speedLimit > 1) ? QString::number(std::nearbyint(speedLimit)) : "–";
  QString speedStr = QString::number(std::nearbyint(speed));
  QString setSpeedStr = is_cruise_set ? QString::number(std::nearbyint(setSpeed)) : "–";

  // Draw outer box + border to contain set speed and speed limit
  int default_rect_width = 172;
  int rect_width = default_rect_width;
  if (is_metric || has_eu_speed_limit) rect_width = 200;
  if (has_us_speed_limit && speedLimitStr.size() >= 3) rect_width = 223;

  int rect_height = 204;
  if (has_us_speed_limit) rect_height = 402;
  else if (has_eu_speed_limit) rect_height = 392;

  int top_radius = 32;
  int bottom_radius = has_eu_speed_limit ? 100 : 32;

  QRect set_speed_rect(60 + default_rect_width / 2 - rect_width / 2, 45, rect_width, rect_height);
  p.setPen(QPen(whiteColor(75), 6));
  p.setBrush(blackColor(166));
  drawRoundedRect(p, set_speed_rect, top_radius, top_radius, bottom_radius, bottom_radius);

  // Draw MAX
  if (is_cruise_set) {
    if (status == STATUS_DISENGAGED) {
      p.setPen(whiteColor());
    } else if (status == STATUS_OVERRIDE) {
      p.setPen(QColor(0x91, 0x9b, 0x95, 0xff));
    } else if (speedLimit > 0) {
      p.setPen(interpColor(
        setSpeed,
        {speedLimit + 5, speedLimit + 15, speedLimit + 25},
        {QColor(0x80, 0xd8, 0xa6, 0xff), QColor(0xff, 0xe4, 0xbf, 0xff), QColor(0xff, 0xbf, 0xbf, 0xff)}
      ));
    } else {
      p.setPen(QColor(0x80, 0xd8, 0xa6, 0xff));
    }
  } else {
    p.setPen(QColor(0xa6, 0xa6, 0xa6, 0xff));
  }
  configFont(p, "Inter", 40, "SemiBold");
  QRect max_rect = getTextRect(p, Qt::AlignCenter, tr("MAX"));
  max_rect.moveCenter({set_speed_rect.center().x(), 0});
  max_rect.moveTop(set_speed_rect.top() + 27);
  p.drawText(max_rect, Qt::AlignCenter, tr("MAX"));

  // Draw set speed
  if (is_cruise_set) {
    if (speedLimit > 0 && status != STATUS_DISENGAGED && status != STATUS_OVERRIDE) {
      p.setPen(interpColor(
        setSpeed,
        {speedLimit + 5, speedLimit + 15, speedLimit + 25},
        {whiteColor(), QColor(0xff, 0x95, 0x00, 0xff), QColor(0xff, 0x00, 0x00, 0xff)}
      ));
    } else {
      p.setPen(whiteColor());
    }
  } else {
    p.setPen(QColor(0x72, 0x72, 0x72, 0xff));
  }
  configFont(p, "Inter", 90, "Bold");
  QRect speed_rect = getTextRect(p, Qt::AlignCenter, setSpeedStr);
  speed_rect.moveCenter({set_speed_rect.center().x(), 0});
  speed_rect.moveTop(set_speed_rect.top() + 77);
  p.drawText(speed_rect, Qt::AlignCenter, setSpeedStr);



  // US/Canada (MUTCD style) sign
  if (has_us_speed_limit) {
    const int border_width = 6;
    const int sign_width = rect_width - 24;
    const int sign_height = 186;

    // White outer square
    QRect sign_rect_outer(set_speed_rect.left() + 12, set_speed_rect.bottom() - 11 - sign_height, sign_width, sign_height);
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect_outer, 24, 24);

    // Smaller white square with black border
    QRect sign_rect(sign_rect_outer.left() + 1.5 * border_width, sign_rect_outer.top() + 1.5 * border_width, sign_width - 3 * border_width, sign_height - 3 * border_width);
    p.setPen(QPen(blackColor(), border_width));
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect, 16, 16);

    // "SPEED"
    configFont(p, "Inter", 28, "SemiBold");
    QRect text_speed_rect = getTextRect(p, Qt::AlignCenter, tr("SPEED"));
    text_speed_rect.moveCenter({sign_rect.center().x(), 0});
    text_speed_rect.moveTop(sign_rect_outer.top() + 22);
    p.drawText(text_speed_rect, Qt::AlignCenter, tr("SPEED"));

    // "LIMIT"
    QRect text_limit_rect = getTextRect(p, Qt::AlignCenter, tr("LIMIT"));
    text_limit_rect.moveCenter({sign_rect.center().x(), 0});
    text_limit_rect.moveTop(sign_rect_outer.top() + 51);
    p.drawText(text_limit_rect, Qt::AlignCenter, tr("LIMIT"));

    // Speed limit value
    configFont(p, "Inter", 70, "Bold");
    QRect speed_limit_rect = getTextRect(p, Qt::AlignCenter, speedLimitStr);
    speed_limit_rect.moveCenter({sign_rect.center().x(), 0});
    speed_limit_rect.moveTop(sign_rect_outer.top() + 85);
    p.drawText(speed_limit_rect, Qt::AlignCenter, speedLimitStr);
  }

  // EU (Vienna style) sign
  if (has_eu_speed_limit) {
    int outer_radius = 176 / 2;
    int inner_radius_1 = outer_radius - 6; // White outer border
    int inner_radius_2 = inner_radius_1 - 20; // Red circle

    // Draw white circle with red border
    QPoint center(set_speed_rect.center().x() + 1, set_speed_rect.top() + 204 + outer_radius);
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawEllipse(center, outer_radius, outer_radius);
    p.setBrush(QColor(255, 0, 0, 255));
    p.drawEllipse(center, inner_radius_1, inner_radius_1);
    p.setBrush(whiteColor());
    p.drawEllipse(center, inner_radius_2, inner_radius_2);

    // Speed limit value
    int font_size = (speedLimitStr.size() >= 3) ? 60 : 70;
    configFont(p, "Inter", font_size, "Bold");
    QRect speed_limit_rect = getTextRect(p, Qt::AlignCenter, speedLimitStr);
    speed_limit_rect.moveCenter(center);
    p.setPen(blackColor());
    p.drawText(speed_limit_rect, Qt::AlignCenter, speedLimitStr);
  }

  // current speed
  configFont(p, "Inter", 176, "Bold");
  QColor speedColor = is_braking ? redColor() : whiteColor();
  drawTextWithColor(p, rect().center().x(), 210, speedStr, speedColor);
  configFont(p, "Inter", 66, "Regular");
  drawText(p, rect().center().x(), 290, speedUnit, 200);

  // dev ui

  QRect rc2(rect().right() - (bdr_s * 2), bdr_s * 1.5, 184, 202);
  drawRightDevUi(p, rect().right() - 184 - bdr_s * 2, bdr_s * 2 + rc2.height());
  drawRightDevUi2(p, rect().right() - 184 - bdr_s * 2 - 184, bdr_s * 2 + rc2.height());
  drawRightDevUiBorder(p, rect().right() - 184 - bdr_s * 2 - 184, bdr_s * 2 + rc2.height());

  drawIconRotate(
    p, 
    rect().right() - (radius / 2) - (bdr_s * 2), 
    radius / 2 + bdr_s, 
    direction_img, 
    blackColor(100), 
    gpsOk ? 1.0 : 0.2, 
    bearingDeg
  );
  drawTurnSignals(p);

  p.restore();
}

void NvgWindow::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QRect real_rect = getTextRect(p, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void NvgWindow::drawText2(QPainter &p, int x, int y, int flags, const QString &text, const QColor& color) {
  QFontMetrics fm(p.font());
  QRect rect = fm.boundingRect(text);
  rect.adjust(-1, -1, 1, 1);
  p.setPen(color);
  p.drawText(QRect(x, y, rect.width()+1, rect.height()), flags, text);
}

void NvgWindow::drawTextWithColor(QPainter &p, int x, int y, const QString &text, QColor& color) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(color);
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void NvgWindow::drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity) {
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(x - radius / 2, y - radius / 2, radius, radius);
  p.setOpacity(opacity);
  p.drawPixmap(x - img_size / 2, y - img_size / 2, img);
}

void NvgWindow::drawIconRotate(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity, float angle) {
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(x - radius / 2, y - radius / 2, radius, radius);
  p.setOpacity(opacity);
  p.save();
  p.translate(x, y);
  p.rotate(-angle);
  QRect r = img.rect();
  r.moveCenter(QPoint(0,0));
  p.drawPixmap(r, img);
  p.restore();
}

void NvgWindow::initializeGL() {
  CameraViewWidget::initializeGL();
  qInfo() << "OpenGL version:" << QString((const char*)glGetString(GL_VERSION));
  qInfo() << "OpenGL vendor:" << QString((const char*)glGetString(GL_VENDOR));
  qInfo() << "OpenGL renderer:" << QString((const char*)glGetString(GL_RENDERER));
  qInfo() << "OpenGL language version:" << QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

  prev_draw_t = millis_since_boot();
  setBackgroundColor(bg_colors[STATUS_DISENGAGED]);
}

void NvgWindow::updateFrameMat() {
  CameraViewWidget::updateFrameMat();
  UIState *s = uiState();
  int w = width(), h = height();

  s->fb_w = w;
  s->fb_h = h;

  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  // 2) Apply same scaling as video
  // 3) Put (0, 0) in top left corner of video
  s->car_space_transform.reset();
  s->car_space_transform.translate(w / 2 - x_offset, h / 2 - y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
}

void NvgWindow::drawLaneLines(QPainter &painter, const UIState *s) {
  painter.save();

  const UIScene &scene = s->scene;
  // lanelines
  for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
    painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7)));
    painter.drawPolygon(scene.lane_line_vertices[i]);
  }

  painter.setBrush(QColor::fromRgbF(1.0, 0.0, 0.0, 0.8));
  if (left_bsm) painter.drawPolygon(scene.lane_barrier_vertices[0]);
  if (right_bsm) painter.drawPolygon(scene.lane_barrier_vertices[1]);

  // road edges
  for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
    painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
    painter.drawPolygon(scene.road_edge_vertices[i]);
  }

  // paint path
  QLinearGradient bg(0, height(), 0, height() / 4);
  const auto &orientation = (*s->sm)["modelV2"].getModelV2().getOrientation();
  float orientation_future = 0;
  if (orientation.getZ().size() > 16) {
    orientation_future = std::abs(orientation.getZ()[16]);  // 2.5 seconds
  }
  // straight: 112, in turns: 70
  float curve_hue = fmax(70, 112 - (orientation_future * 420));
  // FIXME: painter.drawPolygon can be slow if hue is not rounded
  curve_hue = int(curve_hue * 100 + 0.5) / 100;

  bg.setColorAt(0.0, QColor::fromHslF(148 / 360., 0.94, 0.51, 0.4));
  bg.setColorAt(0.75 / 1.5, QColor::fromHslF(curve_hue / 360., 1.0, 0.68, 0.35));
  bg.setColorAt(1.0, QColor::fromHslF(curve_hue / 360., 1.0, 0.68, 0.0));
  painter.setBrush(bg);
  painter.drawPolygon(scene.track_vertices);

  painter.restore();
}

void NvgWindow::drawLead(QPainter &painter, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const QPointF &vd) {
  painter.save();

  const float speedBuff = 10.;
  const float leadBuff = 40.;
  const float d_rel = lead_data.getX()[0];
  const float v_rel = lead_data.getV()[0];

  float fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  painter.setBrush(QColor(218, 202, 37, 255));
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  painter.setBrush(redColor(fillAlpha));
  painter.drawPolygon(chevron, std::size(chevron));

  configFont(painter, "Inter", 50, "SemiBold");
  char distance_str[16];
  snprintf(distance_str, sizeof(distance_str), "%.1f m", lead_d_rel);
  QString distance_q_str = QString(distance_str);
  QRect distance_str_rect = getTextRect(painter, Qt::AlignCenter, distance_q_str);
  distance_str_rect.moveCenter(QPoint(x - 15, y - 15));
  painter.setPen(whiteColor());
  painter.drawText(distance_str_rect, Qt::AlignCenter, distance_q_str);

  painter.restore();
}

void NvgWindow::paintGL() {
  UIState *s = uiState();
  const cereal::ModelDataV2::Reader &model = (*s->sm)["modelV2"].getModelV2();
  CameraViewWidget::setFrameId(model.getFrameId());
  CameraViewWidget::paintGL();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);

  if (s->worldObjectsVisible()) {

    drawLaneLines(painter, s);

    const auto leads = model.getLeadsV3();
    if (leads[0].getProb() > .5) {
      drawLead(painter, leads[0], s->scene.lead_vertices[0]);
    }
    if (leads[1].getProb() > .5 && (std::abs(leads[1].getX()[0] - leads[0].getX()[0]) > 3.0)) {
      drawLead(painter, leads[1], s->scene.lead_vertices[1]);
    }
  }

  drawHud(painter);
  drawBottomIcons(painter);

  double cur_draw_t = millis_since_boot();
  double dt = cur_draw_t - prev_draw_t;
  double fps = fps_filter.update(1. / dt * 1000);
  if (fps < 15) {
    LOGW("slow frame rate: %.2f fps", fps);
  }
  prev_draw_t = cur_draw_t;
}

void NvgWindow::showEvent(QShowEvent *event) {
  CameraViewWidget::showEvent(event);

  ui_update_params(uiState());
  prev_draw_t = millis_since_boot();
}

// DEV UI
void NvgWindow::drawBottomIcons(QPainter &p) {
  p.save();

  // tire pressure
  {
    const int w = 58;
    const int h = 126;
    const int x = 110;
    const int y = height() - h - 85;

    p.setOpacity(0.8);
    p.drawPixmap(x, y, w, h, ic_tire_pressure);

    configFont(p, "Inter", 38, "Bold");

    QFontMetrics fm(p.font());
    QRect rcFont = fm.boundingRect("9");

    int center_x = x + 3;
    int center_y = y + h/2;
    const int marginX = (int)(rcFont.width() * 2.7f);
    const int marginY = (int)((h/2 - rcFont.height()) * 0.7f);

    drawText2(p, center_x-marginX, center_y-marginY-rcFont.height(), Qt::AlignRight, get_tpms_text(tpms_fl), get_tpms_color(tpms_fl));
    drawText2(p, center_x+marginX, center_y-marginY-rcFont.height(), Qt::AlignLeft, get_tpms_text(tpms_fr), get_tpms_color(tpms_fr));
    drawText2(p, center_x-marginX, center_y+marginY, Qt::AlignRight, get_tpms_text(tpms_rl), get_tpms_color(tpms_rl));
    drawText2(p, center_x+marginX, center_y+marginY, Qt::AlignLeft, get_tpms_text(tpms_rr), get_tpms_color(tpms_rr));
  }

  p.restore();
}

void NvgWindow::drawRightDevUi(QPainter &p, int x, int y) {
  int rh = 5;
  int ry = y;

  // Add Relative Distance to Primary Lead Car
  // Unit: Meters
  if (true) {
    char val_str[16];
    char units_str[8];
    QColor valueColor = QColor(255, 255, 255, 255);

    if (lead_status) {
      // Orange if close, Red if very close
      if (lead_d_rel < 5) {
        valueColor = QColor(255, 0, 0, 255);
      } else if (lead_d_rel < 15) {
        valueColor = QColor(255, 188, 0, 255);
      }
      snprintf(val_str, sizeof(val_str), "%d", (int)lead_d_rel);
    } else {
      snprintf(val_str, sizeof(val_str), "-");
    }

    snprintf(units_str, sizeof(units_str), "m");

    rh += drawDevUiElementRight(p, x, ry, val_str, "REL DIST", units_str, valueColor);
    ry = y + rh;
  }

  // Add Relative Velocity vs Primary Lead Car
  // Unit: kph if metric, else mph
  if (true) {
    char val_str[16];
    QColor valueColor = QColor(255, 255, 255, 255);

     if (lead_status) {
       // Red if approaching faster than 10mph
       // Orange if approaching (negative)
       if (lead_v_rel < -4.4704) {
        valueColor = QColor(255, 0, 0, 255);
       } else if (lead_v_rel < 0) {
         valueColor = QColor(255, 188, 0, 255);
       }

       if (speedUnit == "mph") {
         snprintf(val_str, sizeof(val_str), "%d", (int)(lead_v_rel * 2.236936)); //mph
       } else {
         snprintf(val_str, sizeof(val_str), "%d", (int)(lead_v_rel * 3.6)); //kph
       }
     } else {
       snprintf(val_str, sizeof(val_str), "-");
     }

    rh += drawDevUiElementRight(p, x, ry, val_str, "REL SPEED", speedUnit.toStdString().c_str(), valueColor);
    ry = y + rh;
  }

  // Add Real Steering Angle
  // Unit: Degrees
  if (true) {
    char val_str[16];
    QColor valueColor = QColor(0, 255, 0, 255);

    // Red if large steering angle
    // Orange if moderate steering angle
    if (std::fabs(angleSteers) > 50) {
      valueColor = QColor(255, 0, 0, 255);
    } else if (std::fabs(angleSteers) > 30) {
      valueColor = QColor(255, 188, 0, 255);
    }

    snprintf(val_str, sizeof(val_str), "%.1f%s%s", angleSteers , "°", "");

    rh += drawDevUiElementRight(p, x, ry, val_str, "REAL STEER", "", valueColor);
    ry = y + rh;
  }

  if (true) {
    char val_str[16];
    QColor valueColor = QColor(255, 255, 255, 255);

    if (!gpsOk) {
      snprintf(val_str, sizeof(val_str), "-");
    } else {
      snprintf(val_str, sizeof(val_str), "%.1f", std::fabs(gpsAccuracy));
    }

    rh += drawDevUiElementRight(p, x, ry, val_str, "GPS (acc)", "m", valueColor);
    ry = y + rh;
  }

  // Add GPS Sat Count
  if (true) {
    char val_str[16];
    QColor valueColor = QColor(255, 255, 255, 255);

    if (gpsSatCount < 10) {
      valueColor = QColor(255, 188, 0, 255);
    }

    snprintf(val_str, sizeof(val_str), "%d", gpsSatCount);

    rh += drawDevUiElementRight(p, x, ry, val_str, "SAT", "count", valueColor);
    ry = y + rh;
  }

  rh += 25;
  p.setBrush(QColor(0, 0, 0, 0));
  QRect ldu(x, y, 184, rh);
  //p.drawRoundedRect(ldu, 20, 20);
}

void NvgWindow::drawRightDevUi2(QPainter &p, int x, int y) {
  int rh = 5;
  int ry = y;


  // Add Acceleration from Car
  // Unit: Meters per Second Squared
  if (true) {
    char val_str[16];
    QColor valueColor = QColor(255, 255, 255, 255);

    snprintf(val_str, sizeof(val_str), "%.1f", aEgo);

    rh += drawDevUiElementLeft(p, x, ry, val_str, "ACCEL", "m/s²", valueColor);
    ry = y + rh;
  }

  // Add Velocity of Primary Lead Car
  // Unit: kph if metric, else mph
  if (true) {
    char val_str[16];
    QColor valueColor = QColor(255, 255, 255, 255);

     if (lead_status) {
       if (speedUnit == "mph") {
         snprintf(val_str, sizeof(val_str), "%d", (int)((lead_v_rel + vEgo) * 2.236936)); //mph
       } else {
         snprintf(val_str, sizeof(val_str), "%d", (int)((lead_v_rel + vEgo) * 3.6)); //kph
       }
     } else {
       snprintf(val_str, sizeof(val_str), "-");
     }

    rh += drawDevUiElementLeft(p, x, ry, val_str, "LEAD SPD", speedUnit.toStdString().c_str(), valueColor);
    ry = y + rh;
  }

  if (true) {
    char val_str[16];
    QColor valueColor = QColor(255, 255, 255, 255);

    // Red if large steering angle
    // Orange if moderate steering angle
    if (std::fabs(angleSteers) > 50) {
      valueColor = QColor(255, 0, 0, 255);
    } else if (std::fabs(angleSteers) > 30) {
      valueColor = QColor(255, 188, 0, 255);
    }

    snprintf(val_str, sizeof(val_str), "%.1f%s%s", steerAngleDesired, "°", "");

    rh += drawDevUiElementLeft(p, x, ry, val_str, "DESIR STEER", "", valueColor);
    ry = y + rh;
  }

  // Add Bearing Degree and Direction from Car (Compass)
  // Unit: Meters
  if (true) {
    char val_str[16];
    char dir_str[8];
    QColor valueColor = QColor(255, 255, 255, 255);

    if (bearingAccuracyDeg != 180.00) {
      snprintf(val_str, sizeof(val_str), "%.0d%s%s", (int)bearingDeg, "°", "");
      if (((bearingDeg >= 337.5) && (bearingDeg <= 360)) || ((bearingDeg >= 0) && (bearingDeg <= 22.5))) {
        snprintf(dir_str, sizeof(dir_str), "N");
      } else if ((bearingDeg > 22.5) && (bearingDeg < 67.5)) {
        snprintf(dir_str, sizeof(dir_str), "NE");
      } else if ((bearingDeg >= 67.5) && (bearingDeg <= 112.5)) {
        snprintf(dir_str, sizeof(dir_str), "E");
      } else if ((bearingDeg > 112.5) && (bearingDeg < 157.5)) {
        snprintf(dir_str, sizeof(dir_str), "SE");
      } else if ((bearingDeg >= 157.5) && (bearingDeg <= 202.5)) {
        snprintf(dir_str, sizeof(dir_str), "S");
      } else if ((bearingDeg > 202.5) && (bearingDeg < 247.5)) {
        snprintf(dir_str, sizeof(dir_str), "SW");
      } else if ((bearingDeg >= 247.5) && (bearingDeg <= 292.5)) {
        snprintf(dir_str, sizeof(dir_str), "W");
      } else if ((bearingDeg > 292.5) && (bearingDeg < 337.5)) {
        snprintf(dir_str, sizeof(dir_str), "NW");
      }
    } else {
      snprintf(dir_str, sizeof(dir_str), "OFF");
      snprintf(val_str, sizeof(val_str), "-");
    }

    rh += drawDevUiElementLeft(p, x, ry, dir_str, val_str, "", valueColor);
    ry = y + rh;
  }

  // Add Altitude of Current Location
  // Unit: Meters
  if (true) {
    char val_str[16];
    QColor valueColor = QColor(255, 255, 255, 255);

    if (gpsOk) {
      snprintf(val_str, sizeof(val_str), "%.1f", altitude);
    } else {
      snprintf(val_str, sizeof(val_str), "-");
    }

    rh += drawDevUiElementLeft(p, x, ry, val_str, "ALTITUDE", "m", valueColor);
    ry = y + rh;
  }

  rh += 25;
  p.setBrush(QColor(0, 0, 0, 0));
  QRect ldu(x, y, 184, rh);
  //p.drawRoundedRect(ldu, 20, 20);
}

void NvgWindow::drawRightDevUiBorder(QPainter &p, int x, int y) {
  int rh = 580;
  int rw = 184;
  p.setPen(QPen(QColor(0xff, 0xff, 0xff, 100), 6));
  p.setBrush(QColor(0, 0, 0, 0));
  rw *= 2;
  QRect ldu(x, y, rw, rh);
  p.drawRoundedRect(ldu, 20, 20);
}

int NvgWindow::drawDevUiElementRight(QPainter &p, int x, int y, const char* value, const char* label, const char* units, QColor &color) {
  configFont(p, "Inter", 30 * 2, "SemiBold");
  drawTextWithColor(p, x + 92, y + 80, QString(value), color);

  configFont(p, "Inter", 28, "Regular");
  drawText(p, x + 92, y + 80 + 42, QString(label), 255);

  if (strlen(units) > 0) {
    p.save();
    p.translate(x + 54 + 30 - 3 + 92, y + 37 + 25);
    p.rotate(-90);
    drawText(p, 0, 0, QString(units), 255);
    p.restore();
  }

  return 110;
}

int NvgWindow::drawDevUiElementLeft(QPainter &p, int x, int y, const char* value, const char* label, const char* units, QColor &color) {
  configFont(p, "Inter", 30 * 2, "SemiBold");
  drawTextWithColor(p, x + 92, y + 80, QString(value), color);

  configFont(p, "Inter", 28, "Regular");
  drawText(p, x + 92, y + 80 + 42, QString(label), 255);

  if (strlen(units) > 0) {
    p.save();
    p.translate(x + 11, y + 37 + 25);
    p.rotate(90);
    drawText(p, 0, 0, QString(units), 255);
    p.restore();
  }

  return 110;
}

void NvgWindow::drawTurnSignals(QPainter &p) {
  // turnsignal
  static int blink_index = 0;
  static int blink_wait = 0;
  static double prev_ts = 0.0;

  if (blink_wait > 0) {
    blink_wait--;
    blink_index = 0;
  } else {
    const float img_alpha = 1.0f;
    const int center_x = width() / 2;
    const int w = 300;
    const int h = 300;
    const int y = (height() - h) / 2;
    const int draw_count = 8;

    int x = center_x;
    if (left_blinker) {
      for (int i = 0; i < draw_count; i++) {
        float alpha = img_alpha;
        int d = std::abs(blink_index - i);
        if (d > 0)
          alpha /= d * 2;
        p.setOpacity(alpha);
        p.drawPixmap(x - w, y, w, h, turnsignal_l_img);
        x -= w * 0.6;
      }
    }

    x = center_x;
    if (right_blinker) {
      for (int i = 0; i < draw_count; i++) {
        float alpha = img_alpha;
        int d = std::abs(blink_index - i);
        if (d > 0)
          alpha /= d * 2;
        p.setOpacity(alpha);
        p.drawPixmap(x, y, w, h, turnsignal_r_img);
        x += w * 0.6;
      }
    }

    if (left_blinker || right_blinker) {
      double now = millis_since_boot();
      if (now - prev_ts > 900 / UI_FREQ) {
        prev_ts = now;
        blink_index++;
      }
      if (blink_index >= draw_count) {
        blink_index = draw_count - 1;
        blink_wait = UI_FREQ / 4;
      }
    } else {
      blink_index = 0;
    }
  }
  p.setOpacity(1.);
  p.restore();
}