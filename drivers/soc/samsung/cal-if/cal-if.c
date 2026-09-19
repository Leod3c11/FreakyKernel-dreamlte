#include <linux/module.h>
#include <linux/exynos-ss.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos8895-g3d-hardcoded.h>
#include <linux/string.h>

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

unsigned int cal_clk_is_enabled(unsigned int id)
{
	return 0;
}

#if defined(CONFIG_SOC_EXYNOS8895)
int cal_g3d_validate_rate_exact(unsigned long rate);

static bool cal_is_exynos8895_g3d(unsigned int id)
{
    struct vclk *vclk = cmucal_get_node(id);

    return vclk && vclk->name && !strcmp(vclk->name, "dvfs_g3d");
}

static unsigned int cal_g3d_pll_id(void)
{
    return cmucal_get_id("PLL_G3D");
}

static int cal_g3d_build_hardcoded_voltage_table(unsigned int id,
                                                  unsigned int *table)
{
    unsigned long stock_rate[48];
    unsigned int stock_volt[48];
    int stock_count, volt_count;
    unsigned int i;
    int j;

    stock_count = vclk_get_rate_table(id, stock_rate);
    volt_count = fvmap_get_voltage_table(id, stock_volt);
    if (stock_count <= 0 || stock_count != volt_count)
        return 0;

    for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++) {
        const struct exynos8895_g3d_hardcoded_opp *opp =
            &exynos8895_g3d_opp_table[i];
        int voltage = 0;

        for (j = 0; j < stock_count; j++) {
            if (stock_rate[j] == opp->acpm_anchor_khz) {
                voltage = (int)stock_volt[j] + opp->volt_margin_uv;
                break;
            }
        }

        if (!voltage) {
            pr_err("G3D hardcoded: ACPM anchor %u kHz not found\n",
                   opp->acpm_anchor_khz);
            return 0;
        }

        table[i] = voltage;
    }

    return EXYNOS8895_G3D_OPP_COUNT;
}
#endif

unsigned long cal_dfs_get_max_freq(unsigned int id)
{
#if defined(CONFIG_SOC_EXYNOS8895)
    if (cal_is_exynos8895_g3d(id))
        return exynos8895_g3d_opp_table[0].clock_khz;
#endif
    return vclk_get_max_freq(id);
}

unsigned long cal_dfs_get_min_freq(unsigned int id)
{
#if defined(CONFIG_SOC_EXYNOS8895)
    if (cal_is_exynos8895_g3d(id))
        return exynos8895_g3d_opp_table[EXYNOS8895_G3D_OPP_COUNT - 1].clock_khz;
#endif
    return vclk_get_min_freq(id);
}

unsigned int cal_dfs_get_lv_num(unsigned int id)
{
#if defined(CONFIG_SOC_EXYNOS8895)
    if (cal_is_exynos8895_g3d(id))
        return EXYNOS8895_G3D_OPP_COUNT;
#endif
    return vclk_get_lv_num(id);
}

int cal_dfs_get_bigturbo_max_freq(unsigned int *table)
{
	return vclk_get_bigturbo_table(table);
}

int cal_dfs_set_rate(unsigned int id, unsigned long rate)
{
    struct vclk *vclk;
    int ret;

#if defined(CONFIG_SOC_EXYNOS8895)
    if (cal_is_exynos8895_g3d(id)) {
        const struct exynos8895_g3d_hardcoded_opp *opp;
        unsigned int pll_id;
        unsigned long actual;

        opp = exynos8895_g3d_find_opp(rate);
        if (!opp) {
            pr_err("G3D hardcoded: %lu kHz is not in source table\n", rate);
            return -EINVAL;
        }

        ret = cal_g3d_validate_rate_exact(rate);
        if (ret) {
            pr_err("G3D hardcoded: %lu kHz is not exactly synthesizable (%d)\n",
                   rate, ret);
            return ret;
        }

        /*
         * 1) Park at a real Samsung FVMap OPP first. This lets ACPM/ASV
         *    establish a valid rail voltage and a known-safe clock.
         * 2) Apply the source-controlled margin.
         * 3) Re-apply the anchor so the new margin is active.
         * 4) Program PLL_G3D to the exact hardcoded target.
         */
        ret = exynos_acpm_set_rate(GET_IDX(id), opp->acpm_anchor_khz);
        if (ret)
            return ret;

        ret = exynos_acpm_set_volt_margin(id, opp->volt_margin_uv);
        if (ret)
            return ret;

        ret = exynos_acpm_set_rate(GET_IDX(id), opp->acpm_anchor_khz);
        if (ret)
            return ret;

        pll_id = cal_g3d_pll_id();
        if (pll_id == INVALID_CLK_ID)
            return -ENODEV;

        /* ra_set_rate() expects Hz for a PLL and converts to kHz internally. */
        ret = ra_set_rate(pll_id, rate * 1000UL);
        if (ret)
            return ret;

        actual = ra_recalc_rate(pll_id) / 1000UL;
        if (actual != rate) {
            pr_err("G3D hardcoded verify failed: requested=%lu actual=%lu kHz\n",
                   rate, actual);
            return -EIO;
        }

        vclk = cmucal_get_node(id);
        if (vclk)
            vclk->vrate = rate;

        return 0;
    }
#endif

    if (IS_ACPM_VCLK(id)) {
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
#if defined(CONFIG_SOC_EXYNOS8895)
    if (cal_is_exynos8895_g3d(id)) {
        unsigned int pll_id = cal_g3d_pll_id();
        if (pll_id == INVALID_CLK_ID)
            return 0;
        return ra_recalc_rate(pll_id) / 1000UL;
    }
#endif
    return vclk_get_rate(id);
}

unsigned long cal_dfs_get_rate(unsigned int id)
{
#if defined(CONFIG_SOC_EXYNOS8895)
    if (cal_is_exynos8895_g3d(id)) {
        unsigned int pll_id = cal_g3d_pll_id();
        if (pll_id == INVALID_CLK_ID)
            return 0;
        return ra_recalc_rate(pll_id) / 1000UL;
    }
#endif
    return vclk_recalc_rate(id);
}

int cal_dfs_get_rate_table(unsigned int id, unsigned long *table)
{
#if defined(CONFIG_SOC_EXYNOS8895)
    if (cal_is_exynos8895_g3d(id)) {
        unsigned int i;
        for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++)
            table[i] = exynos8895_g3d_opp_table[i].clock_khz;
        return EXYNOS8895_G3D_OPP_COUNT;
    }
#endif
    return vclk_get_rate_table(id, table);
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

/*
 * Exynos8895 exact G3D PLL control.
 *
 * ACPM exposes nominal DVFS rates. This helper validates that the requested
 * kHz value is exactly representable by PLL_G3D before touching hardware,
 * then programs the physical PLL through the existing CAL RA layer.
 */
int cal_g3d_validate_rate_exact(unsigned long rate)
{
	unsigned int pll_id;
	unsigned int fin;
	struct cmucal_clk *clk;
	struct cmucal_pll *pll;
	struct cmucal_pll_table table;
	int ret;

	pll_id = cmucal_get_id("PLL_G3D");
	if (pll_id == INVALID_CLK_ID)
		return -ENODEV;

	clk = cmucal_get_node(pll_id);
	if (!clk || !IS_PLL(clk->id))
		return -ENODEV;

	pll = to_clk_pll(clk);
	if (IS_FIXED_RATE(clk->pid))
		fin = ra_get_value(clk->pid);
	else
		fin = FIN_HZ_26M;

	ret = pll_find_table(pll, &table, fin, rate);
	if (ret)
		return ret;

	/* Reject the nearest integer-N result: exact means exact in Hz. */
	if (table.rate != khz_to_hz(rate))
		return -ERANGE;

	return 0;
}
EXPORT_SYMBOL_GPL(cal_g3d_validate_rate_exact);

int cal_g3d_set_rate_exact(unsigned long rate)
{
	unsigned int pll_id;
	unsigned long actual;
	int ret;

	ret = cal_g3d_validate_rate_exact(rate);
	if (ret)
		return ret;

	pll_id = cmucal_get_id("PLL_G3D");
	if (pll_id == INVALID_CLK_ID)
		return -ENODEV;

	ret = ra_set_rate(pll_id, rate * 1000UL);
	if (ret)
		return ret;

	actual = ra_recalc_rate(pll_id) / 1000;
	if (actual != rate) {
		pr_err("G3D exact clock verify failed: requested=%lu actual=%lu kHz\n",
		       rate, actual);
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cal_g3d_set_rate_exact);

unsigned long cal_g3d_get_rate_exact(void)
{
	unsigned int pll_id;

	pll_id = cmucal_get_id("PLL_G3D");
	if (pll_id == INVALID_CLK_ID)
		return 0;

	return ra_recalc_rate(pll_id) / 1000;
}
EXPORT_SYMBOL_GPL(cal_g3d_get_rate_exact);

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
	return vclk_get_boot_freq(id);
}

unsigned int cal_dfs_get_resume_freq(unsigned int id)
{
	return vclk_get_resume_freq(id);
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
#if defined(CONFIG_SOC_EXYNOS8895)
    if (cal_is_exynos8895_g3d(id))
        return cal_g3d_build_hardcoded_voltage_table(id, table);
#endif
    return fvmap_get_voltage_table(id, table);
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

	vclk_initialize();

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
