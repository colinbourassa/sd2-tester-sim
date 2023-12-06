#ifndef SIMMAIN_H
#define SIMMAIN_H

#include <QMainWindow>
#include <thread>
#include "TesterSim.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SimMain; }
QT_END_NAMESPACE

class SimMain : public QMainWindow
{
  Q_OBJECT

public:
  SimMain(QWidget *parent = nullptr);
  ~SimMain();

private slots:
  void on_connectButton_clicked();
  void on_startListeningButton_clicked();
  void on_setRAMButton_clicked();

private:
  Ui::SimMain *ui;
  TesterSim m_sim;
  std::thread m_simthread;
  static void listenOnSock(SimMain* sim);
};
#endif // SIMMAIN_H
