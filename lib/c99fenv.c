/* <fenv.h> for Windows targets (part of libtcc1.a): msvcrt.dll has none of
   it.  The floating point environment is the x87 control/status words and
   the SSE control/status register MXCSR (tcc computes with SSE; the x87
   state is kept in step for code that uses it).  The FE_* values are the
   hardware bit positions, so flag sets pass through unchanged.

   Limits: feraiseexcept sets the status flags; it does not trap, so with
   exceptions unmasked (via _controlfp) no signal is delivered.
   long double is double on these targets.

   ARM64: FPCR (rounding, trap enables) and FPSR (flags).

   Without _WIN32 this file builds natively (x86) for unit tests when the
   public names are redefined with -D (see tests/c99rt-host.sh). */

#include <fenv.h>

#if defined(__x86_64__) || defined(__i386__)

#define X87_FLAGS 0x3f
#define X87_RC    0x0c00
#define MXCSR_RC  0x6000

static unsigned get_mxcsr(void)
{
    unsigned m;
    __asm__ volatile("stmxcsr %0" : "=m"(m));
    return m;
}

static void set_mxcsr(unsigned m)
{
    __asm__ volatile("ldmxcsr %0" : : "m"(m));
}

static unsigned short get_cw(void)
{
    unsigned short c;
    __asm__ volatile("fnstcw %0" : "=m"(c));
    return c;
}

static void set_cw(unsigned short c)
{
    __asm__ volatile("fldcw %0" : : "m"(c));
}

static unsigned short get_sw(void)
{
    unsigned short s;
    __asm__ volatile("fnstsw %0" : "=m"(s));
    return s;
}

/* current exception flags, as the union of x87 and SSE state */
static int flags_now(void)
{
    return (get_mxcsr() | get_sw()) & X87_FLAGS;
}

/* replace the exception flags selected by 'mask' with those in 'val' */
static void flags_put(int mask, int val)
{
    mask &= FE_ALL_EXCEPT;
    set_mxcsr((get_mxcsr() & ~mask) | (val & mask));
    /* x87: the status word is only writable through the environment image */
    {
        fenv_t e;
        __asm__ volatile("fnstenv %0" : "=m"(e));
        e.__status_word = (e.__status_word & ~mask) | (val & mask);
        __asm__ volatile("fldenv %0" : : "m"(e));
    }
}

int feclearexcept(int excepts)
{
    if (excepts & ~FE_ALL_EXCEPT)
        return 1;
    flags_put(excepts, 0);
    return 0;
}

int fetestexcept(int excepts)
{
    return flags_now() & excepts & FE_ALL_EXCEPT;
}

int feraiseexcept(int excepts)
{
    if (excepts & ~FE_ALL_EXCEPT)
        return 1;
    flags_put(excepts, excepts);
    return 0;
}

int fegetexceptflag(fexcept_t *flagp, int excepts)
{
    *flagp = flags_now() & excepts & FE_ALL_EXCEPT;
    return 0;
}

int fesetexceptflag(const fexcept_t *flagp, int excepts)
{
    flags_put(excepts, *flagp);
    return 0;
}

/* C23 */
int fetestexceptflag(const fexcept_t *flagp, int excepts)
{
    return *flagp & excepts & FE_ALL_EXCEPT;
}

int fegetround(void)
{
    return get_cw() & X87_RC;
}

int fesetround(int mode)
{
    if (mode & ~X87_RC)
        return 1;
    set_cw((get_cw() & ~X87_RC) | mode);
    set_mxcsr((get_mxcsr() & ~MXCSR_RC) | (mode << 3));
    return 0;
}

static void default_env(fenv_t *e, int pc)
{
    unsigned short *w = (unsigned short *)e;
    int i;
    for (i = 0; i < (int)(sizeof *e / 2); i++)
        w[i] = 0;
    e->__control_word = pc == 53 ? 0x27f : 0x37f;
    e->__tag_word = 0xffff;
    e->__mxcsr = 0x1f80;
}

int fegetenv(fenv_t *envp)
{
    /* fnstenv masks all x87 exceptions as a side effect: load it back */
    __asm__ volatile("fnstenv %0" : "=m"(*envp));
    __asm__ volatile("fldenv %0" : : "m"(*envp));
    envp->__mxcsr = get_mxcsr();
    return 0;
}

int fesetenv(const fenv_t *envp)
{
    fenv_t e;
    if (envp == FE_PC64_ENV)
        default_env(&e, 64);
    else if (envp == FE_PC53_ENV)
        default_env(&e, 53);
    else if (envp == FE_DFL_ENV)
        default_env(&e, 64);
    else
        e = *envp;
    __asm__ volatile("fldenv %0" : : "m"(e));
    set_mxcsr(e.__mxcsr);
    return 0;
}

int feholdexcept(fenv_t *envp)
{
    fegetenv(envp);
    flags_put(FE_ALL_EXCEPT, 0);
    /* non-stop mode: mask every exception */
    set_mxcsr(get_mxcsr() | 0x1f80);
    set_cw(get_cw() | 0x3f);
    return 0;
}

int feupdateenv(const fenv_t *envp)
{
    int raised = flags_now();
    int r = fesetenv(envp);
    flags_put(raised, raised);
    return r;
}

/* C23 7.6.5: control modes (rounding, exception masks), no flags */
int fegetmode(femode_t *modep)
{
    modep->__control_word = get_cw();
    modep->__mxcsr = get_mxcsr() & ~(unsigned)X87_FLAGS;
    return 0;
}

int fesetmode(const femode_t *modep)
{
    femode_t m;
    if (modep == FE_DFL_MODE) {
        m.__control_word = 0x37f;
        m.__mxcsr = 0x1f80;
        modep = &m;
    }
    set_cw(modep->__control_word);
    set_mxcsr((get_mxcsr() & X87_FLAGS) | (modep->__mxcsr & ~(unsigned)X87_FLAGS));
    return 0;
}

#elif defined(__aarch64__)

/* ARM64: the FPCR (rounding mode in bits 23:22, trap enables in bits 12:8)
   and the FPSR (cumulative flags in bits 4:0, the same positions as the FE_*
   values).  Windows on ARM64 does not deliver traps for the enable bits;
   feraiseexcept only sets the flags. */

#define FPCR_RMODE 0xc00000u
#define FPCR_TRAPS 0x1f00u

static unsigned get_fpcr(void)
{
    unsigned long long v;
    __asm__ volatile("mrs %0, fpcr" : "=r"(v));
    return (unsigned)v;
}

static void set_fpcr(unsigned v)
{
    unsigned long long x = v;
    __asm__ volatile("msr fpcr, %0" : : "r"(x));
}

static unsigned get_fpsr(void)
{
    unsigned long long v;
    __asm__ volatile("mrs %0, fpsr" : "=r"(v));
    return (unsigned)v;
}

static void set_fpsr(unsigned v)
{
    unsigned long long x = v;
    __asm__ volatile("msr fpsr, %0" : : "r"(x));
}

int feclearexcept(int excepts)
{
    if (excepts & ~FE_ALL_EXCEPT)
        return 1;
    set_fpsr(get_fpsr() & ~(unsigned)excepts);
    return 0;
}

int fetestexcept(int excepts)
{
    return get_fpsr() & excepts & FE_ALL_EXCEPT;
}

int feraiseexcept(int excepts)
{
    if (excepts & ~FE_ALL_EXCEPT)
        return 1;
    set_fpsr(get_fpsr() | excepts);
    return 0;
}

int fegetexceptflag(fexcept_t *flagp, int excepts)
{
    *flagp = get_fpsr() & excepts & FE_ALL_EXCEPT;
    return 0;
}

int fesetexceptflag(const fexcept_t *flagp, int excepts)
{
    unsigned m = excepts & FE_ALL_EXCEPT;
    set_fpsr((get_fpsr() & ~m) | (*flagp & m));
    return 0;
}

/* C23 */
int fetestexceptflag(const fexcept_t *flagp, int excepts)
{
    return *flagp & excepts & FE_ALL_EXCEPT;
}

int fegetround(void)
{
    return get_fpcr() & FPCR_RMODE;
}

int fesetround(int mode)
{
    if (mode & ~FPCR_RMODE)
        return 1;
    set_fpcr((get_fpcr() & ~FPCR_RMODE) | mode);
    return 0;
}

int fegetenv(fenv_t *envp)
{
    envp->__fpcr = get_fpcr();
    envp->__fpsr = get_fpsr();
    return 0;
}

int fesetenv(const fenv_t *envp)
{
    if (envp == FE_DFL_ENV) {
        set_fpcr(0);
        set_fpsr(0);
    } else {
        set_fpcr(envp->__fpcr);
        set_fpsr(envp->__fpsr);
    }
    return 0;
}

int feholdexcept(fenv_t *envp)
{
    fegetenv(envp);
    set_fpsr(get_fpsr() & ~(unsigned)FE_ALL_EXCEPT);
    /* non-stop mode: no exception traps */
    set_fpcr(get_fpcr() & ~FPCR_TRAPS);
    return 0;
}

int feupdateenv(const fenv_t *envp)
{
    unsigned raised = get_fpsr() & FE_ALL_EXCEPT;
    int r = fesetenv(envp);
    set_fpsr(get_fpsr() | raised);
    return r;
}

/* C23 7.6.5: control modes (rounding, trap enables), no flags */
int fegetmode(femode_t *modep)
{
    modep->__fpcr = get_fpcr();
    return 0;
}

int fesetmode(const femode_t *modep)
{
    set_fpcr(modep == FE_DFL_MODE ? 0 : modep->__fpcr);
    return 0;
}

#endif /* x86, arm64 */
