#ifndef LZW_HPP
#define LZW_HPP
// -------
//  SETUP
// -------
// #define LZW_IMPLEMENTATION in one source file before including
// this file, then use lzw.hpp as a normal header file elsewhere.
//
// ----------
//  OVERVIEW
// ----------
// Lempel–Ziv–Welch (LZW) encoder/decoder.
//
// This is the compression scheme used by the GIF image format and the Unix 'compress' tool.
// Main differences from this implementation is that End Of Input (EOI) and Clear Codes (CC)
// are not stored in the output and the max code length in bits is 12, vs 16 in compress.
//
// EOI is simply detected by the end of the data stream, while CC happens if the
// dictionary gets filled. Data is written/read from bit streams, which handle
// byte-alignment for us in a transparent way.
//
// The decoder relies on the hardcoded data layout produced by the encoder, since
// no additional reconstruction data is added to the output, so they must match.
// The nice thing about LZW is that we can reconstruct the dictionary directly from
// the stream of codes generated by the encoder, so this avoids storing additional
// headers in the bit stream.
//
// The output code length is variable. It starts with the minimum number of bits
// required to store the base byte-sized dictionary and automatically increases
// as the dictionary gets larger (it starts at 9-bits and grows to 10-bits when
// code 512 is added, then 11-bits when 1024 is added, and so on). If the dictionary
// is filled (4096 items for a 12-bits dictionary), the whole thing is cleared and
// the process starts over. This is the main reason why the encoder and the decoder
// must match perfectly, since the lengths of the codes will not be specified with
// the data itself.

#include <cstdint>
#include <cstdlib>

// Disable the bit stream => std::string dumping methods.
#ifndef LZW_NO_STD_STRING

#include <string>

#endif // LZW_NO_STD_STRING

// If you provide a custom malloc(), you must also provide a custom free().
// Note: We never check LZW_MALLOC's return for null. A custom implementation
// should just abort with a fatal error if the program runs out of memory.
#ifndef LZW_MALLOC
#define LZW_MALLOC std::malloc
#define LZW_MFREE std::free
#endif // LZW_MALLOC

namespace lzw {

// ========================================================

// The default fatalError() function writes to stderr and aborts.
#ifndef LZW_ERROR

    void fatalError(const char *message);

#define LZW_USING_DEFAULT_ERROR_HANDLER
#define LZW_ERROR(message) ::lzw::fatalError(message)
#endif // LZW_ERROR

    // ========================================================
    // class BitStreamWriter:
    // ========================================================

    class BitStreamWriter final {
    public:
        // No copy/assignment.
        BitStreamWriter(const BitStreamWriter &) = delete;

        BitStreamWriter &operator=(const BitStreamWriter &) = delete;

        BitStreamWriter();

        explicit BitStreamWriter(int initialSizeInBits, int growthGranularity = 2);

        void allocate(int bitsWanted);

        void setGranularity(int growthGranularity);

        std::uint8_t *release();

        void appendBit(int bit);

        void appendBitsU64(std::uint64_t num, int bitCount);

#ifndef LZW_NO_STD_STRING

        std::string toBitString() const; // Useful for debugging.
        void appendBitString(const std::string &bitStr);

#endif // LZW_NO_STD_STRING

        int getByteCount() const;

        int getBitCount() const;

        const std::uint8_t *getBitStream() const;

        ~BitStreamWriter();

    private:
        void internalInit();

        static std::uint8_t *allocBytes(int bytesWanted, std::uint8_t *oldPtr, int oldSize);

        std::uint8_t *stream; // Growable buffer to store our bits. Heap allocated & owned by the class instance.
        int bytesAllocated;   // Current size of heap-allocated stream buffer *in bytes*.
        int granularity;      // Amount bytesAllocated multiplies by when auto-resizing in appendBit().
        int currBytePos;      // Current byte being written to, from 0 to bytesAllocated-1.
        int nextBitPos;       // Bit position within the current byte to access next. 0 to 7.
        int numBitsWritten;   // Number of bits in use from the stream buffer, not including byte-rounding padding.
    };

    // ========================================================
    // class BitStreamReader:
    // ========================================================

    class BitStreamReader final {
    public:
        // No copy/assignment.
        BitStreamReader(const BitStreamReader &) = delete;

        BitStreamReader &operator=(const BitStreamReader &) = delete;

        BitStreamReader(const BitStreamWriter &bitStreamWriter);

        BitStreamReader(const std::uint8_t *bitStream, int byteCount, int bitCount);

        bool isEndOfStream() const;

        bool readNextBit(int &bitOut);

        std::uint64_t readBitsU64(int bitCount);

        void reset();

    private:
        const std::uint8_t *stream; // Pointer to the external bit stream. Not owned by the reader.
        const int sizeInBytes;      // Size of the stream *in bytes*. Might include padding.
        const int sizeInBits;       // Size of the stream *in bits*, padding *not* include.
        int currBytePos;            // Current byte being read in the stream.
        int nextBitPos;             // Bit position within the current byte to access next. 0 to 7.
        int numBitsRead;            // Total bits read from the stream so far. Never includes byte-rounding padding.
    };

    // ========================================================
    // LZW Dictionary helper:
    // ========================================================

    constexpr int Nil = -1;
    constexpr int MaxDictBits = 12;
    constexpr int StartBits = 9;
    constexpr int FirstCode = (1 << (StartBits - 1));  // 256
    constexpr int MaxDictEntries = (1 << MaxDictBits); // 4096

    class Dictionary final {
    public:
        struct Entry {
            int code;
            int value;
        };

        // Dictionary entries 0-255 are always reserved to the byte/ASCII range.
        int size;
        Entry entries[MaxDictEntries];

        Dictionary();

        int findIndex(int code, int value) const;

        bool add(int code, int value);

        bool flush(int &codeBitsWidth);
    };

    // ========================================================
    // easyEncode() / easyDecode():
    // ========================================================

    // Quick LZW data compression. Output compressed data is heap allocated
    // with LZW_MALLOC() and should be later freed with LZW_MFREE().
    void easyEncode(const std::uint8_t *uncompressed, int uncompressedSizeBytes,
                    std::uint8_t **compressed, int *compressedSizeBytes, int *compressedSizeBits);

    // Decompress back the output of easyEncode().
    // The uncompressed output buffer is assumed to be big enough to hold the uncompressed data,
    // if it happens to be smaller, the decoder will return a partial output and the return value
    // of this function will be less than uncompressedSizeBytes.
    int easyDecode(const std::uint8_t *compressed, int compressedSizeBytes, int compressedSizeBits,
                   std::uint8_t *uncompressed, int uncompressedSizeBytes);

} // namespace lzw {}

// ================== End of header file ==================
#endif // LZW_HPP
// ================== End of header file ==================

// ================================================================================================
//
//                                     LZW Implementation
//
// ================================================================================================

#ifdef LZW_IMPLEMENTATION

#ifdef LZW_USING_DEFAULT_ERROR_HANDLER
#include <cstdio> // For the default error handler
#endif            // LZW_USING_DEFAULT_ERROR_HANDLER

#include <cassert>
#include <cstring>

namespace lzw
{

    // ========================================================

    // Round up to the next power-of-two number, e.g. 37 => 64
    static int nextPowerOfTwo(int num)
    {
        --num;
        for (std::size_t i = 1; i < sizeof(num) * 8; i <<= 1)
        {
            num = num | num >> i;
        }
        return ++num;
    }

    // ========================================================

#ifdef LZW_USING_DEFAULT_ERROR_HANDLER

    // Prints a fatal error to stderr and aborts the process.
    // This is the default method used by LZW_ERROR(), but
    // you can override the macro to use other error handling
    // mechanisms, such as C++ exceptions.
    void fatalError(const char *const message)
    {
        std::fprintf(stderr, "LZW encoder/decoder error: %s\n", message);
        std::abort();
    }

#endif // LZW_USING_DEFAULT_ERROR_HANDLER

    // ========================================================
    // class BitStreamWriter:
    // ========================================================

    BitStreamWriter::BitStreamWriter()
    {
        // 8192 bits for a start (1024 bytes). It will resize if needed.
        // Default granularity is 2.
        internalInit();
        allocate(8192);
    }

    BitStreamWriter::BitStreamWriter(const int initialSizeInBits, const int growthGranularity)
    {
        internalInit();
        setGranularity(growthGranularity);
        allocate(initialSizeInBits);
    }

    BitStreamWriter::~BitStreamWriter()
    {
        if (stream != nullptr)
        {
            LZW_MFREE(stream);
        }
    }

    void BitStreamWriter::internalInit()
    {
        stream = nullptr;
        bytesAllocated = 0;
        granularity = 2;
        currBytePos = 0;
        nextBitPos = 0;
        numBitsWritten = 0;
    }

    void BitStreamWriter::allocate(int bitsWanted)
    {
        // Require at least a byte.
        if (bitsWanted <= 0)
        {
            bitsWanted = 8;
        }

        // Round upwards if needed:
        if ((bitsWanted % 8) != 0)
        {
            bitsWanted = nextPowerOfTwo(bitsWanted);
        }

        // We might already have the required count.
        const int sizeInBytes = bitsWanted / 8;
        if (sizeInBytes <= bytesAllocated)
        {
            return;
        }

        stream = allocBytes(sizeInBytes, stream, bytesAllocated);
        bytesAllocated = sizeInBytes;
    }

    void BitStreamWriter::appendBit(const int bit)
    {
        const std::uint32_t mask = std::uint32_t(1) << nextBitPos;
        stream[currBytePos] = (stream[currBytePos] & ~mask) | (-bit & mask);
        ++numBitsWritten;

        if (++nextBitPos == 8)
        {
            nextBitPos = 0;
            if (++currBytePos == bytesAllocated)
            {
                allocate(bytesAllocated * granularity * 8);
            }
        }
    }

    void BitStreamWriter::appendBitsU64(const std::uint64_t num, const int bitCount)
    {
        assert(bitCount <= 64);
        for (int b = 0; b < bitCount; ++b)
        {
            const std::uint64_t mask = std::uint64_t(1) << b;
            const int bit = !!(num & mask);
            appendBit(bit);
        }
    }

#ifndef LZW_NO_STD_STRING

    void BitStreamWriter::appendBitString(const std::string &bitStr)
    {
        for (std::size_t i = 0; i < bitStr.length(); ++i)
        {
            appendBit(bitStr[i] == '0' ? 0 : 1);
        }
    }

    std::string BitStreamWriter::toBitString() const
    {
        std::string bitString;

        int usedBytes = numBitsWritten / 8;
        int leftovers = numBitsWritten % 8;
        if (leftovers != 0)
        {
            ++usedBytes;
        }
        assert(usedBytes <= bytesAllocated);

        for (int i = 0; i < usedBytes; ++i)
        {
            const int nBits = (leftovers == 0) ? 8 : (i == usedBytes - 1) ? leftovers
                                                                          : 8;
            for (int j = 0; j < nBits; ++j)
            {
                bitString += (stream[i] & (1 << j) ? '1' : '0');
            }
        }

        return bitString;
    }

#endif // LZW_NO_STD_STRING

    std::uint8_t *BitStreamWriter::release()
    {
        std::uint8_t *oldPtr = stream;
        internalInit();
        return oldPtr;
    }

    void BitStreamWriter::setGranularity(const int growthGranularity)
    {
        granularity = (growthGranularity >= 2) ? growthGranularity : 2;
    }

    int BitStreamWriter::getByteCount() const
    {
        int usedBytes = numBitsWritten / 8;
        int leftovers = numBitsWritten % 8;
        if (leftovers != 0)
        {
            ++usedBytes;
        }
        assert(usedBytes <= bytesAllocated);
        return usedBytes;
    }

    int BitStreamWriter::getBitCount() const
    {
        return numBitsWritten;
    }

    const std::uint8_t *BitStreamWriter::getBitStream() const
    {
        return stream;
    }

    std::uint8_t *BitStreamWriter::allocBytes(const int bytesWanted, std::uint8_t *oldPtr, const int oldSize)
    {
        std::uint8_t *newMemory = static_cast<std::uint8_t *>(LZW_MALLOC(bytesWanted));
        std::memset(newMemory, 0, bytesWanted);

        if (oldPtr != nullptr)
        {
            std::memcpy(newMemory, oldPtr, oldSize);
            LZW_MFREE(oldPtr);
        }

        return newMemory;
    }

    // ========================================================
    // class BitStreamReader:
    // ========================================================

    BitStreamReader::BitStreamReader(const BitStreamWriter &bitStreamWriter)
        : stream(bitStreamWriter.getBitStream()), sizeInBytes(bitStreamWriter.getByteCount()), sizeInBits(bitStreamWriter.getBitCount())
    {
        reset();
    }

    BitStreamReader::BitStreamReader(const std::uint8_t *bitStream, const int byteCount, const int bitCount)
        : stream(bitStream), sizeInBytes(byteCount), sizeInBits(bitCount)
    {
        reset();
    }

    bool BitStreamReader::readNextBit(int &bitOut)
    {
        if (numBitsRead >= sizeInBits)
        {
            return false; // We are done.
        }

        const std::uint32_t mask = std::uint32_t(1) << nextBitPos;
        bitOut = !!(stream[currBytePos] & mask);
        ++numBitsRead;

        if (++nextBitPos == 8)
        {
            nextBitPos = 0;
            ++currBytePos;
        }
        return true;
    }

    std::uint64_t BitStreamReader::readBitsU64(const int bitCount)
    {
        assert(bitCount <= 64);

        std::uint64_t num = 0;
        for (int b = 0; b < bitCount; ++b)
        {
            int bit;
            if (!readNextBit(bit))
            {
                LZW_ERROR("Failed to read bits from stream! Unexpected end.");
                break;
            }

            // Based on a "Stanford bit-hack":
            // http://graphics.stanford.edu/~seander/bithacks.html#ConditionalSetOrClearBitsWithoutBranching
            const std::uint64_t mask = std::uint64_t(1) << b;
            num = (num & ~mask) | (-bit & mask);
        }

        return num;
    }

    void BitStreamReader::reset()
    {
        currBytePos = 0;
        nextBitPos = 0;
        numBitsRead = 0;
    }

    bool BitStreamReader::isEndOfStream() const
    {
        return numBitsRead >= sizeInBits;
    }

    // ========================================================
    // class Dictionary:
    // ========================================================

    Dictionary::Dictionary()
    {
        // First 256 dictionary entries are reserved to the byte/ASCII
        // range. Additional entries follow for the character sequences
        // found in the input. Up to 4096 - 256 (MaxDictEntries - FirstCode).
        size = FirstCode;
        for (int i = 0; i < size; ++i)
        {
            entries[i].code = Nil;
            entries[i].value = i;
        }
    }

    int Dictionary::findIndex(const int code, const int value) const
    {
        if (code == Nil)
        {
            return value;
        }

        // Linear search for now.
        // TODO: Worth optimizing with a proper hash-table?
        for (int i = 0; i < size; ++i)
        {
            if (entries[i].code == code && entries[i].value == value)
            {
                return i;
            }
        }

        return Nil;
    }

    bool Dictionary::add(const int code, const int value)
    {
        if (size == MaxDictEntries)
        {
            LZW_ERROR("Dictionary overflowed!");
            return false;
        }

        entries[size].code = code;
        entries[size].value = value;
        ++size;
        return true;
    }

    bool Dictionary::flush(int &codeBitsWidth)
    {
        if (size == (1 << codeBitsWidth))
        {
            ++codeBitsWidth;
            if (codeBitsWidth > MaxDictBits)
            {
                // Clear the dictionary (except the first 256 byte entries).
                codeBitsWidth = StartBits;
                size = FirstCode;
                return true;
            }
        }
        return false;
    }

    // ========================================================
    // easyEncode() implementation:
    // ========================================================

    void easyEncode(const std::uint8_t *uncompressed, int uncompressedSizeBytes,
                    std::uint8_t **compressed, int *compressedSizeBytes, int *compressedSizeBits)
    {
        if (uncompressed == nullptr || compressed == nullptr)
        {
            LZW_ERROR("lzw::easyEncode(): Null data pointer(s)!");
            return;
        }

        if (uncompressedSizeBytes <= 0 || compressedSizeBytes == nullptr || compressedSizeBits == nullptr)
        {
            LZW_ERROR("lzw::easyEncode(): Bad in/out sizes!");
            return;
        }

        // LZW encoding context:
        int code = Nil;
        int codeBitsWidth = StartBits;
        Dictionary dictionary;

        // Output bit stream we write to. This will allocate
        // memory as needed to accommodate the encoded data.
        BitStreamWriter bitStream;

        for (; uncompressedSizeBytes > 0; --uncompressedSizeBytes, ++uncompressed)
        {
            const int value = *uncompressed;
            const int index = dictionary.findIndex(code, value);

            if (index != Nil)
            {
                code = index;
                continue;
            }

            // Write the dictionary code using the minimum bit-with:
            bitStream.appendBitsU64(code, codeBitsWidth);

            // Flush it when full so we can restart the sequences.
            if (!dictionary.flush(codeBitsWidth))
            {
                // There's still space for this sequence.
                dictionary.add(code, value);
            }
            code = value;
        }

        // Residual code at the end:
        if (code != Nil)
        {
            bitStream.appendBitsU64(code, codeBitsWidth);
        }

        // Pass ownership of the compressed data buffer to the user pointer:
        *compressedSizeBytes = bitStream.getByteCount();
        *compressedSizeBits = bitStream.getBitCount();
        *compressed = bitStream.release();
    }

    // ========================================================
    // easyDecode() and helpers:
    // ========================================================

    static bool outputByte(int code, std::uint8_t *&output, int outputSizeBytes, int &bytesDecodedSoFar)
    {
        if (bytesDecodedSoFar >= outputSizeBytes)
        {
            LZW_ERROR("Decoder output buffer too small!");
            return false;
        }

        assert(code >= 0 && code < 256);
        *output++ = static_cast<std::uint8_t>(code);
        ++bytesDecodedSoFar;
        return true;
    }

    static bool outputSequence(const Dictionary &dict, int code,
                               std::uint8_t *&output, int outputSizeBytes,
                               int &bytesDecodedSoFar, int &firstByte)
    {
        // A sequence is stored backwards, so we have to write
        // it to a temp then output the buffer in reverse.
        int i = 0;
        std::uint8_t sequence[MaxDictEntries];
        do
        {
            assert(i < MaxDictEntries - 1 && code >= 0);
            sequence[i++] = dict.entries[code].value;
            code = dict.entries[code].code;
        } while (code >= 0);

        firstByte = sequence[--i];
        for (; i >= 0; --i)
        {
            if (!outputByte(sequence[i], output, outputSizeBytes, bytesDecodedSoFar))
            {
                return false;
            }
        }
        return true;
    }

    int easyDecode(const std::uint8_t *compressed, const int compressedSizeBytes, const int compressedSizeBits,
                   std::uint8_t *uncompressed, const int uncompressedSizeBytes)
    {
        if (compressed == nullptr || uncompressed == nullptr)
        {
            LZW_ERROR("lzw::easyDecode(): Null data pointer(s)!");
            return 0;
        }

        if (compressedSizeBytes <= 0 || compressedSizeBits <= 0 || uncompressedSizeBytes <= 0)
        {
            LZW_ERROR("lzw::easyDecode(): Bad in/out sizes!");
            return 0;
        }

        int code = Nil;
        int prevCode = Nil;
        int firstByte = 0;
        int bytesDecoded = 0;
        int codeBitsWidth = StartBits;

        // We'll reconstruct the dictionary based on the
        // bit stream codes. Unlike Huffman encoding, we
        // don't store the dictionary as a prefix to the data.
        Dictionary dictionary;
        BitStreamReader bitStream(compressed, compressedSizeBytes, compressedSizeBits);

        // We check to avoid an overflow of the user buffer.
        // If the buffer is smaller than the decompressed size,
        // LZW_ERROR() is called. If that doesn't throw or
        // terminate we break the loop and return the current
        // decompression count.
        while (!bitStream.isEndOfStream())
        {
            assert(codeBitsWidth <= MaxDictBits);
            code = static_cast<int>(bitStream.readBitsU64(codeBitsWidth));

            if (prevCode == Nil)
            {
                if (!outputByte(code, uncompressed,
                                uncompressedSizeBytes, bytesDecoded))
                {
                    break;
                }
                firstByte = code;
                prevCode = code;
                continue;
            }

            if (code >= dictionary.size)
            {
                if (!outputSequence(dictionary, prevCode, uncompressed,
                                    uncompressedSizeBytes, bytesDecoded, firstByte))
                {
                    break;
                }
                if (!outputByte(firstByte, uncompressed,
                                uncompressedSizeBytes, bytesDecoded))
                {
                    break;
                }
            }
            else
            {
                if (!outputSequence(dictionary, code, uncompressed,
                                    uncompressedSizeBytes, bytesDecoded, firstByte))
                {
                    break;
                }
            }

            dictionary.add(prevCode, firstByte);
            if (dictionary.flush(codeBitsWidth))
            {
                prevCode = Nil;
            }
            else
            {
                prevCode = code;
            }
        }

        return bytesDecoded;
    }

} // namespace lzw {}

// ================ End of implementation =================
#endif // LZW_IMPLEMENTATION
// ================ End of implementation =================
