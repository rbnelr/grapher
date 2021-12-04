#pragma once
#include "common.hpp"
#include "parse.hpp"

#define ARGCHECK(funcname, expected_argc) do { \
	if (argc != expected_argc) { \
		return funcname " takes " TO_STRING(expected_argc) " argument!"; \
	} \
} while (false)

struct DegreeMode {
	// if axis X in deg mode -> (2*PI)/360 else 1
	// -> for  angle -> scalar  funcs (eg. sin)
	float from_deg_x;
	// if axis Y in deg mode -> 360/(2*PI) else 1
	// -> for  scalar -> angle  funcs (eg. asin)
	float   to_deg_y;
};

inline float mypow (float a, float b) {
	//if (b == 2.0f) return a*a;
	//if (b == 3.0f) return a*a*a;
	//if (b == 4.0f) return (a*a)*(a*a);

	return powf(a, b);
}
inline float mymod (float a, float b) {
	float val = fmodf(a, b);
	//if (b > 0.0f) if (a < 0.0f) val += b;
	//else          if (a > 0.0f) val += b;
	//
	if (a*b < 0.0f) // differing sign
		val += b;
	return val;
}

inline const char* exec_sqrt  (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("sqrt", 1);
	*result = sqrtf(args[0]);
	return nullptr;
}
inline const char* exec_abs   (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("abs", 1);
	*result = fabsf(args[0]);
	return nullptr;
}

inline const char* exec_mod   (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("mod", 2);
	*result = mymod(args[0], args[1]);
	return nullptr;
}
inline const char* exec_floor (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("floor", 1);
	*result = floorf(args[0]);
	return nullptr;
}
inline const char* exec_ceil  (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("ceil", 1);
	*result = ceilf(args[0]);
	return nullptr;
}
inline const char* exec_round (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("round", 1);
	*result = roundf(args[0]);
	return nullptr;
}

inline const char* exec_min   (int argc, float* args, float* result, const char** errstr) {
	if (argc < 2) return "min() takes at least 2 argument!";

	float minf = args[0];
	for (int i=1; i<argc; ++i)
		minf = min(minf, args[i]);

	*result = minf;
	return nullptr;
}
inline const char* exec_max   (int argc, float* args, float* result, const char** errstr) {
	if (argc < 2) return "max() takes at least 2 argument!";

	float maxf = args[0];
	for (int i=1; i<argc; ++i)
		maxf = max(maxf, args[i]);

	*result = maxf;
	return nullptr;
}
inline const char* exec_clamp (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("clamp", 3);
	*result = clamp(args[0], args[1], args[2]);
	return nullptr;
}

inline const char* exec_sin  (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("sin", 1);
	*result = sinf(args[0] * deg.from_deg_x); // assume args[0] comes from x axis for now 
	return nullptr;
}
inline const char* exec_cos  (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("cos", 1);
	*result = cosf(args[0] * deg.from_deg_x);
	return nullptr;
}
inline const char* exec_tan  (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("tan", 1);
	*result = tanf(args[0] * deg.from_deg_x);
	return nullptr;
}
inline const char* exec_asin (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("asin", 1);
	*result = asinf(args[0]) * deg.to_deg_y; // assume result goes to y axis for now 
	return nullptr;
}
inline const char* exec_acos (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("acos", 1);
	*result = acosf(args[0]) * deg.to_deg_y;
	return nullptr;
}
inline const char* exec_atan (DegreeMode const& deg, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("atan", 1);
	*result = atanf(args[0]) * deg.to_deg_y;
	return nullptr;
}

typedef const char* (*std_function) (int argc, float* args, float* result);
typedef const char* (*std_angle_function) (DegreeMode const& deg, int argc, float* args, float* result);

struct StdFunction {
	void* func_ptr;
	bool angle_func = false;
};
std::unordered_map<std::string_view, StdFunction> std_functions {
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

inline bool call_const_func (Operation& op, float* args, float* result) {
	auto it = std_functions.find(op.text);
	if (it == std_functions.end() || it->second.angle_func)
		return false;

	auto func = (std_function)it->second.func_ptr;
	return func(op.argc, args, result) != nullptr;
}


struct Evaluator {
	DegreeMode deg_mode;

	// variables that are constant over the function
	std::unordered_map<std::string_view, float> var_values;

	struct Function {
		EquationDef*                               def;
		std::vector<Operation>*                    ops;
	};
	std::unordered_map<std::string_view, Function> functions;

	int frame_ptr;
	int stack_ptr;

	static constexpr int STACK_SIZE = 64;
	union StackValue {
		float f;
		int   i;
	};
	StackValue stack[STACK_SIZE];

	bool lookup_arg (std::string_view const& name, float* value, EquationDef& funcdef) {
		auto arg_i = funcdef.arg_map.find(name);
		if (arg_i == funcdef.arg_map.end())
			return false;

		assert(arg_i->second >= 0 && arg_i->second < (int)funcdef.arg_map.size());
		assert(frame_ptr + arg_i->second < stack_ptr);

		*value = stack[frame_ptr + arg_i->second].f;

		return true;
	}
	bool lookup_var (std::string_view const& name, float* value) {
		auto var = var_values.find(name);
		if (var == var_values.end())
			return false;
		*value = var->second;
		return true;
	}

#define PUSH(val) \
	if (stack_ptr >= STACK_SIZE) return "stack overflow!"; \
	stack[stack_ptr++].f = (val);

#define POP(N) \
	if (stack_ptr < (N)) return "stack underflow!"; \
	stack_ptr -= (N)

	const char* call_function (Operation& op, float* result) {
		auto it = std_functions.find(op.text);
		if (it != std_functions.end()) {

			POP(op.argc);
			float* args = &stack[stack_ptr].f;

			if (it->second.angle_func) {
				auto func = (std_angle_function)it->second.func_ptr;
				return func(deg_mode, op.argc, args, result);
			} else {
				auto func = (std_function)it->second.func_ptr;
				return func(op.argc, args, result);
			}
		}

		auto funcit = functions.find(op.text);
		if (funcit != functions.end()) {
			auto& func = funcit->second;

			if (op.argc != (int)func.def->arg_map.size()) {
				return "function argument count does not match!";
			}

			int return_ptr = frame_ptr; // remember our stack frame
			frame_ptr = stack_ptr - op.argc; // stack frame of function is top of stack

			auto res = execute(*func.def, *func.ops);
			if (res) return res;

			frame_ptr = return_ptr; // return to our stack frame
			
			POP(1);
			*result = stack[stack_ptr].f;

			POP(op.argc);

			return nullptr;
		}

		return "unknown function!";
	}

	const char* execute (EquationDef& funcdef, std::vector<Operation>& ops) {
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
					if (lookup_arg(op.text, &value, funcdef)) {

					} else if (lookup_var(op.text, &value)) {

					} else {
						return "lookup_var() failed!";
					}
				} break;

				case OP_FUNCCALL: {
					auto err = call_function(op, &value);
					if (err) return err;

				} break;

				case OP_UNARY_NEGATE: {
					POP(1);;
					float a = stack[stack_ptr].f;

					value = -a;
				} break;

				case OP_ADD       :
				case OP_SUBSTRACT :
				case OP_MULTIPLY  :
				case OP_DIVIDE    :
				case OP_POW       : {
					POP(2);
					float a = stack[stack_ptr].f;
					float b = stack[stack_ptr+1].f;

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
					return "unknown op type!";
				}
			}

			PUSH(value);
		}

		return nullptr;
	}

	bool execute (EquationDef& funcdef, std::vector<Operation>& ops, float x, float* result, std::string* last_err) {
		int argc = (int)funcdef.arg_map.size();
		assert(argc <= 1);

		stack_ptr = 0;
		frame_ptr = 0;

		if (argc == 1)
			stack[stack_ptr++].f = x;

		const char* err = execute(funcdef, ops);
		if (err) {
			*last_err = err;
			return false;
		}

		assert(frame_ptr == 0);
		assert(stack_ptr == argc + 1);
		stack_ptr--;
		*result = stack[stack_ptr].f;

		return true;
	}
};

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
