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

struct ExecState {
	
	struct Variables {
		float x;

		bool lookup (std::string_view const& name, float* out) {
			if (name == "x") {
				*out = x;
				return true;
			}

			return false;
		}
	};

	Variables vars;

	// if axis X in deg mode -> (2*PI)/360 else 1
	// -> for  angle -> scalar  funcs (eg. sin)
	float from_deg_x;
	// if axis Y in deg mode -> 360/(2*PI) else 1
	// -> for  scalar -> angle  funcs (eg. asin)
	float   to_deg_y;
};

#define ARGCHECK(funcname, expected_argc) \
	if (argc != expected_argc) { \
		*errstr = funcname " takes " TO_STRING(expected_argc) " argument!"; \
		return false; \
	}

inline bool exec_sqrt (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("sqrt", 1)
	*result = sqrtf(args[0]);
	return true;
}
inline bool exec_abs (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("abs", 1)
	*result = fabsf(args[0]);
	return true;
}

inline bool exec_mod (int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("mod", 2)
	*result = mymod(args[0], args[1]);
	return true;
}

inline bool exec_min (int argc, float* args, float* result, const char** errstr) {
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
inline bool exec_max (int argc, float* args, float* result, const char** errstr) {
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
	ARGCHECK("clamp", 3)
	*result = clamp(args[0], args[1], args[2]);
	return true;
}

inline bool exec_sin (ExecState& state, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("sin", 1)
	*result = sinf(args[0] * state.from_deg_x); // assume args[0] comes from x axis for now 
	return true;
}
inline bool exec_cos (ExecState& state, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("cos", 1)
	*result = cosf(args[0] * state.from_deg_x);
	return true;
}
inline bool exec_tan (ExecState& state, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("tan", 1)
	*result = tanf(args[0] * state.from_deg_x);
	return true;
}
inline bool exec_asin (ExecState& state, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("asin", 1)
	*result = asinf(args[0]) * state.to_deg_y; // assume result goes to y axis for now 
	return true;
}
inline bool exec_acos (ExecState& state, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("acos", 1)
	*result = acosf(args[0]) * state.to_deg_y;
	return true;
}
inline bool exec_atan (ExecState& state, int argc, float* args, float* result, const char** errstr) {
	ARGCHECK("atan", 1)
	*result = atanf(args[0]) * state.to_deg_y;
	return true;
}


inline bool call_func (ExecState& state, Operation& op, float* args, float* result, const char** errstr) {
	
	if      (op.text == "sqrt")  return exec_sqrt (       op.argc, args, result, errstr);
	else if (op.text == "abs")   return exec_abs  (       op.argc, args, result, errstr);
	
	else if (op.text == "min")   return exec_min  (       op.argc, args, result, errstr);
	else if (op.text == "max")   return exec_max  (       op.argc, args, result, errstr);
	else if (op.text == "clamp") return exec_clamp(       op.argc, args, result, errstr);

	else if (op.text == "sin")   return exec_sin  (state, op.argc, args, result, errstr);
	else if (op.text == "cos")   return exec_cos  (state, op.argc, args, result, errstr);
	else if (op.text == "tan")   return exec_tan  (state, op.argc, args, result, errstr);
	else if (op.text == "asin")  return exec_asin (state, op.argc, args, result, errstr);
	else if (op.text == "acos")  return exec_acos (state, op.argc, args, result, errstr);
	else if (op.text == "atan")  return exec_atan (state, op.argc, args, result, errstr);

	else if (op.text == "mod")   return exec_mod  (       op.argc, args, result, errstr);

	else {
		*errstr = "unknown function!";
		return false;
	}
}
inline bool call_const_func (Operation& op, float* args, float* result, const char** errstr) {

	if      (op.text == "sqrt")  return exec_sqrt (       op.argc, args, result, errstr);
	else if (op.text == "abs")   return exec_abs  (       op.argc, args, result, errstr);

	else if (op.text == "min")   return exec_min  (       op.argc, args, result, errstr);
	else if (op.text == "max")   return exec_max  (       op.argc, args, result, errstr);
	else if (op.text == "clamp") return exec_clamp(       op.argc, args, result, errstr);

	// no trig functions due to rad / deg modes not allowing for constant folding

	else if (op.text == "mod")   return exec_mod  (       op.argc, args, result, errstr);

	else {
		*errstr = "unknown function!";
		return false;
	}
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
				if (!state.vars.lookup(op.text, &value)) {
					errstr = "vars.lookup() failed!";
					goto error;
				}
			} break;

			case OP_FUNCCALL: {
				if (stack_idx < op.argc) {
					errstr = "stack underflow!";
					goto error;
				}

				stack_idx -= op.argc;
				if (!call_func(state, op, &stack[stack_idx], &value, &errstr))
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
