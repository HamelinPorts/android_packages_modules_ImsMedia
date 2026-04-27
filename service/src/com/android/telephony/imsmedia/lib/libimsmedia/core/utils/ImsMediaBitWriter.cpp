/**
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ImsMediaBitWriter.h>
#include <ImsMediaTrace.h>
#include <string.h>

ImsMediaBitWriter::ImsMediaBitWriter()
{
    mBuffer = nullptr;
    mMaxBufferSize = 0;
    mBytePos = 0;
    mBitPos = 0;
    mBitBuffer = 0;
    mBufferFull = false;
}

ImsMediaBitWriter::~ImsMediaBitWriter() {}

void ImsMediaBitWriter::SetBuffer(uint8_t* pbBuffer, uint32_t nBufferSize)
{
    mBytePos = 0;
    mBitPos = 0;
    mBitBuffer = 0;
    mBufferFull = false;
    mBuffer = pbBuffer;
    mMaxBufferSize = nBufferSize;
}

bool ImsMediaBitWriter::Write(uint32_t nValue, uint32_t nSize)
{
    if (nSize == 0)
    {
        return false;
    }

    if (mBuffer == nullptr || nSize > 24 || mBufferFull)
    {
        IMLOGE2("[Write] nSize[%d], BufferFull[%d]", nSize, mBufferFull);
        return false;
    }

    // write to bit buffer
    mBitBuffer += (nValue << (32 - nSize) >> mBitPos);
    mBitPos += nSize;

    // write to byte buffer
    while (mBitPos >= 8)
    {
        // OR-merge into the destination byte instead of overwriting.
        // Two BitWriters frequently target the same backing buffer at
        // overlapping byte offsets (e.g. AudioRtpPayloadEncoderNode uses
        // separate header/payload writers that meet at the byte holding
        // the F+FT+Q ToC bits and the first speech bits). With plain
        // assignment, whichever writer spilled second clobbered the
        // other's bits — most visibly turning AMR-WB FT=15 NoData frames
        // into wire FT=14 SPEECH_LOST when the payload writer's
        // AddPadding spilled zeros over a header byte holding Q=1.
        // The byte starts at zero (memset before encoding) so OR
        // matches `=` on the first writer, and merges correctly on
        // subsequent ones. Flush() already uses `+=` for the same
        // reason; this brings the spill path in line.
        mBuffer[mBytePos++] |= (uint8_t)(mBitBuffer >> 24);
        mBitBuffer <<= 8;
        mBitPos -= 8;
    }

    if (mBytePos >= mMaxBufferSize)
    {
        mBufferFull = true;
    }

    return true;
}

bool ImsMediaBitWriter::WriteByteBuffer(uint8_t* pbSrc, uint32_t nBitSize)
{
    uint32_t nByteSize;
    uint32_t nRemainBitSize;
    nByteSize = nBitSize >> 3;
    nRemainBitSize = nBitSize & 0x07;

    if (mBitPos == 0)
    {
        memcpy(mBuffer + mBytePos, pbSrc, nByteSize);
        mBytePos += nByteSize;
    }
    else
    {
        uint32_t i;

        for (i = 0; i < nByteSize; i++)
        {
            if (!Write(pbSrc[i], 8))
            {
                return false;
            }
        }
    }

    if (nRemainBitSize > 0)
    {
        uint32_t v = pbSrc[nByteSize];
        v >>= (8 - nRemainBitSize);

        if (!Write(v, nRemainBitSize))
        {
            return false;
        }
    }

    return true;
}

bool ImsMediaBitWriter::WriteByteBuffer(uint32_t value)
{
    uint32_t nRemainBitSize = 32;

    for (int32_t i = 0; i < 4; i++)
    {
        nRemainBitSize -= 8;
        uint8_t v = (value >> nRemainBitSize) & 0x00ff;

        if (!Write(v, 8))
        {
            return false;
        }
    }

    return true;
}

void ImsMediaBitWriter::Seek(uint32_t nSize)
{
    Flush();
    mBitPos += nSize;

    while (mBitPos >= 8)
    {
        mBytePos++;
        mBitPos -= 8;
    }
}

void ImsMediaBitWriter::AddPadding()
{
    if (mBitPos > 0)
    {
        Write(0, 8 - mBitPos);
    }
}

uint32_t ImsMediaBitWriter::GetBufferSize()
{
    uint32_t nSize;
    nSize = (mBitPos + 7) >> 3;
    nSize += mBytePos;
    return nSize;
}

void ImsMediaBitWriter::Flush()
{
    if (mBitPos > 0)
    {
        mBuffer[mBytePos] += (uint8_t)(mBitBuffer >> 24);
        mBitBuffer = 0;
    }
}