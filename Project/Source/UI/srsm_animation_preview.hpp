//
// SR Spritesheet Manager
//
// file:   srsm_animation_preview.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef SRSM_ANIMATION_PREVIEW_HPP
#define SRSM_ANIMATION_PREVIEW_HPP

#include "srsm_animated_sprite.hpp"  // AnimatedSprite

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
  Animation*            m_CurrentAnim;
  QPixmap               m_AtlasPixmap;
  bfAnimation2DCtx*     m_AnimCtx;
  bfAnim2DScene*        m_Anim2DScene;
  bool                  m_AnimNewlySelected;
  bool                  m_IsPlayingAnimation;
  Ui::AnimationPreview* ui;

 public:
  explicit AnimationPreview(QWidget* parent = nullptr);

  bool isPlayingAnimation() const { return m_IsPlayingAnimation; }

  ~AnimationPreview();

 public slots:
  void onAnimationSelected(Animation* anim);
  void onAtlasUpdated(AtlasExport& atlas);
  void onFrameSelected(Animation* anim, int index);
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

 private:
  void fitSpriteIntoView();
  void mainUpdateLoop();
};

#endif  // SRSM_ANIMATION_PREVIEW_HPP
