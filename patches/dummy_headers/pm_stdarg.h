#ifndef PM_DUMMY_STDARG_H
#define PM_DUMMY_STDARG_H
// The decomp's stdarg.h is KMC gcc's (stdlib/va-mips.h, gcc/stdarg.h), which declares va_end, a builtin to clang, and
// the decomp includes it by a quoted path, which finds it beside the including header first. So patches.h includes
// this before any of the decomp's headers: clang's own va_list, and the guards that make the decomp's skip theirs.
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#define _STDARG_H
#define _ANSI_STDARG_H_
#define __GNUC_VA_LIST
#endif
