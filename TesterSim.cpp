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

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

std::map<uint8_t,const char*> TesterSim::s_outColors =
{
  { 0x01, RED },
  { 0x02, RED },
  { 0x05, YEL },
  { 0x06, GRN }
};

// TODO: Determine what response (if any?) is expected from 0x11 (do slow init).
// TODO: Begin to implement responses for KWP71 and FIAT9141

std::map<uint8_t,std::function<void(const uint8_t*,uint8_t*,TesterSim*)>> TesterSim::s_commandProcs =
{
  { 0x01, TesterSim::process01TabletInfo },
  { 0x02, TesterSim::process02SerialNo },
  { 0x09, TesterSim::process09 },
  { 0x0A, TesterSim::process0AWorkshopData },
  { 0x0B, TesterSim::process0BStartApplModGest },
  { 0x11, TesterSim::process11DoSlowInit },
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

std::unordered_map<int,std::vector<uint8_t>> TesterSim::s_isoBytes =
{
  { 145, { 0x55, 0x00, 0x81 } }
};

TesterSim::TesterSim()
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

void TesterSim::setRAMLoc(uint16_t addr, uint8_t val)
{
  m_ramData[addr] = val;
}

int TesterSim::readBytes(uint8_t* buf, int count)
{
  int bufPos = 0;
  int readResult = 0;

  while (bufPos < count)
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
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

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

    if (s_commandProcs.count(m_inbuf[6]))
    {
      s_commandProcs.at(m_inbuf[6])(m_inbuf, m_outbuf, this);
    }
    else
    {
      m_outbuf[2] = 7;
      m_outbuf[7] = 1;
    }
    status = sendReply(print);
  }
  else
  {
    printf("Warning: received message of fewer than 7 bytes.\n");
  }
  return status;
}

bool TesterSim::connectToSocket(const std::string& sockPath)
{
  bool status = false;
  struct sockaddr_un addr;

  m_sockFd = socket((AF_UNIX), SOCK_STREAM, 0);
  if (m_sockFd > 0)
  {
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(m_sockFd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == 0)
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
    status = false;
    printf(".");
    fflush(stdout);
  }
  memcpy(m_lastInbuf, buf, numBytes);
  return status;
}

void TesterSim::printPacket(const uint8_t* buf)
{
  auto duration = std::chrono::system_clock::now().time_since_epoch();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  printf("%.3f : ", ms / 1000.0);

  const char* color = nullptr;
  for (int i = 0; i <= buf[2]; i++)
  {
    color = s_outColors.count(i) ? s_outColors.at(i) : "";
    printf("%s%02X " RESET, color, buf[i]);
  }
  printf("\n");
}

bool TesterSim::listen()
{
  bool status = true;
  int fullPacketSize = 0;

  while (status)
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
          printf("\n");
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
          printf("Error: reported packet size of %d too small\n",
                 fullPacketSize);
        }
        else
        {
          printf("Error receiving packet body\n");
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

void TesterSim::process01TabletInfo(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  printf("Request for Tester info\n");
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
  outbuf[19] = 0x00; // free space on flash storage
  outbuf[20] = 0x23;
  outbuf[21] = 0x33;
  outbuf[22] = 0x33;
  outbuf[23] = 0;   // serial num hi
  outbuf[24] = 212; // serial num lo
}

void TesterSim::process02SerialNo(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  printf("Request for Tester serial no.\n");
  outbuf[2] = 8;
  outbuf[7] = 0;   // hi byte
  outbuf[8] = 212; // lo byte
}

void TesterSim::process09(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  outbuf[1] = 0;
  outbuf[2] = 7;
  outbuf[7] = 0x10;
}

void TesterSim::process0AWorkshopData(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  printf("Request for workshop data\n");
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
  c = 'a';
  while (pos < 0x77)
  {
    outbuf[pos++] = '\0';
  }
}

void TesterSim::process0BStartApplModGest(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const uint16_t ecuId = (inbuf[7] * 0x100) + inbuf[8];
  const uint8_t pipeNum = inbuf[9];
  printf("--------------------------------------------\n"
         " Starting _applModGest%04d thread on pipe %d\n"
         "--------------------------------------------", ecuId, pipeNum);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  sim->m_applRun[pipeNum] = true;
  sim->m_currentECUID = ecuId;
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process11DoSlowInit(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const uint8_t ecuAddr = inbuf[7];
  printf("Do 5-baud slow init for ECU address 0x%02X\n", ecuAddr);

  if (sim->s_isoBytes.count(sim->m_currentECUID))
  {
    const std::vector<uint8_t>& isoBytes = sim->s_isoBytes.at(sim->m_currentECUID);
    const int isoByteCount = isoBytes.size();
    outbuf[2] = 10;
    outbuf[2] = 7 + isoByteCount;
    outbuf[7] = 1;
    for (int i = 0; i < isoByteCount; i++)
    {
      outbuf[8 + i] = isoBytes[i];
    }
  }
  else
  {
    printf("Warning: no ISO byte record for ECU ID %04d\n", sim->m_currentECUID);
  }
}

void TesterSim::process13CommandToECU(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  // Additional delay beyond the normal artificial delay we insert before replying.
  // Just doing this to slow things down and make them a bit easier to follow when
  // ECU parameters are being updated.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  if (inbuf[7] == 0x00) // Req ID code
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
  else if (inbuf[7] == 0x01) // Read RAM
  {
    const uint8_t count = inbuf[8];
    const uint16_t addr = (inbuf[9] * 0x100) + inbuf[10];
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
    outbuf[2] = 7;
    outbuf[7] = 1;
  }
}

void TesterSim::process15DisplayString(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  std::string dstring((char*)(inbuf + 14), inbuf[2] - 13);
  printf("Display string on Tester screen: %s\n", dstring.c_str());
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process1C(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const uint8_t pipeNum = inbuf[5];
  printf("Shut down ECU appl thread monitoring pipe %d\n", pipeNum);

  if (sim->m_applRun[pipeNum])
  {
    sim->m_applRun[pipeNum] = false;
    outbuf[2] = 7;
    outbuf[7] = 1;
  }
  else
  {
    printf("Thread not yet running; replying with negative status from applModGen...\n");
    outbuf[2] = 7;
    outbuf[5] = 0;
    outbuf[7] = 0xfe;
  }
}

void TesterSim::process1ECloseFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  sim->m_curFileContents = nullptr;
  printf("Close file (which is currently %s)\n", sim->m_curFile.c_str());
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process20OpenFileForWriting(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const std::string filenameWithPath((char*)(inbuf + 7), inbuf[2] - 6);
  const size_t lastPathSeparator = filenameWithPath.find_last_of('/');
  const std::string filenameOnly = (lastPathSeparator == std::string::npos) ?
    filenameWithPath : filenameWithPath.substr(lastPathSeparator + 1);
  const std::string dirOnly = filenameWithPath.substr(0, lastPathSeparator);

  sim->m_curDir = dirOnly;
  sim->m_curFile = filenameOnly;
  sim->m_curFileContents = &(sim->m_fileContents[dirOnly][filenameOnly]);
  sim->m_curFileContents->clear(); // only truncate is supported (no append)
  printf("Open file for writing: %s (in dir %s)\n", sim->m_curFile.c_str(), sim->m_curDir.c_str());
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process21WriteToFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const int byteCount = inbuf[2] - 0xb;
  printf("Write %d bytes to file\n", byteCount);
  std::vector<uint8_t> newBuf(&inbuf[0xb], &inbuf[0xb] + byteCount);
  sim->m_curFileContents->insert(sim->m_curFileContents->end(), newBuf.begin(), newBuf.end());
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process23OpenFileForReading(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  const std::string filenameWithPath((char*)(inbuf + 7), inbuf[2] - 6);
  const size_t lastPathSeparator = filenameWithPath.find_last_of('/');
  const std::string filenameOnly = (lastPathSeparator == std::string::npos) ?
    filenameWithPath : filenameWithPath.substr(lastPathSeparator + 1);
  const std::string dirOnly = filenameWithPath.substr(0, lastPathSeparator);

  sim->m_curDir = dirOnly;
  sim->m_curFile = filenameOnly;
  sim->m_fileReadPos = 0;
  sim->m_curFileContents = &(sim->m_fileContents[dirOnly][filenameOnly]);
  memset(sim->m_checksumBuf, 0, CHKSUM_BUF_SIZE);

  printf("Open file for reading: %s (in dir %s)\n", sim->m_curFile.c_str(), sim->m_curDir.c_str());
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process24ReadFromFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  if (!sim->m_curFileContents)
  {
    printf("Error: m_curFileContents is null. File read/write operation without an open file?\n");
    return;
  }

  const int bytesLeftInFile = (sim->m_curFileContents->size() - sim->m_fileReadPos);
  const int numBytesToSend = (bytesLeftInFile >= CHKSUM_BUF_SIZE) ? CHKSUM_BUF_SIZE : bytesLeftInFile;
  printf("Read from file (%d bytes left in file, %d bytes to send in this chunk starting at file pos %08X)\n",
         bytesLeftInFile, numBytesToSend, sim->m_fileReadPos);
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
    printf("Computed checksum of %02X for this chunk\n", outbuf[checksumBufPos]);
    sim->m_fileReadPos += numBytesToSend;
  }
  else
  {
    // No more bytes to send from this file.
    // Indicate this with a 00 in position 0xC of the reply message.
    outbuf[12] = 0;
  }
}

void TesterSim::process25ChecksumFile(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  printf("Request for checksum verification of file\n");
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
  const std::string curDir((char*)(inbuf + 7), inbuf[2] - 6);
  sim->m_curDir = curDir;
  sim->m_curDirIterator = sim->m_fileContents[curDir].begin();
  printf("Change directory: %s\n", curDir.c_str());
  outbuf[2] = 7;
  outbuf[7] = 1;
}

void TesterSim::process2BGetNextDirEntry(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  printf("Request for next directory entry\n");

  if (sim->m_curDirIterator !=
      sim->m_fileContents[sim->m_curDir].end())
  {
    const std::string filename = sim->m_curDirIterator->first;
    const uint32_t filesize = sim->m_curDirIterator->second.size();
    printf(" File: %s, size %u\n", filename.c_str(), filesize);
    const uint8_t truncLen = (filename.length() < 90) ? filename.length() : 90;
    printf(" Truncated length of filename: %d\n", truncLen);

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
    strcpy((char*)(outbuf + 17), "AUG-06-1998  13:24:55");
    strncpy((char*)(outbuf + 38), filename.c_str(), 89);

    sim->m_curDirIterator++;
  }
  else
  {
    // indicate end of directory
    outbuf[2] = 7;
    outbuf[7] = 4;
  }
}

void TesterSim::process3AGetDateTime(const uint8_t* inbuf, uint8_t* outbuf, TesterSim* sim)
{
  printf("Request for Tester date/time\n");
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
  printf("Command to erase flash on Tester\n");
  outbuf[2] = 8;
  outbuf[8] = inbuf[7];
  outbuf[7] = 1;
}

