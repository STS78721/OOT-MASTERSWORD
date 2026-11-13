/* MasterSword CPU Lab: your own APO-style thread placement, for experiments on your own PC.

   It watches a running game (mastersword.exe by default) and applies rules from cpulab.ini to its threads, by thread name:
   which cores (P, E, all, a list or a mask), how (hard affinity, or Windows CPU Sets: a preference the scheduler keeps),
   thread priority, and Windows' power throttling (EcoQoS). Process-wide: priority class and power throttling.

   It touches nothing but the target process, needs no administrator rights, and everything it sets ends with the process.
   It does not touch Intel's DTT/APO driver or its files.

   Note: MasterSword places its own threads (P-cores for the game, E-cores for shader builds). To experiment from scratch, turn
   that off in hookshot_env.txt: HOS_P_CORES=0, HOS_GAME_P_CORES=0, RX_VK_PAR_ANY_CORE=1, RX_VK_JOBS_ANY_CORE=1, RX_VK_REPLAY_PIN=0.
   The game pins a thread when it makes and names it; the lab looks every interval_ms and applies its rule to every thread it has
   not seen yet or that has a new name since, so a thread ends with the lab's rule.

   Usage:
     cpulab topo                 the CPU's cores: P/E, logical processors, CPU Set ids, L2/L3 sharing
     cpulab threads [exe]        the target's threads: id, name, priority, ideal processor, selected CPU Sets
     cpulab watch [ini]          wait for the target, apply the rules, keep applying them to new and renamed threads
     cpulab watch [ini] --dry    show what would be applied, change nothing

   Build (MSYS2 UCRT64):  gcc -O2 -municode -o cpulab.exe cpulab.c
*/
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>

/* ---- topology ---- */
typedef struct {
    int ncores;
    uint64_t core_mask[64];
    unsigned char core_class[64];
    uint64_t fast, slow, all;   /* logical processor masks: the fastest kind, the others, every one */
    int hybrid;
    ULONG cpuset_id[64];         /* CPU Set id of each logical processor (group 0) */
    int has_cpuset[64];
} Topo;
static Topo topo;

static void topo_read(void) {
    memset(&topo, 0, sizeof topo);
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &len);
    char *buf = (char *)malloc(len);
    if (buf && GetLogicalProcessorInformationEx(RelationProcessorCore, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
        int best = -1;
        for (DWORD at = 0; at < len && topo.ncores < 64;) {
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(buf + at);
            if (!e->Size) break;
            if (e->Relationship == RelationProcessorCore && e->Processor.GroupMask[0].Group == 0) {
                topo.core_mask[topo.ncores] = (uint64_t)e->Processor.GroupMask[0].Mask;
                topo.core_class[topo.ncores] = e->Processor.EfficiencyClass;
                if ((int)e->Processor.EfficiencyClass > best) best = e->Processor.EfficiencyClass;
                topo.ncores++;
            }
            at += e->Size;
        }
        for (int i = 0; i < topo.ncores; i++) {
            topo.all |= topo.core_mask[i];
            if (topo.core_class[i] == best) topo.fast |= topo.core_mask[i];
            else topo.slow |= topo.core_mask[i], topo.hybrid = 1;
        }
    }
    free(buf);
    len = 0;
    GetSystemCpuSetInformation(NULL, 0, &len, GetCurrentProcess(), 0);
    buf = (char *)malloc(len);
    if (buf && GetSystemCpuSetInformation((PSYSTEM_CPU_SET_INFORMATION)buf, len, &len, GetCurrentProcess(), 0))
        for (DWORD at = 0; at < len;) {
            PSYSTEM_CPU_SET_INFORMATION s = (PSYSTEM_CPU_SET_INFORMATION)(buf + at);
            if (!s->Size) break;
            if (s->Type == CpuSetInformation && s->CpuSet.Group == 0 && s->CpuSet.LogicalProcessorIndex < 64) {
                topo.cpuset_id[s->CpuSet.LogicalProcessorIndex] = s->CpuSet.Id;
                topo.has_cpuset[s->CpuSet.LogicalProcessorIndex] = 1;
            }
            at += s->Size;
        }
    free(buf);
}

static void print_mask_list(uint64_t m) {
    int first = 1;
    for (int i = 0; i < 64; i++)
        if (m >> i & 1) {
            int j = i;
            while (j + 1 < 64 && (m >> (j + 1) & 1)) j++;
            printf("%s%d", first ? "" : ",", i);
            if (j > i) printf("-%d", j);
            first = 0, i = j;
        }
    if (first) printf("(none)");
}

static void cmd_topo(void) {
    printf("%d cores, %s\n", topo.ncores, topo.hybrid ? "hybrid (two kinds of core)" : "all of one kind");
    printf("  fastest kind (P): mask %#llx = ", (unsigned long long)topo.fast), print_mask_list(topo.fast), printf("\n");
    if (topo.hybrid) printf("  other kind   (E): mask %#llx = ", (unsigned long long)topo.slow), print_mask_list(topo.slow), printf("\n");
    for (int i = 0; i < topo.ncores; i++) {
        printf("  core %2d: logical ", i), print_mask_list(topo.core_mask[i]);
        printf("  class %d%s", topo.core_class[i], topo.hybrid ? (topo.core_mask[i] & topo.fast ? " (P)" : " (E)") : "");
        for (int b = 0; b < 64; b++)
            if (topo.core_mask[i] >> b & 1 && topo.has_cpuset[b]) printf("  cpuset id %lu", topo.cpuset_id[b]);
        printf("\n");
    }
    /* caches: which logical processors share each L2 and L3 */
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationCache, NULL, &len);
    char *buf = (char *)malloc(len);
    if (buf && GetLogicalProcessorInformationEx(RelationCache, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len))
        for (DWORD at = 0; at < len;) {
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(buf + at);
            if (!e->Size) break;
            if (e->Relationship == RelationCache && (e->Cache.Level == 2 || e->Cache.Level == 3) &&
                (e->Cache.Type == CacheUnified || e->Cache.Level == 3)) {
                printf("  L%d %5lu KB shared by logical ", e->Cache.Level, e->Cache.CacheSize / 1024);
                print_mask_list((uint64_t)e->Cache.GroupMask.Mask), printf("\n");
            }
            at += e->Size;
        }
    free(buf);
}

/* ---- rules ---- */
enum { METHOD_AFFINITY, METHOD_CPUSET };
typedef struct {
    char match[64];       /* a substring of the thread's name; "*" any; "" (unnamed) for threads without a name */
    uint64_t cores;       /* 0: leave alone */
    int method;
    int priority;         /* THREAD_PRIORITY_*, or 99: leave alone */
    int throttle;         /* -1 leave alone, 0 off (no EcoQoS), 1 on */
} Rule;
static Rule rules[64];
static int nrules;
static char target_exe[MAX_PATH] = "mastersword.exe";
static int interval_ms = 2000;
static int proc_class = 0;          /* 0: leave alone */
static int proc_throttle = -1;

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int parse_cores(const char *v, uint64_t *out) {
    if (!_stricmp(v, "P")) return *out = topo.fast, 1;
    if (!_stricmp(v, "E")) return *out = topo.hybrid ? topo.slow : topo.all, 1;
    if (!_stricmp(v, "all")) return *out = topo.all, 1;
    if (!_strnicmp(v, "mask:", 5)) return *out = strtoull(v + 5, NULL, 0), 1;
    if (!_strnicmp(v, "list:", 5)) {   /* list:0,1,10-13 */
        uint64_t m = 0;
        const char *p = v + 5;
        while (*p) {
            char *end;
            long a = strtol(p, &end, 10), b = a;
            if (end == p) return 0;
            if (*end == '-') b = strtol(end + 1, &end, 10);
            for (long i = a; i <= b && i < 64; i++) if (i >= 0) m |= 1ull << i;
            p = *end == ',' ? end + 1 : end;
            if (*end != ',' && *end) return 0;
        }
        return *out = m, 1;
    }
    if (!_strnicmp(v, "Pminus:", 7)) {   /* Pminus:23 or Pminus:22,23: the P-cores less those logical processors */
        uint64_t less = 0;
        char tmp[96];
        snprintf(tmp, sizeof tmp, "list:%s", v + 7);
        if (!parse_cores(tmp, &less)) return 0;
        return *out = topo.fast & ~less, 1;
    }
    return 0;
}

static int parse_priority(const char *v) {
    static const struct { const char *n; int p; } k[] = {
        {"idle", THREAD_PRIORITY_IDLE}, {"lowest", THREAD_PRIORITY_LOWEST}, {"below_normal", THREAD_PRIORITY_BELOW_NORMAL},
        {"normal", THREAD_PRIORITY_NORMAL}, {"above_normal", THREAD_PRIORITY_ABOVE_NORMAL}, {"highest", THREAD_PRIORITY_HIGHEST},
        {"time_critical", THREAD_PRIORITY_TIME_CRITICAL}, {"keep", 99}};
    for (size_t i = 0; i < sizeof k / sizeof k[0]; i++) if (!_stricmp(v, k[i].n)) return k[i].p;
    return 99;
}
static int parse_class(const char *v) {
    if (!_stricmp(v, "idle")) return IDLE_PRIORITY_CLASS;
    if (!_stricmp(v, "below_normal")) return BELOW_NORMAL_PRIORITY_CLASS;
    if (!_stricmp(v, "normal")) return NORMAL_PRIORITY_CLASS;
    if (!_stricmp(v, "above_normal")) return ABOVE_NORMAL_PRIORITY_CLASS;
    if (!_stricmp(v, "high")) return HIGH_PRIORITY_CLASS;
    return 0;   /* (realtime is not offered: it can starve the system) */
}
static int parse_throttle(const char *v) { return !_stricmp(v, "off") ? 0 : !_stricmp(v, "on") ? 1 : -1; }

static int load_ini(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cpulab: cannot open %s\n", path); return 0; }
    char line[512], section[32] = "";
    Rule *r = NULL;
    int lineno = 0, ok = 1;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        char *s = trim(line);
        char *c = strchr(s, ';');
        if (c) *c = 0, s = trim(s);
        if (!*s || *s == '#') continue;
        if (*s == '[') {
            snprintf(section, sizeof section, "%s", s);
            if (!_stricmp(section, "[rule]")) {
                if (nrules >= 64) { fprintf(stderr, "cpulab: more than 64 rules\n"); ok = 0; break; }
                r = &rules[nrules++];
                memset(r, 0, sizeof *r), r->priority = 99, r->throttle = -1, r->method = METHOD_AFFINITY;
                strcpy(r->match, "*");
            } else r = NULL;
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) { fprintf(stderr, "cpulab: %s:%d: no '='\n", path, lineno); ok = 0; continue; }
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (!_stricmp(section, "[target]")) {
            if (!_stricmp(k, "exe")) snprintf(target_exe, sizeof target_exe, "%s", v);
            else if (!_stricmp(k, "interval_ms")) interval_ms = atoi(v) < 250 ? 250 : atoi(v);
        } else if (!_stricmp(section, "[process]")) {
            if (!_stricmp(k, "priority")) proc_class = parse_class(v);
            else if (!_stricmp(k, "power_throttling")) proc_throttle = parse_throttle(v);
        } else if (r) {
            if (!_stricmp(k, "match")) snprintf(r->match, sizeof r->match, "%s", v);
            else if (!_stricmp(k, "cores")) {
                if (!parse_cores(v, &r->cores)) { fprintf(stderr, "cpulab: %s:%d: cores '%s'?\n", path, lineno, v); ok = 0; }
            } else if (!_stricmp(k, "method")) r->method = !_stricmp(v, "cpuset") ? METHOD_CPUSET : METHOD_AFFINITY;
            else if (!_stricmp(k, "priority")) r->priority = parse_priority(v);
            else if (!_stricmp(k, "power_throttling")) r->throttle = parse_throttle(v);
        }
    }
    fclose(f);
    return ok;
}

/* ---- the target ---- */
typedef HRESULT(WINAPI *GetDescFn)(HANDLE, PWSTR *);
static GetDescFn get_desc;

static DWORD find_process(const char *exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    memset(&pe, 0, sizeof pe);
    pe.dwSize = sizeof pe;
    wchar_t wexe[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, exe, -1, wexe, MAX_PATH);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe))
        do
            if (!_wcsicmp(pe.szExeFile, wexe)) { pid = pe.th32ProcessID; break; }
        while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return pid;
}

static void thread_name(HANDLE h, char *out, size_t n) {
    out[0] = 0;
    PWSTR w = NULL;
    if (get_desc && SUCCEEDED(get_desc(h, &w)) && w) {
        WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)n, NULL, NULL);
        LocalFree(w);
    }
}

static const Rule *rule_for(const char *name) {
    for (int i = 0; i < nrules; i++) {
        const Rule *r = &rules[i];
        if (!strcmp(r->match, "*") || (!r->match[0] && !name[0]) || (r->match[0] && strstr(name, r->match))) return r;
    }
    return NULL;
}

static int apply_rule(HANDLE h, const Rule *r, int dry, char *what, size_t n) {
    int k = 0;
    what[0] = 0;
    if (r->cores) {
        uint64_t m = r->cores & topo.all;
        if (!m) return 0;
        if (r->method == METHOD_CPUSET) {
            ULONG ids[64];
            ULONG nids = 0;
            for (int b = 0; b < 64; b++) if (m >> b & 1 && topo.has_cpuset[b]) ids[nids++] = topo.cpuset_id[b];
            if (!dry) {
                SetThreadAffinityMask(h, (DWORD_PTR)topo.all);   /* (CPU Sets work inside the affinity: open it up first) */
                SetThreadSelectedCpuSets(h, ids, nids);
            }
            k += snprintf(what + k, n - k, "cpusets %#llx", (unsigned long long)m);
        } else {
            if (!dry) { SetThreadSelectedCpuSets(h, NULL, 0); SetThreadAffinityMask(h, (DWORD_PTR)m); }
            k += snprintf(what + k, n - k, "affinity %#llx", (unsigned long long)m);
        }
    }
    if (r->priority != 99) {
        if (!dry) SetThreadPriority(h, r->priority);
        k += snprintf(what + k, n - k, "%spriority %d", k ? ", " : "", r->priority);
    }
    if (r->throttle >= 0) {
        THREAD_POWER_THROTTLING_STATE s = {THREAD_POWER_THROTTLING_CURRENT_VERSION, THREAD_POWER_THROTTLING_EXECUTION_SPEED,
                                           r->throttle ? THREAD_POWER_THROTTLING_EXECUTION_SPEED : 0};
        if (!dry) SetThreadInformation(h, ThreadPowerThrottling, &s, sizeof s);
        k += snprintf(what + k, n - k, "%sEcoQoS %s", k ? ", " : "", r->throttle ? "on" : "off");
    }
    return 1;
}

static void apply_process(DWORD pid, int dry) {
    HANDLE p = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) { printf("  process: cannot open (%lu)\n", GetLastError()); return; }
    if (proc_class) {
        if (!dry) SetPriorityClass(p, proc_class);
        printf("  process: priority class %#x%s\n", proc_class, dry ? " (dry)" : "");
    }
    if (proc_throttle >= 0) {
        PROCESS_POWER_THROTTLING_STATE s = {PROCESS_POWER_THROTTLING_CURRENT_VERSION, PROCESS_POWER_THROTTLING_EXECUTION_SPEED,
                                            proc_throttle ? PROCESS_POWER_THROTTLING_EXECUTION_SPEED : 0};
        if (!dry) SetProcessInformation(p, ProcessPowerThrottling, &s, sizeof s);
        printf("  process: EcoQoS %s%s\n", proc_throttle ? "on" : "off", dry ? " (dry)" : "");
    }
    CloseHandle(p);
}

/* what was applied to which thread, by its name then: a thread renamed later is looked at again */
typedef struct { DWORD tid; char name[64]; } Seen;
static Seen seen[4096];
static int nseen;

static int seen_same(DWORD tid, const char *name) {
    for (int i = 0; i < nseen; i++)
        if (seen[i].tid == tid) {
            if (!strcmp(seen[i].name, name)) return 1;
            snprintf(seen[i].name, sizeof seen[i].name, "%s", name);
            return 0;
        }
    if (nseen < 4096) seen[nseen].tid = tid, snprintf(seen[nseen].name, sizeof seen[nseen].name, "%s", name), nseen++;
    return 0;
}

static int pass(DWORD pid, int dry, int list_only) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te;
    memset(&te, 0, sizeof te);
    te.dwSize = sizeof te;
    int n = 0, changed = 0;
    if (Thread32First(snap, &te))
        do {
            if (te.th32OwnerProcessID != pid) continue;
            n++;
            HANDLE h = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION | THREAD_SET_LIMITED_INFORMATION,
                                  FALSE, te.th32ThreadID);
            if (!h) continue;
            char name[64];
            thread_name(h, name, sizeof name);
            if (list_only) {
                PROCESSOR_NUMBER ideal = {0};
                GetThreadIdealProcessorEx(h, &ideal);
                ULONG ids[64], count = 0;
                GetThreadSelectedCpuSets(h, ids, 64, &count);
                printf("  %6lu  %-36s prio %3d  ideal %2u  cpusets %lu%s\n", te.th32ThreadID, name[0] ? name : "(unnamed)",
                       GetThreadPriority(h), ideal.Number, count, count ? "" : " (any)");
            } else if (!seen_same(te.th32ThreadID, name)) {
                const Rule *r = rule_for(name);
                char what[160];
                if (r && apply_rule(h, r, dry, what, sizeof what) && what[0]) {
                    printf("  %6lu  %-36s -> %s%s\n", te.th32ThreadID, name[0] ? name : "(unnamed)", what, dry ? " (dry)" : "");
                    changed++;
                }
            }
            CloseHandle(h);
        } while (Thread32Next(snap, &te));
    CloseHandle(snap);
    if (list_only) printf("%d threads\n", n);
    return changed;
}

int wmain(int argc, wchar_t **wargv) {
    SetConsoleOutputCP(CP_UTF8);
    get_desc = (GetDescFn)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription");
    topo_read();
    char argv[4][MAX_PATH] = {{0}};
    for (int i = 1; i < argc && i < 4; i++) WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], MAX_PATH, NULL, NULL);
    const char *cmd = argc > 1 ? argv[1] : "";
    if (!strcmp(cmd, "topo")) { cmd_topo(); return 0; }
    if (!strcmp(cmd, "threads")) {
        if (argc > 2) snprintf(target_exe, sizeof target_exe, "%s", argv[2]);
        DWORD pid = find_process(target_exe);
        if (!pid) { printf("%s is not running\n", target_exe); return 1; }
        printf("%s (pid %lu):\n", target_exe, pid);
        pass(pid, 1, 1);
        return 0;
    }
    if (!strcmp(cmd, "watch")) {
        char ini[MAX_PATH] = "cpulab.ini";
        int dry = 0;
        for (int i = 2; i < argc && i < 4; i++) {
            if (!strcmp(argv[i], "--dry")) dry = 1;
            else snprintf(ini, sizeof ini, "%s", argv[i]);
        }
        if (!load_ini(ini)) return 1;
        printf("cpulab: %d rules for %s, every %d ms%s. Ctrl+C to stop (what is set ends with the game).\n", nrules, target_exe,
               interval_ms, dry ? ", dry run" : "");
        DWORD pid = 0;
        for (;;) {
            const DWORD now = find_process(target_exe);
            if (now != pid) {
                pid = now, nseen = 0;
                if (pid) printf("%s started (pid %lu)\n", target_exe, pid), apply_process(pid, dry);
                else printf("waiting for %s...\n", target_exe);
            }
            if (pid) pass(pid, dry, 0);
            Sleep(interval_ms);
        }
    }
    printf("MasterSword CPU Lab\n"
           "  cpulab topo                 the CPU: P/E cores, logical processors, CPU Set ids, caches\n"
           "  cpulab threads [exe]        the target's threads (default mastersword.exe)\n"
           "  cpulab watch [ini] [--dry]  apply cpulab.ini's rules to the running game, and keep applying them\n");
    return 0;
}
