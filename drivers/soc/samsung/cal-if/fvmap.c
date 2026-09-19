#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/export.h>
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
static bool exynos8895_g3d_sram_active;
static bool exynos8895_g3d_cal_active;

bool exynos8895_g3d_hardcoded_active(void)
{
	return exynos8895_g3d_sram_active;
}
EXPORT_SYMBOL_GPL(exynos8895_g3d_hardcoded_active);

static const unsigned int exynos8895_g3d_stock_rate[EXYNOS8895_G3D_OPP_COUNT] = {
	839000, 764000, 683000, 572000, 546000, 455000, 385000, 338000, 260000,
};

/* Exact PLL-derived aliases reported by the supplied PLL_G3D all_dump. */
static const unsigned int exynos8895_g3d_stock_pll_rate[EXYNOS8895_G3D_OPP_COUNT] = {
	838000, 764000, 682000, 572000, 546000, 455000, 384000, 338000, 260000,
};

static bool exynos8895_g3d_opp_valid(
	const struct exynos8895_g3d_hardcoded_opp *opp)
{
	unsigned long long lhs, rhs, fvco_num;

	if (!opp->pll_p || opp->pll_p > 63 ||
	    opp->pll_m < 64 || opp->pll_m > 1023 || opp->pll_s > 6)
		return false;

	if ((unsigned long long)2000 * opp->pll_p > EXYNOS8895_G3D_PLL_FIN_KHZ ||
	    (unsigned long long)8000 * opp->pll_p < EXYNOS8895_G3D_PLL_FIN_KHZ)
		return false;

	fvco_num = (unsigned long long)EXYNOS8895_G3D_PLL_FIN_KHZ * opp->pll_m;
	if (fvco_num < (unsigned long long)600000 * opp->pll_p ||
	    fvco_num > (unsigned long long)1200000 * opp->pll_p)
		return false;

	if (opp->clock_khz < 9500 || opp->clock_khz > 1200000)
		return false;

	lhs = (unsigned long long)EXYNOS8895_G3D_PLL_FIN_KHZ * opp->pll_m;
	rhs = (unsigned long long)opp->clock_khz * opp->pll_p *
	      (1ULL << opp->pll_s);
	if (lhs != rhs)
		return false;

	if (!opp->voltage_uv || opp->voltage_uv < EXYNOS8895_G3D_MIN_UV ||
	    opp->voltage_uv > EXYNOS8895_G3D_MAX_UV)
		return false;

	return true;
}

static bool exynos8895_g3d_source_table_valid(void)
{
	unsigned int i;

	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++) {
		if (!exynos8895_g3d_opp_valid(&exynos8895_g3d_opp_table[i])) {
			pr_err("G3D hardcoded: invalid source row %u (%u kHz %u uV PMS=%u/%u/%u)\n",
			       i,
			       exynos8895_g3d_opp_table[i].clock_khz,
			       exynos8895_g3d_opp_table[i].voltage_uv,
			       exynos8895_g3d_opp_table[i].pll_m,
			       exynos8895_g3d_opp_table[i].pll_p,
			       exynos8895_g3d_opp_table[i].pll_s);
			return false;
		}
		if (i && exynos8895_g3d_opp_table[i - 1].clock_khz <=
			 exynos8895_g3d_opp_table[i].clock_khz) {
			pr_err("G3D hardcoded: rates must be strictly descending (row %u)\n", i);
			return false;
		}
	}
	return true;
}

bool exynos8895_g3d_hardcoded_sync_cal(void)
{
	struct vclk *vclk;
	unsigned int i;

	vclk = cmucal_get_node(ACPM_VCLK_TYPE | EXYNOS8895_G3D_ACPM_INDEX);
	if (!vclk || !vclk->lut || vclk->num_rates != EXYNOS8895_G3D_OPP_COUNT)
		return false;

	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++)
		vclk->lut[i].rate = exynos8895_g3d_opp_table[i].clock_khz;

	vclk->max_freq = exynos8895_g3d_opp_table[0].clock_khz;
	vclk->min_freq = exynos8895_g3d_opp_table[EXYNOS8895_G3D_OPP_COUNT - 1].clock_khz;
	vclk->boot_freq = vclk->min_freq;
	vclk->resume_freq = vclk->min_freq;
	exynos8895_g3d_cal_active = true;

	pr_info("G3D hardcoded: CAL view forced to %u..%u kHz (9 slots)\n",
		vclk->max_freq, vclk->min_freq);
	return true;
}
EXPORT_SYMBOL_GPL(exynos8895_g3d_hardcoded_sync_cal);

int exynos8895_g3d_hardcoded_apply(void)
{
	struct fvmap_header *sram_header;
	struct fvmap_header *copy_header;
	struct rate_volt_header *sram_rv;
	struct rate_volt_header *copy_rv = NULL;
	struct clocks *clks;
	struct pll_header *pll;
	unsigned int pll_offset;
	unsigned int idx = EXYNOS8895_G3D_ACPM_INDEX;
	unsigned int i;

	if (!sram_fvmap_base)
		return -EAGAIN;
	if (!exynos8895_g3d_source_table_valid())
		return -EINVAL;

	sram_header = sram_fvmap_base;
	if (sram_header[idx].num_of_lv != EXYNOS8895_G3D_OPP_COUNT ||
	    sram_header[idx].num_of_pll != 1 ||
	    sram_header[idx].num_of_members < 1) {
		pr_err("G3D hardcoded: FVMap identity mismatch idx=%u lv=%u members=%u pll=%u\n",
		       idx, sram_header[idx].num_of_lv,
		       sram_header[idx].num_of_members, sram_header[idx].num_of_pll);
		return -ENODEV;
	}

	if ((unsigned int)sram_header[idx].o_ratevolt +
	    sizeof(struct rate_volt) * EXYNOS8895_G3D_OPP_COUNT > FVMAP_SIZE ||
	    (unsigned int)sram_header[idx].o_members +
	    sizeof(unsigned short) * sram_header[idx].num_of_members > FVMAP_SIZE)
		return -EINVAL;

	sram_rv = sram_fvmap_base + sram_header[idx].o_ratevolt;
	clks = sram_fvmap_base + sram_header[idx].o_members;
	pll_offset = clks->addr[0];
	if (pll_offset >= FVMAP_SIZE ||
	    pll_offset + sizeof(struct pll_header) +
	    sizeof(unsigned int) * EXYNOS8895_G3D_OPP_COUNT > FVMAP_SIZE)
		return -EINVAL;
	pll = sram_fvmap_base + pll_offset;

	/*
	 * The live FVMap is authoritative for the PLL register relocation.
	 * Stock fvmap_copy_from_sram() performs the same low-16-bit comparison
	 * and moves the generated CMUCAL PLL from +0x120 to the SRAM offset.
	 * On this Exynos8895 device the real PLL_CON0_PLL_G3D is +0x140.
	 */
	if ((pll->addr & 0xffffU) != EXYNOS8895_G3D_PLL_SFR_LO) {
		pr_err("G3D hardcoded: ACPM idx4 PLL addr=0x%x, expected live G3D CON0 +0x%x\n",
		       pll->addr, EXYNOS8895_G3D_PLL_SFR_LO);
		return -ENODEV;
	}
	pr_info("G3D hardcoded: live PLL_G3D identity accepted addr=0x%x offset=0x%x\n",
		pll->addr, pll->addr & 0xffffU);

	/* Accept either untouched Samsung rates or our already-installed rates. */
	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++) {
		unsigned int rate = sram_rv->table[i].rate;
		if (rate != exynos8895_g3d_stock_rate[i] &&
		    rate != exynos8895_g3d_stock_pll_rate[i] &&
		    rate != exynos8895_g3d_opp_table[i].clock_khz) {
			pr_err("G3D hardcoded: SRAM rate slot %u unexpected %u (stock %u pll %u source %u)\n",
			       i, rate, exynos8895_g3d_stock_rate[i],
			       exynos8895_g3d_stock_pll_rate[i],
			       exynos8895_g3d_opp_table[i].clock_khz);
			return -EINVAL;
		}
	}

	/* fvmap_base is the CAL-visible copy. Mirror rate/voltage into it as well. */
	if (fvmap_base) {
		copy_header = fvmap_base;
		if (copy_header[idx].num_of_lv == EXYNOS8895_G3D_OPP_COUNT &&
		    copy_header[idx].o_ratevolt)
			copy_rv = fvmap_base + copy_header[idx].o_ratevolt;
	}

	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++) {
		const struct exynos8895_g3d_hardcoded_opp *opp =
			&exynos8895_g3d_opp_table[i];
		unsigned int old_pms = pll->pms[i];
		unsigned int new_pms = EXYNOS8895_G3D_PACK_PMS(
			opp->pll_m, opp->pll_p, opp->pll_s);

		/*
		 * Keep Samsung's nominal rate key in the live FVMap.  ACPM uses that
		 * key to select the slot; the slot's voltage/PMS own the real output.
		 */
		sram_rv->table[i].rate = opp->acpm_key_khz;
		sram_rv->table[i].volt = opp->voltage_uv;
		pll->pms[i] = (old_pms & ~EXYNOS8895_G3D_PMS_MASK) | new_pms;

		if (copy_rv) {
			copy_rv->table[i].rate = opp->acpm_key_khz;
			copy_rv->table[i].volt = opp->voltage_uv;
		}
	}

	exynos8895_g3d_sram_active = true;
	exynos8895_g3d_hardcoded_sync_cal();
	pr_info("G3D hardcoded: ACPM SRAM programmed idx=4 slots=9 logical=%u..%u keys=%u..%u cal=%u\n",
		exynos8895_g3d_opp_table[0].clock_khz,
		exynos8895_g3d_opp_table[EXYNOS8895_G3D_OPP_COUNT - 1].clock_khz,
		exynos8895_g3d_opp_table[0].acpm_key_khz,
		exynos8895_g3d_opp_table[EXYNOS8895_G3D_OPP_COUNT - 1].acpm_key_khz,
		exynos8895_g3d_cal_active ? 1 : 0);
	return 0;
}
EXPORT_SYMBOL_GPL(exynos8895_g3d_hardcoded_apply);

int exynos8895_g3d_sram_debug_dump(char *buf, unsigned int size)
{
	struct fvmap_header *header;
	struct rate_volt_header *rv;
	struct clocks *clks;
	struct pll_header *pll;
	unsigned int idx = EXYNOS8895_G3D_ACPM_INDEX;
	unsigned int pll_offset;
	unsigned int i;
	unsigned int len = 0;

	if (!buf || !size)
		return -EINVAL;
	if (!sram_fvmap_base)
		return scnprintf(buf, size, "sram=unavailable\n");

	header = sram_fvmap_base;
	len += scnprintf(buf + len, size - len,
		"G3D_SRAM idx=%u lv=%u members=%u pll=%u ratevolt_off=0x%x members_off=0x%x\n",
		idx, header[idx].num_of_lv, header[idx].num_of_members,
		header[idx].num_of_pll, header[idx].o_ratevolt,
		header[idx].o_members);

	if (header[idx].num_of_lv != EXYNOS8895_G3D_OPP_COUNT ||
	    header[idx].num_of_pll < 1 || header[idx].num_of_members < 1)
		return len + scnprintf(buf + len, size - len, "ERROR identity mismatch\n");

	if ((unsigned int)header[idx].o_ratevolt +
	    sizeof(struct rate_volt) * EXYNOS8895_G3D_OPP_COUNT > FVMAP_SIZE ||
	    (unsigned int)header[idx].o_members +
	    sizeof(unsigned short) * header[idx].num_of_members > FVMAP_SIZE)
		return len + scnprintf(buf + len, size - len, "ERROR offsets outside FVMAP_SIZE\n");

	rv = sram_fvmap_base + header[idx].o_ratevolt;
	clks = sram_fvmap_base + header[idx].o_members;
	pll_offset = clks->addr[0];
	len += scnprintf(buf + len, size - len, "pll_offset=0x%x\n", pll_offset);

	if (pll_offset >= FVMAP_SIZE ||
	    pll_offset + sizeof(struct pll_header) +
	    sizeof(unsigned int) * EXYNOS8895_G3D_OPP_COUNT > FVMAP_SIZE)
		return len + scnprintf(buf + len, size - len, "ERROR pll offset outside FVMAP_SIZE\n");

	pll = sram_fvmap_base + pll_offset;
	len += scnprintf(buf + len, size - len,
		"pll_addr=0x%08x expected_lo=0x%04x\n",
		pll->addr, EXYNOS8895_G3D_PLL_SFR_LO);
	len += scnprintf(buf + len, size - len,
		"slot live_rate live_uv pms_raw M P S | src_rate src_uv src_M src_P src_S acpm_key mif\n");

	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT && len < size; i++) {
		const struct exynos8895_g3d_hardcoded_opp *opp = &exynos8895_g3d_opp_table[i];
		unsigned int raw = pll->pms[i];
		unsigned int m = (raw >> 16) & 0x3ffU;
		unsigned int p = (raw >> 8) & 0x3fU;
		unsigned int s = raw & 0x7U;
		len += scnprintf(buf + len, size - len,
			"%u %u %u 0x%08x %u %u %u | %u %u %u %u %u %u %u\n",
			i, rv->table[i].rate, rv->table[i].volt, raw, m, p, s,
			opp->clock_khz, opp->voltage_uv, opp->pll_m, opp->pll_p,
			opp->pll_s, opp->acpm_key_khz, opp->mem_freq);
	}
	return len;
}
EXPORT_SYMBOL_GPL(exynos8895_g3d_sram_debug_dump);

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

	/* First snapshot the firmware headers/offsets, then replace G3D in both maps. */
	fvmap_copy_from_sram(map_base, sram_base);
#if defined(CONFIG_SOC_EXYNOS8895)
	exynos8895_g3d_hardcoded_apply();
#endif

	if (IS_ENABLED(CONFIG_VDD_AUTO_CAL))
		exynos_acpm_vdd_auto_calibration(1);

	return 0;
}
