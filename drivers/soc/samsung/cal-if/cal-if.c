#include <linux/module.h>
#include <linux/exynos-ss.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/cal-if.h>
#ifdef CONFIG_SHARK_CUSTOM_DVFS
#include <soc/samsung/exynos-soc_interface.h>
#endif

#include "pwrcal-env.h"
#include "pwrcal-rae.h"
#include "cmucal.h"
#include "ra.h"
#include "acpm_dvfs.h"
#include "fvmap.h"
#include "asv.h"

#include "pmucal_system.h"
#include "pmucal_local.h"
#include "pmucal_cpu.h"
#include "pmucal_rae.h"

#ifdef CONFIG_SHARK_CUSTOM_DVFS
static int cal_shark_get_domain(unsigned int id, struct vclk **out_vclk,
				unsigned int *rates, unsigned int *count)
{
	unsigned int shark_rates[SHARK_SOC_MAX_DOMAIN_OPPS];
	unsigned int shark_count = 0;
	struct vclk *vclk;
	unsigned int i;
	int ret;

	if (!IS_ACPM_VCLK(id))
		return -ENOENT;
	vclk = cmucal_get_node(id);
	if (!vclk || !vclk->name || !vclk->lut)
		return -ENOENT;

	ret = shark_soc_get_domain_rate_table(vclk->name, id, shark_rates,
					      ARRAY_SIZE(shark_rates),
					      &shark_count);
	if (ret)
		return ret;
	if (!shark_count || shark_count != vclk->num_rates)
		return -EINVAL;
	for (i = 0; i < shark_count; i++) {
		if (!shark_rates[i] ||
		    (i && shark_rates[i] >= shark_rates[i - 1]) ||
		    shark_rates[i] != vclk->lut[i].rate)
			return -EINVAL;
		if (rates)
			rates[i] = shark_rates[i];
	}

	if (out_vclk)
		*out_vclk = vclk;
	if (count)
		*count = shark_count;
	return 0;
}

typedef int (*cal_shark_policy_getter_t)(const char *, unsigned int,
					 unsigned int *);

static unsigned long cal_shark_get_policy(unsigned int id,
					  cal_shark_policy_getter_t getter)
{
	struct vclk *vclk;
	unsigned int rate;
	int ret;

	ret = cal_shark_get_domain(id, &vclk, NULL, NULL);
	if (ret) {
		pr_err("Shark DVFS: domain 0x%x is unavailable (%d)\n", id, ret);
		return 0;
	}
	ret = getter(vclk->name, id, &rate);
	if (ret) {
		pr_err("Shark DVFS: policy for %s is unavailable (%d)\n",
		       vclk->name, ret);
		return 0;
	}
	return rate;
}
#endif

unsigned int cal_clk_is_enabled(unsigned int id)
{
	return 0;
}

unsigned long cal_dfs_get_max_freq(unsigned int id)
{
#ifdef CONFIG_SHARK_CUSTOM_DVFS
	return cal_shark_get_policy(id, shark_soc_get_domain_max_freq);
#else
	return vclk_get_max_freq(id);
#endif
}

unsigned long cal_dfs_get_min_freq(unsigned int id)
{
#ifdef CONFIG_SHARK_CUSTOM_DVFS
	return cal_shark_get_policy(id, shark_soc_get_domain_min_freq);
#else
	return vclk_get_min_freq(id);
#endif
}

unsigned int cal_dfs_get_lv_num(unsigned int id)
{
#ifdef CONFIG_SHARK_CUSTOM_DVFS
	unsigned int count;
	int ret;

	ret = cal_shark_get_domain(id, NULL, NULL, &count);
	if (ret) {
		pr_err("Shark DVFS: level table for 0x%x is unavailable (%d)\n",
		       id, ret);
		return 0;
	}
	return count;
#else
	return vclk_get_lv_num(id);
#endif
}

int cal_dfs_get_bigturbo_max_freq(unsigned int *table)
{
#ifdef CONFIG_SHARK_CUSTOM_DVFS
	unsigned int count;

	return shark_soc_get_bigturbo_table(asv_get_table_ver(), table, 4U,
					    &count);
#else
	return vclk_get_bigturbo_table(table);
#endif
}

int cal_dfs_set_rate(unsigned int id, unsigned long rate)
{
	struct vclk *vclk;
	int ret;

	if (IS_ACPM_VCLK(id)) {
#ifdef CONFIG_SHARK_CUSTOM_DVFS
		unsigned int resolved;

		ret = cal_shark_get_domain(id, &vclk, NULL, NULL);
		if (ret)
			return ret;
		ret = shark_soc_resolve_rate(vclk->name, id, rate, &resolved);
		if (ret)
			return ret;
		if (rate != resolved)
			pr_debug("Shark DVFS: %s request %lu -> %u kHz\n",
				 vclk->name, rate, resolved);
		rate = resolved;
#endif
		ret = exynos_acpm_set_rate(GET_IDX(id), rate);
		if (!ret) {
			vclk = cmucal_get_node(id);
			if (vclk)
				vclk->vrate = rate;
		}
	} else {
		ret = vclk_set_rate(id, rate);
	}

	return ret;
}

int cal_dfs_set_rate_switch(unsigned int id, unsigned long switch_rate)
{
	int ret = 0;

	ret = vclk_set_rate_switch(id, switch_rate);

	return ret;
}

int cal_dfs_set_rate_restore(unsigned int id, unsigned long switch_rate)
{
	int ret = 0;

	ret = vclk_set_rate_restore(id, switch_rate);

	return ret;
}

unsigned long cal_dfs_cached_get_rate(unsigned int id)
{
	int ret;

	ret = vclk_get_rate(id);

	return ret;
}

unsigned long cal_dfs_get_rate(unsigned int id)
{
	int ret;

	ret = vclk_recalc_rate(id);

	return ret;
}

int cal_dfs_get_rate_table(unsigned int id, unsigned long *table)
{
	int ret;

#ifdef CONFIG_SHARK_CUSTOM_DVFS
	{
		unsigned int rates[SHARK_SOC_MAX_DOMAIN_OPPS];
		unsigned int count;
		unsigned int i;

		ret = cal_shark_get_domain(id, NULL, rates, &count);
		if (ret)
			return ret;
		for (i = 0; i < count; i++)
			table[i] = rates[i];
		return count;
	}
#else
	ret = vclk_get_rate_table(id, table);

	return ret;
#endif
}

int cal_clk_setrate(unsigned int id, unsigned long rate)
{
	int ret = -EINVAL;

	ret = vclk_set_rate(id, rate);

	return ret;
}

unsigned long cal_clk_getrate(unsigned int id)
{
	int ret = 0;

	ret = vclk_recalc_rate(id);

	return ret;
}

int cal_clk_enable(unsigned int id)
{
	int ret = 0;

	ret = vclk_set_enable(id);

	return ret;
}

int cal_clk_disable(unsigned int id)
{
	int ret = 0;

	ret = vclk_set_disable(id);

	return ret;
}

int cal_qch_init(unsigned int id, unsigned int use_qch)
{
	int ret = 0;

	ret = ra_set_qch(id, use_qch, 0, 0);

	return ret;
}

unsigned int cal_dfs_get_boot_freq(unsigned int id)
{
#ifdef CONFIG_SHARK_CUSTOM_DVFS
	return cal_shark_get_policy(id, shark_soc_get_domain_boot_freq);
#else
	return vclk_get_boot_freq(id);
#endif
}

unsigned int cal_dfs_get_resume_freq(unsigned int id)
{
#ifdef CONFIG_SHARK_CUSTOM_DVFS
	return cal_shark_get_policy(id, shark_soc_get_domain_resume_freq);
#else
	return vclk_get_resume_freq(id);
#endif
}

int cal_pd_control(unsigned int id, int on)
{
	unsigned int index;

	if ((id & 0xFFFF0000) != BLKPWR_MAGIC)
		return -1;

	index = id & 0x0000FFFF;

	if (on)
		return pmucal_local_enable(index);
	else
		return pmucal_local_disable(index);
}

int cal_pd_status(unsigned int id)
{
	unsigned int index;

	if ((id & 0xFFFF0000) != BLKPWR_MAGIC)
		return -1;

	index = id & 0x0000FFFF;

	return pmucal_local_is_enabled(index);
}

int cal_pm_enter(int mode)
{
	return pmucal_system_enter(mode);
}

int cal_pm_exit(int mode)
{
	return pmucal_system_exit(mode);
}

int cal_pm_earlywakeup(int mode)
{
	return pmucal_system_earlywakeup(mode);
}

int cal_cpu_enable(unsigned int cpu)
{
	return pmucal_cpu_enable(cpu);
}

int cal_cpu_disable(unsigned int cpu)
{
	return pmucal_cpu_disable(cpu);
}

int cal_cpu_status(unsigned int cpu)
{
	return pmucal_cpu_is_enabled(cpu);
}

int cal_cluster_enable(unsigned int cluster)
{
	return pmucal_cpu_cluster_enable(cluster);
}

int cal_cluster_disable(unsigned int cluster)
{
	return pmucal_cpu_cluster_disable(cluster);
}

int cal_cluster_status(unsigned int cluster)
{
	return pmucal_cpu_cluster_is_enabled(cluster);
}

int cal_dfs_get_asv_table(unsigned int id, unsigned int *table)
{
#ifdef CONFIG_SHARK_CUSTOM_DVFS
	unsigned int rates[SHARK_SOC_MAX_DOMAIN_OPPS];
	unsigned int count = 0;
	struct vclk *vclk;
	unsigned int i;
	int ret;

	ret = cal_shark_get_domain(id, &vclk, NULL, NULL);
	if (ret)
		return ret;
	ret = shark_soc_get_domain_table(vclk->name, id, rates, table,
					 ARRAY_SIZE(rates), &count);
	if (ret)
		return ret;
	for (i = 0; i < count; i++)
		if (!table[i])
			return -ENODATA;
	return count;
#else
	return fvmap_get_voltage_table(id, table);
#endif
}

void cal_dfs_set_volt_margin(unsigned int id, int volt)
{
	if (IS_ACPM_VCLK(id))
		exynos_acpm_set_volt_margin(id, volt);
}

int cal_dfs_get_rate_asv_table(unsigned int id,
					struct dvfs_rate_volt *table)
{
	unsigned long rate[48];
	unsigned int volt[48];
	int num_of_entry;
	int idx;

	num_of_entry = cal_dfs_get_rate_table(id, rate);
	if (num_of_entry == 0)
		return 0;

	if (num_of_entry != cal_dfs_get_asv_table(id, volt))
		return 0;

	for (idx = 0; idx < num_of_entry; idx++) {
		table[idx].rate = rate[idx];
		table[idx].volt = volt[idx];
	}

	return num_of_entry;
}

int cal_asv_get_ids_info(unsigned int id)
{
	return asv_get_ids_info(id);
}

int cal_asv_get_grp(unsigned int id)
{
	return asv_get_grp(id);
}

int cal_asv_get_tablever(void)
{
	return asv_get_table_ver();
}

int __init cal_if_init(void *dev)
{
	static int cal_initialized;
	int ret;

	if (cal_initialized == 1)
		return 0;

	ect_parse_binary_header();

	ret = vclk_initialize();
	if (ret < 0)
		return ret;

	if (cal_data_init)
		cal_data_init();

	ret = pmucal_rae_init();
	if (ret < 0)
		return ret;

	ret = pmucal_system_init();
	if (ret < 0)
		return ret;

	ret = pmucal_local_init();
	if (ret < 0)
		return ret;

	ret = pmucal_cpu_init();
	if (ret < 0)
		return ret;

	exynos_acpm_set_device(dev);

	cal_initialized = 1;

	return 0;
}
