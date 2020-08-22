#ifndef MAIN_HPP
#define MAIN_HPP

#include "bf/Animation2D.h"  // k_bfSRSMServerPort
#include "bf/anim2D/bf_anim2D_network.h"

#include <QObject>     // QObject
#include <QTcpServer>  // QTcpServer
#include <QTcpSocket>  // QTcpSocket

#include <QTimer>

class LiveReloadServer final : public QObject
{
  Q_OBJECT

 private:
  QTcpServer            m_Server;
  QVector<QTcpSocket *> m_Clients;
  QTimer                m_DataFlow;

 public:
  LiveReloadServer() :
    QObject(nullptr),
    m_Server{nullptr},
    m_Clients{},
    m_DataFlow{}
  {
  }

  bool startServer()
  {
    return m_Server.listen(QHostAddress::LocalHost, k_bfSRSMServerPort);
  }

  void setup()
  {
    QObject::connect(&m_Server, &QTcpServer::newConnection, [this]() {
      QTcpSocket *const client_socket = m_Server.nextPendingConnection();

      qDebug() << "onClientConnect";

      // client_socket->write("Hello client");
      // client_socket->flush();

      //client_socket->waitForBytesWritten(-1);

      QObject::connect(client_socket, &QTcpSocket::disconnected, this, &LiveReloadServer::onClientDisconnect);

      m_Clients.push_back(client_socket);

      //client_socket->close();
    });

    connect(&m_DataFlow, &QTimer::timeout, [this]() {
      for (QTcpSocket *client : m_Clients)
      {
        do {
          char data[128];

          int bytes_read = client->read(data, sizeof(data) - 1);

          if (bytes_read > 0)
          {
            data[bytes_read] = 0;

            qDebug() << "bytes_read = " << bytes_read << " : " << data;
          }
          else
          {
            break;
          }
        } while (true);

        client->write("Hello Again");
      }
    });

    // m_DataFlow.start(1000);
  }

  void sendAnimChangedPacket(const char* guid)
  {
    for (QTcpSocket *client : m_Clients)
    {
      bfAnim2DPacketHeader header;

      header.packet_size = 4 + 1 + 37;
      header.packet_type = bfAnim2DPacketType_TextureChanged;

      client->write((const char*)&header, 5u);
      client->write((const char*)guid, 37);
    }
  }

  ~LiveReloadServer()
  {
    for (QTcpSocket *client : m_Clients)
    {
      client->close();
      client->deleteLater();
    }
  }

 private slots:
  void onClientDisconnect()
  {
    qDebug() << "onClientDisconnect";
    QTcpSocket *const client = static_cast<QTcpSocket *>(QObject::sender());
    m_Clients.removeOne(client);
  }
};

extern std::unique_ptr<LiveReloadServer> g_Server;

#endif  // MAIN_HPP
