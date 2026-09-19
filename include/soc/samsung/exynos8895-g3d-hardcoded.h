#ifndef __EXYNOS8895_G3D_HARDCODED_H__
#define __EXYNOS8895_G3D_HARDCODED_H__

/*
 * Compatibility facade.
 *
 * Editable G3D data now lives only in:
 *   include/soc/samsung/exynos8895-hardcoded-profile.h
 */

#include <soc/samsung/exynos8895-hardcoded-profile.h>

#define EXYNOS8895_G3D_ACPM_INDEX        4U
#define EXYNOS8895_G3D_OPP_COUNT         EXYNOS8895_HC_G3D_OPP_COUNT
#define EXYNOS8895_G3D_FVMAP_COUNT       EXYNOS8895_HC_G3D_OPP_COUNT
#define EXYNOS8895_G3D_TMU_COUNT         7U
#define EXYNOS8895_G3D_PLL_FIN_KHZ       26000U
#define EXYNOS8895_G3D_PLL_SFR_LO        0x0140U
#define EXYNOS8895_G3D_MIN_UV            450000U
#define EXYNOS8895_G3D_MAX_UV            850000U
#define EXYNOS8895_G3D_START_KHZ         260000U
#define EXYNOS8895_G3D_HIGHSPEED_KHZ     455000U

#define EXYNOS8895_G3D_PACK_PMS(_m, _p, _s) \
    ((((unsigned int)(_m) & 0x3ffU) << 16) | \
     (((unsigned int)(_p) & 0x3fU) << 8) | \
     ((unsigned int)(_s) & 0x7U))

#define EXYNOS8895_G3D_PMS_MASK \
    EXYNOS8895_G3D_PACK_PMS(0x3ffU, 0x3fU, 0x7U)

static const unsigned int
exynos8895_g3d_stock_rate[EXYNOS8895_G3D_OPP_COUNT] = {
    839000, 764000, 683000, 572000, 546000,
    455000, 385000, 338000, 260000,
};

static const unsigned int
exynos8895_g3d_stock_pms[EXYNOS8895_G3D_OPP_COUNT] = {
    EXYNOS8895_G3D_PACK_PMS(129, 4, 0),
    EXYNOS8895_G3D_PACK_PMS(147, 5, 0),
    EXYNOS8895_G3D_PACK_PMS(105, 4, 0),
    EXYNOS8895_G3D_PACK_PMS(176, 4, 1),
    EXYNOS8895_G3D_PACK_PMS(168, 4, 1),
    EXYNOS8895_G3D_PACK_PMS(140, 4, 1),
    EXYNOS8895_G3D_PACK_PMS(148, 5, 1),
    EXYNOS8895_G3D_PACK_PMS(104, 4, 1),
    EXYNOS8895_G3D_PACK_PMS(160, 4, 2),
};

static const unsigned int
exynos8895_g3d_tmu_khz[EXYNOS8895_G3D_TMU_COUNT] = {
    850000, 800000, 700000, 572000, 455000, 385000, 260000,
};

static const unsigned int
exynos8895_g3d_thermal_khz[EXYNOS8895_G3D_TMU_COUNT] = {
    850000, 800000, 700000, 572000, 455000, 385000, 260000,
};

static inline const struct exynos8895_g3d_hardcoded_opp *
exynos8895_g3d_find_opp(unsigned long clock_khz)
{
    return exynos8895_hc_find_g3d(clock_khz);
}

static inline const struct exynos8895_g3d_hardcoded_opp *
exynos8895_g3d_find_opp_by_acpm_key(unsigned long acpm_key_khz)
{
    unsigned int i;

    for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++)
        if (exynos8895_g3d_opp_table[i].acpm_key_khz == acpm_key_khz)
            return &exynos8895_g3d_opp_table[i];

    return NULL;
}

int exynos8895_g3d_hardcoded_apply(void);
bool exynos8895_g3d_hardcoded_active(void);
bool exynos8895_g3d_hardcoded_sync_cal(void);
int exynos8895_g3d_sram_debug_dump(char *buf, unsigned int size);

#endif /* __EXYNOS8895_G3D_HARDCODED_H__ */
