/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Shark custom DVFS authority for Exynos8895.
 *
 * The stock ECT image is parsed into writable kernel mappings.  Shark checks
 * every table shape before publishing the catalog generated from all_dump, so
 * an incompatible firmware image is left untouched.  FVMap voltages are not
 * selected from an arbitrary ASV group: the live, fuse-selected voltage table
 * remains the source and is merged with Shark's authoritative rate table.
 */

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/string.h>

#include <soc/samsung/ect_parser.h>
#include <soc/samsung/exynos-soc-interface.h>

#include "exynos8895-shark-data.h"

#define SHARK_MAX_DOMAINS	10U
#define SHARK_MAX_ECT_BLOCKS	7U
#define SHARK_MIN_VOLT_UV	400000U
#define SHARK_MAX_VOLT_UV	1500000U
#define SHARK_VOLT_STEP_UV	6250U
#define SHARK_RATE_TOLERANCE_KHZ	1000U

struct shark_domain {
	const char *name;
	unsigned int id;
	unsigned int count;
	unsigned int boot_level;
	unsigned int resume_level;
	unsigned int max_freq;
	unsigned int min_freq;
	unsigned int boot_freq;
	unsigned int resume_freq;
	unsigned int rates[SHARK_SOC_MAX_DOMAIN_OPPS];
	unsigned int volts[SHARK_SOC_MAX_DOMAIN_OPPS];
	bool vclk_bound;
	bool fvmap_bound;
};

struct shark_ect_backend {
	const char *name;
	void *handle;
	int publish_result;
};

struct shark_voltage_floor {
	const char *name;
	unsigned int rate;
	unsigned int voltage;
};

static const struct shark_voltage_floor shark_voltage_floors[] = {
	{ "dvfs_cpucl1", 1898000U, 1200000U },
	{ "dvfs_cpucl1", 2002000U, 1300000U },
	{ "dvfs_cpucl0", 2652000U, 1150000U },
	{ "dvfs_cpucl0", 2704000U, 1175000U },
	{ "dvfs_cpucl0", 2808000U, 1400000U },
};

static DEFINE_MUTEX(shark_lock);
static struct shark_domain shark_domains[SHARK_MAX_DOMAINS];
static struct shark_ect_backend shark_backends[SHARK_MAX_ECT_BLOCKS];
static unsigned int shark_domain_count;
static unsigned int shark_backend_count;
static bool shark_registry_ready;
static bool shark_enabled = true;

module_param_named(enabled, shark_enabled, bool, 0444);
MODULE_PARM_DESC(enabled, "Enable Shark static ECT and FVMap authority");

bool shark_soc_is_enabled(void)
{
	return shark_enabled;
}
EXPORT_SYMBOL_GPL(shark_soc_is_enabled);

unsigned int shark_soc_catalog_get_count(void)
{
	return SHARK_SOC_STATIC_CATALOG_COUNT;
}
EXPORT_SYMBOL_GPL(shark_soc_catalog_get_count);

const struct shark_soc_catalog_table *
shark_soc_catalog_get_table(unsigned int index)
{
	if (index >= SHARK_SOC_STATIC_CATALOG_COUNT)
		return NULL;

	return &shark_soc_static_catalog[index];
}
EXPORT_SYMBOL_GPL(shark_soc_catalog_get_table);

const struct shark_soc_catalog_table *
shark_soc_catalog_find(const char *block, const char *name,
		       const char *subname, unsigned int kind)
{
	unsigned int i;

	for (i = 0; i < SHARK_SOC_STATIC_CATALOG_COUNT; i++) {
		const struct shark_soc_catalog_table *table;

		table = &shark_soc_static_catalog[i];
		if (block && strcmp(block, table->block))
			continue;
		if (name && strcmp(name, table->name))
			continue;
		if (subname && strcmp(subname, table->subname))
			continue;
		if (kind != SHARK_SOC_CATALOG_ANY && kind != table->kind)
			continue;
		return table;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(shark_soc_catalog_find);

static unsigned int shark_normalize_rate(const char *name, unsigned int rate)
{
	/* The Exynos8895 G3D PLL produces 838 MHz; 839 MHz is an ECT label. */
	if (!strcmp(name, "dvfs_g3d") && rate == 839000U)
		return 838000U;

	return rate;
}

static const char *shark_canonical_domain_name(const char *name)
{
	if (!name)
		return NULL;
	if (!strcmp(name, "dvfs_big"))
		return "dvfs_cpucl0";
	if (!strcmp(name, "dvfs_little"))
		return "dvfs_cpucl1";
	return name;
}

static int shark_voltage_valid(unsigned int voltage)
{
	return voltage >= SHARK_MIN_VOLT_UV &&
	       voltage <= SHARK_MAX_VOLT_UV &&
	       !(voltage % SHARK_VOLT_STEP_UV);
}

static unsigned int shark_apply_voltage_floor(const char *name,
					       unsigned int rate,
					       unsigned int voltage)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(shark_voltage_floors); i++) {
		const struct shark_voltage_floor *floor;

		floor = &shark_voltage_floors[i];
		if (!strcmp(name, floor->name) && rate == floor->rate &&
		    voltage < floor->voltage)
			return floor->voltage;
	}

	return voltage;
}

static void shark_registry_init(void)
{
	unsigned int i;

	if (shark_registry_ready)
		return;

	for (i = 0; i < SHARK_SOC_STATIC_CATALOG_COUNT; i++) {
		const struct shark_soc_catalog_table *table;
		struct shark_domain *domain;
		unsigned int level;

		table = &shark_soc_static_catalog[i];
		if (table->kind != SHARK_SOC_CATALOG_DVFS_LEVELS)
			continue;
		if (shark_domain_count >= ARRAY_SIZE(shark_domains) ||
		    table->cols > SHARK_SOC_MAX_DOMAIN_OPPS)
			break;

		domain = &shark_domains[shark_domain_count++];
		domain->name = table->name;
		domain->id = SHARK_SOC_DOMAIN_ID_ANY;
		domain->count = table->cols;
		domain->boot_level = table->aux0;
		domain->resume_level = table->aux1;
		for (level = 0; level < domain->count; level++)
			domain->rates[level] = shark_normalize_rate(
				domain->name, (unsigned int)table->data[level]);
		domain->max_freq = domain->rates[0];
		domain->min_freq = domain->rates[domain->count - 1];
		if (domain->boot_level < domain->count)
			domain->boot_freq = domain->rates[domain->boot_level];
		if (domain->resume_level < domain->count)
			domain->resume_freq =
				domain->rates[domain->resume_level];
	}

	shark_registry_ready = true;
}

static struct shark_domain *shark_find_domain(const char *name,
					       unsigned int domain_id)
{
	const char *canonical_name = shark_canonical_domain_name(name);
	unsigned int i;

	shark_registry_init();
	for (i = 0; i < shark_domain_count; i++) {
		struct shark_domain *domain = &shark_domains[i];

		if (canonical_name && !strcmp(canonical_name, domain->name))
			return domain;
		if (domain_id != SHARK_SOC_DOMAIN_ID_ANY &&
		    domain->id == domain_id)
			return domain;
	}

	return NULL;
}

static unsigned int shark_domain_rate(const char *name, unsigned int level,
				      unsigned int fallback)
{
	struct shark_domain *domain;

	domain = shark_find_domain(name, SHARK_SOC_DOMAIN_ID_ANY);
	if (!domain || level >= domain->count)
		return shark_normalize_rate(name, fallback);

	return domain->rates[level];
}

static bool shark_domain_has_rate(const struct shark_domain *domain,
				  unsigned int rate)
{
	unsigned int i;

	if (!rate || rate == ~0U)
		return false;

	for (i = 0; i < domain->count; i++)
		if (domain->rates[i] == rate)
			return true;

	return false;
}

int shark_soc_get_domain_table(const char *name, unsigned int domain_id,
			       unsigned int *rates_khz,
			       unsigned int *volts_uv,
			       unsigned int capacity,
			       unsigned int *count)
{
	struct shark_domain *domain;
	unsigned int i;

	if (!count)
		return -EINVAL;
	if (!shark_enabled)
		return -ENODEV;

	mutex_lock(&shark_lock);
	domain = shark_find_domain(name, domain_id);
	if (!domain) {
		mutex_unlock(&shark_lock);
		return -ENOENT;
	}
	*count = domain->count;
	if (capacity < domain->count) {
		mutex_unlock(&shark_lock);
		return -ENOSPC;
	}
	for (i = 0; i < domain->count; i++) {
		if (rates_khz)
			rates_khz[i] = domain->rates[i];
		if (volts_uv)
			volts_uv[i] = domain->volts[i];
	}
	mutex_unlock(&shark_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(shark_soc_get_domain_table);

int shark_soc_get_domain_rate_table(const char *name, unsigned int domain_id,
				    unsigned int *rates_khz,
				    unsigned int capacity,
				    unsigned int *count)
{
	return shark_soc_get_domain_table(name, domain_id, rates_khz, NULL,
					  capacity, count);
}
EXPORT_SYMBOL_GPL(shark_soc_get_domain_rate_table);

static int shark_soc_get_domain_policy(const char *name,
				       unsigned int domain_id,
				       unsigned int *rate_khz,
				       unsigned int policy)
{
	struct shark_domain *domain;
	unsigned int rate;

	if (!rate_khz)
		return -EINVAL;
	if (!shark_enabled)
		return -ENODEV;

	mutex_lock(&shark_lock);
	domain = shark_find_domain(name, domain_id);
	if (!domain) {
		mutex_unlock(&shark_lock);
		return -ENOENT;
	}

	switch (policy) {
	case 0:
		rate = domain->max_freq;
		break;
	case 1:
		rate = domain->min_freq;
		break;
	case 2:
		rate = domain->boot_freq;
		break;
	case 3:
		rate = domain->resume_freq;
		break;
	default:
		mutex_unlock(&shark_lock);
		return -EINVAL;
	}
	mutex_unlock(&shark_lock);

	if (!rate)
		return -ENODATA;
	*rate_khz = rate;
	return 0;
}

int shark_soc_get_domain_max_freq(const char *name, unsigned int domain_id,
				  unsigned int *rate_khz)
{
	return shark_soc_get_domain_policy(name, domain_id, rate_khz, 0);
}
EXPORT_SYMBOL_GPL(shark_soc_get_domain_max_freq);

int shark_soc_get_domain_min_freq(const char *name, unsigned int domain_id,
				  unsigned int *rate_khz)
{
	return shark_soc_get_domain_policy(name, domain_id, rate_khz, 1);
}
EXPORT_SYMBOL_GPL(shark_soc_get_domain_min_freq);

int shark_soc_get_domain_boot_freq(const char *name, unsigned int domain_id,
				   unsigned int *rate_khz)
{
	return shark_soc_get_domain_policy(name, domain_id, rate_khz, 2);
}
EXPORT_SYMBOL_GPL(shark_soc_get_domain_boot_freq);

int shark_soc_get_domain_resume_freq(const char *name,
				     unsigned int domain_id,
				     unsigned int *rate_khz)
{
	return shark_soc_get_domain_policy(name, domain_id, rate_khz, 3);
}
EXPORT_SYMBOL_GPL(shark_soc_get_domain_resume_freq);

int shark_soc_bind_clock_domain(const char *name, unsigned int domain_id,
				unsigned int count,
				const unsigned int *rates_khz,
				unsigned int max_freq_khz,
				unsigned int min_freq_khz,
				unsigned int boot_freq_khz,
				unsigned int resume_freq_khz)
{
	struct shark_domain *domain;
	unsigned int i;
	int ret = 0;

	if (!shark_enabled)
		return -ENODEV;
	if (!name || !rates_khz || !count)
		return -EINVAL;

	mutex_lock(&shark_lock);
	domain = shark_find_domain(name, domain_id);
	if (!domain || domain->count != count) {
		ret = -EINVAL;
		goto out;
	}
	for (i = 0; i < count; i++) {
		if (rates_khz[i] != domain->rates[i]) {
			ret = -EINVAL;
			goto out;
		}
	}

	domain->id = domain_id;
	domain->max_freq = shark_domain_has_rate(domain, max_freq_khz) ?
		max_freq_khz : domain->rates[0];
	domain->min_freq = shark_domain_has_rate(domain, min_freq_khz) ?
		min_freq_khz : domain->rates[domain->count - 1];
	if (shark_domain_has_rate(domain, boot_freq_khz))
		domain->boot_freq = boot_freq_khz;
	if (shark_domain_has_rate(domain, resume_freq_khz))
		domain->resume_freq = resume_freq_khz;
	domain->vclk_bound = true;
out:
	mutex_unlock(&shark_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(shark_soc_bind_clock_domain);

int shark_soc_resolve_rate(const char *name, unsigned int domain_id,
			   unsigned int requested_khz,
			   unsigned int *resolved_khz)
{
	struct shark_domain *domain;
	unsigned int requested;
	unsigned int i;

	if (!resolved_khz || !requested_khz)
		return -EINVAL;
	if (!shark_enabled)
		return -ENODEV;

	mutex_lock(&shark_lock);
	domain = shark_find_domain(name, domain_id);
	if (!domain) {
		mutex_unlock(&shark_lock);
		return -ENOENT;
	}
	requested = shark_normalize_rate(domain->name, requested_khz);
	for (i = 0; i < domain->count; i++)
		if (requested >= domain->rates[i])
			break;
	if (i == domain->count)
		i = domain->count - 1;
	*resolved_khz = domain->rates[i];
	mutex_unlock(&shark_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(shark_soc_resolve_rate);

static int shark_find_live_voltage(unsigned int wanted_rate,
				   unsigned int count,
				   const unsigned int *live_rates,
				   const unsigned int *live_volts,
				   unsigned int *voltage)
{
	unsigned int i;
	unsigned int best = ~0U;
	unsigned int best_delta = ~0U;

	for (i = 0; i < count; i++) {
		unsigned int delta;

		if (live_rates[i] == wanted_rate) {
			*voltage = live_volts[i];
			return 0;
		}
		if (live_rates[i] > wanted_rate)
			delta = live_rates[i] - wanted_rate;
		else
			delta = wanted_rate - live_rates[i];
		if (delta < best_delta) {
			best_delta = delta;
			best = i;
		}
	}
	if (best == ~0U || best_delta > SHARK_RATE_TOLERANCE_KHZ)
		return -ENOENT;

	*voltage = live_volts[best];
	return 0;
}

int shark_soc_fvmap_prepare(const char *name, unsigned int domain_id,
			    unsigned int count,
			    const unsigned int *live_rates_khz,
			    const unsigned int *live_volts_uv,
			    unsigned int *rates_khz,
			    unsigned int *volts_uv)
{
	struct shark_domain *domain;
	unsigned int candidate_rates[SHARK_SOC_MAX_DOMAIN_OPPS];
	unsigned int candidate_volts[SHARK_SOC_MAX_DOMAIN_OPPS];
	unsigned int i;
	int ret = 0;

	if (!shark_enabled)
		return 0;
	if (!name || !live_rates_khz || !live_volts_uv || !rates_khz ||
	    !volts_uv || !count || count > SHARK_SOC_MAX_DOMAIN_OPPS)
		return -EINVAL;

	mutex_lock(&shark_lock);
	domain = shark_find_domain(name, domain_id);
	if (!domain || domain->count != count) {
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < count; i++) {
		candidate_rates[i] = domain->rates[i];
		ret = shark_find_live_voltage(candidate_rates[i], count,
			live_rates_khz, live_volts_uv, &candidate_volts[i]);
		if (ret)
			goto out;
		candidate_volts[i] = shark_apply_voltage_floor(
			name, candidate_rates[i], candidate_volts[i]);
		if (!shark_voltage_valid(candidate_volts[i])) {
			ret = -ERANGE;
			goto out;
		}
		if (i && candidate_rates[i] >= candidate_rates[i - 1]) {
			ret = -ERANGE;
			goto out;
		}
	}

	for (i = 0; i < count; i++) {
		rates_khz[i] = candidate_rates[i];
		volts_uv[i] = candidate_volts[i];
		domain->volts[i] = candidate_volts[i];
	}
	domain->id = domain_id;
	domain->fvmap_bound = true;
	ret = 1;
out:
	mutex_unlock(&shark_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(shark_soc_fvmap_prepare);

static unsigned int shark_catalog_kind_count(unsigned int kind)
{
	unsigned int i;
	unsigned int count = 0;

	for (i = 0; i < SHARK_SOC_STATIC_CATALOG_COUNT; i++)
		if (shark_soc_static_catalog[i].kind == kind)
			count++;
	return count;
}

static int shark_publish_pll(struct ect_pll_header *header)
{
	unsigned int i, j;

	if (!header || header->num_of_pll !=
	    shark_catalog_kind_count(SHARK_SOC_CATALOG_PLL_RATES))
		return -EINVAL;
	for (i = 0; i < header->num_of_pll; i++) {
		struct ect_pll *pll = &header->pll_list[i];
		const struct shark_soc_catalog_table *table;

		table = shark_soc_catalog_find("PLL", pll->pll_name, "rates",
			SHARK_SOC_CATALOG_PLL_RATES);
		if (!table || table->rows != pll->num_of_frequency ||
		    table->cols != 5U)
			return -EINVAL;
	}
	for (i = 0; i < header->num_of_pll; i++) {
		struct ect_pll *pll = &header->pll_list[i];
		const struct shark_soc_catalog_table *table;

		table = shark_soc_catalog_find("PLL", pll->pll_name, "rates",
			SHARK_SOC_CATALOG_PLL_RATES);
		pll->type_pll = table->aux0;
		for (j = 0; j < table->rows; j++) {
			struct ect_pll_frequency *rate;
			const u64 *data = &table->data[j * 5U];

			rate = &pll->frequency_list[j];
			rate->frequency = (unsigned int)data[0];
			rate->p = (unsigned int)data[1];
			rate->m = (unsigned int)data[2];
			rate->s = (unsigned int)data[3];
			rate->k = (unsigned int)data[4];
		}
	}
	return 0;
}

static int shark_validate_dvfs_domain(struct ect_dvfs_domain *domain)
{
	const struct shark_soc_catalog_table *levels;
	const struct shark_soc_catalog_table *enabled;
	const struct shark_soc_catalog_table *sfr;
	const struct shark_soc_catalog_table *params;

	levels = shark_soc_catalog_find("DVFS", domain->domain_name, "levels",
		SHARK_SOC_CATALOG_DVFS_LEVELS);
	enabled = shark_soc_catalog_find("DVFS", domain->domain_name, "enabled",
		SHARK_SOC_CATALOG_DVFS_ENABLED);
	sfr = shark_soc_catalog_find("DVFS", domain->domain_name, "sfr",
		SHARK_SOC_CATALOG_DVFS_SFR);
	params = shark_soc_catalog_find("DVFS", domain->domain_name, "params",
		SHARK_SOC_CATALOG_DVFS_PARAMS);
	if (!levels || !enabled || !sfr || !params ||
	    levels->cols != domain->num_of_level ||
	    enabled->cols != domain->num_of_level ||
	    sfr->cols != domain->num_of_clock ||
	    params->rows != domain->num_of_level ||
	    params->cols != domain->num_of_clock ||
	    domain->mode != e_dvfs_mode_sfr_address || !domain->list_sfr)
		return -EINVAL;
	return 0;
}

static int shark_publish_dvfs(struct ect_dvfs_header *header)
{
	unsigned int i, j;

	if (!header || header->num_of_domain !=
	    shark_catalog_kind_count(SHARK_SOC_CATALOG_DVFS_LEVELS))
		return -EINVAL;
	for (i = 0; i < header->num_of_domain; i++)
		if (shark_validate_dvfs_domain(&header->domain_list[i]))
			return -EINVAL;

	for (i = 0; i < header->num_of_domain; i++) {
		struct ect_dvfs_domain *domain = &header->domain_list[i];
		const struct shark_soc_catalog_table *levels;
		const struct shark_soc_catalog_table *enabled;
		const struct shark_soc_catalog_table *sfr;
		const struct shark_soc_catalog_table *params;

		levels = shark_soc_catalog_find("DVFS", domain->domain_name,
			"levels",
			SHARK_SOC_CATALOG_DVFS_LEVELS);
		enabled = shark_soc_catalog_find("DVFS", domain->domain_name,
			"enabled",
			SHARK_SOC_CATALOG_DVFS_ENABLED);
		sfr = shark_soc_catalog_find("DVFS", domain->domain_name, "sfr",
			SHARK_SOC_CATALOG_DVFS_SFR);
		params = shark_soc_catalog_find("DVFS", domain->domain_name,
			"params",
			SHARK_SOC_CATALOG_DVFS_PARAMS);
		domain->boot_level_idx = (int)levels->aux0;
		domain->resume_level_idx = (int)levels->aux1;
		domain->max_frequency = enabled->aux0;
		domain->min_frequency = enabled->aux1;
		for (j = 0; j < domain->num_of_level; j++) {
			domain->list_level[j].level = shark_domain_rate(
				domain->domain_name, j,
				(unsigned int)levels->data[j]);
			domain->list_level[j].level_en = (int)enabled->data[j];
		}
		for (j = 0; j < domain->num_of_clock; j++)
			domain->list_sfr[j] = (unsigned int)sfr->data[j];
		for (j = 0; j < params->rows * params->cols; j++)
			domain->list_dvfs_value[j] =
				(unsigned int)params->data[j];
	}
	return 0;
}

static const struct shark_soc_catalog_table *
shark_find_asv_table(const char *domain_name, int version)
{
	char subname[24];

	snprintf(subname, sizeof(subname), "table_v%d", version);
	return shark_soc_catalog_find("ASV", domain_name, subname,
		SHARK_SOC_CATALOG_ASV_TABLE);
}

static int shark_validate_asv_domain(struct ect_voltage_domain *domain)
{
	const struct shark_soc_catalog_table *freqs;
	unsigned int i;

	freqs = shark_soc_catalog_find("ASV", domain->domain_name,
		"frequencies",
		SHARK_SOC_CATALOG_ASV_FREQS);
	if (!freqs || freqs->cols != domain->num_of_level ||
	    freqs->aux0 != domain->num_of_group ||
	    freqs->aux1 != domain->num_of_table)
		return -EINVAL;
	for (i = 0; i < domain->num_of_table; i++) {
		const struct ect_voltage_table *live = &domain->table_list[i];
		const struct shark_soc_catalog_table *table;
		unsigned int cell;

		table = shark_find_asv_table(domain->domain_name,
			live->table_version);
		if (!table || table->rows != domain->num_of_level ||
		    table->cols != domain->num_of_group ||
		    (!live->voltages && !live->voltages_step))
			return -EINVAL;
		if (live->voltages_step)
			for (cell = 0; cell < table->rows * table->cols; cell++)
				if (table->data[cell] % SHARK_VOLT_STEP_UV ||
				    table->data[cell] / SHARK_VOLT_STEP_UV >
				    255U)
					return -ERANGE;
	}
	return 0;
}

static int shark_publish_asv(struct ect_voltage_header *header)
{
	unsigned int i, j, cell;

	if (!header || header->num_of_domain !=
	    shark_catalog_kind_count(SHARK_SOC_CATALOG_ASV_FREQS))
		return -EINVAL;
	for (i = 0; i < header->num_of_domain; i++)
		if (shark_validate_asv_domain(&header->domain_list[i]))
			return -EINVAL;

	for (i = 0; i < header->num_of_domain; i++) {
		struct ect_voltage_domain *domain = &header->domain_list[i];
		const struct shark_soc_catalog_table *freqs;

		freqs = shark_soc_catalog_find("ASV", domain->domain_name,
			"frequencies", SHARK_SOC_CATALOG_ASV_FREQS);
		for (j = 0; j < domain->num_of_level; j++)
			domain->level_list[j] = shark_domain_rate(
				domain->domain_name, j,
				(unsigned int)freqs->data[j] * 1000U) / 1000U;

		for (j = 0; j < domain->num_of_table; j++) {
			struct ect_voltage_table *live = &domain->table_list[j];
			const struct shark_soc_catalog_table *table;

			table = shark_find_asv_table(domain->domain_name,
				live->table_version);
			live->boot_level_idx = (int)table->aux0;
			live->resume_level_idx = (int)table->aux1;
			for (cell = 0;
			     cell < table->rows * table->cols; cell++) {
				if (live->voltages)
					live->voltages[cell] =
						(unsigned int)table->data[cell];
				else
					live->voltages_step[cell] =
						(unsigned char)
						(table->data[cell] /
						 SHARK_VOLT_STEP_UV);
			}
		}
	}
	return 0;
}

static int shark_publish_gen(struct ect_gen_param_header *header)
{
	unsigned int i, cell;

	if (!header || header->num_of_table !=
	    shark_catalog_kind_count(SHARK_SOC_CATALOG_GEN_TABLE))
		return -EINVAL;
	for (i = 0; i < header->num_of_table; i++) {
		struct ect_gen_param_table *live = &header->table_list[i];
		const struct shark_soc_catalog_table *table;

		table = shark_soc_catalog_find("GEN", live->table_name,
			"parameters",
			SHARK_SOC_CATALOG_GEN_TABLE);
		if (!table || table->rows != live->num_of_row ||
		    table->cols != live->num_of_col)
			return -EINVAL;
	}
	for (i = 0; i < header->num_of_table; i++) {
		struct ect_gen_param_table *live = &header->table_list[i];
		const struct shark_soc_catalog_table *table;

		table = shark_soc_catalog_find("GEN", live->table_name,
			"parameters",
			SHARK_SOC_CATALOG_GEN_TABLE);
		for (cell = 0; cell < table->rows * table->cols; cell++)
			live->parameter[cell] = (unsigned int)table->data[cell];
	}
	return 0;
}

static const struct shark_soc_catalog_table *
shark_find_newtime_table(unsigned long long key)
{
	char name[24];

	snprintf(name, sizeof(name), "%llX", key);
	return shark_soc_catalog_find("NEWTIME", name, "parameters",
		SHARK_SOC_CATALOG_NEWTIME_TABLE);
}

static int shark_publish_newtime(struct ect_new_timing_param_header *header)
{
	unsigned int i, cell;

	if (!header || header->num_of_size !=
	    shark_catalog_kind_count(SHARK_SOC_CATALOG_NEWTIME_TABLE))
		return -EINVAL;
	for (i = 0; i < header->num_of_size; i++) {
		struct ect_new_timing_param_size *live = &header->size_list[i];
		const struct shark_soc_catalog_table *table;

		table = shark_find_newtime_table(live->parameter_key);
		if (!table || table->rows != live->num_of_level ||
		    table->cols != live->num_of_timing_param ||
		    table->aux0 != live->mode)
			return -EINVAL;
	}
	for (i = 0; i < header->num_of_size; i++) {
		struct ect_new_timing_param_size *live = &header->size_list[i];
		const struct shark_soc_catalog_table *table;

		table = shark_find_newtime_table(live->parameter_key);
		for (cell = 0; cell < table->rows * table->cols; cell++) {
			u64 value = table->data[cell];

			if (live->mode == e_mode_normal_value)
				live->timing_parameter[cell] =
					(unsigned int)value;
			else {
				live->timing_parameter[cell * 2U] =
					(unsigned int)value;
				live->timing_parameter[cell * 2U + 1U] =
					(unsigned int)(value >> 32);
			}
		}
	}
	return 0;
}

static int shark_publish_thermal(struct ect_ap_thermal_header *header)
{
	unsigned int i, j;

	if (!header || header->num_of_function !=
	    shark_catalog_kind_count(SHARK_SOC_CATALOG_THERMAL_RANGES))
		return -EINVAL;
	for (i = 0; i < header->num_of_function; i++) {
		struct ect_ap_thermal_function *live;
		const struct shark_soc_catalog_table *table;

		live = &header->function_list[i];
		table = shark_soc_catalog_find("THERMAL", live->function_name,
			"ranges", SHARK_SOC_CATALOG_THERMAL_RANGES);
		if (!table || table->rows != live->num_of_range ||
		    table->cols != 5U)
			return -EINVAL;
	}
	for (i = 0; i < header->num_of_function; i++) {
		struct ect_ap_thermal_function *live;
		const struct shark_soc_catalog_table *table;

		live = &header->function_list[i];
		table = shark_soc_catalog_find("THERMAL", live->function_name,
			"ranges", SHARK_SOC_CATALOG_THERMAL_RANGES);
		for (j = 0; j < live->num_of_range; j++) {
			struct ect_ap_thermal_range *range;
			const u64 *data = &table->data[j * 5U];

			range = &live->range_list[j];
			range->lower_bound_temperature = (unsigned int)data[0];
			range->upper_bound_temperature = (unsigned int)data[1];
			range->max_frequency = (unsigned int)data[2];
			range->sw_trip = (unsigned int)data[3];
			range->flag = (unsigned int)data[4];
		}
	}
	return 0;
}

static int shark_publish_minlock(struct ect_minlock_header *header)
{
	unsigned int i, j;

	if (!header || header->num_of_domain !=
	    shark_catalog_kind_count(SHARK_SOC_CATALOG_MINLOCK_TABLE))
		return -EINVAL;
	for (i = 0; i < header->num_of_domain; i++) {
		struct ect_minlock_domain *live = &header->domain_list[i];
		const struct shark_soc_catalog_table *table;

		table = shark_soc_catalog_find("MINLOCK", live->domain_name,
			"levels",
			SHARK_SOC_CATALOG_MINLOCK_TABLE);
		if (!table || table->rows != live->num_of_level ||
		    table->cols != 2U)
			return -EINVAL;
	}
	for (i = 0; i < header->num_of_domain; i++) {
		struct ect_minlock_domain *live = &header->domain_list[i];
		const struct shark_soc_catalog_table *table;

		table = shark_soc_catalog_find("MINLOCK", live->domain_name,
			"levels",
			SHARK_SOC_CATALOG_MINLOCK_TABLE);
		for (j = 0; j < live->num_of_level; j++) {
			unsigned int main = (unsigned int)table->data[j * 2U];

			live->level[j].main_frequencies =
				shark_normalize_rate(live->domain_name, main);
			live->level[j].sub_frequencies =
				(unsigned int)table->data[j * 2U + 1U];
		}
	}
	return 0;
}

static int shark_publish_ect_block(const char *name, void *handle)
{
	if (!strcmp(name, BLOCK_PLL))
		return shark_publish_pll(handle);
	if (!strcmp(name, BLOCK_DVFS))
		return shark_publish_dvfs(handle);
	if (!strcmp(name, BLOCK_ASV))
		return shark_publish_asv(handle);
	if (!strcmp(name, BLOCK_GEN_PARAM))
		return shark_publish_gen(handle);
	if (!strcmp(name, BLOCK_NEW_TIMING_PARAM))
		return shark_publish_newtime(handle);
	if (!strcmp(name, BLOCK_AP_THERMAL))
		return shark_publish_thermal(handle);
	if (!strcmp(name, BLOCK_MINLOCK))
		return shark_publish_minlock(handle);

	return -EOPNOTSUPP;
}

static bool shark_ect_block_supported(const char *name)
{
	return !strcmp(name, BLOCK_PLL) || !strcmp(name, BLOCK_DVFS) ||
	       !strcmp(name, BLOCK_ASV) || !strcmp(name, BLOCK_GEN_PARAM) ||
	       !strcmp(name, BLOCK_NEW_TIMING_PARAM) ||
	       !strcmp(name, BLOCK_AP_THERMAL) || !strcmp(name, BLOCK_MINLOCK);
}

void shark_soc_ect_block_post_parse(const char *block_name, void *handle)
{
	struct shark_ect_backend *backend = NULL;
	unsigned int i;
	int ret;

	if (!shark_enabled || !block_name || !handle ||
	    !shark_ect_block_supported(block_name))
		return;
	shark_registry_init();
	for (i = 0; i < shark_backend_count; i++)
		if (!strcmp(block_name, shark_backends[i].name)) {
			backend = &shark_backends[i];
			break;
		}
	if (!backend && shark_backend_count < ARRAY_SIZE(shark_backends)) {
		backend = &shark_backends[shark_backend_count++];
		backend->name = block_name;
	}
	if (!backend)
		return;

	backend->handle = handle;
	ret = shark_publish_ect_block(block_name, handle);
	backend->publish_result = ret;
	if (ret && ret != -EOPNOTSUPP)
		pr_warn("Shark DVFS: rejected incompatible ECT %s block (%d)\n",
			block_name, ret);
	else if (!ret)
		pr_info("Shark DVFS: published %s from all_dump catalog\n",
			block_name);
}
EXPORT_SYMBOL_GPL(shark_soc_ect_block_post_parse);

static int shark_domains_show(struct seq_file *s, void *unused)
{
	unsigned int i, j;

	mutex_lock(&shark_lock);
	shark_registry_init();
	seq_printf(s, "enabled=%u dump_sha256=%s domains=%u\n",
		shark_enabled, SHARK_SOC_DUMP_SHA256, shark_domain_count);
	for (i = 0; i < shark_domain_count; i++) {
		struct shark_domain *domain = &shark_domains[i];

		seq_printf(s, "%s id=0x%x levels=%u vclk=%u fvmap=%u\n",
			domain->name, domain->id, domain->count,
			domain->vclk_bound, domain->fvmap_bound);
		seq_printf(s, "  policy=%u..%u boot=%u resume=%u\n",
			domain->min_freq, domain->max_freq,
			domain->boot_freq, domain->resume_freq);
		for (j = 0; j < domain->count; j++)
			seq_printf(s, "  L%-2u %7u kHz %7u uV\n", j,
				domain->rates[j], domain->volts[j]);
	}
	mutex_unlock(&shark_lock);
	return 0;
}

static int shark_catalog_show(struct seq_file *s, void *unused)
{
	unsigned int i;

	seq_printf(s, "dump_sha256=%s tables=%u\n", SHARK_SOC_DUMP_SHA256,
		(unsigned int)SHARK_SOC_STATIC_CATALOG_COUNT);
	for (i = 0; i < SHARK_SOC_STATIC_CATALOG_COUNT; i++) {
		const struct shark_soc_catalog_table *table;

		table = &shark_soc_static_catalog[i];
		seq_printf(s, "%u %s/%s/%s kind=%u %ux%u aux=%u,%u\n", i,
			   table->block, table->name, table->subname,
			   table->kind, table->rows, table->cols,
			   table->aux0, table->aux1);
	}
	return 0;
}

static int shark_status_show(struct seq_file *s, void *unused)
{
	unsigned int i;

	seq_printf(s, "enabled=%u catalog_tables=%u dump_sha256=%s\n",
		shark_enabled, (unsigned int)SHARK_SOC_STATIC_CATALOG_COUNT,
		SHARK_SOC_DUMP_SHA256);
	for (i = 0; i < shark_backend_count; i++)
		seq_printf(s, "%s handle=%pK result=%d\n",
			shark_backends[i].name, shark_backends[i].handle,
			shark_backends[i].publish_result);
	return 0;
}

static int shark_domains_open(struct inode *inode, struct file *file)
{
	return single_open(file, shark_domains_show, inode->i_private);
}

static int shark_catalog_open(struct inode *inode, struct file *file)
{
	return single_open(file, shark_catalog_show, inode->i_private);
}

static int shark_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, shark_status_show, inode->i_private);
}

static const struct file_operations shark_domains_fops = {
	.owner = THIS_MODULE,
	.open = shark_domains_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations shark_catalog_fops = {
	.owner = THIS_MODULE,
	.open = shark_catalog_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations shark_status_fops = {
	.owner = THIS_MODULE,
	.open = shark_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init shark_soc_debugfs_init(void)
{
	struct dentry *root;

	shark_registry_init();
	root = debugfs_create_dir("shark_custom_dvfs", NULL);
	if (IS_ERR_OR_NULL(root))
		return root ? PTR_ERR(root) : -ENOMEM;
	debugfs_create_file("domains", 0444, root, NULL, &shark_domains_fops);
	debugfs_create_file("catalog", 0444, root, NULL, &shark_catalog_fops);
	debugfs_create_file("status", 0444, root, NULL, &shark_status_fops);
	return 0;
}
late_initcall(shark_soc_debugfs_init);

MODULE_DESCRIPTION("Shark custom DVFS authority for Exynos8895");
MODULE_LICENSE("GPL");
