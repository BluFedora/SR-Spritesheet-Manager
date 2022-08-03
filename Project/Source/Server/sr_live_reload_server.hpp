#ifndef LIVERELOADSERVER_HPP
#define LIVERELOADSERVER_HPP

#include "sprite_anim/bf_sprite_animation.hpp" // k_ServerPort

#include <QBuffer>     // QBuffer
#include <QImage>      // QImage
#include <QObject>     // QObject
#include <QTcpServer>  // QTcpServer
#include <QTcpSocket>  // QTcpSocket

class LiveReloadServer final : public QObject
{
  Q_OBJECT

 private:
  QTcpServer            m_Server;
  QVector<QTcpSocket *> m_Clients;
  qint64                m_NumBytesNeedSend;
  qint64                m_NumBytesSent;

 public:
  LiveReloadServer();

  qint64 numBytesNeedSend() const { return m_NumBytesNeedSend; }
  qint64 numBytesSent() const { return m_NumBytesSent; }
  bool   startServer() { return m_Server.listen(QHostAddress::LocalHost, SpriteAnim::k_ServerPort); }

  void setup();
  void sendTextureChangedPacket(const char *guid, const QImage &atlas_image);
  void sendAnimChangedPacket(const char *guid, const QBuffer &srsm_byte_buffer);

  ~LiveReloadServer();

 signals:
  void bytesSent();

 private slots:
  void onClientBytesSent(qint64 num_bytes);
  void onClientDisconnect();
};

extern std::unique_ptr<LiveReloadServer> g_Server;

#endif  // LIVERELOADSERVER_HPP
