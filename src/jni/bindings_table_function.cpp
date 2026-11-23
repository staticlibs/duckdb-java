
#include "bindings_table_function.hpp"

#include "refs.hpp"
#include "util.hpp"

#include <algorithm>
#include <cctype>

/*
 * The table function as it is configured from Java.
 *
 * In C API v1 a table function is created without a connection, the C++ API requires the function to be
 * created with a factory of the database it is going to be registered in. So the configuration is collected
 * here and the C++ API function is created when the function is registered.
 */
struct TableFunctionSpec {
	std::string name;
	std::vector<cxx::LogicalType> parameters;
	std::vector<std::pair<std::string, cxx::LogicalType>> named_parameters;
	std::unique_ptr<GlobalRefHolder> callback;
	bool has_bind = false;
	bool has_init = false;
	bool has_local_init = false;
	bool has_function = false;
	bool projection_pushdown = false;
};

static bool names_equal_case_insensitive(const std::string &a, const std::string &b) {
	return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char ca, char cb) {
		       return std::tolower(static_cast<unsigned char>(ca)) == std::tolower(static_cast<unsigned char>(cb));
	       });
}

static std::string quote_identifier(const std::string &name) {
	std::string res = "\"";
	for (char c : name) {
		if (c == '"') {
			res += '"';
		}
		res += c;
	}
	res += '"';
	return res;
}

static std::string value_to_sql_string(const cxx::Value &value) {
	if (value.IsNull()) {
		return "NULL";
	}
	auto text = value.ToText();
	if (value.GetLogicalType().GetTypeId() != cxx::LogicalTypeId::VARCHAR) {
		return text;
	}
	std::string res = "'";
	for (char c : text) {
		if (c == '\'') {
			res += '\'';
		}
		res += c;
	}
	res += '\'';
	return res;
}

static std::string cast_error_message(const cxx::Exception &e) {
	std::string msg = e.GetRawMessage().empty() ? e.what() : e.GetRawMessage();
	static const std::string prefix = "Failed to cast value: ";
	if (msg.compare(0, prefix.size(), prefix) == 0) {
		msg = msg.substr(prefix.size());
	}
	return msg;
}

/*
 * The named parameters of a C API v1 table function are optional and are cast to their declared types. In the C++
 * API the named parameters without a default value are required, so the named parameters are received with the
 * "**kwargs" parameter and are checked and cast here, the same way as it is done for C API v1 functions.
 */
static void resolve_named_parameters(TableFunctionBindInfoHolder &info) {
	auto &input = info.input;
	auto counts = input.GetArgumentCounts();
	auto &declared = info.user_data.named_parameters;
	for (idx_t i = counts.NamedOffset(); i < counts.Total(); i++) {
		std::string name = input.GetArgName(i);
		auto it =
		    std::find_if(declared.begin(), declared.end(), [&](const std::pair<std::string, cxx::LogicalType> &p) {
			    return names_equal_case_insensitive(p.first, name);
		    });
		if (it == declared.end()) {
			throw cxx_binder_exception("Invalid named parameter " + quote_identifier(name) + " for function " +
			                           info.user_data.name);
		}
		for (auto &passed : info.named_parameters) {
			if (names_equal_case_insensitive(passed.first, it->first)) {
				throw cxx_binder_exception("Named parameter " + quote_identifier(it->first) +
				                           " was passed more than once in function call to " + info.user_data.name);
			}
		}
		auto passed_value = input.GetConstantArgument(i);
		auto passed_type = passed_value.GetLogicalType();
		if (passed_type == it->second) {
			info.named_parameters.emplace_back(it->first, std::move(passed_value));
			continue;
		}
		try {
			auto value = passed_value.Cast(input.GetContext().GetFactory(), it->second);
			info.named_parameters.emplace_back(it->first, std::move(value));
		} catch (const cxx::Exception &e) {
			throw cxx_invalid_input_exception("Could not cast value " + value_to_sql_string(passed_value) +
			                                  " to named parameter " + quote_identifier(it->first) + " of type " +
			                                  it->second.ToText() + ": " + cast_error_message(e));
		}
	}
}

static void table_function_bind(cxx::TableFunction::BindInput &input) {
	auto &user_data = *input.GetUserData<std::unique_ptr<TableFunctionUserData>>();
	TableFunctionBindInfoHolder info(input, user_data);
	resolve_named_parameters(info);

	auto callback_holder = user_data.callback.get();
	if (callback_holder == nullptr) {
		throw cxx_binder_exception("Table function callback not found");
	}
	AttachedJNIEnv attached = callback_holder->attach_current_thread();
	if (attached.env == nullptr) {
		throw cxx_binder_exception("Unable to attach JNI environment");
	}
	jobject info_buf = make_ptr_buf(attached.env, &info);
	if (info_buf == nullptr) {
		throw cxx_binder_exception("Unable to create bind info buffer");
	}
	LocalRefHolder info_buf_holder(attached.env, info_buf);
	attached.env->CallVoidMethod(callback_holder->global_ref, J_DuckDBTableFunctionWrapper_executeBind, info_buf);
	if (attached.env->ExceptionCheck()) {
		throw cxx_binder_exception("Java callback system error");
	}
	if (info.has_error) {
		throw cxx_binder_exception(info.error);
	}

	// the bind data is always set, it is empty when it was not set from Java
	GlobalRefHolder *bind_data = info.bind_data ? info.bind_data->release() : nullptr;
	input.SetBindData<TableFunctionState>(bind_data);
}

static void table_function_init(cxx::TableFunction::InitGlobalInput &input) {
	auto &user_data = *input.GetUserData<std::unique_ptr<TableFunctionUserData>>();
	TableFunctionInitInfoHolder info;
	info.global_input = &input;

	auto callback_holder = user_data.callback.get();
	if (callback_holder == nullptr) {
		throw cxx_invalid_input_exception("Table function callback not found");
	}
	AttachedJNIEnv attached = callback_holder->attach_current_thread();
	if (attached.env == nullptr) {
		throw cxx_invalid_input_exception("Unable to attach JNI environment");
	}
	jobject info_buf = make_ptr_buf(attached.env, &info);
	if (info_buf == nullptr) {
		throw cxx_invalid_input_exception("Unable to create global init info buffer");
	}
	LocalRefHolder info_buf_holder(attached.env, info_buf);
	attached.env->CallVoidMethod(callback_holder->global_ref, J_DuckDBTableFunctionWrapper_executeGlobalInit, info_buf);
	if (attached.env->ExceptionCheck()) {
		throw cxx_invalid_input_exception("Java callback system error");
	}
	if (info.has_error) {
		throw cxx_invalid_input_exception(info.error);
	}

	// the init data is always set, it is empty when it was not set from Java
	GlobalRefHolder *init_data = info.init_data ? info.init_data->release() : nullptr;
	input.SetGlobalState<TableFunctionState>(init_data);
}

static void table_function_local_init(cxx::TableFunction::InitLocalInput &input) {
	auto &user_data = *input.GetUserData<std::unique_ptr<TableFunctionUserData>>();
	TableFunctionInitInfoHolder info;
	info.local_input = &input;

	auto callback_holder = user_data.callback.get();
	if (callback_holder == nullptr) {
		throw cxx_invalid_input_exception("Table function callback not found");
	}
	AttachedJNIEnv attached = callback_holder->attach_current_thread();
	if (attached.env == nullptr) {
		throw cxx_invalid_input_exception("Unable to attach JNI environment");
	}
	jobject info_buf = make_ptr_buf(attached.env, &info);
	if (info_buf == nullptr) {
		throw cxx_invalid_input_exception("Unable to create local init info buffer");
	}
	LocalRefHolder info_buf_holder(attached.env, info_buf);
	attached.env->CallVoidMethod(callback_holder->global_ref, J_DuckDBTableFunctionWrapper_executeLocalInit, info_buf);
	if (attached.env->ExceptionCheck()) {
		throw cxx_invalid_input_exception("Java callback system error");
	}
	if (info.has_error) {
		throw cxx_invalid_input_exception(info.error);
	}

	GlobalRefHolder *init_data = info.init_data ? info.init_data->release() : nullptr;
	input.SetLocalState<TableFunctionState>(init_data);
}

static void table_function_exec(cxx::TableFunction::ExecInput &input) {
	auto &user_data = *input.GetUserData<std::unique_ptr<TableFunctionUserData>>();
	TableFunctionInfoHolder info(input, user_data.has_local_init);
	DataChunkHolder output(input.GetOutputChunk());

	auto callback_holder = user_data.callback.get();
	if (callback_holder == nullptr) {
		throw cxx_invalid_input_exception("Table function callback not found");
	}
	AttachedJNIEnv attached = callback_holder->attach_current_thread();
	if (attached.env == nullptr) {
		throw cxx_invalid_input_exception("Unable to attach JNI environment");
	}
	jobject info_buf = make_ptr_buf(attached.env, &info);
	if (info_buf == nullptr) {
		throw cxx_invalid_input_exception("Unable to create function info buffer");
	}
	LocalRefHolder info_buf_holder(attached.env, info_buf);
	jobject output_buf = make_ptr_buf(attached.env, &output);
	if (output_buf == nullptr) {
		throw cxx_invalid_input_exception("Unable to create output buffer");
	}
	LocalRefHolder output_buf_holder(attached.env, output_buf);
	attached.env->CallVoidMethod(callback_holder->global_ref, J_DuckDBTableFunctionWrapper_executeFunction, info_buf,
	                             output_buf);
	if (attached.env->ExceptionCheck()) {
		throw cxx_invalid_input_exception("Java callback system error");
	}
	if (info.has_error) {
		throw cxx_invalid_input_exception(info.error);
	}
}

/*
 * Creates the C++ API function from the specified configuration and registers it, the checks are the same as in
 * C API v1.
 */
static jint register_table_function(cxx::Connection &conn, TableFunctionSpec &spec) {
	if (spec.name.empty() || !spec.has_bind || !spec.has_init || !spec.has_function) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}
	for (auto &param : spec.parameters) {
		if (!param) {
			return static_cast<jint>(CAPI_STATE_ERROR);
		}
	}
	for (auto &param : spec.named_parameters) {
		if (!param.second) {
			return static_cast<jint>(CAPI_STATE_ERROR);
		}
	}

	try {
		auto &factory = conn.GetFactory();
		auto function = cxx::TableFunction::Create(factory);
		function.SetName(spec.name);
		{
			auto signature = function.GetSignature();
			// C API v1 declares no parameter names, the parameters are positional-only and are named "colN"
			for (size_t i = 0; i < spec.parameters.size(); i++) {
				signature.AddParameter("col" + std::to_string(i), spec.parameters[i],
				                       cxx::FunctionParameterKind::POSITIONAL_ONLY);
			}
			// named parameters are checked and cast in bind, see resolve_named_parameters
			if (!spec.named_parameters.empty()) {
				signature.AddKwargs("options", factory.CreateType(cxx::LogicalTypeId::ANY));
			}
		}
		function.SetBindCallback(table_function_bind);
		function.SetInitGlobalCallback(table_function_init);
		if (spec.has_local_init) {
			function.SetInitLocalCallback(table_function_local_init);
		}
		function.SetExecCallback(table_function_exec);
		function.SetProjectionPushdown(spec.projection_pushdown);

		// same as the "extra info" in C API v1, the callback is owned by the registered function
		auto user_data = std::unique_ptr<TableFunctionUserData>(new TableFunctionUserData());
		user_data->name = spec.name;
		user_data->callback = std::move(spec.callback);
		for (auto &param : spec.named_parameters) {
			user_data->named_parameters.emplace_back(param.first, param.second.Copy());
		}
		user_data->has_local_init = spec.has_local_init;
		function.SetUserData<std::unique_ptr<TableFunctionUserData>>(std::move(user_data));
		conn.Register(function);
	} catch (const std::exception &) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}

	return static_cast<jint>(CAPI_STATE_SUCCESS);
}

static TableFunctionSpec *table_function_buf_to_table_function(JNIEnv *env, jobject table_function_buf) {
	if (table_function_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid table function buffer");
		return nullptr;
	}

	auto table_function = reinterpret_cast<TableFunctionSpec *>(env->GetDirectBufferAddress(table_function_buf));
	if (table_function == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid table function");
		return nullptr;
	}
	return table_function;
}

static TableFunctionInfoHolder *function_info_buf_to_function_info(JNIEnv *env, jobject function_info_buf) {
	if (function_info_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid function info buffer");
		return nullptr;
	}

	auto function_info = reinterpret_cast<TableFunctionInfoHolder *>(env->GetDirectBufferAddress(function_info_buf));
	if (function_info == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid function info");
		return nullptr;
	}
	return function_info;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_create_table_function
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1create_1table_1function(JNIEnv *env, jclass) {

	auto tf = new TableFunctionSpec();
	jobject buf = make_ptr_buf(env, tf);
	if (buf == nullptr) {
		delete tf;
	}
	return buf;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_destroy_table_function
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1destroy_1table_1function(JNIEnv *env, jclass,
                                                                                       jobject table_function) {

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}

	delete tf;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_table_function_set_name
 * Signature: (Ljava/nio/ByteBuffer;[B)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1table_1function_1set_1name(JNIEnv *env, jclass,
                                                                                         jobject table_function,
                                                                                         jbyteArray name) {

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}
	std::string name_str = jbyteArray_to_string(env, name);
	if (env->ExceptionCheck()) {
		return;
	}
	if (name_str.empty()) {
		env->ThrowNew(J_SQLException, "Specified function name must be not empty");
		return;
	}

	tf->name = name_str;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_table_function_add_parameter
 * Signature: (Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1table_1function_1add_1parameter(JNIEnv *env, jclass,
                                                                                              jobject table_function,
                                                                                              jobject logical_type) {

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}
	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return;
	}

	tf->parameters.emplace_back(copy_logical_type(*lt));
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_table_function_add_named_parameter
 * Signature: (Ljava/nio/ByteBuffer;[BLjava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1table_1function_1add_1named_1parameter(
    JNIEnv *env, jclass, jobject table_function, jbyteArray name, jobject logical_type) {

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}
	std::string name_str = jbyteArray_to_string(env, name);
	if (env->ExceptionCheck()) {
		return;
	}
	if (name_str.empty()) {
		env->ThrowNew(J_SQLException, "Specified parameter name must be not empty");
		return;
	}
	auto lt = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return;
	}

	// same as in C API v1, the first declaration of a named parameter is used
	for (auto &param : tf->named_parameters) {
		if (names_equal_case_insensitive(param.first, name_str)) {
			return;
		}
	}
	tf->named_parameters.emplace_back(name_str, copy_logical_type(*lt));
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_table_function_set_extra_info
 * Signature: (Ljava/nio/ByteBuffer;Lorg/duckdb/DuckDBTableFunctionWrapper;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1table_1function_1set_1extra_1info(JNIEnv *env, jclass,
                                                                                                jobject table_function,
                                                                                                jobject callback) {

	if (callback == nullptr) {
		env->ThrowNew(J_SQLException, "Specified callback must be not null");
		return;
	}

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}

	auto callback_holder = std::unique_ptr<GlobalRefHolder>(new GlobalRefHolder(env, callback));
	if (callback_holder->vm == nullptr) {
		env->ThrowNew(J_SQLException, "Unable to create a global reference to the specified table function callback");
		return;
	}

	tf->callback = std::move(callback_holder);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_table_function_set_bind
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1table_1function_1set_1bind(JNIEnv *env, jclass,
                                                                                         jobject table_function) {

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}

	tf->has_bind = true;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_table_function_set_init
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1table_1function_1set_1init(JNIEnv *env, jclass,
                                                                                         jobject table_function) {

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}

	tf->has_init = true;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_table_function_set_local_init
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL
Java_org_duckdb_DuckDBBindings_duckdb_1table_1function_1set_1local_1init(JNIEnv *env, jclass, jobject table_function) {

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}

	tf->has_local_init = true;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_table_function_set_function
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1table_1function_1set_1function(JNIEnv *env, jclass,
                                                                                             jobject table_function) {

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}

	tf->has_function = true;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_table_function_supports_projection_pushdown
 * Signature: (Ljava/nio/ByteBuffer;Z)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1table_1function_1supports_1projection_1pushdown(
    JNIEnv *env, jclass, jobject table_function, jboolean pushdown) {

	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return;
	}

	bool pushdown_flag = !!pushdown;
	tf->projection_pushdown = pushdown_flag;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_register_table_function
 * Signature: (Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1register_1table_1function(JNIEnv *env, jclass,
                                                                                        jobject connection,
                                                                                        jobject table_function) {

	auto conn = conn_ref_buf_to_conn(env, connection);
	if (env->ExceptionCheck()) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}
	auto tf = table_function_buf_to_table_function(env, table_function);
	if (env->ExceptionCheck()) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}

	return register_table_function(*conn, *tf);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_function_get_bind_data
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1function_1get_1bind_1data(JNIEnv *env, jclass,
                                                                                           jobject function_info) {

	auto fi = function_info_buf_to_function_info(env, function_info);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		return fi->input.GetBindData<TableFunctionState>().get_object();
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_function_get_init_data
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1function_1get_1init_1data(JNIEnv *env, jclass,
                                                                                           jobject function_info) {

	auto fi = function_info_buf_to_function_info(env, function_info);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	try {
		return fi->input.GetGlobalState<TableFunctionState>().get_object();
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_function_get_local_init_data
 * Signature: (Ljava/nio/ByteBuffer;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_org_duckdb_DuckDBBindings_duckdb_1function_1get_1local_1init_1data(JNIEnv *env, jclass, jobject function_info) {

	auto fi = function_info_buf_to_function_info(env, function_info);
	if (env->ExceptionCheck()) {
		return nullptr;
	}

	if (!fi->has_local_init) {
		return nullptr;
	}
	try {
		return fi->input.GetLocalState<TableFunctionState>().get_object();
	} catch (const std::exception &e) {
		throw_sql_exception(env, e);
		return nullptr;
	}
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_function_set_error
 * Signature: (Ljava/nio/ByteBuffer;[B)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1function_1set_1error(JNIEnv *env, jclass,
                                                                                   jobject function_info,
                                                                                   jbyteArray error) {

	auto fi = function_info_buf_to_function_info(env, function_info);
	if (env->ExceptionCheck()) {
		return;
	}

	std::string error_str = jbyteArray_to_string(env, error);
	if (env->ExceptionCheck()) {
		return;
	}

	fi->has_error = true;
	fi->error = error_str;
}
