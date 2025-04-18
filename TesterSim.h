#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <unordered_map>
#include <map>
#include <QMap>
#include <QObject>
#include <QString>

constexpr int CHKSUM_BUF_SIZE = 110;
constexpr int DEFAULT_SNAPSHOT_SIZE = 16;
constexpr int DEFAULT_ERROR_MEMORY_SIZE = 16;

enum class ProtocolType
{
  KWP71,
  FIAT9141,
  Marelli1AF,

  /**
   * This one seems to be an ealier version of the protocol used by Bosch with
   * the Smartra III immobilizer, the spec for which was made available as
   * Appendix H in the user manual provided to the FCC (FCC ID: LXP-VIMA01).
   */
  BoschAlarm,
  BilsteinSuspension
};

class TesterSim : public QObject
{
  Q_OBJECT

public:
  explicit TesterSim(QObject* parent = nullptr);
  bool connectToSocket(const QString& path);
  bool listen();
  void stopListening();
  void setRAMLoc(uint16_t addr, uint8_t val);
  void setValue(uint16_t addr, uint32_t val);
  bool loadState(const QString& filename);
  bool saveState(const QString& filename);
  const std::vector<uint8_t>& getSnapshotContent(int snapshotIndex);
  void setSnapshotContent(int snapshotIndex, const std::vector<uint8_t>& content);
  void setErrorMemoryContent(const std::vector<uint8_t>& content);

signals:
  void logMsg(const QString& line);
  void lastLogMsgRepeated();
  void consecutiveWriteToFileCmd();

private:
  bool m_shutdown = false;
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
  bool m_lastCmdWasWriteToFile = false;
  std::unordered_map<uint16_t,uint8_t> m_ramData;
  std::unordered_map<uint8_t,uint32_t> m_valueData;
  std::map<int,std::vector<uint8_t>> m_snapshotData;
  std::vector<uint8_t> m_errorMemory;

  QMap<QString,QMap<QString,QVector<quint8>>> m_fileContents;
  QMap<QString,QVector<quint8>>::Iterator m_curDirIterator;
  QVector<quint8>* m_curFileContents = nullptr;

  void log(const QString& line);
  bool shouldDisplayPacket(const uint8_t* buf);
  void printPacket(const uint8_t* buf);
  int readBytes(uint8_t* buf, int count);
  bool sendReply(bool print);
  bool processBuf(bool print);
  void chdir(const std::string& dir);
  void addToFile(const std::string& name, int numBytes);
  void emitConsecutiveWriteToFileSignal();

  static std::map<uint8_t,std::function<void(const uint8_t*,uint8_t*,TesterSim*)>> s_commandProcs;
  static const std::unordered_map<int,ProtocolType> s_protocols;
  static const std::unordered_map<int,std::vector<uint8_t>> s_isoBytes;
  static const std::unordered_map<int,std::vector<uint8_t>> s_moduleExtraInitInfo;

  static void process01TabletInfo(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process02SerialNo(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process09(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process0AWorkshopData(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process0BStartApplModGest(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process11DoSlowInit(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
  static void process12GetISOKeyword(const uint8_t* inbuf, uint8_t* outbuf, TesterSim*);
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

  static void processKWP71CommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim, bool hasVerbosePayload);
  static void processFIAT9141CommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim, bool hasVerbosePayload);
  static void processMarelli1AFCommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim, bool hasVerbosePayload);
  static void processBoschAlarmCommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim, bool hasVerbosePayload);
  static void processBilsteinSuspensionCommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim, bool hasVerbosePayload);
};

