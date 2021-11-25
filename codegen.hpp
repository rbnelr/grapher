#pragma once
#include "common.hpp"
#include "parse.hpp"
#include "execute.hpp"

inline bool constant_folding (ASTNode* node) {
	if (node->op.code == OP_VALUE)
		return true;
	if (node->op.code == OP_VARIABLE)
		return false;

	bool const_children = true;

	std::vector<float> values;
	values.reserve(16);

	for (auto* cur = node->child.get(); cur; cur = cur->next.get()) {
		bool is_const = constant_folding(cur);
		if (!is_const)
			const_children = false;
		else
			values.push_back(cur->op.value);
	}
	
	if (!const_children)
		return false;

	float value;
	const char* errstr;

	switch (node->op.code) {
		case OP_FUNCCALL: {
			assert((int)values.size() == node->op.argc);
			call_func(node->op, values.data(), &value, &errstr);
		} break;

		case OP_UNARY_NEGATE: {
			assert((int)values.size() == 1);

			value = -values[0];
		} break;

		case OP_ADD       :
		case OP_SUBSTRACT :
		case OP_MULTIPLY  :
		case OP_DIVIDE    :
		case OP_POW       : {
			assert((int)values.size() == 2);

			float a = values[0];
			float b = values[1];

			switch (node->op.code) {
				case OP_ADD       : value = a + b; break;
				case OP_SUBSTRACT : value = a - b; break;
				case OP_MULTIPLY  : value = a * b; break;
				case OP_DIVIDE    : value = a / b; break;
				case OP_POW       : value = mypow(a, b); break;
				default: assert(false);
			}
		} break;
	}

	node->op.code = OP_VALUE;
	node->op.value = value;
	node->op.text = std::string_view();

	node->child = nullptr;

	return true;
}

inline void emit_ops (ASTNode const* node, std::vector<Operation>* ops) {

	for (auto* cur = node->child.get(); cur; cur = cur->next.get())
		emit_ops(cur, ops);

	ops->emplace_back( node->op );
}

inline bool generate_code (ASTNode* ast, std::vector<Operation>* out_ops, std::string* last_err, bool optimize) {
	out_ops->clear();

	if (optimize)
		constant_folding(ast);
	
	emit_ops(ast, out_ops);

	return true;
}