#ifndef __EXYNOS8895_G3D_HARDCODED_H__
#define __EXYNOS8895_G3D_HARDCODED_H__

/*
 * Exynos8895 G3D source-owned OPP table.
 *
 * There are exactly nine slots because the stock ACPM/FVMap firmware exposes
 * nine G3D levels.  Do not change EXYNOS8895_G3D_OPP_COUNT until the FVMap
 * layout itself is rebuilt; changing the count would move following SRAM
 * structures.
 *
 * clock_khz     - frequency name requested through ACPM_DVFS_G3D
 * voltage_uv    - absolute FVMap voltage; 0 preserves the firmware/ASV value
 * pll_m/p/s     - desired PLL values for this slot
 * override_pms  - 0 preserves firmware PMS; 1 writes packed M/P/S into FVMap
 *
 * IMPORTANT: keep override_pms = 0 until boot logs prove the firmware PMS
 * encoding matches EXYNOS8895_G3D_PACK_PMS().
 */

#define EXYNOS8895_G3D_ACPM_INDEX 4U
#define EXYNOS8895_G3D_OPP_COUNT  9U

#define EXYNOS8895_G3D_PACK_PMS(_m, _p, _s) \
	((((unsigned int)(_m) & 0x3ffU) << 16) | \
	 (((unsigned int)(_p) & 0x3fU) << 8) | \
	 ((unsigned int)(_s) & 0x7U))

struct exynos8895_g3d_hardcoded_opp {
	unsigned int clock_khz;
	unsigned int voltage_uv;
	unsigned int pll_m;
	unsigned int pll_p;
	unsigned int pll_s;
	unsigned int override_pms;
	unsigned int min_threshold;
	unsigned int max_threshold;
	unsigned int down_staycount;
	unsigned int mem_freq;
	unsigned int cpu_little_min_freq;
	unsigned int cpu_big_max_freq;
};

/*
 * Safe validation table: stock frequency names and stock PMS observations.
 * voltage_uv=0 and override_pms=0 mean the first v6 build preserves the
 * firmware's voltage and PMS byte-for-byte while proving the new ownership
 * path is stable.  Edit these nine rows later; no duplicate rate table is
 * required elsewhere in the kernel.
 */
static const struct exynos8895_g3d_hardcoded_opp exynos8895_g3d_opp_table[EXYNOS8895_G3D_OPP_COUNT] = {
	/* clock   volt    M    P  S  pms  min max stay   MIF      little big */
	{ 839000,     0, 129, 4, 0, 0,   44, 65, 1, 2093000,       0,   0 },
	{ 764000,     0, 147, 5, 0, 0,   43, 65, 1, 2093000,       0,   0 },
	{ 683000,     0, 105, 4, 0, 0,   39, 65, 1, 2093000,       0,   0 },
	{ 572000,     0, 176, 4, 1, 0,   47, 65, 1, 2093000,       0,   0 },
	{ 546000,     0, 168, 4, 1, 0,   40, 65, 1, 2002000,       0,   0 },
	{ 455000,     0, 140, 4, 1, 0,   40, 65, 1, 2002000,       0,   0 },
	{ 385000,     0, 148, 5, 1, 0,   42, 65, 1, 1794000,       0,   0 },
	{ 338000,     0, 104, 4, 1, 0,   35, 65, 1, 1352000,       0,   0 },
	{ 260000,     0, 160, 4, 2, 0,   35, 65, 1, 1352000,       0,   0 },
};

#endif
