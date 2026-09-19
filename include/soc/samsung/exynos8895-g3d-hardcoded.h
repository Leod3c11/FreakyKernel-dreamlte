#ifndef __EXYNOS8895_G3D_HARDCODED_H__
#define __EXYNOS8895_G3D_HARDCODED_H__

#include <linux/types.h>

/*
 * Single source of truth for Exynos8895 G3D DVFS.
 *
 * clock_khz:
 *   Physical PLL_G3D target. Custom entries must be exactly synthesizable.
 *
 * acpm_anchor_khz:
 *   Existing Samsung/ACPM OPP used to establish a safe voltage before the
 *   kernel re-programs PLL_G3D to clock_khz. Keep this at a stock FVMap rate.
 *
 * volt_margin_uv:
 *   ACPM voltage margin added to the selected anchor. 0 keeps stock ASV
 *   voltage. Positive values are intentionally left for manual tuning.
 *
 * The table MUST be ordered from highest to lowest clock.
 */
struct exynos8895_g3d_hardcoded_opp {
    unsigned int clock_khz;
    unsigned int acpm_anchor_khz;
    int volt_margin_uv;
    int min_threshold;
    int max_threshold;
    int down_staycount;
    unsigned int mem_freq;
    unsigned int cpu_little_min_freq;
    unsigned int cpu_big_max_freq;
};

static const struct exynos8895_g3d_hardcoded_opp exynos8895_g3d_opp_table[] = {
    /* clock   ACPM anchor  margin  min max stay   MIF      little  big */
    { 900000,  839000,          0, 44, 65, 1, 2093000,       0,    0 },
    { 850000,  839000,          0, 44, 65, 1, 2093000,       0,    0 },
    { 800000,  839000,          0, 43, 65, 1, 2093000,       0,    0 },
    { 750000,  764000,          0, 43, 65, 1, 2093000,       0,    0 },
    { 700000,  764000,          0, 39, 65, 1, 2093000,       0,    0 },
    { 650000,  683000,          0, 39, 65, 1, 2093000,       0,    0 },
    { 600000,  683000,          0, 47, 65, 1, 2093000,       0,    0 },
    { 550000,  572000,          0, 47, 65, 1, 2093000,       0,    0 },
    { 500000,  546000,          0, 40, 65, 1, 2002000,       0,    0 },
    { 450000,  455000,          0, 40, 65, 1, 2002000,       0,    0 },
    { 400000,  455000,          0, 42, 65, 1, 1794000,       0,    0 },
    { 350000,  385000,          0, 42, 65, 1, 1794000,       0,    0 },
    { 300000,  338000,          0, 35, 65, 1, 1352000,       0,    0 },
    { 260000,  260000,          0, 35, 65, 1, 1352000,       0,    0 },
};

#define EXYNOS8895_G3D_OPP_COUNT \
    (sizeof(exynos8895_g3d_opp_table) / sizeof(exynos8895_g3d_opp_table[0]))

#define EXYNOS8895_G3D_START_KHZ      260000U
#define EXYNOS8895_G3D_HIGHSPEED_KHZ  450000U

/* Thermal locks must also be entries in exynos8895_g3d_opp_table[]. */
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
