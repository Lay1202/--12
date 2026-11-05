#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define MAXKEY        1000000
#define SUPP_TRIALS   100
#define BONUS_N       10000
#define BONUS_REPEAT  500
#define BONUS_ROUNDS  3
#define DATA_SEED     20240601ULL
#define PIVOT_SEED    987654321ULL

enum { PIVOT_FIRST = 0, PIVOT_RANDOM = 1, PIVOT_MEDIAN3 = 2 };

static const char *PIVOT_LABEL[3] = {
    "Quick Sort (First)   ",
    "Quick Sort (Random)  ",
    "Quick Sort (Median-3)"
};

static const int SIZES[] = {1000, 2000, 4000, 8000, 16000, 20000};
#define NSIZES ((int)(sizeof(SIZES) / sizeof(SIZES[0])))

static long long g_comp = 0;

static unsigned long long data_state = DATA_SEED;
static unsigned long long pivot_state = PIVOT_SEED;

static unsigned long long xorshift(unsigned long long *s) {
    *s ^= *s >> 12;
    *s ^= *s << 25;
    *s ^= *s >> 27;
    return *s * 2685821657736338717ULL;
}

static void gen_random(int *a, int n) {
    for (int i = 0; i < n; i++)
        a[i] = (int)(xorshift(&data_state) % (unsigned long long)(MAXKEY + 1));
}

static void swap_int(int *x, int *y) {
    int t = *x;
    *x = *y;
    *y = t;
}

static int median_of_three(const int *a, int lo, int hi) {
    int mid = lo + (hi - lo) / 2;
    int x = a[lo], y = a[mid], z = a[hi];
    g_comp++;
    if (x < y) {
        g_comp++;
        if (y < z) return mid;
        g_comp++;
        if (x < z) return hi;
        return lo;
    }
    g_comp++;
    if (x < z) return lo;
    g_comp++;
    if (y < z) return hi;
    return mid;
}

static int choose_pivot(const int *a, int lo, int hi, int type) {
    if (type == PIVOT_FIRST) return lo;
    if (type == PIVOT_RANDOM)
        return lo + (int)(xorshift(&pivot_state) % (unsigned long long)(hi - lo + 1));
    return median_of_three(a, lo, hi);
}

static int partition(int *a, int lo, int hi, int type) {
    int p = choose_pivot(a, lo, hi, type);
    swap_int(&a[lo], &a[p]);
    int pivot = a[lo];
    int i = lo;
    for (int j = lo + 1; j <= hi; j++) {
        g_comp++;
        if (a[j] < pivot) {
            i++;
            swap_int(&a[i], &a[j]);
        }
    }
    swap_int(&a[lo], &a[i]);
    return i;
}

static void quick_recursive(int *a, int lo, int hi, int type) {
    if (lo >= hi) return;
    int p = partition(a, lo, hi, type);
    quick_recursive(a, lo, p - 1, type);
    quick_recursive(a, p + 1, hi, type);
}

static void quick_iterative(int *a, int n, int type) {
    if (n < 2) return;
    int *stack = (int *)malloc(sizeof(int) * 2 * ((size_t)n + 1));
    int top = 0;
    stack[top++] = 0;
    stack[top++] = n - 1;
    while (top > 0) {
        int hi = stack[--top];
        int lo = stack[--top];
        int p = partition(a, lo, hi, type);
        if (p + 1 < hi) {
            stack[top++] = p + 1;
            stack[top++] = hi;
        }
        if (lo < p - 1) {
            stack[top++] = lo;
            stack[top++] = p - 1;
        }
    }
    free(stack);
}

static int is_sorted(const int *a, int n) {
    for (int i = 0; i + 1 < n; i++)
        if (a[i] > a[i + 1]) return 0;
    return 1;
}

static void checksum(const int *a, int n, long long *s1, long long *s2) {
    long long x = 0, y = 0;
    for (int i = 0; i < n; i++) {
        x += a[i];
        y += (long long)a[i] * a[i];
    }
    *s1 = x;
    *s2 = y;
}

static int verify(const int *result, const int *orig, int n) {
    long long a1, a2, b1, b2;
    checksum(result, n, &a1, &a2);
    checksum(orig, n, &b1, &b2);
    return is_sorted(result, n) && a1 == b1 && a2 == b2;
}

static long long run_recursive(const int *orig, int *work, int n, int type, int *ok) {
    memcpy(work, orig, sizeof(int) * (size_t)n);
    pivot_state = PIVOT_SEED;
    g_comp = 0;
    quick_recursive(work, 0, n - 1, type);
    *ok = verify(work, orig, n);
    return g_comp;
}

static long long run_iterative(const int *orig, int *work, int n, int type, int *ok) {
    memcpy(work, orig, sizeof(int) * (size_t)n);
    pivot_state = PIVOT_SEED;
    g_comp = 0;
    quick_iterative(work, n, type);
    *ok = verify(work, orig, n);
    return g_comp;
}

static double now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

int main(void) {
    int maxn = SIZES[NSIZES - 1];
    int *orig = (int *)malloc(sizeof(int) * (size_t)maxn);
    int *work = (int *)malloc(sizeof(int) * (size_t)maxn);
    int all_ok = 1;

    for (int s = 0; s < NSIZES; s++) {
        int n = SIZES[s];
        gen_random(orig, n);
        printf("Data Size : %d\n\n", n);
        for (int t = 0; t < 3; t++) {
            int ok;
            long long c = run_recursive(orig, work, n, t, &ok);
            if (!ok) all_ok = 0;
            printf("%s : %lld comparisons\n", PIVOT_LABEL[t], c);
        }
        printf("\n");
    }
    printf("All results sorted correctly : %s\n\n", all_ok ? "YES" : "NO");
    printf("------------------------------------\n\n");

    printf("[Supplementary 1] Average comparisons over %d random datasets\n\n", SUPP_TRIALS);
    for (int s = 0; s < NSIZES; s++) {
        int n = SIZES[s];
        long long total[3] = {0, 0, 0};
        for (int k = 0; k < SUPP_TRIALS; k++) {
            gen_random(orig, n);
            for (int t = 0; t < 3; t++) {
                int ok;
                total[t] += run_recursive(orig, work, n, t, &ok);
                if (!ok) all_ok = 0;
            }
        }
        printf("Data Size : %d\n", n);
        for (int t = 0; t < 3; t++)
            printf("%s : %.1f comparisons\n", PIVOT_LABEL[t], (double)total[t] / SUPP_TRIALS);
        printf("\n");
    }
    printf("------------------------------------\n\n");

    printf("[Supplementary 2] Already sorted input (iterative implementation)\n\n");
    for (int s = 0; s < NSIZES; s++) {
        int n = SIZES[s];
        for (int i = 0; i < n; i++) orig[i] = i;
        printf("Data Size : %d\n", n);
        for (int t = 0; t < 3; t++) {
            int ok;
            long long c = run_iterative(orig, work, n, t, &ok);
            if (!ok) all_ok = 0;
            printf("%s : %lld comparisons\n", PIVOT_LABEL[t], c);
        }
        printf("\n");
    }
    printf("All supplementary results sorted correctly : %s\n\n", all_ok ? "YES" : "NO");
    printf("------------------------------------\n\n");

    {
        int n = BONUS_N;
        int ok_r, ok_i;
        double total_r = 0.0, total_i = 0.0;
        long long comp_r, comp_i;
        gen_random(orig, n);

        comp_r = run_recursive(orig, work, n, PIVOT_MEDIAN3, &ok_r);
        comp_i = run_iterative(orig, work, n, PIVOT_MEDIAN3, &ok_i);

        for (int round = 0; round < BONUS_ROUNDS; round++) {
            for (int r = 0; r < BONUS_REPEAT; r++) {
                double t0, t1;
                memcpy(work, orig, sizeof(int) * (size_t)n);
                pivot_state = PIVOT_SEED;
                t0 = now_ms();
                quick_recursive(work, 0, n - 1, PIVOT_MEDIAN3);
                t1 = now_ms();
                total_r += t1 - t0;
            }
            for (int r = 0; r < BONUS_REPEAT; r++) {
                double t0, t1;
                memcpy(work, orig, sizeof(int) * (size_t)n);
                pivot_state = PIVOT_SEED;
                t0 = now_ms();
                quick_iterative(work, n, PIVOT_MEDIAN3);
                t1 = now_ms();
                total_i += t1 - t0;
            }
        }
        {
            double runs = (double)BONUS_REPEAT * BONUS_ROUNDS;
            printf("[Bonus] Recursive vs Iterative Quick Sort\n\n");
            printf("Data Size : %d\n", n);
            printf("Pivot     : Median-of-Three\n\n");
            printf("Recursive Quick Sort : %.4f ms\n", total_r / runs);
            printf("Iterative Quick Sort : %.4f ms\n\n", total_i / runs);
            printf("Average of %d runs per method (copy time excluded)\n", (int)runs);
            printf("Comparisons (Recursive / Iterative) : %lld / %lld\n", comp_r, comp_i);
            printf("Both sorted correctly : %s\n", (ok_r && ok_i) ? "YES" : "NO");
        }
    }

    free(orig);
    free(work);
    return 0;
}
