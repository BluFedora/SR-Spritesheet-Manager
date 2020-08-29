//
// SR Spritesheet Manager
//
// file:   srsm_animated_sprite.cpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#include "srsm_animated_sprite.hpp"

#include <QPainter>

AnimatedSprite::AnimatedSprite(Project* project) :
  m_Project{project},
  m_UvRect{0.0f, 0.0f, 1.0f, 1.0f}
{
  // setFlags(QGraphicsItem::ItemIsSelectable  | QGraphicsItem::ItemIsMovable);
  setCacheMode(NoCache);
}

QRectF AnimatedSprite::boundingRect() const
{
  const QSize  pixmap_size = pixmap().size();
  const QRectF src_rect    = QRectF(
   0,
   0,
   m_UvRect.width() * pixmap_size.width(),
   m_UvRect.height() * pixmap_size.height());

  return src_rect;
}

void AnimatedSprite::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  (void)option;
  (void)widget;

  // painter->fillRect(boundingRect(), Qt::red);
  // QGraphicsPixmapItem::paint(painter, option, widget);

  const QSize  pixmap_size = pixmap().size();
  const QRectF src_rect    = QRectF(
   m_UvRect.x() * pixmap_size.width(),
   m_UvRect.y() * pixmap_size.height(),
   m_UvRect.width() * pixmap_size.width(),
   m_UvRect.height() * pixmap_size.height());

  painter->drawPixmap(
   boundingRect(),
   pixmap(),
   src_rect);
}
