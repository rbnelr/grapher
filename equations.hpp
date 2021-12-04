#include "common.hpp"
#include "parse.hpp"
#include "codegen.hpp"
#include "execute.hpp"

struct Equation {
	std::string text;

	bool enable = true;
	
	float4 col;
	float line_w = 1.5f;

	bool valid = false;
	bool exec_valid = false;
	std::string last_err = "";

	// pointers into std::string text seem to break? small string optimization?
	std::unique_ptr<char[]> string_buf;

	EquationDef            def;

	std::vector<Operation> ops;
	
	inline static bool optimize = true;

	Equation (std::string_view text = "", float4 const& col = float4(1,1,1,1)): text{text}, col{col} {
		parse();
	}

	void parse () {
		ZoneScoped;

		valid = false;

		last_err = "";

		def.is_variable = false;
		def.name = "";
		def.args.clear();
		def.arg_map.clear();

		string_buf = std::unique_ptr<char[]>(new char[text.size()+1]);
		memcpy(string_buf.get(), text.c_str(), text.size()+1);

		std::vector<Token> tokens;
		tokens.reserve(64);

		if (!tokenize(string_buf.get(), &tokens, &last_err)) {
			return;
		}

		BlockBumpAllocator allocator;
		Parser parser = {
			tokens.data(),
			last_err,
			allocator
		};
		
		ast_ptr formula;
		if (!parser.parse_equation(&def, &formula)) {
			return;
		}

		def.create_arg_map();

		valid = generate_code(GET_AST_PTR(formula), &ops, &last_err, optimize);
	}

	void evaluate (ExecState& state, float* result) {
		exec_valid = execute(state, ops, result, &last_err);
	}

	std::string dbg_eval () {
		std::string str;

		if (def.is_variable) {
			str.append("var: \""+ def.name +"\"");
		} else {
			str.append("func: \""+ def.name +"\" args: [");
			for (size_t i=0; i<def.args.size(); ++i) {
				if (i>0) str.append(",");
				str.append(def.args[i]);
			}
			str.append("]");
		}

		if (!valid) return "invalid equation";

		std::string result;
		if (!execute_str(ops, &result))
			return "execute error! "+ last_err;

		str.append("\nops: "+ result);

		return str;
	}
};

void dbg_equation (Equation& eq) {
	ImGui::Text("[%s]:", eq.text.c_str());
	ImGui::Indent();
	ImGui::Text(eq.dbg_eval().c_str());
	ImGui::Unindent();
}

struct Equations {
	std::vector<Equation> equations;

	static std::vector<lrgb> gen_std_colors () {
		std::vector<lrgb> cols;

		int n = 8;
		for (int i=0; i<n; ++i) {
			cols.emplace_back(hsv2rgb((float)i / n, 1,1));
		}

		return cols;
	}
	std::vector<lrgb> colors = gen_std_colors();

	int next_std_col = 0;
	lrgb get_std_col () {
		return colors[next_std_col++ % colors.size()];
	}

	void add_equation (std::string_view text) {
		equations.emplace_back(text, float4(get_std_col(), 1));
	}

	Equations () {
		int coli = 0;

		add_equation("3*x");
		add_equation("f = 3");
		add_equation("f() = 4");
		add_equation("f( = 5");
		add_equation("f) = 6");
		add_equation(" = 6");
		add_equation("f(x,) = 6");
		add_equation("f(x) = m*x + b + 0.2");
		// ambiguous ref
		add_equation("k(x) = f");
		// ref that has error
		add_equation("u(x) = 7&3");
		add_equation("v(x) = u(x)");
		// circ ref
		add_equation("e(x) = e(x)");
		add_equation("r(x) = t(x)");
		add_equation("t(x) = r(x)");

		add_equation("g(x,m,b) = m*x + b");
		add_equation("h(x) = g(x, m,b)");
		add_equation("m = 0.5");
		add_equation("b = pi - 3");

		//add_equation("x");
		//add_equation("-x");
		//add_equation("(-x)");
		//add_equation("(((-x)))");
		//add_equation("abs(-x)" );
		//add_equation("max(x, -x)");

		//add_equation("-x+2");
		//add_equation("-x*2");
		//add_equation("-x^2");
		//
		//add_equation("-x+-2");
		//add_equation("-x*-2");
		//add_equation("-x^-2");

		//add_equation("5^x");
		//add_equation("10^x");
		//add_equation("pi^x");
		//add_equation("clamp(x/3, 0,1)");
		//add_equation("sin(x)");
		//add_equation("acos(x)");
		//add_equation("1.3^sqrt(abs(x^2 + 5*x)) - 1");
		//add_equation("x^4 + x^3 + x^2");
		//
		//add_equation("x^2 + -2/3 / sqrt(0.2)");
		//
		//add_equation("a*b/c");
		//add_equation("a-b/c");
		//add_equation("a/b-c");
		//add_equation("a/b-c/d");
		//add_equation("a-b/c-d");
	}
	
	std::unordered_map<std::string_view, int> name_map;

	void create_name_map () {
		name_map.clear();

		for (int eq_i=0; eq_i<(int)equations.size(); ++eq_i) {
			auto& eq = equations[eq_i];

			if (eq.valid && !eq.def.name.empty()) {
				auto res = name_map.emplace(eq.def.name, eq_i);
				if (!res.second) { // dupliacte name, did not insert
					// set value of name_map (the equation index coresponding to the name key) to -1
					// to signal that the name exists, but is ambiguous
					res.first->second = -1;
				}
			}

			eq.exec_valid = true;
		}
	}


	void recurse_dependency_sort (int eq_i, std::vector<int>& visited, std::vector<int>& sorted) {

		Equation& eq = equations[eq_i];
		if (!eq.valid)
			return; // pretend invalid equations don't exist

		if (visited[eq_i] > 0) {
			if (visited[eq_i] >= 2 && eq.exec_valid) {
				// circular dependency detected!
				eq.exec_valid = false;
				eq.last_err = "circular reference!";
			}
			return; // equation already visited, skip
		}
		visited[eq_i] = 2; // set to <currently visiting>

		for (auto& op : eq.ops) {
			if (op.code == OP_VARIABLE || op.code == OP_FUNCCALL) {
				auto it = name_map.find(op.text);
				if (it == name_map.end()) {
					// var or func not found, could be function argument or actually a missing dependency
					// leave potential error reporting to later function evaluation
				} else {
					// recurse into equation dependecies
					int dep_eq_i = it->second;
					if (dep_eq_i <= -1) {
						// name exists, but is ambiguous dupliacte ref
						eq.exec_valid = false;
						eq.last_err = "reference to ambiguous function/variable name";
					} else {
						recurse_dependency_sort(dep_eq_i, visited, sorted);
					}
				}
			}
		}

		visited[eq_i] = 1; // set to <visited>

		// add to sorted list after recursive calls have inserted all our dependencies first
		sorted.push_back(eq_i);
	}

	void dependency_sort (std::vector<int>* sorted) {

		create_name_map();

		// per equation, 0 = not visited   1 = visited   2 = currently visiting
		std::vector<int> visited (equations.size(), 0);

		for (int eq_i=0; eq_i<(int)equations.size(); ++eq_i) {
			recurse_dependency_sort(eq_i, visited, *sorted);
		}
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
			auto& eq = *it;
			int idx = (int)(it - equations.begin());

			ImGui::PushID(imgui_id);

			ImGui::PushStyleColor(ImGuiCol_FrameBg,        (ImVec4)eq.col);
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, (ImVec4)eq.col+0.2f); // TODO: generating highlighted colors like this is not ideal...
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  (ImVec4)eq.col+0.3f);
			ImGui::Checkbox("##enable", &eq.enable);
			ImGui::PopStyleColor(3);

			if (ImGui::BeginPopupContextItem()) {
				ImGui::SliderFloat("Line Thickness", &eq.line_w, 0.5f, 4);
				ImGui::ColorPicker3("Line Color", &eq.col.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
				ImGui::EndPopup();
			}

			ImGui::SameLine();
			if (ImGui::InputText("##text", &eq.text))
				eq.parse();

			if (ImGui::BeginDragDropTarget()) {
				if (auto* payload = ImGui::AcceptDragDropPayload("DND_EQUATION")) {
					IM_ASSERT(payload->DataSize == sizeof(idx));
					drag_drop_equations(*(int*)payload->Data, idx);
				}
				ImGui::EndDragDropTarget();
			}

			bool valid = eq.valid && eq.exec_valid;
			ImGui::SameLine();
			ImGui::PushStyleColor(0, !valid ? ImVec4(1,0,0,1) : ImVec4(1,1,1,1));
			ImGui::SmallButton(!valid ? "!##err":" ##err");
			ImGui::PopStyleColor();

			if (!valid && ImGui::IsItemHovered())
				ImGui::SetTooltip(eq.last_err.c_str());

			ImGui::SameLine();
			ImGui::Button("::");

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				ImGui::SetDragDropPayload("DND_EQUATION", &idx, sizeof(idx));
				ImGui::Text("%s", eq.text.c_str());
				ImGui::EndDragDropSource();
			}

			ImGui::SameLine();
			bool del = ImGui::Button("X");

			// TODO: awkward to be looking directly at the generated code here?
			// but also don't really want to look at the text or ast either (let's not reparse just because the slider value has changed)
			bool show_slider = eq.valid && eq.def.is_variable && eq.ops.size() == 1 && eq.ops[0].code == OP_VALUE;

			if (show_slider) {
				ImGui::SameLine();
				if (ImGui::TreeNodeEx("##submenu", ImGuiTreeNodeFlags_DefaultOpen)) {
					
					float value = eq.ops[0].value;
					if (ImGui::DragFloat("##slider", &value, 0.01f)) {
						// update text and code, new text _should_ parse to new code
						// This is not ideal though, since the user might expect the text to keep his formatting
						// 'correct' solution to avoid this would be to let user select a slider which then makes the text input window disappear and overrides the code
						eq.text = eq.def.name + prints(" = %g", value);
						eq.ops[0].value = value;
					}

					ImGui::TreePop();
				}
			}

			ImGui::PopID();

			if (del) {
				it = equations.erase(it);
			} else {
				++it;
			}
			imgui_id++; // inc even if X button was pressed to avoid duplicate imgui IDS
		}

		if (ImGui::Button("+")) {
			add_equation("");
		}

		bool reparse = ImGui::Checkbox("codegen optimize", &Equation::optimize);
		
		if (reparse) {
			for (auto& eq : equations)
			eq.parse();
		}

		ImGui::PopID();
	}
};