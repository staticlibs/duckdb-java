#include "bindings.hpp"
#include "functions.hpp"
#include "holders.hpp"
#include "refs.hpp"
#include "util.hpp"

#include <memory>
#include <string>
#include <vector>

/*
 * The scalar function as it is configured from Java.
 *
 * In C API v1 a scalar function is created without a connection, the C++ API requires the function to be
 * created with a factory of the database it is going to be registered in. So the configuration is collected
 * here and the C++ API function is created when the function is registered.
 */
struct ScalarFunctionSpec {
	std::string name;
	std::vector<cxx::LogicalType> parameters;
	cxx::LogicalType return_type = empty_logical_type();
	cxx::LogicalType varargs = empty_logical_type();
	bool is_volatile = false;
	bool special_handling = false;
	bool has_function = false;
	std::unique_ptr<GlobalRefHolder> callback;
};

/*
 * User data of the registered C++ API function, the same as "extra info" in C API v1.
 */
struct ScalarFunctionUserData {
	explicit ScalarFunctionUserData(std::unique_ptr<GlobalRefHolder> callback_p) : callback(std::move(callback_p)) {
	}

	std::unique_ptr<GlobalRefHolder> callback;
};

/*
 * Per-thread state of the function, holds the data chunk, that references the arguments of the function and
 * is passed to Java as an input chunk.
 */
struct ScalarFunctionExecState {
	std::unique_ptr<DataChunkHolder> input;
};

/*
 * Function info that is passed to Java during the function call, it is used to report the error.
 */
struct ScalarFunctionInfoHolder {
	bool has_error = false;
	std::string error;
};

static ScalarFunctionSpec *scalar_function_buf_to_scalar_function(JNIEnv *env, jobject scalar_function_buf) {

	if (scalar_function_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid scalar function buffer");
		return nullptr;
	}

	auto scalar_function = reinterpret_cast<ScalarFunctionSpec *>(env->GetDirectBufferAddress(scalar_function_buf));
	if (scalar_function == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid scalar function");
		return nullptr;
	}

	return scalar_function;
}

static ScalarFunctionInfoHolder *function_info_buf_to_function_info(JNIEnv *env, jobject function_info_buf) {
	if (function_info_buf == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid scalar function info buffer");
		return nullptr;
	}

	auto function_info = reinterpret_cast<ScalarFunctionInfoHolder *>(env->GetDirectBufferAddress(function_info_buf));
	if (function_info == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid scalar function info");
		return nullptr;
	}
	return function_info;
}

static void scalar_function_init(cxx::ScalarFunction::InitInput &input) {
	input.SetInitData<ScalarFunctionExecState>();
}

static void scalar_function_exec(cxx::ScalarFunction::ExecInput &input) {
	auto callback_holder = input.GetUserData<ScalarFunctionUserData>().callback.get();
	if (callback_holder == nullptr) {
		throw cxx_invalid_input_exception("Scalar function callback not found");
	}

	// Same as in C API v1, the input vectors are flattened, the input chunk is created once per thread
	// and references the argument vectors
	auto &state = input.GetInitData<ScalarFunctionExecState>();
	idx_t arg_count = input.GetArgCount();
	if (!state.input) {
		std::vector<cxx::LogicalType> types;
		types.reserve(arg_count);
		for (idx_t i = 0; i < arg_count; i++) {
			types.emplace_back(input.GetArg(i).GetLogicalType());
		}
		// a chunk without columns cannot be created, the input of a function without arguments has only the row count
		auto chunk = arg_count > 0 ? cxx::DataChunk(input.GetContext().GetFactory(), types)
		                           : cxx_wrap<cxx::DataChunk>(nullptr, false);
		state.input = std::unique_ptr<DataChunkHolder>(new DataChunkHolder(std::move(chunk)));
		state.input->has_row_count = true;
	}
	for (idx_t i = 0; i < arg_count; i++) {
		auto arg = input.GetArg(i);
		arg.Flatten();
		state.input->chunk.GetVector(i).Reference(arg);
	}
	state.input->row_count = input.GetRowCount();
	auto output = input.GetResult();

	ScalarFunctionInfoHolder info;
	AttachedJNIEnv attached = callback_holder->attach_current_thread();
	if (attached.env == nullptr) {
		throw cxx_invalid_input_exception("Unable to attach JNI environment");
	}
	jobject info_buf = make_ptr_buf(attached.env, &info);
	if (info_buf == nullptr) {
		throw cxx_invalid_input_exception("Unable to create function info buffer");
	}
	LocalRefHolder info_buf_holder(attached.env, info_buf);
	jobject input_buf = make_ptr_buf(attached.env, state.input.get());
	if (input_buf == nullptr) {
		throw cxx_invalid_input_exception("Unable to create input buffer");
	}
	LocalRefHolder input_buf_holder(attached.env, input_buf);
	jobject output_buf = make_ptr_buf(attached.env, cxx_raw_handle(output));
	if (output_buf == nullptr) {
		throw cxx_invalid_input_exception("Unable to create output buffer");
	}
	LocalRefHolder output_buf_holder(attached.env, output_buf);

	attached.env->CallVoidMethod(callback_holder->global_ref, J_DuckDBScalarFunctionWrapper_execute, info_buf,
	                             input_buf, output_buf);
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
static jint register_scalar_function(cxx::Connection &conn, ScalarFunctionSpec &spec) {
	if (spec.name.empty() || !spec.has_function) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}
	auto return_type_id = logical_type_to_capi(spec.return_type);
	if (return_type_id == capi_type::INVALID || return_type_id == capi_type::ANY) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}
	for (auto &param : spec.parameters) {
		if (!param) {
			return static_cast<jint>(CAPI_STATE_ERROR);
		}
	}

	try {
		auto function = cxx::ScalarFunction::Create(conn.GetFactory());
		function.SetName(spec.name);
		{
			auto signature = function.GetSignature();
			// C API v1 declares no parameter names, the parameters are positional-only and are named "colN"
			for (size_t i = 0; i < spec.parameters.size(); i++) {
				signature.AddParameter("col" + std::to_string(i), spec.parameters[i],
				                       cxx::FunctionParameterKind::POSITIONAL_ONLY);
			}
			// same as in C API v1, varargs also receive the named arguments that are not declared
			if (spec.varargs) {
				signature.AddArgs("args", spec.varargs);
				signature.AddKwargs("kwargs", spec.varargs);
			}
			signature.SetReturnType(spec.return_type);
		}
		if (spec.is_volatile) {
			function.SetStability(cxx::FunctionStability::VOLATILE);
		}
		if (spec.special_handling) {
			function.SetNullHandling(cxx::FunctionNullHandling::SPECIAL);
		}
		function.SetInitCallback(scalar_function_init);
		function.SetExecCallback(scalar_function_exec);
		// same as the "extra info" in C API v1, the callback is owned by the registered function
		function.SetUserData<ScalarFunctionUserData>(std::move(spec.callback));
		conn.Register(function);
	} catch (const std::exception &) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}

	return static_cast<jint>(CAPI_STATE_SUCCESS);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_create_scalar_function
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1create_1scalar_1function(JNIEnv *env, jclass) {
	auto function = new ScalarFunctionSpec();
	jobject buf = make_ptr_buf(env, function);
	if (buf == nullptr) {
		delete function;
	}
	return buf;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_destroy_scalar_function
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1destroy_1scalar_1function(JNIEnv *env, jclass,
                                                                                        jobject scalar_function) {
	auto function = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return;
	}
	delete function;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_scalar_function_set_name
 * Signature: (Ljava/nio/ByteBuffer;[B)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1scalar_1function_1set_1name(JNIEnv *env, jclass,
                                                                                          jobject scalar_function,
                                                                                          jbyteArray name) {
	auto function = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return;
	}
	if (name == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid scalar function name");
		return;
	}
	auto function_name = jbyteArray_to_string(env, name);
	if (env->ExceptionCheck()) {
		return;
	}
	function->name = function_name;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_scalar_function_add_parameter
 * Signature: (Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1scalar_1function_1add_1parameter(JNIEnv *env, jclass,
                                                                                               jobject scalar_function,
                                                                                               jobject logical_type) {
	auto function = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return;
	}
	auto type = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return;
	}
	function->parameters.emplace_back(copy_logical_type(*type));
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_scalar_function_set_return_type
 * Signature: (Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1scalar_1function_1set_1return_1type(
    JNIEnv *env, jclass, jobject scalar_function, jobject logical_type) {
	auto function = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return;
	}
	auto type = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return;
	}
	function->return_type = copy_logical_type(*type);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_scalar_function_set_varargs
 * Signature: (Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1scalar_1function_1set_1varargs(JNIEnv *env, jclass,
                                                                                             jobject scalar_function,
                                                                                             jobject logical_type) {
	auto function = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return;
	}
	auto type = logical_type_buf_to_logical_type(env, logical_type);
	if (env->ExceptionCheck()) {
		return;
	}
	function->varargs = copy_logical_type(*type);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_scalar_function_set_volatile
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1scalar_1function_1set_1volatile(JNIEnv *env, jclass,
                                                                                              jobject scalar_function) {
	auto function = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return;
	}
	function->is_volatile = true;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_scalar_function_set_special_handling
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1scalar_1function_1set_1special_1handling(
    JNIEnv *env, jclass, jobject scalar_function) {
	auto function = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return;
	}
	function->special_handling = true;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_register_scalar_function
 * Signature: (Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1register_1scalar_1function(JNIEnv *env, jclass,
                                                                                         jobject connection,
                                                                                         jobject scalar_function) {
	auto conn = conn_ref_buf_to_conn(env, connection);
	if (env->ExceptionCheck()) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}
	auto function = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return static_cast<jint>(CAPI_STATE_ERROR);
	}

	return register_scalar_function(*conn, *function);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_scalar_function_set_extra_info
 * Signature: (Ljava/nio/ByteBuffer;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1scalar_1function_1set_1extra_1info(
    JNIEnv *env, jclass, jobject scalar_function, jobject callback) {

	if (callback == nullptr) {
		env->ThrowNew(J_SQLException, "Specified callback must be not null");
		return;
	}

	auto sf = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return;
	}

	auto callback_holder = std::unique_ptr<GlobalRefHolder>(new GlobalRefHolder(env, callback));
	if (callback_holder->vm == nullptr) {
		env->ThrowNew(J_SQLException, "Unable to create a global reference to the specified scalar function callback");
		return;
	}

	sf->callback = std::move(callback_holder);
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_scalar_function_set_function
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1scalar_1function_1set_1function(JNIEnv *env, jclass,
                                                                                              jobject scalar_function) {
	auto sf = scalar_function_buf_to_scalar_function(env, scalar_function);
	if (env->ExceptionCheck()) {
		return;
	}

	sf->has_function = true;
}

/*
 * Class:     org_duckdb_DuckDBBindings
 * Method:    duckdb_scalar_function_set_error
 * Signature: (Ljava/nio/ByteBuffer;[B)V
 */
JNIEXPORT void JNICALL Java_org_duckdb_DuckDBBindings_duckdb_1scalar_1function_1set_1error(JNIEnv *env, jclass,
                                                                                           jobject function_info_buf,
                                                                                           jbyteArray error) {
	auto function_info = function_info_buf_to_function_info(env, function_info_buf);
	if (env->ExceptionCheck()) {
		return;
	}
	if (error == nullptr) {
		env->ThrowNew(J_SQLException, "Invalid scalar function error");
		return;
	}
	auto error_message = jbyteArray_to_string(env, error);
	if (env->ExceptionCheck()) {
		return;
	}
	function_info->has_error = true;
	function_info->error = error_message;
}
