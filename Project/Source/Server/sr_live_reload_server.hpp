#ifndef LIVERELOADSERVER_HPP
#define LIVERELOADSERVER_HPP

#include "sprite_anim/bf_sprite_animation.hpp"  // k_ServerPort

#include "Data/sr_animation.hpp"

#include <QBuffer>     // QBuffer
#include <QImage>      // QImage
#include <QObject>     // QObject
#include <QTcpServer>  // QTcpServer
#include <QTcpSocket>  // QTcpSocket
#include <QUuid>       // QUuid

struct Animation;

class LiveReloadServer final : public QObject
{
  Q_OBJECT

 private:
  QTcpServer           m_Server;
  QVector<QTcpSocket*> m_Clients;
  qint64               m_NumBytesNeedSend;
  qint64               m_NumBytesSent;

 public:
  LiveReloadServer();

  qint64 numBytesNeedSend() const { return m_NumBytesNeedSend; }
  qint64 numBytesSent() const { return m_NumBytesSent; }
  bool   startServer() { return m_Server.listen(QHostAddress::LocalHost, SpriteAnim::k_ServerPort); }

  void setup();

  void sendAnimationAdded(const QUuid& spritesheet, const Animation& animation, const QMap<QString, std::uint32_t>& frame_to_index);
  void sendAnimationRenamed(const QUuid& spritesheet, const QString& old_name, const Animation& animation, const QMap<QString, std::uint32_t>& frame_to_index);
  void sendAnimationFramesChanged(const QUuid& spritesheet, const Animation& animation, const QMap<QString, std::uint32_t>& frame_to_index);
  void sendAnimationRemoved(const QUuid& spritesheet, const QString& animation_name);
  void sendAtlasTextureChanged(const QUuid& spritesheet, const QImage& atlas_image);

  ~LiveReloadServer();

 signals:
  void bytesSent();

 private slots:
  void onClientBytesSent(qint64 num_bytes);
  void onClientDisconnect();

 private:
  void writeEventHeader(const SpriteAnim::LiveReloadPacketHeader& event);
  void writeString(const QString& str);
  void writeAnimationData(const Animation& animation, const QMap<QString, std::uint32_t>& frame_to_index);

  void packetSizeAddString(SpriteAnim::LiveReloadPacketHeader& event, const QString& str);
  void packetSizeAddAnimation(SpriteAnim::LiveReloadPacketHeader& event, const Animation& animation);
};

extern std::unique_ptr<LiveReloadServer> g_Server;

#endif  // LIVERELOADSERVER_HPP
