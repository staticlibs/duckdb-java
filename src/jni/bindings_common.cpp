#include "bindings.hpp"

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_vector_size
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1vector_1size(JNIEnv *, jclass) {

	// The C++ API does not expose the vector size constant directly, it is the capacity of a newly created chunk
	static const idx_t vector_size = [] {
		auto &factory = default_factory();
		std::vector<cxx::LogicalType> types;
		types.emplace_back(factory.CreateType(cxx::LogicalTypeId::BOOLEAN));
		cxx::DataChunk chunk(factory, types);
		return chunk.GetCapacity();
	}();

	return static_cast<jlong>(vector_size);
}
