#ifndef SR_WELCOME_WINDOW_H
#define SR_WELCOME_WINDOW_H

#include <QDialog>

namespace Ui {
  class WelcomeWindow;
}

class WelcomeWindow : public QDialog
{
    Q_OBJECT

  private:
    Ui::WelcomeWindow *ui;

public:
    explicit WelcomeWindow(QWidget *parent = nullptr);
    ~WelcomeWindow();

  private slots:
    void on_m_NewProjectBtn_clicked();
    void on_m_OpenProjectBtn_clicked();
};

#endif // SR_WELCOME_WINDOW_H
