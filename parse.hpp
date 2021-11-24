#pragma once
#include "common.hpp"
#include "tokenize.hpp"

enum OPType {
	OP_VALUE,        // push value
	OP_VARIABLE,     // push vars.lookup(varname)

	OP_FUNCCALL,     // push <argc> arguments, call function

	OP_ADD,          // pop a, pop b, push a+b
	OP_SUBSTRACT,    // pop a, pop b, push a-b
	OP_MULTIPLY,     // pop a, pop b, push a*b
	OP_DIVIDE,       // pop a, pop b, push a/b
	OP_POW,          // pop a, pop b, push a^b

	OP_UNARY_NEGATE, // pop a,        push -a
};
inline constexpr const char* OPType_str[] = {
	"OP_VALUE",
	"OP_VARIABLE",

	"OP_FUNCCALL",

	"OP_ADD",
	"OP_SUBSTRACT",
	"OP_MULTIPLY",
	"OP_DIVIDE",
	"OP_POW",

	"OP_UNARY_NEGATE",
};

inline constexpr bool is_binary_op (TokenType tok) {
	return tok >= T_PLUS && tok <= T_POWER;
}

inline constexpr uint8_t BINARY_OP_PRECEDENCE[] = {
	2, // T_PLUS,
	2, // T_MINUS,
	1, // T_MULTIPLY,
	1, // T_DIVIDE,
	0, // T_POWER,
};
inline constexpr int MAX_PRECEDENCE = 2;

inline constexpr uint8_t BINARY_RIGHT_TO_LEFT[] = {
	0, // T_PLUS,
	0, // T_MINUS,
	0, // T_MULTIPLY,
	0, // T_DIVIDE,
	1, // T_POWER,
};

inline int get_binary_op_precedence (TokenType tok) {
	assert(is_binary_op(tok));
	return BINARY_OP_PRECEDENCE[tok - T_PLUS];
}
inline int get_binary_op_right_to_left (TokenType tok) {
	assert(is_binary_op(tok));
	return (bool)BINARY_RIGHT_TO_LEFT[tok - T_PLUS];
}

struct Operation {
	OPType           code;

	std::string_view text;
	union {
		float        value;
		int          argc;
	};

	static Operation literal_or_variable (Token& tok) {
		Operation op = {};
		switch (tok.type) {
			case T_LITERAL    : op.code = OP_VALUE;    break;
			case T_IDENTIFIER : op.code = OP_VARIABLE; break;
			default: {
				assert(false);
			}
		}
		op.text = (std::string_view)tok;
		op.value = tok.value;
		return op;
	}
	static Operation binary_op (Token& tok) {
		Operation op = {};
		op.code = (OPType)(tok.type - T_PLUS + OP_ADD);
		op.text = (std::string_view)tok;
		return op;
	}
	static Operation func_call (Token& tok, int argc) {
		Operation op = {};
		op.code = OP_FUNCCALL;
		op.text = (std::string_view)tok;
		op.argc = argc;
		return op;
	}
};

inline bool value_expression (Token*& tok, std::vector<Operation>& ops, std::string* last_err);

inline bool recurse_subexpression (Token*& tok, std::vector<Operation>& ops, std::string* last_err, int precedence) {

	for (;;) {
		if (tok->type == T_EOI || tok->type == T_PAREN_CLOSE || tok->type == T_COMMA)
			return true;

		if (!is_binary_op(tok[0].type)) {
			*last_err = "syntax error, binary operator expected!";
			return false;
		}

		int op_prec = get_binary_op_precedence(tok[0].type);

		if (op_prec > precedence) {
			// operator has higher precedence than recursion level, return to handle operator there
			return true;
		} else if (op_prec < precedence) {
			// right hand operator has lower precedence, recurse into next precedence level
			if (!recurse_subexpression(tok, ops, last_err, precedence - 1))
				return false;
		} else {
			// operator precedence matches recursion level, handle operand-operator pair here

			auto& op = *tok++;

			if (!value_expression(tok, ops, last_err))
				return false;

			auto& op2 = *tok;

			if (is_binary_op(op2.type)) {
				int recurse_precedence = -1;

				int op2_prec = get_binary_op_precedence(op2.type);
				if (op2_prec < precedence) {
					// next operator has lower precedence than recursion level
					recurse_precedence = precedence - 1;
				} if (op2_prec == precedence && op.type == op2.type && get_binary_op_right_to_left(op2.type)) {
					// next operator is the same as the current, but is a right-to-left operator
					recurse_precedence = precedence;
				}

				if (recurse_precedence != -1) {
					// recurse to let it insert it's ops before our operator op
					if (!recurse_subexpression(tok, ops, last_err, precedence))
						return false; // propagate error
				}
			}

			ops.push_back( Operation::binary_op(op) );
		}
	}
}

inline bool subexpression (Token*& tok, std::vector<Operation>& ops, std::string* last_err) {
	if (!value_expression(tok, ops, last_err))
		return false;

	if (!recurse_subexpression(tok, ops, last_err, MAX_PRECEDENCE))
		return false;

	return true;
}

// [-/+] ([value_expression] [recurse_subexpression])  ex. -(-5.0 + x^3)
// [-/+] [literal_or_variable]                         ex. -5.0  or  x
// [-/+] [func_ident]([value_expression] [recurse_subexpression], ...)  ex. func(-5.0, x, a*func((b))+c)
inline bool value_expression (Token*& tok, std::vector<Operation>& ops, std::string* last_err) {
	bool unary_negate = false;
	if (tok->type == T_MINUS || tok->type == T_PLUS) {
		unary_negate = tok->type == T_MINUS;
		tok++;
	}

	if (tok->type == T_PAREN_OPEN) {
		tok++;

		if (!subexpression(tok, ops, last_err))
			return false;

		if (tok->type != T_PAREN_CLOSE) {
			*last_err = "syntax error, '(' not closed with ')'!";
			return false;
		}
		tok++;
	}
	else if (tok[0].type == T_IDENTIFIER && tok[1].type == T_PAREN_OPEN) {
		// function call
		auto& funcname = *tok++;
		tok++; // T_PAREN_OPEN

		int argc = 0;
		for (;;) {

			if (!subexpression(tok, ops, last_err))
				return false;

			argc++;

			if (!(tok->type == T_COMMA || tok->type == T_PAREN_CLOSE)) {
				*last_err = "syntax error, ',' or ')' expected!";
				return false;
			}
			if ((*tok++).type == T_PAREN_CLOSE)
				break;
		}

		ops.push_back( Operation::func_call(funcname, argc) );
	}
	else {
		if (!(tok->type == T_LITERAL || tok->type == T_IDENTIFIER)) {
			*last_err = "syntax error, literal or variable expected!";
			return false;
		}

		ops.push_back( Operation::literal_or_variable(*tok++) );
	}

	if (unary_negate)
		ops.push_back({ OP_UNARY_NEGATE, "", 0 });
	return true;
}

inline bool parse_equation (Token*& tok, std::vector<Operation>& ops, std::string* last_err) {
	if (!subexpression(tok, ops, last_err))
		return false;

	if (tok->type == T_PAREN_CLOSE) {
		*last_err = "syntax error, ')' without matching '('!";
		return false;
	}
	if (tok->type != T_EOI) {
		*last_err = "syntax error, end of input expected!";
		return false;
	}

	return true;
}
