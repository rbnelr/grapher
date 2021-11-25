#pragma once
#include "common.hpp"
#include "tokenize.hpp"
#include <stack>

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
	0, // T_PLUS,
	0, // T_MINUS,
	1, // T_MULTIPLY,
	1, // T_DIVIDE,
	2, // T_POWER,
};

enum Associativity : uint8_t {
	LEFT_ASSOC=0,
	RIGHT_ASSOC=1,
};
inline constexpr Associativity BINARY_OP_ASSOCIATIVITY[] = { // 0 = left (left to right execution)  1 = right
	 LEFT_ASSOC, // T_PLUS,
	 LEFT_ASSOC, // T_MINUS,
	 LEFT_ASSOC, // T_MULTIPLY,
	 LEFT_ASSOC, // T_DIVIDE,
	RIGHT_ASSOC, // T_POWER,
};

inline int get_binary_op_precedence (TokenType tok) {
	assert(is_binary_op(tok));
	return BINARY_OP_PRECEDENCE[tok - T_PLUS];
}
inline int get_binary_op_associativity (TokenType tok) {
	assert(is_binary_op(tok));
	return (bool)BINARY_OP_ASSOCIATIVITY[tok - T_PLUS];
}

struct Operation {
	OPType           code;

	union {
		float        value;
		int          argc;
	};

	std::string_view text;

	Operation () {}
	Operation (OPType code, Token& tok_for_text):
		code{code},
		text{(std::string_view)tok_for_text} {}
};

struct ASTNode;
typedef std::unique_ptr<ASTNode> ast_ptr;

struct ASTNode {
	OPType           type;

	union {
		float        value;
		int          argc;
	};

	std::string_view text;

	ast_ptr          next;
	ast_ptr          child; // first child node, following are linked via next pointer
};

ast_ptr ast_node (OPType type, Token& tok_for_text) {
	ast_ptr node = std::make_unique<ASTNode>();
	node->type = type;
	node->text = (std::string_view)tok_for_text;
	return node;
}

// Recursive decent parsing with precedence climbing
// with a little help from https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing

inline ast_ptr expression (Token*& tok, std::string* last_err, int min_prec = 0);

//    a single value                  ex. 5.3  or  x
// or an expression in parentheses    ex. (-x^3 + 5)
// or a function call                 ex. abs(x+3)
// any of these can be preceded by a unary minus  -5.3 or -x
inline ast_ptr atom (Token*& tok, std::string* last_err) {
	ast_ptr unary_minus = nullptr;
	if      (tok->type == T_MINUS) unary_minus = ast_node(OP_UNARY_NEGATE, *tok++);
	else if (tok->type == T_PLUS ) tok++; // skip, since unary plus is no-op

	ast_ptr result;
	if (tok->type == T_PAREN_OPEN) {
		tok++;

		result = expression(tok, last_err);
		if (!result) return nullptr;

		if (tok->type != T_PAREN_CLOSE) {
			*last_err = "syntax error, ')' expected!";
			return nullptr;
		}
		tok++;
	}
	else if (tok[0].type == T_IDENTIFIER && tok[1].type == T_PAREN_OPEN) {
		// function call
		ast_ptr funccall = ast_node(OP_FUNCCALL, *tok++);
		tok++; // T_PAREN_OPEN

		ast_ptr* arg_ptr = &funccall->child;

		funccall->argc = 0;
		for (;;) {
			*arg_ptr = expression(tok, last_err);
			if (!*arg_ptr) return nullptr;

			funccall->argc++;
			arg_ptr = &(*arg_ptr)->next;

			if (!(tok->type == T_COMMA || tok->type == T_PAREN_CLOSE)) {
				*last_err = "syntax error, ',' or ')' expected!";
				return nullptr;
			}
			if ((*tok++).type == T_PAREN_CLOSE)
				break;
		}
	}
	else {
		OPType type;
		if      (tok->type == T_LITERAL   ) type = OP_VALUE;
		else if (tok->type == T_IDENTIFIER) type = OP_VARIABLE;
		else {
			*last_err = "syntax error, number or variable expected!";
			return nullptr;
		}
		result = ast_node(type, *tok++);
		result->value = tok->value;
	}

	if (unary_minus) {
		unary_minus->child = std::move(result);
		return unary_minus;
	}
	return result;
}

// a series of atoms seperated by binary operators (of precedence higher or equal than min_prec)
// ex. -x^(y+3) + 5
// note that the (y+3) is an atom, which happens to be a sub-expression
// expression calls itself recursively with increasing min_precedences to generate operators in the correct order (precedence climbing algorithm)
inline ast_ptr expression (Token*& tok, std::string* last_err, int min_prec) {

	ast_ptr lhs = atom(tok, last_err);
	if (!lhs) return nullptr;

	for (;;) {
		if (!is_binary_op(tok->type))
			break;

		int  prec  = get_binary_op_precedence(   tok->type);
		auto assoc = get_binary_op_associativity(tok->type);

		if (prec < min_prec)
			break;

		ast_ptr op = ast_node((OPType)(tok->type + (OP_ADD-T_PLUS)), *tok++);

		ast_ptr rhs = expression(tok, last_err, assoc == LEFT_ASSOC ? prec+1 : prec);
		if (!rhs) return nullptr;

		lhs->next = std::move(rhs);
		op->child = std::move(lhs);

		lhs = std::move(op);
	}

	return lhs;
}

inline bool parse_equation (Token*& tok, std::vector<Operation>& ops, std::string* last_err) {
	ast_ptr root = expression(tok, last_err);
	if (!root) return false;
	
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
