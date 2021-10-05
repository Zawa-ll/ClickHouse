#include "CachedCompressedReadBuffer.h"

#include <IO/WriteHelpers.h>
#include <Compression/LZ4_decompress_faster.h>

#include <utility>


namespace DB
{

namespace ErrorCodes
{
    extern const int SEEK_POSITION_OUT_OF_BOUND;
}


void CachedCompressedReadBuffer::initInput()
{
    if (!file_in)
    {
        file_in = file_in_creator();
        compressed_in = file_in.get();

        if (profile_callback)
            file_in->setProfileCallback(profile_callback, clock_type);
    }
}


bool CachedCompressedReadBuffer::nextImpl()
{
    /// Let's check for the presence of a decompressed block in the cache, grab the ownership of this block, if it exists.
    UInt128 key = cache->hash(path, file_pos);

    owned_cell = cache->getOrSet(key, [&]()
    {
        initInput();
        file_in->seek(file_pos, SEEK_SET);

        auto cell = std::make_shared<UncompressedCacheCell>();

        size_t size_decompressed;
        size_t size_compressed_without_checksum;
        cell->compressed_size = readCompressedData(size_decompressed, size_compressed_without_checksum, false);

        if (cell->compressed_size)
        {
            cell->additional_bytes = codec->getAdditionalSizeAtTheEndOfBuffer();
            cell->data.resize(size_decompressed + cell->additional_bytes);
            decompressTo(cell->data.data(), size_decompressed, size_compressed_without_checksum);
        }

        return cell;
    });

    if (owned_cell->data.size() == 0)
        return false;

    working_buffer = Buffer(owned_cell->data.data(), owned_cell->data.data() + owned_cell->data.size() - owned_cell->additional_bytes);

    /// nextimpl_working_buffer_offset is set in the seek function (lazy seek). So we have to
    /// check that we are not seeking beyond working buffer.
    if (nextimpl_working_buffer_offset > working_buffer.size())
        throw Exception("Seek position is beyond the decompressed block"
        " (pos: " + toString(nextimpl_working_buffer_offset) + ", block size: " + toString(working_buffer.size()) + ")",
        ErrorCodes::SEEK_POSITION_OUT_OF_BOUND);

    file_pos += owned_cell->compressed_size;

    return true;
}

CachedCompressedReadBuffer::CachedCompressedReadBuffer(
    const std::string & path_, std::function<std::unique_ptr<ReadBufferFromFileBase>()> file_in_creator_, UncompressedCache * cache_, bool allow_different_codecs_)
    : ReadBuffer(nullptr, 0), file_in_creator(std::move(file_in_creator_)), cache(cache_), path(path_), file_pos(0)
{
    allow_different_codecs = allow_different_codecs_;
}

void CachedCompressedReadBuffer::seek(size_t offset_in_compressed_file, size_t offset_in_decompressed_block)
{

    /// Nothing to do if we already at required position
    if (file_pos == offset_in_compressed_file
        && (offset() == offset_in_decompressed_block ||
            nextimpl_working_buffer_offset == offset_in_decompressed_block))
        return;

    if (owned_cell &&
        offset_in_compressed_file == file_pos - owned_cell->compressed_size &&
        offset_in_decompressed_block <= working_buffer.size())
    {
        pos = working_buffer.begin() + offset_in_decompressed_block;
    }
    else
    {

        LOG_INFO(&Poco::Logger::get("DEBUG"), "Adding offset {}", offset());
        /// Remember position in compressed file (will be moved in nextImpl)
        file_pos = offset_in_compressed_file;
        /// We will discard our working_buffer, but have to account rest bytes
        bytes += offset();
        /// No data, everything discarded
        pos = working_buffer.end();
        /// Remember required offset in decompressed block which will be set in
        /// the next ReadBuffer::next() call
        nextimpl_working_buffer_offset = offset_in_decompressed_block;
    }
}

}
