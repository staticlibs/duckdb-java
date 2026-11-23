#include "bindings.hpp"
#include "refs.hpp"
#include "util.hpp"

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

/*
 * Vectors are passed to Java as the underlying handles of the (non-owning) cxx::Vector wrappers.
 */
static void *vector_buf_to_vector(JNIEnv *env, jobject vector_buf) {

	if (vector_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid vector buffer");
		return nullptr;
	}

	void *vector = env->GetDirectBufferAddress(vector_buf);

	if (vector == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid vector");
		return nullptr;
	}

	return vector;
}

static jobject make_vector_buf(JNIEnv *env, const cxx::Vector &vector) {
	return make_ptr_buf(env, cxx_raw_handle(vector));
}

/*
 * In the C++ API vectors are always borrowed from data chunks, standalone vectors created with
 * duckdb_create_vector are backed by single-column data chunks that are kept here until the vector is destroyed.
 */
static std::mutex standalone_vectors_lock;
static std::unordered_map<void *, std::unique_ptr<cxx::DataChunk>> standalone_vectors;

/*
 * Same as in C API v1, returns the data of FLAT and CONSTANT vectors, nullptr otherwise.
 */
static void *vector_get_data(void *vec) {
	try {
		return cxx_wrap<cxx::Vector>(vec).GetDataMutable();
	} catch (const std::exception &) {
		return nullptr;
	}
}

/*
 * Same as in C API v1, returns the validity mask of FLAT and CONSTANT vectors, the mask can be nullptr if all rows are
 * valid. Returns nullptr for other vector types.
 */
static uint64_t *vector_get_validity(void *vec) {
	try {
		auto vector = cxx_wrap<cxx::Vector>(vec);
		switch (vector.GetVectorType()) {
		case cxx::VectorType::FLAT:
		case cxx::VectorType::CONSTANT:
			return const_cast<uint64_t *>(vector.GetView().validity);
		default:
			return nullptr;
		}
	} catch (const std::exception &) {
		return nullptr;
	}
}

static void vector_ensure_validity_writable(void *vec) {
	cxx_wrap<cxx::Vector>(vec).GetValidityMutable();
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_vector_get_string
 * Signature: (Ljava/nio/ByteBuffer;J)[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1vector_1get_1string(JNIEnv *env, jclass,
                                                                                        jobject vector_data,
                                                                                        jlong row) {

	if (vector_data == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid vector data buffer");
		return nullptr;
	}
	auto data = reinterpret_cast<cxx::varchar_t *>(env->GetDirectBufferAddress(vector_data));
	if (data == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid vector data");
		return nullptr;
	}
	idx_t row_idx = jlong_to_idx(env, row);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	auto &string_value = data[row_idx];
	auto string_len = string_value.size();
	auto string_ptr = string_value.data();
	return make_jbyteArray(env, string_ptr, string_len);
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_org_duckdb_DuckDBBindings_duckdb_1vector_1get_1string__Ljava_nio_ByteBuffer_2J(JNIEnv *env, jclass clazz,
                                                                                    jobject vector_data, jlong row) {
	return Java_org_duckdb_DuckDBBindings_duckdb_1vector_1get_1string(env, clazz, vector_data, row);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_create_vector
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1create_1vector(JNIEnv *env, jclass,
                                                                                jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		if (!*lt) {
			return nullptr;
		}
		std::vector<cxx::LogicalType> types;
		types.emplace_back(lt->Copy());
		// the chunk is created with the standard vector size capacity
		auto chunk = std::unique_ptr<cxx::DataChunk>(new cxx::DataChunk(default_factory(), types));
		void *vec = cxx_raw_handle(chunk->GetVector(0));
		std::lock_guard<std::mutex> guard(standalone_vectors_lock);
		standalone_vectors.emplace(vec, std::move(chunk));
		return make_ptr_buf(env, vec);
	} catch (const std::exception &) {
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_destroy_vector
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1destroy_1vector(JNIEnv *env, jclass, jobject vector) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return;
	}

	std::unique_ptr<cxx::DataChunk> chunk;
	{
		std::lock_guard<std::mutex> guard(standalone_vectors_lock);
		auto it = standalone_vectors.find(vec);
		if (it == standalone_vectors.end()) {
			return;
		}
		chunk = std::move(it->second);
		standalone_vectors.erase(it);
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_vector_get_column_type
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1vector_1get_1column_1type(JNIEnv *env, jclass,
                                                                                           jobject vector) {

	void *vec_handle = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		auto vec = cxx_wrap<cxx::Vector>(vec_handle);
		return make_logical_type_buf(env, vec.GetLogicalType());
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_vector_get_data
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1vector_1get_1data(JNIEnv *env, jclass, jobject vector,
                                                                                   jlong size_bytes) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	idx_t vector_size = jlong_to_idx(env, size_bytes);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	void *data = vector_get_data(vec);

	if (data != nullptr) {
		return make_data_buf(env, data, vector_size);
	}

	return nullptr;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_vector_get_data_zeroed
 * Signature: (Ljava/nio/ByteBuffer;J)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1vector_1get_1data_1zeroed(JNIEnv *env, jclass,
                                                                                           jobject vector,
                                                                                           jlong size_bytes) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	idx_t vector_size = jlong_to_idx(env, size_bytes);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	void *data = vector_get_data(vec);

	if (data == nullptr) {
		return nullptr;
	}

	std::memset(data, '\0', vector_size);

	return make_data_buf(env, data, vector_size);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_vector_get_validity
 * Signature: (Ljava/nio/ByteBuffer;J)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1vector_1get_1validity(JNIEnv *env, jclass,
                                                                                       jobject vector,
                                                                                       jlong vector_size_elems) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	idx_t vector_size = jlong_to_idx(env, vector_size_elems);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	uint64_t *mask = vector_get_validity(vec);
	idx_t vector_size_rounded = vector_size;
	if (vector_size % 64 != 0) {
		vector_size_rounded += 64 - (vector_size % 64);
	}
	idx_t mask_len = vector_size_rounded * sizeof(uint64_t) / 64;

	return make_data_buf(env, mask, mask_len);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_vector_ensure_validity_writable
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1vector_1ensure_1validity_1writable(JNIEnv *env, jclass,
                                                                                                 jobject vector) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return;
	}

	try {
		vector_ensure_validity_writable(vec);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_vector_assign_string_element_len
 * Signature: (Ljava/nio/ByteBuffer;J[B)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1vector_1assign_1string_1element_1len(JNIEnv *env, jclass,
                                                                                                   jobject vector,
                                                                                                   jlong index,
                                                                                                   jbyteArray str) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return;
	}
	idx_t idx = jlong_to_idx(env, index);
	if (env->ExceptionCheck()) {
		return;
	}
	auto str_ptr = make_jbyteArray_ptr(env, str);
	if (env->ExceptionCheck()) {
		return;
	}

	idx_t len = static_cast<idx_t>(env->GetArrayLength(str));

	try {
		auto vector = cxx_wrap<cxx::Vector>(vec);
		std::string_view data(str_ptr.get(), len);
		// same as in C API v1, invalid UTF-8 strings in VARCHAR vectors are set to NULL
		if (vector.GetLogicalType().GetTypeId() == cxx::LogicalTypeId::VARCHAR) {
			try {
				cxx::ValidateUTF8(data);
			} catch (const cxx::InvalidInputException &) {
				vector.GetValidityMutable().SetInvalid(idx);
				return;
			}
		}
		vector.AssignStringUnsafe(idx, data);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_vector_assign_string_elements
 * Signature: (Ljava/nio/ByteBuffer;J[B[JI)V
 *
 * Batch variant of duckdb_vector_assign_string_element_len: assigns count
 * concatenated UTF-8 strings starting at row index, in a single JNI call.
 * lengths[i] >= 0 marks the byte length of element i, a negative length marks
 * a NULL element. The complete payload is validated once before the loop uses
 * the unsafe assignment primitive for each valid element.
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1vector_1assign_1string_1elements(
    JNIEnv *env, jclass, jobject vector, jlong index, jbyteArray data, jlongArray lengths, jint data_length) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return;
	}
	auto payload = make_jbyteArray_ptr(env, data);
	if (env->ExceptionCheck()) {
		return;
	}
	if (lengths == nullptr || data_length < 0 || data_length > env->GetArrayLength(data)) {
		env->ThrowNew(J_SQLException, "Invalid string batch length");
		return;
	}
	jint count = env->GetArrayLength(lengths);
	idx_t row_idx = jlong_to_idx(env, index);
	if (env->ExceptionCheck()) {
		return;
	}
	jlong *lens = env->GetLongArrayElements(lengths, nullptr);
	if (lens == nullptr) {
		return;
	}
	jlong payload_length = 0;
	for (jint i = 0; i < count; i++) {
		jlong len = lens[i];
		if (len < 0 || len <= data_length - payload_length) {
			if (len >= 0) {
				payload_length += len;
			}
			continue;
		}
		env->ReleaseLongArrayElements(lengths, lens, JNI_ABORT);
		env->ThrowNew(J_SQLException, "Invalid string batch lengths");
		return;
	}
	if (payload_length != data_length) {
		env->ReleaseLongArrayElements(lengths, lens, JNI_ABORT);
		env->ThrowNew(J_SQLException, "String batch length does not match payload");
		return;
	}
	try {
		cxx::ValidateUTF8(std::string_view(payload.get(), static_cast<idx_t>(payload_length)));
	} catch (const cxx::InvalidInputException &) {
		env->ReleaseLongArrayElements(lengths, lens, JNI_ABORT);
		env->ThrowNew(J_SQLException, "Invalid UTF-8 string batch");
		return;
	}
	try {
		auto vector = cxx_wrap<cxx::Vector>(vec);
		cxx::ValidityMask validity = vector.GetValidityMutable();
		if (validity.words == nullptr) {
			env->ReleaseLongArrayElements(lengths, lens, JNI_ABORT);
			env->ThrowNew(J_SQLException, "Invalid validity mask");
			return;
		}
		auto data = vector.GetDataMutable<cxx::varchar_t>();
		auto heap = vector.GetHeap();
		const char *p = payload.get();
		for (jint i = 0; i < count; i++) {
			jlong len = lens[i];
			idx_t row = row_idx + static_cast<idx_t>(i);
			if (len < 0) {
				validity.SetInvalid(row);
				continue;
			}
			auto bytes = heap.AddBlob(std::string_view(p, static_cast<idx_t>(len)));
			data[row] = cxx::varchar_t(bytes.data(), bytes.size());
			p += len;
			validity.SetValid(row);
		}
	} catch (const std::exception &e) {
		env->ReleaseLongArrayElements(lengths, lens, JNI_ABORT);
		throw_sql_exception(env, e);
		return;
	}
	env->ReleaseLongArrayElements(lengths, lens, JNI_ABORT);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_list_vector_get_child
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1list_1vector_1get_1child(JNIEnv *env, jclass,
                                                                                          jobject vector) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		return make_vector_buf(env, cxx_wrap<cxx::Vector>(vec).GetChild(0));
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_list_vector_get_size
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1list_1vector_1get_1size(JNIEnv *env, jclass,
                                                                                       jobject vector) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		// the size of a list is the size of its child vector
		idx_t size = cxx_wrap<cxx::Vector>(vec).GetChild(0).GetSize();
		return uint64_to_jlong(size);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_list_vector_set_size
 * Signature: (Ljava/nio/ByteBuffer;J)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1list_1vector_1set_1size(JNIEnv *env, jclass,
                                                                                      jobject vector, jlong size) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return -1;
	}
	idx_t size_idx = jlong_to_idx(env, size);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		// sets the size of the child vector, reserving the space for it if necessary
		cxx_wrap<cxx::Vector>(vec).GetChild(0).SetSize(size_idx);
		return static_cast<jint>(CAPI_STATE_SUCCESS);
	} catch (const std::exception &) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_list_vector_reserve
 * Signature: (Ljava/nio/ByteBuffer;J)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1list_1vector_1reserve(JNIEnv *env, jclass, jobject vector,
                                                                                    jlong capacity) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return -1;
	}
	idx_t cap = jlong_to_idx(env, capacity);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		// The C++ API does not have a reserve-only call, the child vector is resized to the requested capacity,
		// that reserves the space for it, and then its size is restored
		auto child = cxx_wrap<cxx::Vector>(vec).GetChild(0);
		idx_t size = child.GetSize();
		if (cap > size) {
			child.SetSize(cap);
			child.SetSize(size);
		}
		return static_cast<jint>(CAPI_STATE_SUCCESS);
	} catch (const std::exception &) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_struct_vector_get_child
 * Signature: (Ljava/nio/ByteBuffer;J)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1struct_1vector_1get_1child(JNIEnv *env, jclass,
                                                                                            jobject vector,
                                                                                            jlong index) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	idx_t idx = jlong_to_idx(env, index);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		return make_vector_buf(env, cxx_wrap<cxx::Vector>(vec).GetChild(idx));
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_array_vector_get_child
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1array_1vector_1get_1child(JNIEnv *env, jclass,
                                                                                           jobject vector) {

	void *vec = vector_buf_to_vector(env, vector);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		return make_vector_buf(env, cxx_wrap<cxx::Vector>(vec).GetChild(0));
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}
