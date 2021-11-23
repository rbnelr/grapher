#include "common.hpp"
#include "kisslib/parse.hpp"

enum TokenType {
	T_EOI=0, // end of input

	T_LITERAL,
	T_IDENTIFIER,

	T_PLUS,
	T_MINUS,
	T_MULTIPLY,
	T_DIVIDE,
	T_POWER,

	T_PAREN_OPEN,
	T_PAREN_CLOSE,
	T_COMMA,

	T_EQUALS,
};

struct Token {
	TokenType   type;

	float       value; // only for T_LITERAL

	char const* begin;
	char const* end;

	operator std::string_view () const { // points into str from tokenize() ie. Equation::string_buf
		return std::string_view(begin, end - begin);
	}
};

bool tokenize (const char* str, std::vector<Token>* tok, std::string* err_msg) {
	using namespace parse;

	const char* cur = str;
	for (;;) {
		whitespace(cur); // skip all whitespace until next token

		if (*cur == '\0')
			break; // stop

		const char* start = cur;

		TokenType type;
		float value = 0;

		if (is_decimal_c(*cur)) {
			if (!parse_float(cur, &value)) {
				*err_msg = prints("tokenize: parse_float error: \n\"%s\"", cur);
				return false;
			}

			type = T_LITERAL;
		}
		else if (is_ident_start_c(*cur)) {

			while (is_ident_c(*cur))
				cur++; // find end of identifier

			type = T_IDENTIFIER;
		}
		else {
			switch (*cur) {
				case '+': type = T_PLUS;          break;
				case '-': type = T_MINUS;         break;
				case '*': type = T_MULTIPLY;      break;
				case '/': type = T_DIVIDE;        break;
				case '^': type = T_POWER;         break;

				case '(': type = T_PAREN_OPEN;    break;
				case ')': type = T_PAREN_CLOSE;   break;
				case ',': type = T_COMMA;         break;

				case '=': type = T_EQUALS;        break;
				
				default: {
					*err_msg = prints("tokenize: unknown token: \n\"%s\"", cur);
					return false;
				}
			}
			cur++; // single-char token
		}

		tok->push_back({ type, value, start, cur });
	}

	tok->push_back({ T_EOI, 0, cur, cur+1 });
	return true;
}

enum OPType {
	OP_VALUE,        // push value
	OP_VARIABLE,     // push vars.lookup(varname)

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
inline constexpr bool is_binary_op (OPType code) {
	return code >= OP_ADD && code <= OP_POW;
}

inline constexpr uint8_t BINARY_OP_PRECEDENCE[] = {
	2, // T_PLUS,
	2, // T_MINUS,
	1, // T_MULTIPLY,
	1, // T_DIVIDE,
	0, // T_POWER,
};
inline constexpr int MAX_PRECEDENCE = 2;

inline int get_binary_op_precedence (TokenType tok) {
	assert(is_binary_op(tok));
	return BINARY_OP_PRECEDENCE[tok - T_PLUS];
}

struct Operation {
	OPType           code;

	std::string_view text;
	float            value;
};

inline Operation literal_or_variable (Token& tok) {
	OPType type;
	switch (tok.type) {
		case T_LITERAL    : type = OP_VALUE;    break;
		case T_IDENTIFIER : type = OP_VARIABLE; break;
		default: {
			assert(false);
		}
	}
	return { type, (std::string_view)tok, tok.value };
}
inline Operation binary_op (Token& tok) {
	return { (OPType)(tok.type - T_PLUS + OP_ADD), (std::string_view)tok, tok.value };
}

inline bool value_expression (Token*& tok, std::vector<Operation>& ops, std::string* last_err);

inline bool recurse_subexpression (Token*& tok, std::vector<Operation>& ops, std::string* last_err, int precedence) {

	for (;;) {
		if (tok->type == T_EOI || tok->type == T_PAREN_CLOSE)
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
				return false; // propagate error
		} else {
			// operator precedence matches recursion level, handle operand-operator pair here

			auto& op = *tok++;

			if (!value_expression(tok, ops, last_err))
				return false; // propagate error

			auto& op2 = *tok;

			if (is_binary_op(tok[0].type)) {
				int op2_prec = get_binary_op_precedence(tok[0].type);
				if (op2_prec < precedence) {
					if (!recurse_subexpression(tok, ops, last_err, precedence - 1))
						return false; // propagate error
				}
			}

			ops.push_back( binary_op(op) );
		}
	}
}

inline bool value_expression (Token*& tok, std::vector<Operation>& ops, std::string* last_err) {
	bool unary_negate = false;
	if (tok->type == T_MINUS || tok->type == T_PLUS) {
		unary_negate = tok->type == T_MINUS;
		tok++;
	}

	if (tok->type == T_PAREN_OPEN) {
		tok++;

		if (!value_expression(tok, ops, last_err))
			return false; // propagate error

		if (!recurse_subexpression(tok, ops, last_err, MAX_PRECEDENCE))
			return false; // propagate error

		if (tok->type != T_PAREN_CLOSE) {
			*last_err = "syntax error, '(' not closed with ')'!";
				return false;
		}
		tok++;

	} else {
		if (!(tok->type == T_LITERAL || tok->type == T_IDENTIFIER)) {
			*last_err = "syntax error, literal or variable expected!";
			return false;
		}

		ops.push_back( literal_or_variable(*tok++) );
	}

	if (unary_negate)
		ops.push_back({ OP_UNARY_NEGATE, "", 0 });
	return true;
}

inline bool parse_equation (Token*& tok, std::vector<Operation>& ops, std::string* last_err) {
	if (!value_expression(tok, ops, last_err))
		return false; // propagate error

	if (!recurse_subexpression(tok, ops, last_err, MAX_PRECEDENCE))
		return false; // propagate error

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

struct Variables {
	float x;

	float lookup (std::string_view const& name, float* out) {
		if (name == "x") {
			*out = x;
			return true;
		}

		return false;
	}
};

float mypow (float a, float b) {
	//if (b == 2.0f) return a*a;
	//if (b == 3.0f) return a*a*a;
	//if (b == 4.0f) return (a*a)*(a*a);

	return powf(a, b);
}

bool eval (Variables& vars, std::vector<Operation>& ops, float* result, std::string* last_err) {
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
				if (!vars.lookup(op.text, &value)) {
					errstr = "vars.lookup() failed!";
					goto error;
				}
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

			default: {
				if (!is_binary_op(op.code)) {
					errstr = "unknown op type!";
					goto error;
				}

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
					default: return false;
				}
			} break;
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
	*last_err = prints("%s\nop: %s text: \"%*s\"", errstr, OPType_str[code.code], code.text.size(), code.text.data());
	return false;
}
bool eval_str (std::vector<Operation>& ops, std::string* result, std::string* dbg_str) {
	ZoneScoped;
	
	std::vector<std::string> stack;

	for (auto& code : ops) {
		std::string value;

		if (code.code == OP_VALUE) {
			value = (std::string)code.text;
		}
		else if (code.code == OP_VARIABLE) {
			value = (std::string)code.text;
		}
		else if (is_binary_op(code.code)) {
			assert(stack.size() >= 2);
			std::string a = std::move(stack[stack.size() - 2]);
			std::string b = std::move(stack[stack.size() - 1]);
			stack.resize(stack.size() - 2);

			switch (code.code) {
				case OP_ADD       : value = prints("(%s + %s)", a.c_str(), b.c_str()); break;
				case OP_SUBSTRACT : value = prints("(%s - %s)", a.c_str(), b.c_str()); break;
				case OP_MULTIPLY  : value = prints("(%s * %s)", a.c_str(), b.c_str()); break;
				case OP_DIVIDE    : value = prints("(%s / %s)", a.c_str(), b.c_str()); break;
				case OP_POW       : value = prints("(%s ^ %s)", a.c_str(), b.c_str()); break;
				default: return false;
			}
		}
		else if (code.code == OP_UNARY_NEGATE) {
			assert(stack.size() >= 1);
			std::string a = std::move(stack[stack.size() - 1]);
			stack.resize(stack.size() - 1);

			value = prints("-(%s)", a.c_str());
		}
		else {
			*dbg_str = "unknown op type!";
			return false;
		}

		stack.push_back(std::move(value));
	}
	assert(stack.size() == 1);
	*result = std::move(stack[0]);

	return true;
}

struct Equation {
	std::string text;
	float4 col;

	bool valid = false;
	std::string last_err = "";

	std::unique_ptr<char[]> string_buf; // pointers into std::string text seem to break? small string optimization?
	std::vector<Operation> ops;
	
	Equation (std::string_view text = "", float4 const& col = float4(1,1,1,1)): text{text}, col{col} {
		parse();
	}

	void parse () {
		ZoneScoped;

		string_buf = std::unique_ptr<char[]>(new char[text.size()+1]);
		memcpy(string_buf.get(), text.c_str(), text.size()+1);

		last_err = "";

		std::vector<Token> tokens;
		tokens.reserve(64);

		if (!tokenize(string_buf.get(), &tokens, &last_err)) {
			valid = false;
			return;
		}
		
		auto* tok = &tokens[0];
		ops.clear();
		valid = parse_equation(tok, ops, &last_err);

		if (!valid)
			ops.clear();
	}

	bool evaluate (Variables& vars, float x, float* result) {
		assert(valid);
		valid = eval(vars, ops, result, &last_err);
		return valid;
	}
	std::string dbg_eval () {
		if (!valid) return "invalid equation";

		std::string result, err;
		if (!eval_str(ops, &result, &err))
			return "eval error: "+ err;

		return result;
	}
};

void dbg_equation (Equation& eq) {
	ImGui::Text(eq.dbg_eval().c_str());
}

static constexpr float4 colors[] = {
	float4(1,0,0,1),
	float4(0,1,0,1),
	float4(0,0,1,1),
	float4(1,1,0,1),
	float4(1,0,1,1),
	float4(0,1,1,1),
	float4(0.5f,0.5f,0.5f,1),
};

struct Equations {
	std::vector<Equation> equations;

	Equations () {
		int coli = 0;

		equations.emplace_back("x^2",             colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("2^x",             colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("2^(x-2)",         colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("4^3^2",           colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("4^(3^2)",         colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("(4^3)^2",         colors[coli++ % ARRLEN(colors)]);

		//equations.emplace_back("(x)",            colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("(x+2)",          colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("(x+2*3)",        colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("5*(x+3)",        colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("(5*(x+3))",      colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("2-(5*(x+3)-6)",  colors[coli++ % ARRLEN(colors));

		//equations.emplace_back("x",             colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("x+3",           colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("0.5*x",         colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("4+x-8",         colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("3+x/3",         colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("8/x+5",         colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("x+3*1*2",       colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("-5*x+0.2*x",    colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("x*x*x+x",       colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("x+x/x+3",       colors[coli++ % ARRLEN(colors));

		//equations.emplace_back("3",             colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("x",             colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("x * -0.5",      colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("2+1/x",         colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("1/(x-1)",       colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("2-1/(x-1)",     colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("sqrt(x)",       colors[coli++ % ARRLEN(colors));
	}

	void drag_drop_equations (int src, int dst) {
		assert(src >= 0 && src < (int)equations.size());
		assert(dst >= 0 && dst < (int)equations.size());

		if (dst == src) return;

		Equation tmp = std::move(equations[src]);
		
		if (dst < src) {
			// move item downwards
			// shuffle arr[dst, src-1] -> arr[dst+1, src] (upwards)
			for (int i=src; i>=dst+1; --i)
				equations[i] = std::move(equations[i-1]);
		} else {
			// move item upwards
			// shuffle arr[src+1, dst] -> arr[src, dst-1] (downwards)
			for (int i=src; i<=dst-1; ++i)
				equations[i] = std::move(equations[i+1]);
		}

		equations[dst] = std::move(tmp);
	}

	void imgui () {
		if (!imgui_Header("Equations", true)) return;
		ZoneScoped;

		int imgui_id=0;
		for (auto it = equations.begin(); it != equations.end(); ) {
			int idx = (int)(it - equations.begin());

			ImGui::PushID(imgui_id);

			ImGui::ColorEdit3("##col", &it->col.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

			ImGui::SameLine();
			if (ImGui::InputText("##text", &it->text))
				it->parse();

			if (ImGui::BeginDragDropTarget()) {
				if (auto* payload = ImGui::AcceptDragDropPayload("DND_EQUATION")) {
					IM_ASSERT(payload->DataSize == sizeof(idx));
					drag_drop_equations(*(int*)payload->Data, idx);
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(0, !it->valid ? ImVec4(1,0,0,1) : ImVec4(1,1,1,1));
			ImGui::SmallButton(!it->valid ? "!###err":" ###err");
			ImGui::PopStyleColor();

			if (!it->valid && ImGui::IsItemHovered())
				ImGui::SetTooltip(it->last_err.c_str());

			ImGui::SameLine();
			ImGui::Button("::");

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				ImGui::SetDragDropPayload("DND_EQUATION", &idx, sizeof(idx));
				ImGui::Text("%s", it->text.c_str());
				ImGui::EndDragDropSource();
			}

			ImGui::SameLine();
			bool del = ImGui::Button("X");

			ImGui::PopID();

			if (del) {
				it = equations.erase(it);
			} else {
				++it;
			}
			imgui_id++; // inc even if X button was pressed to avoid duplicate imgui IDS
		}

		if (ImGui::Button("+")) {
			equations.emplace_back("");
		}

		ImGui::PopID();
	}
};