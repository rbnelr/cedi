
#define _USING_V110_SDK71_ 1
#include "windows.h"

#undef min
#undef max

#include <cstdio>

#include "lang_helpers.hpp"
#include "math.hpp"
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
	bool	draw_whitespace =	true;
	s32		tab_spaces =		4;
};

static Options opt;

static void init ();
static void frame ();

#include "glfw_engine.hpp"
#include "font.hpp"

//
static font::Font			g_font; // one font for everything for now

struct Text_Buffer { // A buffer (think file) that the editor can display, it contains lines of text
	struct Line {
		Line*	next;
		Line*	prev;
		
		std::vector<utf32>	text; // can contain U'\0' since we want to be able to handle files with null termintors in them
		std::vector<v2>		chars_pos_px; // seperate y pos for each char to support line wrap (one line in the file gets visually split into multiple lines if it would cross outside the buffer window)
	};
	
	Line*	first_line;
	
	struct Cursor {
		s32		l; // mainly use this var for debggung and diplaying to the user
		
		s32		c; // char index the cursor is on (cursor appears on the left edge of the char it's on)
		
		Line*	lp; // line the cursor is in (use this for logic)
	};
	
	Cursor	cursor;
	Cursor	select_cursor; // select_cursor.lp == nullptr -> not in selecting state
	
	std::vector<VBO_Pos_Tex_Col::V> vbo_char_vert_data;
	
	void _dbg_init_from_str (utf32 const* str) {
		
		first_line = new Line;
		first_line->prev = nullptr;
		
		Line* cur_line = first_line;
		
		auto* in = str;
		while (*in != U'\0') {
			utf32 c = *in++;
			
			cur_line->text.push_back( c );
			
			if (c == U'\n') {
				Line* next_line = new Line;
				
				cur_line->next = next_line;
				next_line->prev = cur_line;
				
				cur_line = next_line;
			}
		}
		
		cur_line->next = nullptr; // cur_line == last line
		
		cursor.lp = first_line;
	}
	
	struct _Cursor_Line {
		v2 a, b;
	};
	
	_Cursor_Line cursor_line;
	
	void gen_chars_pos_and_vbo_data () {
		
		vbo_char_vert_data.clear();
		
		v4 text_col = 1;
		
		f32	pos_x_px;
		f32	pos_y_px = g_font.border_top;
		
		auto* cur_line = first_line;
		u32	line_i=0;
		do {
			pos_x_px = g_font.border_left;
			
			auto emit_glyph = [&] (utf32 c, v4 col) {
				pos_x_px = g_font.emit_glyph(&vbo_char_vert_data, pos_x_px,pos_y_px, c, col);
			};
			
			{ // emit line numbers
				emit_glyph(U'0' +(line_i / 10) % 10, text_col*v4(1,1,1, 0.25f));
				emit_glyph(U'0' +(line_i /  1) % 10, text_col*v4(1,1,1, 0.25f));
				emit_glyph(U'|', text_col*v4(1,1,1, 0.1f));
			}
			
			cur_line->chars_pos_px.clear();
			
			u32 char_i=0;
			for (utf32 c : cur_line->text) {
				
				cur_line->chars_pos_px.push_back( v2(pos_x_px, pos_y_px) );
				
				switch (c) {
					case U'\t': {
						s32 spaces_needed = opt.tab_spaces -(char_i % opt.tab_spaces);
						
						for (s32 j=0; j<spaces_needed; ++j) {
							auto c = U' ';
							if (opt.draw_whitespace) {
								c = j<spaces_needed-1 ? U'-' : U'>';
							}
							
							emit_glyph(c, text_col*v4(1,1,1, 0.1f));
							
							++char_i;
						}
						
					} break;
					
					case U'\n': {
						if (opt.draw_whitespace) {
							// draw backslash and t at the same position to create a '\n' glypth
							auto tmp = pos_x_px;
							emit_glyph(U'\\', text_col*v4(1,1,1, 0.1f));
							pos_x_px = tmp;
							emit_glyph(U'n', text_col*v4(1,1,1, 0.1f));
						}
						
						++char_i;
					} break;
					
					default: {
						emit_glyph(c, text_col);
						++char_i;
					} break;
				}
			}
			
			cur_line->chars_pos_px.push_back( v2(pos_x_px, pos_y_px) ); // push char pos for imaginary last character
			
			//
			cur_line = cur_line->next;
			
			pos_y_px += g_font.line_height;
			
			++line_i;
		} while (cur_line);
		
		v2 pos = cursor.lp->chars_pos_px[ cursor.c ];
		
		cursor_line = { pos, v2(pos.x, pos.y -g_font.line_height) };
	}
	
	s32 max_cursor_pos_on_line (Line const* l) {
		// on each line except the last there always has to be at least a newline char
		// on last line there is no newline char -> we can place the cursor on the imaginary last character
		s32 ret = l->text.size();
		if (l->next) {
			dbg_assert(ret > 0);
			ret -= 1;
		}
		return ret; 
	}
	
	void move_cursor_left () {
		if (cursor.c > 0) {
			--cursor.c;
		} else {
			if (cursor.lp->prev) {
				dbg_assert(cursor.l > 0);
				--cursor.l;
				cursor.lp = cursor.lp->prev;
				cursor.c = max_cursor_pos_on_line(cursor.lp);
			}
		}
	}
	void move_cursor_right () {
		if (cursor.c < max_cursor_pos_on_line(cursor.lp)) {
			++cursor.c;
		} else {
			if (cursor.lp->next) {
				++cursor.l;
				cursor.lp = cursor.lp->next;
				cursor.c = 0;
			}
		}
	}
	void move_cursor_up () {
		if (cursor.lp->prev) {
			--cursor.l;
			cursor.lp = cursor.lp->prev;
			cursor.c = min(cursor.c, max_cursor_pos_on_line(cursor.lp));
		}
	}
	void move_cursor_down () {
		if (cursor.lp->next) {
			++cursor.l;
			cursor.lp = cursor.lp->next;
			cursor.c = min(cursor.c, max_cursor_pos_on_line(cursor.lp));
		}
	}
};

Text_Buffer g_buf; // init to zero/null

static void move_cursor_left () {	g_buf.move_cursor_left();	}
static void move_cursor_right () {	g_buf.move_cursor_right();	}
static void move_cursor_up () {		g_buf.move_cursor_up();		}
static void move_cursor_down () {	g_buf.move_cursor_down();	}

#define TEST U"test Tst\n123\nかきくけこ　ゴゴごご\nあべし\nABeShi\n"
#define C_PROG_ \
	U"\n" \
	U"#include <stdio.h>\n" \
	U"\n" \
	U"int main (int argc, char** argv) {\n" \
	U"	\n" \
	U"	printf(\"Hello World!\\n\");\n" \
	U"	\n" \
	U"	return 0;\n" \
	U"}\n"
#define C_PROG \
	U"\n" \
	U"#include <stdio.h>\n" \
	U"int main (int argc, char** argv) {\n" \
	U"	printf(\"Hello World!\\n\");\n" \
	U"	return 0;\n" \
	U"}\n"
#define TEST2 U"static void insert_char (utf32 c) {\n"

static constexpr cstr		APP_NAME =		u8"cedi";

static f32					running_avg_fps;

static std::basic_string<utf8>		wnd_title;

static Shader_Clip_Tex_Col	shad_tex;
static Shader_Px_Col		shad_px_col;

static VBO_Pos_Col			vbo_px_col;

typedef VBO_Pos_Col::V Vertex;

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
	
	running_avg_fps = 60; // assume 60 fps initially
	
	g_font.init("consola.ttf");
	
	shad_tex.init();
	shad_px_col.init();
	vbo_px_col.init();
	
	g_buf._dbg_init_from_str( C_PROG );
	g_buf.cursor.l = 3;
	g_buf.cursor.lp = g_buf.first_line->next->next->next;
}

static void frame () {
	
	if (frame_indx != 0) {
		f32 alpha = 0.025f;
		running_avg_fps = running_avg_fps*(1.0f -alpha) +(1.0f/dt)*alpha;
	}
	{
		prints(&wnd_title, "%s  ~%.1f fps  %.3f ms", APP_NAME, running_avg_fps, dt*1000);
		glfwSetWindowTitle(wnd, wnd_title.c_str());
	}
	
	v4 background_col = v4( srgb(41,49,52), 1 );
	
	glViewport(0, 0, wnd_dim.x, wnd_dim.y);
	
	glClearColor(background_col.x, background_col.y, background_col.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	
	{
		g_buf.gen_chars_pos_and_vbo_data();
		
		// draw buffer text
		shad_tex.bind();
		shad_tex.wnd_dim.set( (v2)wnd_dim );
		shad_tex.bind_texture(g_font.tex);
		
		g_font.draw_emitted_glyphs(shad_tex, &g_buf.vbo_char_vert_data);
		
		// draw cursor line
		shad_px_col.bind();
		shad_px_col.wnd_dim.set( (v2)wnd_dim );
		
		{
			v4 col = v4(srgb(147,199,99), 1);
			
			f32 l = -2;
			f32 r = +2;
			
			std::initializer_list<Vertex> data = {
				{ g_buf.cursor_line.a +v2(r,0), col },
				{ g_buf.cursor_line.b +v2(r,0), col },
				{ g_buf.cursor_line.a +v2(l,0), col },
				{ g_buf.cursor_line.a +v2(l,0), col },
				{ g_buf.cursor_line.b +v2(r,0), col },
				{ g_buf.cursor_line.b +v2(l,0), col },
			};
			
			vbo_px_col.upload(data);
			vbo_px_col.bind(shad_px_col);
			
			glDrawArrays(GL_TRIANGLES, 0, data.size());
		}
	}
	
}
