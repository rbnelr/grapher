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

	std::unique_ptr<char[]> string_buf; // pointers into std::string text seem to break? small string optimization?
	std::vector<Operation> ops;
	
	inline static bool optimize = true;

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

		BlockBumpAllocator allocator;
		Parser parser = {
			tokens.data(),
			last_err,
			allocator
		};
		
		auto ast = parser.parse_equation();
		if (ast == nullptr) {
			valid = false;
			return;
		}

		valid = generate_code(GET_AST_PTR(ast), &ops, &last_err, optimize);

		exec_valid = true;
	}

	bool evaluate (ExecState& state, float x, float* result) {
		exec_valid = execute(state, ops, result, &last_err);
		return exec_valid;
	}

	std::string dbg_eval () {
		if (!valid) return "invalid equation";

		std::string result;
		if (!execute_str(ops, &result))
			return "execute error! "+ last_err;

		return result;
	}
};

void dbg_equation (Equation& eq) {
	ImGui::Text(eq.dbg_eval().c_str());
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

		add_equation("x");
		add_equation("-x");
		add_equation("(-x)");
		add_equation("(((-x)))");
		add_equation("abs(-x)" );
		add_equation("max(x, -x)");

		//add_equation("-x+2");
		//add_equation("-x*2");
		//add_equation("-x^2");
		//
		//add_equation("-x+-2");
		//add_equation("-x*-2");
		//add_equation("-x^-2");

		add_equation("5^x");
		add_equation("10^x");
		add_equation("pi^x");
		add_equation("clamp(x/3, 0,1)");
		add_equation("sin(x)");
		add_equation("acos(x)");
		add_equation("1.3^sqrt(abs(x^2 + 5*x)) - 1");
		add_equation("x^4 + x^3 + x^2");
		
		add_equation("x^2 + -2/3 / sqrt(0.2)");

		add_equation("a*b/c");
		add_equation("a-b/c");
		add_equation("a/b-c");
		add_equation("a/b-c/d");
		add_equation("a-b/c-d");
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