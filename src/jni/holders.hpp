#pragma once

#include "cxx_api.hpp"
#include "duckdb.hpp"
#include "duckdb/main/capi_v2/capi_v2_internal.hpp"
#include "refs.hpp"

#include <jni.h>
#include <mutex>

/**
 * Holds a copy of a shared_ptr to an existing DB instance.
 * Is used to keep this DB alive (and accessible from DB cache)
 * even after the last connection to this DB is closed.
 */
struct DBHolder {
	duckdb::shared_ptr<duckdb::DuckDB> db;

	DBHolder(duckdb::shared_ptr<duckdb::DuckDB> _db) : db(std::move(_db)) {};

	DBHolder(const DBHolder &) = delete;

	DBHolder &operator=(const DBHolder &) = delete;
};

/**
 * State of an Arrow stream exported from or registered with a connection. Arrow streams are owned by their consumers
 * and can outlive the connection, connection closes the state of all its live streams before it is disconnected.
 */
struct ArrowStreamState {
	virtual ~ArrowStreamState() = default;

	//! Releases the resources held by the stream, must be idempotent
	virtual void Close() = 0;
};

/**
 * Associates a duckdb::Connection with a duckdb::DuckDB. The DB may be shared amongst many ConnectionHolders, but the
 * Connection is unique to this holder. Every Java DuckDBConnection has exactly 1 of these holders, and they are never
 * shared. The holder is freed when the DuckDBConnection is closed. When the last holder sharing a DuckDB is freed, the
 * DuckDB is released as well.
 *
 * The connection is created as a capiv2::CV2Connection (that is a duckdb::Connection) so it can also be used as
 * a connection handle of the C API v2 and wrapped into a (non-owning) cxx::Connection of the C++ API.
 */
struct ConnectionHolder {
	const duckdb::shared_ptr<duckdb::DuckDB> db;
	const duckdb::unique_ptr<duckdb::capiv2::CV2Connection> connection;

	ConnectionHolder(duckdb::shared_ptr<duckdb::DuckDB> _db)
	    : db(_db), connection(duckdb::make_uniq<duckdb::capiv2::CV2Connection>(*_db)) {
	}

	~ConnectionHolder() {
		close_arrow_streams();
	}

	void track_arrow_stream(duckdb::shared_ptr<ArrowStreamState> state);

	void close_arrow_streams();

	DBHolder *create_db_ref() {
		return new DBHolder(db);
	}

private:
	std::mutex arrow_streams_lock;
	//! Streams are untracked implicitly when they are released by their consumers
	duckdb::vector<duckdb::weak_ptr<ArrowStreamState>> arrow_streams;
};

struct StatementHolder {
	duckdb::unique_ptr<duckdb::PreparedStatement> stmt;
};

struct PendingHolder {
	duckdb::unique_ptr<duckdb::QueryResult> pending;
	bool streaming = false;
};

struct ResultHolder {
	duckdb::unique_ptr<duckdb::QueryResult> res;
	duckdb::unique_ptr<duckdb::QueryResultStream<>> stream;
	duckdb::unique_ptr<duckdb::DataChunk> chunk;
};

/**
 * Result of a prepared statement execution that is read by chunks (see DuckDBChunkedResult) using C++ API.
 * The column names and types are captured when the result is created, the error is set when fetching of
 * a chunk fails, same as with the C API v1 result.
 */
struct ChunkedResultHolder {
	cxx::QueryResult result;
	std::vector<std::string> column_names;
	std::vector<capi_type> column_types;
	bool has_error = false;
	std::string error;

	explicit ChunkedResultHolder(cxx::QueryResult result_p) : result(std::move(result_p)) {
		auto schema = result.GetSchema();
		idx_t count = schema.GetFieldCount();
		for (idx_t i = 0; i < count; i++) {
			column_names.emplace_back(schema.GetFieldName(i));
			column_types.push_back(logical_type_to_capi(schema.GetFieldType(i)));
		}
	}
};

struct AttachedJNIEnv {
	JavaVM *vm = nullptr;
	JNIEnv *env = nullptr;
	bool need_to_detach = false;

	AttachedJNIEnv();

	AttachedJNIEnv(JavaVM *vm_in, JNIEnv *env_in, bool need_to_detach_in);

	~AttachedJNIEnv() noexcept;
};

struct GlobalRefHolder {
	JavaVM *vm = nullptr;
	jobject global_ref = nullptr;

	GlobalRefHolder(JNIEnv *env, jobject local_ref);

	~GlobalRefHolder() noexcept;

	AttachedJNIEnv attach_current_thread();

	void detach_current_thread();

	static void destroy(void *holder_in) noexcept;
	static void destroy_table_function_state(void *holder_in) noexcept;
};

struct LocalRefHolder {
	JNIEnv *env = nullptr;
	jobject local_ref = nullptr;

	LocalRefHolder(JNIEnv *env_in, jobject local_ref_in);

	~LocalRefHolder() noexcept;
};

ConnectionHolder *get_connection_ref(JNIEnv *env, jobject conn_ref_buf);

duckdb::Connection *get_connection(JNIEnv *env, jobject conn_ref_buf);

/**
 * Throws a SQLException and returns nullptr if a valid Connection can't be retrieved from the buffer.
 */
duckdb::capiv2::CV2Connection *conn_ref_buf_to_engine_conn(JNIEnv *env, jobject conn_ref_buf);

/**
 * Returns a non-owning C++ API connection wrapping the connection of the specified holder. Throws a SQLException
 * and returns nullptr if a valid Connection can't be retrieved from the buffer.
 */
std::unique_ptr<cxx::Connection> conn_ref_buf_to_conn(JNIEnv *env, jobject conn_ref_buf);
