#include "simmain.h"
#include <QMessageBox>
#include <QString>
#include "ui_simmain.h"

SimMain::SimMain(const QString& domainSockName, QWidget* parent)
  : QMainWindow(parent)
  , ui(new Ui::SimMain)
{
  ui->setupUi(this);
  ui->domainSocketLine->setText(domainSockName);
}

SimMain::~SimMain()
{
  delete ui;
}

void SimMain::listenOnSock(SimMain* sim)
{
  printf("Calling TesterSim::listen()...\n");
  sim->m_sim.listen();
}

void SimMain::on_connectButton_clicked()
{
  const QString domainSockName = ui->domainSocketLine->text();
  if (m_sim.connectToSocket(domainSockName))
  {
    ui->connectButton->setEnabled(false);
  }
  else
  {
    QMessageBox::warning(this, "Error",
      QString("Could not connect to domain socket '%1'").arg(domainSockName));
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
  if (!m_sim.loadState(m_stateFilename))
  {
    QMessageBox::warning(this, "Error", QString("Failed to load state from file '%1'").arg(m_stateFilename));
  }
}

void SimMain::on_saveStateButton_clicked()
{
  if (!m_sim.saveState(m_stateFilename))
  {
    QMessageBox::warning(this, "Error", QString("Failed to save state to file '%1'").arg(m_stateFilename));
  }
}
