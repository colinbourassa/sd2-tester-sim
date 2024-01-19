#pragma once
#include <stdint.h>
#include <string>
#include <map>
#include <functional>

#include <QMap>

constexpr int CHKSUM_BUF_SIZE = 110;

enum class ProtocolType
{
  KWP71,
  FIAT9141,
  Marelli1AF
};

class TesterSim
{
public:
  TesterSim();
  bool connectToSocket(const QString& path);
  bool listen();
  void setRAMLoc(uint16_t addr, uint8_t val);
  bool loadState(const QString& filename);
  bool saveState(const QString& filename);

private:
  int m_sockFd = -1;
  uint8_t m_inbuf[128];
  uint8_t m_outbuf[128];
  uint8_t m_checksumBuf[CHKSUM_BUF_SIZE];
  uint8_t m_lastInbuf[128];
  QString m_curDir;
  QString m_curFile;
  int m_fileReadPos = 0;
  bool m_applRun[16];
  int m_currentECUID = 0;
  std::unordered_map<uint16_t,uint8_t> m_ramData;

  QMap<QString,QMap<QString,QVector<quint8>>> m_fileContents;
  QMap<QString,QVector<quint8>>::Iterator m_curDirIterator;
  QVector<quint8>* m_curFileContents = nullptr;

  bool shouldDisplayPacket(const uint8_t* buf);
  void printPacket(const uint8_t* buf);
  int readBytes(uint8_t* buf, int count);
  bool sendReply(bool print);
  bool processBuf(bool print);
  void chdir(const std::string& dir);
  void addToFile(const std::string& name, int numBytes);

  static std::map<uint8_t,const char*> s_outColors;
  static std::map<uint8_t,std::function<void(const uint8_t*,uint8_t*,TesterSim*)>> s_commandProcs;
  static std::unordered_map<int,std::vector<uint8_t>> s_isoBytes;
  static std::unordered_map<int,ProtocolType> s_protocols;

  static void process01TabletInfo(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process02SerialNo(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process09(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process0AWorkshopData(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process0BStartApplModGest(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process11DoSlowInit(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process13CommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process15DisplayString(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process1C(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process1ECloseFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process20OpenFileForWriting(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process21WriteToFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process23OpenFileForReading(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process24ReadFromFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process25ChecksumFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process2AChdir(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process2BGetNextDirEntry(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process3AGetDateTime(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process3DEraseFlash(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);

  static void processKWP71CommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim);
  static void processFIAT9141CommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim);
  static void processMarelli1AFCommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim);
};

