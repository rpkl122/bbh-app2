// Fine-grained (4 KiB step) AVX masked-load scan around a candidate _text.
// Under KPTI the entry trampoline (at _text+0x10..~_text+0x234b) stays mapped
// in the user page table; the rest of the kernel image does not. So the mapped
// band pins _text exactly.
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

#define FOCUS 0xffffffff94e00000ul
#define HALF  0x400000ul /* +/- 4 MiB */
#define STEP  0x1000ul
#define LO    (FOCUS - HALF)
#define N     ((2 * HALF) / STEP) /* 2048 */
#define ITERS 300
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
  unsigned char *hole = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  munmap(hole, 4096);
  signal(SIGSEGV, on_fault); signal(SIGBUS, on_fault);
  if (sigsetjmp(jb, 1) != 0) { printf("FAULT\n"); return 1; }

  static int64_t prof[N];
  for (int i = 0; i < N; i++) prof[i] = delta(LO + (unsigned long)i * STEP, (unsigned long)hole);

  /* print the 60 most-mapped (lowest delta) pages */
  static int idx[N];
  for (int i = 0; i < N; i++) idx[i] = i;
  for (int i = 0; i < N; i++)
    for (int j = i + 1; j < N; j++)
      if (prof[idx[j]] < prof[idx[i]]) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
  printf("=== window 0x%lx..0x%lx step 4K ===\n", LO, LO + (unsigned long)N * STEP);
  printf("=== 60 lowest-delta pages ===\n");
  for (int i = 0; i < 60; i++)
    printf("  0x%016lx  %+7ld\n", LO + (unsigned long)idx[i] * STEP, (long)prof[idx[i]]);
  return 0;
}
