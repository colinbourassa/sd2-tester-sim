#include "utilities.h"

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
void add8BitChecksum(uint8_t* frame)
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
void add16BitChecksum(uint8_t* frame)
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

