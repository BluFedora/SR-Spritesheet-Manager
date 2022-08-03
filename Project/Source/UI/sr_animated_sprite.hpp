//
// SR Spritesheet Manager
//
// file:   srsm_animated_sprite.hpp
// author: Shareef Abdoul-Raheem
// Copyright (c) 2020 Shareef Abdoul-Raheem
//

#ifndef SRSM_ANIMATED_SPRITE_HPP
#define SRSM_ANIMATED_SPRITE_HPP

#include <QGraphicsPixmapItem>  // QGraphicsPixmapItem

class Project;

class AnimatedSprite : public QGraphicsPixmapItem
{
 private:
  Project* m_Project;
  QRectF   m_UvRect;
  QRectF   m_Bounds;

 public:
  AnimatedSprite(Project* project);

  // QGraphicsItem Inteface

  QRectF boundingRect() const override;
  void   updateBounds();
  int    type() const override { return QGraphicsItem::UserType + 1; }

  void setUVRect(const QRectF& rect) { m_UvRect = rect; }

  // QGraphicsItem interface
 public:
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

#endif  // SRSM_ANIMATED_SPRITE_HPP
