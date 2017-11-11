﻿
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

static iv2 wnd_dim; // dimensions of windows window in px

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
		
		bool _visible;
		
		u32 _count_newlines () {
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
		u64 get_newlineless_len () {
			u32 newline_chars = _count_newlines();
			dbg_assert(!next || newline_chars > 0, "only last line can not end in a newline char");
			
			return text.size() -newline_chars;
		}
		
		s64 get_max_cursor_c () {
			u32 newline_chars = _count_newlines();
			dbg_assert(!next || newline_chars > 0, "only last line can not end in a newline char");
			
			u64 max_c = text.size();
			if (opt.draw_whitespace) {
				max_c -= newline_chars ? 1 : 0; // max cursor c is on the last newline char (or beyon the last character of the last line which does not have a newline char)
			} else {
				max_c -= newline_chars; // max cursor c is on the first newline char
			}
			
			return (s64)max_c;
		}
	};
	
	Line*	first_line;
	s64		line_count;
	
	s64 _search_line_i (Line* lp) {
		Line* cur = first_line;
		for (s64 i=0;; ++i) {
			if (lp == cur) {
				return i;
			}
			cur = cur->next;
		}
		dbg_assert(false);
		return 0;
	}
	bool _linep_matched_li (Line* lp, s64 li) {
		return _search_line_i(lp) == li;
	}
	Line* _inc_lp (Line* lp, s64 diff) {
		if (diff > 0) {
			for (s64 i=0; i<diff; ++i) {
				if (!lp) break;
				lp = lp->next;
			}
		} else {
			for (s64 i=0; i<-diff; ++i) {
				if (!lp) break;
				lp = lp->prev;
			}
		}
		return lp;
	}
	
	struct Cursor {
		Line*	lp; // line the cursor is in (use this for logic)
		s64		l; // mainly use this var for debggung and diplaying to the user (getting l from a linked list is slow)
		
		s64		c; // char index the cursor is on (cursor appears on the left edge of the char it's on)
		
	};
	
	Cursor	cursor;
	Cursor	select_cursor; // select_cursor.lp == nullptr -> not in selecting state
	
	iv2		sub_wnd_dim;
	
	// scrolling
	s64		scroll; // index of first line visible in text buffer window (from the top) (can overscroll, then this will be negative)
	Line*	first_visible_line;
	
	s64 get_visible_line_count () {
		s64 ret = max( (s64)ceil( (f32)sub_wnd_dim.y / (f32)g_font.line_height ) -1, (s64)1 );
		ret += min(scroll, (s64)0);
		dbg_assert(ret >= 1);
		return ret;
	}
	
	#if 0
	void scroll_diff (s64 diff) {
		if (diff > 0) {
			s64 first_visible_line_diff = max( (s64)0, diff +min(scroll, (s64)0) );
			for (s64 i=0; i<first_visible_line_diff; ++i) {
				if (!first_visible_line->next) break;
				first_visible_line = first_visible_line->next;
			}
		} else {
			for (s64 i=0; i<-diff; ++i) {
				if (!first_visible_line->prev) break;
				first_visible_line = first_visible_line->prev;
			}
		}
		scroll += diff;
		
		printf(">> scroll %lld diff %lld\n", scroll, diff);
		dbg_assert(_linep_matched_li(first_visible_line, max((s64)0, scroll)),
				">>> is %lld should be %lld", _search_line_i(first_visible_line), max((s64)0, scroll));
	}
	void update_scroll () {
		s64 vis = get_visible_line_count();
		
		s64 new_scroll = scroll;
		new_scroll = min(new_scroll, cursor.l);
		new_scroll = max(new_scroll +(vis -1), cursor.l) -(vis -1);
		
		scroll_diff(new_scroll -scroll);
	}
	#else
	void cursor_move_scroll () {
		
	}
	void pageup_scroll () {
		
	}
	void pagedown_scroll () {
		
	}
	void mouse_scroll (s32 diff) {
		diff = -diff;
		
		if (diff > 0) {
			s64 first_visible_line_diff = max( (s64)0, diff +min(scroll, (s64)0) );
			for (s64 i=0; i<first_visible_line_diff; ++i) {
				if (!first_visible_line->next) break;
				first_visible_line = first_visible_line->next;
			}
		} else {
			for (s64 i=0; i<-diff; ++i) {
				if (!first_visible_line->prev) break;
				first_visible_line = first_visible_line->prev;
			}
		}
		scroll += diff;
		
	}
	#endif
	
	std::vector<VBO_Text::V> vbo_char_vert_data;
	
	void init () {
		first_line = new Line;
		first_line->next = nullptr;
		first_line->prev = nullptr;
		
		line_count = 1;
		
		cursor.lp = first_line;
		cursor.l = 0;
		cursor.c = 0;
		
		scroll = 0;
		first_visible_line = first_line;
		
		//open_file("src/cedi.cpp");
		open_file("build.bat");
	}
	void free_lines () {
		Line* cur = first_line;
		first_line = nullptr;
		do {
			Line* tmp = cur->next;
			delete cur;
			cur = tmp;
		} while (cur);
	}
	void init_from_str (utf8 const* str, u64 len) {
		
		free_lines();
		
		first_line = new Line;
		first_line->prev = nullptr;
		line_count = 1;
		
		Line* cur_line = first_line;
		
		auto* in = str;
		while (in != (str +len)) {
			utf32 c = utf8_to_utf32(&in);
			
			cur_line->text.push_back( c );
			
			if (c == U'\n' || c == U'\r') {
				if (in != (str +len) && (*in == '\n' || *in == '\r') && (utf32)*in != c) {
					cur_line->text.push_back( utf8_to_utf32(&in) );
				}
				
				Line* next_line = new Line;
				++line_count;
				
				cur_line->next = next_line;
				next_line->prev = cur_line;
				
				cur_line = next_line;
			}
		}
		
		cur_line->next = nullptr; // cur_line == last line
		
		cursor.lp = first_line;
		cursor.l = 0;
		cursor.c = 0;
		
		scroll = 0;
		first_visible_line = first_line;
	}
	
	void gen_chars_pos_and_vbo_data () {
		
		vbo_char_vert_data.clear();
		
		f32	pos_x_px;
		f32	pos_y_px = g_font.ascent_plus_gap +opt.tex_buffer_margin +(f32)((s64)g_font.line_height * -scroll);
		
		auto* cur_line = first_line;
		u32	line_i=0;
		do {
			pos_x_px = g_font.border_left +opt.tex_buffer_margin;
			
			auto emit_glyph = [&] (utf32 c, v3 col) {
				
				if (cur_line == first_visible_line)
					col *= v3(1,0,0);
				if (cur_line == _inc_lp(first_visible_line, get_visible_line_count() -1))
					col *= v3(0,0,1);
				
				pos_x_px = g_font.emit_glyph(&vbo_char_vert_data, pos_x_px,pos_y_px, c, v4(col,1));
			};
			
			{ // emit line numbers
				u64 digit_count = 0;
				{
					dbg_assert(line_count > 0);
					u64 num = line_count -1;
					while (num != 0) {
						num /= 10;
						++digit_count;
					}
					digit_count = max(digit_count, (u64)1);
				}
				
				u64 num = line_i;
				utf32 buf[32];
				u64 num_len = 0;
				for (; num_len<digit_count; ++num_len) {
					if (num_len > 0 && num == 0) break;
					buf[num_len] = num % 10;
					num /= 10;
				}
				num_len = max(num_len, (u64)1);
				
				for (u64 i=digit_count; i!=0;) { --i;
					emit_glyph(i < num_len ? U'0' +buf[i] : U' ', opt.col_line_numbers);
				}
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
						u32 spaces_needed = opt.tab_spaces -(tab_char_i % opt.tab_spaces);
						
						for (u32 j=0; j<spaces_needed; ++j) {
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
		f32 x;
		
			x = cursor.lp->chars_pos_px[ cursor.c ];
			
			if (cursor.c < (cursor.lp->chars_pos_px.size() -1)) {
				w = cursor.lp->chars_pos_px[ cursor.c +1 ] -x; // could be imaginary last character
			}
			// w might end up zero because either the final few chars on the line are invisible (newline because draw_whitespace is off) or is not a character (end of file)
			w *= opt.min_cursor_w_percent_of_char;
		
		w = max(w, opt.min_cursor_w_px);
		
		return {	v2(x -g_font.border_left, cursor.lp->pos_y -g_font.line_height +g_font.descent_plus_gap),
					v2(w, g_font.line_height) };
	}
	
	//
	void move_cursor_left () {
		// when the cursor is sticking beyond the line then left moves the cursor onto the last proper char position (makes the cursor non-sticking)
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
	}
	
	void _cursor_vertical (Line* dst, Line* src) {
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
		
		++line_count;
		
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
		if (cursor.lp != newline_l) --cursor.l;
		cursor.lp = prev;
		cursor.c = (s32)prev->text.size();
		
		// Merge line text
		prev->text.insert(prev->text.end(), cur->text.begin(), cur->text.end());
		
		delete cur;
		--line_count;
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
	
	void open_file (cstr filename) {
		std::vector<byte> tmp;
		if (load_file_skip_bom(filename, &tmp, UTF8_BOM, arrlen(UTF8_BOM))) {
			init_from_str((utf8*)&tmp[0], tmp.size());
			printf("done.\n");
		} else {
			printf("Could not open file '%s'!\n", filename);
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
	g_font.init("consola.ttf");
	
	shad_text			.init();
	shad_text_copy		.init();
	shad_cursor_pass	.init();
	
	vbo_cursor			.init();
	
	fb_text				.init();
	
	g_buf.init();
	
	{ // show window
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
	}
	
	draw();
}

static void resize_wnd (iv2 dim) {
	wnd_dim = dim;
	g_buf.sub_wnd_dim = dim;
	
	draw();
}

f64 last_t;
static void draw () {
	
	auto t = glfwGetTime();
	printf(">>> frame %f\n", (t -last_t) * 1000);
	last_t = t;
	
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

// input events
static void move_cursor_left () {	g_buf.move_cursor_left();	}
static void move_cursor_right () {	g_buf.move_cursor_right();	}
static void move_cursor_up () {		g_buf.move_cursor_up();		}
static void move_cursor_down () {	g_buf.move_cursor_down();	}
static void mouse_scroll (s32 diff) {	g_buf.mouse_scroll(diff);	}

static void insert_char (utf32 c) {	g_buf.insert_char(c);		}
static void insert_tab () {			g_buf.insert_tab();			}
static void insert_newline () {		g_buf.insert_newline();		}
static void delete_prev_char () {	g_buf.delete_prev_char();	}
static void delete_next_char () {	g_buf.delete_next_char();	}

static void open_file (cstr filename) { g_buf.open_file(filename); }
