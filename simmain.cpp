#include "simmain.h"
#include <QMessageBox>
#include "ui_simmain.h"

SimMain::SimMain(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::SimMain)
{
  ui->setupUi(this);
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
  if (m_sim.connectToSocket(std::string("/home/cmb/vbox-unix-domain-socket-com-port")))
  {
    ui->connectButton->setEnabled(false);
  }
  else
  {
    QMessageBox::warning(this, "Error", "Could not connect");
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

