#include "srsm_animation_preview.hpp"

#include "ui_srsm_animation_preview.h"

#include "Data/srsm_animation.hpp"

#include <QDebug>
#include <QDragEnterEvent>
#include <QGraphicsTextItem>
#include <QListView>
#include <QMimeData>
#include <QOpenGLWidget>

static constexpr qreal k_ScaleFactor    = 1.1;
static constexpr qreal k_InvScaleFactor = 1.0 / k_ScaleFactor;

AnimationPreview::AnimationPreview(QWidget* parent) :
  QGraphicsView(parent),
  m_Scene{},
  m_UpdateLoop{},
  m_Sprite{nullptr},
  ui(new Ui::AnimationPreview)
{
  ui->setupUi(this);

  setAcceptDrops(true);

  // Setup OpenGL Rendering
  QOpenGLWidget* gl = new QOpenGLWidget();

  QSurfaceFormat gl_format;
  gl_format.setSamples(4);

  gl->setFormat(gl_format);

  setViewport(gl);

  // Setup Optimizations and Other Settings
  // setBackgroundBrush(QBrush(Qt::lightGray, Qt::Dense2Pattern));
  setBackgroundBrush(QBrush(QPixmap(":/Res/Images/Runtime/bg-grid.png")));
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setResizeAnchor(AnchorViewCenter);
  setInteractive(true);
  setTransformationAnchor(AnchorUnderMouse);
  setDragMode(QGraphicsView::NoDrag);
  //setCacheMode(QGraphicsView::CacheBackground);
  //setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
  // setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);

  setScene(&m_Scene);
  setSceneRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);

  m_UpdateLoop.start(std::chrono::milliseconds(16));

  m_Sprite = new AnimatedSprite(nullptr);
  m_Sprite->setPixmap(QPixmap(":/Res/Images/Runtime/no-animation-selected.png"));
  scene()->addItem(m_Sprite);

  centerOn(m_Sprite);

  QObject::connect(&m_UpdateLoop, &QTimer::timeout, this, &AnimationPreview::mainUpdateLoop);
}

AnimationPreview::~AnimationPreview()
{
  setScene(nullptr);
  delete ui;
}

#if 0
void AnimationPreview::setContext(BifrostAnimation2DCtx* ctx)
{
 // m_Ctx = ctx;
}
#endif

void AnimationPreview::mainUpdateLoop()
{
  //if (m_Ctx)
  {
  }
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
        qDebug() << "Dropped animation" << anim->data(Qt::DisplayRole);
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
  if (anim)
  {
    if (!m_Sprite)
    {
      // m_Sprite = new AnimatedSprite(nullptr);
      // m_Sprite->setPixmap(QPixmap(":/Res/Images/Runtime/no-animation-selected.png"));
      // scene()->addItem(m_Sprite);
    }
  }
}
