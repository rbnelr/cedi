
static constexpr byte UTF8_BOM[3] = { 0xef,0xbb,0xbf };

static bool load_file_skip_bom (cstr filename, std::vector<byte>* data, byte const* bom, u32 bom_len) {
	auto f = fopen(filename, "rb");
	if (!f) return false; // fail
	defer { fclose(f); };
	
	fseek(f, 0, SEEK_END);
	u64 file_size = ftell(f); // only 32 support for now
	rewind(f);
	
	if (bom && file_size >= bom_len) {
		data->resize(bom_len);
		
		fread(&(*data)[0], 1,bom_len, f);
		
		if (memcmp(&(*data)[0], bom, bom_len) == 0) {
			file_size -= bom_len;
		} else {
			rewind(f);
		}
	}
	
	data->resize(file_size);
	
	auto ret = fread(&(*data)[0], 1,file_size, f);
	dbg_assert(ret == file_size);
	file_size = ret;
	
	return true;
}
static bool load_file (cstr filename, std::vector<byte>* data) {
	return load_file_skip_bom(filename, data, nullptr, 0);
}

static utf32 utf8_to_utf32 (utf8 const** cur) {
	
	if ((*(*cur) & 0b10000000) == 0b00000000) {
		return (utf32)(u8)(*(*cur)++);
	}
	if ((*(*cur) & 0b11100000) == 0b11000000) {
		dbg_assert(((*cur)[1] & 0b11000000) == 0b10000000);
		auto a = (utf32)(u8)(*(*cur)++ & 0b00011111);
		auto b = (utf32)(u8)(*(*cur)++ & 0b00111111);
		return a<<6|b;
	}
	if ((*(*cur) & 0b11110000) == 0b11100000) {
		dbg_assert(((*cur)[1] & 0b11000000) == 0b10000000);
		dbg_assert(((*cur)[2] & 0b11000000) == 0b10000000);
		auto a = (utf32)(u8)(*(*cur)++ & 0b00001111);
		auto b = (utf32)(u8)(*(*cur)++ & 0b00111111);
		auto c = (utf32)(u8)(*(*cur)++ & 0b00111111);
		return a<<12|b<<6|c;
	}
	if ((*(*cur) & 0b11111000) == 0b11110000) {
		dbg_assert(((*cur)[1] & 0b11000000) == 0b10000000);
		dbg_assert(((*cur)[2] & 0b11000000) == 0b10000000);
		dbg_assert(((*cur)[3] & 0b11000000) == 0b10000000);
		auto a = (utf32)(u8)(*(*cur)++ & 0b00000111);
		auto b = (utf32)(u8)(*(*cur)++ & 0b00111111);
		auto c = (utf32)(u8)(*(*cur)++ & 0b00111111);
		auto d = (utf32)(u8)(*(*cur)++ & 0b00111111);
		return a<<18|b<<12|c<<6|d;
	}
	dbg_assert(false);
	
	return (utf32)(u32)-1;
}
