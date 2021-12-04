#pragma once
#include "common.hpp"
#include "parse.hpp"

float mypow (float a, float b) {
	//if (b == 2.0f) return a*a;
	//if (b == 3.0f) return a*a*a;
	//if (b == 4.0f) return (a*a)*(a*a);

	return powf(a, b);
}
float mymod (float a, float b) {
	float val = fmodf(a, b);
	//if (b > 0.0f) if (a < 0.0f) val += b;
	//else          if (a > 0.0f) val += b;
	//
	if (a*b < 0.0f) // differing sign
		val += b;
	return val;
}

#define ARGCHECK(funcname, expected_argc) do { \
	if (argc != expected_argc) { \
		*errstr = funcname " takes " TO_STRING(expected_argc) " argument!"; \
		return false; \
	} \
} while (false)

struct ExecState;

struct DegreeMode {
	// if axis X in deg mode -> (2*PI)/360 else 1
	// -> for  angle -> scalar  funcs (eg. sin)
	float from_deg_x;
	// if axis Y in deg mode -> 360/(2*PI) else 1
	// -> for  scalar -> angle  funcs (eg. asin)
	float   to_deg_y;
};

struct ExecState {

	DegreeMode deg_mode;

	// the inputs of this function evaluation
	std::unordered_map<std::string_view, int>* arg_map;
	std::vector<float> arg_values;
	
	// variables that are constant over the function
	std::unordered_map<std::string_view, float> var_values;

	bool lookup_var (std::string_view const& name, float* value) {
		auto arg_i = arg_map->find(name);
		if (arg_i != arg_map->end()) {
			assert(arg_i->second >= 0 && arg_i->second < (int)arg_values.size());
			*value = arg_values[arg_i->second];
			return true;
		}
		
		auto var = var_values.find(name);
		if (var != var_values.end()) {
			*value = var->second;
			return true;
		}
		
		return false;
	}
};

inline bool exec_sqrt  (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("sqrt", 1);
	*result = sqrtf(args[0]);
	return true;
}
inline bool exec_abs   (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("abs", 1);
	*result = fabsf(args[0]);
	return true;
}

inline bool exec_mod   (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("mod", 2);
	*result = mymod(args[0], args[1]);
	return true;
}
inline bool exec_floor (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("floor", 1);
	*result = floorf(args[0]);
	return true;
}
inline bool exec_ceil  (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("ceil", 1);
	*result = ceilf(args[0]);
	return true;
}
inline bool exec_round (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("round", 1);
	*result = roundf(args[0]);
	return true;
}

inline bool exec_min   (int argc, float* args, float* result, const char** errstr) {
	if (argc < 2) {
		*errstr = "min() takes at least 2 argument!";
		return false;
	}

	float minf = args[0];
	for (int i=1; i<argc; ++i)
		minf = min(minf, args[i]);

	*result = minf;
	return true;
}
inline bool exec_max   (int argc, float* args, float* result, const char** errstr) {
	if (argc < 2) {
		*errstr = "max() takes at least 2 argument!";
		return false;
	}

	float maxf = args[0];
	for (int i=1; i<argc; ++i)
		maxf = max(maxf, args[i]);

	*result = maxf;
	return true;
}
inline bool exec_clamp (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("clamp", 3);
	*result = clamp(args[0], args[1], args[2]);
	return true;
}

inline bool exec_sin  (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("sin", 1);
	*result = sinf(args[0] * deg.from_deg_x); // assume args[0] comes from x axis for now 
	return true;
}
inline bool exec_cos  (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("cos", 1);
	*result = cosf(args[0] * deg.from_deg_x);
	return true;
}
inline bool exec_tan  (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("tan", 1);
	*result = tanf(args[0] * deg.from_deg_x);
	return true;
}
inline bool exec_asin (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("asin", 1);
	*result = asinf(args[0]) * deg.to_deg_y; // assume result goes to y axis for now 
	return true;
}
inline bool exec_acos (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("acos", 1);
	*result = acosf(args[0]) * deg.to_deg_y;
	return true;
}
inline bool exec_atan (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("atan", 1);
	*result = atanf(args[0]) * deg.to_deg_y;
	return true;
}

typedef bool (*std_function) (int argc, float* args, float* result, const char** errstr);
typedef bool (*std_angle_function) (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr);

struct Function {
	void* func_ptr;
	bool angle_func = false;
};

std::unordered_map<std::string_view, Function> std_functions {
	{ "sqrt",  { (void*)&exec_sqrt  } },
	{ "abs",   { (void*)&exec_abs   } },
	{ "min",   { (void*)&exec_min   } },
	{ "max",   { (void*)&exec_max   } },
	{ "clamp", { (void*)&exec_clamp } },
	{ "mod",   { (void*)&exec_mod   } },
	{ "floor", { (void*)&exec_floor } },
	{ "ceil",  { (void*)&exec_ceil  } },
	{ "round", { (void*)&exec_round } },

	{ "sin",   { (void*)&exec_sin  , true } },
	{ "cos",   { (void*)&exec_cos  , true } },
	{ "tan",   { (void*)&exec_tan  , true } },
	{ "asin",  { (void*)&exec_asin , true } },
	{ "acos",  { (void*)&exec_acos , true } },
	{ "atan",  { (void*)&exec_atan , true } },
};

inline bool call_const_func (Operation& op, float* args, float* result, const char** errstr) {
	auto it = std_functions.find(op.text);
	if (it == std_functions.end() || it->second.angle_func)
		return false;

	auto func = (std_function)it->second.func_ptr;
	return func(op.argc, args, result, errstr);
}

inline bool call_function (ExecState const& state, Operation& op, float* args, float* result, const char** errstr) {
	auto it = std_functions.find(op.text);
	if (it != std_functions.end()) {

		if (it->second.angle_func) {
			auto func = (std_angle_function)it->second.func_ptr;
			return func(state.deg_mode, op.argc, args, result, errstr);
		} else {
			auto func = (std_function)it->second.func_ptr;
			return func(op.argc, args, result, errstr);
		}
	}

	*errstr = "unknown function!";
	return false;
}

bool execute (ExecState& state, std::vector<Operation>& ops, float* result, std::string* last_err) {
	static constexpr int STACK_SIZE = 64;

	float stack[STACK_SIZE];
	int stack_idx = 0;

	const char* errstr;

	auto op_it  = ops.begin();
	auto op_end = ops.end();
	for (; op_it != op_end; ++op_it) {
		auto& op = *op_it;

		float value;
		switch (op.code) {
			case OP_VALUE: {
				value = op.value;
			} break;

			case OP_VARIABLE: {
				if (!state.lookup_var(op.text, &value)) {
					errstr = "lookup_var() failed!";
					goto error;
				}
			} break;

			case OP_FUNCCALL: {
				if (stack_idx < op.argc) {
					errstr = "stack underflow!";
					goto error;
				}

				stack_idx -= op.argc;
				if (!call_function(state, op, &stack[stack_idx], &value, &errstr))
					goto error;

			} break;

			case OP_UNARY_NEGATE: {
				//assert(stack_idx >= 1);
				if (stack_idx < 1) {
					errstr = "stack underflow!";
					goto error;
				}

				stack_idx -= 1;
				float a = stack[stack_idx];

				value = -a;
			} break;

			case OP_ADD       :
			case OP_SUBSTRACT :
			case OP_MULTIPLY  :
			case OP_DIVIDE    :
			case OP_POW       : {
				//assert(stack_idx >= 2);
				if (stack_idx < 2) {
					errstr = "stack underflow!";
					goto error;
				}

				stack_idx -= 2;
				float a = stack[stack_idx];
				float b = stack[stack_idx+1];

				switch (op.code) {
					case OP_ADD       : value = a + b; break;
					case OP_SUBSTRACT : value = a - b; break;
					case OP_MULTIPLY  : value = a * b; break;
					case OP_DIVIDE    : value = a / b; break;
					case OP_POW       : value = mypow(a, b); break;
					default: assert(false);
				}
			} break;

			default: {
				errstr = "unknown op type!";
				goto error;
			}
		}

		//assert(stack_idx < STACK_SIZE);
		if (stack_idx >= STACK_SIZE) {
			errstr = "stack overflow!";
			goto error;
		}

		stack[stack_idx++] = value;
	}

	assert(stack_idx == 1);
	*result = stack[0];
	return true;

error:
	auto& code = *op_it;
	*last_err = prints("%s\nop: %s text: ", errstr, OPType_str[code.code]) + code.text;
	return false;
}

// eval as a string to quickly debug execution
bool execute_str (std::vector<Operation>& ops, std::string* result) {
	ZoneScoped;

	std::vector<std::string> stack;

	for (auto& op : ops) {
		std::string value;
		switch (op.code) {
			case OP_VALUE: {
				value = prints("%g", op.value);
			} break;

			case OP_VARIABLE: {
				value = (std::string)op.text;
			} break;

			case OP_FUNCCALL: {
				if (stack.size() < op.argc) {
					return false;
				}

				std::string args = "(";
				for (int i=0; i<op.argc; ++i) {
					if (i > 0)
						args += ",";
					args += std::move(stack[stack.size() - op.argc + i]);
				}
				args += ")";

				stack.resize(stack.size() - op.argc);

				value = op.text + args;

			} break;

			case OP_UNARY_NEGATE: {
				std::string a = std::move(stack[stack.size() - 1]);
				stack.resize(stack.size() - 1);

				value = prints("-(%s)", a.c_str());
			} break;

			case OP_ADD       :
			case OP_SUBSTRACT :
			case OP_MULTIPLY  :
			case OP_DIVIDE    :
			case OP_POW       : {
				assert(stack.size() >= 2);
				std::string a = std::move(stack[stack.size() - 2]);
				std::string b = std::move(stack[stack.size() - 1]);
				stack.resize(stack.size() - 2);

				switch (op.code) {
					case OP_ADD       : value = prints("(%s + %s)", a.c_str(), b.c_str()); break;
					case OP_SUBSTRACT : value = prints("(%s - %s)", a.c_str(), b.c_str()); break;
					case OP_MULTIPLY  : value = prints("(%s * %s)", a.c_str(), b.c_str()); break;
					case OP_DIVIDE    : value = prints("(%s / %s)", a.c_str(), b.c_str()); break;
					case OP_POW       : value = prints("(%s ^ %s)", a.c_str(), b.c_str()); break;
				}
			} break;

			default: {
				return false;
			}
		}

		stack.push_back(std::move(value));
	}
	assert(stack.size() == 1);
	*result = std::move(stack[0]);

	return true;
}
