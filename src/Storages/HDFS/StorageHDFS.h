#pragma once

#if !defined(ARCADIA_BUILD)
#include <Common/config.h>
#endif

#if USE_HDFS

#include <Storages/IStorage.h>
#include <Poco/URI.h>
#include <base/logger_useful.h>
#include <base/shared_ptr_helper.h>

namespace DB
{
/**
 * This class represents table engine for external hdfs files.
 * Read method is supported for now.
 */
class StorageHDFS final : public shared_ptr_helper<StorageHDFS>, public IStorage, WithContext
{
    friend struct shared_ptr_helper<StorageHDFS>;
public:
    String getName() const override { return "HDFS"; }

    Pipe read(
        const Names & column_names,
        const StorageMetadataPtr & /*metadata_snapshot*/,
        SelectQueryInfo & query_info,
        ContextPtr context,
        QueryProcessingStage::Enum processed_stage,
        size_t max_block_size,
        unsigned num_streams) override;

    SinkToStoragePtr write(const ASTPtr & query, const StorageMetadataPtr & /*metadata_snapshot*/, ContextPtr context) override;

    void truncate(const ASTPtr & query, const StorageMetadataPtr & metadata_snapshot, ContextPtr context_, TableExclusiveLockHolder &) override;

    NamesAndTypesList getVirtuals() const override;

    bool supportsPartitionBy() const override { return true; }

protected:
    StorageHDFS(
        const String & uri_,
        const StorageID & table_id_,
        const String & format_name_,
        const ColumnsDescription & columns_,
        const ConstraintsDescription & constraints_,
        const String & comment,
        ContextPtr context_,
        const String & compression_method_,
        ASTPtr partition_by = nullptr);

private:
    const String uri;
    String format_name;
    String compression_method;
    ASTPtr partition_by;

    Poco::Logger * log = &Poco::Logger::get("StorageHDFS");
};
}

#endif
