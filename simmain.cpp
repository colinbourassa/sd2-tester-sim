#include "simmain.h"
#include <QMessageBox>
#include <QString>
#include <QFile>
#include <QFileDialog>
#include <vector>
#include "ui_simmain.h"

SimMain::SimMain(const QString& domainSockName, QWidget* parent)
  : QMainWindow(parent)
  , ui(new Ui::SimMain)
{
  ui->setupUi(this);
  ui->domainSocketLine->setText(domainSockName);
  connect(&m_sim, &TesterSim::logMsg, this, &SimMain::onLogMsg);
  connect(&m_sim, &TesterSim::lastLogMsgRepeated, this, &SimMain::onLastLogMsgRepeated);
  connect(&m_sim, &TesterSim::consecutiveWriteToFileCmd, this, &SimMain::onConsecutiveWriteToFile);
  updateSnapshotDisplay(0);
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
  sim->m_sim.listen();
}

void SimMain::on_startListeningButton_clicked()
{
  const QString domainSockName = ui->domainSocketLine->text();
  if (m_sim.connectToSocket(domainSockName))
  {
    ui->startListeningButton->setEnabled(false);
    ui->stopListeningButton->setEnabled(true);
    m_simthread = std::thread(SimMain::listenOnSock, this);
    log("Starting listening thread.");
  }
  else
  {
    log(QString("Could not connect to domain socket '%1'").arg(domainSockName));
  }
}

void SimMain::on_stopListeningButton_clicked()
{
  m_sim.stopListening();
  if (m_simthread.joinable())
  {
    m_simthread.join();
  }
  ui->startListeningButton->setEnabled(true);
  ui->stopListeningButton->setEnabled(false);
}

void SimMain::on_setRAMButton_clicked()
{
  bool addrOk = false;
  bool valOk = false;
  const uint16_t addr = ui->addrBox->text().toUInt(&addrOk, 0);
  const uint16_t val = ui->valBox->text().toUInt(&valOk, 0);
  if (addrOk && valOk)
  {
    m_sim.setRAMLoc(addr, val);
    log(QString("Set RAM location %1 to %2.").arg(addr, 4, 16, QChar('0')).arg(val, 2, 16, QChar('0')));
  }
  else
  {
    log(QString("Error parsing RAM address and/or value input box."));
  }
}

void SimMain::on_loadStateButton_clicked()
{
  const QString filename = QFileDialog::getOpenFileName(
    this, "Open SD2 Tester filesystem state data", "", "SD2 Filesystem Data (*.sd2)");

  if (!filename.isEmpty())
  {
    if (m_sim.loadState(filename))
    {
      log(QString("Loaded state from file '%1'").arg(filename));
    }
    else
    {
      log(QString("Failed to load state from file '%1'").arg(filename));
    }
  }
}

void SimMain::on_saveStateButton_clicked()
{
  QString filename = QFileDialog::getSaveFileName(
    this, "Select filename to save SD2 filesystem state data", "", "SD2 Filesystem Data (*.sd2)");

  if (!filename.isEmpty())
  {
    if (!filename.endsWith(".sd2", Qt::CaseInsensitive))
    {
      filename += ".sd2";
    }
    if (m_sim.saveState(filename))
    {
      log(QString("Saved state to file '%1'").arg(filename));
    }
    else
    {
      log(QString("Failed to save state to file '%1'").arg(filename));
    }
  }
}

void SimMain::log(const QString& line)
{
  const auto duration = std::chrono::system_clock::now().time_since_epoch();
  const double secs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
  const QString formattedLine = QString("[%1] %2").arg(secs, 0, 'f', 3).arg(line);
  ui->logView->appendPlainText(formattedLine);
}

void SimMain::onLogMsg(const QString& line)
{
  log(line);
}

void SimMain::onLastLogMsgRepeated()
{
  const int currentVal = ui->progressBar->value();

  if (m_heartbeatBarIncreasing && (currentVal < ui->progressBar->maximum()))
  {
    ui->progressBar->setValue(currentVal + 1);
  }
  else if (!m_heartbeatBarIncreasing && (currentVal > ui->progressBar->minimum()))
  {
    ui->progressBar->setValue(currentVal - 1);
  }

  if (ui->progressBar->value() == 0)
  {
    m_heartbeatBarIncreasing = true;
  }
  else if (ui->progressBar->value() == ui->progressBar->maximum())
  {
    m_heartbeatBarIncreasing = false;
  }
}

void SimMain::onConsecutiveWriteToFile()
{
  ui->logView->textCursor().movePosition(QTextCursor::End);
  ui->logView->textCursor().insertText(".");
}

void SimMain::on_snapshotNumberBox_valueChanged(int snapshotIndex)
{
  updateSnapshotDisplay(snapshotIndex);
}

void SimMain::updateSnapshotDisplay(int snapshotIndex)
{
  const std::vector<uint8_t>& content = m_sim.getSnapshotContent(snapshotIndex);
  for (int i = 0; i < ui->snapshotDataTable->rowCount(); i++)
  {
    const uint8_t contentByte = (static_cast<int>(content.size()) > i) ? content.at(i) : 0;
    QTableWidgetItem* item = ui->snapshotDataTable->item(i, 0);
    item->setText(QString("0x%1").arg(contentByte, 2, 16, QChar('0')));
  }
}

void SimMain::on_snapshotSetButton_clicked()
{
  std::vector<uint8_t> content;
  for (int i = 0; i < ui->snapshotDataTable->rowCount(); i++)
  {
    content.push_back(ui->snapshotDataTable->item(i, 0)->text().toInt(nullptr, 0));
  }
  m_sim.setSnapshotContent(ui->snapshotNumberBox->value(), content);
}

