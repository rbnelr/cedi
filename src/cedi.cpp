
#define _USING_V110_SDK71_ 1
#include "windows.h"

#undef min
#undef max

#include <cstdio>

#include "lang_helpers.hpp"
#include "math.hpp"

#if RZ_COMP == RZ_COMP_GCC
	// gcc compiler bug with constexpr:
	// internal compiler error: in cxx_eval_constant_expression, at cp/constexpr.c:3503
	//  static Options opt;
	#define constexpr 
#else
	
#endif

#include "vector/vector.hpp"

typedef s32v2	iv2;
typedef s32v3	iv3;
typedef s32v4	iv4;
typedef fv2		v2;
typedef fv3		v3;
typedef fv4		v4;
typedef fm2		m2;
typedef fm3		m3;
typedef fm4		m4;

#include "gl.hpp"
#include "util.hpp"

struct Options {
	v3		col_background =				srgb(41,49,52);
	v3		col_text =						srgb(244,246,248);
	v3		col_text_highlighted =			srgb(20);
	v3		col_draw_whitespace =			col_text * 0.2f;
	v3		col_line_numbers =				col_text * 0.4f;
	v3		col_line_numbers_bar =			col_text * 0.2f;
	v3		col_cursor =					srgb(147,199,99);
	
	bool	draw_whitespace =				true;
	
	s32		tab_spaces =					4;
	
	f32		min_cursor_w_percent_of_char =	0 ? 0.25f : 1;
	f32		min_cursor_w_px =				6;
	
	f32		tex_buffer_margin =				4;
};

static Options opt;

#include "glfw_engine.hpp"
#include "font.hpp"

//
static font::Font			g_font; // one font for everything for now

struct Text_Buffer { // A buffer (think file) that the editor can display, it contains lines of text
	struct Line {
		Line*	next;
		Line*	prev;
		
		std::vector<utf32>	text; // can contain U'\0' since we want to be able to handle files with null termintors in them
		
		f32		pos_y;
		std::vector<f32>		chars_pos_px;
		
		s32 _count_newlines () {
			auto len = text.size();
			if (len == 0) return 0;
			
			utf32 a = text[len -1];
			if (a != '\n' && a != '\r') return 0;
			
			if (len == 1) return 1;
			
			utf32 b = text[len -2];
			if (b != '\n' && b != '\r') return 1;
			
			dbg_assert(a != b);
			return 2;
		}
		s32 get_newlineless_len () {
			s32 newline_chars = _count_newlines();
			dbg_assert(!next || newline_chars > 0, "only last line can not end in a newline char");
			
			return text.size() -newline_chars;
		}
		
		s32 get_max_cursor_c () {
			s32 newline_chars = _count_newlines();
			dbg_assert(!next || newline_chars > 0, "only last line can not end in a newline char");
			
			s32 max_c = text.size();
			if (opt.draw_whitespace) {
				max_c -= newline_chars ? 1 : 0; // max cursor c is on the last newline char (or beyon the last character of the last line which does not have a newline char)
			} else {
				max_c -= newline_chars; // max cursor c is on the first newline char
			}
			
			return max_c;
		}
	};
	
	Line*	first_line;
	
	struct Cursor {
		Line*	lp; // line the cursor is in (use this for logic)
		s32		l; // mainly use this var for debggung and diplaying to the user (getting l from a linked list is slow)
		
		s32		c; // char index the cursor is on (cursor appears on the left edge of the char it's on)
		//f32		stick_x; // always stores the x position the cursor is sticking at
		//
		//bool is_sticking_beyond_line () {
		//	f32 line_max_x = lp->chars_pos_px[ lp->text.size() ];
		//	return stick_x > line_max_x;
		//}
	};
	
	Cursor	cursor;
	Cursor	select_cursor; // select_cursor.lp == nullptr -> not in selecting state
	
	std::vector<VBO_Text::V> vbo_char_vert_data;
	
	void _dbg_init_from_str (utf32 const* str, u32 len) {
		
		first_line = new Line;
		first_line->prev = nullptr;
		
		Line* cur_line = first_line;
		
		auto* in = str;
		while (in != (str +len)) {
			utf32 c = *in++;
			
			cur_line->text.push_back( c );
			
			if (c == U'\n' || c == U'\r') {
				if ((*in == U'\n' || *in == U'\r') && *in != c) {
					cur_line->text.push_back( *in++ );
				}
				
				Line* next_line = new Line;
				
				cur_line->next = next_line;
				next_line->prev = cur_line;
				
				cur_line = next_line;
			}
		}
		
		cur_line->next = nullptr; // cur_line == last line
		
		cursor.lp = first_line;
	}
	
	void gen_chars_pos_and_vbo_data () {
		
		vbo_char_vert_data.clear();
		
		f32	pos_x_px;
		f32	pos_y_px = g_font.ascent_plus_gap +opt.tex_buffer_margin;
		
		auto* cur_line = first_line;
		u32	line_i=0;
		do {
			pos_x_px = g_font.border_left +opt.tex_buffer_margin;
			
			auto emit_glyph = [&] (utf32 c, v3 col) {
				pos_x_px = g_font.emit_glyph(&vbo_char_vert_data, pos_x_px,pos_y_px, c, v4(col,1));
			};
			
			{ // emit line numbers
				emit_glyph(U'0' +(line_i / 10) % 10, opt.col_line_numbers);
				emit_glyph(U'0' +(line_i /  1) % 10, opt.col_line_numbers);
				emit_glyph(U'|', opt.col_line_numbers_bar);
			}
			
			cur_line->pos_y = pos_y_px;
			cur_line->chars_pos_px.clear();
			
			u32 tab_char_i=0;
			
			auto draw_escaped_char = [&] (utf32 c) {
				auto tmp = pos_x_px;
				emit_glyph(U'\\', opt.col_draw_whitespace);
				pos_x_px = lerp(tmp, pos_x_px, 0.6f); // squash \ and c closer together to make it seem like 1 glyph
				
				emit_glyph(c, opt.col_draw_whitespace);
				++tab_char_i;
			};
			
			for (s32 char_i=0; char_i<(s32)cur_line->text.size(); ++char_i) {
				utf32 c = cur_line->text[ char_i ];
				
				cur_line->chars_pos_px.push_back(pos_x_px);
				
				switch (c) {
					case U'\t': {
						s32 spaces_needed = opt.tab_spaces -(tab_char_i % opt.tab_spaces);
						
						for (s32 j=0; j<spaces_needed; ++j) {
							auto c = U' ';
							if (opt.draw_whitespace) {
								c = j<spaces_needed-1 ? U'—' : U'→';
							}
							
							emit_glyph(c, opt.col_draw_whitespace);
							
							++tab_char_i;
						}
						
					} break;
					
					case U'\n': {
						if (opt.draw_whitespace) draw_escaped_char(U'n');
					} break;
					case U'\r': {
						if (opt.draw_whitespace) draw_escaped_char(U'r');
					} break;
					case U'\0': {
						draw_escaped_char(U'0');
					} break;
					
					case U' ': {
						if (opt.draw_whitespace) {
							emit_glyph(U'·', opt.col_draw_whitespace);
						} else {
							emit_glyph(c, opt.col_text);
						}
						
						++tab_char_i;
					} break;
					
					default: {
						emit_glyph(c, opt.col_text);
						++tab_char_i;
					} break;
				}
			}
			
			cur_line->chars_pos_px.push_back(pos_x_px); // push char pos for imaginary last character, to be able to determine width of last char on line
			
			//
			cur_line = cur_line->next;
			
			pos_y_px += g_font.line_height;
			
			++line_i;
		} while (cur_line);
		
	}
	
	struct Cursor_Rect {
		v2 pos;
		v2 dim;
	};
	
	Cursor_Rect get_cursor_rect () {
		f32 w = 0;
		//f32 x = cursor.stick_x;
		f32 x;
		
		//if (!cursor.is_sticking_beyond_line()) {
			x = cursor.lp->chars_pos_px[ cursor.c ];
			
			if (cursor.c < ((s32)cursor.lp->chars_pos_px.size() -1)) {
				w = cursor.lp->chars_pos_px[ cursor.c +1 ] -x; // could be imaginary last character
			}
			// w might end up zero because either the final few chars on the line are invisible (newline because draw_whitespace is off) or is not a character (end of file)
			w *= opt.min_cursor_w_percent_of_char;
		//}
		
		w = max(w, opt.min_cursor_w_px);
		
		return {	v2(x -g_font.border_left, cursor.lp->pos_y -g_font.line_height +g_font.descent_plus_gap),
					v2(w, g_font.line_height) };
	}
	
	//
	void move_cursor_left () {
		// when the cursor is sticking beyond the line then left moves the cursor onto the last proper char position (makes the cursor non-sticking)
		//if (!cursor.is_sticking_beyond_line()) {
			
			if (cursor.c > 0) {
				--cursor.c;
			} else {
				if (cursor.lp->prev) {
					dbg_assert(cursor.l > 0);
					--cursor.l;
					cursor.lp = cursor.lp->prev;
					cursor.c = cursor.lp->get_max_cursor_c();
				}
			}
			
		//}
		
		//cursor.stick_x = cursor.lp->chars_pos_px[ cursor.c ];
	}
	void move_cursor_right () {
		if (cursor.c < cursor.lp->get_max_cursor_c()) {
			++cursor.c;
		} else {
			if (cursor.lp->next) {
				++cursor.l;
				cursor.lp = cursor.lp->next;
				cursor.c = 0;
			}
		}
		
		//cursor.stick_x = cursor.lp->chars_pos_px[ cursor.c ];
	}
	
	//static s32 nearest_char_to_cur_char_x (Line* dst, f32 x) {
	//	dbg_assert(dst->chars_pos_px.size() >= 1);
	//	
	//	f32 nearest_dist = +INF;
	//	s32 nearest_i;
	//	
	//	s32 i=0;
	//	for (; i<(s32)dst->chars_pos_px.size(); ++i) {
	//		f32 dist = abs(dst->chars_pos_px[i] -x);
	//		if (dist < nearest_dist) {
	//			nearest_dist = dist;
	//			nearest_i = i;
	//		}
	//	}
	//	
	//	return nearest_i;
	//}
	
	void _cursor_vertical (Line* dst, Line* src) {
		//cursor.c = min( dst->get_max_cursor_c(), nearest_char_to_cur_char_x(dst, cursor.stick_x) );
		cursor.c = min(dst->get_max_cursor_c(), cursor.c);
		cursor.lp = dst;
	}
	
	void move_cursor_up () {
		Line* cur_line = cursor.lp;
		Line* dst_line = cursor.lp->prev;
		if (dst_line) {
			--cursor.l;
			_cursor_vertical(dst_line, cur_line);
		}
	}
	void move_cursor_down () {
		Line* cur_line = cursor.lp;
		Line* dst_line = cursor.lp->next;
		if (dst_line) {
			++cursor.l;
			_cursor_vertical(dst_line, cur_line);
		}
	}
	
	void insert_char (utf32 c) {
		cursor.lp->text.insert(cursor.lp->text.begin() +cursor.c, c);
		++cursor.c;
	}
	void insert_tab () {
		insert_char(U'\t');
	}
	void insert_newline () {
		// insert line after current line
		Line* prev = cursor.lp;
		Line* new_ = new Line;
		Line* next = cursor.lp->next;
		
		if (prev) prev->next = new_;
		
		new_->prev = prev;
		new_->next = next;
		
		if (next) next->prev = new_;
		
		{ // Move chars afte cursor to new line
			auto b = cursor.lp->text.begin() +cursor.c;
			auto e = cursor.lp->text.end();
			
			new_->text.insert(new_->text.begin(), b, e);
			cursor.lp->text.erase(b, e);
		}
		// insert newline char
		cursor.lp->text.push_back(U'\n');
		// move cursor to new line
		cursor.lp = new_;
		++cursor.l;
		cursor.c = 0;
	}
	
	void newline_delete_merge_lines (Line* newline_l) {
		// marge two lines by deleting newline
		dbg_assert(newline_l && newline_l->next);
		
		Line* prev = newline_l;
		Line* cur = prev->next;
		Line* next = cur->next;
		
		prev->next = next;
		
		if (next) next->prev = prev;
		
		// delete prev newline char
		prev->text.erase(	prev->text.begin() +prev->get_newlineless_len(),
							prev->text.begin() +prev->text.size());
		
		// move cursor to end of prev line
		cursor.lp = prev;
		--cursor.l;
		cursor.c = (s32)prev->text.size();
		
		// Merge line text
		prev->text.insert(prev->text.end(), cur->text.begin(), cur->text.end());
		
		delete cur;
	}
	
	void delete_prev_char () {
		if (cursor.c > 0) {
			// TODO: need to handle case of last line here
			cursor.lp->text.erase(cursor.lp->text.begin() +(cursor.c -1));
			--cursor.c;
		} else {
			if (cursor.lp->prev) {
				// marge line with previous line
				newline_delete_merge_lines(cursor.lp->prev);
			}
		}
	}
	void delete_next_char () {
		if (cursor.c < cursor.lp->get_newlineless_len()) {
			cursor.lp->text.erase(cursor.lp->text.begin() +cursor.c);
		} else {
			if (cursor.lp->next) {
				// marge line with next line
				newline_delete_merge_lines(cursor.lp);
			}
		}
	}
};

Text_Buffer g_buf; // init to zero/null

#define TEST U"test Tst\n123\nかきくけこ　ゴゴごご\nあべし\nABeShi\n"
#define C_PROG \
	U"\n" \
	U"#include <stdio.h>\n" \
	U"\n" \
	U"int main (int argc, char** argv) {\n" \
	U"	\n" \
	U"	printf(\"Hello World!\\n\");\n" \
	U"	\n" \
	U"	return 0;\n" \
	U"}\n"
#define C_PROG2 \
	U"\n" \
	U"#include <stdio.h>\r\n" \
	U"int main (int argc, char** argv) {\n" \
	U"\0	printf(\"Hello World!\\n\");\r\n" \
	U"	char ああああああああああああ = 'A';\r\n" \
	U" 	return 0;\n" \
	U"}\r" \
	U"\0blah"
#define TEST2 U"static void insert_char (utf32 c) {\n"

static Shader_Text					shad_text;
static Shader_Fullscreen_Tex_Copy	shad_text_copy;
static Shader_Cursor_Pass			shad_cursor_pass;

static VBO_Cursor_Pass		vbo_cursor;

typedef VBO_Cursor_Pass::V Vertex;

static RGBA_Framebuffer		fb_text;

static void pre_update ();
static void draw ();

static void init  () {
	auto mr = get_monitor_rect();
	#if 0
	s32 border_top =		31;			// window title
	s32 border_bottom =		8 +29;		// window bottom border +taskbar
	s32 border_right =		8;			// window right border
	s32 h = mr.dim.y -border_top -border_bottom;
	Rect r = {mr.pos +iv2(mr.dim.x,border_top) -iv2(h +border_right, 0), h};
	#else
	s32 border_top =		31;			// window title
	s32 border_bottom =		8 +29;		// window bottom border +taskbar
	s32 border_left =		8;			// window right border
	s32 h = mr.dim.y -border_top -border_bottom;
	Rect r = {mr.pos +iv2(border_left,border_top), h};
	#endif
	init_show_window(false, r);
	
	glfwSetWindowTitle(wnd, u8"cedi");
	
	g_font.init("consola.ttf");
	
	shad_text			.init();
	shad_text_copy		.init();
	shad_cursor_pass	.init();
	
	vbo_cursor			.init();
	
	fb_text				.init();
	
	{
		utf32 str[] = C_PROG;
		g_buf._dbg_init_from_str( str, arrlen(str) -1 );
	}
	
	g_buf.cursor.l = 3;
	g_buf.cursor.lp = g_buf.first_line->next->next->next;
	
	pre_update();
	draw();
}

static void pre_update () {
	platform_get_frame_input(); // update 
	
}

static void draw () {
	
	g_buf.gen_chars_pos_and_vbo_data();
	auto cursor_rect = g_buf.get_cursor_rect();
	
	{ // text pass
		fb_text.bind_and_clear(wnd_dim, v4(0));
		
		shad_text.bind();
		shad_text.wnd_dim.set( (v2)wnd_dim );
		shad_text.bind_texture(g_font.tex);
		
		g_font.draw_emitted_glyphs(shad_text, &g_buf.vbo_char_vert_data);
	}
	
	{ // draw cursor
		bind_backbuffer(wnd_dim);
		clear_framebuffer(v4(opt.col_background,0));
		
		//
		shad_text_copy.bind();
		shad_text_copy.bind_fb(fb_text);
		shad_text_copy.wnd_dim.set( (v2)wnd_dim );
		
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		
		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		//
		shad_cursor_pass.bind();
		shad_cursor_pass.bind_fb(fb_text);
		shad_cursor_pass.wnd_dim.set( (v2)wnd_dim );
		shad_cursor_pass.col_background.set( opt.col_background );
		shad_cursor_pass.col_highlighted.set( opt.col_text_highlighted );
		
		{
			auto& r = cursor_rect;
			
			std::initializer_list<Vertex> data = {
				{ r.pos +r.dim * v2(1,0), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(1,1), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(0,0), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(0,0), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(1,1), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(0,1), v4(opt.col_cursor,1) },
			};
			
			vbo_cursor.upload(data);
			vbo_cursor.bind(shad_cursor_pass);
			
			glDrawArrays(GL_TRIANGLES, 0, data.size());
		}
	}
	
	platform_present_frame();
}

static void refresh () { // refresh without input event
	pre_update();
	draw();
}

// input events
static void move_cursor_left () {	pre_update();	g_buf.move_cursor_left();	draw();	}
static void move_cursor_right () {	pre_update();	g_buf.move_cursor_right();	draw();	}
static void move_cursor_up () {		pre_update();	g_buf.move_cursor_up();		draw();	}
static void move_cursor_down () {	pre_update();	g_buf.move_cursor_down();	draw();	}

static void insert_char (utf32 c) {	pre_update();	g_buf.insert_char(c);		draw();	}
static void insert_tab () {			pre_update();	g_buf.insert_tab();			draw();	}
static void insert_newline () {		pre_update();	g_buf.insert_newline();		draw();	}
static void delete_prev_char () {	pre_update();	g_buf.delete_prev_char();	draw();	}
static void delete_next_char () {	pre_update();	g_buf.delete_next_char();	draw();	}
