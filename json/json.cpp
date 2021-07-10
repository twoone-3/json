#include "json.h"
#include <cassert>

#define JSON_CHECK(expression) do { ErrorCode tmp = expression; if (tmp != OK)return tmp; } while (0)
#define JSON_SKIP JSON_CHECK(skipWhiteSpace(cur))
#define JSON_UTF8 0

namespace json {
/*!
implements the Grisu2 algorithm for binary to decimal floating-point
conversion.
Adapted from JSON for Modern C++

This implementation is a slightly modified version of the reference
implementation which may be obtained from
http://florian.loitsch.com/publications (bench.tar.gz).
The code is distributed under the MIT license, Copyright (c) 2009 Florian
Loitsch. For a detailed description of the algorithm see: [1] Loitsch, "Printing
Floating-Point Numbers Quickly and Accurately with Integers", Proceedings of the
ACM SIGPLAN 2010 Conference on Programming Language Design and Implementation,
PLDI 2010 [2] Burger, Dybvig, "Printing Floating-Point Numbers Quickly and
Accurately", Proceedings of the ACM SIGPLAN 1996 Conference on Programming
Language Design and Implementation, PLDI 1996
*/
namespace dtoa {

template <typename Target, typename Source>
Target reinterpret_bits(const Source source) {
	static_assert(sizeof(Target) == sizeof(Source), "size mismatch");

	Target target;
	std::memcpy(&target, &source, sizeof(Source));
	return target;
}
// f * 2^e
struct diyfp {
	static constexpr int kPrecision = 64; // = q

	uint64_t f = 0;
	int e = 0;

	constexpr diyfp(uint64_t f_, int e_) noexcept : f(f_), e(e_) {}

	/*!
	@brief returns x - y
	@pre x.e == y.e and x.f >= y.f
	*/
	static diyfp sub(const diyfp& x, const diyfp& y) noexcept {
		return { x.f - y.f, x.e };
	}

	/*!
	@brief returns x * y
	@note The result is rounded. (Only the upper q bits are returned.)
	*/
	static diyfp mul(const diyfp& x, const diyfp& y) noexcept {
		static_assert(kPrecision == 64, "atod error");

		// Computes:
		//  f = round((x.f * y.f) / 2^q)
		//  e = x.e + y.e + q

		// Emulate the 64-bit * 64-bit multiplication:
		//
		// p = u * v
		//   = (u_lo + 2^32 u_hi) (v_lo + 2^32 v_hi)
		//   = (u_lo v_lo         ) + 2^32 ((u_lo v_hi         ) + (u_hi v_lo )) +
		//   2^64 (u_hi v_hi         ) = (p0                ) + 2^32 ((p1 ) + (p2 ))
		//   + 2^64 (p3                ) = (p0_lo + 2^32 p0_hi) + 2^32 ((p1_lo +
		//   2^32 p1_hi) + (p2_lo + 2^32 p2_hi)) + 2^64 (p3                ) =
		//   (p0_lo             ) + 2^32 (p0_hi + p1_lo + p2_lo ) + 2^64 (p1_hi +
		//   p2_hi + p3) = (p0_lo             ) + 2^32 (Q ) + 2^64 (H ) = (p0_lo ) +
		//   2^32 (Q_lo + 2^32 Q_hi                           ) + 2^64 (H )
		//
		// (Since Q might be larger than 2^32 - 1)
		//
		//   = (p0_lo + 2^32 Q_lo) + 2^64 (Q_hi + H)
		//
		// (Q_hi + H does not overflow a 64-bit int)
		//
		//   = p_lo + 2^64 p_hi

		const uint64_t u_lo = x.f & 0xFFFFFFFFu;
		const uint64_t u_hi = x.f >> 32u;
		const uint64_t v_lo = y.f & 0xFFFFFFFFu;
		const uint64_t v_hi = y.f >> 32u;

		const uint64_t p0 = u_lo * v_lo;
		const uint64_t p1 = u_lo * v_hi;
		const uint64_t p2 = u_hi * v_lo;
		const uint64_t p3 = u_hi * v_hi;

		const uint64_t p0_hi = p0 >> 32u;
		const uint64_t p1_lo = p1 & 0xFFFFFFFFu;
		const uint64_t p1_hi = p1 >> 32u;
		const uint64_t p2_lo = p2 & 0xFFFFFFFFu;
		const uint64_t p2_hi = p2 >> 32u;

		uint64_t Q = p0_hi + p1_lo + p2_lo;

		// The full product might now be computed as
		//
		// p_hi = p3 + p2_hi + p1_hi + (Q >> 32)
		// p_lo = p0_lo + (Q << 32)
		//
		// But in this particular case here, the full p_lo is not required.
		// Effectively we only need to add the highest bit in p_lo to p_hi (and
		// Q_hi + 1 does not overflow).

		Q += uint64_t{ 1 } << (64u - 32u - 1u); // round, ties up

		const uint64_t h = p3 + p2_hi + p1_hi + (Q >> 32u);

		return { h, x.e + y.e + 64 };
	}

	/*!
	@brief normalize x such that the significand is >= 2^(q-1)
	@pre x.f != 0
	*/
	static diyfp normalize(diyfp x) noexcept {

		while ((x.f >> 63u) == 0) {
			x.f <<= 1u;
			x.e--;
		}

		return x;
	}

	/*!
	@brief normalize x such that the result has the exponent E
	@pre e >= x.e and the upper e - x.e bits of x.f must be zero.
	*/
	static diyfp normalize_to(const diyfp& x,
		const int target_exponent) noexcept {
		const int delta = x.e - target_exponent;

		return { x.f << delta, target_exponent };
	}
};

struct boundaries {
	diyfp w;
	diyfp minus;
	diyfp plus;
};

/*!
Compute the (normalized) diyfp representing the input number 'value' and its
boundaries.
@pre value must be finite and positive
*/
template <typename FloatType> boundaries compute_boundaries(FloatType value) {

	// Convert the IEEE representation into a diyfp.
	//
	// If v is denormal:
	//      value = 0.F * 2^(1 - bias) = (          F) * 2^(1 - bias - (p-1))
	// If v is normalized:
	//      value = 1.F * 2^(E - bias) = (2^(p-1) + F) * 2^(E - bias - (p-1))

	static_assert(std::numeric_limits<FloatType>::is_iec559,
		"atod error: dtoa_short requires an IEEE-754 "
		"floating-point implementation");

	constexpr int kPrecision =
		std::numeric_limits<FloatType>::digits; // = p (includes the hidden bit)
	constexpr int kBias =
		std::numeric_limits<FloatType>::max_exponent - 1 + (kPrecision - 1);
	constexpr int kMinExp = 1 - kBias;
	constexpr uint64_t kHiddenBit = uint64_t(1) << (kPrecision - 1); // = 2^(p-1)

	using bits_type = typename std::conditional<kPrecision == 24, std::uint32_t,
		uint64_t>::type;

	const uint64_t bits = reinterpret_bits<bits_type>(value);
	const uint64_t E = bits >> (kPrecision - 1);
	const uint64_t F = bits & (kHiddenBit - 1);

	const bool is_denormal = E == 0;
	const diyfp v = is_denormal
		? diyfp(F, kMinExp)
		: diyfp(F + kHiddenBit, static_cast<int>(E) - kBias);

	// Compute the boundaries m- and m+ of the floating-point value
	// v = f * 2^e.
	//
	// Determine v- and v+, the floating-point predecessor and successor if v,
	// respectively.
	//
	//      v- = v - 2^e        if f != 2^(p-1) or e == e_min                (A)
	//         = v - 2^(e-1)    if f == 2^(p-1) and e > e_min                (B)
	//
	//      v+ = v + 2^e
	//
	// Let m- = (v- + v) / 2 and m+ = (v + v+) / 2. All real numbers _strictly_
	// between m- and m+ round to v, regardless of how the input rounding
	// algorithm breaks ties.
	//
	//      ---+-------------+-------------+-------------+-------------+---  (A)
	//         v-            m-            v             m+            v+
	//
	//      -----------------+------+------+-------------+-------------+---  (B)
	//                       v-     m-     v             m+            v+

	const bool lower_boundary_is_closer = F == 0 && E > 1;
	const diyfp m_plus = diyfp(2 * v.f + 1, v.e - 1);
	const diyfp m_minus = lower_boundary_is_closer
		? diyfp(4 * v.f - 1, v.e - 2)  // (B)
		: diyfp(2 * v.f - 1, v.e - 1); // (A)

// Determine the normalized w+ = m+.
	const diyfp w_plus = diyfp::normalize(m_plus);

	// Determine w- = m- such that e_(w-) = e_(w+).
	const diyfp w_minus = diyfp::normalize_to(m_minus, w_plus.e);

	return { diyfp::normalize(v), w_minus, w_plus };
}

// Given normalized diyfp w, Grisu needs to find a (normalized) cached
// power-of-ten c, such that the exponent of the product c * w = f * 2^e lies
// within a certain range [alpha, gamma] (Definition 3.2 from [1])
//
//      alpha <= e = e_c + e_w + q <= gamma
//
// or
//
//      f_c * f_w * 2^alpha <= f_c 2^(e_c) * f_w 2^(e_w) * 2^q
//                          <= f_c * f_w * 2^gamma
//
// Since c and w are normalized, i.e. 2^(q-1) <= f < 2^q, this implies
//
//      2^(q-1) * 2^(q-1) * 2^alpha <= c * w * 2^q < 2^q * 2^q * 2^gamma
//
// or
//
//      2^(q - 2 + alpha) <= c * w < 2^(q + gamma)
//
// The choice of (alpha,gamma) determines the size of the table and the form of
// the digit generation procedure. Using (alpha,gamma)=(-60,-32) works out well
// in practice:
//
// The idea is to cut the number c * w = f * 2^e into two parts, which can be
// processed independently: An integral part p1, and a fractional part p2:
//
//      f * 2^e = ( (f div 2^-e) * 2^-e + (f mod 2^-e) ) * 2^e
//              = (f div 2^-e) + (f mod 2^-e) * 2^e
//              = p1 + p2 * 2^e
//
// The conversion of p1 into decimal form requires a series of divisions and
// modulos by (a power of) 10. These operations are faster for 32-bit than for
// 64-bit integers, so p1 should ideally fit into a 32-bit integer. This can be
// achieved by choosing
//
//      -e >= 32   or   e <= -32 := gamma
//
// In order to convert the fractional part
//
//      p2 * 2^e = p2 / 2^-e = d[-1] / 10^1 + d[-2] / 10^2 + ...
//
// into decimal form, the fraction is repeatedly multiplied by 10 and the digits
// d[-i] are extracted in order:
//
//      (10 * p2) div 2^-e = d[-1]
//      (10 * p2) mod 2^-e = d[-2] / 10^1 + ...
//
// The multiplication by 10 must not overflow. It is sufficient to choose
//
//      10 * p2 < 16 * p2 = 2^4 * p2 <= 2^64.
//
// Since p2 = f mod 2^-e < 2^-e,
//
//      -e <= 60   or   e >= -60 := alpha

constexpr int kAlpha = -60;
constexpr int kGamma = -32;

struct cached_power // c = f * 2^e ~= 10^k
{
	uint64_t f;
	int e;
	int k;
};

/*!
For a normalized diyfp w = f * 2^e, this function returns a (normalized) cached
power-of-ten c = f_c * 2^e_c, such that the exponent of the product w * c
satisfies (Definition 3.2 from [1])
	 alpha <= e_c + e + q <= gamma.
*/
inline cached_power get_cached_power_for_binary_exponent(int e) {
	// Now
	//
	//      alpha <= e_c + e + q <= gamma                                    (1)
	//      ==> f_c * 2^alpha <= c * 2^e * 2^q
	//
	// and since the c's are normalized, 2^(q-1) <= f_c,
	//
	//      ==> 2^(q - 1 + alpha) <= c * 2^(e + q)
	//      ==> 2^(alpha - e - 1) <= c
	//
	// If c were an exact power of ten, i.e. c = 10^k, one may determine k as
	//
	//      k = ceil( log_10( 2^(alpha - e - 1) ) )
	//        = ceil( (alpha - e - 1) * log_10(2) )
	//
	// From the paper:
	// "In theory the result of the procedure could be wrong since c is rounded,
	//  and the computation itself is approximated [...]. In practice, however,
	//  this simple function is sufficient."
	//
	// For IEEE double precision floating-point numbers converted into
	// normalized diyfp's w = f * 2^e, with q = 64,
	//
	//      e >= -1022      (min IEEE exponent)
	//           -52        (p - 1)
	//           -52        (p - 1, possibly normalize denormal IEEE numbers)
	//           -11        (normalize the diyfp)
	//         = -1137
	//
	// and
	//
	//      e <= +1023      (max IEEE exponent)
	//           -52        (p - 1)
	//           -11        (normalize the diyfp)
	//         = 960
	//
	// This binary exponent range [-1137,960] results in a decimal exponent
	// range [-307,324]. One does not need to store a cached power for each
	// k in this range. For each such k it suffices to find a cached power
	// such that the exponent of the product lies in [alpha,gamma].
	// This implies that the difference of the decimal exponents of adjacent
	// table entries must be less than or equal to
	//
	//      floor( (gamma - alpha) * log_10(2) ) = 8.
	//
	// (A smaller distance gamma-alpha would require a larger table.)

	// NB:
	// Actually this function returns c, such that -60 <= e_c + e + 64 <= -34.

	constexpr int kCachedPowersMinDecExp = -300;
	constexpr int kCachedPowersDecStep = 8;

	static constexpr cached_power kCachedPowers[] = {
		{0xAB70FE17C79AC6CA, -1060, -300}, {0xFF77B1FCBEBCDC4F, -1034, -292},
		{0xBE5691EF416BD60C, -1007, -284}, {0x8DD01FAD907FFC3C, -980, -276},
		{0xD3515C2831559A83, -954, -268},  {0x9D71AC8FADA6C9B5, -927, -260},
		{0xEA9C227723EE8BCB, -901, -252},  {0xAECC49914078536D, -874, -244},
		{0x823C12795DB6CE57, -847, -236},  {0xC21094364DFB5637, -821, -228},
		{0x9096EA6F3848984F, -794, -220},  {0xD77485CB25823AC7, -768, -212},
		{0xA086CFCD97BF97F4, -741, -204},  {0xEF340A98172AACE5, -715, -196},
		{0xB23867FB2A35B28E, -688, -188},  {0x84C8D4DFD2C63F3B, -661, -180},
		{0xC5DD44271AD3CDBA, -635, -172},  {0x936B9FCEBB25C996, -608, -164},
		{0xDBAC6C247D62A584, -582, -156},  {0xA3AB66580D5FDAF6, -555, -148},
		{0xF3E2F893DEC3F126, -529, -140},  {0xB5B5ADA8AAFF80B8, -502, -132},
		{0x87625F056C7C4A8B, -475, -124},  {0xC9BCFF6034C13053, -449, -116},
		{0x964E858C91BA2655, -422, -108},  {0xDFF9772470297EBD, -396, -100},
		{0xA6DFBD9FB8E5B88F, -369, -92},   {0xF8A95FCF88747D94, -343, -84},
		{0xB94470938FA89BCF, -316, -76},   {0x8A08F0F8BF0F156B, -289, -68},
		{0xCDB02555653131B6, -263, -60},   {0x993FE2C6D07B7FAC, -236, -52},
		{0xE45C10C42A2B3B06, -210, -44},   {0xAA242499697392D3, -183, -36},
		{0xFD87B5F28300CA0E, -157, -28},   {0xBCE5086492111AEB, -130, -20},
		{0x8CBCCC096F5088CC, -103, -12},   {0xD1B71758E219652C, -77, -4},
		{0x9C40000000000000, -50, 4},      {0xE8D4A51000000000, -24, 12},
		{0xAD78EBC5AC620000, 3, 20},       {0x813F3978F8940984, 30, 28},
		{0xC097CE7BC90715B3, 56, 36},      {0x8F7E32CE7BEA5C70, 83, 44},
		{0xD5D238A4ABE98068, 109, 52},     {0x9F4F2726179A2245, 136, 60},
		{0xED63A231D4C4FB27, 162, 68},     {0xB0DE65388CC8ADA8, 189, 76},
		{0x83C7088E1AAB65DB, 216, 84},     {0xC45D1DF942711D9A, 242, 92},
		{0x924D692CA61BE758, 269, 100},    {0xDA01EE641A708DEA, 295, 108},
		{0xA26DA3999AEF774A, 322, 116},    {0xF209787BB47D6B85, 348, 124},
		{0xB454E4A179DD1877, 375, 132},    {0x865B86925B9BC5C2, 402, 140},
		{0xC83553C5C8965D3D, 428, 148},    {0x952AB45CFA97A0B3, 455, 156},
		{0xDE469FBD99A05FE3, 481, 164},    {0xA59BC234DB398C25, 508, 172},
		{0xF6C69A72A3989F5C, 534, 180},    {0xB7DCBF5354E9BECE, 561, 188},
		{0x88FCF317F22241E2, 588, 196},    {0xCC20CE9BD35C78A5, 614, 204},
		{0x98165AF37B2153DF, 641, 212},    {0xE2A0B5DC971F303A, 667, 220},
		{0xA8D9D1535CE3B396, 694, 228},    {0xFB9B7CD9A4A7443C, 720, 236},
		{0xBB764C4CA7A44410, 747, 244},    {0x8BAB8EEFB6409C1A, 774, 252},
		{0xD01FEF10A657842C, 800, 260},    {0x9B10A4E5E9913129, 827, 268},
		{0xE7109BFBA19C0C9D, 853, 276},    {0xAC2820D9623BF429, 880, 284},
		{0x80444B5E7AA7CF85, 907, 292},    {0xBF21E44003ACDD2D, 933, 300},
		{0x8E679C2F5E44FF8F, 960, 308},    {0xD433179D9C8CB841, 986, 316},
		{0x9E19DB92B4E31BA9, 1013, 324},
	};

	// This computation gives exactly the same results for k as
	//      k = ceil((kAlpha - e - 1) * 0.30102999566398114)
	// for |e| <= 1500, but doesn't require floating-point operations.
	// NB: log_10(2) ~= 78913 / 2^18
	const int f = kAlpha - e - 1;
	const int k = (f * 78913) / (1 << 18) + static_cast<int>(f > 0);

	const int index = (-kCachedPowersMinDecExp + k + (kCachedPowersDecStep - 1)) /
		kCachedPowersDecStep;

	const cached_power cached = kCachedPowers[static_cast<std::size_t>(index)];

	return cached;
}

/*!
For n != 0, returns k, such that pow10 := 10^(k-1) <= n < 10^k.
For n == 0, returns 1 and sets pow10 := 1.
*/
inline int find_largest_pow10(const std::uint32_t n, std::uint32_t& pow10) {
	// LCOV_EXCL_START
	if (n >= 1000000000) {
		pow10 = 1000000000;
		return 10;
	}
	// LCOV_EXCL_STOP
	else if (n >= 100000000) {
		pow10 = 100000000;
		return 9;
	}
	else if (n >= 10000000) {
		pow10 = 10000000;
		return 8;
	}
	else if (n >= 1000000) {
		pow10 = 1000000;
		return 7;
	}
	else if (n >= 100000) {
		pow10 = 100000;
		return 6;
	}
	else if (n >= 10000) {
		pow10 = 10000;
		return 5;
	}
	else if (n >= 1000) {
		pow10 = 1000;
		return 4;
	}
	else if (n >= 100) {
		pow10 = 100;
		return 3;
	}
	else if (n >= 10) {
		pow10 = 10;
		return 2;
	}
	else {
		pow10 = 1;
		return 1;
	}
}

inline void grisu2_round(char* buf, int len, uint64_t dist,
	uint64_t delta, uint64_t rest,
	uint64_t ten_k) {

	//               <--------------------------- delta ---->
	//                                  <---- dist --------->
	// --------------[------------------+-------------------]--------------
	//               M-                 w                   M+
	//
	//                                  ten_k
	//                                <------>
	//                                       <---- rest ---->
	// --------------[------------------+----+--------------]--------------
	//                                  w    V
	//                                       = buf * 10^k
	//
	// ten_k represents a unit-in-the-last-place in the decimal representation
	// stored in buf.
	// Decrement buf by ten_k while this takes buf closer to w.

	// The tests are written in this order to avoid overflow in unsigned
	// integer arithmetic.

	while (rest < dist && delta - rest >= ten_k &&
		(rest + ten_k < dist || dist - rest > rest + ten_k - dist)) {
		buf[len - 1]--;
		rest += ten_k;
	}
}

/*!
Generates V = buffer * 10^decimal_exponent, such that M- <= V <= M+.
M- and M+ must be normalized and share the same exponent -60 <= e <= -32.
*/
inline void grisu2_digit_gen(char* buffer, int& length, int& decimal_exponent,
	diyfp M_minus, diyfp w, diyfp M_plus) {
	static_assert(kAlpha >= -60, "atod error");
	static_assert(kGamma <= -32, "atod error");

	// Generates the digits (and the exponent) of a decimal floating-point
	// number V = buffer * 10^decimal_exponent in the range [M-, M+]. The diyfp's
	// w, M- and M+ share the same exponent e, which satisfies alpha <= e <=
	// gamma.
	//
	//               <--------------------------- delta ---->
	//                                  <---- dist --------->
	// --------------[------------------+-------------------]--------------
	//               M-                 w                   M+
	//
	// Grisu2 generates the digits of M+ from left to right and stops as soon as
	// V is in [M-,M+].

	uint64_t delta =
		diyfp::sub(M_plus, M_minus)
		.f; // (significand of (M+ - M-), implicit exponent is e)
	uint64_t dist =
		diyfp::sub(M_plus, w)
		.f; // (significand of (M+ - w ), implicit exponent is e)

// Split M+ = f * 2^e into two parts p1 and p2 (note: e < 0):
//
//      M+ = f * 2^e
//         = ((f div 2^-e) * 2^-e + (f mod 2^-e)) * 2^e
//         = ((p1        ) * 2^-e + (p2        )) * 2^e
//         = p1 + p2 * 2^e

	const diyfp one(uint64_t{ 1 } << -M_plus.e, M_plus.e);

	auto p1 = static_cast<std::uint32_t>(
		M_plus.f >>
		-one.e); // p1 = f div 2^-e (Since -e >= 32, p1 fits into a 32-bit int.)
	uint64_t p2 = M_plus.f & (one.f - 1); // p2 = f mod 2^-e

	// 1)
	//
	// Generate the digits of the integral part p1 = d[n-1]...d[1]d[0]

	std::uint32_t pow10;
	const int k = find_largest_pow10(p1, pow10);

	//      10^(k-1) <= p1 < 10^k, pow10 = 10^(k-1)
	//
	//      p1 = (p1 div 10^(k-1)) * 10^(k-1) + (p1 mod 10^(k-1))
	//         = (d[k-1]         ) * 10^(k-1) + (p1 mod 10^(k-1))
	//
	//      M+ = p1                                             + p2 * 2^e
	//         = d[k-1] * 10^(k-1) + (p1 mod 10^(k-1))          + p2 * 2^e
	//         = d[k-1] * 10^(k-1) + ((p1 mod 10^(k-1)) * 2^-e + p2) * 2^e
	//         = d[k-1] * 10^(k-1) + (                         rest) * 2^e
	//
	// Now generate the digits d[n] of p1 from left to right (n = k-1,...,0)
	//
	//      p1 = d[k-1]...d[n] * 10^n + d[n-1]...d[0]
	//
	// but stop as soon as
	//
	//      rest * 2^e = (d[n-1]...d[0] * 2^-e + p2) * 2^e <= delta * 2^e

	int n = k;
	while (n > 0) {
		// Invariants:
		//      M+ = buffer * 10^n + (p1 + p2 * 2^e)    (buffer = 0 for n = k)
		//      pow10 = 10^(n-1) <= p1 < 10^n
		//
		const std::uint32_t d = p1 / pow10; // d = p1 div 10^(n-1)
		const std::uint32_t r = p1 % pow10; // r = p1 mod 10^(n-1)
		//
		//      M+ = buffer * 10^n + (d * 10^(n-1) + r) + p2 * 2^e
		//         = (buffer * 10 + d) * 10^(n-1) + (r + p2 * 2^e)
		//
		buffer[length++] = static_cast<char>('0' + d); // buffer := buffer * 10 + d
		//
		//      M+ = buffer * 10^(n-1) + (r + p2 * 2^e)
		//
		p1 = r;
		n--;
		//
		//      M+ = buffer * 10^n + (p1 + p2 * 2^e)
		//      pow10 = 10^n
		//

		// Now check if enough digits have been generated.
		// Compute
		//
		//      p1 + p2 * 2^e = (p1 * 2^-e + p2) * 2^e = rest * 2^e
		//
		// Note:
		// Since rest and delta share the same exponent e, it suffices to
		// compare the significands.
		const uint64_t rest = (uint64_t{ p1 } << -one.e) + p2;
		if (rest <= delta) {
			// V = buffer * 10^n, with M- <= V <= M+.

			decimal_exponent += n;

			// We may now just stop. But instead look if the buffer could be
			// decremented to bring V closer to w.
			//
			// pow10 = 10^n is now 1 ulp in the decimal representation V.
			// The rounding procedure works with diyfp's with an implicit
			// exponent of e.
			//
			//      10^n = (10^n * 2^-e) * 2^e = ulp * 2^e
			//
			const uint64_t ten_n = uint64_t{ pow10 } << -one.e;
			grisu2_round(buffer, length, dist, delta, rest, ten_n);

			return;
		}

		pow10 /= 10;
		//
		//      pow10 = 10^(n-1) <= p1 < 10^n
		// Invariants restored.
	}

	// 2)
	//
	// The digits of the integral part have been generated:
	//
	//      M+ = d[k-1]...d[1]d[0] + p2 * 2^e
	//         = buffer            + p2 * 2^e
	//
	// Now generate the digits of the fractional part p2 * 2^e.
	//
	// Note:
	// No decimal point is generated: the exponent is adjusted instead.
	//
	// p2 actually represents the fraction
	//
	//      p2 * 2^e
	//          = p2 / 2^-e
	//          = d[-1] / 10^1 + d[-2] / 10^2 + ...
	//
	// Now generate the digits d[-m] of p1 from left to right (m = 1,2,...)
	//
	//      p2 * 2^e = d[-1]d[-2]...d[-m] * 10^-m
	//                      + 10^-m * (d[-m-1] / 10^1 + d[-m-2] / 10^2 + ...)
	//
	// using
	//
	//      10^m * p2 = ((10^m * p2) div 2^-e) * 2^-e + ((10^m * p2) mod 2^-e)
	//                = (                   d) * 2^-e + (                   r)
	//
	// or
	//      10^m * p2 * 2^e = d + r * 2^e
	//
	// i.e.
	//
	//      M+ = buffer + p2 * 2^e
	//         = buffer + 10^-m * (d + r * 2^e)
	//         = (buffer * 10^m + d) * 10^-m + 10^-m * r * 2^e
	//
	// and stop as soon as 10^-m * r * 2^e <= delta * 2^e

	int m = 0;
	for (;;) {
		// Invariant:
		//      M+ = buffer * 10^-m + 10^-m * (d[-m-1] / 10 + d[-m-2] / 10^2 + ...)
		//      * 2^e
		//         = buffer * 10^-m + 10^-m * (p2                                 )
		//         * 2^e = buffer * 10^-m + 10^-m * (1/10 * (10 * p2) ) * 2^e =
		//         buffer * 10^-m + 10^-m * (1/10 * ((10*p2 div 2^-e) * 2^-e +
		//         (10*p2 mod 2^-e)) * 2^e
		//
		p2 *= 10;
		const uint64_t d = p2 >> -one.e;     // d = (10 * p2) div 2^-e
		const uint64_t r = p2 & (one.f - 1); // r = (10 * p2) mod 2^-e
		//
		//      M+ = buffer * 10^-m + 10^-m * (1/10 * (d * 2^-e + r) * 2^e
		//         = buffer * 10^-m + 10^-m * (1/10 * (d + r * 2^e))
		//         = (buffer * 10 + d) * 10^(-m-1) + 10^(-m-1) * r * 2^e
		//
		buffer[length++] = static_cast<char>('0' + d); // buffer := buffer * 10 + d
		//
		//      M+ = buffer * 10^(-m-1) + 10^(-m-1) * r * 2^e
		//
		p2 = r;
		m++;
		//
		//      M+ = buffer * 10^-m + 10^-m * p2 * 2^e
		// Invariant restored.

		// Check if enough digits have been generated.
		//
		//      10^-m * p2 * 2^e <= delta * 2^e
		//              p2 * 2^e <= 10^m * delta * 2^e
		//                    p2 <= 10^m * delta
		delta *= 10;
		dist *= 10;
		if (p2 <= delta) {
			break;
		}
	}

	// V = buffer * 10^-m, with M- <= V <= M+.

	decimal_exponent -= m;

	// 1 ulp in the decimal representation is now 10^-m.
	// Since delta and dist are now scaled by 10^m, we need to do the
	// same with ulp in order to keep the units in sync.
	//
	//      10^m * 10^-m = 1 = 2^-e * 2^e = ten_m * 2^e
	//
	const uint64_t ten_m = one.f;
	grisu2_round(buffer, length, dist, delta, p2, ten_m);

	// By construction this algorithm generates the shortest possible decimal
	// number (Loitsch, Theorem 6.2) which rounds back to w.
	// For an input number of precision p, at least
	//
	//      N = 1 + ceil(p * log_10(2))
	//
	// decimal digits are sufficient to identify all binary floating-point
	// numbers (Matula, "In-and-Out conversions").
	// This implies that the algorithm does not produce more than N decimal
	// digits.
	//
	//      N = 17 for p = 53 (IEEE double precision)
	//      N = 9  for p = 24 (IEEE single precision)
}

/*!
v = buf * 10^decimal_exponent
len is the length of the buffer (number of decimal digits)
The buffer must be large enough, i.e. >= max_digits10.
*/
inline void grisu2(char* buf, int& len, int& decimal_exponent, diyfp m_minus,
	diyfp v, diyfp m_plus) {

	//  --------(-----------------------+-----------------------)--------    (A)
	//          m-                      v                       m+
	//
	//  --------------------(-----------+-----------------------)--------    (B)
	//                      m-          v                       m+
	//
	// First scale v (and m- and m+) such that the exponent is in the range
	// [alpha, gamma].

	const cached_power cached = get_cached_power_for_binary_exponent(m_plus.e);

	const diyfp c_minus_k(cached.f, cached.e); // = c ~= 10^-k

	// The exponent of the products is = v.e + c_minus_k.e + q and is in the range
	// [alpha,gamma]
	const diyfp w = diyfp::mul(v, c_minus_k);
	const diyfp w_minus = diyfp::mul(m_minus, c_minus_k);
	const diyfp w_plus = diyfp::mul(m_plus, c_minus_k);

	//  ----(---+---)---------------(---+---)---------------(---+---)----
	//          w-                      w                       w+
	//          = c*m-                  = c*v                   = c*m+
	//
	// diyfp::mul rounds its result and c_minus_k is approximated too. w, w- and
	// w+ are now off by a small amount.
	// In fact:
	//
	//      w - v * 10^k < 1 ulp
	//
	// To account for this inaccuracy, add resp. subtract 1 ulp.
	//
	//  --------+---[---------------(---+---)---------------]---+--------
	//          w-  M-                  w                   M+  w+
	//
	// Now any number in [M-, M+] (bounds included) will round to w when input,
	// regardless of how the input rounding algorithm breaks ties.
	//
	// And digit_gen generates the shortest possible such number in [M-, M+].
	// Note that this does not mean that Grisu2 always generates the shortest
	// possible number in the interval (m-, m+).
	const diyfp M_minus(w_minus.f + 1, w_minus.e);
	const diyfp M_plus(w_plus.f - 1, w_plus.e);

	decimal_exponent = -cached.k; // = -(-k) = k

	grisu2_digit_gen(buf, len, decimal_exponent, M_minus, w, M_plus);
}

/*!
v = buf * 10^decimal_exponent
len is the length of the buffer (number of decimal digits)
The buffer must be large enough, i.e. >= max_digits10.
*/
template <typename FloatType>
void grisu2(char* buf, int& len, int& decimal_exponent, FloatType value) {
	static_assert(diyfp::kPrecision >= std::numeric_limits<FloatType>::digits + 3,
		"atod error: not enough precision");

	// If the neighbors (and boundaries) of 'value' are always computed for
	// double-precision numbers, all float's can be recovered using strtod (and
	// strtof). However, the resulting decimal representations are not exactly
	// "short".
	//
	// The documentation for 'std::to_chars'
	// (https://en.cppreference.com/w/cpp/utility/to_chars) says "value is
	// converted to a string as if by std::sprintf in the default ("C") locale"
	// and since sprintf promotes float's to double's, I think this is exactly
	// what 'std::to_chars' does. On the other hand, the documentation for
	// 'std::to_chars' requires that "parsing the representation using the
	// corresponding std::from_chars function recovers value exactly". That
	// indicates that single precision floating-point numbers should be recovered
	// using 'std::strtof'.
	//
	// NB: If the neighbors are computed for single-precision numbers, there is a
	// single float
	//     (7.0385307e-26f) which can't be recovered using strtod. The resulting
	//     double precision value is off by 1 ulp.
#if 0
	const boundaries w = compute_boundaries(static_cast<double>(value));
#else
	const boundaries w = compute_boundaries(value);
#endif

	grisu2(buf, len, decimal_exponent, w.minus, w.w, w.plus);
}

/*!
@brief appends a decimal representation of e to buf
@return a pointer to the element following the exponent.
@pre -1000 < e < 1000
*/
inline char* append_exponent(char* buf, int e) {

	if (e < 0) {
		e = -e;
		*buf++ = '-';
	}
	else {
		*buf++ = '+';
	}

	auto k = static_cast<std::uint32_t>(e);
	if (k < 10) {
		// Always print at least two digits in the exponent.
		// This is for compatibility with printf("%g").
		*buf++ = '0';
		*buf++ = static_cast<char>('0' + k);
	}
	else if (k < 100) {
		*buf++ = static_cast<char>('0' + k / 10);
		k %= 10;
		*buf++ = static_cast<char>('0' + k);
	}
	else {
		*buf++ = static_cast<char>('0' + k / 100);
		k %= 100;
		*buf++ = static_cast<char>('0' + k / 10);
		k %= 10;
		*buf++ = static_cast<char>('0' + k);
	}

	return buf;
}

/*!
@brief prettify v = buf * 10^decimal_exponent
If v is in the range [10^min_exp, 10^max_exp) it will be printed in fixed-point
notation. Otherwise it will be printed in exponential notation.
@pre min_exp < 0
@pre max_exp > 0
*/
inline char* format_buffer(char* buf, int len, int decimal_exponent,
	int min_exp, int max_exp) {

	const int k = len;
	const int n = len + decimal_exponent;

	// v = buf * 10^(n-k)
	// k is the length of the buffer (number of decimal digits)
	// n is the position of the decimal point relative to the start of the buffer.

	if (k <= n && n <= max_exp) {
		// digits[000]
		// len <= max_exp + 2

		std::memset(buf + k, '0', static_cast<size_t>(n) - static_cast<size_t>(k));
		// Make it look like a floating-point number (#362, #378)
		// buf[n + 0] = '.';
		// buf[n + 1] = '0';
		return buf + (static_cast<size_t>(n));
	}

	if (0 < n && n <= max_exp) {
		// dig.its
		// len <= max_digits10 + 1
		std::memmove(buf + (static_cast<size_t>(n) + 1), buf + n,
			static_cast<size_t>(k) - static_cast<size_t>(n));
		buf[n] = '.';
		return buf + (static_cast<size_t>(k) + 1U);
	}

	if (min_exp < n && n <= 0) {
		// 0.[000]digits
		// len <= 2 + (-min_exp - 1) + max_digits10

		std::memmove(buf + (2 + static_cast<size_t>(-n)), buf,
			static_cast<size_t>(k));
		buf[0] = '0';
		buf[1] = '.';
		std::memset(buf + 2, '0', static_cast<size_t>(-n));
		return buf + (2U + static_cast<size_t>(-n) + static_cast<size_t>(k));
	}

	if (k == 1) {
		// dE+123
		// len <= 1 + 5

		buf += 1;
	}
	else {
		// d.igitsE+123
		// len <= max_digits10 + 1 + 5

		std::memmove(buf + 2, buf + 1, static_cast<size_t>(k) - 1);
		buf[1] = '.';
		buf += 1 + static_cast<size_t>(k);
	}

	*buf++ = 'e';
	return append_exponent(buf, n - 1);
}

} // namespace dtoa
namespace atod {

/**
 * The code in the atod::from_chars function is meant to handle the floating-point number parsing
 * when we have more than 19 digits in the decimal mantissa. This should only be seen
 * in adversarial scenarios: we do not expect production systems to even produce
 * such floating-point numbers.
 *
 * The parser is based on work by Nigel Tao (at https://github.com/google/wuffs/)
 * who credits Ken Thompson for the design (via a reference to the Go source
 * code). See
 * https://github.com/google/wuffs/blob/aa46859ea40c72516deffa1b146121952d6dfd3b/atod/cgen/base/floatconv-submodule-data.c
 * https://github.com/google/wuffs/blob/46cd8105f47ca07ae2ba8e6a7818ef9c0df6c152/atod/cgen/base/floatconv-submodule-code.c
 * It is probably not very fast but it is a fallback that should almost never be
 * called in real life. Google Wuffs is published under APL 2.0.
 **/

constexpr uint32_t MAX_DIGITS = 768;
constexpr int32_t DECIMAL_POINT_RANGE = 2047;
constexpr int MANTISSA_EXPLICIT_BITS = 52;
constexpr int MINIMUM_EXPONENT = -1023;
constexpr int INFINITE_POWER = 0x7FF;
constexpr int SIGN_INDEX = 63;

struct AdjustedMantissa {
	uint64_t mantissa;
	int power2;
	AdjustedMantissa() : mantissa(0), power2(0) {}
};

struct Decimal {
	uint32_t num_digits;
	int32_t decimal_point;
	bool negative;
	bool truncated;
	uint8_t digits[MAX_DIGITS];
};

bool is_integer(char c)noexcept { return (c >= '0' && c <= '9'); }

// This should always succeed since it follows a call to parse_number.
Decimal parse_decimal(const char*& p) noexcept {
	Decimal answer;
	answer.num_digits = 0;
	answer.decimal_point = 0;
	answer.truncated = false;
	answer.negative = (*p == '-');
	if ((*p == '-') || (*p == '+')) {
		++p;
	}

	while (*p == '0') {
		++p;
	}
	while (is_integer(*p)) {
		if (answer.num_digits < MAX_DIGITS) {
			answer.digits[answer.num_digits] = uint8_t(*p - '0');
		}
		answer.num_digits++;
		++p;
	}
	if (*p == '.') {
		++p;
		const char* first_after_period = p;
		// if we have not yet encountered a zero, we have to skip it as well
		if (answer.num_digits == 0) {
			// skip zeros
			while (*p == '0') {
				++p;
			}
		}
		while (is_integer(*p)) {
			if (answer.num_digits < MAX_DIGITS) {
				answer.digits[answer.num_digits] = uint8_t(*p - '0');
			}
			answer.num_digits++;
			++p;
		}
		answer.decimal_point = int32_t(first_after_period - p);
	}
	if (answer.num_digits > 0) {
		const char* preverse = p - 1;
		int32_t trailing_zeros = 0;
		while ((*preverse == '0') || (*preverse == '.')) {
			if (*preverse == '0') { trailing_zeros++; };
			--preverse;
		}
		answer.decimal_point += int32_t(answer.num_digits);
		answer.num_digits -= uint32_t(trailing_zeros);
	}
	if (answer.num_digits > MAX_DIGITS) {
		answer.num_digits = MAX_DIGITS;
		answer.truncated = true;
	}
	if (('e' == *p) || ('E' == *p)) {
		++p;
		bool neg_exp = false;
		if ('-' == *p) {
			neg_exp = true;
			++p;
		}
		else if ('+' == *p) {
			++p;
		}
		int32_t exp_number = 0; // exponential part
		while (is_integer(*p)) {
			uint8_t digit = uint8_t(*p - '0');
			if (exp_number < 0x10000) {
				exp_number = 10 * exp_number + digit;
			}
			++p;
		}
		answer.decimal_point += (neg_exp ? -exp_number : exp_number);
	}
	return answer;
}

// remove all final zeroes
inline void trim(Decimal& h) {
	while ((h.num_digits > 0) && (h.digits[h.num_digits - 1] == 0)) {
		h.num_digits--;
	}
}

uint32_t number_of_digits_decimal_left_shift(Decimal& h, uint32_t shift) {
	shift &= 63;
	const static uint16_t number_of_digits_decimal_left_shift_table[65] = {
		0x0000, 0x0800, 0x0801, 0x0803, 0x1006, 0x1009, 0x100D, 0x1812, 0x1817,
		0x181D, 0x2024, 0x202B, 0x2033, 0x203C, 0x2846, 0x2850, 0x285B, 0x3067,
		0x3073, 0x3080, 0x388E, 0x389C, 0x38AB, 0x38BB, 0x40CC, 0x40DD, 0x40EF,
		0x4902, 0x4915, 0x4929, 0x513E, 0x5153, 0x5169, 0x5180, 0x5998, 0x59B0,
		0x59C9, 0x61E3, 0x61FD, 0x6218, 0x6A34, 0x6A50, 0x6A6D, 0x6A8B, 0x72AA,
		0x72C9, 0x72E9, 0x7B0A, 0x7B2B, 0x7B4D, 0x8370, 0x8393, 0x83B7, 0x83DC,
		0x8C02, 0x8C28, 0x8C4F, 0x9477, 0x949F, 0x94C8, 0x9CF2, 0x051C, 0x051C,
		0x051C, 0x051C,
	};
	uint32_t x_a = number_of_digits_decimal_left_shift_table[shift];
	uint32_t x_b = number_of_digits_decimal_left_shift_table[shift + 1];
	uint32_t num_new_digits = x_a >> 11;
	uint32_t pow5_a = 0x7FF & x_a;
	uint32_t pow5_b = 0x7FF & x_b;
	const static uint8_t
		number_of_digits_decimal_left_shift_table_powers_of_5[0x051C] = {
			5, 2, 5, 1, 2, 5, 6, 2, 5, 3, 1, 2, 5, 1, 5, 6, 2, 5, 7, 8, 1, 2, 5,
			3, 9, 0, 6, 2, 5, 1, 9, 5, 3, 1, 2, 5, 9, 7, 6, 5, 6, 2, 5, 4, 8, 8,
			2, 8, 1, 2, 5, 2, 4, 4, 1, 4, 0, 6, 2, 5, 1, 2, 2, 0, 7, 0, 3, 1, 2,
			5, 6, 1, 0, 3, 5, 1, 5, 6, 2, 5, 3, 0, 5, 1, 7, 5, 7, 8, 1, 2, 5, 1,
			5, 2, 5, 8, 7, 8, 9, 0, 6, 2, 5, 7, 6, 2, 9, 3, 9, 4, 5, 3, 1, 2, 5,
			3, 8, 1, 4, 6, 9, 7, 2, 6, 5, 6, 2, 5, 1, 9, 0, 7, 3, 4, 8, 6, 3, 2,
			8, 1, 2, 5, 9, 5, 3, 6, 7, 4, 3, 1, 6, 4, 0, 6, 2, 5, 4, 7, 6, 8, 3,
			7, 1, 5, 8, 2, 0, 3, 1, 2, 5, 2, 3, 8, 4, 1, 8, 5, 7, 9, 1, 0, 1, 5,
			6, 2, 5, 1, 1, 9, 2, 0, 9, 2, 8, 9, 5, 5, 0, 7, 8, 1, 2, 5, 5, 9, 6,
			0, 4, 6, 4, 4, 7, 7, 5, 3, 9, 0, 6, 2, 5, 2, 9, 8, 0, 2, 3, 2, 2, 3,
			8, 7, 6, 9, 5, 3, 1, 2, 5, 1, 4, 9, 0, 1, 1, 6, 1, 1, 9, 3, 8, 4, 7,
			6, 5, 6, 2, 5, 7, 4, 5, 0, 5, 8, 0, 5, 9, 6, 9, 2, 3, 8, 2, 8, 1, 2,
			5, 3, 7, 2, 5, 2, 9, 0, 2, 9, 8, 4, 6, 1, 9, 1, 4, 0, 6, 2, 5, 1, 8,
			6, 2, 6, 4, 5, 1, 4, 9, 2, 3, 0, 9, 5, 7, 0, 3, 1, 2, 5, 9, 3, 1, 3,
			2, 2, 5, 7, 4, 6, 1, 5, 4, 7, 8, 5, 1, 5, 6, 2, 5, 4, 6, 5, 6, 6, 1,
			2, 8, 7, 3, 0, 7, 7, 3, 9, 2, 5, 7, 8, 1, 2, 5, 2, 3, 2, 8, 3, 0, 6,
			4, 3, 6, 5, 3, 8, 6, 9, 6, 2, 8, 9, 0, 6, 2, 5, 1, 1, 6, 4, 1, 5, 3,
			2, 1, 8, 2, 6, 9, 3, 4, 8, 1, 4, 4, 5, 3, 1, 2, 5, 5, 8, 2, 0, 7, 6,
			6, 0, 9, 1, 3, 4, 6, 7, 4, 0, 7, 2, 2, 6, 5, 6, 2, 5, 2, 9, 1, 0, 3,
			8, 3, 0, 4, 5, 6, 7, 3, 3, 7, 0, 3, 6, 1, 3, 2, 8, 1, 2, 5, 1, 4, 5,
			5, 1, 9, 1, 5, 2, 2, 8, 3, 6, 6, 8, 5, 1, 8, 0, 6, 6, 4, 0, 6, 2, 5,
			7, 2, 7, 5, 9, 5, 7, 6, 1, 4, 1, 8, 3, 4, 2, 5, 9, 0, 3, 3, 2, 0, 3,
			1, 2, 5, 3, 6, 3, 7, 9, 7, 8, 8, 0, 7, 0, 9, 1, 7, 1, 2, 9, 5, 1, 6,
			6, 0, 1, 5, 6, 2, 5, 1, 8, 1, 8, 9, 8, 9, 4, 0, 3, 5, 4, 5, 8, 5, 6,
			4, 7, 5, 8, 3, 0, 0, 7, 8, 1, 2, 5, 9, 0, 9, 4, 9, 4, 7, 0, 1, 7, 7,
			2, 9, 2, 8, 2, 3, 7, 9, 1, 5, 0, 3, 9, 0, 6, 2, 5, 4, 5, 4, 7, 4, 7,
			3, 5, 0, 8, 8, 6, 4, 6, 4, 1, 1, 8, 9, 5, 7, 5, 1, 9, 5, 3, 1, 2, 5,
			2, 2, 7, 3, 7, 3, 6, 7, 5, 4, 4, 3, 2, 3, 2, 0, 5, 9, 4, 7, 8, 7, 5,
			9, 7, 6, 5, 6, 2, 5, 1, 1, 3, 6, 8, 6, 8, 3, 7, 7, 2, 1, 6, 1, 6, 0,
			2, 9, 7, 3, 9, 3, 7, 9, 8, 8, 2, 8, 1, 2, 5, 5, 6, 8, 4, 3, 4, 1, 8,
			8, 6, 0, 8, 0, 8, 0, 1, 4, 8, 6, 9, 6, 8, 9, 9, 4, 1, 4, 0, 6, 2, 5,
			2, 8, 4, 2, 1, 7, 0, 9, 4, 3, 0, 4, 0, 4, 0, 0, 7, 4, 3, 4, 8, 4, 4,
			9, 7, 0, 7, 0, 3, 1, 2, 5, 1, 4, 2, 1, 0, 8, 5, 4, 7, 1, 5, 2, 0, 2,
			0, 0, 3, 7, 1, 7, 4, 2, 2, 4, 8, 5, 3, 5, 1, 5, 6, 2, 5, 7, 1, 0, 5,
			4, 2, 7, 3, 5, 7, 6, 0, 1, 0, 0, 1, 8, 5, 8, 7, 1, 1, 2, 4, 2, 6, 7,
			5, 7, 8, 1, 2, 5, 3, 5, 5, 2, 7, 1, 3, 6, 7, 8, 8, 0, 0, 5, 0, 0, 9,
			2, 9, 3, 5, 5, 6, 2, 1, 3, 3, 7, 8, 9, 0, 6, 2, 5, 1, 7, 7, 6, 3, 5,
			6, 8, 3, 9, 4, 0, 0, 2, 5, 0, 4, 6, 4, 6, 7, 7, 8, 1, 0, 6, 6, 8, 9,
			4, 5, 3, 1, 2, 5, 8, 8, 8, 1, 7, 8, 4, 1, 9, 7, 0, 0, 1, 2, 5, 2, 3,
			2, 3, 3, 8, 9, 0, 5, 3, 3, 4, 4, 7, 2, 6, 5, 6, 2, 5, 4, 4, 4, 0, 8,
			9, 2, 0, 9, 8, 5, 0, 0, 6, 2, 6, 1, 6, 1, 6, 9, 4, 5, 2, 6, 6, 7, 2,
			3, 6, 3, 2, 8, 1, 2, 5, 2, 2, 2, 0, 4, 4, 6, 0, 4, 9, 2, 5, 0, 3, 1,
			3, 0, 8, 0, 8, 4, 7, 2, 6, 3, 3, 3, 6, 1, 8, 1, 6, 4, 0, 6, 2, 5, 1,
			1, 1, 0, 2, 2, 3, 0, 2, 4, 6, 2, 5, 1, 5, 6, 5, 4, 0, 4, 2, 3, 6, 3,
			1, 6, 6, 8, 0, 9, 0, 8, 2, 0, 3, 1, 2, 5, 5, 5, 5, 1, 1, 1, 5, 1, 2,
			3, 1, 2, 5, 7, 8, 2, 7, 0, 2, 1, 1, 8, 1, 5, 8, 3, 4, 0, 4, 5, 4, 1,
			0, 1, 5, 6, 2, 5, 2, 7, 7, 5, 5, 5, 7, 5, 6, 1, 5, 6, 2, 8, 9, 1, 3,
			5, 1, 0, 5, 9, 0, 7, 9, 1, 7, 0, 2, 2, 7, 0, 5, 0, 7, 8, 1, 2, 5, 1,
			3, 8, 7, 7, 7, 8, 7, 8, 0, 7, 8, 1, 4, 4, 5, 6, 7, 5, 5, 2, 9, 5, 3,
			9, 5, 8, 5, 1, 1, 3, 5, 2, 5, 3, 9, 0, 6, 2, 5, 6, 9, 3, 8, 8, 9, 3,
			9, 0, 3, 9, 0, 7, 2, 2, 8, 3, 7, 7, 6, 4, 7, 6, 9, 7, 9, 2, 5, 5, 6,
			7, 6, 2, 6, 9, 5, 3, 1, 2, 5, 3, 4, 6, 9, 4, 4, 6, 9, 5, 1, 9, 5, 3,
			6, 1, 4, 1, 8, 8, 8, 2, 3, 8, 4, 8, 9, 6, 2, 7, 8, 3, 8, 1, 3, 4, 7,
			6, 5, 6, 2, 5, 1, 7, 3, 4, 7, 2, 3, 4, 7, 5, 9, 7, 6, 8, 0, 7, 0, 9,
			4, 4, 1, 1, 9, 2, 4, 4, 8, 1, 3, 9, 1, 9, 0, 6, 7, 3, 8, 2, 8, 1, 2,
			5, 8, 6, 7, 3, 6, 1, 7, 3, 7, 9, 8, 8, 4, 0, 3, 5, 4, 7, 2, 0, 5, 9,
			6, 2, 2, 4, 0, 6, 9, 5, 9, 5, 3, 3, 6, 9, 1, 4, 0, 6, 2, 5,
	};
	const uint8_t* pow5 =
		&number_of_digits_decimal_left_shift_table_powers_of_5[pow5_a];
	uint32_t i = 0;
	uint32_t n = pow5_b - pow5_a;
	for (; i < n; i++) {
		if (i >= h.num_digits) {
			return num_new_digits - 1;
		}
		else if (h.digits[i] == pow5[i]) {
			continue;
		}
		else if (h.digits[i] < pow5[i]) {
			return num_new_digits - 1;
		}
		else {
			return num_new_digits;
		}
	}
	return num_new_digits;
}

uint64_t round(Decimal& h) {
	if ((h.num_digits == 0) || (h.decimal_point < 0)) {
		return 0;
	}
	else if (h.decimal_point > 18) {
		return UINT64_MAX;
	}
	// at this point, we know that h.decimal_point >= 0
	uint32_t dp = uint32_t(h.decimal_point);
	uint64_t n = 0;
	for (uint32_t i = 0; i < dp; i++) {
		n = (10 * n) + ((i < h.num_digits) ? h.digits[i] : 0);
	}
	bool round_up = false;
	if (dp < h.num_digits) {
		round_up = h.digits[dp] >= 5; // normally, we round up
		// but we may need to round to even!
		if ((h.digits[dp] == 5) && (dp + 1 == h.num_digits)) {
			round_up = h.truncated || ((dp > 0) && (1 & h.digits[dp - 1]));
		}
	}
	if (round_up) {
		n++;
	}
	return n;
}

// computes h * 2^-shift
void decimal_left_shift(Decimal& h, uint32_t shift) {
	if (h.num_digits == 0) {
		return;
	}
	uint32_t num_new_digits = number_of_digits_decimal_left_shift(h, shift);
	int32_t read_index = int32_t(h.num_digits - 1);
	uint32_t write_index = h.num_digits - 1 + num_new_digits;
	uint64_t n = 0;

	while (read_index >= 0) {
		n += uint64_t(h.digits[read_index]) << shift;
		uint64_t quotient = n / 10;
		uint64_t remainder = n - (10 * quotient);
		if (write_index < MAX_DIGITS) {
			h.digits[write_index] = uint8_t(remainder);
		}
		else if (remainder > 0) {
			h.truncated = true;
		}
		n = quotient;
		write_index--;
		read_index--;
	}
	while (n > 0) {
		uint64_t quotient = n / 10;
		uint64_t remainder = n - (10 * quotient);
		if (write_index < MAX_DIGITS) {
			h.digits[write_index] = uint8_t(remainder);
		}
		else if (remainder > 0) {
			h.truncated = true;
		}
		n = quotient;
		write_index--;
	}
	h.num_digits += num_new_digits;
	if (h.num_digits > MAX_DIGITS) {
		h.num_digits = MAX_DIGITS;
	}
	h.decimal_point += int32_t(num_new_digits);
	trim(h);
}

// computes h * 2^shift
void decimal_right_shift(Decimal& h, uint32_t shift) {
	uint32_t read_index = 0;
	uint32_t write_index = 0;

	uint64_t n = 0;

	while ((n >> shift) == 0) {
		if (read_index < h.num_digits) {
			n = (10 * n) + h.digits[read_index++];
		}
		else if (n == 0) {
			return;
		}
		else {
			while ((n >> shift) == 0) {
				n = 10 * n;
				read_index++;
			}
			break;
		}
	}
	h.decimal_point -= int32_t(read_index - 1);
	if (h.decimal_point < -DECIMAL_POINT_RANGE) { // it is zero
		h.num_digits = 0;
		h.decimal_point = 0;
		h.negative = false;
		h.truncated = false;
		return;
	}
	uint64_t mask = (uint64_t(1) << shift) - 1;
	while (read_index < h.num_digits) {
		uint8_t new_digit = uint8_t(n >> shift);
		n = (10 * (n & mask)) + h.digits[read_index++];
		h.digits[write_index++] = new_digit;
	}
	while (n > 0) {
		uint8_t new_digit = uint8_t(n >> shift);
		n = 10 * (n & mask);
		if (write_index < MAX_DIGITS) {
			h.digits[write_index++] = new_digit;
		}
		else if (new_digit > 0) {
			h.truncated = true;
		}
	}
	h.num_digits = write_index;
	trim(h);
}

AdjustedMantissa compute_float(Decimal& d) {
	AdjustedMantissa answer;
	if (d.num_digits == 0) {
		// should be zero
		answer.power2 = 0;
		answer.mantissa = 0;
		return answer;
	}
	// At this point, going further, we can assume that d.num_digits > 0.
	// We want to guard against excessive decimal point values because
	// they can result in long running times. Indeed, we do
	// shifts by at most 60 bits. We have that log(10**400)/log(2**60) ~= 22
	// which is fine, but log(10**299995)/log(2**60) ~= 16609 which is not
	// fine (runs for a long time).
	//
	if (d.decimal_point < -324) {
		// We have something smaller than 1e-324 which is always zero
		// in binary64 and binary32.
		// It should be zero.
		answer.power2 = 0;
		answer.mantissa = 0;
		return answer;
	}
	else if (d.decimal_point >= 310) {
		// We have something at least as large as 0.1e310 which is
		// always infinite.
		answer.power2 = INFINITE_POWER;
		answer.mantissa = 0;
		return answer;
	}

	static const uint32_t max_shift = 60;
	static const uint32_t num_powers = 19;
	static const uint8_t powers[19] = {
		0,  3,  6,  9,  13, 16, 19, 23, 26, 29, //
		33, 36, 39, 43, 46, 49, 53, 56, 59,     //
	};
	int32_t exp2 = 0;
	while (d.decimal_point > 0) {
		uint32_t n = uint32_t(d.decimal_point);
		uint32_t shift = (n < num_powers) ? powers[n] : max_shift;
		decimal_right_shift(d, shift);
		if (d.decimal_point < -DECIMAL_POINT_RANGE) {
			// should be zero
			answer.power2 = 0;
			answer.mantissa = 0;
			return answer;
		}
		exp2 += int32_t(shift);
	}
	// We shift left toward [1/2 ... 1].
	while (d.decimal_point <= 0) {
		uint32_t shift;
		if (d.decimal_point == 0) {
			if (d.digits[0] >= 5) {
				break;
			}
			shift = (d.digits[0] < 2) ? 2 : 1;
		}
		else {
			uint32_t n = uint32_t(-d.decimal_point);
			shift = (n < num_powers) ? powers[n] : max_shift;
		}
		decimal_left_shift(d, shift);
		if (d.decimal_point > DECIMAL_POINT_RANGE) {
			// we want to get infinity:
			answer.power2 = 0xFF;
			answer.mantissa = 0;
			return answer;
		}
		exp2 -= int32_t(shift);
	}
	// We are now in the range [1/2 ... 1] but the binary format uses [1 ... 2].
	exp2--;
	constexpr int32_t minimum_exponent = MINIMUM_EXPONENT;
	while ((minimum_exponent + 1) > exp2) {
		uint32_t n = uint32_t((minimum_exponent + 1) - exp2);
		if (n > max_shift) {
			n = max_shift;
		}
		decimal_right_shift(d, n);
		exp2 += int32_t(n);
	}
	if ((exp2 - minimum_exponent) >= INFINITE_POWER) {
		answer.power2 = INFINITE_POWER;
		answer.mantissa = 0;
		return answer;
	}

	const int mantissa_size_in_bits = MANTISSA_EXPLICIT_BITS + 1;
	decimal_left_shift(d, mantissa_size_in_bits);

	uint64_t mantissa = round(d);
	// It is possible that we have an overflow, in which case we need
	// to shift back.
	if (mantissa >= (uint64_t(1) << mantissa_size_in_bits)) {
		decimal_right_shift(d, 1);
		exp2 += 1;
		mantissa = round(d);
		if ((exp2 - minimum_exponent) >= INFINITE_POWER) {
			answer.power2 = INFINITE_POWER;
			answer.mantissa = 0;
			return answer;
		}
	}
	answer.power2 = exp2 - MINIMUM_EXPONENT;
	if (mantissa < (uint64_t(1) << MANTISSA_EXPLICIT_BITS)) {
		answer.power2--;
	}
	answer.mantissa =
		mantissa & ((uint64_t(1) << MANTISSA_EXPLICIT_BITS) - 1);
	return answer;
}

AdjustedMantissa parse_long_mantissa(const char*& first) {
	Decimal d = parse_decimal(first);
	return compute_float(d);
}

double from_chars(const char* first, const char** end) noexcept {
	bool negative = first[0] == '-';
	if (negative) {
		first++;
	}
	AdjustedMantissa am = parse_long_mantissa(first);
	uint64_t word = am.mantissa;
	word |= uint64_t(am.power2)
		<< MANTISSA_EXPLICIT_BITS;
	word = negative ? word | (uint64_t(1) << SIGN_INDEX)
		: word;
	double value;
	std::memcpy(&value, &word, sizeof(double));
	if (end)
		*end = first;
	return value;
}

} // atod

/*!
The format of the resulting decimal representation is similar to printf's %g
format. Returns an iterator pointing past-the-end of the decimal representation.
@note The input number must be finite, i.e. NaN's and Inf's are not supported.
@note The buffer must be large enough.
@note The result is NOT null-terminated.
*/
char* to_chars(char* first, double value) {
	bool negative = std::signbit(value);
	if (negative) {
		value = -value;
		*first++ = '-';
	}

	if (value == 0) // +-0
	{
		*first++ = '0';
		// Make it look like a floating-point number (#362, #378)
		if (negative) {
			*first++ = '.';
			*first++ = '0';
		}
		return first;
	}
	// Compute v = buffer * 10^decimal_exponent.
	// The decimal digits are stored in the buffer, which needs to be interpreted
	// as an unsigned decimal integer.
	// len is the length of the buffer, i.e. the number of decimal digits.
	int len = 0;
	int decimal_exponent = 0;
	dtoa::grisu2(first, len, decimal_exponent, value);
	// Format the buffer like printf("%.*g", prec, value)
	constexpr int kMinExp = -4;
	constexpr int kMaxExp = std::numeric_limits<double>::digits10;

	return dtoa::format_buffer(first, len, decimal_exponent, kMinExp,
		kMaxExp);
}

static constexpr const char* ErrorToString(ErrorCode code) {
	switch (code) {
	case json::OK:
		return "OK";
	case json::INVALID_CHARACTER:
		return "Invalid character";
	case json::INVALID_ESCAPE:
		return "Invalid escape";
	case json::MISSING_QUOTE:
		return "Missing '\"'";
	case json::MISSING_NULL:
		return "Missing 'null'";
	case json::MISSING_TRUE:
		return "Missing 'true'";
	case json::MISSING_FALSE:
		return "Missing 'false'";
	case json::MISSING_COMMAS_OR_BRACKETS:
		return "Missing ',' or ']'";
	case json::MISSING_COMMAS_OR_BRACES:
		return "Missing ',' or '}'";
	case json::MISSING_COLON:
		return "Missing ':'";
	case json::MISSING_SURROGATE_PAIR:
		return "Missing another '\\u' token to begin the second half of a unicode surrogate pair";
	case json::UNEXPECTED_ENDING_CHARACTER:
		return "Unexpected '\\0'";
	default:
		return "Unknow";
	}
}
static void CodePointToUTF8(String& s, UInt u) {
	if (u <= 0x7F)
		s += (u & 0xFF);
	else if (u <= 0x7FF) {
		s += (0xC0 | (0xFF & (u >> 6)));
		s += (0x80 | (0x3F & u));
	}
	else if (u <= 0xFFFF) {
		s += (0xE0 | (0xFF & (u >> 12)));
		s += (0x80 | (0x3F & (u >> 6)));
		s += (0x80 | (0x3F & u));
	}
	else {
		s += (0xF0 | (0xFF & (u >> 18)));
		s += (0x80 | (0x3F & (u >> 12)));
		s += (0x80 | (0x3F & (u >> 6)));
		s += (0x80 | (0x3F & u));
	}
}
static bool hasEsecapingChar(const String& str) {
	for (auto c : str) {
		if (c == '\\' || c == '"' || c < 0x20 || c > 0x7F)
			return true;
	}
	return false;
}
static UInt UTF8ToCodepoint(const char* s, const char* e) {
	const UInt REPLACEMENT_CHARACTER = 0xFFFD;

	UInt firstByte = static_cast<unsigned char>(*s);

	if (firstByte < 0x80)
		return firstByte;

	if (firstByte < 0xE0) {
		if (e - s < 2)
			return REPLACEMENT_CHARACTER;

		UInt calculated =
			((firstByte & 0x1F) << 6) | (static_cast<UInt>(s[1]) & 0x3F);
		s += 1;
		// oversized encoded characters are invalid
		return calculated < 0x80 ? REPLACEMENT_CHARACTER : calculated;
	}

	if (firstByte < 0xF0) {
		if (e - s < 3)
			return REPLACEMENT_CHARACTER;

		UInt calculated = ((firstByte & 0x0F) << 12) |
			((static_cast<UInt>(s[1]) & 0x3F) << 6) |
			(static_cast<UInt>(s[2]) & 0x3F);
		s += 2;
		// surrogates aren't valid codepoints itself
		// shouldn't be UTF-8 encoded
		if (calculated >= 0xD800 && calculated <= 0xDFFF)
			return REPLACEMENT_CHARACTER;
		// oversized encoded characters are invalid
		return calculated < 0x800 ? REPLACEMENT_CHARACTER : calculated;
	}

	if (firstByte < 0xF8) {
		if (e - s < 4)
			return REPLACEMENT_CHARACTER;

		UInt calculated = ((firstByte & 0x07) << 18) |
			((static_cast<UInt>(s[1]) & 0x3F) << 12) |
			((static_cast<UInt>(s[2]) & 0x3F) << 6) |
			(static_cast<UInt>(s[3]) & 0x3F);
		s += 3;
		// oversized encoded characters are invalid
		return calculated < 0x10000 ? REPLACEMENT_CHARACTER : calculated;
	}

	return REPLACEMENT_CHARACTER;
}
static String toHex16Bit(UInt x) {
	constexpr const char hex2[] =
		"000102030405060708090a0b0c0d0e0f"
		"101112131415161718191a1b1c1d1e1f"
		"202122232425262728292a2b2c2d2e2f"
		"303132333435363738393a3b3c3d3e3f"
		"404142434445464748494a4b4c4d4e4f"
		"505152535455565758595a5b5c5d5e5f"
		"606162636465666768696a6b6c6d6e6f"
		"707172737475767778797a7b7c7d7e7f"
		"808182838485868788898a8b8c8d8e8f"
		"909192939495969798999a9b9c9d9e9f"
		"a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
		"b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
		"c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
		"d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
		"e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
		"f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
	const UInt hi = (x >> 8) & 0xff;
	const UInt lo = x & 0xff;
	String out(4, ' ');
	out[0] = hex2[2 * hi];
	out[1] = hex2[2 * hi + 1];
	out[2] = hex2[2 * lo];
	out[3] = hex2[2 * lo + 1];
	return out;
}
static ErrorCode skipWhiteSpace(const char*& cur) {
	for (;;) {
		switch (*cur) {
		case'\t':break;
		case'\n':break;
		case'\r':break;
		case' ':break;
#if 0
		case'/':
			++cur;
			//单行注释
			if (*cur == '/') {
				++cur;
				while (*cur != '\n') {
					++cur;
				}
			}
			//多行注释
			else if (*cur == '*') {
				++cur;
				while (*cur != '*' || cur[1] != '/') {
					if (*cur == '\0') {
						return error_ = "Missing '*/'", false;
					}
					++cur;
				}
				++cur;
			}
			else
				return error_ = "Invalid comment", false;
			break;
#endif
		default:
			return OK;
		}
		++cur;
	}
}
static ErrorCode parseHex4(const char*& cur, UInt& u) {
	//u = 0;
	char ch = 0;
	for (UInt i = 0; i < 4; ++i) {
		u <<= 4;
		ch = *cur++;
		if (ch >= '0' && ch <= '9')
			u |= ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			u |= ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F')
			u |= ch - 'A' + 10;
		else
			return INVALID_CHARACTER;
	}
	return OK;
}
static ErrorCode parseString(const char*& cur, String& s) {
	++cur;
	for (;;) {
		char ch = *cur++;
		if (!ch)
			return MISSING_QUOTE;
		if (ch == '"')
			return OK;
		if (ch == '\\') {
			switch (*cur) {
			case '"': s += '"'; break;
			case 'n': s += '\n'; break;
			case 'r': s += '\r'; break;
			case 't': s += '\t'; break;
			case 'f': s += '\f'; break;
			case 'b': s += '\b'; break;
			case '/': s += '/'; break;
			case '\\': s += '\\'; break;
			case 'u': {
				UInt u = 0;
				JSON_CHECK(parseHex4(cur, u));
				if (u >= 0xD800 && u <= 0xDBFF) {
					if (cur[0] == '\\' && cur[1] == 'u') {
						cur += 2;
						UInt surrogatePair;
						if (!parseHex4(cur, surrogatePair))
							u = 0x10000 + ((u & 0x3FF) << 10) + (surrogatePair & 0x3FF);
						else
							return INVALID_CHARACTER;
					}
					else
						return MISSING_SURROGATE_PAIR;
				}
				CodePointToUTF8(s, u);
			}break;
			default:
				return INVALID_ESCAPE;
			}
			++cur;
		}
		else
			s += ch;
	}
	return OK;
}
static void writeString(String& out, const String& str) {
	out += '"';
	if (hasEsecapingChar(str))
		for (auto c : str) {
			UInt codepoint;
			switch (c) {
			case '\"':
				out += '\\';
				out += '"';
				break;
			case '\\':
				out += '\\';
				out += '\\';
				break;
			case '\b':
				out += '\\';
				out += 'b';
				break;
			case '\f':
				out += '\\';
				out += 'f';
				break;
			case '\n':
				out += '\\';
				out += 'n';
				break;
			case '\r':
				out += '\\';
				out += 'r';
				break;
			case '\t':
				out += '\\';
				out += 't';
				break;
			default:
#if JSON_UTF8
				if (uint8_t(c) < 0x20)
					out.append("\\u").append(toHex16Bit(c));
				else
					out += c);
#else
				codepoint = UTF8ToCodepoint(&c, &*str.end()); // modifies `c`
				if (codepoint < 0x20) {
					out.append("\\u").append(toHex16Bit(codepoint));
				}
				else if (codepoint < 0x80) {
					out += static_cast<char>(codepoint);
				}
				else if (codepoint < 0x10000) {
					// Basic Multilingual Plane
					out.append("\\u").append(toHex16Bit(codepoint));
				}
				else {
					// Extended Unicode. Encode 20 bits as a surrogate pair.
					codepoint -= 0x10000;
					out.append("\\u").append(toHex16Bit(0xd800 + ((codepoint >> 10) & 0x3ff)));
					out.append("\\u").append(toHex16Bit(0xdc00 + (codepoint & 0x3ff)));
				}
#endif
				break;
			}
		}
	else
		out.append(str);
	out += '"';
}
Value::Value() :type_(nullValue) { data_.u64 = 0; }
Value::Value(nullptr_t) : type_(nullValue) { data_.u64 = 0; }
Value::Value(bool b) : type_(booleanValue) { data_.u64 = b; }
Value::Value(Int num) : type_(intValue) { data_.i64 = num; }
Value::Value(UInt num) : type_(uintValue) { data_.u64 = num; }
Value::Value(Int64 num) : type_(intValue) { data_.i64 = num; }
Value::Value(UInt64 num) : type_(uintValue) { data_.u64 = num; }
Value::Value(Float num) : type_(realValue) { data_.d = num; }
Value::Value(Double num) : type_(realValue) { data_.d = num; }
Value::Value(const char* s) : type_(stringValue) { data_.s = new String(s); }
Value::Value(const String& s) : type_(stringValue) { data_.s = new String(s); }
Value::Value(const ValueType type) : type_(type) {
	data_.u64 = 0;
	switch (type_) {
	case nullValue:
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		data_.s = new String;
		break;
	case booleanValue:
		break;
	case arrayValue:
		data_.a = new Array;
		break;
	case objectValue:
		data_.o = new Object;
		break;
	default:
		break;
	}
};
Value::Value(const Value& other) {
	data_.u64 = 0;
	type_ = other.type_;
	switch (other.type_) {
	case nullValue:
		break;
	case intValue:
		data_.i64 = other.data_.i64;
		break;
	case uintValue:
		data_.u64 = other.data_.u64;
		break;
	case realValue:
		data_.d = other.data_.d;
		break;
	case stringValue:
		data_.s = new String(*other.data_.s);
		break;
	case booleanValue:
		data_.b = other.data_.b;
		break;
	case arrayValue:
		data_.a = new Array(*other.data_.a);
		break;
	case objectValue:
		data_.o = new Object(*other.data_.o);
		break;
	default:
		break;
	}
};
Value::Value(Value&& other)noexcept {
	data_ = other.data_;
	type_ = other.type_;
	other.type_ = nullValue;
	other.data_.u64 = 0;
};
Value::~Value() {
	switch (type_) {
	case nullValue:
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		delete data_.s;
		break;
	case booleanValue:
		break;
	case arrayValue:
		delete data_.a;
		break;
	case objectValue:
		delete data_.o;
		break;
	default:
		break;
	}
	type_ = nullValue;
}
Value& Value::operator=(const Value& other) {
	return operator=(Value(other));
};
Value& Value::operator=(Value&& other)noexcept {
	swap(other);
	return *this;
};
bool Value::equal(const Value& other)const {
	if (type_ != other.type_)
		return false;
	switch (type_) {
	case nullValue:
		return OK;
	case intValue:
		return data_.i64 == other.data_.i64;
	case uintValue:
		return data_.u64 == other.data_.u64;
	case realValue:
		return data_.d == other.data_.d;
	case stringValue:
		return *data_.s == *other.data_.s;
	case booleanValue:
		return data_.b == other.data_.b;
	case arrayValue:
		return *data_.a == *other.data_.a;
	case objectValue:
		return *data_.o == *other.data_.o;
	default:
		break;
	}
	return false;
}
bool Value::operator==(const Value& other)const {
	return equal(other);
}
Value& Value::operator[](const String& index) {
	assert(isObject() || isNull());
	if (isNull())
		new (this)Value(objectValue);
	return data_.o->operator[](index);
}
Value& Value::operator[](size_t index) {
	assert(isArray() || isNull());
	if (isNull())
		new (this)Value(arrayValue);
	return data_.a->operator[](index);
}
void Value::insert(const String& index, Value&& value) {
	assert(isObject() || isNull());
	if (isNull())
		new (this)Value(objectValue);
	data_.o->emplace(index, value);
}
bool Value::asBool()const { return data_.b; }
Int Value::asInt()const { return data_.i; }
UInt Value::asUInt()const { return data_.u; }
Int64 Value::asInt64()const { return data_.i64; }
UInt64 Value::asUInt64()const { return data_.u64; }
Float Value::asFloat()const { return data_.f; }
Double Value::asDouble()const { return data_.d; }
String Value::asString()const {
	return *data_.s;
}
Array Value::asArray()const {
	return *data_.a;
}
Object Value::asObject()const {
	return *data_.o;
}
bool Value::isNull()const { return type_ == nullValue; }
bool Value::isBool()const { return type_ == booleanValue; }
bool Value::isNumber()const { return type_ == intValue || type_ == uintValue || type_ == realValue; }
bool Value::isString()const { return type_ == stringValue; }
bool Value::isArray()const { return type_ == arrayValue; }
bool Value::isObject()const { return type_ == objectValue; }
ValueType Value::getType()const { return type_; }
void Value::swap(Value& other) {
	std::swap(data_, other.data_);
	std::swap(type_, other.type_);
}
bool Value::remove(const String& index) {
	if (isObject()) {
		return data_.o->erase(index);
	}
	return false;
}
void Value::append(const Value& value) {
	append(Value(value));
}
void Value::append(Value&& value) {
	assert(isArray() || isNull());
	if (isNull())
		new (this)Value(arrayValue);
	data_.a->push_back(move(value));
}
size_t Value::size()const {
	switch (type_) {
	case nullValue:
		return 0;
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		return data_.s->size();
		break;
	case booleanValue:
		break;
	case arrayValue:
		return data_.a->size();
		break;
	case objectValue:
		return data_.o->size();
		break;
	default:
		break;
	}
	return 1;
}
bool Value::empty()const {
	switch (type_) {
	case nullValue:
		return OK;
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		return data_.s->empty();
		break;
	case booleanValue:
		break;
	case arrayValue:
		return data_.a->empty();
		break;
	case objectValue:
		return data_.o->empty();
		break;
	default:
		break;
	}
	return false;
}
bool Value::has(const String& key)const {
	if (!isObject())
		return false;
	return data_.o->find(key) != data_.o->end();
}
void Value::clear() {
	switch (type_) {
	case nullValue:
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		data_.s->clear();
		break;
	case booleanValue:
		break;
	case arrayValue:
		data_.a->clear();
		break;
	case objectValue:
		data_.o->clear();
		break;
	default:
		break;
	}
	type_ = nullValue;
}
String Value::toShortString()const {
	String out;
	UInt indent = 0;
	_toString(out, indent, false);
	return out;
}
String Value::toStyledString()const {
	String out;
	UInt indent = 0;
	_toString(out, indent, true);
	return out;
}
ErrorCode Value::parse(const char* str, size_t len) {
	const char* cur = str;
	ErrorCode err = _parseValue(cur);
	if (err != OK) {
		size_t line = 1;
		for (const char* i = str; i != cur; ++i) {
			if (*i == '\n')++line;
		}
		std::cerr << err << " in line " << line << std::endl;
	}
	return err;
}
ErrorCode Value::parse(const String& str) {
	return parse(str.c_str(), str.length());
}
ErrorCode Value::_parseValue(const char*& cur) {
	JSON_SKIP;
	switch (*cur) {
	case '\0':
		abort();
		break;
	case 'n': {
		if (cur[1] == 'u' && cur[2] == 'l' && cur[3] == 'l')
			return cur += 4, OK;
		else
			return MISSING_NULL;
	}
	case 't': {
		if (cur[1] == 'r' && cur[2] == 'u' && cur[3] == 'e')
			return *this = true, cur += 4, OK;
		else
			return MISSING_TRUE;
	}
	case 'f': {
		if (cur[1] == 'a' && cur[2] == 'l' && cur[3] == 's' && cur[4] == 'e')
			return *this = false, cur += 5, OK;
		else
			return MISSING_FALSE;
	}
	case '[': {
		++cur;
		JSON_SKIP;
		*this = arrayValue;
		if (*cur == ']') {
			++cur;
			return OK;
		}
		for (;;) {
			data_.a->push_back({});
			JSON_CHECK(data_.a->back()._parseValue(cur));
			JSON_SKIP;
			if (*cur == ',') {
				++cur;
			}
			else if (*cur == ']') {
				++cur;
				break;
			}
			else {
				return MISSING_COMMAS_OR_BRACKETS;
			}
		}
		return OK;
	}
	case '{': {
		++cur;
		JSON_SKIP;
		*this = objectValue;
		if (*cur == '}') {
			++cur;
			return OK;
		}
		for (;;) {
			String key;
			JSON_SKIP;
			if (*cur != '"') {
				return MISSING_QUOTE;
			}
			JSON_CHECK(parseString(cur, key));
			JSON_SKIP;
			if (*cur != ':') {
				return MISSING_COLON;
			}
			++cur;
			JSON_CHECK(data_.o->operator[](key)._parseValue(cur));
			JSON_SKIP;
			if (*cur == ',')
				++cur;
			else if (*cur == '}') {
				++cur;
				return OK;
			}
			else {
				return MISSING_COMMAS_OR_BRACES;
			}
		}
	}
	case '"': {
		*this = stringValue;
		JSON_CHECK(parseString(cur, *data_.s));
		return OK;
	}
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '-': {
		const char* end;
		*this = atod::from_chars(cur, &end);
		cur = end;
		return OK;
	}
	default:
		return INVALID_CHARACTER;
	}
}
void Value::_toString(String& out, UInt& indent, bool styled)const {
	switch (type_) {
	case nullValue:
		out.append("null", 4);
		break;
	case intValue:
		out.append(std::to_string(data_.i64));
		break;
	case uintValue:
		out.append(std::to_string(data_.u64));
		break;
	case realValue: {
		char buffer[21]{ 0 };
		to_chars(buffer, data_.d);
		out.append(buffer);
		break;
	}
	case stringValue:
		writeString(out, *data_.s);
		break;
	case booleanValue:
		asBool() ? out.append("true", 4) : out.append("false", 5);
		break;
	case arrayValue:
		if (styled) {
			out += '[';
			if (!empty()) {
				out += '\n';
				++indent;
				for (auto& x : *data_.a) {
					out.append(indent, '\t');
					x._toString(out, indent, styled);
					out += ',';
					out += '\n';
				}
				--indent;
				out.pop_back();
				out.pop_back();
				out += '\n';
				out.append(indent, '\t');
			}
			out += ']';
		}
		else {
			out += '[';
			if (!empty()) {
				for (auto& x : *data_.a) {
					x._toString(out, indent, styled);
					out += ',';
				}
				out.pop_back();
			}
			out += ']';
		}
		break;
	case objectValue:
		if (styled) {
			out += '{';
			if (!empty()) {
				out += '\n';
				++indent;
				for (auto& x : *data_.o) {
					out.append(indent, '\t');
					writeString(out, x.first);
					out += ':';
					out += ' ';
					x.second._toString(out, indent, styled);
					out += ',';
					out += '\n';
				}
				--indent;
				out.pop_back();
				out.pop_back();
				out += '\n';
				out.append(indent, '\t');
			}
			out += '}';
		}
		else {
			out += '{';
			if (!empty()) {
				for (auto& x : *data_.o) {
					writeString(out, x.first);
					out += ':';
					x.second._toString(out, indent, styled);
					out += ',';
				}
				out.pop_back();
			}
			out += '}';
		}
		break;
	default:
		break;
	}

}
std::ostream& operator<<(std::ostream& os, const Value& value) {
	return os << value.toStyledString();
}
std::ostream& operator<<(std::ostream& os, ErrorCode err) {
	return os << ErrorToString(err);
}
} // namespace Json
