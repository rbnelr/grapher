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

	std::string_view text;
	union {
		float        value;
		int          argc;
	};

	Operation () {}
	Operation (OPType code, Token& tok_for_text):
		code{code},
		text{(std::string_view)tok_for_text} {}
};

struct ASTNode {
	OPType           type;

	std::string_view text;
	union {
		float        value;
		int          argc;
	};

	std::vector<ASTNode> nodes; // child nodes
};

#if 0
inline bool atom (Token*& tok, ASTNode& node, std::string* last_err);

inline bool expression (Token*& tok, ASTNode& node, std::string* last_err, int min_prec = 0) {

	ASTNode lhs;
	if (!atom(tok, lhs, last_err))
		return false;

	for (;;) {
		if (tok->type == T_EOI || tok->type == T_PAREN_CLOSE || tok->type == T_COMMA)
			break;

		if (!is_binary_op(tok->type)) {
			*last_err = "syntax error, binary operator expected!";
			return false;
		}

		int prec = get_binary_op_precedence(tok->type);
		if (prec < min_prec)
			break;

		auto assoc = get_binary_op_associativity(tok->type);
		int recurse_prec = assoc == LEFT_ASSOC ? prec+1 : prec;

		ASTNode op = {};
		op.type = (OPType)(tok->type - T_PLUS + OP_ADD);
		op.text = (std::string_view)*tok;

		tok++;

		ASTNode rhs;
		if (!expression(tok, rhs, last_err, recurse_prec))
			return false;

		op.nodes = { std::move(lhs), std::move(rhs) };
		lhs = std::move(op);
	}

	node = std::move(lhs);
	return true;
}

inline bool atom (Token*& tok, ASTNode& node, std::string* last_err) {
	Token* unary_negate = nullptr;
	if (tok->type == T_MINUS) {
		unary_negate = tok++;
	}
	else if (tok->type == T_PLUS) {
		tok++;
	}
	
	if (tok->type == T_PAREN_OPEN) {
		tok++;

		if (!expression(tok, node, last_err))
			return false;

		if (tok->type != T_PAREN_CLOSE) {
			*last_err = "syntax error, '(' not closed with ')'!";
			return false;
		}
		tok++;
	}
	else if (tok[0].type == T_IDENTIFIER && tok[1].type == T_PAREN_OPEN) {
		// function call
		node = {};
		node.type = OP_FUNCCALL;
		node.text = (std::string_view)*tok;
		node.argc = 0;

		tok += 2; // T_IDENTIFIER, T_PAREN_OPEN

		for (;;) {

			ASTNode arg_node;
			if (!expression(tok, arg_node, last_err))
				return false;

			node.nodes.emplace_back( std::move(arg_node) );
			node.argc++;

			if (!(tok->type == T_COMMA || tok->type == T_PAREN_CLOSE)) {
				*last_err = "syntax error, ',' or ')' expected!";
				return false;
			}
			if ((*tok++).type == T_PAREN_CLOSE)
				break;
		}
	}
	else {
		node = {};

		if      (tok->type == T_LITERAL   ) node.type = OP_VALUE;
		else if (tok->type == T_IDENTIFIER) node.type = OP_VARIABLE;
		else {
			*last_err = "syntax error, literal or variable expected!";
			return false;
		}

		node.text = (std::string_view)*tok;
		node.value = tok->value;

		tok++;
	}

	if (unary_negate) {
		auto expr = std::move(node);

		node = {};
		node.type = OP_UNARY_NEGATE;
		node.text = (std::string_view)*unary_negate;
		node.nodes = { std::move(expr) };
	}
	return true;
}
#endif

// Recursive decent parsing with precedence climbing
// with a little help from https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing

inline bool expression (Token*& tok, std::vector<Operation>& ops, std::string* last_err, int min_prec = 0);

//    a single value                  ex. 5.3  or  x
// or an expression in parentheses    ex. (-x^3 + 5)
// or a function call                 ex. abs(x+3)
// any of these can be preceded by a unary minus  -5.3 or -x
inline bool atom (Token*& tok, std::vector<Operation>& ops, std::string* last_err) {
	Token* unary_minus = nullptr;
	if      (tok->type == T_MINUS) unary_minus = tok++; // do a unary negate later
	else if (tok->type == T_PLUS ) tok++; // skip, since unary plus is no-op

	if (tok->type == T_PAREN_OPEN) {
		tok++;

		if (!expression(tok, ops, last_err))
			return false;

		if (tok->type != T_PAREN_CLOSE) {
			*last_err = "syntax error, '(' not closed with ')'!";
			return false;
		}
		tok++;
	}
	else if (tok[0].type == T_IDENTIFIER && tok[1].type == T_PAREN_OPEN) {
		// function call
		auto& funccall = *tok++;
		tok++; // T_PAREN_OPEN

		int argc = 0;
		for (;;) {
			if (!expression(tok, ops, last_err))
				return false;

			argc++;

			if (!(tok->type == T_COMMA || tok->type == T_PAREN_CLOSE)) {
				*last_err = "syntax error, ',' or ')' expected!";
				return false;
			}
			if ((*tok++).type == T_PAREN_CLOSE)
				break;
		}

		auto& op = ops.emplace_back( OP_FUNCCALL, funccall );
		op.argc = argc;
	}
	else {
		auto& op = ops.emplace_back();

		if      (tok->type == T_LITERAL   ) op.code = OP_VALUE;
		else if (tok->type == T_IDENTIFIER) op.code = OP_VARIABLE;
		else {
			*last_err = "syntax error, literal or variable expected!";
			return false;
		}

		op.text = (std::string_view)*tok;
		op.value = tok->value;

		tok++;
	}

	if (unary_minus) {
		ops.emplace_back( OP_UNARY_NEGATE, *unary_minus );
	}
	return true;
}

// a series of atoms seperated by binary operators (of precedence higher or equal than min_prec)
// ex. -x^(y+3) + 5
// note that the (y+3) is an atom, which happens to be a sub-expression
// expression calls itself recursively with increasing min_precedences to generate operators in the correct order (precedence climbing algorithm)
inline bool expression (Token*& tok, std::vector<Operation>& ops, std::string* last_err, int min_prec) {

	if (!atom(tok, ops, last_err))
		return false;

	for (;;) {
		if (tok->type == T_EOI || tok->type == T_PAREN_CLOSE || tok->type == T_COMMA)
			break;

		if (!is_binary_op(tok->type)) {
			*last_err = "syntax error, binary operator expected!";
			return false;
		}

		int  prec  = get_binary_op_precedence(   tok->type);
		auto assoc = get_binary_op_associativity(tok->type);

		if (prec < min_prec)
			break;

		Token& op = *tok++;

		if (!expression(tok, ops, last_err, assoc == LEFT_ASSOC ? prec+1 : prec))
			return false;

		ops.emplace_back( (OPType)(op.type - T_PLUS + OP_ADD), op );
	}

	return true;
}

inline bool parse_equation (Token*& tok, std::vector<Operation>& ops, std::string* last_err) {
	//ASTNode root;
	if (!expression(tok, ops, last_err))
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
