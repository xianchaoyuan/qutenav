#pragma once

#include <KXmlGuiWindow>
#include <QObject>
#include "drawable.h"


class GLWidget;

class MainWindow: public KXmlGuiWindow {
  Q_OBJECT

public:

  explicit MainWindow();
  ~MainWindow() override;

protected:

  void closeEvent(QCloseEvent *event) override;

private slots:

  void on_quit_triggered();
  void on_northUp_triggered();
  void on_fullScreen_toggled(bool on);
  void on_panNorth_triggered();
  void on_panEast_triggered();
  void on_panSouth_triggered();
  void on_panWest_triggered();

private:

  void readSettings();
  void writeSettings();
  void addActions();

private:

  GLWidget* m_GLWidget;
  QRect m_fallbackGeom;

};
