#ifndef SRSM_ANIMATION_PREVIEW_HPP
#define SRSM_ANIMATION_PREVIEW_HPP

//#include "bifrost_sprite_animation_api.h" // BifrostAnimation2DCtx

#include "srsm_animated_sprite.hpp"  // AnimatedSprite

#include <QGraphicsView>
#include <QTimer>

namespace Ui
{
  class AnimationPreview;
}

struct Animation;

class AnimationPreview : public QGraphicsView
{
  Q_OBJECT

 private:
  QGraphicsScene        m_Scene;
  QTimer                m_UpdateLoop;
  AnimatedSprite*       m_Sprite;
  Ui::AnimationPreview* ui;
  //BifrostAnimation2DCtx* m_Ctx;

 public:
  explicit AnimationPreview(QWidget* parent = nullptr);
  ~AnimationPreview();

  //void setContext(BifrostAnimation2DCtx* ctx);
  void mainUpdateLoop();

 public slots:
  void onAnimationSelected(Animation* anim);

  // QWidget interface
 protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
};

#endif  // SRSM_ANIMATION_PREVIEW_HPP
