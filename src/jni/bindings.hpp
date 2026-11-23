#pragma once

#include "cxx_api.hpp"
#include "org_duckdb_DuckDBBindings.h"
#include "util.hpp"

cxx::LogicalType *logical_type_buf_to_logical_type(JNIEnv *env, jobject logical_type_buf);

jobject make_logical_type_buf(JNIEnv *env, cxx::LogicalType logical_type);

/**
 * Data chunk passed to Java. The C++ API derives the row count of a chunk from its vectors and does not allow
 * to create chunks without columns. So the inputs of the scalar functions without arguments are represented
 * with an empty chunk handle and an explicitly set row count.
 */
struct DataChunkHolder {
	explicit DataChunkHolder(cxx::DataChunk chunk_p) : chunk(std::move(chunk_p)) {
	}

	cxx::DataChunk chunk;
	bool has_row_count = false;
	idx_t row_count = 0;

	idx_t GetRowCount() const {
		return has_row_count ? row_count : chunk.GetRowCount();
	}

	idx_t GetColumnCount() const {
		return chunk ? chunk.GetVectorCount() : 0;
	}
};

DataChunkHolder *chunk_buf_to_chunk(JNIEnv *env, jobject chunk_buf);

jobject make_data_chunk_buf(JNIEnv *env, cxx::DataChunk chunk);

/**
 * Converts an exception thrown by the C++ API into a Java SQLException, unless a Java exception is already pending.
 * C++ exceptions must not escape from the JNI functions.
 */
void throw_sql_exception(JNIEnv *env, const std::exception &e);
