/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#include <cassert>
#include <sstream>
#include <uavcan/internal/marshalling/bit_stream.hpp>

namespace uavcan
{

int BitStream::write(const uint8_t* bytes, const int bitlen)
{
    // Temporary buffer is needed to merge new bits with cached unaligned bits from the last write() (see byte_cache_)
    uint8_t tmp[MAX_BYTES_PER_RW + 1];

    // Tmp space must be large enough to accomodate new bits AND unaligned bits from the last write()
    const int bytelen = bitlenToBytelen(bitlen + (bit_offset_ % 8));
    assert(MAX_BYTES_PER_RW >= bytelen);
    tmp[0] = tmp[bytelen - 1] = 0;

    std::fill(tmp, tmp + bytelen, 0);
    copyBitArray(bytes, 0, bitlen, tmp, bit_offset_ % 8);

    const int new_bit_offset = bit_offset_ + bitlen;

    // Bitcopy algorithm resets skipped bits in the first byte. Restore them back.
    tmp[0] |= byte_cache_;

    // (new_bit_offset % 8 == 0) means that this write was perfectly aligned.
    byte_cache_ = (new_bit_offset % 8) ? tmp[bytelen - 1] : 0;

    /*
     * Dump the data into the destination buffer.
     * Note that if this write was unaligned, last written byte in the buffer will be rewritten with updated value
     * within the next write() operation.
     */
    const int write_res = buf_.write(bit_offset_ / 8, tmp, bytelen);
    if (write_res < 0)
        return write_res;
    if (write_res != bytelen)
        return -1;

    bit_offset_ = new_bit_offset;
    return 0;
}

int BitStream::read(uint8_t* bytes, const int bitlen)
{
    uint8_t tmp[MAX_BYTES_PER_RW + 1];

    const int bytelen = bitlenToBytelen(bitlen + (bit_offset_ % 8));
    assert(MAX_BYTES_PER_RW >= bytelen);

    const int read_res = buf_.read(bit_offset_ / 8, tmp, bytelen);
    if (read_res < 0)
        return read_res;
    if (read_res != bytelen)
        return -1;

    std::fill(bytes, bytes + bitlenToBytelen(bitlen), 0);
    copyBitArray(tmp, bit_offset_ % 8, bitlen, bytes, 0);
    bit_offset_ += bitlen;
    return 0;
}

std::string BitStream::toString() const
{
    std::ostringstream os;
    for (int offset = 0; true; offset++)
    {
        uint8_t byte = 0;
        if (1 != buf_.read(offset, &byte, 1))
            break;
        for (int i = 7; i >= 0; i--)     // Most significant goes first
            os << !!(byte & (1 << i));
        os << " ";
    }
    std::string output = os.str();
    if (output.length())
        output.erase(output.length() - 1, 1);
    return output;
}

}
