#include "srsm_animation_preview.hpp"

#include "Data/srsm_animation.hpp"
#include "ui_srsm_animation_preview.h"

#include <QOpenGLWidget>
#include <QGraphicsTextItem>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QListView>
#include <QDebug>

AnimationPreview::AnimationPreview(QWidget *parent) :
  QGraphicsView(parent),
  m_Scene{},
  //m_Ctx{nullptr},
  m_UpdateLoop{},
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

  setBackgroundBrush(QBrush(Qt::lightGray, Qt::SolidPattern));

  // Setup Optimizations and Other Settings
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setResizeAnchor(AnchorViewCenter);
  setInteractive(true);
  setTransformationAnchor(AnchorUnderMouse);
  setDragMode(QGraphicsView::RubberBandDrag);
  //setCacheMode(QGraphicsView::CacheBackground);
  //setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
 // setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);

#if 0
  auto* const txt = m_Scene.addText("THIS IS SOME TEST TEXT");

  txt->setDefaultTextColor(Qt::red);
  txt->setFlag(QGraphicsItem::ItemIsMovable, true);
  txt->setFlag(QGraphicsItem::ItemIsSelectable, true);

  auto* const img = m_Scene.addPixmap((QPixmap(":/Res/Images/reefy_profile_no_bg.png")));

  img->setScale(0.5f);
  img->setFlag(QGraphicsItem::ItemIsMovable, true);
  img->setFlag(QGraphicsItem::ItemIsSelectable, true);
#endif

  setScene(&m_Scene);

  m_UpdateLoop.start(std::chrono::milliseconds(16));

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
    QObject* const evt_source = event->source();
    QListView* const item_model = dynamic_cast<QListView*>(evt_source);

    if (!item_model)
    {
      return;
    }

    QByteArray encoded = event->mimeData()->data(k_MimeFormat);
    QDataStream stream(&encoded, QIODevice::ReadOnly);

    while (!stream.atEnd())
    {
      int row, col;
      QMap<int,  QVariant> roleDataMap;
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
