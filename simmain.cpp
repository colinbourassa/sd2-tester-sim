#include "simmain.h"
#include <QMessageBox>
#include <QString>
#include <QFile>
#include "ui_simmain.h"

SimMain::SimMain(const QString& domainSockName, QWidget* parent)
  : QMainWindow(parent)
  , ui(new Ui::SimMain)
{
  ui->setupUi(this);
  ui->domainSocketLine->setText(domainSockName);
  connect(&m_sim, &TesterSim::logMsg, this, &SimMain::onLogMsg);
  connect(&m_sim, &TesterSim::lastLogMsgRepeated, this, &SimMain::onLastLogMsgRepeated);
}

SimMain::~SimMain()
{
  m_sim.stopListening();
  if (m_simthread.joinable())
  {
    m_simthread.join();
  }
  delete ui;
}

void SimMain::listenOnSock(SimMain* sim)
{
  sim->log("Started listening thread.\n");
  sim->m_sim.listen();
}

void SimMain::on_connectButton_clicked()
{
  const QString domainSockName = ui->domainSocketLine->text();
  if (m_sim.connectToSocket(domainSockName))
  {
    ui->connectButton->setEnabled(false);
    ui->startListeningButton->setEnabled(true);
  }
  else
  {
    log(QString("Could not connect to domain socket '%1'").arg(domainSockName));
  }
}

void SimMain::on_startListeningButton_clicked()
{
  m_simthread = std::thread(SimMain::listenOnSock, this);
}

void SimMain::on_setRAMButton_clicked()
{
  uint16_t addr = ui->addrBox->text().toUInt(nullptr, 16);
  uint8_t val = ui->valBox->text().toUInt(nullptr, 16);
  m_sim.setRAMLoc(addr, val);
}

void SimMain::on_loadStateButton_clicked()
{
  if (m_sim.loadState(m_stateFilename))
  {
    log(QString("Loaded state from file '%1'").arg(m_stateFilename));
  }
  else
  {
    log(QString("Failed to load state from file '%1'").arg(m_stateFilename));
  }
}

void SimMain::on_saveStateButton_clicked()
{
  ui->logView->addDot(); return;
  bool proceed = true;

  if (QFile::exists(m_stateFilename))
  {
    proceed = (QMessageBox::question(this, "Overwrite",
      QString("'%1' exists. Overwrite?").arg(m_stateFilename)) == QMessageBox::Yes);
  }

  if (proceed)
  {
    if (m_sim.saveState(m_stateFilename))
    {
      log(QString("Saved state to file '%1'").arg(m_stateFilename));
    }
    else
    {
      log(QString("Failed to save state to file '%1'").arg(m_stateFilename));
    }
  }
}

void SimMain::log(const QString& line)
{
  ui->logView->appendLine(line);
}

void SimMain::onLogMsg(const QString& line)
{
  log(line);
}

void SimMain::onLastLogMsgRepeated()
{
  ui->logView->addDot();
}

