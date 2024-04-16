

/* Copyright (C) 2011 by Valentin Ochs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Minor changes by Rich Felker for integration in musl, 2011-04-27. */

// NOTE(tbt): slight modifications

/* Smoothsort, an adaptive variant of Heapsort.  Memory usage: O(1).
   Run time: Worst case O(n log n), close to O(n) in the mostly-sorted case. */


static inline int MUSL_a_ctz_32(uint32_t x)
{
	static const char debruijn32[32] = {
		0, 1, 23, 2, 29, 24, 19, 3, 30, 27, 25, 11, 20, 8, 4, 13,
		31, 22, 28, 18, 26, 10, 7, 12, 21, 17, 9, 6, 16, 5, 15, 14
	};
	return debruijn32[(x&-x)*0x076be629 >> 27];
}

static inline int MUSL_a_ctz_64(uint64_t x)
{
	static const char debruijn64[64] = {
		0, 1, 2, 53, 3, 7, 54, 27, 4, 38, 41, 8, 34, 55, 48, 28,
		62, 5, 39, 46, 44, 42, 22, 9, 24, 35, 59, 56, 49, 18, 29, 11,
		63, 52, 6, 26, 37, 40, 33, 47, 61, 45, 43, 21, 23, 58, 17, 10,
		51, 25, 36, 32, 60, 20, 57, 16, 50, 31, 19, 15, 30, 14, 13, 12
	};
	if (sizeof(long) < 8) {
		uint32_t y = x;
		if (!y) {
			y = x>>32;
			return 32 + MUSL_a_ctz_32(y);
		}
		return MUSL_a_ctz_32(y);
	}
	return debruijn64[(x&-x)*0x022fdd63cc95386dull >> 58];
}

static inline int MUSL_a_ctz_l(unsigned long x)
{
	return (sizeof(long) < 8) ? MUSL_a_ctz_32(x) : MUSL_a_ctz_64(x);
}

#define MUSL_ntz(x) MUSL_a_ctz_l((x))

static inline int MUSL_pntz(size_t p[2]) {
	int r = MUSL_ntz(p[0] - 1);
	if(r != 0 || (r = 8*sizeof(size_t) + MUSL_ntz(p[1])) != 8*sizeof(size_t)) {
		return r;
	}
	return 0;
}

static void cycle(size_t width, unsigned char* ar[], int n)
{
	unsigned char tmp[256];
	size_t l;
	int i;
    
	if(n < 2) {
		return;
	}
    
	ar[n] = tmp;
	while(width) {
		l = sizeof(tmp) < width ? sizeof(tmp) : width;
		MemoryCopy(ar[n], ar[0], l);
		for(i = 0; i < n; i++) {
			MemoryCopy(ar[i], ar[i + 1], l);
			ar[i] += l;
		}
		width -= l;
	}
}

/* MUSL_shl() and MUSL_shr() need n > 0 */
static inline void MUSL_shl(size_t p[2], int n)
{
	if(n >= 8 * sizeof(size_t)) {
		n -= 8 * sizeof(size_t);
		p[1] = p[0];
		p[0] = 0;
	}
	p[1] <<= n;
	p[1] |= p[0] >> (sizeof(size_t) * 8 - n);
	p[0] <<= n;
}

static inline void MUSL_shr(size_t p[2], int n)
{
	if(n >= 8 * sizeof(size_t)) {
		n -= 8 * sizeof(size_t);
		p[0] = p[1];
		p[1] = 0;
	}
	p[0] >>= n;
	p[0] |= p[1] << (sizeof(size_t) * 8 - n);
	p[1] >>= n;
}

static void MUSL_sift(unsigned char *head, size_t width, SortComparator cmp, void *arg, int pshift, size_t lp[])
{
	unsigned char *rt, *lf;
	unsigned char *ar[14 * sizeof(size_t) + 1];
	int i = 1;
    
	ar[0] = head;
	while(pshift > 1) {
		rt = head - width;
		lf = head - width - lp[pshift - 2];
        
		if(cmp(ar[0], lf, arg) >= 0 && cmp(ar[0], rt, arg) >= 0) {
			break;
		}
		if(cmp(lf, rt, arg) >= 0) {
			ar[i++] = lf;
			head = lf;
			pshift -= 1;
		} else {
			ar[i++] = rt;
			head = rt;
			pshift -= 2;
		}
	}
	cycle(width, ar, i);
}

static void MUSL_trinkle(unsigned char *head, size_t width, SortComparator cmp, void *arg, size_t pp[2], int pshift, int trusty, size_t lp[])
{
	unsigned char *stepson,
    *rt, *lf;
	size_t p[2];
	unsigned char *ar[14 * sizeof(size_t) + 1];
	int i = 1;
	int trail;
    
	p[0] = pp[0];
	p[1] = pp[1];
    
	ar[0] = head;
	while(p[0] != 1 || p[1] != 0) {
		stepson = head - lp[pshift];
		if(cmp(stepson, ar[0], arg) <= 0) {
			break;
		}
		if(!trusty && pshift > 1) {
			rt = head - width;
			lf = head - width - lp[pshift - 2];
			if(cmp(rt, stepson, arg) >= 0 || cmp(lf, stepson, arg) >= 0) {
				break;
			}
		}
        
		ar[i++] = stepson;
		head = stepson;
		trail = MUSL_pntz(p);
		MUSL_shr(p, trail);
		pshift += trail;
		trusty = 0;
	}
	if(!trusty) {
		cycle(width, ar, i);
		MUSL_sift(head, width, cmp, arg, pshift, lp);
	}
}

void MUSL__qsort_r(void *base, size_t nel, size_t width, SortComparator cmp, void *arg)
{
	size_t lp[12*sizeof(size_t)];
	size_t i, size = width * nel;
	unsigned char *head, *high;
	size_t p[2] = {1, 0};
	int pshift = 1;
	int trail;
    
	if (!size) return;
    
	head = base;
	high = head + size - width;
    
	/* Precompute Leonardo numbers, scaled by element width */
	for(lp[0]=lp[1]=width, i=2; (lp[i]=lp[i-2]+lp[i-1]+width) < size; i++);
    
	while(head < high) {
		if((p[0] & 3) == 3) {
			MUSL_sift(head, width, cmp, arg, pshift, lp);
			MUSL_shr(p, 2);
			pshift += 2;
		} else {
			if(lp[pshift - 1] >= high - head) {
				MUSL_trinkle(head, width, cmp, arg, p, pshift, 0, lp);
			} else {
				MUSL_sift(head, width, cmp, arg, pshift, lp);
			}
            
			if(pshift == 1) {
				MUSL_shl(p, 1);
				pshift = 0;
			} else {
				MUSL_shl(p, pshift - 1);
				pshift = 1;
			}
		}
        
		p[0] |= 1;
		head += width;
	}
    
	MUSL_trinkle(head, width, cmp, arg, p, pshift, 0, lp);
    
	while(pshift != 1 || p[0] != 1 || p[1] != 0) {
		if(pshift <= 1) {
			trail = MUSL_pntz(p);
			MUSL_shr(p, trail);
			pshift += trail;
		} else {
			MUSL_shl(p, 2);
			pshift -= 2;
			p[0] ^= 7;
			MUSL_shr(p, 1);
			MUSL_trinkle(head - lp[pshift] - width, width, cmp, arg, p, pshift + 1, 1, lp);
			MUSL_shl(p, 1);
			p[0] |= 1;
			MUSL_trinkle(head - width, width, cmp, arg, p, pshift, 1, lp);
		}
		head -= width;
	}
}
