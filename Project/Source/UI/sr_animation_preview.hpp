//
// SR Spritesheet Animator
//
// file:   sr_animation_preview.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020-2021 Shareef Abdoul-Raheem
//

#ifndef SR_ANIMATION_PREVIEW_HPP
#define SR_ANIMATION_PREVIEW_HPP

#include "sr_animated_sprite.hpp"  // AnimatedSprite

#include "bf/Animation2D.h"

#include <QGraphicsView>
#include <QTimer>

namespace Ui
{
  class AnimationPreview;
}

struct Animation;
struct AtlasExport;

class AnimationPreview : public QGraphicsView
{
  Q_OBJECT

 private:
  AtlasExport*          m_Atlas;
  QGraphicsScene        m_Scene;
  QTimer                m_UpdateLoop;
  AnimatedSprite*       m_Sprite;
  QPixmap               m_NoSelectedAnimPixmap;
  QPixmap               m_NoAnimFramesPixmap;
  QPixmap               m_SceneDocImage;
  Animation*            m_CurrentAnim;
  int                   m_CurrentAnimIndex;
  bfAnim2DCtx*          m_AnimCtx;
  bfSpritesheet*        m_Spritesheet;
  bool                  m_AnimNewlySelected;
  bool                  m_IsPlayingAnimation;
  Ui::AnimationPreview* ui;

 public:
  explicit AnimationPreview(QWidget* parent = nullptr);

  bool isPlayingAnimation() const { return m_IsPlayingAnimation; }

  ~AnimationPreview();

 public slots:
  void onAnimationSelected(Animation* anim, int index);
  void onAtlasUpdated(AtlasExport& atlas);
  void onFrameSelected(Animation* anim);
  void onTogglePlayAnimation(void);

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
  void drawForeground(QPainter* painter, const QRectF& rect) override;

 private:
  void fitSpriteIntoView();
  void fitSpriteOneToOne();
  void mainUpdateLoop();
};

#endif  // SR_ANIMATION_PREVIEW_HPP
