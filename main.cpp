#include "simmain.h"

#include <QApplication>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  QString domainSockName;

  if (argc > 1)
  {
    domainSockName = argv[1];
  }

  SimMain w(domainSockName);
  w.show();
  return a.exec();
}
