//
// SR Spritesheet Manager
//
// file:   srsm_welcome_window.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef SR_WELCOME_WINDOW_H
#define SR_WELCOME_WINDOW_H

#include <QDialog>

namespace Ui
{
  class WelcomeWindow;
}

QT_FORWARD_DECLARE_CLASS(QListWidgetItem);

class WelcomeWindow : public QDialog
{
  Q_OBJECT

 private:
  Ui::WelcomeWindow*   ui;
  std::vector<QString> m_RecentFilePaths;
  QPoint               m_DragPos;
  bool                 m_DoDrag;

 public:
  explicit WelcomeWindow(QWidget* parent = nullptr);
  ~WelcomeWindow();

 private slots:
  void on_m_NewProjectBtn_clicked();
  void on_m_OpenProjectBtn_clicked();
  void onCloseRequested();

  // QWidget interface
  void on_m_RecentFiles_itemDoubleClicked(QListWidgetItem* item);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void tryOpenProject(const QString& file_path);
};

#endif  // SR_WELCOME_WINDOW_H
