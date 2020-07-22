//
// SR Texture Packer
// Copyright (c) 2020 Shareef Aboudl-Raheem
//

#ifndef FRAMELISTVIEW_HPP
#define FRAMELISTVIEW_HPP

#include <QListView>

struct Animation;

class FrameListView : public QListView
{
    Q_OBJECT

  private:
    Animation* m_CurrentAnimation;

  public:
    FrameListView(QWidget *parent = Q_NULLPTR);

  public slots:
    void onSelectAnimation(Animation* animation);

    // QWidget interface
  protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
};

#endif // FRAMELISTVIEW_HPP
