#include "bindings.hpp"
#include "refs.hpp"
#include "util.hpp"

#include <cstring>
#include <vector>

void throw_sql_exception(JNIEnv *env, const std::exception &e) {
	if (!env->ExceptionCheck()) {
		env->ThrowNew(J_SQLException, e.what());
	}
}

cxx::LogicalType *logical_type_buf_to_logical_type(JNIEnv *env, jobject logical_type_buf) {

	if (logical_type_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid logical type buffer");
		return nullptr;
	}

	auto logical_type = reinterpret_cast<cxx::LogicalType *>(env->GetDirectBufferAddress(logical_type_buf));
	if (logical_type == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid logical type");
		return nullptr;
	}

	return logical_type;
}

jobject make_logical_type_buf(JNIEnv *env, cxx::LogicalType logical_type) {
	auto lt = new cxx::LogicalType(std::move(logical_type));
	jobject buf = make_ptr_buf(env, lt);
	if (buf == nullptr) {
		delete lt;
	}
	return buf;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_create_logical_type
 * Signature: (I)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1create_1logical_1type(JNIEnv *env, jclass, jint type) {

	// same as in C API v1, types that require parameters cannot be created from type ID alone and
	// are reported as INVALID, INVALID type is represented with an empty handle
	cxx::LogicalTypeId type_id = cxx::LogicalTypeId::INVALID;
	if (!type_id_from_capi(static_cast<int32_t>(type), type_id)) {
		return make_logical_type_buf(env, empty_logical_type());
	}
	switch (type_id) {
	case cxx::LogicalTypeId::DECIMAL:
	case cxx::LogicalTypeId::ENUM:
	case cxx::LogicalTypeId::LIST:
	case cxx::LogicalTypeId::STRUCT:
	case cxx::LogicalTypeId::MAP:
	case cxx::LogicalTypeId::ARRAY:
	case cxx::LogicalTypeId::UNION:
		return make_logical_type_buf(env, empty_logical_type());
	default:
		break;
	}

	try {
		auto &factory = default_factory();
		if (type_id == cxx::LogicalTypeId::VARIANT || type_id == cxx::LogicalTypeId::GEOMETRY) {
			// these types cannot be created from type ID without parameters, their default variants
			// are created from their names
			auto name = type_id == cxx::LogicalTypeId::VARIANT ? "VARIANT" : "GEOMETRY";
			return make_logical_type_buf(env, factory.ParseType(name));
		}
		cxx::LogicalType lt = factory.CreateType(type_id);
		return make_logical_type_buf(env, std::move(lt));
	} catch (const std::exception &) {
		return make_logical_type_buf(env, empty_logical_type());
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_create_decimal_type
 * Signature: (II)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1create_1decimal_1type(JNIEnv *env, jclass, jint width,
                                                                                       jint scale) {

	if (width < 1 || width > 38) {
		env->ThrowNew(J_SQLException, "Invalid decimal width: expected 1..38");
		return nullptr;
	}
	if (scale < 0 || scale > width) {
		env->ThrowNew(J_SQLException, "Invalid decimal scale: expected 0..width");
		return nullptr;
	}
	try {
		auto &factory = default_factory();
		std::vector<cxx::TypeParam> params;
		params.emplace_back(factory.CreateValue(static_cast<uint8_t>(width)));
		params.emplace_back(factory.CreateValue(static_cast<uint8_t>(scale)));
		cxx::LogicalType lt = factory.CreateType(cxx::LogicalTypeId::DECIMAL, params);
		return make_logical_type_buf(env, std::move(lt));
	} catch (const std::exception &) {
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_type_id
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1type_1id(JNIEnv *env, jclass, jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return 1;
	}

	try {
		return static_cast<jint>(logical_type_to_capi(*lt));
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return 1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_decimal_width
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1decimal_1width(JNIEnv *env, jclass,
                                                                             jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		if (logical_type_to_capi(*lt) != capi_type::DECIMAL) {
			return 0;
		}
		uint32_t width = lt->GetDecimalWidth();
		// same as in C API v1, wider decimals report 0
		return width > UINT8_MAX ? 0 : static_cast<jint>(width);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_decimal_scale
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1decimal_1scale(JNIEnv *env, jclass,
                                                                             jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		if (logical_type_to_capi(*lt) != capi_type::DECIMAL) {
			return 0;
		}
		if (lt->GetDecimalWidth() > UINT8_MAX) {
			return 0;
		}
		uint32_t scale = lt->GetDecimalScale();
		return scale > UINT8_MAX ? 0 : static_cast<jint>(scale);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_decimal_internal_type
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1decimal_1internal_1type(JNIEnv *env, jclass,
                                                                                      jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		if (logical_type_to_capi(*lt) != capi_type::DECIMAL) {
			return static_cast<jint>(capi_type::INVALID);
		}
		switch (lt->GetDecimalInternalTypeId()) {
		case cxx::LogicalTypeId::SMALLINT:
		case cxx::LogicalTypeId::INTEGER:
		case cxx::LogicalTypeId::BIGINT:
		case cxx::LogicalTypeId::HUGEINT:
		case cxx::LogicalTypeId::BIGNUM:
			return static_cast<jint>(type_id_to_capi(lt->GetDecimalInternalTypeId()));
		default:
			return static_cast<jint>(capi_type::INVALID);
		}
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_create_list_type
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1create_1list_1type(JNIEnv *env, jclass,
                                                                                    jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		if (!*lt) {
			return nullptr;
		}
		auto &factory = default_factory();
		std::vector<cxx::TypeParam> params;
		params.emplace_back(factory.CreateValue(*lt));
		cxx::LogicalType list_type = factory.CreateType(cxx::LogicalTypeId::LIST, params);
		return make_logical_type_buf(env, std::move(list_type));
	} catch (const std::exception &) {
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_create_array_type
 * Signature: (Ljava/nio/ByteBuffer;J)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1create_1array_1type(JNIEnv *env, jclass,
                                                                                     jobject logical_type,
                                                                                     jlong array_size) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	idx_t size_idx = jlong_to_idx(env, array_size);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		if (!*lt) {
			return nullptr;
		}
		auto &factory = default_factory();
		std::vector<cxx::TypeParam> params;
		params.emplace_back(factory.CreateValue(*lt));
		params.emplace_back(factory.CreateValue(static_cast<uint64_t>(size_idx)));
		cxx::LogicalType array_type = factory.CreateType(cxx::LogicalTypeId::ARRAY, params);
		return make_logical_type_buf(env, std::move(array_type));
	} catch (const std::exception &) {
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_create_struct_type
 * Signature: ([Ljava/nio/ByteBuffer;[[B)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1create_1struct_1type(JNIEnv *env, jclass,
                                                                                      jobjectArray member_types,
                                                                                      jobjectArray member_names) {

	if (member_types == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid member types array");
		return nullptr;
	}
	if (member_names == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid member names array");
		return nullptr;
	}

	size_t member_count = static_cast<size_t>(env->GetArrayLength(member_types));
	size_t names_count = static_cast<size_t>(env->GetArrayLength(member_names));
	if (member_count != names_count) {
		env->ThrowNew(J_SQLException, "Invalid member names array size");
		return nullptr;
	}

	std::vector<cxx::LogicalType *> mt_vec;
	mt_vec.reserve(member_count);

	for (size_t i = 0; i < member_count; i++) {
		jobject lt_buf = env->GetObjectArrayElement(member_types, i);
		if (env->ExceptionCheck()) {
			return nullptr;
		}
		if (nullptr == lt_buf) {
			env->ThrowNew(J_SQLException, "Invalid null type specified");
			return nullptr;
		}
		auto lt = logical_type_buf_to_logical_type(env, lt_buf);
		env->DeleteLocalRef(lt_buf);
		if (env->ExceptionCheck()) {
			return nullptr;
		}
		mt_vec.push_back(lt);
	}

	std::vector<std::string> names_vec;
	names_vec.reserve(member_count);

	for (size_t i = 0; i < member_count; i++) {
		jbyteArray jba = reinterpret_cast<jbyteArray>(env->GetObjectArrayElement(member_names, i));
		if (env->ExceptionCheck()) {
			return nullptr;
		}
		if (nullptr == jba) {
			env->ThrowNew(J_SQLException, "Invalid null name specified");
			return nullptr;
		}
		std::string str = jbyteArray_to_string(env, jba);
		env->DeleteLocalRef(jba);
		if (env->ExceptionCheck()) {
			return nullptr;
		}
		names_vec.emplace_back(std::move(str));
	}

	try {
		auto &factory = default_factory();
		std::vector<cxx::TypeParam> params;
		params.reserve(member_count);
		bool unnamed = true;
		for (size_t i = 0; i < member_count; i++) {
			if (!*mt_vec[i]) {
				return nullptr;
			}
			if (!names_vec[i].empty()) {
				unnamed = false;
			}
			params.emplace_back(names_vec[i], factory.CreateValue(*mt_vec[i]));
		}
		// same as in C API v1, an unnamed (empty member names) struct is a TUPLE
		auto type_id = unnamed && member_count > 0 ? cxx::LogicalTypeId::TUPLE : cxx::LogicalTypeId::STRUCT;
		cxx::LogicalType struct_type = factory.CreateType(type_id, params);
		return make_logical_type_buf(env, std::move(struct_type));
	} catch (const std::exception &) {
		return nullptr;
	}
}

/*
 * Returns the number of struct children, the same way as C API v1, that considers the hidden "tag" field
 * of a UNION as its first child.
 */
static idx_t struct_type_child_count(const cxx::LogicalType &lt) {
	switch (logical_type_to_capi(lt)) {
	case capi_type::STRUCT:
		return lt.GetStructChildCount();
	case capi_type::UNION:
		return lt.GetUnionMemberCount() + 1;
	default:
		return 0;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_struct_type_child_count
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1struct_1type_1child_1count(JNIEnv *env, jclass,
                                                                                          jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		idx_t count = struct_type_child_count(*lt);
		return static_cast<jlong>(count);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_struct_type_child_name
 * Signature: (Ljava/nio/ByteBuffer;J)[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1struct_1type_1child_1name(JNIEnv *env, jclass,
                                                                                              jobject logical_type,
                                                                                              jlong index) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	idx_t index_idx = jlong_to_idx(env, index);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		idx_t count = struct_type_child_count(*lt);
		if (index_idx >= count) {
			env->ThrowNew(J_SQLException, "Invalid struct field index specified");
			return nullptr;
		}

		std::string name;
		if (logical_type_to_capi(*lt) == capi_type::UNION) {
			// the hidden "tag" field of a UNION has an empty name
			name = index_idx == 0 ? std::string() : lt->GetUnionMemberName(index_idx - 1);
		} else {
			name = lt->GetStructChildName(index_idx);
		}

		return make_jbyteArray(env, name.data(), static_cast<idx_t>(name.length()));
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_array_type_array_size
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1array_1type_1array_1size(JNIEnv *env, jclass,
                                                                                        jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		if (logical_type_to_capi(*lt) != capi_type::ARRAY) {
			return 0;
		}
		idx_t size = lt->GetArraySize();
		return static_cast<jlong>(size);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_enum_internal_type
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1enum_1internal_1type(JNIEnv *env, jclass,
                                                                                   jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		if (logical_type_to_capi(*lt) != capi_type::ENUM) {
			return static_cast<jint>(capi_type::INVALID);
		}
		switch (lt->GetEnumInternalTypeId()) {
		case cxx::LogicalTypeId::UTINYINT:
		case cxx::LogicalTypeId::USMALLINT:
		case cxx::LogicalTypeId::UINTEGER:
			return static_cast<jint>(type_id_to_capi(lt->GetEnumInternalTypeId()));
		default:
			return static_cast<jint>(capi_type::INVALID);
		}
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_enum_dictionary_size
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1enum_1dictionary_1size(JNIEnv *env, jclass,
                                                                                      jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return -1;
	}

	try {
		if (logical_type_to_capi(*lt) != capi_type::ENUM) {
			return 0;
		}
		idx_t size = lt->GetEnumSize();
		return static_cast<jlong>(size);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_enum_dictionary_value
 * Signature: (Ljava/nio/ByteBuffer;J)[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1enum_1dictionary_1value(JNIEnv *env, jclass,
                                                                                            jobject logical_type,
                                                                                            jlong index) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	idx_t index_idx = jlong_to_idx(env, index);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		if (logical_type_to_capi(*lt) != capi_type::ENUM) {
			return nullptr;
		}

		idx_t size = lt->GetEnumSize();
		if (index_idx >= size) {
			env->ThrowNew(J_SQLException, "Invalid enum field index specified");
			return nullptr;
		}

		std::string name = lt->GetEnumValue(index_idx);

		return make_jbyteArray(env, name.data(), static_cast<idx_t>(name.length()));
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_destroy_logical_type
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1destroy_1logical_1type(JNIEnv *env, jclass,
                                                                                     jobject logical_type) {

	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return;
	}

	delete lt;
}
