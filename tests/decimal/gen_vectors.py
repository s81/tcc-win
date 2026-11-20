#!/usr/bin/env python3
# Test-vector generator for the tcc-win decimal library (lib/decbid.c).
#
# Oracle: Python's pure-Python decimal implementation (_pydecimal), run in
# contexts with the precision and exponent range of decimal32/64/128,
# clamp=1, traps off, all five IEEE rounding modes.  Square root, integer and
# binary-float conversions are computed here from exact integers/fractions
# because the Python methods round differently (sqrt is always half-even).
# Nothing from any decimal library is read or used; nothing is shipped.
#
# usage: python -I gen_vectors.py [--seed N] [--count N] [--out FILE]
#   --count is the number of vectors per operation and format (default 60).
#
# Vector lines (all numbers hex unless noted; W = 32, 64 or 128; R = 0..4):
#   add|sub|mul|div|quantize W R a b -> res flags
#   sqrt W R a -> res flags            fma W R a b c -> res flags
#   compare W R a b -> res(decimal: -1 0 1 2) flags
#   cvt W R a V -> res flags           (to decimal format V)
#   fromi64|fromu64 W R v -> res flags
#   toi64|tou64|toi32|tou32 W R a -> v flags
#   fromdbl|fromflt W R bits -> res flags     todbl|toflt W R a -> bits flags
#   fromstr W R x+hex(string) -> res flags endpos(decimal)
#   tostr W a s|f -> x+hex(string)
# flags: 1 invalid, 2 divbyzero, 4 overflow, 8 underflow, 16 inexact.
import sys, random, struct, math, argparse
from fractions import Fraction
import _pydecimal as D
from _pydecimal import Decimal

FM = {32: dict(p=7, emax=96, bias=101, eb=8, t=20),
      64: dict(p=16, emax=384, bias=398, eb=10, t=50),
      128: dict(p=34, emax=6144, bias=6176, eb=14, t=110)}
for w, f in FM.items():
    f['n'] = w; f['etiny'] = -f['bias']; f['qmax'] = f['emax'] - f['p'] + 1
MODES = [D.ROUND_HALF_EVEN, D.ROUND_HALF_UP, D.ROUND_DOWN, D.ROUND_CEILING, D.ROUND_FLOOR]
FLAGMAP = [(D.InvalidOperation, 1), (D.DivisionByZero, 2), (D.Overflow, 4),
           (D.Underflow, 8), (D.Inexact, 16)]

def ctx(f, r):
    F = FM[f]
    return D.Context(prec=F['p'], Emax=F['emax'], Emin=1 - F['emax'], rounding=MODES[r],
                     clamp=1, traps=[], flags=[], capitals=1)

def flags(c):
    return sum(b for k, b in FLAGMAP if c.flags[k])

def mk(sign, c, e):
    return Decimal((sign, tuple(int(x) for x in str(c)), e))

def enc(f, v):
    F = FM[f]; n = F['n']; t = F['t']
    s, digs, e = v.as_tuple()
    if e == 'F':
        bits = 0b11110 << (n - 6)
    elif e in ('n', 'N'):
        pl = int(''.join(map(str, digs))) if digs else 0
        assert pl < 10 ** (F['p'] - 1)
        bits = (0b11111 << (n - 6)) | ((1 if e == 'N' else 0) << (n - 7)) | pl
    else:
        c = int(''.join(map(str, digs))); E = e + F['bias']
        assert 0 <= c < 10 ** F['p'] and 0 <= E <= 3 * 2 ** (F['eb'] - 2) - 1, (v, f)
        if c < 1 << (t + 3):
            bits = (E << (t + 3)) | c
        else:
            bits = (3 << (n - 3)) | (E << (t + 1)) | (c & ((1 << (t + 1)) - 1))
    return bits | (s << (n - 1))

def hx(f, bits):
    return '%0*x' % (FM[f]['n'] // 4, bits)

# ---------------------------------------------------------------- operands
def rcoef(rng, p):
    r = rng.random()
    nd = p if r < .55 else rng.randint(1, p)
    if rng.random() < .1:
        return 10 ** (nd - 1) if rng.random() < .5 else 10 ** nd - 1
    return rng.randint(10 ** (nd - 1), 10 ** nd - 1)

def rexp(rng, F):
    r = rng.random(); p = F['p']
    if r < .45: return rng.randint(-p - 3, p + 3)
    if r < .6: return rng.randint(F['etiny'], F['etiny'] + p + 3)
    if r < .75: return rng.randint(F['qmax'] - p - 3, F['qmax'])
    return rng.randint(F['etiny'], F['qmax'])

def special(rng, F):
    r = rng.random(); p = F['p']
    if r < .25: return mk(rng.randint(0, 1), 0, rng.choice([0, F['etiny'], F['qmax'], rng.randint(F['etiny'], F['qmax'])]))
    if r < .4: return Decimal('-Infinity' if rng.random() < .5 else 'Infinity')
    pl = rng.choice([0, 0, 1, rng.randint(0, 10 ** (p - 1) - 1), 10 ** (p - 1) - 1])
    if r < .6: return Decimal((rng.randint(0, 1), tuple(int(x) for x in str(pl)), 'n'))
    if r < .7: return Decimal((rng.randint(0, 1), tuple(int(x) for x in str(pl)), 'N'))
    if r < .85: return mk(rng.randint(0, 1), 10 ** p - 1, F['qmax'])
    return mk(rng.randint(0, 1), rng.choice([1, 10 ** (p - 1)]), rng.choice([F['etiny'], F['qmax']]))

def operand(rng, F, spec=.12):
    if rng.random() < spec: return special(rng, F)
    return mk(rng.randint(0, 1), rcoef(rng, F['p']), rexp(rng, F))

def near(rng, F, x, spec=.05):
    """second operand placed so that rounding/alignment is exercised"""
    if rng.random() < spec or not x.is_finite() or x.is_zero(): return operand(rng, F, .3)
    p = F['p']; s, digs, e = x.as_tuple()
    r = rng.random()
    e2 = max(F['etiny'], min(F['qmax'], e + rng.randint(-p - 3, p + 3)))
    if r < .35:        # halfway / just around halfway of the unit at some digit
        k = rng.randint(1, p); base = rng.choice([5, 49, 51, 4999999, 5000001]) * 10 ** rng.randint(0, p - 2)
        return mk(rng.randint(0, 1), min(base, 10 ** p - 1), max(F['etiny'], min(F['qmax'], e - k)))
    if r < .6:         # cancellation: close to -x
        c = int(''.join(map(str, digs))) + rng.randint(-3, 3)
        return mk(1 - s, min(max(c, 0), 10 ** p - 1), e)
    return mk(rng.randint(0, 1), rcoef(rng, p), e2)

def mul_ties(rng, F):
    """a*b whose exact product ends in ...5 followed by zeros at the rounding digit"""
    p = F['p']; a = rng.choice([2, 4, 5, 8, 16, 25, 125, 3, 7])
    q = rng.randint(10 ** (p - 1), 10 ** p - 1) // 10 * 10 + 5
    b = q * 10 ** rng.randint(0, 3) // a * a // a
    if b <= 0 or b >= 10 ** p: b = rcoef(rng, p)
    e1 = rexp(rng, F); lo = max(F['etiny'], -e1 - p); hi = min(F['qmax'], -e1 + p)
    e2 = rng.randint(lo, hi) if lo <= hi else rexp(rng, F)
    return mk(rng.randint(0, 1), a, e1), mk(rng.randint(0, 1), b, e2)

def div_pair(rng, F):
    p = F['p']; r = rng.random()
    if r < .3:         # exact quotient
        b = rcoef(rng, rng.randint(1, max(1, p // 2))); q = rcoef(rng, rng.randint(1, p // 2 + 1))
        a = b * q
        if a >= 10 ** p: a = b
        e1 = rexp(rng, F)
        return mk(rng.randint(0, 1), a, e1), mk(rng.randint(0, 1), b, rng.randint(e1 - p, e1 + p) if F['etiny'] < e1 - p and e1 + p < F['qmax'] else 0)
    if r < .45:        # tie: quotient with p+1 digits ending in 5
        b = rng.choice([2, 4, 8, 16, 5, 25]); q = rng.randint(10 ** p, 10 ** (p + 1) - 1) // 10 * 10 + 5
        a = q * b // 10 ** 0
        # a/b = q exactly; scale a down if it has too many digits by using exponent
        a2 = a
        while a2 >= 10 ** p: a2 //= 10
        return mk(rng.randint(0, 1), a2, rng.randint(-5, 5)), mk(rng.randint(0, 1), b, rng.randint(-5, 5))
    x = operand(rng, F, .1)
    return x, near(rng, F, x, .1) if rng.random() < .5 else operand(rng, F, .1)

# ------------------------------------------------------------------ oracles
def run(f, r, fn):
    c = ctx(f, r); res = fn(c); return res, flags(c)

def snan_quiet(v):
    s, d, e = v.as_tuple(); return Decimal((s, d, 'n'))

def fix(c, v):
    """round an exact Decimal (any finite exponent) into the context, keeping the sign of zero"""
    if v.is_snan():
        c._raise_error(D.InvalidOperation, 'sNaN'); return snan_quiet(v)._fix(c)
    return v._fix(c)

def o_sqrt(f, r, x):
    F = FM[f]; c = ctx(f, r)
    if not x.is_finite() or x.is_zero() or x.is_signed():
        return (c.sqrt(x), flags(c)) if True else None
    s, digs, e = x.as_tuple(); cf = int(''.join(map(str, digs)))
    if e % 2: cf *= 10; e -= 1
    m = max(0, (2 * (F['p'] + 12) - len(str(cf)) + 1) // 2)
    n = cf * 10 ** (2 * m); sr = math.isqrt(n); ex = e // 2 - m
    if sr * sr == n:
        while ex < e // 2 and sr % 10 == 0: sr //= 10; ex += 1
        v = mk(0, sr, ex)
    else:
        v = mk(0, 10 * sr + 5, ex - 1)
    res = v._fix(c)
    return res, flags(c)

def o_quant(f, r, x, y):
    F = FM[f]; c = ctx(f, r)
    res = x.quantize(y, context=c) if False else None
    res = c.quantize(x, y)
    return res, flags(c)

def o_compare(f, r, x, y):
    c = ctx(f, r)
    if x.is_snan() or y.is_snan(): return 2, 1
    if x.is_nan() or y.is_nan(): return 2, 0
    return (x > y) - (x < y), 0

def o_cvt(f, r, x, g):
    c = ctx(g, r)
    return fix(c, x), flags(c)

def i_range(op):
    return {'i64': (-2 ** 63, 2 ** 63 - 1), 'u64': (0, 2 ** 64 - 1),
            'i32': (-2 ** 31, 2 ** 31 - 1), 'u32': (0, 2 ** 32 - 1)}[op]

def o_toint(f, r, x, kind):
    lo, hi = i_range(kind); bits = 64 if '64' in kind else 32
    if x.is_nan(): return 0, 1
    if x.is_infinite(): return (lo if x.is_signed() else hi) & (2 ** bits - 1), 1
    c = ctx(f, r); v = x.to_integral_value(rounding=MODES[r], context=c)
    iv = int(v); fl = 0
    if iv < lo or iv > hi: return (lo if iv < lo else hi) & (2 ** bits - 1), 1
    if v != x: fl = 16
    return iv & (2 ** bits - 1), fl

def o_fromint(f, r, iv):
    c = ctx(f, r); return fix(c, Decimal(iv)), flags(c)

def o_fromfp(f, r, x, isdbl):
    c = ctx(f, r)
    if math.isnan(x):
        return Decimal((1 if math.copysign(1, x) < 0 else 0, (), 'n')), 0
    if math.isinf(x): return Decimal('-Infinity' if x < 0 else 'Infinity'), 0
    v = Decimal(x)
    if x == 0: v = mk(1 if math.copysign(1, x) < 0 else 0, 0, 0)
    return fix(c, v), flags(c)

def o_tofp(f, r, x, dbl):
    mant, emin, emax = (53, -1022, 1023) if dbl else (24, -126, 127)
    nb = 64 if dbl else 32
    qn = (0x7ff8000000000000 if dbl else 0x7fc00000)
    sgn = 1 << (nb - 1)
    if x.is_nan(): return (qn | (sgn if x.is_signed() else 0)), (1 if x.is_snan() else 0)
    if x.is_infinite():
        return ((0x7ff0000000000000 if dbl else 0x7f800000) | (sgn if x.is_signed() else 0)), 0
    if x.is_zero(): return (sgn if x.is_signed() else 0), 0
    neg = x.is_signed(); fr = abs(Fraction(x))
    E = fr.numerator.bit_length() - fr.denominator.bit_length()
    if fr < Fraction(2) ** E: E -= 1
    q = max(E, emin) - (mant - 1)
    m = fr / Fraction(2) ** q; lo = m.numerator // m.denominator; rem = m - lo
    if r == 0: up = rem > Fraction(1, 2) or (rem == Fraction(1, 2) and lo % 2)
    elif r == 1: up = rem >= Fraction(1, 2)
    elif r == 2: up = False
    elif r == 3: up = rem > 0 and not neg
    else: up = rem > 0 and neg
    mm = lo + (1 if up else 0)
    inexact = rem > 0
    tiny = fr < Fraction(2) ** emin
    fl = (16 if inexact else 0) | (8 if (tiny and inexact) else 0)
    if mm >= 2 ** mant and q + mant > emax:   # past max finite
        pass
    if mm * Fraction(2) ** q >= Fraction(2) ** (emax + 1):
        inf_ = (r in (0, 1)) or (r == 3 and not neg) or (r == 4 and neg)
        fl = 4 | 16
        if inf_: b = 0x7ff0000000000000 if dbl else 0x7f800000
        else: b = 0x7fefffffffffffff if dbl else 0x7f7fffff
        return b | (sgn if neg else 0), fl
    v = float(mm * Fraction(2) ** q)
    b = struct.unpack('<Q' if dbl else '<I', struct.pack('<d' if dbl else '<f', v))[0]
    return b | (sgn if neg else 0), fl

def parse_str_oracle(f, r, s):
    """expected (decimal value, flags, consumed) for a string made by make_str"""
    raise NotImplementedError

# ------------------------------------------------------------- generation
def gen_ops(f, rng, count, out):
    F = FM[f]; p = F['p']
    def emit(op, r, ins, res, fl, extra=''):
        out.append('%s %d %d %s -> %s %x%s' % (op, f, r, ' '.join(ins), res, fl, extra))
    H = lambda v: hx(f, enc(f, v))
    for op in ('add', 'sub', 'mul', 'div', 'fma', 'sqrt', 'quantize', 'compare'):
        for i in range(count):
            r = rng.randrange(5)
            if op in ('add', 'sub'):
                x = operand(rng, F); y = near(rng, F, x)
                if rng.random() < .5: x, y = y, x
                res, fl = run(f, r, (lambda c: c.add(x, y)) if op == 'add' else (lambda c: c.subtract(x, y)))
                emit(op, r, [H(x), H(y)], H(res), fl)
            elif op == 'mul':
                if rng.random() < .3: x, y = mul_ties(rng, F)
                else:
                    x = operand(rng, F); y = operand(rng, F) if rng.random() < .5 else mk(rng.randint(0, 1), rcoef(rng, p), max(F['etiny'], min(F['qmax'], -rexp(rng, F) // 2)))
                res, fl = run(f, r, lambda c: c.multiply(x, y))
                emit(op, r, [H(x), H(y)], H(res), fl)
            elif op == 'div':
                x, y = div_pair(rng, F)
                res, fl = run(f, r, lambda c: c.divide(x, y))
                emit(op, r, [H(x), H(y)], H(res), fl)
            elif op == 'fma':
                x = operand(rng, F); y = operand(rng, F, .08)
                if rng.random() < .5: x, y = mul_ties(rng, F)
                if x.is_finite() and y.is_finite() and rng.random() < .6:
                    z = mk(rng.randint(0, 1), rcoef(rng, p), max(F['etiny'], min(F['qmax'], x.as_tuple()[2] + y.as_tuple()[2] + rng.randint(-p - 3, p + 3))))
                else: z = operand(rng, F)
                res, fl = run(f, r, lambda c: c.fma(x, y, z))
                emit(op, r, [H(x), H(y), H(z)], H(res), fl)
            elif op == 'sqrt':
                if rng.random() < .3 and True:
                    k = rcoef(rng, p // 2 + 1); e = rng.randint(F['etiny'] // 2 + 1, F['qmax'] // 2)
                    x = mk(0, k * k if k * k < 10 ** p else k, 2 * e if F['etiny'] <= 2 * e <= F['qmax'] else 0)
                else: x = operand(rng, F, .15)
                if rng.random() < .8 and x.is_finite(): x = x.copy_abs() if not x.is_zero() else x
                res, fl = o_sqrt(f, r, x)
                emit(op, r, [H(x)], H(res), fl)
            elif op == 'quantize':
                x = operand(rng, F, .1)
                y = mk(rng.randint(0, 1), 1, rng.choice([rexp(rng, F), rexp(rng, F), 0, -1, -p])) if rng.random() > .1 else operand(rng, F, .6)
                res, fl = o_quant(f, r, x, y)
                emit(op, r, [H(x), H(y)], H(res), fl)
            else:
                x = operand(rng, F); y = near(rng, F, x, .1) if rng.random() < .5 else operand(rng, F)
                if rng.random() < .2 and x.is_finite(): y = x.copy_sign(y) if rng.random() < .5 else mk(0, 0, 0)
                rv, fl = o_compare(f, r, x, y)
                emit(op, r, [H(x), H(y)], str(rv), fl)

def gen_conv(f, rng, count, out):
    F = FM[f]; p = F['p']
    H = lambda v, g=None: hx(g or f, enc(g or f, v))
    for g in FM:
        if g == f: continue
        for i in range(count // 2 + 1):
            r = rng.randrange(5); G = FM[g]
            if rng.random() < .15: x = special(rng, F)
            else:
                lo = max(F['etiny'], G['etiny'] - 40); hi = min(F['qmax'], G['qmax'] + 40)
                ri = lambda a, b: rng.randint(min(a, b), max(a, b))
                e = rng.choice([ri(lo, hi), ri(max(lo, G['etiny'] - 5), min(hi, G['etiny'] + p)), ri(max(lo, G['qmax'] - p), min(hi, G['qmax'] + 3))])
                e = max(F['etiny'], min(F['qmax'], e))
                x = mk(rng.randint(0, 1), rcoef(rng, p), e)
            res, fl = o_cvt(f, r, x, g)
            out.append('cvt %d %d %s %d -> %s %x' % (f, r, H(x), g, hx(g, enc(g, res)), fl))
    for op in ('fromi64', 'fromu64'):
        for i in range(count):
            r = rng.randrange(5)
            pick = rng.random()
            if pick < .3: v = rng.randint(0, 10 ** rng.randint(1, 19))
            elif pick < .5: v = (10 ** rng.randint(min(p - 1, 17), min(19, p + 2)) + rng.choice([-1, 0, 1, 5, 4, 6])) * rng.choice([1, 10, 1])
            else: v = rng.choice([0, 1, 2 ** 63 - 1, 2 ** 63, 2 ** 64 - 1, 2 ** 53 + 1, rng.getrandbits(64)])
            if op == 'fromi64':
                v = v % 2 ** 63
                if rng.random() < .5: v = -v
                if rng.random() < .03: v = -2 ** 63
            else: v = v % 2 ** 64
            res, fl = o_fromint(f, r, v)
            out.append('%s %d %d %x -> %s %x' % (op, f, r, v % 2 ** 64, H(res), fl))
    for kind in ('i64', 'u64', 'i32', 'u32'):
        for i in range(count):
            r = rng.randrange(5)
            if rng.random() < .1: x = special(rng, F)
            else:
                nd = rng.randint(1, p); big = rng.choice([0, 0, 1, 10 ** 9, 10 ** 18, 2 ** 63, 2 ** 64, 2 ** 31, 2 ** 32])
                if big and rng.random() < .8:
                    c = big + rng.randint(-2, 2); c = max(c, 0)
                    sh = rng.randint(0, 3)
                    x = mk(rng.randint(0, 1), c if c < 10 ** p else rcoef(rng, p), -sh if c < 10 ** p else 0)
                else:
                    x = mk(rng.randint(0, 1), rcoef(rng, p), rng.randint(-p - 2, max(0, 20 - p)))
            v, fl = o_toint(f, r, x, kind)
            out.append('to%s %d %d %s -> %x %x' % (kind, f, r, H(x), v, fl))
    for dbl in (True, False):
        for i in range(count):
            r = rng.randrange(5)
            if dbl:
                pick = rng.random()
                if pick < .3: b = rng.getrandbits(64)
                elif pick < .6: x = rng.uniform(-1, 1) * 10.0 ** rng.randint(-30, 30); b = struct.unpack('<Q', struct.pack('<d', x))[0]
                elif pick < .8: x = float(rng.randint(0, 10 ** 17) * 2 ** rng.randint(-60, 10)) / rng.choice([1, 1, 10 ** rng.randint(0, 5)]); b = struct.unpack('<Q', struct.pack('<d', x))[0]
                else: b = rng.choice([0, 1 << 63, 0x7ff0000000000000, 0xfff0000000000000, 0x7ff8000000000000, 0x7fefffffffffffff, 1, 0x000fffffffffffff, 0x0010000000000000])
                x = struct.unpack('<d', struct.pack('<Q', b))[0]
                if math.isnan(x) and (b & 0x0007ffffffffffff) != 0 or (math.isnan(x) and not (b >> 51) & 1): b = 0x7ff8000000000000; x = struct.unpack('<d', struct.pack('<Q', b))[0]
            else:
                pick = rng.random()
                if pick < .5: b = rng.getrandbits(32)
                elif pick < .8:
                    x = rng.uniform(-1, 1) * 10.0 ** rng.randint(-20, 20); b = struct.unpack('<I', struct.pack('<f', x))[0] if abs(x) < 3e38 else 0
                else: b = rng.choice([0, 1 << 31, 0x7f800000, 0xff800000, 0x7fc00000, 0x7f7fffff, 1, 0x00800000])
                x = struct.unpack('<f', struct.pack('<I', b))[0]
                if math.isnan(x): b = 0x7fc00000; x = struct.unpack('<f', struct.pack('<I', b))[0]
            res, fl = o_fromfp(f, r, x, dbl)
            out.append('%s %d %d %x -> %s %x' % ('fromdbl' if dbl else 'fromflt', f, r, b, H(res), fl))
        for i in range(count):
            r = rng.randrange(5)
            if rng.random() < .1: x = special(rng, F)
            else:
                pick = rng.random()
                if pick < .5: e = rng.randint(-p - 330, 310)
                else: e = rng.randint(-p - 60, 40)
                if not dbl:
                    e = rng.randint(-p - 50, 40) if pick < .7 else rng.randint(-p - 60, 45)
                e = max(F['etiny'], min(F['qmax'], e))
                x = mk(rng.randint(0, 1), rcoef(rng, p), e)
            b, fl = o_tofp(f, r, x, dbl)
            out.append('%s %d %d %s -> %x %x' % ('todbl' if dbl else 'toflt', f, r, H(x), b, fl))

def to_sci(v):
    return str(v)

def to_plain(v):
    return format(v, 'f')

def gen_str(f, rng, count, out):
    F = FM[f]; p = F['p']
    H = lambda v: hx(f, enc(f, v))
    for i in range(count):
        x = operand(rng, F, .25)
        for k in 's', 'f':
            if k == 'f' and x.is_finite() and not -300 < x.as_tuple()[2] < 60: continue
            s = to_sci(x) if k == 's' else to_plain(x)
            out.append('tostr %d %s %s -> %s' % (f, H(x), k, 'x' + s.encode().hex()))
    for i in range(count * 2):
        r = rng.randrange(5); c = ctx(f, r)
        kind = rng.random()
        if kind < .5:
            x = operand(rng, F, .2); s = str(x)
            if rng.random() < .3 and x.is_finite():          # other spellings
                t = x.as_tuple(); ds = ''.join(map(str, t[1])); s = ('-' if t[0] else rng.choice(['', '+'])) + ds + 'e%d' % t[2]
            v = Decimal(s.replace('Infinity', 'inf')) if x.is_finite() or True else None
            exp_end = len(s)
            if rng.random() < .4 and not s.startswith(('-', '+')) is None: pass
            pre = rng.choice(['', '', '  ', '\t']); post = rng.choice(['', '', 'xyz', 'e', 'e+', ' 5'])
            full = pre + s + post
            res = v._fix(c)
            if v.is_nan() and not v.is_snan() and not v.is_nan(): pass
            end = len(pre) + exp_end
            if post.startswith('e') and x.is_finite() and 'e' not in s and 'E' not in s and False: pass
        else:                                           # long digit strings: sticky digits and halfway cases
            nd = rng.randint(p, p + 60); digs = str(rng.randint(10 ** (nd - 1), 10 ** nd - 1))
            if rng.random() < .5:
                keep = digs[:p]; digs = keep[:-1] + rng.choice('0123456789') if False else digs
                half = rng.choice(['5', '4999', '5000', '50001']) if True else ''
                digs = str(rng.randint(10 ** (p - 1), 10 ** p - 1)) + half + '0' * rng.randint(0, 25) + rng.choice(['', '', '1', '0000001'])
            pt = rng.randint(0, len(digs)); s = digs[:pt] + ('.' if rng.random() < .8 else '') + digs[pt:]
            if s.startswith('.') and len(s) == 1: s = '0'
            if rng.random() < .8: s += 'e' + str(rng.randint(F['etiny'] - 40, F['qmax'] + 40) if rng.random() < .8 else rng.randint(-10 ** 6, 10 ** 6))
            if rng.random() < .3: s = rng.choice(['-', '+', '']) + s
            full = s; end = len(s)
            v = Decimal(s); res = fix(c, v)
        fl = flags(c)
        out.append('fromstr %d %d %s -> %s %x %d' % (f, r, 'x' + full.encode().hex(), H(res), fl, end))
    # a few fixed syntax cases
    for s, end in [('', 0), ('abc', 0), ('1', 1), ('1.', 2), ('.5', 2), ('.', 0), ('1e', 1), ('1e+', 1), ('1e5x', 3), ('-', 0), ('+.e5', 0), ('  7', 3), ('0x10', 1)]:
        r = 0; c = ctx(f, r)
        res = Decimal(s[:end].strip()) if end else mk(0, 0, 0)
        res = fix(c, res)
        out.append('fromstr %d %d %s -> %s %x %d' % (f, r, 'x' + s.encode().hex(), H(res), flags(c), end))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seed', type=int, default=754); ap.add_argument('--count', type=int, default=60)
    ap.add_argument('--out', default='-')
    a = ap.parse_args()
    out = []
    for f in (64, 128, 32):                      # decimal64 first
        rng = random.Random(a.seed * 1000 + f)
        gen_ops(f, rng, a.count, out)
        gen_conv(f, rng, max(8, a.count // 3), out)
        gen_str(f, rng, max(8, a.count // 4), out)
    text = '\n'.join(out) + '\n'
    if a.out == '-': sys.stdout.write(text)
    else: open(a.out, 'w', newline='\n').write(text)
    sys.stderr.write('%d vectors\n' % len(out))

if __name__ == '__main__':
    main()
