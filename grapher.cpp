#include "common_app.hpp"
#include "equations.hpp"
#include <array>

struct AxisUnits {
	char const* name;
	std::string unit_str;
	float scale;
	bool log = false;
	bool deg = false;
	float stretch = 1;
};
inline AxisUnits std_unit_modes[] = {
	{ "1"      , ""   , 1 },
	{ "Deg"    , "°"  , 1,  false,  true, RAD_TO_DEG },
	{ "Pi"     , "π"  , PI },
	{ "%"      , "%"  , 0.01f },
	{ "Log10"  , ""   , 1,   true, false },
};

struct Subtick {
	float offs;
	float size; // > 0.75f gets text label
};

struct Axis {
	std::string name;
	float4      col;

	const char* display_name;

	AxisUnits   custom_unit = { "Custom" , "", 1, false };

	AxisUnits*  units = &std_unit_modes[0];

	//bool        base_2 = false;

	void imgui (const char* axis) {
		int std_count = ARRLEN(std_unit_modes);

		ImGui::PushID(axis);
		ImGui::PushItemWidth(70);

		ImGui::Text(axis);

		ImGui::SameLine();
		ImGui::ColorEdit3("###col", &col.x, ImGuiColorEditFlags_NoInputs);

		ImGui::SameLine();
		ImGui::InputText("###name", &name);

		display_name = name.empty() ? axis : name.c_str();

		for (int i=0; i<std_count; ++i) {
			ImGui::SameLine();
			if (ImGui::RadioButton(std_unit_modes[i].name, units == &std_unit_modes[i]))
				units = &std_unit_modes[i];
		}
		ImGui::SameLine();
		if (ImGui::RadioButton(custom_unit.name, units == &custom_unit))
			units = &custom_unit;

		if (units == &custom_unit && ImGui::TreeNodeEx("Custom Unit", ImGuiTreeNodeFlags_DefaultOpen)) {

			ImGui::InputFloat("Scale", &custom_unit.scale, 0,0, "%g");
			ImGui::InputText("Unit Text", &custom_unit.unit_str);
			ImGui::Checkbox("Log10", &custom_unit.log);

			ImGui::TreePop();
		}

		//ImGui::Checkbox("base_2", &base_2);

		ImGui::PopItemWidth();
		ImGui::PopID();
	}

	float       tick_step;
	Subtick     subticks[16];
	int         subtick_count;

	float min_tick_dist_px = 80;

	void get_tick_step (float px2world) {
		subtick_count = 0;

		auto add_subtick = [&] (float offs, float size) {
			assert(subtick_count < ARRLEN(subticks));
			subticks[subtick_count++] = { offs, size };
		};
		add_subtick(0, 1); // add base tick, rest are "subticks"

		float min_units = min_tick_dist_px * px2world; // world units per <min_tick_dist_px>

		if (units->log) {
			float log_spacing = 0.33f;
			//static float log_spacing = 0.33f;
			//ImGui::SliderFloat("log_spacing", &log_spacing, 0, 1);

			if (min_units >= 1.00f) {
				min_units = max(min_units * log_spacing, 1.0f);

				// base-2 scaling for large log scales
				// (every 2nd tick mark disappears when zooming out)
				tick_step = powf(2.0f, ceil(log2f(min_units)));

				// show smaller tick marks for int powers of 10
				if (tick_step >= 4.0f) {
					add_subtick( 0.25f, 0.50f );
					add_subtick( 0.50f, 0.75f );
					add_subtick( 0.75f, 0.50f );
				}
				else if (tick_step >= 2.0f) {
					add_subtick( 0.50f, 0.75f );
				}
			} else {
				// integer multiples of log-space values are integer exponents in normal space
				// at small scale, ie. fractional log-space values usually don't map to nice normal space values
				// ex. while 2   in log-space is 100 in normal space
				//           0.5 in log-space is sqrt(10) = 3.1622
				// so instead use explicit places for ticks up to a certain zoom level
				// above that level I'm clueless as to how to place ticks procedurally and have them be nice number

				tick_step = 1.0f;

				if (min_units >= 0.50f) {
					add_subtick( log10f(0.25f), 1.00f );
					add_subtick( log10f(0.50f), 1.00f );
					add_subtick( log10f(0.75f), 0.75f );
				}
				else if (min_units >= 0.25f) {
					add_subtick( log10f(0.2f), 1.00f );
					add_subtick( log10f(0.3f), 0.75f );
					add_subtick( log10f(0.4f), 0.75f );
					add_subtick( log10f(0.5f), 1.00f );
					add_subtick( log10f(0.6f), 0.75f );
					add_subtick( log10f(0.7f), 0.75f );
					add_subtick( log10f(0.8f), 0.75f );
					add_subtick( log10f(0.9f), 0.75f );
				}
				else {
					add_subtick( log10f(0.2f), 1.00f );
					add_subtick( log10f(0.3f), 1.00f );
					add_subtick( log10f(0.4f), 1.00f );
					add_subtick( log10f(0.5f), 1.00f );
					add_subtick( log10f(0.6f), 1.00f );
					add_subtick( log10f(0.7f), 1.00f );
					add_subtick( log10f(0.8f), 1.00f );
					add_subtick( log10f(0.9f), 1.00f );
				}
			}
		}
		else if (units->deg && min_units >= 10.0f) {

			if (min_units > 90.0f) {
				min_units = max(min_units / 180.0f, 1.0f);
				tick_step = powf(2.0f, ceil(log2f(min_units))) * 180.0f;

				add_subtick( 0.25f, 0.50f );
				add_subtick( 0.50f, 0.75f );
				add_subtick( 0.75f, 0.50f );
			}
			else if (min_units > 45.0f) {
				tick_step = 90.0f;
				add_subtick( 15 / 90.0f, 0.50f );
				add_subtick( 30 / 90.0f, 0.50f );
				add_subtick( 45 / 90.0f, 0.75f );
				add_subtick( 60 / 90.0f, 0.50f );
				add_subtick( 75 / 90.0f, 0.50f );
			}
			else if (min_units > 20.0f) {
				tick_step = 45.0f;

				add_subtick( 15 / 45.0f, 0.75f );
				add_subtick( 30 / 45.0f, 0.75f );
			}
			else {
				tick_step = 15.0f;

				add_subtick(  5 / 15.0f, 0.75f );
				add_subtick( 10 / 15.0f, 0.75f );
			}
		}
		else {
			min_units /= units->scale;

			float scale = powf(10.0f, ceil(log10f(min_units))); // rounded up to next ..., 0.1, 1, 10, 100, ...
			float fract = min_units / scale; // remaining factor  0.3 -> scale=1 factor=0.3    25 -> scale=10 factor=2.5 etc.

			if      (fract <= 0.2f)   {
				fract = 0.2f;

				add_subtick( 0.05f / fract, 0.50f );
				add_subtick( 0.10f / fract, 0.75f );
				add_subtick( 0.15f / fract, 0.50f );
			}
			else if (fract <= 0.5f)   {
				fract = 0.5f;

				add_subtick( 0.10f / fract, 0.50f );
				add_subtick( 0.20f / fract, 0.50f );
				add_subtick( 0.30f / fract, 0.50f );
				add_subtick( 0.40f / fract, 0.50f );
			}
			else {
				fract = 1.0f; 

				add_subtick( 0.25f / fract, 0.50f );
				add_subtick( 0.50f / fract, 0.75f );
				add_subtick( 0.75f / fract, 0.50f );
			}

			tick_step = fract * scale * units->scale;
		}
	}
};

struct App : public IApp {
	App () {}
	virtual ~App () {}

	// Equations
	Equations equations;

	// Display
	Camera2D cam = Camera2D(0, 10.0f);
	Flycam flycam = Flycam(float3(0,-7,6), float3(0,-deg(30),0), 100);

	ogl::Renderer r;

	TextRenderer text = TextRenderer("fonts/DroidSerif-WmoY.ttf", 64, true,
		"°∞" // π°∞
		"αβγδεζηθικλμνξοπρστυφχψω" // greek letters
		"ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩ"
	);

	LineRenderer lines;

	LineRenderer::DrawCall              axis_lines;
	std::vector<LineRenderer::DrawCall> eq_lines;
	LineRenderer::DrawCall              select_lines;

	ShapeRenderer circles = {"circle_render"};

	Shader* grid_shad = g_shaders.compile("grid");
	Vao dummy_vao = {"dummy_vao"};

	bool mode_3d = false;

	float text_size = 24.0f;

	float axis_line_w = 1.49f; // round to 1 for not-AA
	bool axis_line_aa = true;
	
	Axis axes[2] = {
		{ "", float4(1.0f,0.1f,0.1f,1) },
		{ "", float4(0.1f,1.0f,0.1f,1) },
	};
	
	void imgui (Input& I) {
		ZoneScoped

		r.debug_draw.imgui();
		text.imgui();

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		ImGui::Checkbox("3D", &mode_3d);

		ImGui::Spacing();
		axes[0].imgui("x");
		axes[1].imgui("y");
		ImGui::Spacing();

		if (ImGui::TreeNode("Graphics")) {
			ImGui::DragFloat("text_size", &text_size, 0.05f, 0, 64);
			ImGui::DragFloat("equation_res", &eq_res_px, 0.02f);

			ImGui::Checkbox("axis_line_antialis", &axis_line_aa);
			ImGui::SliderFloat("axis_line_thickness", &axis_line_w, 0.5f, 8);

			cam.imgui();
			flycam.imgui();

			ImGui::TreePop();
		}

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		equations.imgui();

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
	}

	float4 world2screen (float3 const& pos, View3D const& view) {
		float4 clip = view.world2clip * float4(pos, 1);
		float4 ndc = clip / clip.w;

		return (ndc * 0.5f + 0.5f) * float4(view.viewport_size, 1,1);
	}
	float2 map_text (float3 const& pos, View3D const& view) {
		float4 screen = world2screen(pos, view);
		screen.y = view.viewport_size.y - screen.y;
		return (float2)screen;
	}

	float eq_res_px = 1;

	float ticks_px = 7.0f;

	float4 col_ticks_text = float4(0.8f,0.8f,0.8f,1);

	float4 eq_col = float4(0.95f,0.95f,0.95f,1);

	// 
	float2 px2world, world2px;

	float2 view0, view1;

	View3D update_2d_view (Input& I, float2 const& viewport_size) {

		auto view = cam.update(I, viewport_size);

		float2 scale_fac;
		scale_fac.x = axes[0].units->stretch;
		scale_fac.y = axes[1].units->stretch;

		{ // 
			float4x4 stretch2world = (float4x4)scale(float3(1.0f/scale_fac, 1));
			float4x4 world2stretch = (float4x4)scale(float3(scale_fac, 1));

			view.world2cam = view.world2cam * stretch2world;
			view.cam2world = world2stretch * view.cam2world;

			view.world2clip = view.world2clip * stretch2world;
			view.clip2world = world2stretch * view.clip2world;

			view.cam_pos *= float3(scale_fac, 1);
		}

		//
		px2world = view.frust_near_size * scale_fac / view.viewport_size;
		world2px = 1.0f / px2world;

		axes[0].get_tick_step(px2world.x);
		axes[1].get_tick_step(px2world.y);
		
		float2 view_size = view.frust_near_size * scale_fac * 0.5f;

		view0 = (float2)view.cam_pos - view_size;
		view1 = (float2)view.cam_pos + view_size;

		return view;
	}

	std::string format_axis_tick (float coord, Axis& axis) {
		// unit_str=""   with scale=1    -> 0  1  2  3  4  5  6  7    
		// unit_str="pi" with scale=3.14 -> 0pi       1pi       2pi   
		// unit_str=""   with scale=3.14 -> 0         3.14      6.28  
		coord *= 1.0f / axis.units->scale;

		if (axis.units->log) coord = powf(10.0f, coord);

		return prints("%g%s", coord, axis.units->unit_str.c_str());
	}
	std::string format_point (float coord_x, float coord_y) {
		coord_x *= 1.0f / axes[0].units->scale;
		coord_y *= 1.0f / axes[1].units->scale;

		if (axes[0].units->log) coord_x = powf(10.0f, coord_x);
		if (axes[1].units->log) coord_y = powf(10.0f, coord_y);

		return prints("(%.3f%s, %.3f%s)", coord_x, axes[0].units->unit_str.c_str(), coord_y, axes[1].units->unit_str.c_str());
	}

	void draw_background_grid (Input& I, View3D const& view) {
		OGL_TRACE("background_grid");
		ZoneScoped

		glUseProgram(grid_shad->prog);

		PipelineState s;
		s.depth_test = false;
		s.blend_enable = false;
		r.state.set(s);

		grid_shad->set_uniform("grid_size", float2(axes[0].tick_step, axes[1].tick_step));

		glBindVertexArray(dummy_vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	void draw_axes (Input& I, View3D const& view) {
		ZoneScoped;

		axis_lines = lines.begin_draw(axis_line_w, axis_line_aa);
		auto& line_vc = axis_lines.vertex_count;

		{ // draw axes lines
			float2 line_pad = px2world * 10;
		
			line_vc += lines.draw_line(float3(view0.x-line_pad.x, 0,0), float3(view1.x+line_pad.x, 0,0), axes[0].col);
			line_vc += lines.draw_line(float3( 0,view0.y-line_pad.y,0), float3( 0,view1.y+line_pad.y,0), axes[1].col);
		}

		float ticks_text_px = text_size * 0.66f;
		float axis_label_text_px = text_size * 1.25f;
		float2 ticks_text_padding = ticks_px * 1.5f;

		auto draw_axis_tick_text = [&] (View3D const& view, const char* str, float x, float y, float2 const& align, int axis=-1) {
			auto ptext = text.prepare_text(str, ticks_text_px, col_ticks_text);

			float2 pos_px = map_text(float3(x, y, 0), view);

			float2 padded_bounds = ptext.bounds + ticks_text_padding * 2.0f;
			float2 offset = pos_px + ticks_text_padding - padded_bounds * align;

			if      (axis == 1) offset.x = clamp(offset.x, ticks_text_padding.x, view.viewport_size.x - ptext.bounds.x - ticks_text_padding.x);
			else if (axis == 0) offset.y = clamp(offset.y, ticks_text_padding.y, view.viewport_size.y - ptext.bounds.y - ticks_text_padding.y);

			text.offset_glyphs(ptext.idx, ptext.len, offset);
		};

		draw_axis_tick_text(view, "0", 0,0, float2(1,0));

		float2 tick_sz = ticks_px * px2world;

		{ // X
			auto& axis = axes[0];
			
			int start = floori(view0.x / axis.tick_step);
			int   end =  ceili(view1.x / axis.tick_step);

			for (int i=start; i<=end; ++i) {
				for (int j=0; j<axis.subtick_count; ++j) {
					float coord = ((float)i + axis.subticks[j].offs) * axis.tick_step;
					if (coord == 0.0f) continue;

					if (axis.subticks[j].size >= 1.0f)
						draw_axis_tick_text(view, format_axis_tick(coord, axis).c_str(), coord, 0, float2(0.5f, 0), 0);

					float sz = tick_sz.y * axis.subticks[j].size;
					line_vc += lines.draw_line(float3(coord, -sz, 0), float3(coord, +sz, 0), axis.col);
				}
			}
		}
		{ // Y
			auto& axis = axes[1];

			int start = floori(view0.y / axis.tick_step);
			int   end =  ceili(view1.y / axis.tick_step);

			for (int i=start; i<=end; ++i) {
				for (int j=0; j<axis.subtick_count; ++j) {
					float coord = ((float)i + axis.subticks[j].offs) * axis.tick_step;
					if (coord == 0.0f) continue;

					if (axis.subticks[j].size >= 1.0f)
						draw_axis_tick_text(view, format_axis_tick(coord, axis).c_str(), 0, coord, float2(1, 0.5f), 1);

					float sz = tick_sz.x * axis.subticks[j].size;
					line_vc += lines.draw_line(float3(-sz, coord, 0), float3(+sz, coord, 0), axis.col);
				}
			}
		}

		if (hover_eq >= 0) { // Draw hover point ticks
			float2 sz = tick_sz * 1.2f;
			line_vc += lines.draw_line(float3(hover_point.x, -sz.y, 0), float3(hover_point.x, +sz.y, 0), equations.equations[hover_eq].col);
			line_vc += lines.draw_line(float3(-sz.x, hover_point.y, 0), float3(+sz.x, hover_point.y, 0), equations.equations[hover_eq].col);
		}

		{ // Draw axis labels
			text.draw_text(axes[0].display_name, axis_label_text_px, axes[0].col,
				map_text(float3(view1.x, 0, 0), view), float2(1,1), ticks_text_padding);
			text.draw_text(axes[1].display_name, axis_label_text_px, axes[1].col,
				map_text(float3(0, view1.y, 0), view), float2(0,0), ticks_text_padding);
		}
	}

	int clicked_eq = -1;

	int hover_eq = -1;
	float2 hover_point = -1;

	void draw_equations (Input& I, View3D const& view) {
		ZoneScoped;
		
		Evaluator eval;
		eval.deg_mode.from_deg_x = axes[0].units->deg ? DEG_TO_RAD : 1;
		eval.deg_mode.to_deg_y   = axes[1].units->deg ? RAD_TO_DEG : 1;

		// sort variables such that dependencies are always first
		std::vector<int> sorted_equations;
		equations.dependency_sort(&sorted_equations);

		bool dbg = ImGui::TreeNode("Debug Equations");

		for (int eq_i : sorted_equations) {
			auto& eq = equations.equations[eq_i];

			if (eq.def.is_variable) {
				if (dbg) {
					dbg_equation(eq);
					ImGui::Separator();
				}

				float value;
				eq.exec_valid = eval.execute(eq.def, eq.ops, 0, &value, &eq.last_err);
				if (eq.exec_valid)
					eval.var_values.emplace(eq.def.name, value);
			} else {
				if (eq.exec_valid && equations.name_map.find(eq.def.name) != equations.name_map.end()) // don't insert ambiguous names
					eval.functions.emplace(eq.def.name, Evaluator::Function{ &eq.def, &eq.ops });
			}
		}

		// Plot functions by evaluating them for all desired x values
		// and handle curve hover points
		eq_lines.resize(equations.equations.size());

		float2 cursor = I.cursor_pos_bottom_up;

		float  nearest_dist = INF;
		float2 nearest_point;
		int    nearest_eq = -1;

		auto cursor_select_line = [&] (int eq_i, float2 a, float2 b) {
			if (clicked_eq >= 0 && eq_i != clicked_eq)
				return;

			a = (a - view0) * world2px;
			b = (b - view0) * world2px;

			float2 point;
			float dist = point_line_segment_dist(a, b - a, cursor, &point);
			
			// use <= to let later equations override the nearest point, to better match what's seen visually
			// (later equation lines are drawn on top)
			if (dist <= nearest_dist) {
				nearest_dist = dist;
				nearest_point = point;
				nearest_eq = eq_i;
			}
		};

		eq_res_px = max(eq_res_px, 1.0f / 8);

		float res = px2world.x * eq_res_px;
		int start = floori(view0.x / res), end = ceili(view1.x / res);

		for (int eq_i=0; eq_i<(int)equations.equations.size(); ++eq_i) {
			auto& eq = equations.equations[eq_i];

			if (dbg) {
				dbg_equation(eq);
				ImGui::Separator();
			}

			// only plot functions with zero or one arguments (plot f(b) for convinience even though b!=x)
			// don't plot f=5 for example
			bool show = eq.enable && eq.valid && !eq.def.is_variable && eq.def.arg_map.size() <= 1;
			if (!show) continue;

			ZoneScopedN("draw equation");

			auto eval_func = [&] (Equation& eq, float x, float* result) {
				if (axes[0].units->log)
					x = powf(10.0f, x);

				eq.exec_valid = eval.execute(eq.def, eq.ops, x, result, &eq.last_err);

				if (axes[1].units->log)
					*result = log10f(*result);

			};

			eq_lines[eq_i] = lines.begin_draw(eq.line_w);

			if (!eq.exec_valid) continue;

			float prev_x = (float)start * res;
			float prev_y;
			eval_func(eq, prev_x, &prev_y);

			for (int i=start+1; eq.exec_valid && i<=end; ++i) {
				float plot_x = (float)i * res;
				float plot_y;
				eval_func(eq, plot_x, &plot_y);

				if (!eq.exec_valid) break;

				if (!isnan(prev_y) && !isnan(plot_y)) {
					eq_lines[eq_i].vertex_count += lines.draw_line(float3(prev_x, prev_y, 0), float3(plot_x, plot_y, 0), eq.col);
					
					cursor_select_line(eq_i, float2(prev_x,prev_y), float2(plot_x,plot_y));
				}

				prev_x = plot_x;
				prev_y = plot_y;
			}
		}

		if (dbg) ImGui::TreePop();

		select_lines = lines.begin_draw(1.5f);

		ImGui::Text("nearest_dist: %7.3f nearest_eq: %d", nearest_dist, nearest_eq);
		if (nearest_dist < 20 || clicked_eq >= 0) {
			auto& eq = equations.equations[nearest_eq];

			float2 coord = nearest_point * px2world + view0;
			circles.draw(float3(coord, 0), max(eq.line_w * 2.0f * 2.00f, 5.0f), eq.col * float4(0.8f,0.8f,0.8f, 1));

			std::string str = eq.def.name.empty() ? "" : eq.def.name + "() : ";
			str.append( format_point(coord.x, coord.y) );

			text.draw_text(str, text_size, float4(0.98f,0.98f,0.98f,1),
				map_text(float3(coord, 0), view), 0, ticks_px * 1.5f);

			if (I.buttons[MOUSE_BUTTON_LEFT].went_down)
				clicked_eq = nearest_eq;
			if (!I.buttons[MOUSE_BUTTON_LEFT].is_down)
				clicked_eq = -1;

			hover_point = coord;
			hover_eq = nearest_eq;
		} else {
			hover_eq = -1;
		}
	}

	void render (Input& I, View3D const& view, int2 const& viewport_size) {
		ZoneScoped;

		r.begin(view, viewport_size);
		text.begin();
		lines.begin();
		circles.begin();

		glClearColor(0.05f, 0.06f, 0.07f, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		draw_background_grid(I, view);

		draw_equations(I, view);

		draw_axes(I, view);

		//static float size = 20;
		//ImGui::SliderFloat("size", &size, 0, 70);
		//r.text.draw_text("Hello World!\n"
		//                 "f(x) = 5x^2 + 7x -9.3", map_text(float3(0,0,0), view), size);

		lines.upload_vertices();
		lines.render(r.state, axis_lines);
		lines.render(r.state, eq_lines.data(), (int)eq_lines.size());
		lines.render(r.state, select_lines);

		circles.upload_vertices();
		circles.render(r.state);

		text.render(r.state);
	}

	virtual void frame (Input& I) {
		ZoneScoped;
		imgui(I);

		View3D view;
		if (!mode_3d) {
			view = update_2d_view(I, (float2)I.window_size);
		} else {
			view = flycam.update(I, (float2)I.window_size);
		}	
		render(I, view, I.window_size);
	}
};

inline IApp* make_app () { return new App(); }
int main () {
	return run_window(make_app, "Grapher");
}
