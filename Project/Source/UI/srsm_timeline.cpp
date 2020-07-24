#include "srsm_timeline.hpp"
#include "ui_srsm_timeline.h"

#include "Data/srsm_project.hpp"

#include <QDebug>
#include <QPainter>
#include <QWheelEvent>

#include <cmath>  // round

// UI Constants //

static const QColor k_BackgroundColor             = QColor(0x45, 0x45, 0x45, 255);
static const QBrush k_BackgroundBrush             = QBrush(k_BackgroundColor);
static const QBrush k_FrameTrackBrush             = QBrush(QColor(0x2B, 0x2B, 0x2B, 255));
static const int    k_FrameTrackPadding           = 2;
static const int    k_FramePadding                = 5;
static const int    k_DblFramePadding             = k_FramePadding * 2;
static const float  k_MinFrameTime                = 1.0f / 240.0f;
static const int    k_FrameInnerPadding           = 2;
static const QBrush k_FrameInnerPaddingHightlight = QBrush(QColor(200, 200, 200, 40));
static const QBrush k_FrameInnerPaddingShading    = QBrush(QColor(20, 20, 20, 200));
static const QColor k_TickMarkColor               = QColor(20, 20, 20, 200);

// NOTE(SR):
//   Functions because QRect is stupid...
//   Just read the docs for QRect::right and QRect::bottom :(

static int trueRight(const QRect& r)
{
  return r.x() + r.width();
}

static int trueBottom(const QRect& r)
{
  return r.y() + r.height();
}

// Math Helpers

static int roundToLowerMultiple(int n, int grid_size)
{
  return (n / grid_size) * grid_size;
}

static int roundToUpperMultiple(int n, int grid_size)
{
  const int remainder = grid_size == 0 ? 0 : n % grid_size;

  return (remainder == 0) ? n : n + grid_size - remainder;
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

// TImeline Class

Timeline::Timeline(QWidget* parent) :
  QWidget(parent),
  ui(new Ui::Timeline),
  m_FrameHeight{0},
  m_CurrentAnimation{nullptr},
  m_AtlasExport{nullptr},
  m_FrameInfos{},
  m_DraggedFrameInfo{},
  m_DraggedOffset{0, 0},
  m_DragMode{FrameDragMode::None},
  m_HoveredRect{0, 0, 0, 0}
{
  ui->setupUi(this);

  installEventFilter(this);

  onFrameSizeChanged(100);
  setMouseTracking(true);
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

void Timeline::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  QFont font = painter.font();
  font.setPixelSize(16);
  painter.setFont(font);

  const QRect background_rect = rect();

  // Draw background

  painter.fillRect(background_rect, k_BackgroundBrush);

  // Draw Frame Track

  const int   frame_track_height = qMin(m_FrameHeight, background_rect.height() - k_FrameTrackPadding * 2);
  const QRect track_rect         = QRect(background_rect.x(), background_rect.y() + (background_rect.height() - frame_track_height) / 2, background_rect.width(), frame_track_height);
  QRect       track_rect_outline = track_rect;

  track_rect_outline.setTop(track_rect_outline.top() - 1);
  track_rect_outline.setBottom(track_rect_outline.bottom() + 1);

  painter.fillRect(track_rect_outline, QColor(255, 255, 255, 80));

  painter.fillRect(track_rect, k_FrameTrackBrush);

  // Draw Tick Marks

  painter.setPen(k_TickMarkColor);

  for (int x = 0; x < background_rect.width(); x += m_FrameHeight)
  {
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
      if (m_DragMode != FrameDragMode::None && i == m_DraggedFrameInfo)
      {
        continue;
      }

      drawFrame(atlas_image, painter, i);
    }

    if (m_DragMode != FrameDragMode::None)
    {
      // The dragged frame needs to be drawn on top.

      drawFrame(atlas_image, painter, m_DraggedFrameInfo);

      switch (m_DragMode)
      {
        case FrameDragMode::None:
          break;
        case FrameDragMode::Image:
        {
          // painter.fillRect(m_FrameInfos[m_DraggedFrameInfo].image, Qt::lightGray);
          break;
        }
        case FrameDragMode::Left:
          painter.fillRect(m_FrameInfos[m_DraggedFrameInfo].left_resize, Qt::lightGray);
          break;
        case FrameDragMode::Right:
        {
          AnimationFrame* const frame           = m_CurrentAnimation->frameAt(m_DraggedFrameInfo);
          const float           base_frame_time = 1.0f / float(m_CurrentAnimation->frame_rate);
          const float           frame_time      = frame->frame_time();
          const float           num_frames      = frame_time / base_frame_time;
          const QString         test_str        = tr("%1 ms / ~%2 frame%3").arg(frame_time).arg(num_frames).arg(num_frames > 1.0 ? "s" : "");
          QPainterPath          text_path;

          painter.fillRect(m_FrameInfos[m_DraggedFrameInfo].right_resize, Qt::lightGray);

          text_path.addText(local_mouse_pos, painter.font(), test_str);

          painter.strokePath(text_path, QPen(Qt::black, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
          painter.fillPath(text_path, Qt::white);
          break;
        }
      }
    }
    else if (!m_HoveredRect.isEmpty())
    {
      painter.fillRect(m_HoveredRect, QColor(200, 200, 200, 80));
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

  const QPoint local_mouse_pos = event->pos();

  int frame_index = 0;
  for (auto& frame : m_FrameInfos)
  {
    if (frame.left_resize.contains(local_mouse_pos))
    {
      m_DraggedFrameInfo = frame_index;
      m_DraggedOffset    = frame.left_resize.topLeft() - local_mouse_pos;
      m_DragMode         = FrameDragMode::Left;
      break;
    }

    if (frame.right_resize.contains(local_mouse_pos))
    {
      m_DraggedFrameInfo = frame_index;
      m_DraggedOffset    = frame.right_resize.topLeft() - local_mouse_pos;
      m_DragMode         = FrameDragMode::Right;
      break;
    }

    if (frame.image.contains(local_mouse_pos))
    {
      m_DraggedFrameInfo = frame_index;
      m_DraggedOffset    = frame.image.topLeft() - local_mouse_pos;
      m_DragMode         = FrameDragMode::Image;
      break;
    }

    ++frame_index;
  }

  update();
}

void Timeline::mouseMoveEvent(QMouseEvent* event)
{
  const QPoint local_mouse_pos = event->pos();
  const QPoint rel_pos         = local_mouse_pos + m_DraggedOffset;

  m_HoveredRect = QRect(0, 0, 0, 0);

  switch (m_DragMode)
  {
    case FrameDragMode::None:
    {
      int frame_index = 0;
      for (auto& frame : m_FrameInfos)
      {
#if 0
        if (frame.left_resize.contains(local_mouse_pos, true))
        {
          m_HoveredRect = frame.left_resize;
          break;
        }
#endif
        if (frame.right_resize.contains(local_mouse_pos, true))
        {
          m_HoveredRect = frame.right_resize;
          break;
        }

        if (frame.image.contains(local_mouse_pos, true))
        {
          m_HoveredRect = frame.image;
          break;
        }

        ++frame_index;
      }

      break;
    }
    case FrameDragMode::Image:
    {
      bool needs_recalc = false;

      m_FrameInfos[m_DraggedFrameInfo].image.moveTopLeft(rel_pos);

      int frame_index = 0;
      for (auto& frame : m_FrameInfos)
      {
        if (frame_index != m_DraggedFrameInfo)
        {
          const QRect& base_frame      = frame.image;
          const int    half_base_width = base_frame.width() / 2;
          const QRect& left_frame      = QRect(base_frame.left(), base_frame.top(), half_base_width, INT_MAX / 2);
          const QRect& right_frame     = QRect(base_frame.center().x(), base_frame.top(), half_base_width, INT_MAX / 2);
          const bool   is_to_left      = frame_index < m_DraggedFrameInfo;

          // If I am to the left of a frame that is left of me (and the same for the right side).
          if ((is_to_left && left_frame.contains(local_mouse_pos)) || (!is_to_left && right_frame.contains(local_mouse_pos)))
          {
            m_CurrentAnimation->swapFrames(m_DraggedFrameInfo, frame_index);
            std::swap(m_FrameInfos[m_DraggedFrameInfo], m_FrameInfos[frame_index]);
            m_DraggedFrameInfo = frame_index;
            needs_recalc       = true;
            break;
          }
        }

        ++frame_index;
      }

      if (needs_recalc)
      {
        m_CurrentAnimation->notifyChanged();
      }
      break;
    }
    case FrameDragMode::Left:
      break;
    case FrameDragMode::Right:
    {
      AnimationFrame* const frame           = m_CurrentAnimation->frameAt(m_DraggedFrameInfo);
      const float           base_frame_time = 1.0f / (float)m_CurrentAnimation->frame_rate;
      const int             default_width   = m_FrameHeight;
      const int             snap_width      = default_width / 2;
      QRect&                right_resize    = m_FrameInfos[m_DraggedFrameInfo].right_resize;
      QRect                 frame_rect      = m_FrameInfos[m_DraggedFrameInfo].image;

      right_resize.moveLeft(rel_pos.x());
      frame_rect.setRight(right_resize.right());

      int       frame_width                = frame_rect.width();
      const int lower_snap                 = roundToLowerMultiple(frame_width, snap_width);
      const int upper_snap                 = roundToUpperMultiple(frame_width, snap_width);
      const int distance_from_lower_stap   = std::abs(lower_snap - frame_width);
      const int distance_from_upper_stap   = std::abs(upper_snap - frame_width);
      const int distance_from_nearest_stap = std::min(distance_from_lower_stap, distance_from_upper_stap);

      // qDebug() << "Width: " << frame_rect.width() << "Lower Snap:" << lower_snap << ", Dist:" << distance_from_lower_stap << "Upper Snap:" << upper_snap << ", Dist:" << distance_from_upper_stap;

      if (distance_from_nearest_stap <= 2)
      {
        frame_width = distance_from_nearest_stap == distance_from_lower_stap ? lower_snap : upper_snap;
      }

      const float new_frame_time = qMax(base_frame_time * (float(frame_width) / float(default_width)), k_MinFrameTime);

      // qDebug() << "new_frame_time: " << new_frame_time;

      frame->setFrameTime(new_frame_time);

      recalculateTimelineSize();
      break;
    }
  }

  (void)event;

  update();
}

void Timeline::mouseReleaseEvent(QMouseEvent* event)
{
  (void)event;

  m_HoveredRect = QRect(0, 0, 0, 0);

  switch (m_DragMode)
  {
    case FrameDragMode::None:
      break;
    case FrameDragMode::Image:
      recalculateTimelineSize();
      break;
    case FrameDragMode::Left:
      recalculateTimelineSize();
      break;
    case FrameDragMode::Right:
      recalculateTimelineSize();
      break;
  }

  m_DragMode = FrameDragMode::None;

  update();
}

void Timeline::resizeEvent(QResizeEvent* event)
{
  (void)event;

  recalculateTimelineSize();
}

void Timeline::recalculateTimelineSize()
{
  m_FrameInfos.clear();

  if (m_CurrentAnimation)
  {
    const QRect background_rect    = rect();
    const int   frame_track_height = qMin(m_FrameHeight, background_rect.height() - k_FrameTrackPadding * 2);
    const QRect track_rect         = QRect(background_rect.x(), background_rect.y() + (background_rect.height() - frame_track_height) / 2, background_rect.width(), frame_track_height);
    const int   num_frames         = numFrames();
    const int   frame_top          = track_rect.top() + k_FramePadding;
    const int   frame_height       = track_rect.height() - k_DblFramePadding;
    const float base_frame_time    = 1.0f / (float)m_CurrentAnimation->frame_rate;
    int         current_x          = 0;

    for (int i = 0; i < num_frames; ++i)
    {
      AnimationFrame* const frame             = m_CurrentAnimation->frameAt(i);
      const float           frame_width_scale = frame->frame_time() / base_frame_time;
      const int             frame_width       = int(m_FrameHeight * frame_width_scale);
      const QRect           resize_left       = QRect(current_x, frame_top, k_FramePadding, frame_height);
      const QRect           frame_rect        = QRect(current_x + k_FramePadding, frame_top, frame_width - k_DblFramePadding, frame_height);
      const QRect           resize_right      = QRect(trueRight(frame_rect), frame_top, k_FramePadding, frame_height);

      m_FrameInfos.emplace_back(frame_rect, resize_left, resize_right);

      current_x = trueRight(resize_right);
    }

    setMinimumSize(current_x, 0);
  }

  update();
}

int Timeline::numFrames() const
{
  return m_CurrentAnimation ? m_CurrentAnimation->frame_list.rowCount() : 0;
}

void Timeline::drawFrame(const QPixmap& atlas_image, QPainter& painter, int index)
{
  AnimationFrame* const frame          = m_CurrentAnimation->frameAt(index);
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

  const auto&  frame_rect_info = m_FrameInfos[index];
  const QRect& frame_rect      = frame_rect_info.image;
  const QRect  pixmap_src      = m_AtlasExport->frame_rects.at(frame_index);
  const QRect  pixmap_dst      = aspectRatioDrawRegion(pixmap_src.width(), pixmap_src.height(), frame_rect.width() - 1, frame_rect.height() - 1).translated(frame_rect.topLeft());

  painter.fillRect(frame_rect, k_BackgroundBrush);

  painter.drawPixmap(
   pixmap_dst,
   atlas_image,
   pixmap_src);

  // Draw Inner Framing

  // Bottom Shade
  painter.fillRect(frame_rect.left(), trueBottom(frame_rect) - k_FrameInnerPadding, frame_rect.width(), k_FrameInnerPadding, k_FrameInnerPaddingShading);

  // Right Shade
  painter.fillRect(trueRight(frame_rect) - k_FrameInnerPadding, frame_rect.top(), k_FrameInnerPadding, frame_rect.height(), k_FrameInnerPaddingShading);

  // Top Highlight
  painter.fillRect(frame_rect.left(), frame_rect.top(), frame_rect.width(), k_FrameInnerPadding, k_FrameInnerPaddingHightlight);

  // Left Highlight
  painter.fillRect(frame_rect.left(), frame_rect.top(), k_FrameInnerPadding, frame_rect.height(), k_FrameInnerPaddingHightlight);
}
