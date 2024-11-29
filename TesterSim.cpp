#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include "TesterSim.h"

#include <QFile>
#include <QDataStream>
#include <QFileInfo>

std::map<uint8_t,std::function<void(const uint8_t*,uint8_t*,TesterSim*)>> TesterSim::s_commandProcs =
{
  { 0x01, TesterSim::process01TabletInfo },
  { 0x02, TesterSim::process02SerialNo },
  { 0x09, TesterSim::process09 },
  { 0x0A, TesterSim::process0AWorkshopData },
  { 0x0B, TesterSim::process0BStartApplModGest },
  { 0x11, TesterSim::process11DoSlowInit },
  { 0x12, TesterSim::process12GetISOKeyword },
  { 0x13, TesterSim::process13CommandToECU },
  { 0x15, TesterSim::process15DisplayString },
  { 0x1C, TesterSim::process1C },
  { 0x1E, TesterSim::process1ECloseFile },
  { 0x20, TesterSim::process20OpenFileForWriting },
  { 0x21, TesterSim::process21WriteToFile },
  { 0x23, TesterSim::process23OpenFileForReading },
  { 0x24, TesterSim::process24ReadFromFile },
  { 0x25, TesterSim::process25ChecksumFile },
  { 0x2A, TesterSim::process2AChdir },
  { 0x2B, TesterSim::process2BGetNextDirEntry },
  { 0x3A, TesterSim::process3AGetDateTime },
  { 0x3D, TesterSim::process3DEraseFlash }
};


TesterSim::TesterSim(QObject* parent) : QObject(parent)
{
  memset(m_inbuf, 0, 128);
  memset(m_outbuf, 0, 128);
  memset(m_checksumBuf, 0, CHKSUM_BUF_SIZE);
  memset(m_lastInbuf, 0, 128);
  for (int i = 0; i < 16; i++)
  {
    m_applRun[i] = false;
  }
}

void TesterSim::setRAMLoc(uint16_t addr, uint16_t val)
{
  m_ramData[addr] = val;
}

int TesterSim::readBytes(uint8_t* buf, int count)
{
  int bufPos = 0;
  int readResult = 0;

  while ((bufPos < count) && !m_shutdown)
  {
    readResult = read(m_sockFd, buf + bufPos, count - bufPos);
    if (readResult > 0)
    {
      bufPos += readResult;
    }
  }
  return bufPos;
}

bool TesterSim::sendReply(bool print)
{
  bool status = true;
  if (m_outbuf[2] != 0)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    // Although m_outbuf[1] should contain the hi byte
    // of a 16-bit byte count, it seems that neither the
    // win32 size nor the Tester ever send packets with
    // more than 128 bytes total (including the prefix).
    const uint16_t len = m_outbuf[2] + 1;
    if (print)
    {
      printPacket(m_outbuf);
    }

    const int wroteBytes = write(m_sockFd, m_outbuf, len);
    status = (wroteBytes == len);
  }
  return status;
}

bool TesterSim::processBuf(bool print)
{
  bool status = false;
  const uint8_t size = m_inbuf[2] + 1;

  if (size >= 7)
  {
    memcpy(m_outbuf, m_inbuf, size); // tablet SW usually starts by copying the message
                                     // from the PC into the reply buffer
    m_outbuf[0] = 0x54; // fixed value indicating a reply from the tester in
                        // linked-to-PC mode
    m_outbuf[1] = 0x00; // tester only seems to send messages with a length < 0x80,
                        // so the hi byte is always 00

    // To keep the log output cleaner, we keep track of whether we
    // received multiple consecutive Write-to-File commands.
    if (m_inbuf[6] != 0x21)
    {
      m_lastCmdWasWriteToFile = false;
    }

    if (s_commandProcs.count(m_inbuf[6]))
    {
      s_commandProcs.at(m_inbuf[6])(m_inbuf, m_outbuf, this);
    }
    else
    {
      emit logMsg(QString("Sending generic 'success' response to command msg type 0x%1").arg(m_inbuf[6], 2, 16, QChar('0')));
      m_outbuf[2] = 7;
      m_outbuf[7] = 1;
    }
    status = sendReply(print);

    // TODO: Of the ECUs that send unsolicited info immediately after the ISO
    // keyword sequence, we need to determine which of them have their ID info
    // concatenated to the cmd 0x11 (or 0x12) response payload, and which have
    // their info sent in separate messages after the 0x11/0x12 response.
    // In the case of separate messages, the message type is probably supposed
    // to be set to 0x13.
  }
  else
  {
    emit logMsg("Warning: received message of fewer than 7 bytes.\n");
  }
  return status;
}

bool TesterSim::connectToSocket(const QString &sockPath)
{
  bool status = false;
  struct sockaddr_un addr;

  m_sockFd = socket((AF_UNIX), SOCK_STREAM, 0);
  if (m_sockFd > 0)
  {
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockPath.toStdString().c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(m_sockFd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == 0)
    {
      status = true;
    }
    else
    {
      m_sockFd = -1;
    }
  }

  return status;
}

void TesterSim::stopListening()
{
  m_shutdown = true;
  if (m_sockFd >= 0)
  {
    shutdown(m_sockFd, SHUT_RDWR);
  }
}

/**
 * Determines whether the packet containing the supplied buffer should be
 * printed in its entirety.
 */
bool TesterSim::shouldDisplayPacket(const uint8_t* buf)
{
  const uint8_t numBytes = buf[2];
  bool status = true;
  if (memcmp(buf, m_lastInbuf, numBytes) == 0)
  {
    emit lastLogMsgRepeated();
    status = false;
  }
  memcpy(m_lastInbuf, buf, numBytes);
  return status;
}

void TesterSim::printPacket(const uint8_t* buf)
{
  QString packetStr;
  for (int i = 0; i <= buf[2]; i++)
  {
    packetStr += QString("%1 ").arg(buf[i], 2, 16, QChar('0'));
  }
  log(packetStr);
}

void TesterSim::emitConsecutiveWriteToFileSignal()
{
  emit consecutiveWriteToFileCmd();
}

bool TesterSim::listen()
{
  bool status = true;
  int fullPacketSize = 0;

  while (status && !m_shutdown)
  {
    // Read the first 3 bytes. The first byte (which we'll call the prefix)
    // is fixed to be one of a couple possible values, depending on the mode
    // of the Windows diagnostic software (Ferrari vs. Maserati).
    // The second and third bytes, taken together, are a big-endian 16-bit
    // count of the bytes in the payload for this message, including those
    // two bytes themselves but not including the prefix byte.
    if (readBytes(m_inbuf, 3) == 3)
    {
      fullPacketSize = m_inbuf[2] + 1;

      if ((fullPacketSize >= 7) &&
          (readBytes(m_inbuf + 3, fullPacketSize - 3) ==
            (fullPacketSize - 3)))
      {
        if (shouldDisplayPacket(m_inbuf))
        {
          printPacket(m_inbuf);
          status = processBuf(true);
        }
        else
        {
          status = processBuf(false);
        }
      }
      else
      {
        if (fullPacketSize < 7)
        {
          emit logMsg(QString("Error: reported packet size of %1 too small").arg(fullPacketSize));
        }
        else
        {
          emit logMsg("Error receiving packet body");
        }
        status = false;
      }
    }
    else
    {
      // failed to read the first three bytes of the packet
      status = false;
    }
  }

  return status;
}

void TesterSim::process01TabletInfo(const uint8_t* /*inbuf*/, uint8_t* outbuf, TesterSim* sim)
{
  sim->log("Request for Tester info");
  outbuf[1] = 0;
  outbuf[2] = 0x18;
  outbuf[7] = 0x03; // sys loader maj version
  outbuf[8] = 0x01; // sys loader min version
  outbuf[9] = 0x25; // sys loader date (day)
  outbuf[10] = 0x09; // sys loader date (month)
  outbuf[11] = 0x19; // sys loader date (decade)
  outbuf[12] = 0x98; // sys loader date (year)
  outbuf[13] = 0x05; // OS maj version
  outbuf[14] = 0x05; // OS min version
  outbuf[15] = 0x04; // OS date (day)
  outbuf[16] = 0x03; // OS date (month)
  outbuf[17] = 0x19; // OS date (decade)
  outbuf[18] = 0x99; // OS date (year)
  outbuf[19] = 0x00; // free space on flash storage (32 bit val)
  outbuf[20] = 0x23;
  outbuf[21] = 0x33;
  outbuf[22] = 0x33;
  outbuf[23] = 0;   // serial num hi
  outbuf[24] = 212; // serial num lo
}

void TesterSim::process02SerialNo(const uint8_t* /*inbuf*/, uint8_t* outbuf, TesterSim* sim)
{
  sim->log("Request for Tester serial no.");
  outbuf[2] = 8;
  outbuf[7] = 0;   // hi byte
  outbuf[8] = 212; // lo byte
}

void TesterSim::process09(const uint8_t* /*inbuf*/, uint8_t* outbuf, TesterSim* /*sim*/)
{
  outbuf[1] = 0;
  outbuf[2] = 7;
  outbuf[7] = 0x10;
}

void TesterSim::process0AWorkshopData(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  sim->log("Request for workshop data");
  outbuf[2] = 0x76;
  outbuf[7] = 1;
  outbuf[8] = inbuf[7];
  char c = 'a';
  int pos = 9;
  while (pos < 19)
  {
    outbuf[pos++] = c++;
  }
  outbuf[pos++] = '\0';
  while (pos < 29)
  {
    outbuf[pos++] = c++;
  }
  outbuf[pos++] = '\0';
  while (pos < 0x77)
  {
    outbuf[pos++] = '\0';
  }
}

void TesterSim::process0BStartApplModGest(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const uint16_t ecuId = (inbuf[7] * 0x100) + inbuf[8];
  const uint8_t pipeNum = inbuf[9];
  sim->log(QString("Starting _applModGest%1 thread on pipe %2").arg(ecuId, 4, 10, QChar('0')).arg(pipeNum));
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  sim->m_applRun[pipeNum] = true;
  sim->m_currentECUID = ecuId;
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process11DoSlowInit(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  std::this_thread::sleep_for(std::chrono::seconds(1));

  const uint8_t ecuAddr = inbuf[7];
  if (inbuf[2] >= 8)
  {
    sim->log(QString("Do 5-baud slow init for ECU address 0x%1 with %2 bytes expected in response sequence").arg(ecuAddr, 2, 16, QChar('0')).arg(inbuf[8]));
  }
  else
  {
    sim->log(QString("Do 5-baud slow init for ECU address 0x%1").arg(ecuAddr, 2, 16, QChar('0')));
  }

  process12GetISOKeyword(inbuf, outbuf, sim);
}

void TesterSim::process12GetISOKeyword(const uint8_t* /*inbuf*/, uint8_t* outbuf, TesterSim* sim)
{
  if (sim->s_isoBytes.count(sim->m_currentECUID))
  {
    const std::vector<uint8_t>& isoBytes = sim->s_isoBytes.at(sim->m_currentECUID);
    const int isoByteCount = isoBytes.size();
    QString replyLogMsg = QString("Replying with keyword sequence of %1 bytes:").arg(isoByteCount);
    outbuf[2] = 7 + isoByteCount;
    outbuf[7] = 1;

    for (int i = 0; i < isoByteCount; i++)
    {
      outbuf[8 + i] = isoBytes[i];
      replyLogMsg += QString(" %1").arg(isoBytes[i], 2, 16, QChar('0'));
    }

    // There are some modules whose cmd 11/12 reply message contains
    // more than just the ISO keyword sequence -- it contains one or more
    // frames of ID data from the ECU, which are concatenated into the same
    // serial message payload from the Tester back to WSDC32.
    if (sim->s_moduleExtraInitInfo.count(sim->m_currentECUID))
    {
      const int extraDataLen = sim->s_moduleExtraInitInfo.at(sim->m_currentECUID).size();
      outbuf[2] = 7 + isoByteCount + extraDataLen;
      memcpy(&outbuf[8 + isoByteCount], sim->s_moduleExtraInitInfo.at(sim->m_currentECUID).data(), extraDataLen);

      printf("slow init reply msg:");
      for (int i = 0; i <= outbuf[2]; i++)
      {
        printf(" %02X", outbuf[i]);
      }
      printf("\n");
    }

    sim->log(replyLogMsg);
  }
  else
  {
    sim->log(QString("Warning: no ISO byte record for ECU ID %1").arg(sim->m_currentECUID, 4, 10, QChar('0')));
  }
}

void TesterSim::process13CommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const int currentECU = sim->m_currentECUID;
  if (s_protocols.count(currentECU))
  {
    const ProtocolType proto = s_protocols.at(currentECU);

    // Sometimes (maybe just for certain ECUs like BMOT0145?), WSDC32 sends requests with 0x13 in position 06,
    // and the request payload starting immediately after (at position 07). In this format, the request payload
    // does not contain the block's prefix/size byte, the block sequence number, or the terminator.
    // The other format that cmd 0x13 messages can take (e.g. for BABS0096) has 0x13 at position 06, an unused
    // byte at position 07 (as a placeholder for the 0x01 status in the response), and then a *complete* ECU
    // protocol block starting at 08, for example:
    //   pos:  06 07 08 09 0A 0B
    //         -----------------
    //   val:  13 00 03 04 00 03 <-- note 03 byte count, 04 sequence num, and 03 terminator
    // instead of:
    //   val:  13 00 <-- this 00 is actually just the ID request block title, not a placeholder
    
    // The data is this command packet is assumed to verbosely contain an ECU protocol block
    // if the packet has 9 or more bytes AND there is 0x00 in position 07.
    const bool hasVerbosePayload = ((inbuf[1] > 0) || (inbuf[2] > 8)) && (inbuf[7] == 0x00);

    if (proto == ProtocolType::KWP71)
    {
      processKWP71CommandToECU(inbuf, outbuf, sim, hasVerbosePayload);
    }
    else if (proto == ProtocolType::FIAT9141)
    {
      processFIAT9141CommandToECU(inbuf, outbuf, sim, hasVerbosePayload);
    }
    else if (proto == ProtocolType::Marelli1AF)
    {
      processMarelli1AFCommandToECU(inbuf, outbuf, sim, hasVerbosePayload);
    }
    else if (proto == ProtocolType::BoschAlarm)
    {
      processBoschAlarmCommandToECU(inbuf, outbuf, sim, hasVerbosePayload);
    }
  }
  else
  {
    sim->log(QString("Warning: protocol for ECU ID %1 is not known").arg(sim->m_currentECUID, 4, 10, QChar('0')));
  }
}

void TesterSim::processKWP71CommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim, bool hasVerbosePayload)
{
  const uint8_t blockTitle = hasVerbosePayload ? inbuf[10] : inbuf[7];

  if (blockTitle == 0x00) // Req ID code
  {
    outbuf[2] = 16;    // overall message size (minus prefix byte)
    outbuf[7] = 1;     // 'success' indicator
    outbuf[8] = 8;     // number of bytes following
    outbuf[9] = 0xF6;  // KWP71 response title with ASCII/ID data
    outbuf[10] = 0x31;
    outbuf[11] = 0x31;
    outbuf[12] = 0x32;
    outbuf[13] = 0x33;
    outbuf[14] = 0x35;
    outbuf[15] = 0x38;
    outbuf[16] = 0x03;
  }
  else if (blockTitle == 0x01) // Read RAM
  {
    const uint8_t count = hasVerbosePayload ? inbuf[11] : inbuf[8];
    const uint16_t addr = hasVerbosePayload ? (((uint16_t)inbuf[12] * 0x100) + inbuf[13]) : (((uint16_t)inbuf[9] * 0x100) + inbuf[10]);
    if (sim->m_ramData.count(addr) == 0)
    {
      sim->m_ramData[addr] = 0;
    }

    outbuf[2] = count + 10;
    outbuf[7] = 1;          // indicate success
    outbuf[8] = count + 2;  // number of bytes that follow (response from ECU)
    outbuf[9] = 0xFD;       // KWP71 response type to request 01
    outbuf[10] = sim->m_ramData[addr];
    outbuf[11] = 0x03;      // end-of-packet marker
  }
  else if (blockTitle == 0x07) // read trouble codes
  {
    outbuf[2] = 13;
    outbuf[7] = 1;
    outbuf[8] = 5;
    outbuf[9] = 0x00;
    outbuf[10] = 0x00;
    outbuf[11] = 0x00;
    outbuf[12] = 0x00;
    outbuf[13] = 0x00;
  }
  else
  {
    sim->log("Warning: unhandled KWP71 command");
    outbuf[2] = 7;
    outbuf[7] = 1;
  }
}

void TesterSim::processFIAT9141CommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim, bool hasVerbosePayload)
{
  const uint8_t blockTitle = hasVerbosePayload ? inbuf[9] : inbuf[7];

  if (blockTitle == 0x00) // Req ID code
  {
    outbuf[2] = 16;
    outbuf[7] = 1;
    outbuf[8] = 8;
    outbuf[9] = 0xF6;
    outbuf[10] = 0x31;
    outbuf[11] = 0x31;
    outbuf[12] = 0x32;
    outbuf[13] = 0x33;
    outbuf[14] = 0x35;
    outbuf[15] = 0x38;
    outbuf[16] = 0x03;
  }
  else if (blockTitle == 0x01) // Read RAM
  {
    const uint8_t count = hasVerbosePayload ? inbuf[10] : inbuf[8];
    const uint16_t addr = hasVerbosePayload ? (((uint16_t)inbuf[11] * 0x100) + inbuf[12]) : (((uint16_t)inbuf[9] * 0x100) + inbuf[10]);
    if (sim->m_ramData.count(addr) == 0)
    {
      sim->m_ramData[addr] = 0;
    }

    outbuf[2] = count + 10;
    outbuf[7] = 1;          // indicate success
    outbuf[8] = count + 2;  // number of bytes that follow (response from ECU)
    outbuf[9] = 0xFD;       // KWP71 response type to request 01
    outbuf[10] = sim->m_ramData[addr];
    outbuf[11] = 0x03;      // end-of-packet marker
  }
  else
  {
    sim->log("Warning: unhandled FIAT9141 command");
    outbuf[2] = 7;
    outbuf[7] = 1;
  }
}

void TesterSim::processMarelli1AFCommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim, bool hasVerbosePayload)
{
  const uint8_t blockTitle = hasVerbosePayload ? inbuf[9] : inbuf[7];

  if (blockTitle == 0x51) // request for ID info
  {
    outbuf[2] = 24;
    outbuf[7] = 1;
    outbuf[8] = 16;
    outbuf[9] = 0xAE; // ID of reply to request for info
    outbuf[10] = 0xAA; // normally sync bytes for 1AF protocol, but SD2 seems to expect that
                       // the Marelli controller for the Ferrari 355 F1 gearbox put the
                       // "Marelli ECU code" value here
    outbuf[11] = 0x55;
    outbuf[12] = 0xCC;
    outbuf[13] = 0x33;
    outbuf[14] = 0x31; // start of Marelli SW version
    outbuf[15] = 0x32;
    outbuf[16] = 0x33;
    outbuf[17] = 0x34;
    outbuf[18] = 0x35;
    outbuf[19] = 0x36;
    outbuf[20] = 0x97; // SW release year in BCD
    outbuf[21] = 0x01; // SW release month in BCD
    outbuf[22] = 0x02; // SW release day in BCD
    outbuf[23] = 0xAA; // ID info block terminator
    sim->add8BitChecksum(&outbuf[8]);
  }
  else if ((blockTitle == 0x20) || (blockTitle == 0x21)) // activate actuator / stop actuation
  {
    outbuf[2] = 11;
    outbuf[7] = 1;
    outbuf[8] = 3;
    outbuf[9] = 0x09;
    sim->add16BitChecksum(&outbuf[8]);
  }
  else if (blockTitle == 0x30) // read RAM/ROM/EEPROM
  {
    const uint16_t startAddr = hasVerbosePayload ?
      (static_cast<uint16_t>(inbuf[10] << 8) | (inbuf[11] & 0xff)) :
      (static_cast<uint16_t>(inbuf[8] << 8) | (inbuf[9] & 0xff));
    const uint8_t numBytes = hasVerbosePayload ? inbuf[12] : inbuf[10];

    outbuf[2] = 11 + numBytes;
    outbuf[7] = 1;
    outbuf[8] = 3 + numBytes;
    outbuf[9] = 0xCF;
    for (uint16_t addr = startAddr; addr < (startAddr + numBytes); addr++)
    {
      outbuf[10 + addr] = (sim->m_ramData[addr] & 0xff);
    }
    sim->add16BitChecksum(&outbuf[8]);
  }
  else if (blockTitle == 0x31) // read value
  {
    const uint8_t valueCode = hasVerbosePayload ? inbuf[10] : inbuf[8];

    outbuf[2] = 15;
    outbuf[7] = 1;
    outbuf[8] = 7;    // bytecount
    outbuf[9] = 0xCE; // reply title

    // It looks like the WSDC32 software is expecting more bytes in the response
    // to cmd 0x31; let's just pad out to 32 bits with zeroes:
    outbuf[10] = 0;
    outbuf[11] = 0;
    outbuf[12] = sim->m_ramData[valueCode] >> 8;
    outbuf[13] = sim->m_ramData[valueCode] & 0xff;
    // TODO: This is technically not the same as generic RAM/ROM access, so
    // we should have a separate "value" data store (i.e. not m_ramData).

    sim->add16BitChecksum(&outbuf[8]);
  }
  else if (blockTitle == 0x32) // request for snapshot
  {
    const uint8_t snapshotIndex = hasVerbosePayload ? inbuf[10] : inbuf[8];
    if (sim->m_snapshotData[snapshotIndex].size() < 10)
    {
      sim->m_snapshotData[snapshotIndex].resize(10, 0);
    }
    const uint8_t numBytesInSnapshot = sim->m_snapshotData[snapshotIndex].size();

    outbuf[2] = 11 + numBytesInSnapshot; // bytecount in the SD2 frame (including the ending checksum)
    outbuf[7] = 1;
    outbuf[8] = 3 + numBytesInSnapshot; // bytecount in the 1AF frame; pg. 28 of FIAT 3.00601 Marelli 1AF document seems to have an error here
    outbuf[9] = 0xCD; // reply title

    for (unsigned int i = 0; i < numBytesInSnapshot; i++)
    {
      outbuf[10 + i] = sim->m_snapshotData[snapshotIndex][i];
    }
    sim->add16BitChecksum(&outbuf[8]);
  }
  else
  {
    sim->log("Warning: unhandled FIAT/Marelli 1AF command");
    outbuf[2] = 7;
    outbuf[7] = 1;
  }
}

/**
 * Computes a 16-bit checksum by iterating over the contents of a frame, using
 * the value of the first byte to determine the total number of bytes in the
 * frame. The bytecount value does not count itself, but does count the two
 * bytes of the checksum, e.g:
 *  [00] 05 // number of bytes to follow
 *  [01] 01 // sample block title
 *  [02] 07 // sample data byte A
 *  [03] 08 // sample data byte B
 *  [04] 00 // checksum high byte
 *  [05] 15 // checksum lo byte
 */
void TesterSim::add16BitChecksum(uint8_t* frame) const
{
  const uint8_t bytecount = frame[0];
  uint16_t checksum = 0;
  for (uint8_t i = 0; i < (bytecount - 1); i++)
  {
    checksum += frame[i];
  }
  frame[bytecount - 1] = (checksum >> 8);
  frame[bytecount] = checksum & 0xff;
}

/**
 * Computes a 8-bit checksum by iterating over the contents of a frame, using
 * the value of the first byte to determine the total number of bytes in the
 * frame. The bytecount value does not count itself, but does count the single
 * byte of the checksum, e.g:
 *  [00] 04 // number of bytes to follow
 *  [01] 01 // sample block title
 *  [02] 07 // sample data byte A
 *  [03] 08 // sample data byte B
 *  [04] 14 // checksum
 */
void TesterSim::add8BitChecksum(uint8_t* frame) const
{
  const uint8_t bytecount = frame[0];
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < (bytecount - 1); i++)
  {
    checksum += frame[i];
  }
  frame[bytecount] = checksum;
}


/**
 * When commands 52 FE 01 and 52 FF 01 are sent to the ECU, the bytes in the
 * reply are taken together as a BCD representation of the Bosch VIM security
 * system version, e.g. 01 71 is VIM171.
 * WSDC32's behavior (i.e. the commands it then sends for diagnostics) will
 * change depending on the VIM version.
 */
void TesterSim::processBoschAlarmCommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim, bool /*hasVerbosePayload*/)
{
  // A typical command looks like: (... 13 01) 52 FE 01
  if (inbuf[8] == 0x52)
  {
    const uint8_t commNumberHi = inbuf[9];
    const uint8_t commNumberLo = inbuf[10];
    const uint16_t commNumber = ((uint16_t)commNumberHi << 8) | commNumberLo;

    if (sim->m_ramData.count(commNumber) == 0)
    {
      sim->m_ramData[commNumber] = 0;
    }

    // This command apparently reads a single byte from the ECU, and that is the only
    // thing echoed back to WSDC32 in the payload (i.e. after byte index 07)
    outbuf[2] = 8; // byte count
    outbuf[7] = 1; // indicate success
    outbuf[8] = sim->m_ramData[commNumber];
  }
  else if (inbuf[8] == 0x44)
  {
    // This is another type of Read command -- possible from a different address space or device?
    // Unlike cmd 52h, it is followed by only a single byte (which must be an 8-bit address.)
    const uint8_t commNumber = inbuf[9];
    if (sim->m_ramData.count(commNumber) == 0)
    {
      sim->m_ramData[commNumber] = 0;
    }
    outbuf[2] = 8; // byte count
    outbuf[7] = 1; // indicate success
    outbuf[8] = sim->m_ramData[commNumber];
  }
  else
  {
    sim->log("Warning: unhandled Bosch Alarm command");
  }
}

void TesterSim::process15DisplayString(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  std::string dstring((char*)(inbuf + 14), inbuf[2] - 13);
  sim->log(QString("Display string on Tester screen: '%1'").arg(QString::fromStdString(dstring)));
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process1C(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const uint8_t pipeNum = inbuf[5];
  sim->log(QString("Shut down ECU appl thread monitoring pipe %1").arg(pipeNum));

  if (sim->m_applRun[pipeNum])
  {
    sim->m_applRun[pipeNum] = false;
    outbuf[2] = 7;
    outbuf[7] = 1;
  }
  else
  {
    sim->log("Thread not yet running; replying with negative status from applModGen...");
    outbuf[2] = 7;
    outbuf[5] = 0;
    outbuf[7] = 0xfe;
  }
}

void TesterSim::process1ECloseFile(const uint8_t* /*inbuf*/, uint8_t* outbuf, TesterSim* sim)
{
  sim->m_curFileContents = nullptr;
  sim->log(QString("Close file (which is currently '%1')").arg(sim->m_curFile));
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process20OpenFileForWriting(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const QString filenameWithPath = QString::fromStdString(std::string((char*)(inbuf + 7), inbuf[2] - 6));
  const QFileInfo fileinfo(filenameWithPath);
  const QString filenameOnly = fileinfo.fileName();
  const QString dirOnly = fileinfo.absolutePath();

  sim->m_curDir = dirOnly;
  sim->m_curFile = filenameOnly;
  sim->m_curFileContents = &(sim->m_fileContents[dirOnly][filenameOnly]);
  sim->m_curFileContents->clear(); // only truncate is supported (no append)
  sim->log(QString("Open file for writing: %1 (in dir %2)").arg(sim->m_curFile).arg(sim->m_curDir));
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process21WriteToFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const int byteCount = inbuf[2] - 0xb;

  // If cmd 0x21 (Write-to-File) was the last packet we received,
  // just signal that we're processing another write. This is done
  // to decrease log clutter, since it is common for there to be
  // many dozens (or hundreds) of Write-to-File commands received
  // consecutively.
  if (sim->m_lastCmdWasWriteToFile)
  {
    sim->emitConsecutiveWriteToFileSignal();
  }
  else
  {
    sim->log(QString("Write bytes to file"));
    sim->m_lastCmdWasWriteToFile = true;
  }
  for (int i = 0; i < byteCount; i++)
  {
    sim->m_curFileContents->append(inbuf[0xb + i]);
  }
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process23OpenFileForReading(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const QString filenameWithPath = QString::fromStdString(std::string((char*)(inbuf + 7), inbuf[2] - 6));
  const QFileInfo fileinfo(filenameWithPath);
  const QString filenameOnly = fileinfo.fileName();
  const QString dirOnly = fileinfo.absolutePath();

  sim->m_curDir = dirOnly;
  sim->m_curFile = filenameOnly;
  sim->m_fileReadPos = 0;
  sim->m_curFileContents = &(sim->m_fileContents[dirOnly][filenameOnly]);
  memset(sim->m_checksumBuf, 0, CHKSUM_BUF_SIZE);

  sim->log(QString("Open file for reading: %1 (in dir %2)").arg(sim->m_curFile).arg(sim->m_curDir));
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process24ReadFromFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  if (!sim->m_curFileContents)
  {
    sim->log("Error: m_curFileContents is null. File read/write operation without an open file?");
    return;
  }

  const int bytesLeftInFile = (sim->m_curFileContents->size() - sim->m_fileReadPos);
  const int numBytesToSend = (bytesLeftInFile >= CHKSUM_BUF_SIZE) ? CHKSUM_BUF_SIZE : bytesLeftInFile;
  sim->log(QString("Read from file (%1 bytes left, %2 bytes in this chunk, file pos 0x%3)").
    arg(bytesLeftInFile).arg(numBytesToSend).arg(sim->m_fileReadPos, 8, 16, QChar('0')));
  outbuf[2] = numBytesToSend + 0xc;
  outbuf[7] = 1;
  outbuf[8] = inbuf[7];
  outbuf[9] = inbuf[8];
  outbuf[10] = inbuf[9];
  outbuf[11] = inbuf[10];
  if (numBytesToSend > 0)
  {
    for (int i = 0; i < numBytesToSend; i++)
    {
      outbuf[12 + i] = sim->m_curFileContents->at(sim->m_fileReadPos + i);
      sim->m_checksumBuf[i] += outbuf[12 + i];
    }
    const int checksumBufPos = 12 + numBytesToSend;
    outbuf[checksumBufPos] = 0;
    for (int i = 12; i < 12 + numBytesToSend; i++)
    {
      outbuf[checksumBufPos] += outbuf[i];
    }
    outbuf[checksumBufPos] = ~outbuf[checksumBufPos];
    sim->log(QString("Computed checksum of %1 for this chunk").arg(outbuf[checksumBufPos], 2, 16));
    sim->m_fileReadPos += numBytesToSend;
  }
  else
  {
    // No more bytes to send from this file.
    // Indicate this with a 00 in position 0xC of the reply message.
    outbuf[12] = 0;
  }
}

void TesterSim::process25ChecksumFile(const uint8_t* /*inbuf*/, uint8_t* outbuf, TesterSim* sim)
{
  sim->log("Request for checksum verification of file");
  outbuf[1] = 0;
  outbuf[2] = 0x75;
  outbuf[7] = 1;

  for (int i = 0; i < CHKSUM_BUF_SIZE; i++)
  {
    outbuf[8 + i] = ~(sim->m_checksumBuf[i]);
  }
}

void TesterSim::process2AChdir(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const QString curDir = QString::fromStdString(std::string((char*)(inbuf + 7), inbuf[2] - 6));
  sim->m_curDir = curDir;
  sim->m_curDirIterator = sim->m_fileContents[curDir].begin();
  sim->log(QString("Change directory: %1").arg(curDir));
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process2BGetNextDirEntry(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  sim->log("Request for next directory entry");

  if (sim->m_curDirIterator !=
      sim->m_fileContents[sim->m_curDir].end())
  {
    const QString filename = sim->m_curDirIterator.key();
    const uint32_t filesize = sim->m_curDirIterator.value().size();
    sim->log(QString(" File: %1, size %2").arg(filename).arg(filesize));
    const uint8_t truncLen = (filename.length() < 90) ? filename.length() : 90;
    sim->log(QString(" Truncated length of filename: %1").arg(truncLen));

    outbuf[2] = 38 + truncLen - 1;
    outbuf[7] = 1; // indicate success
    outbuf[8] = inbuf[7]; // 32-bit sequence num
    outbuf[9] = inbuf[8];
    outbuf[10] = inbuf[9];
    outbuf[11] = inbuf[10];
    outbuf[12] = 2; // 1 == dir, 2 == other (e.g. regular file)
    outbuf[13] = (filesize >> 24) & 0xff;
    outbuf[14] = (filesize >> 16) & 0xff;
    outbuf[15] = (filesize >> 8) & 0xff;
    outbuf[16] = filesize & 0xff;
    memcpy(outbuf + 17, "AUG-06-1998  13:24:55", 21);
    strncpy((char*)(outbuf + 38), filename.toStdString().c_str(), 89);

    sim->m_curDirIterator++;
  }
  else
  {
    // indicate end of directory
    outbuf[2] = 7;
    outbuf[7] = 4;
  }
}

void TesterSim::process3AGetDateTime(const uint8_t* /*inbuf*/, uint8_t* outbuf, TesterSim* sim)
{
  sim->log("Request for Tester date/time");
  outbuf[2] = 0x0d;
  outbuf[7] = 0x06;
  outbuf[8] = 0x31;
  outbuf[9] = 0x50;
  outbuf[10] = 0x02;
  outbuf[11] = 0x12;
  outbuf[12] = 0x23;
  outbuf[13] = 0x06;
}

void TesterSim::process3DEraseFlash(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  sim->log("Command to erase flash on Tester");
  outbuf[2] = 8;
  outbuf[8] = inbuf[7];
  outbuf[7] = 1;
}

bool TesterSim::loadState(const QString& filename)
{
  bool status = false;
  QFile infile(filename);

  if (infile.open(QIODevice::ReadOnly))
  {
    QDataStream in(&infile);
    in >> m_fileContents;
    infile.close();
    status = true;

    emit logMsg("Loaded filesystem entries:");
    foreach (QString dirname, m_fileContents.keys())
    {
      emit logMsg(QString(" dir: %1").arg(dirname));
      foreach (QString filename, m_fileContents[dirname].keys())
      {
        emit logMsg(QString("  file: %1 (%2 bytes)").arg(filename).arg(m_fileContents[dirname][filename].size()));
      }
    }
    emit logMsg("(End of filesystem entry list)");
  }

  return status;
}

bool TesterSim::saveState(const QString& filename)
{
  bool status = false;
  QFile outfile(filename);

  if (outfile.open(QIODevice::WriteOnly))
  {
    QDataStream out(&outfile);
    out << m_fileContents;
    outfile.close();
    status = true;
  }

  return status;
}

void TesterSim::log(const QString& line)
{
  emit logMsg(line);
}

const std::vector<uint8_t>& TesterSim::getSnapshotContent(int snapshotIndex)
{
  return m_snapshotData[snapshotIndex];
}

void TesterSim::setSnapshotContent(int snapshotIndex, const std::vector<uint8_t>& content)
{
  m_snapshotData[snapshotIndex] = content;
  emit logMsg(QString("Set snapshot data with %1 bytes").arg(content.size()));
}

