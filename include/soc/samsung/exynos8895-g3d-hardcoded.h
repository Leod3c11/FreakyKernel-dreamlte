#ifndef __EXYNOS8895_G3D_HARDCODED_H__
#define __EXYNOS8895_G3D_HARDCODED_H__

/*
 * Compatibility facade. All editable tables/constants live in the central
 * SoC profile.
 */
#include <soc/samsung/exynos8895-hardcoded-profile.h>

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
