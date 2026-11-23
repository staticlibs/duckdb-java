#include "bindings.hpp"
#include "refs.hpp"
#include "util.hpp"

#include <vector>

DataChunkHolder *chunk_buf_to_chunk(JNIEnv *env, jobject chunk_buf) {

	if (chunk_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid data chunk buffer");
		return nullptr;
	}

	auto chunk = reinterpret_cast<DataChunkHolder *>(env->GetDirectBufferAddress(chunk_buf));

	if (chunk == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid data chunk");
		return nullptr;
	}

	return chunk;
}

jobject make_data_chunk_buf(JNIEnv *env, cxx::DataChunk chunk) {
	if (!chunk) {
		return nullptr;
	}
	auto dc = new DataChunkHolder(std::move(chunk));
	jobject buf = make_ptr_buf(env, dc);
	if (buf == nullptr) {
		delete dc;
	}
	return buf;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_create_data_chunk
 * Signature: ([Ljava/nio/ByteBuffer;)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1create_1data_1chunk(JNIEnv *env, jclass,
                                                                                     jobjectArray logical_types) {

	if (logical_types == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid logical type array");
		return nullptr;
	}

	size_t column_count = static_cast<size_t>(env->GetArrayLength(logical_types));

	std::vector<cxx::LogicalType *> lt_vec;
	lt_vec.reserve(column_count);

	for (size_t i = 0; i < column_count; i++) {

		jobject lt_buf = env->GetObjectArrayElement(logical_types, i);
		if (env->ExceptionCheck()) {
			return nullptr;
		}

		auto lt = logical_type_buf_to_logical_type(env, lt_buf);
		env->DeleteLocalRef(lt_buf);
		if (env->ExceptionCheck()) {
			return nullptr;
		}

		lt_vec.push_back(lt);
	}

	try {
		std::vector<cxx::LogicalType> types;
		types.reserve(column_count);
		for (auto lt : lt_vec) {
			// same as in C API v1, INVALID types (represented with empty handles) are rejected,
			// ANY types are rejected when the chunk is created
			if (!*lt) {
				return nullptr;
			}
			types.emplace_back(lt->Copy());
		}
		cxx::DataChunk data_chunk(default_factory(), types);
		return make_data_chunk_buf(env, std::move(data_chunk));
	} catch (const std::exception &) {
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_destroy_data_chunk
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1destroy_1data_1chunk(JNIEnv *env, jclass, jobject chunk) {

	auto dc = chunk_buf_to_chunk(env, chunk);
	if (env->ExceptionCheck()) {
		return;
	}

	delete dc;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_data_chunk_reset
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1data_1chunk_1reset(JNIEnv *env, jclass, jobject chunk) {

	auto dc = chunk_buf_to_chunk(env, chunk);
	if (env->ExceptionCheck()) {
		return;
	}

	try {
		if (dc->chunk) {
			dc->chunk.Clear();
		}
		if (dc->has_row_count) {
			dc->row_count = 0;
		}
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_data_chunk_get_column_count
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1data_1chunk_1get_1column_1count(JNIEnv *env, jclass,
                                                                                               jobject chunk) {

	auto dc = chunk_buf_to_chunk(env, chunk);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		idx_t count = dc->GetColumnCount();
		return static_cast<jlong>(count);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_data_chunk_get_vector
 * Signature: (Ljava/nio/ByteBuffer;J)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1data_1chunk_1get_1vector(JNIEnv *env, jclass,
                                                                                          jobject chunk,
                                                                                          jlong col_idx) {

	auto dc = chunk_buf_to_chunk(env, chunk);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	idx_t idx = jlong_to_idx(env, col_idx);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		if (idx >= dc->GetColumnCount()) {
			return nullptr;
		}
		cxx::Vector vec = dc->chunk.GetVector(idx);
		// vectors are borrowed from their chunk, the underlying handle is passed to Java
		return make_ptr_buf(env, cxx_raw_handle(vec));
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_data_chunk_get_size
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1data_1chunk_1get_1size(JNIEnv *env, jclass,
                                                                                      jobject chunk) {

	auto dc = chunk_buf_to_chunk(env, chunk);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		idx_t dc_size = dc->GetRowCount();
		return static_cast<jlong>(dc_size);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_data_chunk_set_size
 * Signature: (Ljava/nio/ByteBuffer;J)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1data_1chunk_1set_1size(JNIEnv *env, jclass, jobject chunk,
                                                                                     jlong size) {

	auto dc = chunk_buf_to_chunk(env, chunk);
	if (env->ExceptionCheck()) {
		return;
	}

	try {
		// The C++ API derives the row count of a chunk from its vectors, so the size is set on every vector
		idx_t count = dc->GetColumnCount();
		for (idx_t i = 0; i < count; i++) {
			dc->chunk.GetVector(i).SetSize(static_cast<idx_t>(size));
		}
		if (dc->has_row_count) {
			dc->row_count = static_cast<idx_t>(size);
		}
	} catch (const std::exception &) {
		// same as in C API v1, errors are not reported here
	}
}
