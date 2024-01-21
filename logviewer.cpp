#include "logviewer.h"
#include <QTextCursor>

LogViewer::LogViewer(QWidget* parent) : QTextBrowser(parent)
{
  setHtml("");
}

void LogViewer::appendLine(const QString& line)
{
  if (m_lineCount >= m_maxLines)
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

  append(line);
  m_lineCount++;
}
