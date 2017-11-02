
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

struct File_Data {
	byte*	data;
	u64		size;
	
	void free () { ::free(data); }
};
static File_Data load_file (cstr filename) {
	auto f = fopen(filename, "rb");
	if (!f) {
		dbg_assert(false, "fopen: file '%s' could not be opened", filename);
		return {}; // fail
	}
	defer { fclose(f); };
	
	fseek(f, 0, SEEK_END);
	u64 file_size = ftell(f); // only 32 support for now
	rewind(f);
	
	byte* data = (byte*)malloc(file_size);
	
	auto ret = fread(data, 1,file_size, f);
	dbg_assert(ret == file_size);
	file_size = ret;
	
	return {data,file_size};
}

struct Texture {
	GLuint	gl;
	u8*		data;
	u32		w;
	u32		h;
	
	void alloc (u32 w, u32 h) {
		glGenTextures(1, &gl);
		this->w = w;
		this->h = h;
		data = (u8*)::malloc(w*h*sizeof(u8));
	}
	
	u8* operator[] (u32 row) { return &data[row*w]; }
	
	void inplace_vertical_flip () {
		u8* row_a = (*this)[0];
		u8* row_b = (*this)[h -1];
		for (u32 y=0; y<(h/2); ++y) {
			dbg_assert(row_a < row_b);
			for (u32 x=0; x<w; ++x) {
				u8 tmp = row_a[x];
				row_a[x] = row_b[x];
				row_b[x] = tmp;
			}
			row_a += w;
			row_b -= w;
		}
	}
};

namespace font {
	
	struct Glyph_Range {
		cstr	override_fontname; // nullptr -> use user specified font else always use specified font
		
		stbtt_pack_range	pr;
		
		Glyph_Range (cstr override_font, f32 font_size, utf32 first, utf32 last): override_fontname{override_font}, pr{font_size, (int)first, nullptr, (int)(last +1 -first), nullptr} {
			
		}
		Glyph_Range (cstr override_font, f32 font_size, std::initializer_list<utf32> l): override_fontname{override_font}, pr{font_size, 0, (int*)l.begin(), (int)l.size(), nullptr} {
			
		}
	};
	
	#define R(first, last) (first), (last) +1 -(first)
	
	static std::initializer_list<utf32> ger = { U'ß',U'Ä',U'Ö',U'Ü',U'ä',U'ö',U'ü' };
	static std::initializer_list<utf32> jp_sym = { U'　',U'、',U'。',U'”',U'「',U'」' };
	
	f32 sz = 0 ? 16 : 24;
	f32 jpsz = 0 ? 24 : 32;
	
	static std::initializer_list<Glyph_Range> ranges = { // Could improve the packing by putting 
		{ nullptr,		sz,		U'\xfffd', U'\xfffd' },
		{ nullptr,		sz,		U' ', U'~' },
		{ nullptr,		sz,		ger },
		{ "meiryo.ttc",	jpsz,	U'\x3040', U'\x30ff' }, // hiragana +katakana +some jp puncuation
		{ "meiryo.ttc",	jpsz,	jp_sym },
	};
	
	#undef R
	
	static u32 texw = 256;
	static u32 texh = 512;
	
	struct Font {
		Texture					tex;
		VBO_Pos_Tex_Col			vbo;
		
		u32						glyphs_count;
		stbtt_packedchar*		glyphs_packed_chars;
		
		bool init (cstr latin_filename, u32 fontsize=16) {
			
			vbo.init();
			tex.alloc(texw, texh);
			
			cstr fonts_folder = "c:/windows/fonts/";
			
			struct Loaded_Font_File {
				cstr		filename;
				File_Data	f;
			};
			
			std::vector<Loaded_Font_File> loaded_files;
			
			stbtt_pack_context spc;
			stbtt_PackBegin(&spc, tex.data, (s32)tex.w,(s32)tex.h, (s32)tex.w, 1, nullptr);
			
			//stbtt_PackSetOversampling(&spc, 1,1);
			
			glyphs_count = 0;
			for (auto r : ranges) {
				dbg_assert(r.pr.num_chars > 0);
				glyphs_count += r.pr.num_chars;
			}
			glyphs_packed_chars =	(stbtt_packedchar*)malloc(	glyphs_count*sizeof(stbtt_packedchar) );
			
			u32 cur = 0;
			
			for (auto r : ranges) {
				
				cstr filename = r.override_fontname ? r.override_fontname : latin_filename;
				
				auto* font_file = lsearch(loaded_files, [&] (Loaded_Font_File* loaded) {
						return strcmp(loaded->filename, filename) == 0;
					} );
				
				auto filepath = prints("%s%s", fonts_folder, filename);
				
				if (!font_file) {
					loaded_files.push_back({ filename, load_file(filepath.c_str()) });
					font_file = &loaded_files.back();
				}
				
				r.pr.chardata_for_range = &glyphs_packed_chars[cur];
				cur += r.pr.num_chars;
				
				dbg_assert( stbtt_PackFontRanges(&spc, font_file->f.data, 0, &r.pr, 1) > 0);
				
			}
			
			stbtt_PackEnd(&spc);
			
			tex.inplace_vertical_flip(); // TODO: could get rid of this simply by flipping the uv's of the texture
			
			glBindTexture(GL_TEXTURE_2D, tex.gl);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, tex.w,tex.h, 0, GL_RED, GL_UNSIGNED_BYTE, tex.data);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL,	0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,	0);
			
			return true;
		}
		
		int search_glyph (utf32 c) {
			int cur = 0;
			for (auto r : ranges) {
				if (r.pr.array_of_unicode_codepoints) {
					for (int i=0; i<r.pr.num_chars; ++i) {
						if (c == (utf32)r.pr.array_of_unicode_codepoints[i]) return cur; // found
						++cur;
					}
				} else {
					auto first = (utf32)r.pr.first_unicode_codepoint_in_range;
					if (c >= first && (c -first) < (u32)r.pr.num_chars) return cur +(c -first); // found
					cur += (u32)r.pr.num_chars;
				}
			}
			
			dbg_assert(false, "Glyph '%c' [%x] missing in font", c, c);
			return 0; // missing glyph
		}
		
		static std::basic_string<utf32> utf8_to_utf32 (std::basic_string<utf8> cr s) {
			utf8 const* cur = &s[0];
			
			std::basic_string<utf32> ret;
			ret.reserve( s.length() ); // can never be longer than input
			
			for (;;) {
				if ((*cur & 0b10000000) == 0b00000000) {
					char c = *cur++;
					if (c == '\0') break;
					ret.push_back( c );
					continue;
				}
				if ((*cur & 0b11100000) == 0b11000000) {
					dbg_assert((cur[1] & 0b11000000) == 0b10000000);
					utf8 a = *cur++ & 0b00011111;
					utf8 b = *cur++ & 0b00111111;
					ret.push_back( a<<6|b );
					continue;
				}
				if ((*cur & 0b11110000) == 0b11100000) {
					dbg_assert((cur[1] & 0b11000000) == 0b10000000);
					dbg_assert((cur[2] & 0b11000000) == 0b10000000);
					utf8 a = *cur++ & 0b00001111;
					utf8 b = *cur++ & 0b00111111;
					utf8 c = *cur++ & 0b00111111;
					ret.push_back( a<<12|b<<6|c );
					continue;
				}
				if ((*cur & 0b11111000) == 0b11110000) {
					dbg_assert((cur[1] & 0b11000000) == 0b10000000);
					dbg_assert((cur[2] & 0b11000000) == 0b10000000);
					dbg_assert((cur[3] & 0b11000000) == 0b10000000);
					utf8 a = *cur++ & 0b00000111;
					utf8 b = *cur++ & 0b00111111;
					utf8 c = *cur++ & 0b00111111;
					utf8 d = *cur++ & 0b00111111;
					ret.push_back( a<<18|b<<12|c<<6|d );
					continue;
				}
				dbg_assert(false);
			}
			
			return ret;
		}
		
		void draw_text_line (Basic_Shader cr shad, std::basic_string<utf32> cr line, v2 pos_screen, v4 col, u32 highl_char=-1) {
			
			v2 pos = v2(pos_screen.x, pos_screen.y -wnd_dim.y);
			
			constexpr v2 QUAD_VERTS[] = {
				v2(1,0),
				v2(1,1),
				v2(0,0),
				v2(0,0),
				v2(1,1),
				v2(0,1),
			};
			
			#define SHOW_TEXTURE 1
			
			std::vector<VBO_Pos_Tex_Col::V> text_data;
			text_data.reserve( 0*line.length() * 6
					#if SHOW_TEXTURE
					+6
					#endif
					);
			
			u32 i=0;
			for (utf32 c : line) {
				
				stbtt_aligned_quad quad;
				
				stbtt_GetPackedQuad(glyphs_packed_chars, (s32)tex.w,(s32)tex.h, search_glyph(c),
						&pos.x,&pos.y, &quad, 1);
				
				for (v2 quad_vert : QUAD_VERTS) {
					text_data.push_back({
						/*pos*/ lerp(v2(quad.x0,-quad.y0), v2(quad.x1,-quad.y1), quad_vert) / (v2)wnd_dim * 2 -1,
						/*uv*/ lerp(v2(quad.s0,-quad.t0), v2(quad.s1,-quad.t1), quad_vert),
						/*col*/ highl_char == i ? v4(1,0.1f,0.1f,1) : col });
				}
				
				++i;
			}
			
			#if SHOW_TEXTURE
			for (v2 quad_vert : QUAD_VERTS) {
				text_data.push_back({
					/*pos*/ lerp( ((v2)wnd_dim -v2((f32)tex.w,(f32)tex.h)) / (v2)wnd_dim * 2 -1, 1, quad_vert),
					/*uv*/ quad_vert,
					/*col*/ col });
			}
			#endif
			
			#undef SHOW_TEXTURE
			
			vbo.upload(text_data);
			vbo.bind(shad);
			
			glDrawArrays(GL_TRIANGLES, 0, text_data.size());
			
		}
		void draw_text_line_utf8 (Basic_Shader cr shad, std::basic_string<utf8> cr text_, v2 pos_screen, v4 col) {
			auto line = utf8_to_utf32(text_);
			draw_text_line(shad, line, pos_screen, col);
		}
	};
	
}
