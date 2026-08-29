/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Shark custom DVFS interface for Samsung Exynos SoC domains.
 *
 * Exynos 8895 / Galaxy S8 adaptation based on the original Shark interface.
 * Keep this header synchronized with drivers/soc/samsung/exynos-soc_interface.c.
 */
#ifndef __SOC_SAMSUNG_EXYNOS_SOC_INTERFACE_H__
#define __SOC_SAMSUNG_EXYNOS_SOC_INTERFACE_H__

#include <linux/types.h>

/*
 * Exynos 8895 G3D DVFS domain.
 * Frequencies: 839, 764, 683, 572, 546, 455, 385, 338 and 260 MHz.
 * The current .c implementation carries one static fallback ASV profile.
 */
#define SHARK_G3D_DVFS_LEVEL_COUNT	9
#define SHARK_G3D_ASV_GROUP_COUNT	1
#define SHARK_G3D_DEFAULT_ASV_GROUP	0
#define SHARK_G3D_VOLT_STEP_UV		6250U
#define SHARK_G3D_MIN_VOLT_UV		718750U
#define SHARK_G3D_MAX_VOLT_UV		793750U

/*
 * Exynos 8895 MIF DVFS domain.
 * Frequencies: 2093, 2002, 1794, 1540, 1352, 1014, 845, 676,
 *              546, 421, 286 and 208 MHz.
 * The current .c implementation carries one static fallback ASV profile.
 */
#define SHARK_MIF_DVFS_LEVEL_COUNT	12
#define SHARK_MIF_ASV_GROUP_COUNT	1
#define SHARK_MIF_DEFAULT_ASV_GROUP	0
#define SHARK_MIF_VOLT_STEP_UV		6250U

/* ECT dvfs_mif boot/resume level indices for Exynos 8895. */
#define SHARK_MIF_DEFAULT_BOOT_LEVEL	0
#define SHARK_MIF_DEFAULT_RESUME_LEVEL	5

unsigned int shark_g3d_get_clamped_asv_group(unsigned int asv_group);
unsigned long shark_g3d_get_max_freq(void);
unsigned long shark_g3d_get_min_freq(void);
unsigned long shark_g3d_get_freq(unsigned int level);
unsigned int shark_g3d_get_default_volt(unsigned int asv_group,
					unsigned int level);
int shark_g3d_get_level_from_freq(unsigned long freq);
unsigned long shark_g3d_snap_freq(unsigned long freq);

unsigned long shark_mif_get_max_freq(void);
unsigned long shark_mif_get_min_freq(void);
unsigned long shark_mif_get_freq(unsigned int level);
unsigned long shark_mif_get_boot_freq(void);
unsigned long shark_mif_get_resume_freq(void);
unsigned int shark_mif_get_clamped_asv_group(unsigned int asv_group);
unsigned int shark_mif_get_default_volt(unsigned int asv_group,
					unsigned int level);
unsigned int shark_mif_get_interpolated_volt(unsigned long freq);
unsigned long shark_mif_snap_freq(unsigned long freq);

#endif /* __SOC_SAMSUNG_EXYNOS_SOC_INTERFACE_H__ */
