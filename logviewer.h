#ifndef LOGVIEWER_H
#define LOGVIEWER_H

#include <QTextBrowser>

class LogViewer : public QTextBrowser
{
public:
  LogViewer(QWidget* parent);
  void appendLine(const QString& text);
  inline int lineCount() const { return m_lineCount; }

private:
  int m_maxLines = 1024;
  int m_lineCount = 0;
};

#endif // LOGVIEWER_H
