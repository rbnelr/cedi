
#define RZ_COMP_GCC				1
#define RZ_COMP_LLVM			2
#define RZ_COMP_MSVC			3
// Determining the compiler
#if !defined RZ_COMP
	#if _MSC_VER && !__INTELRZ_COMPILER && !__clang__
		#define RZ_COMP RZ_COMP_MSVC
	#elif __GNUC__ && !__clang__
		#define RZ_COMP RZ_COMP_GCC
	#elif __clang__
		#define RZ_COMP RZ_COMP_LLVM
	#else
		#warning Cannot determine compiler!.
	#endif
#endif

#define RZ_ARCH_X64				1
#define RZ_ARCH_ARM_CORTEX_M4	2
#define RZ_ARCH_ARM_V6_HF		3

#define RZ_PLATF_GENERIC_WIN	1
#define RZ_PLATF_GENERIC_UNIX	2
#define RZ_PLATF_NONE			3

#undef FORCEINLINE

#if RZ_COMP == RZ_COMP_MSVC
	#define FORCEINLINE						__forceinline
	#define NOINLINE						__declspec(noinline)
	#define BUILTIN_F32_INF					((float)(1e+300 * 1e+300))
	#define BUILTIN_F64_INF					(1e+300 * 1e+300)
	#define BUILTIN_F32_QNAN				__builtin_nanf("0")
	#define BUILTIN_F64_QNAN				__builtin_nan("0")
	#define DBGBREAK						__debugbreak()
	
	#define F32_INF							((float)(1e+300 * 1e+300))
	#define F64_INF							(1e+300 * 1e+300)
	#define F32_QNAN						__builtin_nanf("0")
	#define F64_QNAN						__builtin_nan("0")
	
#elif RZ_COMP == RZ_COMP_LLVM
	#define FORCEINLINE						__attribute__((always_inline)) inline
	#define NOINLINE						__attribute__((noinline))
	#define BUILTIN_F32_INF					(__builtin_inff())
	#define BUILTIN_F64_INF					(__builtin_inf())
	#define BUILTIN_F32_QNAN				__builtin_nan("0")
	#define BUILTIN_F64_QNAN				__builtin_nan("0")
	#define DBGBREAK						do { asm volatile ("int3"); } while(0)
		
	#define BUILTIN_F32_INF					(__builtin_inff())
	#define BUILTIN_F64_INF					(__builtin_inf())
	#define BUILTIN_F32_QNAN				__builtin_nan("0")
	#define BUILTIN_F64_QNAN				__builtin_nan("0")
	
#elif RZ_COMP == RZ_COMP_GCC
	#define FORCEINLINE						__attribute__((always_inline)) inline
	#define NOINLINE						__attribute__((noinline))
	#define F32_INF							(__builtin_inff())
	#define F64_INF							(__builtin_inf())
	#define F32_QNAN						__builtin_nan("0")
	#define F64_QNAN						__builtin_nan("0")
	
	#if RZ_PLATF == RZ_PLATF_GENERIC_WIN
		#define DBGBREAK					do { __debugbreak(); } while(0)
	#elif RZ_PLATF == RZ_PLATF_GENERIC_UNIX
		#if RZ_ARCH == RZ_ARCH_ARM_V6_HF
			#define DBGBREAK				do { asm volatile ("bkpt #0"); } while(0)
		#endif
	#endif
	
	#define F32_INF							(__builtin_inff())
	#define F64_INF							(__builtin_inf())
	#define F32_QNAN						__builtin_nan("0")
	#define F64_QNAN						__builtin_nan("0")
	
#endif

#include "types.hpp"

#if RZ_PLATF == RZ_PLATF_GENERIC_WIN
	
	#if RZ_DBG // try to not use windows lib directly to simplify porting, but still allow for debugging (Sleep() for ex.)
		
		#define DWORD unsigned long
		#define BOOL int
		
		// For debugging
		__declspec(dllimport) BOOL __stdcall IsDebuggerPresent(void);
		
		__declspec(dllimport) void __stdcall Sleep(
		  DWORD dwMilliseconds
		);
		
		#define IS_DEBUGGER_PRESENT				IsDebuggerPresent()
		#define DBGBREAK_IF_DEBUGGER_PRESENT	if (IS_DEBUGGER_PRESENT) { DBGBREAK; }
		#define BREAK_IF_DEBUGGING_ELSE_STALL	if (IS_DEBUGGER_PRESENT) { DBGBREAK; } else { Sleep(100); }
		
		static void dbg_sleep (f32 sec) {
			Sleep( (DWORD)(sec * 1000.0f) );
		}
		
		#undef BOOL
		#undef DWORD
		
	#endif
	
#endif

////
#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define STATIC_ASSERT(cond) static_assert((cond), STRINGIFY(cond))

#define ANSI_COLOUR_CODE_RED	"\033[1;31m"
#define ANSI_COLOUR_CODE_YELLOW	"\033[0;33m"
#define ANSI_COLOUR_CODE_NC		"\033[0m"


#include "assert.hpp"

//
template<typename CAST_T, typename T>
static constexpr bool _safe_cast (T x);

template<> constexpr bool _safe_cast<u32, u64> (u64 x) { return x <= 0xffffffffull; }
template<> constexpr bool _safe_cast<s32, u64> (u64 x) { return x <= 0x7fffffffull; }

#define safe_cast(cast_t, val) _safe_cast<cast_t>(val)

//
template <typename T=u32, typename AT, uptr N>
static constexpr T _arrlen (AT (& arr)[N]) {
	static_assert(safe_cast(T, N), "arrlen:: array length too large for output type!");
	return (T)N;
}
template <typename T=u32, typename AT, uptr M, uptr N>
static constexpr T _arrlen (AT (& arr)[M][N]) {
	static_assert(safe_cast(T, M*N), "arrlen:: array length too large for output type!");
	return (T)(M*N);
}
#define arrlent(T, arr) _arrlen<T>(arr)
#define arrlen(arr) _arrlen<>(arr)

static u32 strlen (utf32 const* str) {
	u32 ret = 0;
	while (*str++) ++ret;
	return ret;
}
template <u32 N>
static u32 strlen (utf32 const (& str)[N]) {
	STATIC_ASSERT(N >= 1);
	return N -1;
}

template <typename FUNC>
struct At_Scope_Exit {
	FUNC	f;
	void operator= (At_Scope_Exit &) = delete;
	
	FORCEINLINE At_Scope_Exit (FUNC f): f(f) {}
	FORCEINLINE ~At_Scope_Exit () { f(); }
};

struct _Defer_Helper {};

template<typename FUNC>
static FORCEINLINE At_Scope_Exit<FUNC> operator+(_Defer_Helper, FUNC f) {
	return At_Scope_Exit<FUNC>(f);
}

#define CONCAT(a,b) a##b

#define _defer(counter) auto CONCAT(_defer_helper, counter) = _Defer_Helper() +[&] () 
#define defer _defer(__COUNTER__)
// use like: defer { lambda code };

#undef DEFINE_ENUM_FLAG_OPS
#define DEFINE_ENUM_FLAG_OPS(TYPE, UNDERLYING_TYPE) \
	static FORCEINLINE TYPE& operator|= (TYPE& l, TYPE r) { \
		return l = (TYPE)((UNDERLYING_TYPE)l | (UNDERLYING_TYPE)r); \
	} \
	static FORCEINLINE TYPE& operator&= (TYPE& l, TYPE r) { \
		return l = (TYPE)((UNDERLYING_TYPE)l & (UNDERLYING_TYPE)r); \
	} \
	static FORCEINLINE TYPE operator| (TYPE l, TYPE r) { \
		return (TYPE)((UNDERLYING_TYPE)l | (UNDERLYING_TYPE)r); \
	} \
	static FORCEINLINE TYPE operator& (TYPE l, TYPE r) { \
		return (TYPE)((UNDERLYING_TYPE)l & (UNDERLYING_TYPE)r); \
	} \
	static FORCEINLINE TYPE operator~ (TYPE e) { \
		return (TYPE)(~(UNDERLYING_TYPE)e); \
	}

#define DEFINE_ENUM_ITER_OPS(TYPE, UNDERLYING_TYPE) \
	static FORCEINLINE TYPE& operator++ (TYPE& val) { \
		return val = (TYPE)((UNDERLYING_TYPE)val +1); \
	}

#if 1 // try using std::vector and std::array
#include <array>
#include <vector>
#include <string>
#else
#include <initializer_list>

// WARNING: code is not tested, look at older git versions if you intend to use these
template <typename LEN_T=u32>
struct _array { // common implementation to prevent code bloat, and allow generic non-template functions to use the array
	byte*	arr;
	LEN_T	len;
	u32		_stride;
	
	void _malloc (LEN_T len_, u32 stride) {
		arr = (byte*)::malloc(len_*stride);
		len = len_;
		_stride = stride;
	}
	static _array malloc (LEN_T len, u32 stride) {
		_array ret;
		ret._malloc(len, stride);
		return ret;
	}
	void free () {
		::free(arr);
	}
	void realloc (LEN_T new_len) {
		arr = (byte*)::realloc(arr, new_len*_stride);
		len = new_len;
	}
	
	void* operator[] (LEN_T indx) {
		dbg_assert(indx >= 0 && indx < len, "array:: operator[]: indx: %d len: %d", indx, len);
		return arr[indx*_stride];
	}
	
	void*					begin () {						return arr; }
	constexpr void const*	begin () const {				return arr; }
	
	void*					end () {						return &arr[len*_stride]; }
	constexpr void const*	end () const {					return &arr[len*_stride]; }
	
	// inefficient because of divide
	//LEN_T					get_i (void const* it) {		return (it -arr) / _stride; }
	//constexpr LEN_T			get_i (void const* it) const {	return (it -arr) / _stride; }
	
};

template <typename T, typename LEN_T=u32>
struct array : _array<LEN_T> {
	static constexpr u32 STRIDE = sizeof(T);
	
	array () {}
	constexpr array (T* a, LEN_T l): _array<LEN_T>{(byte*)a, l, STRIDE} {}
	constexpr array (std::initializer_list<T> l): _array<LEN_T>{ (byte*)l.begin(), (LEN_T)l.size(), STRIDE} {
				//static_assert(safe_cast(LEN_T, l.size()), "array<> :: initializer_list.size() out of range for len!"); // can't do this in c++
			}
	template<LEN_T N> constexpr array (T (& a)[N]): _array<LEN_T>{(byte*)a, N, STRIDE} {}
	
	static constexpr array null () { return {nullptr, 0}; } // when arr==nullptr, realloc() still works -> so we can push() on a array initialized with null
	
	operator array<T const> () { return array(arr, len); }
	
	static array malloc (LEN_T len) {
		array ret;
		ret._malloc(len, STRIDE);
		return ret;
	}
	
	T* get_arr () const {
		return (T*)this->arr;
	}
	
	// override these operators because the compiler might not realize that our stride always stays constant (variable stride of our parent class is only so that we can use this array in a generic function)
	T cr operator[] (LEN_T indx) const {
		dbg_assert(this->_stride == STRIDE); // sanity checking
		dbg_assert(indx >= 0 && indx < this->len, "array:: operator[]: indx: %d len: %d", indx, this->len);
		return get_arr()[indx];
	}
	T& operator[] (LEN_T indx) {
		dbg_assert(this->_stride == STRIDE); // sanity checking
		dbg_assert(indx >= 0 && indx < this->len, "array:: operator[]: indx: %d len: %d", indx, this->len);
		return get_arr()[indx];
	}
	
	T*					begin () {					return get_arr(); }
	constexpr T const*	begin () const {			return get_arr(); }
	
	T*					end () {					return get_arr() +this->len*STRIDE; }
	constexpr T const*	end () const {				return get_arr() +this->len*STRIDE; }
	
	LEN_T				get_i (T const* it) {		return it -get_arr(); }
	constexpr LEN_T		get_i (T const* it) const {	return it -get_arr(); }
	
};

template<typename T, typename FUNC>
static T* lsearch (array<T> arr, FUNC comp_with) {
	for (T& x : arr) {
		if (comp_with(&x)) return &x; // found
	}
	return nullptr; // not found
}

template <typename T, typename LEN_T=u32>
struct dynarr : array<T, LEN_T> {
	
	constexpr dynarr (T* a, LEN_T l): array<T, LEN_T>{a, l} {}
	dynarr () {}
	
	static constexpr dynarr null () { return {nullptr, 0}; } // when arr==nullptr, realloc() still works -> so we can push() on a array initialized with null
	
	LEN_T grow_by (LEN_T diff) {
		LEN_T old_len = this->len;
		this->realloc(this->len +diff);
		return old_len;
	}
	void shrink_by (LEN_T diff) {
		dbg_assert(diff <= this->len);
		this->realloc(this->len -diff);
	}
	
	T& push () {
		LEN_T old_len = grow_by(1);
		return get_arr()[old_len];
	}
	void push (T cr val) {
		push() = val;
	}
	
	LEN_T pushn (LEN_T count) {
		return grow_by(count);
	}
	#if 0
	T* pushn (T const* val, LEN_T count) {
		LEN_T old_len = this->len;
		grow_by(count);
		return copy_values(old_len, val, count);
	}
	#endif
	
	void delete_by_moving_last (LEN_T indx) {
		dbg_assert(indx >= 0 && indx < this->len);
		if (this->len > 1) {
			get_arr()[indx] = get_arr()[this->len -1];
		}
		shrink_by(1);
	}
};

static va_list _print_array (array<char>* arr, cstr format, va_list vl) { // print 
	for (;;) {
		auto ret = vsnprintf(arr->get_arr(), arr->len, format, vl);
		dbg_assert(ret >= 0);
		if ((u32)ret < arr->len) break;
		// buffer was to small, increase buffer size
		arr->free();
		*arr = array<char>::malloc((u32)ret +1);
		// now snprintf has to succeed, so call it again
	}
}
static void print_array (array<char>* arr, cstr format, ...) {
	va_list vl;
	va_start(vl, format);
	
	_print_array(arr, format, vl);
	
	va_end(vl);
}
static array<char> print_array (cstr format, ...) {
	va_list vl;
	va_start(vl, format);
	
	auto ret = array<char>::malloc(128); // overallocate to prevent calling printf twice in most cases
	_print_array(arr, format, vl);
	
	va_end(vl);
	
	return ret;
}
#endif


template<typename T, typename FUNC>
static T* lsearch (std::vector<T>& arr, FUNC comp_with) {
	for (T& x : arr) {
		if (comp_with(&x)) return &x; // found
	}
	return nullptr; // not found
}

static void _prints (std::string* s, cstr format, va_list vl) { // print 
	for (;;) {
		auto ret = vsnprintf(&(*s)[0], s->length()+1, format, vl); // i think i'm technically not allowed to overwrite the null terminator
		dbg_assert(ret >= 0);
		bool was_big_enough = (u32)ret < s->length()+1;
		s->resize((u32)ret);
		if (was_big_enough) break;
		// buffer was to small, buffer size was increased
		// now snprintf has to succeed, so call it again
	}
}
static void prints (std::string* s, cstr format, ...) {
	va_list vl;
	va_start(vl, format);
	
	_prints(s, format, vl);
	
	va_end(vl);
}
static std::string prints (cstr format, ...) {
	va_list vl;
	va_start(vl, format);
	
	std::string ret;
	ret.reserve(128); // overallocate to prevent calling printf twice in most cases
	ret.resize(ret.capacity());
	_prints(&ret, format, vl);
	
	va_end(vl);
	
	return ret;
}
