#ifndef __EXYNOS8895_G3D_HARDCODED_H__
#define __EXYNOS8895_G3D_HARDCODED_H__

#include <linux/types.h>

/*
 * Exynos8895 G3D hardcoded DVFS table.
 *
 * This file is intentionally the single place to edit GPU operating points.
 * Every active row owns:
 *   - the DVFS-visible frequency (clock_khz)
 *   - the exact PLL_G3D PMS values (pll_m/pll_p/pll_s)
 *   - the stock ACPM anchor used before direct PLL programming
 *   - an optional absolute voltage override (voltage_uv)
 *   - governor thresholds / QoS data
 *
 * IMPORTANT UNITS
 *   clock_khz  : kHz
 *   voltage_uv : microvolts
 *
 * voltage_uv == 0 means: keep ACPM/ASV voltage control for that row.
 * A non-zero voltage asks the vdd_g3d regulator for that exact value.
 * The dreamlte DTS currently constrains vdd_g3d to 450000..850000 uV.
 *
 * PLL_G3D uses a 26 MHz reference and integer PMS on this SoC:
 *   Fout = 26 MHz * M / (P * 2^S)
 *
 * The table MUST be ordered highest -> lowest.
 */
struct exynos8895_g3d_hardcoded_opp {
    unsigned int clock_khz;
    unsigned short pll_m;
    unsigned short pll_p;
    unsigned short pll_s;
    unsigned int acpm_anchor_khz;
    unsigned int voltage_uv;
    int min_threshold;
    int max_threshold;
    int down_staycount;
    unsigned int mem_freq;
    unsigned int cpu_little_min_freq;
    unsigned int cpu_big_max_freq;
};

static const struct exynos8895_g3d_hardcoded_opp exynos8895_g3d_opp_table[] = {
    /* clock   M    P  S   ACPM anchor  voltage_uV  min max stay    MIF     little big */
    { 900000, 450, 13, 0,     839000,          0, 44, 65, 1, 2093000,       0, 0 },
    { 850000, 425, 13, 0,     839000,          0, 44, 65, 1, 2093000,       0, 0 },
    { 800000, 400, 13, 0,     839000,          0, 43, 65, 1, 2093000,       0, 0 },
    { 750000, 375, 13, 0,     764000,          0, 43, 65, 1, 2093000,       0, 0 },
    { 700000, 350, 13, 0,     764000,          0, 39, 65, 1, 2093000,       0, 0 },
    { 650000, 325, 13, 0,     683000,          0, 39, 65, 1, 2093000,       0, 0 },
    { 600000, 300, 13, 0,     683000,          0, 47, 65, 1, 2093000,       0, 0 },
    { 550000, 550, 13, 1,     572000,          0, 47, 65, 1, 2093000,       0, 0 },
    { 500000, 500, 13, 1,     546000,          0, 40, 65, 1, 2002000,       0, 0 },
    { 450000, 450, 13, 1,     455000,          0, 40, 65, 1, 2002000,       0, 0 },
    { 400000, 400, 13, 1,     455000,          0, 42, 65, 1, 1794000,       0, 0 },
    { 350000, 350, 13, 1,     385000,          0, 42, 65, 1, 1794000,       0, 0 },
    { 300000, 300, 13, 1,     338000,          0, 35, 65, 1, 1352000,       0, 0 },
    { 260000, 520, 13, 2,     260000,          0, 35, 65, 1, 1352000,       0, 0 },
};

#define EXYNOS8895_G3D_OPP_COUNT \
    (sizeof(exynos8895_g3d_opp_table) / sizeof(exynos8895_g3d_opp_table[0]))

#define EXYNOS8895_G3D_START_KHZ      260000U
#define EXYNOS8895_G3D_HIGHSPEED_KHZ  450000U

/* Matches the regulator constraints in exynos8895-dreamlte_common.dtsi. */
#define EXYNOS8895_G3D_MIN_UV          450000U
#define EXYNOS8895_G3D_MAX_UV          850000U

/* Thermal locks must also exist in exynos8895_g3d_opp_table[]. */
static const unsigned int exynos8895_g3d_thermal_khz[] = {
    800000, 700000, 600000, 500000, 400000, 300000,
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

#endif /* __EXYNOS8895_G3D_HARDCODED_H__ */
