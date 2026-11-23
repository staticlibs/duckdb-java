#include "bindings.hpp"
#include "duckdb/common/error_data.hpp"
#include "duckdb/main/appender.hpp"
#include "holders.hpp"
#include "util.hpp"

/*
 * The appender of the C++ API (cxx::Appender) buffers the rows and flushes them by running an INSERT statement,
 * it does not support appending DEFAULT column values to a chunk (duckdb_append_default_to_chunk). So the
 * internal duckdb::Appender is used here, wrapped the same way as it is done in C API v1.
 */
struct AppenderHolder {
	duckdb::unique_ptr<duckdb::Appender> appender;
	duckdb::ErrorData error_data;
	bool flush_failed = false;
};

template <class FUN>
static int32_t appender_run_function(AppenderHolder *holder, FUN &&function) {
	if (!holder->appender) {
		holder->error_data = duckdb::ErrorData("not a valid appender");
		return CAPI_STATE_ERROR;
	}
	try {
		function(*holder->appender);
	} catch (std::exception &ex) {
		holder->error_data = duckdb::ErrorData(ex);
		return CAPI_STATE_ERROR;
	} catch (...) {
		holder->error_data = duckdb::ErrorData("Unknown appender error");
		return CAPI_STATE_ERROR;
	}
	return CAPI_STATE_SUCCESS;
}

static int32_t appender_close(AppenderHolder *holder) {
	auto state = appender_run_function(holder, [&](duckdb::Appender &appender) { appender.Close(); });
	if (state == CAPI_STATE_ERROR) {
		holder->flush_failed = true;
	}
	return state;
}

/*
 * Data chunks are passed to Java as cxx::DataChunk wrappers, the engine data chunk is the underlying handle.
 */
static duckdb::DataChunk &chunk_to_engine_chunk(DataChunkHolder &chunk) {
	auto handle = reinterpret_cast<duckdb_v2_data_chunk_handle>(cxx_raw_handle(chunk.chunk));
	return *duckdb::capiv2::Convert(handle);
}

static AppenderHolder *appender_buf_to_appender(JNIEnv *env, jobject appender_buf) {

	if (appender_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid appender buffer");
		return nullptr;
	}

	auto appender = reinterpret_cast<AppenderHolder *>(env->GetDirectBufferAddress(appender_buf));
	if (appender == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid appender");
		return nullptr;
	}

	return appender;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_appender_create_ext
 * Signature: (Ljava/nio/ByteBuffer;[B[B[B[Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1appender_1create_1ext(JNIEnv *env, jclass,
                                                                                    jobject connection,
                                                                                    jbyteArray catalog,
                                                                                    jbyteArray schema, jbyteArray table,
                                                                                    jobjectArray out_appender) {

	duckdb::Connection *conn = conn_ref_buf_to_engine_conn(env, connection);
	if (env->ExceptionCheck()) {
		return -1;
	}

	std::string catalog_str = jbyteArray_to_string(env, catalog);
	if (env->ExceptionCheck()) {
		return -1;
	}
	const char *catalog_ptr = nullptr != catalog ? catalog_str.c_str() : INVALID_CATALOG;

	std::string schema_str = jbyteArray_to_string(env, schema);
	if (env->ExceptionCheck()) {
		return -1;
	}
	const char *schema_ptr = nullptr != schema ? schema_str.c_str() : DEFAULT_SCHEMA;

	std::string table_str = jbyteArray_to_string(env, table);
	if (env->ExceptionCheck()) {
		return -1;
	}
	if (nullptr == table) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}
	const char *table_ptr = table_str.c_str();

	check_out_param(env, out_appender);
	if (env->ExceptionCheck()) {
		return -1;
	}

	auto appender = duckdb::unique_ptr<AppenderHolder>(new AppenderHolder());
	try {
		appender->appender = duckdb::make_uniq<duckdb::Appender>(
		    *conn, duckdb::Identifier(catalog_ptr), duckdb::Identifier(schema_ptr), duckdb::Identifier(table_ptr));
	} catch (std::exception &) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	} catch (...) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}

	jobject appender_ref_buf = env->NewDirectByteBuffer(appender.release(), 0);
	set_out_param(env, out_appender, appender_ref_buf);

	return static_cast<jint>(CAPI_STATE_SUCCESS);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_appender_error
 * Signature: (Ljava/nio/ByteBuffer;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1appender_1error(JNIEnv *env, jclass,
                                                                                    jobject appender) {

	auto app = appender_buf_to_appender(env, appender);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	if (!app->error_data.HasError()) {
		return nullptr;
	}
	const char *error_msg = app->error_data.RawMessage().c_str();

	idx_t len = static_cast<idx_t>(std::strlen(error_msg));

	return make_jbyteArray(env, error_msg, len);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_appender_flush
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1appender_1flush(JNIEnv *env, jclass, jobject appender) {

	auto app = appender_buf_to_appender(env, appender);
	if (env->ExceptionCheck()) {
		return -1;
	}

	auto state = appender_run_function(app, [&](duckdb::Appender &appender) { appender.Flush(); });
	if (state == CAPI_STATE_ERROR) {
		app->flush_failed = true;
	}

	return static_cast<jint>(state);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_appender_close
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1appender_1close(JNIEnv *env, jclass, jobject appender) {

	auto app = appender_buf_to_appender(env, appender);
	if (env->ExceptionCheck()) {
		return -1;
	}

	auto state = appender_close(app);

	return static_cast<jint>(state);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_appender_destroy
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1appender_1destroy(JNIEnv *env, jclass, jobject appender) {

	auto app = appender_buf_to_appender(env, appender);
	if (env->ExceptionCheck()) {
		return -1;
	}

	auto state = appender_close(app);
	// If flush/close previously failed, preserve that error even if close just succeeded with empty collection.
	if (app->flush_failed) {
		state = CAPI_STATE_ERROR;
	}
	delete app;

	return static_cast<jint>(state);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_appender_column_count
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1appender_1column_1count(JNIEnv *env, jclass,
                                                                                       jobject appender) {

	auto app = appender_buf_to_appender(env, appender);
	if (env->ExceptionCheck()) {
		return -1;
	}

	if (!app->appender) {
		return 0;
	}
	idx_t count = app->appender->GetActiveTypes().size();

	return static_cast<jlong>(count);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_appender_column_type
 * Signature: (Ljava/nio/ByteBuffer;J)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1appender_1column_1type(JNIEnv *env, jclass,
                                                                                        jobject appender,
                                                                                        jlong col_idx) {

	auto app = appender_buf_to_appender(env, appender);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	idx_t idx = jlong_to_idx(env, col_idx);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	if (!app->appender || idx >= app->appender->GetActiveTypes().size()) {
		return nullptr;
	}

	// the engine type is passed to Java as an owned logical type of the C++ API
	auto &logical_type = app->appender->GetActiveTypes()[idx];
	auto lt = cxx_wrap<cxx::LogicalType>(duckdb::capiv2::Convert(logical_type.Copy()));

	return make_logical_type_buf(env, std::move(lt));
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_append_data_chunk
 * Signature: (Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1append_1data_1chunk(JNIEnv *env, jclass, jobject appender,
                                                                                  jobject chunk) {

	auto app = appender_buf_to_appender(env, appender);
	if (env->ExceptionCheck()) {
		return -1;
	}

	auto dc = chunk_buf_to_chunk(env, chunk);
	if (env->ExceptionCheck()) {
		return -1;
	}

	auto &data_chunk = chunk_to_engine_chunk(*dc);
	auto state = appender_run_function(app, [&](duckdb::Appender &appender) { appender.AppendDataChunk(data_chunk); });

	return static_cast<jint>(state);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_append_default_to_chunk
 * Signature: (Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;JJ)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1append_1default_1to_1chunk(JNIEnv *env, jclass,
                                                                                         jobject appender,
                                                                                         jobject chunk, jlong col,
                                                                                         jlong row) {

	auto app = appender_buf_to_appender(env, appender);
	if (env->ExceptionCheck()) {
		return -1;
	}
	auto dc = chunk_buf_to_chunk(env, chunk);
	if (env->ExceptionCheck()) {
		return -1;
	}
	idx_t col_idx = jlong_to_idx(env, col);
	if (env->ExceptionCheck()) {
		return -1;
	}
	idx_t row_idx = jlong_to_idx(env, row);
	if (env->ExceptionCheck()) {
		return -1;
	}

	if (!app->appender) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}
	auto &data_chunk = chunk_to_engine_chunk(*dc);
	try {
		app->appender->AppendDefault(data_chunk, col_idx, row_idx);
	} catch (std::exception &ex) {
		app->error_data = duckdb::ErrorData(ex);
		return static_cast<jint>(CAPI_STATE_ERROR);
	} catch (...) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}

	return static_cast<jint>(CAPI_STATE_SUCCESS);
}
