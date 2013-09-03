#include "clientconnectionmanager.h"

#include "client.h"
#include "clienttoolmodel.h"

#include <common/network/objectbroker.h>

#include <ui/mainwindow.h>
#include <ui/splashscreen.h>

#include <QApplication>
#include <QMessageBox>
#include <QTimer>

using namespace GammaRay;

ClientConnectionManager::ClientConnectionManager(QObject* parent) :
  QObject(parent),
  m_port(0),
  m_client(new Client(this)),
  m_mainWindow(0),
  m_toolModel(0)
{
  showSplashScreen();

  connect(m_client, SIGNAL(connectionEstablished()), SLOT(connectionEstablished()));
  connect(m_client, SIGNAL(connectionError(QAbstractSocket::SocketError,QString)), SLOT(connectionError(QAbstractSocket::SocketError,QString)));
}

ClientConnectionManager::~ClientConnectionManager()
{
}

void ClientConnectionManager::connectToHost(const QString& hostname, quint16 port)
{
  m_hostname = hostname;
  m_port = port;
  m_connectionTimeout.start();
  connectToHost();
}

void ClientConnectionManager::connectToHost()
{
  m_client->connectToHost(m_hostname, m_port);
}

void ClientConnectionManager::connectionEstablished()
{
  m_toolModel = new ClientToolModel(this);
  ObjectBroker::registerModel("com.kdab.GammaRay.ToolModel", m_toolModel);

  if (m_toolModel->rowCount() <= 0) {
    connect(m_toolModel, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(toolModelPopulated()));
    connect(m_toolModel, SIGNAL(layoutChanged()), SLOT(toolModelPopulated()));
    connect(m_toolModel, SIGNAL(modelReset()), SLOT(toolModelPopulated()));
  } else {
    toolModelPopulated();
  }
}

void ClientConnectionManager::toolModelPopulated()
{
  if (m_toolModel->rowCount() <= 0)
    return;

  disconnect(m_toolModel, 0, this, 0);

  m_mainWindow = new MainWindow;
  m_mainWindow->show();
  hideSplashScreen();
}

void ClientConnectionManager::connectionError(QAbstractSocket::SocketError error, const QString& msg)
{
  if (m_connectionTimeout.elapsed() < 60 * 1000 && error == QAbstractSocket::ConnectionRefusedError) {
    // client wasn't up yet, keep trying
    QTimer::singleShot(1000, this, SLOT(connectToHost()));
    return;
  }

  hideSplashScreen();

  if (m_mainWindow) {
    QMessageBox::warning(m_mainWindow, tr("GammaRay - Connection Error"),
                         tr("Lost connection to remote host: %1").arg(msg));
    // interaction may result in assertions
    // this keeps the actions alive though so we could e.g. offer a "reconnect" feature or similar
    // in the future. Also, the user can e.g. accept the above dialog and look at the backtrace
    // from the message handler if that showed up.
    m_mainWindow->centralWidget()->setEnabled(false);
  } else {
    QMessageBox::critical(m_mainWindow, tr("GammaRay - Connection Error"),
                          tr("Could not establish connection to remote host: %1").arg(msg));
    QApplication::exit(1);
  }

}

#include "clientconnectionmanager.moc"
