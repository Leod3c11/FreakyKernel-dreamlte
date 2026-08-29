/* SPDX-License-Identifier: GPL-2.0 */
/* Shark custom DVFS authority for Exynos8895. */
#ifndef __SOC_SAMSUNG_EXYNOS_SOC_INTERFACE_H__
#define __SOC_SAMSUNG_EXYNOS_SOC_INTERFACE_H__

#include <linux/types.h>

#define SHARK_SOC_DOMAIN_ID_ANY		(~0U)
#define SHARK_SOC_MAX_DOMAIN_OPPS	24U

enum shark_soc_catalog_kind {
	SHARK_SOC_CATALOG_ANY = 0,
	SHARK_SOC_CATALOG_PLL_RATES,
	SHARK_SOC_CATALOG_DVFS_LEVELS,
	SHARK_SOC_CATALOG_DVFS_ENABLED,
	SHARK_SOC_CATALOG_DVFS_SFR,
	SHARK_SOC_CATALOG_DVFS_PARAMS,
	SHARK_SOC_CATALOG_ASV_FREQS,
	SHARK_SOC_CATALOG_ASV_TABLE,
	SHARK_SOC_CATALOG_GEN_TABLE,
	SHARK_SOC_CATALOG_NEWTIME_TABLE,
	SHARK_SOC_CATALOG_MINLOCK_TABLE,
	SHARK_SOC_CATALOG_THERMAL_RANGES,
};

struct shark_soc_catalog_table {
	const char *block;
	const char *name;
	const char *subname;
	unsigned int kind;
	unsigned int rows;
	unsigned int cols;
	unsigned int aux0;
	unsigned int aux1;
	const u64 *data;
};

unsigned int shark_soc_catalog_get_count(void);
const struct shark_soc_catalog_table *
shark_soc_catalog_get_table(unsigned int index);
const struct shark_soc_catalog_table *
shark_soc_catalog_find(const char *block, const char *name,
		       const char *subname, unsigned int kind);
bool shark_soc_is_enabled(void);

/* Called synchronously after an ECT block has been parsed. */
void shark_soc_ect_block_post_parse(const char *block_name, void *handle);

/* Runtime access to the authoritative per-domain table. */
int shark_soc_get_domain_table(const char *name, unsigned int domain_id,
			       unsigned int *rates_khz,
			       unsigned int *volts_uv,
			       unsigned int capacity,
			       unsigned int *count);

int shark_soc_get_domain_rate_table(const char *name, unsigned int domain_id,
				    unsigned int *rates_khz,
				    unsigned int capacity,
				    unsigned int *count);
int shark_soc_get_domain_max_freq(const char *name, unsigned int domain_id,
				  unsigned int *rate_khz);
int shark_soc_get_domain_min_freq(const char *name, unsigned int domain_id,
				  unsigned int *rate_khz);
int shark_soc_get_domain_boot_freq(const char *name, unsigned int domain_id,
				   unsigned int *rate_khz);
int shark_soc_get_domain_resume_freq(const char *name,
				     unsigned int domain_id,
				     unsigned int *rate_khz);

/*
 * Bind the effective VCLK policy after ECT/ASV selection.  The rate labels
 * must exactly match the static Shark domain; otherwise the binding is
 * rejected and callers keep using the Samsung VCLK metadata.
 */
int shark_soc_bind_clock_domain(const char *name, unsigned int domain_id,
				unsigned int count,
				const unsigned int *rates_khz,
				unsigned int max_freq_khz,
				unsigned int min_freq_khz,
				unsigned int boot_freq_khz,
				unsigned int resume_freq_khz);

/* Round and clamp a request to a published Shark domain rate. */
int shark_soc_resolve_rate(const char *name, unsigned int domain_id,
			   unsigned int requested_khz,
			   unsigned int *resolved_khz);

/*
 * Merge the static Shark rate authority with the live, chip-binned FVMap
 * voltages. Returns > 0 when output should replace the FVMap table.
 */
int shark_soc_fvmap_prepare(const char *name, unsigned int domain_id,
			    unsigned int count,
			    const unsigned int *live_rates_khz,
			    const unsigned int *live_volts_uv,
			    unsigned int *rates_khz,
			    unsigned int *volts_uv);

#endif /* __SOC_SAMSUNG_EXYNOS_SOC_INTERFACE_H__ */
