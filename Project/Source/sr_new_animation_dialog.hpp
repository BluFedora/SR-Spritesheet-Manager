//
// SR Spritesheet Manager
//
// file:   newanimation.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef NEWANIMATION_HPP
#define NEWANIMATION_HPP

#include "ui_sr_new_animation_dialog.h"

// clang-format off
class NewAnimation : public QDialog, private Ui::NewAnimation
// clang-format on
{
  Q_OBJECT

 public:
  explicit NewAnimation(QWidget *parent = nullptr);

  QString name() const { return m_AnimName->text().trimmed(); }
  int     frameRate() const { return m_Framerate->value(); }

  // QWidget interface
 protected:
  void changeEvent(QEvent *e) override;
  void keyPressEvent(QKeyEvent *event) override;

 private slots:
  void on_m_AnimName_textChanged(const QString &text);
};

#endif  // NEWANIMATION_HPP
