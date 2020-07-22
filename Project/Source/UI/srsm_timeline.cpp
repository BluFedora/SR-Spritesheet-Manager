#include "srsm_timeline.hpp"
#include "ui_srsm_timeline.h"

#include "Data/srsm_project.hpp"

#include <QDebug>
#include <QPainter>
#include <QWheelEvent>

// UI Constants //

static const QColor k_BackgroundColor   = QColor(0x45, 0x45, 0x45, 255);
static const QBrush k_BackgroundBrush   = QBrush(k_BackgroundColor);
static const QBrush k_FrameTrackBrush   = QBrush(QColor(0x2B, 0x2B, 0x2B, 255));
static const int    k_FrameTrackPadding = 5;
static const int    k_FramePadding      = 10;
static const int    k_DblFramePadding   = k_FramePadding * 2;
static const int    k_HalfFramePadding  = k_FramePadding / 2;

Timeline::Timeline(QWidget* parent) :
  QWidget(parent),
  ui(new Ui::Timeline),
  m_FrameHeight{0},
  m_CurrentAnimation{nullptr},
  m_AtlasExport{nullptr}
{
  ui->setupUi(this);

  installEventFilter(this);

  onFrameSizeChanged(100);
}

Timeline::~Timeline()
{
  delete ui;
}

void Timeline::onFrameSizeChanged(int new_value)
{
  m_FrameHeight = new_value;
  recalculateTimelineSize();
}

void Timeline::onAnimationSelected(Animation* anim)
{
  m_CurrentAnimation = anim;
  recalculateTimelineSize();
}

void Timeline::onAnimationChanged(Animation* anim)
{
  if (m_CurrentAnimation == anim)
  {
    recalculateTimelineSize();
  }
}

void Timeline::onAtlasUpdated(AtlasExport& atlas)
{
  m_AtlasExport = &atlas;
  recalculateTimelineSize();
}

static QRect aspectRatioDrawRegion(std::uint32_t aspect_w, std::uint32_t aspect_h, std::uint32_t window_w, std::uint32_t window_h)
{
  QRect result = {};

  if (aspect_w > 0 && aspect_h > 0 && window_w > 0 && window_h > 0)
  {
    const float optimal_w = float(window_h) * (float(aspect_w) / float(aspect_h));
    const float optimal_h = float(window_w) * (float(aspect_h) / float(aspect_w));

    if (optimal_w > float(window_w))
    {
      result.setRight(int(window_w));
      result.setTop(qRound(0.5f * (window_h - optimal_h)));
      result.setBottom(int(result.top() + optimal_h));
    }
    else
    {
      result.setBottom(int(window_h));
      result.setLeft(qRound(0.5f * (window_w - optimal_w)));
      result.setRight(int(result.left() + optimal_w));
    }
  }

  return result;
}

void Timeline::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRect background_rect = rect();

  // Draw background

  painter.fillRect(background_rect, k_BackgroundBrush);

  // Draw Frame Track

  const int   frame_track_height = qMin(m_FrameHeight, background_rect.height() - k_FrameTrackPadding * 2);
  const QRect track_rect         = QRect(background_rect.x(), background_rect.y() + (background_rect.height() - frame_track_height) / 2, background_rect.width(), frame_track_height);

  painter.fillRect(track_rect, k_FrameTrackBrush);

  // Draw Tick Marks

  for (int x = k_HalfFramePadding; x < background_rect.width(); x += m_FrameHeight - k_FramePadding)
  {
    painter.setPen(Qt::magenta);
    painter.drawLine(x, 0, x, background_rect.height());
  }

  // Draw Frames

  if (m_AtlasExport && m_CurrentAnimation)
  {
    QPixmap&     atlas_image     = m_AtlasExport->pixmap;
    const int    num_frames      = numFrames();
    const QPoint local_mouse_pos = mapFromGlobal(QCursor::pos());

    for (int i = 0; i < num_frames; ++i)
    {
      AnimationFrame* const frame          = m_CurrentAnimation->frameAt(i);
      const auto            frame_index_it = m_AtlasExport->abs_path_to_index.find(frame->full_path());
      std::uint32_t         frame_index    = 0;

      if (frame_index_it == m_AtlasExport->abs_path_to_index.end())
      {
        qDebug() << "Failed to find frame for: " << frame->full_path();
      }
      else
      {
        frame_index = *frame_index_it;
      }

      const auto   frame_rect_info = frameRectInfo(track_rect, i);
      const QRect& frame_rect      = frame_rect_info.image;
      const QRect  pixmap_src      = m_AtlasExport->frame_rects.at(frame_index);
      const QRect  pixmap_dst      = aspectRatioDrawRegion(pixmap_src.width(), pixmap_src.height(), frame_rect.width() - 1, frame_rect.height() - 1).translated(frame_rect.topLeft());
      const QRect& resize_left     = frame_rect_info.left_resize;
      const QRect& resize_right    = frame_rect_info.right_resize;

      painter.fillRect(frame_rect, k_BackgroundBrush);

      painter.drawPixmap(
       pixmap_dst,
       atlas_image,
       pixmap_src);

      if (frame_rect.contains(local_mouse_pos))
      {
        painter.setPen(Qt::red);
        painter.drawRect(frame_rect);
      }

      if (resize_left.contains(local_mouse_pos))
      {
        painter.setPen(Qt::green);
        painter.drawRect(resize_left);
      }

      if (resize_right.contains(local_mouse_pos))
      {
        painter.setPen(Qt::blue);
        painter.drawRect(resize_right);
      }
    }
  }
}

void Timeline::wheelEvent(QWheelEvent* event)
{
  event->ignore();
}

bool Timeline::eventFilter(QObject* watched, QEvent* event)
{
  if (event->type() == QEvent::Wheel)
  {
    return false;
  }

  return QWidget::eventFilter(watched, event);
}

void Timeline::mousePressEvent(QMouseEvent* event)
{
  (void)event;

  update();
}

void Timeline::mouseReleaseEvent(QMouseEvent* event)
{
  (void)event;

  update();
}

void Timeline::mouseMoveEvent(QMouseEvent* event)
{
  (void)event;

  update();
}

void Timeline::recalculateTimelineSize()
{
  if (m_CurrentAnimation)
  {
    setMinimumSize(k_FramePadding + (m_FrameHeight - k_FramePadding) * numFrames(), 0);
  }
  update();
}

int Timeline::numFrames() const
{
  return m_CurrentAnimation ? m_CurrentAnimation->frame_list.rowCount() : 0;
}

FrameRectInfo Timeline::frameRectInfo(const QRect& track_rect, int index)
{
  // TODO(Shareef): These first few calculations are uniform across all frames, could be precalculated.

  const int   frame_top    = track_rect.top() + k_FramePadding;
  const int   frame_width  = m_FrameHeight - k_DblFramePadding;
  const int   frame_height = track_rect.height() - k_DblFramePadding;
  const int   current_x    = k_FramePadding + (frame_width + k_FramePadding) * index;
  const QRect frame_rect   = QRect(current_x, frame_top, frame_width, frame_height);
  const QRect resize_left  = QRect(frame_rect.left() - k_HalfFramePadding, frame_rect.y(), k_HalfFramePadding, frame_rect.height());
  const QRect resize_right = QRect(frame_rect.right(), frame_rect.y(), k_HalfFramePadding, frame_rect.height());

  return {frame_rect, resize_left, resize_right};
}
