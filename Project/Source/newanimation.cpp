//
// SR Spritesheet Manager
//
// file:   newanimation.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "newanimation.hpp"

#include <QKeyEvent>

NewAnimation::NewAnimation(QWidget *parent) :
  QDialog(parent)
{
  setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setFixedSize(size());
}

void NewAnimation::changeEvent(QEvent *e)
{
  QDialog::changeEvent(e);

  switch (e->type())
  {
    case QEvent::LanguageChange:
      retranslateUi(this);
      break;
    default:
      break;
  }
}

void NewAnimation::on_m_AnimName_textChanged(const QString &text)
{
  const QString trimmed_text = text.trimmed();

  m_ButtonBox->setStandardButtons((trimmed_text.isEmpty() ? QDialogButtonBox::NoButton : QDialogButtonBox::Ok) | QDialogButtonBox::Cancel);
}

void NewAnimation::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
  {
    if (focusNextChild())
    {
      event->accept();
    }
  }
  else
  {
    QDialog::keyPressEvent(event);
  }
}
