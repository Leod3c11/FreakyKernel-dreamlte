#include <linux/module.h>
#include <linux/io.h>
#include <linux/exynos-ss.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/cal-if.h>
#if defined(CONFIG_SOC_EXYNOS8895)
#include <soc/samsung/exynos8895-g3d-hardcoded.h>
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

unsigned int cal_clk_is_enabled(unsigned int id)
{
	return 0;
}


unsigned long cal_dfs_get_max_freq(unsigned int id)
{
#if defined(CONFIG_SOC_EXYNOS8895)
	if (IS_ACPM_VCLK(id) && exynos8895_hc_override_cal(GET_IDX(id)))
		return exynos8895_hc_domain(GET_IDX(id))->policy_max_khz;
#endif
	return vclk_get_max_freq(id);
}


unsigned long cal_dfs_get_min_freq(unsigned int id)
{
#if defined(CONFIG_SOC_EXYNOS8895)
	if (IS_ACPM_VCLK(id) && exynos8895_hc_override_cal(GET_IDX(id)))
		return exynos8895_hc_domain(GET_IDX(id))->policy_min_khz;
#endif
	return vclk_get_min_freq(id);
}


unsigned int cal_dfs_get_lv_num(unsigned int id)
{
#if defined(CONFIG_SOC_EXYNOS8895)
	if (IS_ACPM_VCLK(id) && exynos8895_hc_override_cal(GET_IDX(id)))
		return exynos8895_hc_domain(GET_IDX(id))->level_count;
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
	if (IS_ACPM_VCLK(id) && GET_IDX(id) == EXYNOS8895_G3D_ACPM_INDEX) {
		const struct exynos8895_g3d_hardcoded_opp *opp;
		unsigned long fw_rate, pll_rate, core_rate;
		unsigned int pll_id, mux_id;

		opp = exynos8895_g3d_find_opp(rate);
		if (!opp) {
			pr_err("G3D hardcoded: refusing unsupported logical rate %lu kHz\n", rate);
			return -EINVAL;
		}

		ret = exynos8895_g3d_hardcoded_apply();
		if (ret) {
			pr_err("G3D hardcoded: SRAM apply failed before %lu kHz (%d)\n", rate, ret);
			return ret;
		}

		/*
		 * ACPM firmware keeps Samsung's nominal frequency as the slot key.
		 * The source-owned FVMap row supplies the voltage and PMS that the
		 * selected slot actually programs into PLL_G3D.
		 */
		ret = exynos_acpm_set_rate(EXYNOS8895_G3D_ACPM_INDEX,
					  opp->acpm_key_khz);
		if (ret) {
			pr_err("G3D hardcoded: ACPM key %u for %lu kHz failed (%d)\n",
			       opp->acpm_key_khz, rate, ret);
			return ret;
		}

		fw_rate = exynos_acpm_get_rate(EXYNOS8895_G3D_ACPM_INDEX);
		pll_id = cmucal_get_id("PLL_G3D");
		if (pll_id == INVALID_CLK_ID)
			return -ENODEV;
		pll_rate = ra_recalc_rate(pll_id) / 1000UL;

		pr_info("G3D hardcoded transition: logical=%lu key=%u fw=%lu pll=%lu kHz PMS=%u/%u/%u\n",
			rate, opp->acpm_key_khz, fw_rate, pll_rate,
			opp->pll_m, opp->pll_p, opp->pll_s);

		/*
		 * This Exynos8895 firmware returns 0 from exynos_acpm_get_rate() for
		 * G3D even after a successful set-rate transaction.  Treat that value
		 * as diagnostic only.  The authoritative success condition is the
		 * physical PLL_G3D readback, which comes from the live registers.
		 */
		if (fw_rate && fw_rate != opp->acpm_key_khz && fw_rate != rate)
			pr_warn("G3D hardcoded: ACPM diagnostic readback logical=%lu key=%u fw=%lu\n",
				rate, opp->acpm_key_khz, fw_rate);

		if (pll_rate != rate) {
			pr_err("G3D hardcoded: physical PLL mismatch logical=%lu actual=%lu kHz\n",
			       rate, pll_rate);
			return -EIO;
		}

		/*
		 * PLL_G3D is not the final clock delivered to the shader core.  The
		 * G3D path ends at MUX_CLK_G3D_BUSD, whose other parent is the
		 * temporary switch clock used while the PLL is being changed.  With
		 * custom FVMap PMS values, explicitly restore BUSD to the now-stable
		 * PLL parent and verify the delivered core clock, not only PLL_G3D.
		 */
		mux_id = cmucal_get_id("MUX_CLK_G3D_BUSD");
		if (mux_id == INVALID_CLK_ID)
			return -ENODEV;

		ret = ra_set_rate(mux_id, rate * 1000UL);
		if (ret) {
			pr_err("G3D hardcoded: failed to restore BUSD mux to %lu kHz (%d)\n",
			       rate, ret);
			return ret;
		}

		core_rate = ra_recalc_rate(mux_id) / 1000UL;
		if (core_rate != rate) {
			pr_err("G3D hardcoded: core-path mismatch logical=%lu pll=%lu core=%lu kHz\n",
			       rate, pll_rate, core_rate);
			return -EIO;
		}

		pr_info("G3D hardcoded delivered: logical=%lu key=%u pll=%lu core=%lu kHz\n",
			rate, opp->acpm_key_khz, pll_rate, core_rate);

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
	int ret;

	ret = vclk_get_rate(id);

	return ret;
}

unsigned long cal_dfs_get_rate(unsigned int id)
{
	int ret;

#if defined(CONFIG_SOC_EXYNOS8895)
	if (IS_ACPM_VCLK(id) &&
	    GET_IDX(id) == EXYNOS8895_G3D_ACPM_INDEX) {
		const struct exynos8895_g3d_hardcoded_opp *opp;
		unsigned int mux_id;
		unsigned long core_rate;

		/*
		 * Report the clock that actually feeds G3D, not merely PLL_G3D.
		 * BUSD may temporarily select CLKCMU_G3D_SWITCH during a transition.
		 */
		mux_id = cmucal_get_id("MUX_CLK_G3D_BUSD");
		if (mux_id == INVALID_CLK_ID)
			return 0;

		core_rate = ra_recalc_rate(mux_id) / 1000UL;
		opp = exynos8895_g3d_find_opp(core_rate);
		return opp ? opp->clock_khz : 0;
	}
#endif

	ret = vclk_recalc_rate(id);

	return ret;
}


int cal_dfs_get_rate_table(unsigned int id, unsigned long *table)
{
#if defined(CONFIG_SOC_EXYNOS8895)
	if (IS_ACPM_VCLK(id) && exynos8895_hc_override_cal(GET_IDX(id)))
		return exynos8895_hc_fill_rate_table(GET_IDX(id), table);
#endif
	return vclk_get_rate_table(id, table);
}

#if defined(CONFIG_SOC_EXYNOS8895)
unsigned long cal_g3d_get_pll_rate_exact(void)
{
	unsigned int pll_id;

	pll_id = cmucal_get_id("PLL_G3D");
	if (pll_id == INVALID_CLK_ID)
		return 0;
	return ra_recalc_rate(pll_id) / 1000UL;
}
EXPORT_SYMBOL_GPL(cal_g3d_get_pll_rate_exact);

unsigned long cal_g3d_get_core_rate_exact(void)
{
	unsigned int mux_id;

	mux_id = cmucal_get_id("MUX_CLK_G3D_BUSD");
	if (mux_id == INVALID_CLK_ID)
		return 0;
	return ra_recalc_rate(mux_id) / 1000UL;
}
EXPORT_SYMBOL_GPL(cal_g3d_get_core_rate_exact);

int cal_g3d_get_pll_pms(unsigned int *m, unsigned int *p, unsigned int *s)
{
	struct cmucal_clk *clk;
	struct cmucal_pll *pll;
	unsigned int pll_id, con0;

	if (!m || !p || !s)
		return -EINVAL;
	pll_id = cmucal_get_id("PLL_G3D");
	if (pll_id == INVALID_CLK_ID)
		return -ENODEV;
	clk = cmucal_get_node(pll_id);
	if (!clk || !IS_PLL(clk->id) || !clk->pll_con0)
		return -ENODEV;
	pll = to_clk_pll(clk);
	con0 = __raw_readl(clk->pll_con0);
	*m = (con0 >> pll->m_shift) & ((1U << pll->m_width) - 1U);
	*p = (con0 >> pll->p_shift) & ((1U << pll->p_width) - 1U);
	*s = (con0 >> pll->s_shift) & ((1U << pll->s_width) - 1U);
	return 0;
}
EXPORT_SYMBOL_GPL(cal_g3d_get_pll_pms);
#endif

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
