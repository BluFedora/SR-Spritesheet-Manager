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
 public:
  enum State
  {
    None      = 0x0,
    ResizingV = (1 << 0),
    ResizingH = (1 << 1),
    FlipX     = (1 << 2),
    FlipY     = (1 << 3),
  };

  Q_DECLARE_FLAGS(States, State)

 private:
  Project* m_Project;
  QSizeF   m_Size;
  QRectF   m_UvRect;
  States   m_Flags;

 public:
  AnimatedSprite(Project* project);

  // QGraphicsItem Inteface

  QRectF boundingRect() const override { return QRectF(0, 0, m_Size.width(), m_Size.height()); }
  int    type() const override { return QGraphicsItem::UserType + 1; }

  QJsonObject serialize();
  void        deserialize(const QJsonObject& data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AnimatedSprite::States)

#endif  // SRSM_ANIMATED_SPRITE_HPP
