#include "simmain.h"

#include <QApplication>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  SimMain w;
  w.show();
  return a.exec();
}
