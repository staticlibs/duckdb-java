#include "bindings_table_function.hpp"
#include "refs.hpp"
#include "util.hpp"

static TableFunctionInitInfoHolder *init_info_buf_to_init_info(JNIEnv *env, jobject init_info_buf) {
	if (init_info_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid table function init info buffer");
		return nullptr;
	}
	auto init_info = reinterpret_cast<TableFunctionInitInfoHolder *>(env->GetDirectBufferAddress(init_info_buf));
	if (init_info == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid table function init info");
		return nullptr;
	}
	return init_info;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_init_get_bind_data
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1init_1get_1bind_1data(JNIEnv *env, jclass,
                                                                                       jobject init_info) {

	auto ii = init_info_buf_to_init_info(env, init_info);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		if (ii->global_input != nullptr) {
			return ii->global_input->GetBindData<TableFunctionState>().get_object();
		}
		return ii->local_input->GetBindData<TableFunctionState>().get_object();
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_init_set_init_data
 * Signature: (Ljava/nio/ByteBuffer;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1init_1set_1init_1data(JNIEnv *env, jclass,
                                                                                    jobject init_info,
                                                                                    jobject init_data) {
	if (init_data == nullptr) {
		return;
	}

	auto ii = init_info_buf_to_init_info(env, init_info);
	if (env->ExceptionCheck()) {
		return;
	}
	auto init_data_holder = std::unique_ptr<GlobalRefHolder>(new GlobalRefHolder(env, init_data));
	if (init_data_holder->vm == nullptr) {
		env->ThrowNew(J_SQLException, "Unable to create a global reference to the specified init data");
		return;
	}

	ii->init_data = std::unique_ptr<TableFunctionState>(new TableFunctionState(init_data_holder.release()));
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_init_get_column_count
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1init_1get_1column_1count(JNIEnv *env, jclass,
                                                                                        jobject init_info) {

	auto ii = init_info_buf_to_init_info(env, init_info);
	if (env->ExceptionCheck()) {
		return 0;
	}

	idx_t count = 0;
	try {
		count = ii->global_input != nullptr ? ii->global_input->GetColumnCount() : ii->local_input->GetColumnCount();
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return 0;
	}
	return uint64_to_jlong(count);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_init_get_column_index
 * Signature: (Ljava/nio/ByteBuffer;J)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1init_1get_1column_1index(JNIEnv *env, jclass,
                                                                                        jobject init_info,
                                                                                        jlong column_index) {

	auto ii = init_info_buf_to_init_info(env, init_info);
	if (env->ExceptionCheck()) {
		return 0;
	}

	idx_t column_index_idx = jlong_to_idx(env, column_index);
	if (env->ExceptionCheck()) {
		return 0;
	}

	idx_t idx = 0;
	try {
		// same as in C API v1, 0 is returned for an invalid index
		if (ii->global_input != nullptr) {
			if (column_index_idx < ii->global_input->GetColumnCount()) {
				idx = ii->global_input->GetColumnIndex(column_index_idx);
			}
		} else {
			if (column_index_idx < ii->local_input->GetColumnCount()) {
				idx = ii->local_input->GetColumnIndex(column_index_idx);
			}
		}
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return 0;
	}
	return uint64_to_jlong(idx);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_init_set_max_threads
 * Signature: (Ljava/nio/ByteBuffer;J)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1init_1set_1max_1threads(JNIEnv *env, jclass,
                                                                                      jobject init_info,
                                                                                      jlong max_threads) {

	auto ii = init_info_buf_to_init_info(env, init_info);
	if (env->ExceptionCheck()) {
		return;
	}
	idx_t mt = jlong_to_idx(env, max_threads);
	if (env->ExceptionCheck()) {
		return;
	}

	// the number of threads can only be set in the global init
	if (ii->global_input == nullptr) {
		return;
	}
	try {
		ii->global_input->SetMaxThreads(mt);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_init_set_error
 * Signature: (Ljava/nio/ByteBuffer;[B)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1init_1set_1error(JNIEnv *env, jclass, jobject init_info,
                                                                               jbyteArray error) {
	auto ii = init_info_buf_to_init_info(env, init_info);
	if (env->ExceptionCheck()) {
		return;
	}
	std::string error_str = jbyteArray_to_string(env, error);
	if (env->ExceptionCheck()) {
		return;
	}

	ii->has_error = true;
	ii->error = error_str;
}
