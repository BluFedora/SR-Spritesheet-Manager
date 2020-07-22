#ifndef SRSM_TIMELINE_HPP
#define SRSM_TIMELINE_HPP

#include <QWidget>

namespace Ui
{
  class Timeline;
}

struct AtlasExport;
struct Animation;

struct FrameRectInfo final
{
  QRect image;
  QRect left_resize;
  QRect right_resize;
};

class Timeline : public QWidget
{
  Q_OBJECT

 private:
  Ui::Timeline* ui;
  int           m_FrameHeight;
  Animation*    m_CurrentAnimation;
  AtlasExport*  m_AtlasExport;

 public:
  explicit Timeline(QWidget* parent = nullptr);
  ~Timeline();

 public slots:
  void onFrameSizeChanged(int new_value);
  void onAnimationSelected(Animation* anim);
  void onAnimationChanged(Animation* anim);
  void onAtlasUpdated(AtlasExport& atlas);

  // QWidget interface
 protected:
  void paintEvent(QPaintEvent* event) override;

  // QWidget interface
 protected:
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;

  // QObject interface
 public:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void          recalculateTimelineSize();
  int           numFrames() const;
  FrameRectInfo frameRectInfo(const QRect& track_rect, int index);
};

#endif  // SRSM_TIMELINE_HPP
