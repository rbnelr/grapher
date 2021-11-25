#include "common.hpp"
#include "parse.hpp"
#include "execute.hpp"

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
		valid = execute(vars, ops, result, &last_err);
		return valid;
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

		//equations.emplace_back("sqrt(1-x^2)                      ", colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("1/sqrt(x)                        ", colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("0.5*x^3 +0.3*x^2 -1.2*x +2       ", colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("sin(x)                           ", colors[coli++ % ARRLEN(colors)]);

		//equations.emplace_back("abs(x)*0.5",        colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("sqrt(x)",           colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("1/sqrt(x)",         colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("max(x,-x)",         colors[coli++ % ARRLEN(colors)]);

		//equations.emplace_back("x^2",             colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("2^x",             colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("2^(x-2)",         colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("4^3^2",           colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("4^(3^2)",         colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("(4^3)^2",         colors[coli++ % ARRLEN(colors)]);

		//equations.emplace_back("(x)",            colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("(x+2)",          colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("(x+2*3)",        colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("5*(x+3)",        colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("(5*(x+3))",      colors[coli++ % ARRLEN(colors));
		//equations.emplace_back("2-(5*(x+3)-6)",  colors[coli++ % ARRLEN(colors));

		equations.emplace_back("x",             colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("x+3",           colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("0.5*x",         colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("4+x-8",         colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("3+x/3",         colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("8/x+5",         colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("x+3*1*2",       colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("-5*x+0.2*x",    colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("x*x*x+x",       colors[coli++ % ARRLEN(colors)]);
		equations.emplace_back("x+x/x+3",       colors[coli++ % ARRLEN(colors)]);

		//equations.emplace_back("3",             colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("x",             colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("x * -0.5",      colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("2+1/x",         colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("1/(x-1)",       colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("2-1/(x-1)",     colors[coli++ % ARRLEN(colors)]);
		//equations.emplace_back("sqrt(x)",       colors[coli++ % ARRLEN(colors)]);
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