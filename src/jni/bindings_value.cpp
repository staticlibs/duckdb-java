#include "bindings.hpp"
#include "refs.hpp"
#include "util.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

static cxx::Value *value_buf_to_value(JNIEnv *env, jobject value_buf) {
	if (value_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid value buffer");
		return nullptr;
	}

	auto value = reinterpret_cast<cxx::Value *>(env->GetDirectBufferAddress(value_buf));
	if (value == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid value");
		return nullptr;
	}
	return value;
}

/*
 * Same as in C API v1, the value is converted to the requested type and a "NULL value" of this type
 * (see duckdb::NullValue) is returned if the conversion fails.
 */
template <class T>
static T null_value() {
	return T();
}

template <>
int8_t null_value() {
	return std::numeric_limits<int8_t>::min();
}

template <>
int16_t null_value() {
	return std::numeric_limits<int16_t>::min();
}

template <>
int32_t null_value() {
	return std::numeric_limits<int32_t>::min();
}

template <>
int64_t null_value() {
	return std::numeric_limits<int64_t>::min();
}

template <>
cxx::int128_t null_value() {
	return cxx::int128_t {0, std::numeric_limits<int64_t>::min()};
}

template <>
cxx::uint128_t null_value() {
	return cxx::uint128_t {0, 0};
}

template <>
float null_value() {
	return NAN;
}

template <>
double null_value() {
	return NAN;
}

template <class T>
static T get_value(const cxx::Value &val) {
	try {
		return val.Get<T>();
	} catch (const std::exception &) {
		return null_value<T>();
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_is_null_value
 * Signature: (Ljava/nio/ByteBuffer;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1is_1null_1value(JNIEnv *env, jclass, jobject value) {
	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return false;
	}

	bool flag = val->IsNull();
	return static_cast<jboolean>(flag);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_value_type
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1value_1type(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return 0;
	}

	try {
		return static_cast<jint>(logical_type_to_capi(val->GetLogicalType()));
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return -1;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_destroy_value
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1destroy_1value(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return;
	}

	delete val;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_bool
 * Signature: (Ljava/nio/ByteBuffer;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1bool(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return false;
	}
	bool flag = get_value<bool>(*val);
	return static_cast<jboolean>(flag);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_int8
 * Signature: (Ljava/nio/ByteBuffer;)B
 */
JNIEXPORT jbyte JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1int8(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jbyte>(0);
	}
	int8_t num = get_value<int8_t>(*val);
	return static_cast<jbyte>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_uint8
 * Signature: (Ljava/nio/ByteBuffer;)S
 */
JNIEXPORT jshort JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1uint8(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jshort>(0);
	}
	uint8_t num = get_value<uint8_t>(*val);
	return static_cast<jshort>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_int16
 * Signature: (Ljava/nio/ByteBuffer;)S
 */
JNIEXPORT jshort JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1int16(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jshort>(0);
	}
	int16_t num = get_value<int16_t>(*val);
	return static_cast<jshort>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_uint16
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1uint16(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jint>(0);
	}
	uint16_t num = get_value<uint16_t>(*val);
	return static_cast<jint>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_int32
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1int32(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jint>(0);
	}
	int32_t num = get_value<int32_t>(*val);
	return static_cast<jint>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_uint32
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1uint32(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jlong>(0);
	}
	uint32_t num = get_value<uint32_t>(*val);
	return static_cast<jlong>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_int64
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1int64(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jlong>(0);
	}
	int64_t num = get_value<int64_t>(*val);
	return static_cast<jlong>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_uint64
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1uint64(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jlong>(0);
	}
	uint64_t num = get_value<uint64_t>(*val);
	return static_cast<jlong>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_hugeint
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/math/BigInteger;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1hugeint(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	cxx::int128_t hi = get_value<cxx::int128_t>(*val);
	jlong lower = static_cast<jlong>(hi.lower);
	jlong upper = static_cast<jlong>(hi.upper);
	return env->CallStaticObjectMethod(J_HugeInt, J_HugeInt_toBigInteger, lower, upper);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_uhugeint
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/math/BigInteger;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1uhugeint(JNIEnv *env, jclass, jobject value) {
	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	cxx::uint128_t uhi = get_value<cxx::uint128_t>(*val);
	jlong lower = static_cast<jlong>(uhi.lower);
	jlong upper = static_cast<jlong>(uhi.upper);
	return env->CallStaticObjectMethod(J_HugeInt, J_HugeInt_toBigInteger, lower, upper);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_bignum
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/math/BigInteger;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1bignum(JNIEnv *env, jclass, jobject value) {
	(void)env;
	(void)value;
	return nullptr;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_decimal
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/math/BigDecimal;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1decimal(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return nullptr;
	}
	// same as in C API v1, zero is returned for non-decimal and NULL values and for the decimals
	// that cannot be represented
	cxx::Value::RuntimeDecimal dec {};
	try {
		dec = val->GetDecimal();
	} catch (const std::exception &) {
		dec = cxx::Value::RuntimeDecimal {};
	}
	jlong lower = static_cast<jlong>(dec.value.lower);
	jlong upper = static_cast<jlong>(dec.value.upper);
	jint scale = static_cast<jint>(dec.scale);
	return env->CallStaticObjectMethod(J_HugeInt, J_HugeInt_toBigDecimal, lower, upper, scale);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_float
 * Signature: (Ljava/nio/ByteBuffer;)F
 */
JNIEXPORT jfloat JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1float(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jfloat>(0);
	}
	float num = get_value<float>(*val);
	return static_cast<jfloat>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_double
 * Signature: (Ljava/nio/ByteBuffer;)D
 */
JNIEXPORT jdouble JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1double(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jdouble>(0);
	}
	double num = get_value<double>(*val);
	return static_cast<jdouble>(num);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_date
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1date(JNIEnv *env, jclass, jobject value) {
	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jint>(0);
	}
	cxx::date_t dt = get_value<cxx::date_t>(*val);
	return static_cast<jint>(dt.days);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_time
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1time(JNIEnv *env, jclass, jobject value) {
	(void)env;
	(void)value;
	return 0;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_time_ns
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1time_1ns(JNIEnv *env, jclass, jobject value) {
	(void)env;
	(void)value;
	return 0;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_time_tz
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1time_1tz(JNIEnv *env, jclass, jobject value) {
	(void)env;
	(void)value;
	return 0;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_timestamp
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1timestamp(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jlong>(0);
	}

	cxx::timestamp_t ts = get_value<cxx::timestamp_t>(*val);
	return static_cast<jlong>(ts.micros);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_timestamp_tz
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1timestamp_1tz(JNIEnv *env, jclass, jobject value) {
	(void)env;
	(void)value;
	return 0;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_timestamp_s
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1timestamp_1s(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jlong>(0);
	}

	cxx::timestamp_s_t ts = get_value<cxx::timestamp_s_t>(*val);
	return static_cast<jlong>(ts.seconds);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_timestamp_ms
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1timestamp_1ms(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jlong>(0);
	}

	cxx::timestamp_ms_t ts = get_value<cxx::timestamp_ms_t>(*val);
	return static_cast<jlong>(ts.millis);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_timestamp_ns
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1timestamp_1ns(JNIEnv *env, jclass, jobject value) {

	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return static_cast<jlong>(0);
	}

	cxx::timestamp_ns_t ts = get_value<cxx::timestamp_ns_t>(*val);
	return static_cast<jlong>(ts.nanos);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_get_varchar
 * Signature: (Ljava/nio/ByteBuffer;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1get_1varchar(JNIEnv *env, jclass, jobject value) {
	cxx::Value *val = value_buf_to_value(env, value);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		// same as in C API v1, the value is cast to VARCHAR
		std::string str;
		if (val->GetLogicalType().GetTypeId() == cxx::LogicalTypeId::VARCHAR) {
			str = std::string(val->Get<cxx::varchar_t>().view());
		} else {
			auto &factory = default_factory();
			auto varchar_type = factory.CreateType(cxx::LogicalTypeId::VARCHAR);
			auto str_val = val->Cast(factory, varchar_type);
			str = std::string(str_val.Get<cxx::varchar_t>().view());
		}
		// same as in C API v1, the string is returned up to the first NUL character
		idx_t len = static_cast<idx_t>(std::strlen(str.c_str()));
		return make_jbyteArray(env, str.c_str(), len);
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}
