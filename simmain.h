#ifndef SIMMAIN_H
#define SIMMAIN_H

#include <QMainWindow>
#include <QString>
#include <thread>
#include "TesterSim.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SimMain; }
QT_END_NAMESPACE

class SimMain : public QMainWindow
{
  Q_OBJECT

public:
  SimMain(const QString& domainSockName, QWidget* parent = nullptr);
  ~SimMain();

private slots:
  void on_startListeningButton_clicked();
  void on_stopListeningButton_clicked();
  void on_setRAMButton_clicked();
  void on_loadStateButton_clicked();
  void on_saveStateButton_clicked();
  void onLogMsg(const QString& line);
  void onLastLogMsgRepeated();
  void onConsecutiveWriteToFile();
  void on_snapshotNumberBox_valueChanged(int arg1);
  void on_snapshotSetButton_clicked();
  void on_snapshotAddButton_clicked();
  void on_snapshotRemoveButton_clicked();

private:
  Ui::SimMain *ui;
  TesterSim m_sim;
  std::thread m_simthread;
  bool m_heartbeatBarIncreasing = true;
  static void listenOnSock(SimMain* sim);
  void log(const QString& line);
  void updateSnapshotDisplay(int snapshotIndex);
};
#endif // SIMMAIN_H
