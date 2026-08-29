// SPDX-License-Identifier: GPL-2.0
/*
 * Shark custom DVFS interface for Samsung Exynos SoC domains.
 *
 * Exynos 8895 / Galaxy S8 adaptation based on the original Shark interface.
 * Frequency tables are taken from the Exynos 8895 ECT DVFS dump.
 * Static voltage fallbacks use ECT ASV TABLE VERSION 1, ASV group 0.
 *
 * Keep the ASV/frequency tables here so the public header does not duplicate
 * static tables in every translation unit that includes it.
 */
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/math64.h>
#include <linux/types.h>

#include <soc/samsung/exynos-soc_interface.h>

/*
 * Exynos 8895 G3D DVFS frequency table.
 * Source: ECT domain dvfs_g3d.
 * The table must remain in descending order.
 */
static const unsigned long shark_g3d_freq_table[SHARK_G3D_DVFS_LEVEL_COUNT] = {
	839000,
	764000,
	683000,
	572000,
	546000,
	455000,
	385000,
	338000,
	260000,
};

/*
 * Exynos 8895 G3D static voltage fallback.
 * Source: ECT dvfs_g3d, TABLE VERSION 1, ASV group 0.
 * Each column matches shark_g3d_freq_table[] in descending order.
 *
 * This is intentionally a single static fallback profile. The device CAL/ASV
 * path may later replace/override these voltages with the fuse-selected table.
 */
static const unsigned int shark_g3d_asv_volt_table
	[SHARK_G3D_ASV_GROUP_COUNT][SHARK_G3D_DVFS_LEVEL_COUNT] = {
	{ 793750, 781250, 768750, 731250, 737500, 731250, 731250, 725000, 718750 },
};

/*
 * Exynos 8895 MIF DVFS frequency table.
 * Source: ECT domain dvfs_mif.
 * The table must remain in descending order.
 */
static const unsigned long shark_mif_freq_table[SHARK_MIF_DVFS_LEVEL_COUNT] = {
	2093000,
	2002000,
	1794000,
	1540000,
	1352000,
	1014000,
	845000,
	676000,
	546000,
	421000,
	286000,
	208000,
};

/*
 * Exynos 8895 MIF static voltage fallback.
 * Source: ECT dvfs_mif, TABLE VERSION 1, ASV group 0.
 * Each column matches shark_mif_freq_table[] in descending order.
 */
static const unsigned int shark_mif_asv_volt_table
	[SHARK_MIF_ASV_GROUP_COUNT][SHARK_MIF_DVFS_LEVEL_COUNT] = {
	{ 918750, 906250, 893750, 862500, 837500, 800000,
	  781250, 756250, 700000, 725000, 712500, 700000 },
};

unsigned int shark_g3d_get_clamped_asv_group(unsigned int asv_group)
{
	if (asv_group >= SHARK_G3D_ASV_GROUP_COUNT)
		return SHARK_G3D_DEFAULT_ASV_GROUP;

	return asv_group;
}
EXPORT_SYMBOL_GPL(shark_g3d_get_clamped_asv_group);

unsigned long shark_g3d_get_max_freq(void)
{
	return shark_g3d_freq_table[0];
}
EXPORT_SYMBOL_GPL(shark_g3d_get_max_freq);

unsigned long shark_g3d_get_min_freq(void)
{
	return shark_g3d_freq_table[SHARK_G3D_DVFS_LEVEL_COUNT - 1];
}
EXPORT_SYMBOL_GPL(shark_g3d_get_min_freq);

unsigned long shark_g3d_get_freq(unsigned int level)
{
	if (level >= SHARK_G3D_DVFS_LEVEL_COUNT)
		return shark_g3d_get_min_freq();

	return shark_g3d_freq_table[level];
}
EXPORT_SYMBOL_GPL(shark_g3d_get_freq);

unsigned int shark_g3d_get_default_volt(unsigned int asv_group,
					unsigned int level)
{
	asv_group = shark_g3d_get_clamped_asv_group(asv_group);

	if (level >= SHARK_G3D_DVFS_LEVEL_COUNT)
		level = SHARK_G3D_DVFS_LEVEL_COUNT - 1;

	return shark_g3d_asv_volt_table[asv_group][level];
}
EXPORT_SYMBOL_GPL(shark_g3d_get_default_volt);

int shark_g3d_get_level_from_freq(unsigned long freq)
{
	unsigned int level;

	for (level = 0; level < SHARK_G3D_DVFS_LEVEL_COUNT; level++) {
		if (shark_g3d_freq_table[level] == freq)
			return level;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(shark_g3d_get_level_from_freq);

unsigned long shark_g3d_snap_freq(unsigned long freq)
{
	unsigned int i;
	unsigned long best_rate = shark_g3d_get_freq(0);
	unsigned long best_delta = (best_rate > freq) ?
		(best_rate - freq) : (freq - best_rate);

	for (i = 1; i < SHARK_G3D_DVFS_LEVEL_COUNT; i++) {
		unsigned long rate = shark_g3d_get_freq(i);
		unsigned long delta = (rate > freq) ?
			(rate - freq) : (freq - rate);

		if (delta < best_delta) {
			best_delta = delta;
			best_rate = rate;
		}
	}

	return best_rate;
}
EXPORT_SYMBOL_GPL(shark_g3d_snap_freq);

unsigned long shark_mif_get_max_freq(void)
{
	return shark_mif_freq_table[0];
}
EXPORT_SYMBOL_GPL(shark_mif_get_max_freq);

unsigned long shark_mif_get_min_freq(void)
{
	return shark_mif_freq_table[SHARK_MIF_DVFS_LEVEL_COUNT - 1];
}
EXPORT_SYMBOL_GPL(shark_mif_get_min_freq);

unsigned long shark_mif_get_freq(unsigned int level)
{
	if (level >= SHARK_MIF_DVFS_LEVEL_COUNT)
		return shark_mif_get_min_freq();

	return shark_mif_freq_table[level];
}
EXPORT_SYMBOL_GPL(shark_mif_get_freq);

unsigned long shark_mif_get_boot_freq(void)
{
	return shark_mif_get_freq(SHARK_MIF_DEFAULT_BOOT_LEVEL);
}
EXPORT_SYMBOL_GPL(shark_mif_get_boot_freq);

unsigned long shark_mif_get_resume_freq(void)
{
	return shark_mif_get_freq(SHARK_MIF_DEFAULT_RESUME_LEVEL);
}
EXPORT_SYMBOL_GPL(shark_mif_get_resume_freq);

unsigned int shark_mif_get_clamped_asv_group(unsigned int asv_group)
{
	if (asv_group >= SHARK_MIF_ASV_GROUP_COUNT)
		return SHARK_MIF_DEFAULT_ASV_GROUP;

	return asv_group;
}
EXPORT_SYMBOL_GPL(shark_mif_get_clamped_asv_group);

unsigned int shark_mif_get_default_volt(unsigned int asv_group,
					unsigned int level)
{
	asv_group = shark_mif_get_clamped_asv_group(asv_group);

	if (level >= SHARK_MIF_DVFS_LEVEL_COUNT)
		level = SHARK_MIF_DVFS_LEVEL_COUNT - 1;

	return shark_mif_asv_volt_table[asv_group][level];
}
EXPORT_SYMBOL_GPL(shark_mif_get_default_volt);

unsigned int shark_mif_get_interpolated_volt(unsigned long freq)
{
	unsigned int i;

	if (freq >= shark_mif_get_freq(0))
		return shark_mif_get_default_volt(SHARK_MIF_DEFAULT_ASV_GROUP, 0);

	if (freq <= shark_mif_get_freq(SHARK_MIF_DVFS_LEVEL_COUNT - 1))
		return shark_mif_get_default_volt(SHARK_MIF_DEFAULT_ASV_GROUP,
						 SHARK_MIF_DVFS_LEVEL_COUNT - 1);

	for (i = 1; i < SHARK_MIF_DVFS_LEVEL_COUNT; i++) {
		unsigned long rate_up = shark_mif_get_freq(i - 1);
		unsigned long rate_down = shark_mif_get_freq(i);
		unsigned int volt_up = shark_mif_get_default_volt(
			SHARK_MIF_DEFAULT_ASV_GROUP, i - 1);
		unsigned int volt_down = shark_mif_get_default_volt(
			SHARK_MIF_DEFAULT_ASV_GROUP, i);

		if (freq == rate_up)
			return volt_up;

		if (freq == rate_down)
			return volt_down;

		if (freq < rate_up && freq > rate_down) {
			unsigned long rate_span = rate_up - rate_down;
			unsigned long rate_delta = rate_up - freq;

			/*
			 * Some Exynos 8895 ECT voltage points are not strictly
			 * monotonic. Handle both interpolation directions instead
			 * of relying on unsigned subtraction.
			 */
			if (volt_up >= volt_down) {
				unsigned int volt_span = volt_up - volt_down;

				return volt_up - div_u64((u64)volt_span * rate_delta,
							 rate_span);
			} else {
				unsigned int volt_span = volt_down - volt_up;

				return volt_up + div_u64((u64)volt_span * rate_delta,
							 rate_span);
			}
		}
	}

	return shark_mif_get_default_volt(SHARK_MIF_DEFAULT_ASV_GROUP,
					 SHARK_MIF_DVFS_LEVEL_COUNT - 1);
}
EXPORT_SYMBOL_GPL(shark_mif_get_interpolated_volt);

unsigned long shark_mif_snap_freq(unsigned long freq)
{
	unsigned int i;
	unsigned long best_rate = shark_mif_get_freq(0);
	unsigned long best_delta = (best_rate > freq) ?
		(best_rate - freq) : (freq - best_rate);

	for (i = 1; i < SHARK_MIF_DVFS_LEVEL_COUNT; i++) {
		unsigned long rate = shark_mif_get_freq(i);
		unsigned long delta = (rate > freq) ?
			(rate - freq) : (freq - rate);

		if (delta < best_delta) {
			best_delta = delta;
			best_rate = rate;
		}
	}

	return best_rate;
}
EXPORT_SYMBOL_GPL(shark_mif_snap_freq);
