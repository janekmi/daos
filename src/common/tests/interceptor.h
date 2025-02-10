#define INTERCEPTOR_ENV "INTERCEPTOR_OUTPUT"

extern char *interceptor_output;

#define FUNC_REAL(name)\
	__real_##name

#define _FUNC_REAL_DECL(name, ret_type, ...)\
	ret_type FUNC_REAL(name)(__VA_ARGS__) __attribute__((unused));

#define FUNC_MOCK(name, ret_type, ...) \
	_FUNC_REAL_DECL(name, ret_type, ##__VA_ARGS__)\
	ret_type __wrap_##name(__VA_ARGS__)

void dump(const char *format, ...);
