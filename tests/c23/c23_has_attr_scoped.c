/* __has_c_attribute with scoped names, spelling variants and defined() */
#ifndef __has_c_attribute
#error __has_c_attribute missing
#endif
#if !defined(__has_c_attribute)
#error defined(__has_c_attribute)
#endif
#if __has_c_attribute(deprecated) != 201904L || __has_c_attribute(__deprecated__) != 201904L
#error deprecated
#endif
#if __has_c_attribute(nodiscard) != 202003L || __has_c_attribute(noreturn) != 202202L
#error nodiscard/noreturn
#endif
#if __has_c_attribute(owner) != 202311L || __has_c_attribute(tcc::borrow) != 202311L
#error ownership
#endif
/* vendor attributes parse, and report 0 when unsupported */
#if __has_c_attribute(gnu::no_such_thing) || __has_c_attribute(vendor::x)
#error unknown vendor attribute reported as supported
#endif
#if __has_c_attribute(__gnu__::unused) && 0
#endif
#if __has_c_attribute(no_such_standard_attribute)
#error unknown
#endif
#define HAS(x) __has_c_attribute(x)
#if HAS(fallthrough) != 201910L || HAS(gnu::foo)
#error through a macro
#endif
[[gnu::unused, vendor::thing(1, (2), {3})]] int h3;
int main(void) { return h3; }
