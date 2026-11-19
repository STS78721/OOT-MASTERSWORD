"""Host code at the entry of a lifted SDK function, with the function itself kept: host/*.c's ENTRY_HOOK_SYM(sym) hooks
(host/hos.h), spliced in by the standalone lift (nx2/lift.py).

ENTRY_HOOK_SYM_FROM(sym, min) is the same hook called only when x2, the size argument, is min or more; the spliced line
tests it, so a smaller call stays in the lifted code at the cost of one compare.

An HLE(sym) function replaces the SDK's definition of sym: its address becomes a tail call to the host and the guest code
there is not lifted. An ENTRY_HOOK_SYM(sym) hook runs first and may decline: `int entry_sym_<sym>(Ctx *c)` returns 1 when
it did the call (the function returns at once, x0 is the result) and 0 to run the lifted function as usual. host/nn_libc.c's
memcpy, memmove, memset, memcmp, strlen and strcmp are such hooks: the host's copy where the call stays in the guest
window, the SDK's own (lifted) code for anything else and with HOS_GUEST_LIBC=1.

add() finds each hooked symbol where the sdk module defines it (the image's symbol table, as the HLE set is found) and
records the call for its lifted function; install() wraps the lifter's emit so that function's C starts with it:

    void f_<address>(Ctx *__restrict A64_NOESC c) {
        { extern int entry_sym_memcpy(Ctx *A64_NOESC); if (entry_sym_memcpy(c)) return; }   /* (nx2/entry_hooks.py) */
        ... the lifted function, unchanged ...

Every way into the function goes through its C entry (direct calls, tail calls and indirect calls through a64_lookup),
so every call reaches the hook. The table is the lifter's `entry_hooks` ({entry: C line}); the functions keep their
registers in the context (Lifter.no_local, as for other code spliced in), so the hook reads the arguments the caller left there.
"""
import os
import re

import a64_lift

HOST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "host")
# (the module whose definitions are hooked: the SDK's. A game module defining a symbol of the same name keeps its own)
MODULE = "sdk"
# ENTRY_HOOK_SYM(sym), or ENTRY_HOOK_SYM_FROM(sym, min): called only when x2 (the size) is min or more, tested in the line
HOOK_RE = re.compile(r"^ENTRY_HOOK_SYM(?:\((\w+)\)|_FROM\((\w+),\s*(\d+)\))", re.M)

_installed = False


def hooks(host=HOST):
    """{symbol: the smallest size (x2) the hook is called for, 0 for any} of the hooks host/*.c defines"""
    out = {}
    for f in sorted(os.listdir(host)):
        if f.endswith(".c"):
            with open(os.path.join(host, f), encoding="utf-8", errors="replace") as fh:
                for plain, sym, least in HOOK_RE.findall(fh.read()):
                    out[plain or sym] = int(least) if sym else 0
    return out


def hooked_symbols(host=HOST):
    """The symbols host/*.c defines ENTRY_HOOK_SYM hooks for, sorted, each with its size gate (memset>=8)"""
    return sorted(s if not n else "%s>=%d" % (s, n) for s, n in hooks(host).items())


def line(sym, least=0):
    """The C a hooked function starts with. With a size gate the call is made only from it on (x2 >= least): the smaller
    calls go on into the lifted function at the cost of one compare"""
    gate = "c->x[2] >= %dULL && " % least if least else ""
    return ("    { extern int entry_sym_%s(Ctx *A64_NOESC); if (%sentry_sym_%s(c)) return; }   "
            "/* (nx2/entry_hooks.py: host's %s first) */" % (sym, gate, sym, sym))


def add(lifter, img, host=HOST):
    """Record the splice of each hook in lifter.entry_hooks, for its symbol's definition in the sdk module. A symbol the
    sdk does not define as a function, or whose address is no lifted entry (an HLE'd one), is left out with a note.
    Returns {symbol: address} of the hooks recorded"""
    table = getattr(lifter, "entry_hooks", None)
    if table is None:
        table = lifter.entry_hooks = {}
    found, notes = {}, []
    for sym, least in sorted(hooks(host).items()):
        hit = img.exports.get(sym)
        if not hit:
            notes.append("%s: no module defines it" % sym)
            continue
        m, s = hit
        addr = m.base + s["value"]
        if m.name != MODULE or s.get("type", "func") != "func":
            notes.append("%s: defined by %s (%s), not the %s's function" % (sym, m.name, s.get("type"), MODULE))
        elif addr in lifter.hooks:
            notes.append("%s: %#x is an HLE function (%s)" % (sym, addr, lifter.hooks[addr]))
        elif addr not in lifter.entries:
            notes.append("%s: %#x is no entry of the lift" % (sym, addr))
        else:
            table[addr] = line(sym, least)
            lifter.no_local.add(addr)
            found[sym] = addr
    print("entry hooks: %d of %d (%s)%s" % (
        len(found), len(found) + len(notes),
        ", ".join("%s %s+%#x" % (sym, MODULE, a - next(m.base for m in img.modules if m.name == MODULE))
                  for sym, a in sorted(found.items(), key=lambda kv: kv[1])) or "none",
        "".join("\n  left out: " + n for n in notes)))
    return found


def install():
    """Wrap the lifter's emit: a function with a recorded hook starts with its call (after the head, before anything of
    the function's own, a goto to its entry included). Once, at nx2/lift.py's module level, so the emit's new processes
    wrap it the same way"""
    global _installed
    if _installed:
        return
    _installed = True
    base_emit_function = a64_lift.Lifter.emit_function

    def emit_function(self, e, texts):
        text, refs = base_emit_function(self, e, texts)
        hook = (getattr(self, "entry_hooks", None) or {}).get(e)
        if hook is None:
            return text, refs
        head = "void f_%08x(Ctx *__restrict A64_NOESC c) {\n" % e
        at = text.index(head) + len(head)
        self.stats["entry_hooks"] += 1
        return text[:at] + hook + "\n" + text[at:], refs

    a64_lift.Lifter.emit_function = emit_function
