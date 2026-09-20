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

enum exynos8895_g3d_persist_stage {
	EXYNOS8895_G3D_PERSIST_BOOT = 1,
	EXYNOS8895_G3D_PERSIST_FVMAP_ENTER = 2,
	EXYNOS8895_G3D_PERSIST_FVMAP_EXPANDED = 3,
	EXYNOS8895_G3D_PERSIST_FVMAP_REFRESH = 4,
	EXYNOS8895_G3D_PERSIST_GPU_REQ = 10,
	EXYNOS8895_G3D_PERSIST_GPU_FAIL = 11,
	EXYNOS8895_G3D_PERSIST_GPU_OK = 12,
	EXYNOS8895_G3D_PERSIST_GPU_DONE = 13,
	EXYNOS8895_G3D_PERSIST_CAL_REQ = 20,
	EXYNOS8895_G3D_PERSIST_FVMAP_OK = 21,
	EXYNOS8895_G3D_PERSIST_FVMAP_FAIL = 22,
	EXYNOS8895_G3D_PERSIST_ACPM_BEGIN = 23,
	EXYNOS8895_G3D_PERSIST_ACPM_END = 24,
	EXYNOS8895_G3D_PERSIST_PLL = 25,
	EXYNOS8895_G3D_PERSIST_CORE = 26,
	EXYNOS8895_G3D_PERSIST_CAL_DONE = 27,
	EXYNOS8895_G3D_PERSIST_RUNTIME_MAX = 30,
};

int exynos8895_g3d_persist_init(void);
void exynos8895_g3d_persist_log(unsigned int stage,
				unsigned int req,
				unsigned int a,
				unsigned int b,
				unsigned int c,
				int ret);

#endif /* __EXYNOS8895_G3D_HARDCODED_H__ */
