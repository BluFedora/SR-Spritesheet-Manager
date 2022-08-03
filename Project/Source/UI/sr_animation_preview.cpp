//
// SR Spritesheet Animator
//
// file:   sr_animation_preview.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020-2021 Shareef Abdoul-Raheem
//

#include "sr_animation_preview.hpp"

#include "ui_sr_animation_preview.h"

#include "Data/sr_animation.hpp"
#include "Data/sr_project.hpp"
#include "UI/sr_image_library.hpp"

#include <QDragEnterEvent>
#include <QGraphicsTextItem>
#include <QListView>
#include <QMimeData>
#include <QOpenGLWidget>
#include <QPainter>

static constexpr qreal k_ScaleFactor     = 1.1;
static constexpr qreal k_InvScaleFactor  = 1.0 / k_ScaleFactor;
static constexpr qreal k_FitInViewGutter = 50.0f;
static const float     k_DeltaTime       = 1.0f / 60.0f;

AnimationPreview::AnimationPreview(QWidget* parent) :
  QGraphicsView(parent),
  m_Atlas{nullptr},
  m_Scene{},
  m_UpdateLoop{},
  m_Sprite{nullptr},
  m_NoSelectedAnimPixmap{":/Res/Images/Runtime/no-animation-selected.png"},
  m_NoAnimFramesPixmap{":/Res/Images/Runtime/no-frames-in-animation.png"},
  m_SceneDocImage{":/Res/Images/Runtime/scene_docs.png"},
  m_CurrentAnim{nullptr},
  m_CurrentAnimIndex{-1},
  m_Spritesheet{nullptr},
  m_AnimNewlySelected{false},
  m_IsPlayingAnimation{false},
  ui(new Ui::AnimationPreview)
{
  ui->setupUi(this);

  // setAcceptDrops(true);

  // Setup OpenGL Rendering

  QOpenGLWidget* gl = new QOpenGLWidget();

  QSurfaceFormat gl_format;
  // gl_format.setSamples(4);

  gl->setFormat(gl_format);

  setViewport(gl);

  // Set Scene Settings

  // Setup Optimizations and Other Settings
  // setBackgroundBrush(QBrush(Qt::lightGray, Qt::Dense2Pattern));
  setBackgroundBrush(QBrush(QPixmap(":/Res/Images/Runtime/bg-grid.png")));
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setResizeAnchor(AnchorViewCenter);
  setInteractive(true);
  setTransformationAnchor(AnchorUnderMouse);
  setDragMode(QGraphicsView::NoDrag);
  // setCacheMode(QGraphicsView::CacheBackground);
  // setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
  // setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);

  setScene(&m_Scene);
  setSceneRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);

  m_Sprite = new AnimatedSprite(nullptr);
  m_Sprite->setPixmap(m_NoSelectedAnimPixmap);
  m_Sprite->updateBounds();
  scene()->addItem(m_Sprite);

  centerOn(m_Sprite);

  // Main Loop Setup

  m_UpdateLoop.start(std::chrono::milliseconds(16));

  QObject::connect(&m_UpdateLoop, &QTimer::timeout, this, &AnimationPreview::mainUpdateLoop);
}

AnimationPreview::~AnimationPreview()
{
  setScene(nullptr);
  delete ui;
}

void AnimationPreview::dragEnterEvent(QDragEnterEvent* event)
{
  event->acceptProposedAction();
}

void AnimationPreview::dragMoveEvent(QDragMoveEvent* event)
{
  event->acceptProposedAction();
}

void AnimationPreview::dragLeaveEvent(QDragLeaveEvent* event)
{
  event->accept();
}

void AnimationPreview::dropEvent(QDropEvent* event)
{
  static const QString k_MimeFormat = "application/x-qabstractitemmodeldatalist";

  if (event->mimeData()->hasFormat(k_MimeFormat))
  {
    QObject* const   evt_source = event->source();
    QListView* const item_model = static_cast<QListView*>(evt_source);

    if (!item_model)
    {
      return;
    }

    QByteArray  encoded = event->mimeData()->data(k_MimeFormat);
    QDataStream stream(&encoded, QIODevice::ReadOnly);

    while (!stream.atEnd())
    {
      int                 row, col;
      QMap<int, QVariant> roleDataMap;

      stream >> row >> col >> roleDataMap;

      Animation* const anim = static_cast<Animation*>(static_cast<QStandardItemModel*>(item_model->model())->item(row, col));

      if (anim)
      {
        // qDebug() << "Dropped animation" << anim->data(Qt::DisplayRole);
      }
    }
  }

  event->acceptProposedAction();
}

void AnimationPreview::wheelEvent(QWheelEvent* event)
{
  const auto key_mods = QGuiApplication::queryKeyboardModifiers();

  if (key_mods & Qt::ControlModifier)
  {
    // NOTE(SR):
    //   This doesn't handle event->pixelData() correctly yet
    //   so it won't work nicely with multitouch trackpads.

    const qreal scale_factor = event->angleDelta().y() > 0 ? k_ScaleFactor : k_InvScaleFactor;

    scale(scale_factor, scale_factor);

    event->accept();
  }
  else
  {
    QGraphicsView::wheelEvent(event);
  }
}

void AnimationPreview::keyPressEvent(QKeyEvent* event)
{
  // No Modifiers
  if (event->key() == Qt::Key_Space)
  {
    if (!event->isAutoRepeat())
    {
      setDragMode(QGraphicsView::ScrollHandDrag);
    }
  }
  else if (event->modifiers() & Qt::ControlModifier)  // Control Modifier
  {
    if (event->key() == Qt::Key_0)
    {
      fitSpriteOneToOne();
    }
    else if (event->key() == Qt::Key_1)
    {
      fitSpriteIntoView();
    }
  }
  else  // Let Qt Handle it
  {
    QGraphicsView::keyPressEvent(event);
  }
}

void AnimationPreview::keyReleaseEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Space)
  {
    if (!event->isAutoRepeat())
    {
      setDragMode(QGraphicsView::NoDrag);
    }
  }
  else
  {
    QGraphicsView::keyReleaseEvent(event);
  }
}

void AnimationPreview::mouseMoveEvent(QMouseEvent* event)
{
  QGraphicsView::mouseMoveEvent(event);
}

void AnimationPreview::drawForeground(QPainter* painter, const QRectF& rect)
{
  const QSize   image_size             = m_SceneDocImage.size();
  const QPoint  screen_space_top_left  = QPoint(10, 10);
  const QPoint  screen_space_bot_right = screen_space_top_left + QPoint(image_size.width(), image_size.height());
  const QPointF top_left               = mapToScene(screen_space_top_left);
  const QPointF bot_right              = mapToScene(screen_space_bot_right);
  const QRectF  target_rect            = QRectF(top_left, bot_right);
  const QRectF  source_rect            = QRectF(0.0f, 0.0f, image_size.width(), image_size.height());

  // Should always be true since we are projecting screenspace coords into world space but whatever...
  if (rect.intersects(target_rect))
  {
    painter->drawPixmap(target_rect, m_SceneDocImage, source_rect);
  }
}

void AnimationPreview::onAnimationSelected(Animation* anim, int index)
{
  m_CurrentAnimIndex  = index;
  m_AnimNewlySelected = m_CurrentAnim != anim;
  m_CurrentAnim       = anim;

  if (m_CurrentAnim)
  {
    m_Sprite->setPixmap(m_Atlas->pixmap);
    m_Sprite->updateBounds();
    onFrameSelected(m_CurrentAnim);
  }
  else
  {
    m_Sprite->setPixmap(m_NoSelectedAnimPixmap);
    m_Sprite->setUVRect(QRectF(0.0f, 0.0f, 1.0f, 1.0f));
    m_Sprite->updateBounds();
    fitSpriteIntoView();
  }
}

void AnimationPreview::onAnimationChanged(Animation* anim)
{
  if (m_Atlas && anim)
  {
    onAtlasUpdated(*m_Atlas);
  }
}

void AnimationPreview::onAtlasUpdated(AtlasExport& atlas)
{
  m_Spritesheet = (SpriteAnim::Spritesheet*)atlas.atlas_data.get();
  m_Atlas       = &atlas;

  if (m_CurrentAnim)
  {
    m_CurrentAnim->previewed_frame      = 0;
    m_CurrentAnim->previewed_frame_time = 0.0f;

    m_Sprite->setPixmap(m_Atlas->pixmap);
    m_Sprite->updateBounds();
    onFrameSelected(m_CurrentAnim);
  }

  fitSpriteIntoView();
}

void AnimationPreview::onFrameSelected(Animation* anim)
{
  const int num_frames = m_CurrentAnim ? m_CurrentAnim->numFrames() : 0;
  const int index      = m_CurrentAnim ? m_CurrentAnim->previewed_frame : 0;

  if (anim == m_CurrentAnim && index < num_frames && index >= 0)
  {
    const QRect& frame_rect = m_Atlas->image_rectangles[m_CurrentAnim->frameAt(index)->source->index];

    m_Sprite->setPixmap(m_Atlas->pixmap);
    m_Sprite->setUVRect(
     QRectF(
      qreal(frame_rect.x()) / qreal(m_Atlas->pixmap.width()),
      qreal(frame_rect.y()) / qreal(m_Atlas->pixmap.height()),
      qreal(frame_rect.width()) / qreal(m_Atlas->pixmap.width()),
      qreal(frame_rect.height()) / qreal(m_Atlas->pixmap.height())));
    m_Sprite->updateBounds();

    if (m_AnimNewlySelected)
    {
      // fitSpriteIntoView();
      fitSpriteOneToOne();
      m_AnimNewlySelected = false;
    }
  }
  else if (m_CurrentAnim)
  {
    m_Sprite->setPixmap(m_NoAnimFramesPixmap);
    m_Sprite->setUVRect(QRectF(0.0f, 0.0f, 1.0f, 1.0f));
    m_Sprite->updateBounds();
  }

  m_Sprite->update();
}

void AnimationPreview::onTogglePlayAnimation()
{
  m_IsPlayingAnimation = !m_IsPlayingAnimation;
}

void AnimationPreview::fitSpriteIntoView()
{
  resetTransform();
  fitInView(m_Sprite->boundingRect().adjusted(-k_FitInViewGutter, -k_FitInViewGutter, k_FitInViewGutter, k_FitInViewGutter), Qt::KeepAspectRatio);
}

void AnimationPreview::fitSpriteOneToOne()
{
  resetTransform();
  centerOn(m_Sprite);
}

void AnimationPreview::mainUpdateLoop()
{
  if (m_Spritesheet && m_IsPlayingAnimation && m_CurrentAnim && m_CurrentAnim->numFrames() != 0)
  {
    // Timing Conversions:
    //   TimeLeftForFrame = FrameTime - PreviewedTime
    //   FrameTime        = TimeLeftForFrame + PreviewedTime
    //   PreviewedTime    = FrameTime - TimeLeftForFrame
    //
    // PreviewedTime    goes from 0.0f => FrameTime.
    // TimeLeftForFrame goes from FrameTime -> 0.0f

    const int   current_frame      = m_CurrentAnim->previewed_frame;
    const float current_frame_time = m_CurrentAnim->frameAt(current_frame)->frame_time;

    SpriteAnim::SpriteAnimationUpdateInfo anim_input;
    anim_input.playback_speed      = 1.0f;
    anim_input.time_left_for_frame = current_frame_time - m_CurrentAnim->previewed_frame_time;
    anim_input.spritesheet_idx     = 0;
    anim_input.animation_idx       = m_CurrentAnimIndex;
    anim_input.current_frame       = current_frame;
    anim_input.is_looping          = true;

    const SpriteAnim::Spritesheet* spritesheet[] = {m_Spritesheet};

    SpriteAnimation_updateSprites(&anim_input, 1, spritesheet, k_DeltaTime);
    m_CurrentAnim->previewed_frame      = anim_input.current_frame;
    m_CurrentAnim->previewed_frame_time = m_CurrentAnim->frameAt(anim_input.current_frame)->frame_time - anim_input.time_left_for_frame;

    if (std::uint32_t(current_frame) != anim_input.current_frame)
    {
      onFrameSelected(m_CurrentAnim);
    }
  }
}
