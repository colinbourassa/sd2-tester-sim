#ifndef LOGVIEWER_H
#define LOGVIEWER_H

#include <QTextBrowser>

class LogViewer : public QTextBrowser
{
public:
  LogViewer(QWidget* parent);
  void appendLine(const QString& text);
  void addDot();
  inline int lineCount() const { return m_lineCount; }

private:
  int m_maxLines = 8192;
  int m_lineCount = 0;
  bool m_atBottom = false;

  void removeFirstLine();

private slots:
  void scrollToBottom();
  void scrolledTo(int);
};

#endif // LOGVIEWER_H
