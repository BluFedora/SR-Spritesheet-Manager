//
// SR Spritesheet Manager
//
// file:   srsm_animation_preview.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "srsm_animation_preview.hpp"

#include "ui_srsm_animation_preview.h"

#include "Data/srsm_animation.hpp"
#include "Data/srsm_project.hpp"
#include "UI/srsm_image_library.hpp"

#include <QDragEnterEvent>
#include <QGraphicsTextItem>
#include <QListView>
#include <QMimeData>
#include <QOpenGLWidget>

static constexpr qreal k_ScaleFactor     = 1.1;
static constexpr qreal k_InvScaleFactor  = 1.0 / k_ScaleFactor;
static constexpr qreal k_FitInViewGutter = 50.0f;

AnimationPreview::AnimationPreview(QWidget* parent) :
  QGraphicsView(parent),
  m_Atlas{nullptr},
  m_Scene{},
  m_UpdateLoop{},
  m_Sprite{nullptr},
  m_NoSelectedAnimPixmap{":/Res/Images/Runtime/no-animation-selected.png"},
  m_CurrentAnim{nullptr},
  m_AtlasPixmap{},
  m_AnimCtx{nullptr},
  m_Anim2DScene{nullptr},
  m_AnimNewlySelected{false},
  m_IsPlayingAnimation{false},
  ui(new Ui::AnimationPreview)
{
  ui->setupUi(this);

  setAcceptDrops(true);

  // Setup The Animation Context

  const bfAnimation2DCreateParams create_anim_ctx = {nullptr, nullptr, nullptr};

  m_AnimCtx = bfAnimation2D_new(&create_anim_ctx);

  m_Anim2DScene = bfAnimation2D_createScene(m_AnimCtx);

  // Setup OpenGL Rendering

  QOpenGLWidget* gl = new QOpenGLWidget();

  QSurfaceFormat gl_format;
  gl_format.setSamples(4);

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
  scene()->addItem(m_Sprite);

  centerOn(m_Sprite);

  // Main Loop Setup

  m_UpdateLoop.start(std::chrono::milliseconds(16));

  QObject::connect(&m_UpdateLoop, &QTimer::timeout, this, &AnimationPreview::mainUpdateLoop);
}

AnimationPreview::~AnimationPreview()
{
  bfAnimation2D_destroyScene(m_AnimCtx, m_Anim2DScene);
  bfAnimation2D_delete(m_AnimCtx);
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
    QListView* const item_model = dynamic_cast<QListView*>(evt_source);

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

      Animation* const anim = dynamic_cast<Animation*>(static_cast<QStandardItemModel*>(item_model->model())->item(row, col));

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

    const qreal scale_factor = event->delta() > 0 ? k_ScaleFactor : k_InvScaleFactor;

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
  if (event->key() == Qt::Key_Space)
  {
    if (!event->isAutoRepeat())
    {
      setDragMode(QGraphicsView::ScrollHandDrag);
    }
  }
  else if (event->key() == Qt::Key_0)
  {
    resetTransform();
    centerOn(m_Sprite);
  }
  else if (event->key() == Qt::Key_1)
  {
    fitSpriteIntoView();
  }
  else
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

void AnimationPreview::onAnimationSelected(Animation* anim)
{
  m_CurrentAnim = anim;

  if (!anim)
  {
    m_Sprite->setPixmap(m_NoSelectedAnimPixmap);
  }

  m_AnimNewlySelected = true;

  fitSpriteIntoView();
}

void AnimationPreview::onAtlasUpdated(AtlasExport& atlas)
{
  m_Atlas       = &atlas;
  m_AtlasPixmap = atlas.pixmap;

  onFrameSelected(m_CurrentAnim, 0);

  fitSpriteIntoView();
}

void AnimationPreview::onFrameSelected(Animation* anim, int index)
{
  const int num_frames = m_CurrentAnim ? m_CurrentAnim->numFrames() : 0;

  if (anim == m_CurrentAnim && index < num_frames && index >= 0)
  {
    const QRect& frame_rect = m_Atlas->image_rectangles[m_CurrentAnim->frameAt(index)->source->index];

    m_Sprite->setPixmap(m_AtlasPixmap.copy(frame_rect));

    if (m_AnimNewlySelected)
    {
      fitSpriteIntoView();

      m_AnimNewlySelected = false;
    }
  }
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

void AnimationPreview::mainUpdateLoop()
{
  int num_frames;

  if (m_IsPlayingAnimation && m_CurrentAnim && (num_frames = m_CurrentAnim->numFrames()) > 0)
  {
    static float timer = 0.0f;

    if (timer <= 0.0f)
    {
      const int next_frame = (++m_CurrentAnim->previewed_frame) % num_frames;

      onFrameSelected(m_CurrentAnim, next_frame);

      timer = m_CurrentAnim->frameAt(next_frame)->frame_time;
    }

    timer -= 1.0f / 60.0f;
  }
}
