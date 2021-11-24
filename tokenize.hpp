#pragma once
#include "common.hpp"
#include "kisslib/strparse.hpp"

/*
	Tokenize a string into tokens
	removes whitespace, and geneates a list of 
	 literals (float numbers)
	 identifiers
	 and single-char tokens
	terminated by a single T_EOI token

	tokens contain pointers to the range of characters they came from (keep the original string allocated!)

	literals also are parsed for their float value

	-5.0 is always  T_MINUS, T_LITERAL
	while tokenizing it as a single T_LITERAL would be preferable (faster to evaluate the code later)
	it seemed to hard to do during tokenization and the code still works if the sign is parsed as a unary minus/plus

	func( is tokenized as  T_IDENTIFIER, T_PAREN_OPEN  (there is no T_FUNCTION_CALL)
	this is handled in parsing
*/

enum TokenType {
	T_EOI=0, // end of input

	// starts with [0-9] then anything that strtod() accepts
	// note: no sign is ever parsed, '+' or '-' are greedily tokenizes as T_PLUS and T_MINUS
	// is parsed for its float value, which gets stored in the Token struct
	T_LITERAL,

	// starts with  '_' or [a-Z]  and then any number of  '_' or [a-Z] or [0-9]
	T_IDENTIFIER,

	T_PLUS,        // +
	T_MINUS,       // -
	T_MULTIPLY,    // *
	T_DIVIDE,      // /
	T_POWER,       // ^

	T_PAREN_OPEN,  // (
	T_PAREN_CLOSE, // )
	T_COMMA,       // ,

	T_EQUALS,      // =
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
