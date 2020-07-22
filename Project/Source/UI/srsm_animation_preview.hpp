#ifndef SRSM_ANIMATION_PREVIEW_HPP
#define SRSM_ANIMATION_PREVIEW_HPP

//#include "bifrost_sprite_animation_api.h" // BifrostAnimation2DCtx

#include <QGraphicsView>
#include <QTimer>

namespace Ui {
  class AnimationPreview;
}

class AnimationPreview : public QGraphicsView
{
    Q_OBJECT

  private:
    QGraphicsScene         m_Scene;
    //BifrostAnimation2DCtx* m_Ctx;
    QTimer                 m_UpdateLoop;
    Ui::AnimationPreview*  ui;

  public:
    explicit AnimationPreview(QWidget *parent = nullptr);
    ~AnimationPreview();

    //void setContext(BifrostAnimation2DCtx* ctx);
    void mainUpdateLoop();

    // QWidget interface
  protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

#endif // SRSM_ANIMATION_PREVIEW_HPP
