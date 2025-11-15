
## `MAX_ADD` constant Derivation

See tarp/hash/checksums/adler32.hxx for where this constant is used.
This is [the 'NMAX' constant used in zlib source code](https://github.com/madler/zlib/blob/develop/adler32.c);
its purpose is the same as for the `MAX_ADD` used for fletcher checksums
(see tarp/hash/checksums/fletcher.hxx): it is the maximum number of
additions we can perform on the internal accumulators before we must
perform a modulo operation to reduce them, otherwise overflow will occur.

Hence, the purpose of this constant is for the same optimization as for
fletcher checksums: delay the relatively expensive modulo operation, i.e.
perform as many additions as safe without doing a modulo.
See tarp/include/hash/checksums/fletcher.hxx for how the `MAX_ADD` is derived
for fletcher checksums. For adler32 the derivation of this value is more
complicated, for a number of reasons:

- the `sum1` internal accumulator of adler32 starts at 1, not 0
- the internal accumulators are reduced modulo `MOD_ADLER`, which is `65521`,
  the largest prime number < 2^16-1 (since, like fletcher32, the width of
  the internal accumulators is half the width of the checksum; hence adler32
  produces a32 bit checksum and uses 16bit-wide internal accumulators).
  This is in contrast to fletcher32, which uses 2^16-1 as the modulus.
   * NOTE `MOD_ADLER` is [the wikipedia term](https://en.wikipedia.org/wiki/Adler-32);
     `zlib` source code calls it `BASE`.
- adler32 as implemented in zlib soure code, uses _another_ optimization
  that effectively serves the same purpose: coalescing together additions
  before a modulo operation is necessary. This optimization relies on the
  fact that for two values x and y, with `y <= x < 2y`, `x MOD y` is the
  same as `x - y`, since in that specific case `x/y={1, r=x-y}`.
  Therefore for short inputs (shorter than can be covered by the generic case
  where the `MOD_ADLER` optimization is applied in a loop), this second
  optimization is used: the internal accumulators are allowed to accumulate
  until they are found to have a value `>= MOD_ADLER`; when that is found
  to be the case, `MOD_ADLER` is subtracted from them, hence replacing the
  relatively expensive modulo operation with a much cheaper subtraction.
  Therefore, at any given point when we're called to process a buffer, the
  value in the internal accmulators can be anywhere between
  [0, `MOD_ADLER-1`] and hence equal to `MOD_ADLER-` (or `BASE-1`, to use
  the `zlib` source code term); this then further complicates the derivation
  of the `MAX_ADD` constant.
   * NOTE that his optimization is also well known from the fletcher
     checksum. The wikipedia article on the fletcher checksum explicitly
     mentions both optimizations:
      > In a 1988 paper, Anastase Nakassis discussed and compared different
      > ways to optimize the algorithm. The most important optimization
      > consists in using larger accumulators and delaying the relatively
      > costly modulo operation for as long as it can be proven that no
      > overflow will occur. Further benefit can be derived from replacing
      > the modulo operator with an equivalent function tailored to this
      > specific case—for instance, a simple compare-and-subtract, since the
      > quotient never exceeds 1.

The derivation is shown below.
```
To keep the following more readable,
let B = BASE-1=65520
-----
In zlib source code, any time sum1 or sum2 are found to be >= BASE (i.e. > B),
they are reduced (S -= BASE, where S is sum1 or sum2, as appropriate,
since they wrap at different rates).

Therefore, at any given point when we receive a buffer for processing,
sum1 could be at most B, and similarly Sum2 could be at most B (i.e. <= BASE-1)
--- otherwise the previous call would've performed (sum1 -= BASE, and/or sum2 -= BASE,
respectively) or similar.

Assume the buffer received has length n bytes; there because the checksum word
is 1 byte, n processing steps (1 per byte) are performed. After n steps (bytes),
in the worst case (i.e. assuming each byte=maxw=2^8-1=255):
-----
sum1 =  B + maxw  // n=1
sum1 =  B + maxw + maxw  // n=2
sum1 =  B + maxw + maxw + maxw  // n=3
...
sum1 = B + n*maxw
-----

For sum2 [with sum2=B at the beginning and sum2+=sum1 at each step]:
sum2 = B + sum1
     = B + (B + maxw)  // n=1

sum2 = (B + B + maxw) + (B + maxw*2)
     = 3B + 3maxw  // n=2

sum2 = (3B + 3maxw) + (B + maxw*3)
     = 4B +6maxw  // n=3
     
sum2 = (4B + 4maxw) + (B + maxw*4)
     = 5B + 8maxw  // n=4

After n steps:
sum1 = B + n*maxw
sum2 = (n+1)B + maxw * (1 + 2 + 3 + ... + n)

Here (1+2+3+..+n) is the triangular number formula:
     n * (n+1) / 2
so, after n steps:
sum2 = [(n+1) * B] + [maxw * [(n * (n+1))/2]]
---------

This must be <= u32::max; we want to find the highest n for which this holds.
[(n+1) * B] + [maxw * [(n * (n+1))/2]] <= u32max
(n+1) * B + (n+1) * maxw * n * 1/2 <= u32max
(n+1) (B + (maxw *n * 1/2))

2 * (n+1) (B + (maxw * n * 1/2)) <= u32max * 2
(2n + 2) (B+ (maxw*n*1/2)) <= u32max * 2
2nB + 2B + maxw * n  + maxw * n^2  <= u32max * 2

with B=65520, and maxw=255 we have:
n*2*65520 + 2*65520 + n * 255 + n^2 * 255 <= u32max * 2
n*131040 + 131040 + n * 255 + n^2 * 255 <= u32max * 2
n^2 * 255 + n * 255 + n * 131040 + 131040 <= u32max * 2
n^2 * 255 + n * (255 + 131040) + 131040 <= u32max * 2
n^2 * 255 + n * 131295 + 131040 <= u32max * 2
n^2 * 255 + n * 131295 <= (u32max * 2) - 131040
n^2 * 255 + n * 131295 <= 8589803550
255*n^2 + 131295 * n <= 8589803550
--
255*n^2 + 131295 * n - 8589803550 <= 0
--
quadratic formula [with a=255, b=131295, c=-8589803550]:
(-131295 +/- sqrt(131295^2 - 4*255*-8589803550))/2*255
(-131295 +/- sqrt(131295^2 - -8761599621000))/510
(-131295 +/- sqrt(8778837998025))/510
(-131295 +/- 2962910.39)/510
(2962910 - 131295)/510
=5552.1, = 5552, floored.

This is the NMAX value found in zlib source code.
```


