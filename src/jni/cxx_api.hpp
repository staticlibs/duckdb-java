#pragma once

// Helpers for the parts of the driver that are built on top of the DuckDB C++ Stable API (duckdb_cpp.hpp).
//
// The Java side keeps native handles in direct ByteBuffers, these helpers convert between such raw pointers
// and the move-only handle wrappers of the C++ API.

#include "duckdb_cpp.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace cxx = duckdb::cxx;

/**
 * Return codes, that are passed to Java, numerically the same as the ones of C API v1 (DuckDBSuccess / DuckDBError).
 */
static constexpr int32_t CAPI_STATE_SUCCESS = 0;
static constexpr int32_t CAPI_STATE_ERROR = 1;

/**
 * Type IDs, that are passed to Java (see DuckDBBindings.CAPIType), numerically the same as the DUCKDB_TYPE
 * enum of C API v1.
 */
enum class capi_type : int32_t {
	INVALID = 0,
	BOOLEAN = 1,
	TINYINT = 2,
	SMALLINT = 3,
	INTEGER = 4,
	BIGINT = 5,
	UTINYINT = 6,
	USMALLINT = 7,
	UINTEGER = 8,
	UBIGINT = 9,
	FLOAT = 10,
	DOUBLE = 11,
	TIMESTAMP = 12,
	DATE = 13,
	TIME = 14,
	INTERVAL = 15,
	HUGEINT = 16,
	VARCHAR = 17,
	BLOB = 18,
	DECIMAL = 19,
	TIMESTAMP_S = 20,
	TIMESTAMP_MS = 21,
	TIMESTAMP_NS = 22,
	ENUM = 23,
	LIST = 24,
	STRUCT = 25,
	MAP = 26,
	UUID = 27,
	UNION = 28,
	BIT = 29,
	TIME_TZ = 30,
	TIMESTAMP_TZ = 31,
	UHUGEINT = 32,
	ARRAY = 33,
	ANY = 34,
	BIGNUM = 35,
	SQLNULL = 36,
	STRING_LITERAL = 37,
	INTEGER_LITERAL = 38,
	TIME_NS = 39,
	GEOMETRY = 40,
	VARIANT = 41,
	TIMESTAMP_TZ_NS = 42,
	GEOGRAPHY = 43,
};

/**
 * Converts the type ID of the C++ API to the C API v1 type ID that is used on Java side.
 */
capi_type type_id_to_capi(cxx::LogicalTypeId type_id);

/**
 * Converts the type ID of the C++ API to the C API v1 type ID, the empty (invalid) type is reported as INVALID.
 */
capi_type logical_type_to_capi(const cxx::LogicalType &logical_type);

/**
 * Converts the C API v1 type ID, that is used on Java side, to the type ID of the C++ API, returns false
 * if the type cannot be represented in the C++ API.
 */
bool type_id_from_capi(int32_t capi_type_id, cxx::LogicalTypeId &type_id);

/**
 * The factory of a process-wide DuckDB instance that has no databases attached to it.
 *
 * Unlike the C API v1, the C++ API requires a factory to create types, data chunks and values. Logical types
 * and data chunks can be created on Java side without a connection, the factory of this hidden instance is
 * used in such cases. It knows only built-in types and casts.
 */
cxx::Factory &default_factory();

/**
 * Underlying C API handle, extracted from the C++ API handle wrapper. This handle can be passed to Java and later
 * wrapped back with cxx_wrap. The wrappers in the C++ API do not expose their handles for this use, so this is
 * done using the handle() accessor with a custom traits type.
 */
struct cxx_raw_handle_traits {
	using handle = void *;
};

template <class T>
void *cxx_raw_handle(const T &wrapper) {
	return wrapper.template handle<cxx_raw_handle_traits>();
}

/**
 * Wraps an existing C API handle into a C++ API handle wrapper. For the wrappers that do not own their handles
 * (like Vector) this does not change ownership of the handle. For the owning wrappers (like LogicalType) the
 * handle is destroyed along with the wrapper.
 */
template <class T, class... ARGS>
T cxx_wrap(ARGS &&...args) {
	return cxx::detail::HandleFactory::Make<T>(std::forward<ARGS>(args)...);
}

/**
 * An empty LogicalType wrapper that does not hold a type, it is used to represent INVALID type that
 * cannot be created with the C++ API.
 */
cxx::LogicalType empty_logical_type();

/**
 * Copy of the specified type, the copy of an empty type is empty.
 */
cxx::LogicalType copy_logical_type(const cxx::LogicalType &logical_type);

/**
 * Error code of the engine exceptions that are thrown from the function callbacks
 * (see DUCKDB_V2_ERROR in duckdb_v2.h).
 */
static constexpr int CXX_ERROR_INPUT_INVALID = 2001;
static constexpr int CXX_ERROR_QUERY_BINDER = 5003;

/**
 * Exception that is reported to the engine as a BinderException when thrown from a function callback.
 */
cxx::Exception cxx_binder_exception(const std::string &message);

/**
 * Exception that is reported to the engine as an InvalidInputException when thrown from a function callback.
 */
cxx::Exception cxx_invalid_input_exception(const std::string &message);
