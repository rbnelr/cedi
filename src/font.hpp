
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace font {
	
	struct Glyph_Range {
		cstr	override_fontname; // nullptr -> use user specified font else always use specified font
		
		stbtt_pack_range	pr;
		
		Glyph_Range (cstr override_font, f32 font_size, utf32 first, utf32 last): override_fontname{override_font}, pr{font_size, (int)first, nullptr, (int)(last +1 -first), nullptr} {
			
		}
		Glyph_Range (cstr override_font, f32 font_size, std::initializer_list<utf32> l): override_fontname{override_font}, pr{font_size, 0, (int*)l.begin(), (int)l.size(), nullptr} {
			
		}
	};
	
	static std::initializer_list<utf32> ger = { U'ß',U'Ä',U'Ö',U'Ü',U'ä',U'ö',U'ü' };
	static std::initializer_list<utf32> jp_sym = { U'　',U'、',U'。',U'”',U'「',U'」' };
	
	f32 sz = 24; // 14 16 24
	f32 jpsz = floor(sz * 1.75f);
	
	static std::initializer_list<Glyph_Range> ranges = {
		{ nullptr,		sz,		U'\xfffd', U'\xfffd' }, // missing glyph placeholder
		//{ "arial.ttf",	sz,		U'\0', U'\x1f' }, // control characters // does not work for some reason, even though FontForge shows that these glyphs exist at least in arial.ttf
		{ nullptr,		sz,		U' ', U'~' },
		//{ nullptr,		sz,		U'\x0', U'\x7f' }, // all ascii
		{ nullptr,		sz,		ger },
		{ "meiryo.ttc",	jpsz,	U'\x3040', U'\x30ff' }, // hiragana +katakana +some jp puncuation
		{ "meiryo.ttc",	jpsz,	jp_sym },
	};
	
	static u32 texw = 512; // hopefully large enough for now, if not 
	static u32 texh = 512;
	
	static constexpr v2 QUAD_VERTS[] = {
		v2(1,0), // MSVC claims this is not a constexpr when i put this arr into the Font struct, but it worked before ???
		v2(1,1),
		v2(0,0),
		v2(0,0),
		v2(1,1),
		v2(0,1),
	};
	
	struct Font {
		Texture					tex;
		VBO_Pos_Tex_Col			vbo;
		
		u32						glyphs_count;
		stbtt_packedchar*		glyphs_packed_chars;
		
		f32 border_left;
		
		f32 ascent_plus_gap;
		f32 descent_plus_gap;
		
		f32 line_height;
		
		bool init (cstr latin_filename) {
			
			vbo.init();
			tex.alloc(texw, texh);
			
			cstr fonts_folder = "c:/windows/fonts/";
			
			struct Loaded_Font_File {
				cstr			filename;
				File_Data		f;
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
					
					if (cur == 0) {
						stbtt_fontinfo info;
						dbg_assert( stbtt_InitFont(&info, font_file->f.data, 0) );
						
						f32 scale = stbtt_ScaleForPixelHeight(&info, sz);
						
						s32 ascent, descent, line_gap;
						stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
						
						s32 x0, x1, y0, y1;
						stbtt_GetFontBoundingBox(&info, &x0, &y0, &x1, &y1);
						
						//border_left = -x0*scale;
						border_left = 0;
						
						line_height = ceil(ascent*scale -descent*scale +line_gap*scale); // ceil, so that lines are always seperated by exactly n pixels (else lines would get rounded to a y pos, which would result in uneven spacing)
						
						f32 ceiled_line_gap = line_height -(ascent*scale -descent*scale);
						
						ascent_plus_gap = +ascent*scale +ceiled_line_gap/2;
						descent_plus_gap = -descent*scale +ceiled_line_gap/2;
						
						printf(">>> %f %f %f %f\n", border_left, ascent_plus_gap, descent_plus_gap, line_height);
						
					}
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
			
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,	GL_LINEAR_MIPMAP_LINEAR);
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,	GL_LINEAR);
			
			return true;
		}
		
		static int search_glyph (utf32 c) {
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
		
		f32 emit_glyph (std::vector<VBO_Pos_Tex_Col::V>* vbo_buf, f32 pos_x_px, f32 pos_y_px, utf32 c, v4 col) {
			
			stbtt_aligned_quad quad;
			
			stbtt_GetPackedQuad(glyphs_packed_chars, (s32)tex.w,(s32)tex.h, search_glyph(c),
					&pos_x_px,&pos_y_px, &quad, 1);
			
			for (v2 quad_vert : QUAD_VERTS) {
				vbo_buf->push_back({
					/*pos*/ lerp(v2(quad.x0,quad.y0), v2(quad.x1,quad.y1), quad_vert),
					/*uv*/ lerp(v2(quad.s0,-quad.t0), v2(quad.s1,-quad.t1), quad_vert),
				/*col*/ col });
			}
			
			return pos_x_px;
		};
		
		void draw_emitted_glyphs (Shader_Clip_Tex_Col cr shad, std::vector<VBO_Pos_Tex_Col::V>* vbo_buf) {
			
			if (1) { // show texture
				v2 left_bottom =	v2(wnd_dim.x -(f32)tex.w, (f32)tex.h);
				v2 right_top =		v2(wnd_dim.x, 0);
				for (v2 quad_vert : QUAD_VERTS) {
					vbo_buf->push_back({
						/*pos*/ lerp(left_bottom, right_top, quad_vert),
						/*uv*/ quad_vert,
						/*col*/ 1 });
				}
			}
			
			vbo.upload(*vbo_buf);
			vbo.bind(shad);
			
			glDrawArrays(GL_TRIANGLES, 0, vbo_buf->size());
		};
		
		#if 0
		void draw_line (Basic_Shader cr shad, std::basic_string<utf32> cr text) {
			
			u32 char_i=0;
			
			for (utf32 c : text) {
				
				switch (c) {
					case U'\t': {
						s32 spaces_needed = tab_spaces -(char_i % tab_spaces);
						
						for (s32 j=0; j<spaces_needed; ++j) {
							auto c = U' ';
							if (draw_whitespace) {
								c = j<spaces_needed-1 ? U'-' : U'>';
							}
							
							emit_glyph(c, base_col*v4(1,1,1, 0.1f));
							
							++char_i;
						}
						
					} break;
					
					case U'\n': {
						if (draw_whitespace) {
							// draw backslash and t at the same position to create a '\n' glypth
							auto tmp = pos_px;
							emit_glyph(U'\\', base_col*v4(1,1,1, 0.1f));
							pos_px = tmp;
							emit_glyph(U'n', base_col*v4(1,1,1, 0.1f));
						}
						
						pos_px.x = border_left;
						pos_px.y += line_height;
						
						++char_i;
						
					} break;
					
					default: {
						emit_glyph(c, base_col);
						++char_i;
					} break;
				}
			}
		}
		#endif
	};
	
}
