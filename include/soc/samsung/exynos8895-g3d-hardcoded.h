#ifndef __EXYNOS8895_G3D_HARDCODED_H__
#define __EXYNOS8895_G3D_HARDCODED_H__

#include <linux/types.h>

/*
 * Exynos8895 G3D hardcoded ACPM/FVMap table.
 *
 * SINGLE SOURCE OF TRUTH:
 *   edit only exynos8895_g3d_opp_table[] to change the nine G3D operating
 *   points.  ACPM remains the only transition owner; the kernel never writes
 *   PLL_G3D directly from the Mali DVFS path.
 *
 * Exactly nine slots are kept because the Exynos8895 DVFS firmware allocates
 * nine G3D levels in SRAM.  Changing the slot count would change the FVMap
 * layout and is intentionally rejected.
 *
 * clock_khz  : rate requested through ACPM_DVFS_G3D
 * voltage_uv : absolute live-FVMap voltage; 0 preserves the firmware value
 * pll_m/p/s  : raw PLL_G3D PMS fields written into the matching SRAM slot
 *
 * For PLL_1052X with FIN=26 MHz:
 *   Fout = 26000 * M / (P * 2^S) kHz
 * The runtime validator rejects rows that are not exact or violate the PLL
 * limits from pll_spec.c (P 1..63, M 64..1023, S 0..6, Fref 2..8 MHz,
 * VCO 600..1200 MHz, Fout 9.5..1200 MHz).
 */

#define EXYNOS8895_G3D_ACPM_INDEX       4U
#define EXYNOS8895_G3D_OPP_COUNT        9U
#define EXYNOS8895_G3D_TMU_COUNT        7U
#define EXYNOS8895_G3D_PLL_FIN_KHZ      26000U
/*
 * The generated CMUCAL table starts PLL_CON0_PLL_G3D at +0x120, but the
 * live Exynos8895 FVMap/PMUCAL relocates the real PLL CON0 to +0x140.
 * fvmap_copy_from_sram() already performs exactly this relocation for CAL;
 * the hardcoded path must validate against the live SRAM/PMUCAL address.
 */
#define EXYNOS8895_G3D_PLL_SFR_LO       0x0140U
#define EXYNOS8895_G3D_MIN_UV           450000U
#define EXYNOS8895_G3D_MAX_UV           850000U

#define EXYNOS8895_G3D_PACK_PMS(_m, _p, _s) \
	((((unsigned int)(_m) & 0x3ffU) << 16) | \
	 (((unsigned int)(_p) & 0x3fU) << 8) | \
	 ((unsigned int)(_s) & 0x7U))

#define EXYNOS8895_G3D_PMS_MASK \
	EXYNOS8895_G3D_PACK_PMS(0x3ffU, 0x3fU, 0x7U)

struct exynos8895_g3d_hardcoded_opp {
	unsigned int clock_khz;
	unsigned int voltage_uv;
	unsigned int pll_m;
	unsigned int pll_p;
	unsigned int pll_s;
	unsigned int min_threshold;
	unsigned int max_threshold;
	unsigned int down_staycount;
	unsigned int mem_freq;
	unsigned int cpu_little_min_freq;
	unsigned int cpu_big_max_freq;
};

/*
 * Default hardcoded table.
 *
 * The top three rates are exact integer-N replacements for the nominal stock
 * 839/764/683 MHz slots.  This device's ASV v8/group 4 disables those stock
 * rows and exposes 0 uV, so an OC row must own an explicit voltage instead of
 * passing 0 to ACPM.  The defaults below are conservative bring-up values
 * within the observed 850000 uV rail limit; tune them for your silicon.
 *
 * 900 MHz exact preset for slot 0: clock=900000, M=450, P=13, S=0.
 *
 * Known device voltages below 572 MHz come from the supplied all_dump/ASV
 * data.  PMS values are exact and are applied by ACPM from its own SRAM.
 */
static const struct exynos8895_g3d_hardcoded_opp exynos8895_g3d_opp_table[EXYNOS8895_G3D_OPP_COUNT] = {
	/* clock   volt      M    P  S   min max stay   MIF      little big */
	{ 850000, 850000,   425, 13, 0,   44, 65, 1, 2093000,       0,   0 },
	{ 800000, 825000,   400, 13, 0,   43, 65, 1, 2093000,       0,   0 },
	{ 700000, 775000,   350, 13, 0,   39, 65, 1, 2093000,       0,   0 },
	{ 572000, 681250,   176,  4, 1,   47, 65, 1, 2093000,       0,   0 },
	{ 546000, 662500,   168,  4, 1,   40, 65, 1, 2002000,       0,   0 },
	{ 455000, 650000,   140,  4, 1,   40, 65, 1, 2002000,       0,   0 },
	{ 385000, 643750,   385, 13, 1,   42, 65, 1, 1794000,       0,   0 },
	{ 338000, 637500,   104,  4, 1,   35, 65, 1, 1352000,       0,   0 },
	{ 260000, 637500,   160,  4, 2,   35, 65, 1, 1352000,       0,   0 },
};

/* Thermal locks must always point at rates that actually exist above. */
static const unsigned int exynos8895_g3d_tmu_khz[EXYNOS8895_G3D_TMU_COUNT] = {
	850000, 800000, 700000, 572000, 455000, 385000, 260000,
};

static inline const struct exynos8895_g3d_hardcoded_opp *
exynos8895_g3d_find_opp(unsigned long clock_khz)
{
	unsigned int i;

	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++)
		if (exynos8895_g3d_opp_table[i].clock_khz == clock_khz)
			return &exynos8895_g3d_opp_table[i];

	return NULL;
}

int exynos8895_g3d_hardcoded_apply(void);
bool exynos8895_g3d_hardcoded_active(void);
bool exynos8895_g3d_hardcoded_sync_cal(void);

#endif
