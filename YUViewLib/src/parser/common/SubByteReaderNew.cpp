/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen
 * University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   In addition, as a special exception, the copyright holders give
 *   permission to link the code of portions of this program with the
 *   OpenSSL library under certain conditions as described in each
 *   individual source file, and distribute linked combinations including
 *   the two.
 *
 *   You must obey the GNU General Public License in all respects for all
 *   of the code used other than OpenSSL. If you modify file(s) with this
 *   exception, you may extend this exception to your version of the
 *   file(s), but you are not obligated to do so. If you do not wish to do
 *   so, delete this exception statement from your version. If you delete
 *   this exception statement from all source files in the program, then
 *   also delete it here.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SubByteReaderNew.h"

#include <cassert>
#include <stdexcept>

namespace parser
{

SubByteReaderNew::SubByteReaderNew(const ByteVector &inArr, size_t inArrOffset)
    : byteVector(inArr), posInBufferBytes(inArrOffset), initialPosInBuffer(inArrOffset){};

std::tuple<uint64_t, std::string> SubByteReaderNew::readBits(size_t nrBits)
{
  uint64_t out        = 0;
  auto     nrBitsRead = nrBits;

  // The return unsigned int is of depth 64 bits
  if (nrBits > 64)
    throw std::logic_error("Trying to read more than 64 bits at once from the bitstream.");
  if (nrBits == 0)
    return {0, ""};

  while (nrBits > 0)
  {
    if (this->posInBufferBits == 8 && nrBits != 0)
    {
      // We read all bits we could from the current byte but we need more. Go to
      // the next byte.
      if (!this->gotoNextByte())
        // We are at the end of the buffer but we need to read more. Error.
        throw std::logic_error("Error while reading annexB file. Trying to "
                               "read over buffer boundary.");
    }

    // How many bits can be gotton from the current byte?
    auto curBitsLeft = 8 - this->posInBufferBits;

    size_t readBits; // Nr of bits to read
    size_t offset;   // Offset for reading from the right
    if (nrBits >= curBitsLeft)
    {
      // Read "curBitsLeft" bits
      readBits = curBitsLeft;
      offset   = 0;
    }
    else
    {
      // Read "nrBits" bits
      assert(nrBits < 8 && nrBits < curBitsLeft);
      readBits = nrBits;
      offset   = curBitsLeft - nrBits;
    }

    // Shift output value so that the new bits fit
    out = out << readBits;

    char c   = this->byteVector[this->posInBufferBytes];
    c        = c >> offset;
    int mask = ((1 << readBits) - 1);

    // Write bits to output
    out += (c & mask);

    // Update counters
    nrBits -= readBits;
    this->posInBufferBits += readBits;
  }

  std::string bitsRead;
  assert(nrBitsRead > 0);
  for (auto i = nrBitsRead - 1; i >= 0; i--)
  {
    if (out & (uint64_t(1) << i))
      bitsRead.push_back('1');
    else
      bitsRead.push_back('0');
  }

  return {out, bitsRead};
}

ByteVector SubByteReaderNew::readBytes(size_t nrBytes)
{
  if (this->posInBufferBits != 0 && this->posInBufferBits != 8)
    throw std::logic_error("When reading bytes from the bitstream, it should be byte aligned.");

  if (this->posInBufferBits == 8)
    if (!this->gotoNextByte())
      // We are at the end of the buffer but we need to read more. Error.
      throw std::logic_error("Error while reading annexB file. Trying to read "
                             "over buffer boundary.");

  ByteVector retVector;
  for (unsigned i = 0; i < nrBytes; i++)
  {
    retVector.push_back(this->byteVector[this->posInBufferBytes]);

    if (!this->gotoNextByte())
    {
      if (i < nrBytes - 1)
        throw std::logic_error("Error while reading annexB file. Trying to "
                               "read over buffer boundary.");
    }
  }

  return retVector;
}

std::tuple<uint64_t, std::string> SubByteReaderNew::readUE_V()
{
  std::string coding;
  {
    auto [firstBit, firstBitCoding] = this->readBits(1);
    if (firstBit == 1)
      return {0, firstBitCoding};
    coding += firstBitCoding;
  }

  // Get the length of the golomb
  unsigned golLength = 0;
  while (true)
  {
    auto [readBit, readCoding] = this->readBits(1);
    coding += readCoding;
    golLength++;
    if (readBit == 1)
      break;
  }

  auto [golBits, golCoding] = this->readBits(golLength);
  coding += golCoding;
  // Exponential part
  auto val = golBits + (uint64_t(1) << golLength) - 1;

  return {val, coding};
}

std::tuple<int64_t, std::string> SubByteReaderNew::readSE_V()
{
  auto [val, coding] = this->readUE_V();
  if (val % 2 == 0)
    return {-int64_t((val + 1) / 2), coding};
  else
    return {int64_t((val + 1) / 2), coding};
}

std::tuple<uint64_t, std::string> SubByteReaderNew::readLeb128()
{
  assert(false);

  // // We will read full bytes (up to 8)
  // // The highest bit indicates if we need to read another bit. The rest of the
  // // bits is added to the counter (shifted accordingly) See the AV1 reading
  // // specification
  // uint64_t value = 0;
  // for (int i = 0; i < 8; i++)
  // {
  //   int leb128_byte = readBits(8, bitsRead);
  //   bit_count += 8;
  //   value |= ((leb128_byte & 0x7f) << (i * 7));
  //   if (!(leb128_byte & 0x80))
  //     break;
  // }
  // return value;

  return {};
}

std::tuple<uint64_t, std::string> SubByteReaderNew::readUVLC()
{
  assert(false);

  // int leadingZeros = 0;
  // while (1)
  // {
  //   int done = readBits(1, bitsRead);
  //   bit_count += 1;
  //   if (done)
  //     break;
  //   leadingZeros++;
  // }
  // if (leadingZeros >= 32)
  //   return ((uint64_t)1 << 32) - 1;
  // uint64_t value = readBits(leadingZeros, bitsRead);
  // return value + ((uint64_t)1 << leadingZeros) - 1;

  return {};
}

std::tuple<int64_t, std::string> SubByteReaderNew::readNS()
{
  assert(false);

  // // FloorLog2
  // int floorVal;
  // {
  //   int x = maxVal;
  //   int s = 0;
  //   while (x != 0)
  //   {
  //     x = x >> 1;
  //     s++;
  //   }
  //   floorVal = s - 1;
  // }

  // int w = floorVal + 1;
  // int m = (1 << w) - maxVal;
  // int v = readBits(w - 1, bitsRead);
  // bit_count += w - 1;
  // if (v < m)
  //   return v;
  // int extra_bit = readBits(1, bitsRead);
  // bit_count++;
  // return (v << 1) - m + extra_bit;

  return {};
}

std::tuple<int64_t, std::string> SubByteReaderNew::readSU()
{
  assert(false);

  // int value    = readBits(nrBits, bitsRead);
  // int signMask = 1 << (nrBits - 1);
  // if (value & signMask)
  //   value = value - 2 * signMask;
  // return value;

  return {};
}

/* Is there more data? There is no more data if the next bit is the terminating
 * bit and all following bits are 0. */
bool SubByteReaderNew::more_rbsp_data()
{
  auto posBytes            = this->posInBufferBytes;
  auto posBits             = this->posInBufferBits;
  auto terminatingBitFound = false;
  if (posBits == 8)
  {
    posBytes++;
    posBits = 0;
  }
  else
  {
    // Check the remainder of the current byte
    unsigned char c = this->byteVector[posBytes];
    if (c & (1 << (7 - posBits)))
      terminatingBitFound = true;
    else
      return true;
    posBits++;
    while (posBits != 8)
    {
      if (c & (1 << (7 - posBits)))
        // Only zeroes should follow
        return true;
      posBits++;
    }
    posBytes++;
  }
  while (posBytes < (unsigned int)this->byteVector.size())
  {
    unsigned char c = this->byteVector[posBytes];
    if (terminatingBitFound && c != 0)
      return true;
    else if (!terminatingBitFound && (c & 128))
      terminatingBitFound = true;
    else
      return true;
    posBytes++;
  }
  if (!terminatingBitFound)
    return true;
  return false;
}

/* Is there more data? If the current position in the sei_payload() syntax
 structure is not the position of the last (least significant, right- most)
 bit that is equal to 1 that is less than 8 * payloadSize bits from the
 beginning of the syntax structure (i.e., the position of the
 payload_bit_equal_to_one syntax element), the return value of
 payload_extension_present( ) is equal to TRUE.
*/
bool SubByteReaderNew::payload_extension_present()
{
  // TODO: What is the difference to this?
  return more_rbsp_data();
}

bool SubByteReaderNew::testReadingBits(unsigned nrBits)
{
  assert(this->posInBufferBits <= 8);
  const auto curBitsLeft = 8 - this->posInBufferBits;
  assert(this->byteVector.size() > this->posInBufferBytes);
  const auto entireBytesLeft  = this->byteVector.size() - this->posInBufferBytes - 1;
  const auto nrBitsLeftToRead = curBitsLeft + entireBytesLeft * 8;

  return nrBits <= nrBitsLeftToRead;
}

size_t SubByteReaderNew::nrBytesRead() const
{
  return this->posInBufferBytes - this->initialPosInBuffer + (this->posInBufferBits != 0 ? 1 : 0);
}

size_t SubByteReaderNew::nrBytesLeft() const
{
  if (this->byteVector.size() <= this->posInBufferBytes)
    return 0;
  return this->byteVector.size() - this->posInBufferBytes - 1;
}

bool SubByteReaderNew::gotoNextByte()
{
  // Before we go to the neyt byte, check if the last (current) byte is a zero
  // byte.
  if (this->posInBufferBytes >= unsigned(this->byteVector.size()))
    throw std::out_of_range("Reading out of bounds");
  if (this->byteVector[this->posInBufferBytes] == (char)0)
    this->numEmuPrevZeroBytes++;

  // Skip the remaining sub-byte-bits
  this->posInBufferBits = 0;
  // Advance pointer
  this->posInBufferBytes++;

  if (this->posInBufferBytes >= (unsigned int)this->byteVector.size())
    // The next byte is outside of the current buffer. Error.
    return false;

  if (this->skipEmulationPrevention)
  {
    if (this->numEmuPrevZeroBytes == 2 && this->byteVector[this->posInBufferBytes] == (char)3)
    {
      // The current byte is an emulation prevention 3 byte. Skip it.
      this->posInBufferBytes++; // Skip byte

      if (this->posInBufferBytes >= (unsigned int)this->byteVector.size())
      {
        // The next byte is outside of the current buffer. Error
        return false;
      }

      // Reset counter
      this->numEmuPrevZeroBytes = 0;
    }
    else if (this->byteVector[this->posInBufferBytes] != (char)0)
      // No zero byte. No emulation prevention 3 byte
      this->numEmuPrevZeroBytes = 0;
  }

  return true;
}

} // namespace parser