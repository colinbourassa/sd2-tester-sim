#include "logviewer.h"
#include <QTextCursor>
#include <QScrollBar>

LogViewer::LogViewer(QWidget* parent) : QTextBrowser(parent)
{
  connect(this, &QTextBrowser::textChanged, this, &LogViewer::scrollToBottom);
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &LogViewer::scrolledTo);
  setHtml("");
}

void LogViewer::scrollToBottom()
{
  if (m_atBottom)
  {
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
  }
}

void LogViewer::scrolledTo(int val)
{
  m_atBottom = (val == verticalScrollBar()->maximum());
}

void LogViewer::removeFirstLine()
{
  QTextCursor cursor = textCursor();
  if (cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor, 1))
  {
    cursor.select(QTextCursor::LineUnderCursor);
    cursor.removeSelectedText();
    cursor.deleteChar();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor, 1);
    setTextCursor(cursor);
    m_lineCount--;
  }
}

void LogViewer::appendLine(const QString& line)
{
  if (m_lineCount >= m_maxLines)
  {
    removeFirstLine();
  }

  const auto duration = std::chrono::system_clock::now().time_since_epoch();
  const double secs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;

  const QString formattedLine = QString("[%1] %2").arg(secs, 0, 'f', 3).arg(line);
  append(formattedLine);
  m_lineCount++;
}

void LogViewer::addDot()
{
  textCursor().insertText(".");
}

