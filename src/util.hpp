
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
