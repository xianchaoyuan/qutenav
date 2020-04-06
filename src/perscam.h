#pragma once

#include <QMatrix4x4>
#include "camera.h"

class PersCam: public Camera {

public:

  PersCam(float wmm, float hmm);
  void pan(QVector2D dragStart, QVector2D dragAmount) override;
  void rotateEye(Angle a) override;
  void setScale(quint32 scale) override;
  void resize(float wmm, float hmm) override;
  void reset() override;
  void reset(WGS84Point eye, Angle tilt) override;
  WGS84Point eye() const override;
  Angle northAngle() const override;
  quint32 maxScale() const override;
  quint32 minScale() const override;


private:

  float eyeDistFromScale() const;
  void setScaleFromEyeDist();
  void setEyeDist();

private:


  const float EPSILON = 1.e-5;
  const float R0 = 6371008.8;
  const float PI = 3.14159265358979323846;
  const float FOV2 = PI / 8;
  const quint32 MIN_SCALE = 5616; // 5M + nip

  QMatrix4x4 m_rot, m_rot0;
  QVector3D m_eye, m_eye0;
  float m_mmHeight;
};