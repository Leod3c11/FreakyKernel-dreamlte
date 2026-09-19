#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <soc/samsung/cal-if.h>
#if defined(CONFIG_SOC_EXYNOS8895)
#include <soc/samsung/exynos8895-g3d-hardcoded.h>
#endif

#include "fvmap.h"
#include "cmucal.h"
#include "vclk.h"
#include "ra.h"
#include "acpm_dvfs.h"

#define FVMAP_SIZE		(SZ_8K)

void __iomem *fvmap_base;
void __iomem *sram_fvmap_base;

int init_margin_table[10];

int set_mif_volt;
int set_int_volt;
int set_cpucl0_volt;
int set_cpucl1_volt;
int set_g3d_volt;
int set_intcam_volt;
int set_cam_volt;
int set_disp_volt;
int set_g3dm_volt;
int set_cp_volt;

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	init_margin_table[0] = set_mif_volt;
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	init_margin_table[1] = set_int_volt;
	return 0;
}
early_param("int", get_int_volt);

static int __init get_cpucl0_volt(char *str)
{
	get_option(&str, &set_cpucl0_volt);
	init_margin_table[2] = set_cpucl0_volt;
	return 0;
}
early_param("big", get_cpucl0_volt);

static int __init get_cpucl1_volt(char *str)
{
	get_option(&str, &set_cpucl1_volt);
	init_margin_table[3] = set_cpucl1_volt;
	return 0;
}
early_param("lit", get_cpucl1_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	init_margin_table[4] = set_g3d_volt;
	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_intcam_volt(char *str)
{
	get_option(&str, &set_intcam_volt);
	init_margin_table[5] = set_intcam_volt;
	return 0;
}
early_param("intcam", get_intcam_volt);

static int __init get_cam_volt(char *str)
{
	get_option(&str, &set_cam_volt);
	init_margin_table[6] = set_cam_volt;
	return 0;
}
early_param("cam", get_cam_volt);

static int __init get_disp_volt(char *str)
{
	get_option(&str, &set_disp_volt);
	init_margin_table[7] = set_disp_volt;
	return 0;
}
early_param("disp", get_disp_volt);

static int __init get_g3dm_volt(char *str)
{
	get_option(&str, &set_g3dm_volt);
	init_margin_table[8] = set_g3dm_volt;
	return 0;
}
early_param("g3dm", get_g3dm_volt);

static int __init get_cp_volt(char *str)
{
	get_option(&str, &set_cp_volt);
	init_margin_table[9] = set_cp_volt;
	return 0;
}
early_param("cp", get_cp_volt);


#if defined(CONFIG_SOC_EXYNOS8895)
static void fvmap_override_exynos8895_g3d(void __iomem *sram_base)
{
	struct fvmap_header *header = sram_base;
	struct rate_volt_header *rv;
	struct clocks *clks;
	struct pll_header *pll = NULL;
	unsigned int idx = EXYNOS8895_G3D_ACPM_INDEX;
	unsigned int i;

	if (!sram_base)
		return;

	if (header[idx].num_of_lv != EXYNOS8895_G3D_OPP_COUNT) {
		pr_err("G3D FVMap: firmware has %u levels, source expects %u; refusing override\n",
		       header[idx].num_of_lv, EXYNOS8895_G3D_OPP_COUNT);
		return;
	}

	rv = sram_base + header[idx].o_ratevolt;

	if (header[idx].num_of_pll > 0) {
		clks = sram_base + header[idx].o_members;
		pll = sram_base + clks->addr[0];
	}

	pr_info("G3D FVMap: ACPM idx=%u levels=%u pll=%u ratevolt=0x%x members=0x%x\n",
		idx, header[idx].num_of_lv, header[idx].num_of_pll,
		header[idx].o_ratevolt, header[idx].o_members);

	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++) {
		const struct exynos8895_g3d_hardcoded_opp *opp =
			&exynos8895_g3d_opp_table[i];
		unsigned int old_rate = rv->table[i].rate;
		unsigned int old_volt = rv->table[i].volt;
		unsigned int old_pms = pll ? pll->pms[i] : 0;
		unsigned int desired_pms = EXYNOS8895_G3D_PACK_PMS(
			opp->pll_m, opp->pll_p, opp->pll_s);

		/* Existing slot only: never move FVMap offsets or increase num_of_lv. */
		rv->table[i].rate = opp->clock_khz;
		if (opp->voltage_uv)
			rv->table[i].volt = opp->voltage_uv;

		if (pll && opp->override_pms)
			pll->pms[i] = desired_pms;

		pr_info("G3D FVMap[%u]: rate %u->%u kHz volt %u->%u uV PMSraw=0x%08x desired=%u/%u/%u packed=0x%08x override=%u\n",
			i, old_rate, rv->table[i].rate,
			old_volt, rv->table[i].volt,
			old_pms, opp->pll_m, opp->pll_p, opp->pll_s,
			desired_pms, opp->override_pms);
	}
}
#endif

int fvmap_set_raw_voltage_table(unsigned int id, int uV)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int num_of_lv;
	int idx, i;

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		fv_table->table[i].volt += uV;

	return 0;
}

int fvmap_get_voltage_table(unsigned int id, unsigned int *table)
{
	struct fvmap_header *fvmap_header = fvmap_base;
	struct rate_volt_header *fv_table;
	int idx, i;
	int num_of_lv;

	if (!IS_ACPM_VCLK(id))
		return 0;

	idx = GET_IDX(id);

	fvmap_header = fvmap_base;
	fv_table = fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table->table[i].volt;

	return num_of_lv;

}

int fvmap_get_raw_voltage_table(unsigned int id)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int idx, i;
	int num_of_lv;
	unsigned int table[20];

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table->table[i].volt;

	for (i = 0; i < num_of_lv; i++)
		printk("dvfs id : %d  %d Khz : %d uv\n", ACPM_VCLK_TYPE | id, fv_table->table[i].rate, table[i]);

	return 0;
}

static void fvmap_copy_from_sram(void __iomem *map_base, void __iomem *sram_base)
{
	volatile struct fvmap_header *fvmap_header, *header;
	struct rate_volt_header *old, *new;
	struct clocks *clks;
	struct pll_header *plls;
	struct vclk *vclk;
	struct cmucal_clk *clk_node;
	unsigned int paddr_offset, fvaddr_offset;
	int size;
	int i, j;

	fvmap_header = map_base;
	header = sram_base;

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);

	for (i = 0; i < size; i++) {
		/* load fvmap info */
		fvmap_header[i].dvfs_type = header[i].dvfs_type;
		fvmap_header[i].num_of_lv = header[i].num_of_lv;
		fvmap_header[i].num_of_members = header[i].num_of_members;
		fvmap_header[i].num_of_pll = header[i].num_of_pll;
		fvmap_header[i].num_of_mux = header[i].num_of_mux;
		fvmap_header[i].num_of_div = header[i].num_of_div;
		fvmap_header[i].gearratio = header[i].gearratio;
		fvmap_header[i].init_lv = header[i].init_lv;
		fvmap_header[i].num_of_gate = header[i].num_of_gate;
		fvmap_header[i].reserved[0] = header[i].reserved[0];
		fvmap_header[i].reserved[1] = header[i].reserved[1];
		fvmap_header[i].block_addr[0] = header[i].block_addr[0];
		fvmap_header[i].block_addr[1] = header[i].block_addr[1];
		fvmap_header[i].block_addr[2] = header[i].block_addr[2];
		fvmap_header[i].o_members = header[i].o_members;
		fvmap_header[i].o_ratevolt = header[i].o_ratevolt;
		fvmap_header[i].o_tables = header[i].o_tables;

		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		if (vclk == NULL)
			continue;
		pr_info("dvfs_type : %s - id : %x\n",
			vclk->name, fvmap_header[i].dvfs_type);
		pr_info("  num_of_lv      : %d\n", fvmap_header[i].num_of_lv);
		pr_info("  num_of_members : %d\n", fvmap_header[i].num_of_members);

		old = sram_base + fvmap_header[i].o_ratevolt;
		new = map_base + fvmap_header[i].o_ratevolt;
		if (init_margin_table[i])
			cal_dfs_set_volt_margin(i | ACPM_VCLK_TYPE,
						init_margin_table[i]);

		for (j = 0; j < fvmap_header[i].num_of_lv; j++) {

			/* increase cpucl1 voltages */
			if (strcmp(vclk->name, "dvfs_cpucl1") == 0) {
				if ((old->table[j].rate == 1898000) && (old->table[j].volt < 1200000))
					old->table[j].volt = 1200000;
				else if ((old->table[j].rate == 2002000) && (old->table[j].volt < 1300000))
					old->table[j].volt = 1300000;
			}

			/* increase cpucl0 voltages */
			if (strcmp(vclk->name, "dvfs_cpucl0") == 0) {
				if ((old->table[j].rate == 2652000) && (old->table[j].volt < 1150000))
					old->table[j].volt = 1150000;
				else if ((old->table[j].rate == 2704000) && (old->table[j].volt < 1175000))
					old->table[j].volt = 1175000;
				else if ((old->table[j].rate == 2808000) && (old->table[j].volt < 1400000))
					old->table[j].volt = 1400000;
			}

			new->table[j].rate = old->table[j].rate;
			new->table[j].volt = old->table[j].volt;
			pr_info("  lv : [%7d], volt = %d uV\n",
				new->table[j].rate, new->table[j].volt);
		}

		for (j = 0; j < fvmap_header[i].num_of_pll; j++) {
			clks = sram_base + fvmap_header[i].o_members;
			plls = sram_base + clks->addr[j];
			clk_node = cmucal_get_node(vclk->list[j]);
			if (clk_node == NULL)
				continue;
			paddr_offset = clk_node->paddr & 0xFFFF;
			fvaddr_offset = plls->addr & 0xFFFF;
			if (paddr_offset == fvaddr_offset)
				continue;

			clk_node->paddr += fvaddr_offset - paddr_offset;
			clk_node->pll_con0 += fvaddr_offset - paddr_offset;
			if (clk_node->pll_con1)
				clk_node->pll_con1 += fvaddr_offset - paddr_offset;
		}
	}
}

int fvmap_init(void __iomem *sram_base)
{
	void __iomem *map_base;

	map_base = kzalloc(FVMAP_SIZE, GFP_KERNEL);

	fvmap_base = map_base;
	sram_fvmap_base = sram_base;
	pr_info("%s:fvmap initialize %pK\n", __func__, sram_base);
#if defined(CONFIG_SOC_EXYNOS8895)
	fvmap_override_exynos8895_g3d(sram_base);
#endif
	fvmap_copy_from_sram(map_base, sram_base);

	if (IS_ENABLED(CONFIG_VDD_AUTO_CAL))
		exynos_acpm_vdd_auto_calibration(1);

	return 0;
}
