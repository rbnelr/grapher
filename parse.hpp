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
};

struct ASTNode;

#define BUMP_ALLOCATOR 0
#if BUMP_ALLOCATOR
typedef ASTNode* ast_ptr;

struct BlockBumpAllocator {
	char* next = nullptr;
	char* end  = nullptr;

	static constexpr size_t BLOCK_SIZE = 16 * KB;
	std::vector<char*> blocks;

	// note: no T::ctor will be called!
	// use placement new yourself if needed
	// this allocator does not call ctors, since it can't call dtors later since individual item locations (and their types) are not remembered
	template <typename T>
	T* alloc () {
		// can't allocate anything larger than BLOCK_SIZE
		if (sizeof(T) > BLOCK_SIZE) {
			assert(false);
			return nullptr;
		}
		// go to next block if item does not fit onto current block
		if (next + sizeof(T) > end)
			next = nullptr;
		if (!next)
			alloc_block();

		// align next ptr where appropriate
		next = (char*)align_up((uintptr_t)next, alignof(T));

		// allocate by returning next and incrementing next past end of current item
		T* item = (T*)next;
		next += sizeof(T);

		return item;
	}

	void alloc_block () {
		auto* block = new char[BLOCK_SIZE];
		blocks.emplace_back(block);

		next = block;
		end  = block + BLOCK_SIZE;
	}
	BlockBumpAllocator () {
		blocks.reserve(32);
		alloc_block();
	}
	// BlockBumpAllocator dtor deallocs all allocated items as well
	~BlockBumpAllocator () {
		for (auto block : blocks)
			delete[] block;
	}
};

#define GET_AST_PTR(ptr) (ptr)
#else
typedef std::unique_ptr<ASTNode> ast_ptr;

struct BlockBumpAllocator {};

#define GET_AST_PTR(ptr) (ptr).get()
#endif

struct ASTNode {
	Operation        op;

	ast_ptr          next  = nullptr;
	ast_ptr          child = nullptr; // first child node, following are linked via next pointer
};

inline bool lookup_constant (std::string_view const& name, float* out) {
	static constexpr float PHI = 1.61803398874989484820f; // golden ratio

	if      (name == "pi")  *out = PI;
	else if (name == "tau") *out = TAU;
	else if (name == "e")   *out = EULER;
	else if (name == "phi") *out = PHI;
	else                    return false;
	return true;
}

struct EquationDef {
	// false: 'f(x) =' syntax  ->  callable via f(x), f will be a syntax error
	//  true: 'f    =' syntax  ->  can get value via f, f(x) will be a syntax error
	bool                            is_variable;

	std::string_view                name;
	std::vector< std::string_view > args;

	std::unordered_map<std::string_view, int> arg_map; // argument name to argument position map

	void create_arg_map () {
		for (int i=0; i<(int)args.size(); ++i) {
			arg_map.emplace(args[i], i);
		}
	}
};

struct TokenState {
	Token*      tok;

	TokenType peek (int lookahead=0) {
		return tok[lookahead].type;
	}
	Token& get () {
		return *tok++;
	}
	bool eat (TokenType type) {
		if (tok->type != type)
			return false;
		tok++;
		return true;
	}
};

struct Parser {
	TokenState tok;

	std::string&        last_err;
	BlockBumpAllocator& allocator;

	ast_ptr ast_node (OPType opcode, Token& tok_for_text) {
	#if BUMP_ALLOCATOR
		ast_ptr node = allocator.alloc<ASTNode>();
	#else
		ast_ptr node = ast_ptr(new ASTNode());
	#endif
		memset(GET_AST_PTR(node), 0, sizeof(ASTNode));

		node->op.code = opcode;
		node->op.text = (std::string_view)tok_for_text;
		return node;
	}

	// Recursive decent parsing with precedence climbing
	// with a little help from https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing

	//    a single value                  ex. 5.3  or  x
	// or an expression in parentheses    ex. (-x^3 + 5)
	// or a function call                 ex. abs(x+3)
	// any of these can be preceded by a unary minus  -5.3 or -x
	ast_ptr atom () {
		ast_ptr result;
		if (tok.eat(T_PAREN_OPEN)) {
			result = expression(0);
			if (!result) return nullptr;

			if (!tok.eat(T_PAREN_CLOSE)) {
				last_err = "syntax error, ')' expected!";
				return nullptr;
			}
		}
		else if (tok.peek(0) == T_IDENTIFIER && tok.peek(1) == T_PAREN_OPEN) {
			// function call
			result = ast_node(OP_FUNCCALL, tok.get());
			tok.get(); // T_PAREN_OPEN

			ast_ptr* arg_ptr = &result->child;

			int argc = 0;

			if (tok.eat(T_PAREN_CLOSE)) {
				// 0 args
			} else {
				for (;;) {
					*arg_ptr = expression(0);
					if (!*arg_ptr) return nullptr;

					argc++;
					arg_ptr = &(*arg_ptr)->next;

					if (tok.eat(T_COMMA)) {
						continue;
					} else if (tok.eat(T_PAREN_CLOSE)) {
						break;
					} else {
						last_err = "syntax error, ',' or ')' expected!";
						return nullptr;
					}
				}
			}

			result->op.argc = argc;
		}
		else {
			OPType type;
			float value = 0;

			auto& t = tok.get();
			if      (t.type == T_LITERAL   ) {
				type = OP_VALUE;
				value = t.value;
			}
			else if (t.type == T_IDENTIFIER) {
				if (lookup_constant((std::string_view)t, &value)) {
					type = OP_VALUE;
				} else {
					type = OP_VARIABLE;
				}
			}
			else {
				last_err = "syntax error, number or variable expected!";
				return nullptr;
			}

			result = ast_node(type, t);
			result->op.value = value;
		}
		return result;
	}

	// a series of atoms seperated by binary operators (of precedence higher or equal than min_prec)
	// ex. -x^(y+3) + 5
	// note that the (y+3) is an atom, which happens to be a sub-expression
	// expression calls itself recursively with increasing min_precedences to generate operators in the correct order (precedence climbing algorithm)
	ast_ptr expression (int min_prec) {

		ast_ptr unary_minus = nullptr;
		if      (tok.peek() == T_MINUS) unary_minus = ast_node(OP_UNARY_NEGATE, tok.get());
		else if (tok.peek() == T_PLUS ) tok.get(); // skip, since unary plus is no-op
		int unary_prec = 1;

		ast_ptr lhs = atom();
		if (!lhs) return nullptr;

		if (unary_minus) {
			min_prec = std::min(unary_prec, min_prec);

			auto op_tok = tok.peek();
			if (is_binary_op(op_tok) && unary_prec >= get_binary_op_precedence(op_tok)) {
				unary_minus->child = std::move(lhs);
				lhs = std::move(unary_minus);
			}
		}

		for (;;) {
			auto op_tok = tok.peek();
			if (!is_binary_op(op_tok))
				break;

			int  prec  = get_binary_op_precedence(   op_tok);
			auto assoc = get_binary_op_associativity(op_tok);

			if (prec < min_prec)
				break;

			if (unary_minus && unary_prec >= prec) {
				unary_minus->child = std::move(lhs);
				lhs = std::move(unary_minus);
			}

			auto op_type = (OPType)(op_tok + (OP_ADD-T_PLUS));
			ast_ptr op = ast_node(op_type, tok.get());

			ast_ptr rhs = expression(assoc == LEFT_ASSOC ? prec+1 : prec);
			if (!rhs) return nullptr;

			lhs->next = std::move(rhs);
			op->child = std::move(lhs);

			lhs = std::move(op);
		}

		if (unary_minus) {
			unary_minus->child = std::move(lhs);
			return unary_minus;
		}
		return lhs;
	}

	bool parse_definition (EquationDef* def) {
		// where we return false it would usually be a syntax error if we were sure this was a function definition
		// but could still be the rhs expression
		// the problem is that "f(x,y) = " is abbigous with "f(x,y)" -> only the '=' tells us the previous tokes were a function def
		auto cur_tok = tok;

		if (cur_tok.peek() != T_IDENTIFIER) {
			def->is_variable = false;
			def->args = {"x"};
			return false;
		}

		def->name = (std::string_view)cur_tok.get();

		if (cur_tok.eat(T_PAREN_OPEN)) {
			def->is_variable = false;
			
			while (!cur_tok.eat(T_PAREN_CLOSE)) {
				if (cur_tok.peek() != T_IDENTIFIER)
					return false; // syntax error, expected argument identifier

				def->args.emplace_back( (std::string_view)cur_tok.get() );

				if (cur_tok.eat(T_COMMA)) {
					continue;
				} else if (cur_tok.eat(T_PAREN_CLOSE)) {
					break;
				} else {
					return false; // syntax error, ',' or ')' expected!
				}
			}

		} else {
			def->is_variable = true;
		}

		if (!cur_tok.eat(T_EQUALS))
			return false; // syntax error, expected '='

		// success in parsing lhs, parse rhs after '=' next
		tok = cur_tok;

		return true;
	}
	bool parse_formula (ast_ptr* formula) {
		*formula = expression(0);
		if (!*formula) return false;

		if (tok.peek() == T_PAREN_CLOSE) {
			last_err = "syntax error, ')' without matching '('!";
			return false;
		}
		if (tok.peek() != T_EOI) {
			last_err = "syntax error, end of input expected!";
			return false;
		}
		return true;
	}

	bool parse_equation (EquationDef* def, ast_ptr* formula) {
		ZoneScoped;

		parse_definition(def);

		return parse_formula(formula);
	}
};
