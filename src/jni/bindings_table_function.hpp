#pragma once

#include "bindings.hpp"
#include "holders.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/*
 * Bind data, global init data and local init data of a table function, holds a global reference to the Java object.
 * Same as in C API v1, the Java object is closed when the state is destroyed.
 */
struct TableFunctionState {
	explicit TableFunctionState(GlobalRefHolder *holder_p) : holder(holder_p) {
	}

	~TableFunctionState() noexcept {
		GlobalRefHolder::destroy_table_function_state(holder);
	}

	TableFunctionState(const TableFunctionState &) = delete;

	TableFunctionState &operator=(const TableFunctionState &) = delete;

	jobject get_object() const {
		return holder != nullptr ? holder->global_ref : nullptr;
	}

	GlobalRefHolder *release() {
		auto res = holder;
		holder = nullptr;
		return res;
	}

	GlobalRefHolder *holder;
};

/*
 * User data of the registered C++ API table function, the same as "extra info" in C API v1.
 */
struct TableFunctionUserData {
	std::string name;
	std::unique_ptr<GlobalRefHolder> callback;
	std::vector<std::pair<std::string, cxx::LogicalType>> named_parameters;
	bool has_local_init = false;
};

/*
 * Bind info that is passed to Java during the bind call.
 */
struct TableFunctionBindInfoHolder {
	TableFunctionBindInfoHolder(cxx::TableFunction::BindInput &input_p, TableFunctionUserData &user_data_p)
	    : input(input_p), user_data(user_data_p) {
	}

	cxx::TableFunction::BindInput &input;
	TableFunctionUserData &user_data;
	//! The named parameters passed to the function, cast to the declared types
	std::vector<std::pair<std::string, cxx::Value>> named_parameters;
	std::unique_ptr<TableFunctionState> bind_data;
	bool has_error = false;
	std::string error;
};

/*
 * Init info that is passed to Java during the global and local init calls.
 */
struct TableFunctionInitInfoHolder {
	cxx::TableFunction::InitGlobalInput *global_input = nullptr;
	cxx::TableFunction::InitLocalInput *local_input = nullptr;
	std::unique_ptr<TableFunctionState> init_data;
	bool has_error = false;
	std::string error;
};

/*
 * Function info that is passed to Java during the function call.
 */
struct TableFunctionInfoHolder {
	TableFunctionInfoHolder(cxx::TableFunction::ExecInput &input_p, bool has_local_init_p)
	    : input(input_p), has_local_init(has_local_init_p) {
	}

	cxx::TableFunction::ExecInput &input;
	bool has_local_init;
	bool has_error = false;
	std::string error;
};
