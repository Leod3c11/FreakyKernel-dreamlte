/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Shark custom DVFS/SoC authority for Exynos8895.
 *
 * This header intentionally exposes declarations only.  All frequency,
 * voltage, PLL and ECT backing tables live in
 * drivers/soc/samsung/exynos-soc_interface.c.
 */
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

struct shark_soc_devfreq_policy {
	unsigned int initial_freq;
	unsigned int default_qos;
	unsigned int suspend_freq;
	unsigned int min_freq;
	unsigned int max_freq;
	unsigned int reboot_freq;
	unsigned int boot_freq;
};

struct shark_soc_cpu_policy {
	unsigned int boot_qos;
	unsigned int jig_boot_qos;
	unsigned int user_default_qos;
};

struct shark_soc_gpu_dvfs_entry {
	unsigned int clock;
	unsigned int min_threshold;
	unsigned int max_threshold;
	unsigned int down_staycount;
	unsigned int mif_freq;
	unsigned int cpu_little_min_freq;
	unsigned int cpu_big_max_freq;
};

struct shark_soc_mfc_qos_frequency {
	unsigned int mfc_freq;
	unsigned int int_freq;
	unsigned int mif_freq;
	unsigned int cpu_freq;
	unsigned int kfc_freq;
};

struct shark_soc_input_boost_frequency {
	unsigned int cpu_freq;
	unsigned int kfc_freq;
	unsigned int mif_freq;
	unsigned int int_freq;
};

struct shark_soc_camera_qos_policy {
	unsigned int int_cam_freq;
	unsigned int int_freq;
	unsigned int cam_freq;
	unsigned int mif_freq;
	unsigned int i2c_level;
	unsigned int hpg;
};

struct shark_soc_g2d_qos_frequency {
	unsigned int cpucl0_freq;
	unsigned int cpucl1_freq;
	unsigned int mif_freq;
};

struct shark_soc_argos_frequency {
	unsigned int cpu_min_freq;
	unsigned int cpu_max_freq;
	unsigned int kfc_min_freq;
	unsigned int kfc_max_freq;
	unsigned int mif_freq;
	unsigned int int_freq;
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

/* Platform frequency policies formerly supplied by Exynos8895 DTS tables. */
int shark_soc_get_devfreq_policy(const char *name, unsigned int domain_id,
				 struct shark_soc_devfreq_policy *policy);
int shark_soc_get_cpu_policy(const char *name, unsigned int domain_id,
			     struct shark_soc_cpu_policy *policy);
int shark_soc_get_bigturbo_table(unsigned int asv_version,
				 unsigned int *rates_khz,
				 unsigned int capacity,
				 unsigned int *count);
int shark_soc_get_gpu_dvfs_table(struct shark_soc_gpu_dvfs_entry *table,
				 unsigned int capacity,
				 unsigned int *count);
int shark_soc_get_gpu_thermal_table(unsigned int *rates_khz,
				    unsigned int capacity,
				    unsigned int *count);
int shark_soc_get_mfc_frequency_policy(unsigned int *clock_rate_hz,
				       unsigned int *min_rate_hz,
				       struct shark_soc_mfc_qos_frequency *table,
				       unsigned int capacity,
				       unsigned int *count);
int shark_soc_get_input_boost_frequency(
	struct shark_soc_input_boost_frequency *policy);
int shark_soc_get_hmp_softlanding_frequencies(unsigned int *rates_khz,
					      unsigned int capacity,
					      unsigned int *count);
int shark_soc_get_schedtune_freqvar_table(unsigned int cpu,
					  unsigned int *table,
					  unsigned int capacity,
					  unsigned int *count);
int shark_soc_get_camera_qos_dimensions(unsigned int *table_count,
					unsigned int *scenario_count);
int shark_soc_get_camera_qos_policy(
	unsigned int table_index, unsigned int scenario_index,
	struct shark_soc_camera_qos_policy *policy);
int shark_soc_get_g2d_qos_table(struct shark_soc_g2d_qos_frequency *table,
				unsigned int capacity,
				unsigned int *count);
int shark_soc_get_client_qos_table(const char *client,
				   unsigned int *rates_khz,
				   unsigned int capacity,
				   unsigned int *count);
int shark_soc_get_argos_frequency_table(
	const char *label, struct shark_soc_argos_frequency *table,
	unsigned int capacity, unsigned int *count);
int shark_soc_get_nad_frequency_table(const char *label,
				      unsigned int *rates_khz,
				      unsigned int capacity,
				      unsigned int *count);
int shark_soc_get_dm_constraint_table(const char *name,
				      unsigned int domain_id,
				      unsigned int *master_rates_khz,
				      unsigned int *constraint_rates_khz,
				      unsigned int capacity,
				      unsigned int *count);
int shark_soc_get_ufc_table(unsigned int ctrl_type,
			    unsigned int execution_mode,
			    unsigned int *master_rates_khz,
			    unsigned int *limit_rates_khz,
			    unsigned int capacity,
			    unsigned int *count);

/*
 * Bind the effective VCLK policy after ECT/ASV selection.  The rate labels
 * must exactly match the static Shark domain; a mismatch rejects CAL
 * initialization instead of enabling a secondary frequency authority.
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
