#include "cxx_api.hpp"

capi_type type_id_to_capi(cxx::LogicalTypeId type_id) {
	switch (type_id) {
	case cxx::LogicalTypeId::INVALID:
	case cxx::LogicalTypeId::UNKNOWN:
		return capi_type::INVALID;
	case cxx::LogicalTypeId::BOOLEAN:
		return capi_type::BOOLEAN;
	case cxx::LogicalTypeId::TINYINT:
		return capi_type::TINYINT;
	case cxx::LogicalTypeId::SMALLINT:
		return capi_type::SMALLINT;
	case cxx::LogicalTypeId::INTEGER:
		return capi_type::INTEGER;
	case cxx::LogicalTypeId::BIGINT:
		return capi_type::BIGINT;
	case cxx::LogicalTypeId::UTINYINT:
		return capi_type::UTINYINT;
	case cxx::LogicalTypeId::USMALLINT:
		return capi_type::USMALLINT;
	case cxx::LogicalTypeId::UINTEGER:
		return capi_type::UINTEGER;
	case cxx::LogicalTypeId::UBIGINT:
		return capi_type::UBIGINT;
	case cxx::LogicalTypeId::HUGEINT:
		return capi_type::HUGEINT;
	case cxx::LogicalTypeId::UHUGEINT:
		return capi_type::UHUGEINT;
	case cxx::LogicalTypeId::FLOAT:
		return capi_type::FLOAT;
	case cxx::LogicalTypeId::DOUBLE:
		return capi_type::DOUBLE;
	case cxx::LogicalTypeId::TIMESTAMP:
		return capi_type::TIMESTAMP;
	case cxx::LogicalTypeId::TIMESTAMP_TZ:
		return capi_type::TIMESTAMP_TZ;
	case cxx::LogicalTypeId::TIMESTAMP_TZ_NS:
		return capi_type::TIMESTAMP_TZ_NS;
	case cxx::LogicalTypeId::TIMESTAMP_SEC:
		return capi_type::TIMESTAMP_S;
	case cxx::LogicalTypeId::TIMESTAMP_MS:
		return capi_type::TIMESTAMP_MS;
	case cxx::LogicalTypeId::TIMESTAMP_NS:
		return capi_type::TIMESTAMP_NS;
	case cxx::LogicalTypeId::DATE:
		return capi_type::DATE;
	case cxx::LogicalTypeId::TIME:
		return capi_type::TIME;
	case cxx::LogicalTypeId::TIME_TZ:
		return capi_type::TIME_TZ;
	case cxx::LogicalTypeId::VARCHAR:
		return capi_type::VARCHAR;
	case cxx::LogicalTypeId::BLOB:
		return capi_type::BLOB;
	case cxx::LogicalTypeId::BIT:
		return capi_type::BIT;
	case cxx::LogicalTypeId::BIGNUM:
		return capi_type::BIGNUM;
	case cxx::LogicalTypeId::INTERVAL:
		return capi_type::INTERVAL;
	case cxx::LogicalTypeId::DECIMAL:
		return capi_type::DECIMAL;
	case cxx::LogicalTypeId::ENUM:
		return capi_type::ENUM;
	case cxx::LogicalTypeId::LIST:
		return capi_type::LIST;
	case cxx::LogicalTypeId::STRUCT:
	case cxx::LogicalTypeId::TUPLE:
		// TUPLE is an unnamed struct - reported as STRUCT, same as in C API v1 (child names are empty)
		return capi_type::STRUCT;
	case cxx::LogicalTypeId::MAP:
		return capi_type::MAP;
	case cxx::LogicalTypeId::UNION:
		return capi_type::UNION;
	case cxx::LogicalTypeId::UUID:
		return capi_type::UUID;
	case cxx::LogicalTypeId::ARRAY:
		return capi_type::ARRAY;
	case cxx::LogicalTypeId::ANY:
		return capi_type::ANY;
	case cxx::LogicalTypeId::SQLNULL:
		return capi_type::SQLNULL;
	case cxx::LogicalTypeId::TIME_NS:
		return capi_type::TIME_NS;
	case cxx::LogicalTypeId::GEOMETRY:
		return capi_type::GEOMETRY;
	case cxx::LogicalTypeId::VARIANT:
		return capi_type::VARIANT;
	default:
		return capi_type::INVALID;
	}
}

capi_type logical_type_to_capi(const cxx::LogicalType &logical_type) {
	if (!logical_type) {
		return capi_type::INVALID;
	}
	return type_id_to_capi(logical_type.GetTypeId());
}

bool type_id_from_capi(int32_t capi_type_id, cxx::LogicalTypeId &type_id) {
	switch (static_cast<capi_type>(capi_type_id)) {
	case capi_type::BOOLEAN:
		type_id = cxx::LogicalTypeId::BOOLEAN;
		return true;
	case capi_type::TINYINT:
		type_id = cxx::LogicalTypeId::TINYINT;
		return true;
	case capi_type::SMALLINT:
		type_id = cxx::LogicalTypeId::SMALLINT;
		return true;
	case capi_type::INTEGER:
		type_id = cxx::LogicalTypeId::INTEGER;
		return true;
	case capi_type::BIGINT:
		type_id = cxx::LogicalTypeId::BIGINT;
		return true;
	case capi_type::UTINYINT:
		type_id = cxx::LogicalTypeId::UTINYINT;
		return true;
	case capi_type::USMALLINT:
		type_id = cxx::LogicalTypeId::USMALLINT;
		return true;
	case capi_type::UINTEGER:
		type_id = cxx::LogicalTypeId::UINTEGER;
		return true;
	case capi_type::UBIGINT:
		type_id = cxx::LogicalTypeId::UBIGINT;
		return true;
	case capi_type::HUGEINT:
		type_id = cxx::LogicalTypeId::HUGEINT;
		return true;
	case capi_type::UHUGEINT:
		type_id = cxx::LogicalTypeId::UHUGEINT;
		return true;
	case capi_type::FLOAT:
		type_id = cxx::LogicalTypeId::FLOAT;
		return true;
	case capi_type::DOUBLE:
		type_id = cxx::LogicalTypeId::DOUBLE;
		return true;
	case capi_type::TIMESTAMP:
		type_id = cxx::LogicalTypeId::TIMESTAMP;
		return true;
	case capi_type::TIMESTAMP_TZ:
		type_id = cxx::LogicalTypeId::TIMESTAMP_TZ;
		return true;
	case capi_type::TIMESTAMP_TZ_NS:
		type_id = cxx::LogicalTypeId::TIMESTAMP_TZ_NS;
		return true;
	case capi_type::TIMESTAMP_S:
		type_id = cxx::LogicalTypeId::TIMESTAMP_SEC;
		return true;
	case capi_type::TIMESTAMP_MS:
		type_id = cxx::LogicalTypeId::TIMESTAMP_MS;
		return true;
	case capi_type::TIMESTAMP_NS:
		type_id = cxx::LogicalTypeId::TIMESTAMP_NS;
		return true;
	case capi_type::DATE:
		type_id = cxx::LogicalTypeId::DATE;
		return true;
	case capi_type::TIME:
		type_id = cxx::LogicalTypeId::TIME;
		return true;
	case capi_type::TIME_TZ:
		type_id = cxx::LogicalTypeId::TIME_TZ;
		return true;
	case capi_type::VARCHAR:
		type_id = cxx::LogicalTypeId::VARCHAR;
		return true;
	case capi_type::BLOB:
		type_id = cxx::LogicalTypeId::BLOB;
		return true;
	case capi_type::INTERVAL:
		type_id = cxx::LogicalTypeId::INTERVAL;
		return true;
	case capi_type::DECIMAL:
		type_id = cxx::LogicalTypeId::DECIMAL;
		return true;
	case capi_type::ENUM:
		type_id = cxx::LogicalTypeId::ENUM;
		return true;
	case capi_type::LIST:
		type_id = cxx::LogicalTypeId::LIST;
		return true;
	case capi_type::STRUCT:
		type_id = cxx::LogicalTypeId::STRUCT;
		return true;
	case capi_type::MAP:
		type_id = cxx::LogicalTypeId::MAP;
		return true;
	case capi_type::ARRAY:
		type_id = cxx::LogicalTypeId::ARRAY;
		return true;
	case capi_type::UUID:
		type_id = cxx::LogicalTypeId::UUID;
		return true;
	case capi_type::UNION:
		type_id = cxx::LogicalTypeId::UNION;
		return true;
	case capi_type::BIT:
		type_id = cxx::LogicalTypeId::BIT;
		return true;
	case capi_type::BIGNUM:
		type_id = cxx::LogicalTypeId::BIGNUM;
		return true;
	case capi_type::ANY:
		type_id = cxx::LogicalTypeId::ANY;
		return true;
	case capi_type::SQLNULL:
		type_id = cxx::LogicalTypeId::SQLNULL;
		return true;
	case capi_type::TIME_NS:
		type_id = cxx::LogicalTypeId::TIME_NS;
		return true;
	case capi_type::GEOMETRY:
		type_id = cxx::LogicalTypeId::GEOMETRY;
		return true;
	case capi_type::VARIANT:
		type_id = cxx::LogicalTypeId::VARIANT;
		return true;
	default:
		// INVALID, STRING_LITERAL, INTEGER_LITERAL, GEOGRAPHY
		return false;
	}
}

cxx::Factory &default_factory() {
	// Intentionally leaked: it must stay valid while types and chunks created with it are in use, including
	// at the time of the static destruction.
	static cxx::Instance *instance = new cxx::Instance(cxx::StartupOptions().Set("threads", "1"));
	return instance->GetFactory();
}

cxx::LogicalType empty_logical_type() {
	return cxx_wrap<cxx::LogicalType>(nullptr);
}

cxx::LogicalType copy_logical_type(const cxx::LogicalType &logical_type) {
	if (!logical_type) {
		return empty_logical_type();
	}
	return logical_type.Copy();
}

cxx::Exception cxx_binder_exception(const std::string &message) {
	return cxx::Exception(CXX_ERROR_QUERY_BINDER, message);
}

cxx::Exception cxx_invalid_input_exception(const std::string &message) {
	return cxx::Exception(CXX_ERROR_INPUT_INVALID, message);
}
