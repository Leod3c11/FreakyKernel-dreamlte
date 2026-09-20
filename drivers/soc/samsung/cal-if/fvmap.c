#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
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


/* Exact PLL-derived aliases reported by the supplied PLL_G3D all_dump. */

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



/*
 * EXYNOS8895-G3D-DYNAMIC-FVMAP-V12
 *
 * V11 assumed a fixed free tail at 0x1e00. Real devices may contain
 * firmware metadata there, which correctly made V11 abort and forced Mali
 * back to the Samsung/ASV path (five visible rows on this device).
 *
 * V12 allocates each enlarged G3D object independently. It derives an
 * occupancy map from the live FVMap, protects every referenced member object
 * conservatively, and reclaims only the three old G3D-owned objects.
 */
#define EXYNOS8895_G3D_MEMBER_GUARD_V12   0x0080U

static void exynos8895_g3d_mark_bytes(unsigned char *map,
				      unsigned int off,
				      unsigned int len,
				      unsigned char value)
{
	if (!map || !len || off >= FVMAP_SIZE)
		return;
	if (len > FVMAP_SIZE - off)
		len = FVMAP_SIZE - off;
	memset(map + off, value, len);
}

static int exynos8895_g3d_build_occupancy(unsigned char *used)
{
	struct fvmap_header *header = sram_fvmap_base;
	unsigned int domains = cmucal_get_list_size(ACPM_VCLK_TYPE);
	unsigned int i, j;

	if (!used || !header)
		return -EINVAL;

	memset(used, 0, FVMAP_SIZE);

	exynos8895_g3d_mark_bytes(used, 0,
		domains * sizeof(struct fvmap_header), 1);

	for (i = 0; i < domains; i++) {
		struct fvmap_header *h = &header[i];
		struct clocks *clks;
		unsigned int bytes;

		bytes = (unsigned int)h->num_of_lv * sizeof(struct rate_volt);
		if (!h->o_ratevolt || h->o_ratevolt + bytes > FVMAP_SIZE)
			return -EINVAL;
		exynos8895_g3d_mark_bytes(used, h->o_ratevolt, bytes, 1);

		bytes = (unsigned int)h->num_of_lv * h->num_of_members;
		if (bytes) {
			if (!h->o_tables || h->o_tables + bytes > FVMAP_SIZE)
				return -EINVAL;
			exynos8895_g3d_mark_bytes(used, h->o_tables, bytes, 1);
		}

		bytes = (unsigned int)h->num_of_members * sizeof(unsigned short);
		if (bytes) {
			if (!h->o_members || h->o_members + bytes > FVMAP_SIZE)
				return -EINVAL;
			exynos8895_g3d_mark_bytes(used, h->o_members, bytes, 1);
		}

		if (!h->num_of_members)
			continue;

		clks = sram_fvmap_base + h->o_members;

		for (j = 0; j < h->num_of_members; j++) {
			unsigned int off = clks->addr[j];
			unsigned int begin;

			if (off == 0xffffU)
				continue;
			if (off >= FVMAP_SIZE)
				return -EINVAL;

			begin = off >= 16U ? off - 16U : 0U;
			exynos8895_g3d_mark_bytes(
				used, begin,
				EXYNOS8895_G3D_MEMBER_GUARD_V12 + 16U, 1);

			if (j < h->num_of_pll) {
				struct pll_header *pll;
				unsigned int pll_bytes;

				if (off + sizeof(struct pll_header) > FVMAP_SIZE)
					return -EINVAL;

				pll = sram_fvmap_base + off;
				pll_bytes = sizeof(struct pll_header) +
					(unsigned int)pll->level *
					sizeof(unsigned int);

				if (off + pll_bytes > FVMAP_SIZE)
					return -EINVAL;

				exynos8895_g3d_mark_bytes(
					used, off, pll_bytes, 1);
			}
		}
	}

	return 0;
}

static bool exynos8895_g3d_region_available(
		unsigned char *used,
		unsigned char *reclaim,
		unsigned int off,
		unsigned int len)
{
	unsigned char *raw = sram_fvmap_base;
	unsigned int i;

	if (!used || !reclaim || !len)
		return false;
	if (off >= FVMAP_SIZE || len > FVMAP_SIZE - off)
		return false;

	for (i = 0; i < len; i++) {
		if (used[off + i])
			return false;
		if (raw[off + i] && !reclaim[off + i])
			return false;
	}

	return true;
}

static int exynos8895_g3d_find_region(
		unsigned char *used,
		unsigned char *reclaim,
		unsigned int preferred,
		unsigned int len,
		unsigned int align,
		unsigned int *result)
{
	unsigned int domains = cmucal_get_list_size(ACPM_VCLK_TYPE);
	unsigned int first = ALIGN(
		domains * sizeof(struct fvmap_header), align);
	unsigned int off;

	if (!result || !align || (align & (align - 1U)))
		return -EINVAL;

	if (!(preferred & (align - 1U)) &&
	    exynos8895_g3d_region_available(
		    used, reclaim, preferred, len)) {
		*result = preferred;
		exynos8895_g3d_mark_bytes(used, preferred, len, 1);
		return 0;
	}

	for (off = first; off + len <= FVMAP_SIZE; off += align) {
		if (!exynos8895_g3d_region_available(
			    used, reclaim, off, len))
			continue;

		*result = off;
		exynos8895_g3d_mark_bytes(used, off, len, 1);
		return 0;
	}

	return -ENOSPC;
}

static unsigned int exynos8895_g3d_find_init_level(unsigned int old_rate)
{
	unsigned int i;
	unsigned int best = EXYNOS8895_G3D_OPP_COUNT - 1;
	unsigned long best_delta = ~0UL;

	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++) {
		unsigned long rate = exynos8895_g3d_opp_table[i].clock_khz;
		unsigned long delta = rate > old_rate ? rate - old_rate : old_rate - rate;

		if (delta < best_delta) {
			best_delta = delta;
			best = i;
		}
	}
	return best;
}

static int exynos8895_g3d_write_expanded_map(struct fvmap_header *h,
					      unsigned int rv_off,
					      unsigned int table_off,
					      unsigned int pll_off,
					      unsigned int pll_addr,
					      unsigned int pll_lock,
					      unsigned int pms_flags)
{
	struct rate_volt_header *rv = sram_fvmap_base + rv_off;
	unsigned char *table = sram_fvmap_base + table_off;
	struct pll_header *pll = sram_fvmap_base + pll_off;
	unsigned int i;

	pll->addr = pll_addr;
	pll->o_lock = pll_lock;
	pll->level = EXYNOS8895_G3D_OPP_COUNT;

	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++) {
		const struct exynos8895_g3d_hardcoded_opp *opp =
			&exynos8895_g3d_opp_table[i];

		rv->table[i].rate = opp->clock_khz;
		rv->table[i].volt = opp->voltage_uv;
		table[i] = (unsigned char)i;
		pll->pms[i] = pms_flags | EXYNOS8895_G3D_PACK_PMS(
			opp->pll_m, opp->pll_p, opp->pll_s);
	}

	return 0;
}

bool exynos8895_g3d_hardcoded_sync_cal(void)
{
	struct vclk *vclk;

	vclk = cmucal_get_node(ACPM_VCLK_TYPE | EXYNOS8895_G3D_ACPM_INDEX);
	if (!vclk)
		return false;

	/*
	 * ECT allocated the original 9-entry LUT.  Do not overrun it after the
	 * ACPM FVMap grows.  Public CAL table/count queries are already sourced
	 * from exynos8895-hardcoded-profile.h, and G3D set-rate has its own ACPM
	 * path, so only the range/cache fields need synchronization here.
	 */
	vclk->max_freq = exynos8895_g3d_opp_table[0].clock_khz;
	vclk->min_freq = exynos8895_g3d_opp_table[EXYNOS8895_G3D_OPP_COUNT - 1].clock_khz;
	vclk->boot_freq = vclk->min_freq;
	vclk->resume_freq = vclk->min_freq;
	exynos8895_g3d_cal_active = true;

	pr_info("G3D hardcoded: CAL source view=%u rows %u..%u kHz (ECT LUT remains %u rows)\n",
		EXYNOS8895_G3D_OPP_COUNT, vclk->max_freq, vclk->min_freq,
		vclk->num_rates);
	return true;
}
EXPORT_SYMBOL_GPL(exynos8895_g3d_hardcoded_sync_cal);

int exynos8895_g3d_hardcoded_apply(void)
{
	struct fvmap_header *header;
	struct fvmap_header *h;
	struct rate_volt_header *old_rv;
	struct clocks *clks;
	struct pll_header *old_pll;
	struct pll_header *new_pll;
	unsigned char *used = NULL;
	unsigned char *reclaim = NULL;
	unsigned int idx = EXYNOS8895_G3D_ACPM_INDEX;
	unsigned int rv_off;
	unsigned int table_off;
	unsigned int pll_off;
	unsigned int rv_bytes;
	unsigned int table_bytes;
	unsigned int pll_bytes;
	unsigned int old_rv_bytes;
	unsigned int old_table_bytes;
	unsigned int old_pll_bytes;
	unsigned int old_init_rate = 0;
	unsigned int old_pll_off;
	unsigned int pms_flags;
	unsigned int i;
	int ret;

	if (!sram_fvmap_base)
		return -EAGAIN;
	if (!exynos8895_g3d_source_table_valid())
		return -EINVAL;
	if (EXYNOS8895_G3D_OPP_COUNT > EXYNOS8895_G3D_MAX_SOURCE_OPPS)
		return -E2BIG;

	header = sram_fvmap_base;
	h = &header[idx];

	if (h->num_of_pll != 1 || h->num_of_members != 1 ||
	    !h->o_members || !h->o_ratevolt || !h->o_tables)
		return -ENODEV;

	clks = sram_fvmap_base + h->o_members;

	rv_bytes = EXYNOS8895_G3D_OPP_COUNT *
		sizeof(struct rate_volt);
	table_bytes = EXYNOS8895_G3D_OPP_COUNT * h->num_of_members;
	pll_bytes = sizeof(struct pll_header) +
		EXYNOS8895_G3D_OPP_COUNT * sizeof(unsigned int);

	if (h->num_of_lv == EXYNOS8895_G3D_OPP_COUNT) {
		rv_off = h->o_ratevolt;
		table_off = h->o_tables;
		pll_off = clks->addr[0];

		if (rv_off + rv_bytes > FVMAP_SIZE ||
		    table_off + table_bytes > FVMAP_SIZE ||
		    pll_off + pll_bytes > FVMAP_SIZE)
			return -EINVAL;

		new_pll = sram_fvmap_base + pll_off;
		if ((new_pll->addr & 0xffffU) !=
				EXYNOS8895_G3D_PLL_SFR_LO ||
		    new_pll->level != EXYNOS8895_G3D_OPP_COUNT)
			return -ENODEV;

		pms_flags = new_pll->pms[0] &
			~EXYNOS8895_G3D_PMS_MASK;

		exynos8895_g3d_write_expanded_map(
			h, rv_off, table_off, pll_off,
			new_pll->addr, new_pll->o_lock, pms_flags);

		exynos8895_g3d_sram_active = true;
		pr_info("G3D V12: existing expanded FVMap refreshed rv=0x%x table=0x%x pll=0x%x\n",
			rv_off, table_off, pll_off);
		return 0;
	}

	if (h->num_of_lv != EXYNOS8895_G3D_STOCK_FVMAP_COUNT)
		return -ENODEV;

	old_rv_bytes = EXYNOS8895_G3D_STOCK_FVMAP_COUNT *
		sizeof(struct rate_volt);
	old_table_bytes = EXYNOS8895_G3D_STOCK_FVMAP_COUNT *
		h->num_of_members;

	if (h->o_ratevolt + old_rv_bytes > FVMAP_SIZE ||
	    h->o_tables + old_table_bytes > FVMAP_SIZE)
		return -EINVAL;

	old_rv = sram_fvmap_base + h->o_ratevolt;

	for (i = 0; i < EXYNOS8895_G3D_STOCK_FVMAP_COUNT; i++) {
		unsigned int rate = old_rv->table[i].rate;

		if (rate != exynos8895_g3d_stock_rate[i] &&
		    rate != exynos8895_g3d_stock_pll_rate[i]) {
			pr_err("G3D V12: stock slot %u unexpected rate %u\n",
				i, rate);
			return -EINVAL;
		}
	}

	if (h->init_lv < EXYNOS8895_G3D_STOCK_FVMAP_COUNT)
		old_init_rate = old_rv->table[h->init_lv].rate;

	old_pll_off = clks->addr[0];
	if (old_pll_off + sizeof(struct pll_header) > FVMAP_SIZE)
		return -EINVAL;

	old_pll = sram_fvmap_base + old_pll_off;
	old_pll_bytes = sizeof(struct pll_header) +
		(unsigned int)old_pll->level * sizeof(unsigned int);

	if ((old_pll->addr & 0xffffU) !=
			EXYNOS8895_G3D_PLL_SFR_LO ||
	    old_pll->level < EXYNOS8895_G3D_STOCK_FVMAP_COUNT ||
	    old_pll_off + old_pll_bytes > FVMAP_SIZE)
		return -ENODEV;

	used = kzalloc(FVMAP_SIZE, GFP_KERNEL);
	reclaim = kzalloc(FVMAP_SIZE, GFP_KERNEL);
	if (!used || !reclaim) {
		ret = -ENOMEM;
		goto out;
	}

	ret = exynos8895_g3d_build_occupancy(used);
	if (ret)
		goto out;

	exynos8895_g3d_mark_bytes(
		reclaim, h->o_ratevolt, old_rv_bytes, 1);
	exynos8895_g3d_mark_bytes(
		reclaim, h->o_tables, old_table_bytes, 1);
	exynos8895_g3d_mark_bytes(
		reclaim, old_pll_off, old_pll_bytes, 1);

	exynos8895_g3d_mark_bytes(
		used, h->o_ratevolt, old_rv_bytes, 0);
	exynos8895_g3d_mark_bytes(
		used, h->o_tables, old_table_bytes, 0);
	exynos8895_g3d_mark_bytes(
		used, old_pll_off, old_pll_bytes, 0);

	ret = exynos8895_g3d_find_region(
		used, reclaim, h->o_ratevolt,
		rv_bytes, 4U, &rv_off);
	if (ret) {
		pr_err("G3D V12: no %u-byte ratevolt region (%d)\n",
			rv_bytes, ret);
		goto out;
	}

	ret = exynos8895_g3d_find_region(
		used, reclaim, old_pll_off,
		pll_bytes, 4U, &pll_off);
	if (ret) {
		pr_err("G3D V12: no %u-byte PLL region (%d)\n",
			pll_bytes, ret);
		goto out;
	}

	ret = exynos8895_g3d_find_region(
		used, reclaim, h->o_tables,
		table_bytes, 4U, &table_off);
	if (ret) {
		pr_err("G3D V12: no %u-byte table region (%d)\n",
			table_bytes, ret);
		goto out;
	}

	pms_flags = old_pll->pms[0] &
		~EXYNOS8895_G3D_PMS_MASK;

	exynos8895_g3d_write_expanded_map(
		h, rv_off, table_off, pll_off,
		old_pll->addr, old_pll->o_lock, pms_flags);

	wmb();
	clks->addr[0] = pll_off;
	h->o_ratevolt = rv_off;
	h->o_tables = table_off;
	h->init_lv =
		exynos8895_g3d_find_init_level(old_init_rate);
	wmb();
	h->num_of_lv = EXYNOS8895_G3D_OPP_COUNT;
	wmb();

	exynos8895_g3d_sram_active = true;

	pr_info("G3D V12: ACPM FVMap %u -> %u levels rv=0x%x table=0x%x pll=0x%x init=%u\n",
		EXYNOS8895_G3D_STOCK_FVMAP_COUNT,
		EXYNOS8895_G3D_OPP_COUNT,
		rv_off, table_off, pll_off, h->init_lv);
	pr_emerg("G3D_FLIGHT FVMAP_EXPANDED levels=%u rv=0x%x table=0x%x pll=0x%x init=%u\n",
		EXYNOS8895_G3D_OPP_COUNT, rv_off, table_off, pll_off, h->init_lv);

	ret = 0;

out:
	kfree(reclaim);
	kfree(used);
	return ret;
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


#if defined(CONFIG_SOC_EXYNOS8895)
/*
 * Generic Exynos8895 ACPM/FVMap probe.
 *
 * READ ONLY.
 */
static struct dentry *exynos8895_soc_debugfs_root;

static const char * const exynos8895_soc_domain_name[] = {
	"dvfs_mif",
	"dvfs_int",
	"dvfs_cpucl0",
	"dvfs_cpucl1",
	"dvfs_g3d",
	"dvfs_intcam",
	"dvfs_cam",
	"dvfs_disp",
	"dvs_g3dm",
	"dvs_cp",
};

#define EXYNOS8895_SOC_ACPM_DOMAINS \
	(sizeof(exynos8895_soc_domain_name) / sizeof(exynos8895_soc_domain_name[0]))

static bool exynos8895_fvmap_range_ok(unsigned int off, unsigned int bytes)
{
	if (off >= FVMAP_SIZE)
		return false;
	if (bytes > FVMAP_SIZE)
		return false;
	if (off > FVMAP_SIZE - bytes)
		return false;
	return true;
}

static int exynos8895_soc_fvmap_show(struct seq_file *m, void *unused)
{
	struct fvmap_header *header;
	unsigned int domain_count;
	unsigned int i, j, p;

	if (!sram_fvmap_base) {
		seq_puts(m, "ERROR sram_fvmap_base=unavailable\n");
		return 0;
	}

	header = sram_fvmap_base;
	domain_count = cmucal_get_list_size(ACPM_VCLK_TYPE);
	if (domain_count > EXYNOS8895_SOC_ACPM_DOMAINS)
		domain_count = EXYNOS8895_SOC_ACPM_DOMAINS;

	seq_printf(m,
		"EXYNOS8895_FVMAP size=%u domains=%u acpm_list=%u\n",
		(unsigned int)FVMAP_SIZE, domain_count,
		cmucal_get_list_size(ACPM_VCLK_TYPE));

	for (i = 0; i < domain_count; i++) {
		struct fvmap_header *h = &header[i];
		struct rate_volt_header *rv;
		struct clocks *clks;
		struct vclk *vclk;
		unsigned int rv_bytes;
		unsigned int member_bytes;

		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);

		seq_printf(m,
			"\n=== DOMAIN %u %s ===\n"
			"id=0x%x vclk=%s\n"
			"dvfs_type=0x%x levels=%u members=%u pll=%u mux=%u div=%u gate=%u init_lv=%u gearratio=%u\n"
			"block_addr=%04x,%04x,%04x o_members=0x%x o_ratevolt=0x%x o_tables=0x%x\n",
			i, exynos8895_soc_domain_name[i],
			ACPM_VCLK_TYPE | i,
			(vclk && vclk->name) ? vclk->name : "NULL",
			h->dvfs_type, h->num_of_lv, h->num_of_members,
			h->num_of_pll, h->num_of_mux, h->num_of_div,
			h->num_of_gate, h->init_lv, h->gearratio,
			h->block_addr[0], h->block_addr[1], h->block_addr[2],
			h->o_members, h->o_ratevolt, h->o_tables);

		rv_bytes = sizeof(struct rate_volt) * h->num_of_lv;
		if (!exynos8895_fvmap_range_ok(h->o_ratevolt, rv_bytes)) {
			seq_printf(m,
				"ERROR ratevolt range off=0x%x bytes=%u outside FVMAP\n",
				h->o_ratevolt, rv_bytes);
			continue;
		}

		rv = sram_fvmap_base + h->o_ratevolt;
		seq_puts(m, "LEVEL rate_khz volt_uv\n");
		for (j = 0; j < h->num_of_lv; j++)
			seq_printf(m, "%u %u %u\n",
				j, rv->table[j].rate, rv->table[j].volt);

		member_bytes = sizeof(unsigned short) * h->num_of_members;
		if (!h->num_of_members)
			continue;

		if (!exynos8895_fvmap_range_ok(h->o_members, member_bytes)) {
			seq_printf(m,
				"ERROR members range off=0x%x bytes=%u outside FVMAP\n",
				h->o_members, member_bytes);
			continue;
		}

		clks = sram_fvmap_base + h->o_members;

		seq_puts(m, "MEMBERS idx offset cal_id\n");
		for (j = 0; j < h->num_of_members; j++) {
			unsigned int cal_id = 0;

			if (vclk && vclk->list && j < vclk->num_list)
				cal_id = vclk->list[j];

			seq_printf(m, "%u 0x%04x 0x%x\n",
				j, clks->addr[j], cal_id);
		}

		for (p = 0; p < h->num_of_pll && p < h->num_of_members; p++) {
			struct pll_header *pll;
			unsigned int off = clks->addr[p];
			unsigned int levels;
			unsigned int bytes;

			if (!exynos8895_fvmap_range_ok(off,
						sizeof(struct pll_header))) {
				seq_printf(m,
					"PLL%u ERROR header offset=0x%x outside FVMAP\n",
					p, off);
				continue;
			}

			pll = sram_fvmap_base + off;
			levels = pll->level;
			if (levels > h->num_of_lv)
				levels = h->num_of_lv;

			bytes = sizeof(struct pll_header) +
				sizeof(unsigned int) * levels;
			if (!exynos8895_fvmap_range_ok(off, bytes)) {
				seq_printf(m,
					"PLL%u ERROR offset=0x%x bytes=%u outside FVMAP\n",
					p, off, bytes);
				continue;
			}

			seq_printf(m,
				"PLL%u offset=0x%x addr=0x%08x lock_off=0x%x levels=%u raw_levels=%u\n",
				p, off, pll->addr, pll->o_lock, levels, pll->level);
			seq_puts(m, "PLL_LEVEL idx raw_pms M P S\n");

			for (j = 0; j < levels; j++) {
				unsigned int raw = pll->pms[j];
				unsigned int pm = (raw >> 16) & 0x3ffU;
				unsigned int pp = (raw >> 8) & 0x3fU;
				unsigned int ps = raw & 0x7U;

				seq_printf(m, "%u 0x%08x %u %u %u\n",
					j, raw, pm, pp, ps);
			}
		}
	}

	return 0;
}

static int exynos8895_soc_live_show(struct seq_file *m, void *unused)
{
	unsigned int count;
	unsigned int i;

	count = cmucal_get_list_size(ACPM_VCLK_TYPE);
	if (count > EXYNOS8895_SOC_ACPM_DOMAINS)
		count = EXYNOS8895_SOC_ACPM_DOMAINS;

	seq_puts(m, "idx name id current_khz min_khz max_khz levels\n");
	for (i = 0; i < count; i++) {
		unsigned int id = ACPM_VCLK_TYPE | i;
		unsigned long cur = cal_dfs_get_rate(id);
		unsigned long min = cal_dfs_get_min_freq(id);
		unsigned long max = cal_dfs_get_max_freq(id);
		unsigned int lv = cal_dfs_get_lv_num(id);

		seq_printf(m, "%u %s 0x%x %lu %lu %lu %u\n",
			i, exynos8895_soc_domain_name[i], id,
			cur, min, max, lv);
	}

	return 0;
}


static const char *exynos8895_hc_trans_name(enum exynos8895_hc_transition_owner o)
{
	switch (o) {
	case EXYNOS8895_HC_TRANS_ACPM:
		return "acpm";
	case EXYNOS8895_HC_TRANS_HYBRID:
		return "hybrid";
	case EXYNOS8895_HC_TRANS_DIRECT:
		return "direct";
	case EXYNOS8895_HC_TRANS_FIRMWARE_ONLY:
		return "firmware-only";
	default:
		return "unknown";
	}
}

static int exynos8895_soc_profile_show(struct seq_file *m, void *unused)
{
	unsigned int i, j;

	seq_printf(m, "EXYNOS8895_HARDCODED_PROFILE version=%u\n",
		   EXYNOS8895_HC_PROFILE_VERSION);
	seq_puts(m,
		 "idx name policy_min policy_max table_min table_max levels members pll mux div transition regulator\n");

	for (i = 0; i < EXYNOS8895_HC_DOMAIN_COUNT; i++) {
		const struct exynos8895_hc_domain_desc *d =
			exynos8895_hc_domain(i);

		seq_printf(m, "%u %s %u %u %u %u %u %u %u %u %u %s %s\n",
			   i, d->name,
			   d->policy_min_khz, d->policy_max_khz,
			   d->table_min_khz, d->table_max_khz,
			   d->level_count, d->member_count, d->pll_count,
			   d->mux_count, d->div_count,
			   exynos8895_hc_trans_name(d->transition_owner),
			   d->regulator_name ? d->regulator_name : "firmware/shared");

		seq_puts(m, "  LEVEL rate_khz volt_uv policy_ok raw_ok\n");
		for (j = 0; j < d->level_count; j++) {
			unsigned int rate = exynos8895_hc_level_rate(i, j);
			unsigned int volt = exynos8895_hc_level_voltage(i, j);

			seq_printf(m, "  %u %u %u %u %u\n",
				   j, rate, volt,
				   exynos8895_hc_rate_allowed(i, rate, false),
				   exynos8895_hc_rate_allowed(i, rate, true));
		}
	}

	return 0;
}

static int exynos8895_soc_lut_show(struct seq_file *m, void *unused)
{
	unsigned int count;
	unsigned int i, j, k;

	count = cmucal_get_list_size(ACPM_VCLK_TYPE);
	if (count > EXYNOS8895_HC_DOMAIN_COUNT)
		count = EXYNOS8895_HC_DOMAIN_COUNT;

	for (i = 0; i < count; i++) {
		struct vclk *vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);

		seq_printf(m, "\n=== LUT %u %s ===\n", i,
			   exynos8895_hc_domain(i)->name);
		if (!vclk || !vclk->lut || !vclk->list) {
			seq_puts(m, "unavailable\n");
			continue;
		}

		seq_printf(m,
			   "num_rates=%u num_list=%u switch_info=%u seq=%u\n",
			   vclk->num_rates, vclk->num_list,
			   vclk->switch_info ? 1 : 0,
			   vclk->seq ? 1 : 0);

		seq_puts(m, "MEMBERS idx cal_id\n");
		for (j = 0; j < vclk->num_list; j++)
			seq_printf(m, "%u 0x%x\n", j, vclk->list[j]);

		for (j = 0; j < vclk->num_rates; j++) {
			seq_printf(m, "RATE %u %u PARAMS",
				   j, vclk->lut[j].rate);
			for (k = 0; k < vclk->num_list; k++)
				seq_printf(m, " %d", vclk->lut[j].params[k]);
			seq_putc(m, '\n');
		}
	}

	return 0;
}



static int exynos8895_soc_verify_show(struct seq_file *m, void *unused)
{
	unsigned int count, i, j, k;
	unsigned int total_errors = 0;

	count = cmucal_get_list_size(ACPM_VCLK_TYPE);
	if (count > EXYNOS8895_HC_DOMAIN_COUNT)
		count = EXYNOS8895_HC_DOMAIN_COUNT;

	seq_printf(m, "EXYNOS8895_HC_VERIFY profile=%u domains=%u\n",
		   EXYNOS8895_HC_PROFILE_VERSION, count);

	for (i = 0; i < count; i++) {
		const struct exynos8895_hc_domain_desc *d =
			exynos8895_hc_domain(i);
		const struct exynos8895_hc_lut_desc *hl =
			exynos8895_hc_lut_desc(i);
		struct vclk *vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		unsigned int errors = 0;

		if (!d || !hl || !vclk || !vclk->lut || !vclk->list) {
			seq_printf(m, "%u %s ERROR unavailable\n",
				   i, d ? d->name : "unknown");
			total_errors++;
			continue;
		}

		if (vclk->num_rates != hl->rows)
			errors++;
		if (vclk->num_list != hl->width)
			errors++;
		if (d->member_count != vclk->num_list)
			errors++;

		if (d->member_count == vclk->num_list)
			for (j = 0; j < vclk->num_list; j++)
				if (d->members[j].cal_id != vclk->list[j])
					errors++;

		if (vclk->num_rates == hl->rows &&
		    vclk->num_list == hl->width)
			for (j = 0; j < hl->rows; j++)
				for (k = 0; k < hl->width; k++)
					if (hl->params[j * hl->width + k] !=
					    vclk->lut[j].params[k])
						errors++;

		seq_printf(m,
			   "%u %s %s errors=%u rows=%u/%u width=%u/%u\n",
			   i, d->name, errors ? "MISMATCH" : "OK",
			   errors, vclk->num_rates, hl->rows,
			   vclk->num_list, hl->width);
		total_errors += errors;
	}

	seq_printf(m, "RESULT %s total_errors=%u\n",
		   total_errors ? "FAIL" : "PASS", total_errors);
	return 0;
}

static int exynos8895_soc_verify_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos8895_soc_verify_show, inode->i_private);
}

static const struct file_operations exynos8895_soc_verify_fops = {
	.owner = THIS_MODULE,
	.open = exynos8895_soc_verify_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int exynos8895_soc_profile_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos8895_soc_profile_show, inode->i_private);
}

static int exynos8895_soc_lut_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos8895_soc_lut_show, inode->i_private);
}

static const struct file_operations exynos8895_soc_profile_fops = {
	.owner = THIS_MODULE,
	.open = exynos8895_soc_profile_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations exynos8895_soc_lut_fops = {
	.owner = THIS_MODULE,
	.open = exynos8895_soc_lut_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int exynos8895_soc_fvmap_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos8895_soc_fvmap_show, inode->i_private);
}

static int exynos8895_soc_live_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos8895_soc_live_show, inode->i_private);
}

static const struct file_operations exynos8895_soc_fvmap_fops = {
	.owner = THIS_MODULE,
	.open = exynos8895_soc_fvmap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations exynos8895_soc_live_fops = {
	.owner = THIS_MODULE,
	.open = exynos8895_soc_live_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int exynos8895_soc_find_domain(const char *name)
{
	unsigned int i;

	if (!name || !*name)
		return -EINVAL;

	for (i = 0; i < EXYNOS8895_SOC_ACPM_DOMAINS; i++) {
		if (!strcmp(name, exynos8895_soc_domain_name[i]))
			return i;

		/* Friendly aliases for shell use. */
		if (!strncmp(exynos8895_soc_domain_name[i], "dvfs_", 5) &&
		    !strcmp(name, exynos8895_soc_domain_name[i] + 5))
			return i;
		if (!strncmp(exynos8895_soc_domain_name[i], "dvs_", 4) &&
		    !strcmp(name, exynos8895_soc_domain_name[i] + 4))
			return i;
	}

	return -ENOENT;
}

static bool exynos8895_soc_rate_supported(unsigned int id,
					   unsigned long rate,
					   bool raw)
{
	unsigned int idx;

	if (!IS_ACPM_VCLK(id))
		return false;

	idx = GET_IDX(id);
	return exynos8895_hc_rate_allowed(idx, rate, raw);
}

static ssize_t exynos8895_soc_control_read(struct file *file,
					    char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	static const char help[] =
		"Exynos8895 SoC runtime control\n"
		"WRITE commands:\n"
		"  rate <domain> <kHz>       (respects policy_min/max)\n"
		"  rate_raw <domain> <kHz>   (TEMPORARILY DISABLED IN v6)\n"
		"  margin <domain> <delta_uV>\n"
		"\n"
		"domains: mif int cpucl0 cpucl1 g3d intcam cam disp g3dm cp\n"
		"\n"
		"rate is limited by the central profile policy.\n"
		"rate_raw is disabled until direct-transition validation passes.\n"
		"margin uses cal_dfs_set_volt_margin(); it is a voltage DELTA, not absolute uV.\n"
		"CPU/devfreq governors may change a requested rate again after this write.\n";

	return simple_read_from_buffer(ubuf, count, ppos, help, sizeof(help) - 1);
}

static ssize_t exynos8895_soc_control_write(struct file *file,
					     const char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	char buf[96];
	char cmd[16];
	char domain[24];
	long value;
	int idx;
	unsigned int id;
	int ret;

	if (!count || count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;
	buf[count] = '\0';

	if (sscanf(buf, "%15s %23s %ld", cmd, domain, &value) != 3)
		return -EINVAL;

	idx = exynos8895_soc_find_domain(domain);
	if (idx < 0)
		return idx;

	id = ACPM_VCLK_TYPE | idx;

	if (!strcmp(cmd, "rate") || !strcmp(cmd, "rate_raw")) {
		bool raw = !strcmp(cmd, "rate_raw");

		if (value <= 0)
			return -EINVAL;

		if (!exynos8895_soc_rate_supported(id, value, raw)) {
			pr_err("Exynos8895 SoC control: rejected %s domain=%s rate=%ld\n",
			       raw ? "rate_raw" : "rate", domain, value);
			return -EPERM;
		}

		if (raw)
			pr_warn("Exynos8895 SoC control: RAW policy bypass domain=%s rate=%ld\n",
				domain, value);

		ret = cal_dfs_set_rate(id, value);
		if (ret) {
			pr_err("Exynos8895 SoC control: rate failed domain=%s rate=%ld ret=%d\n",
			       domain, value, ret);
			return ret;
		}

		pr_info("Exynos8895 SoC control: %s domain=%s requested=%ld actual=%lu\n",
			raw ? "rate_raw" : "rate",
			domain, value, cal_dfs_get_rate(id));
		return count;
	}

	if (!strcmp(cmd, "margin")) {
		/*
		 * Guard against catastrophic typo while still leaving a large
		 * engineering range.  This is a DELTA applied by Samsung CAL/ACPM.
		 */
		if (value < -200000 || value > 200000)
			return -ERANGE;

		cal_dfs_set_volt_margin(id, value);
		pr_info("Exynos8895 SoC control: margin domain=%s delta=%lduV\n",
			domain, value);
		return count;
	}

	return -EINVAL;
}

static const struct file_operations exynos8895_soc_control_fops = {
	.owner = THIS_MODULE,
	.read = exynos8895_soc_control_read,
	.write = exynos8895_soc_control_write,
	.llseek = default_llseek,
};

static void exynos8895_soc_debugfs_init(void)
{
	if (exynos8895_soc_debugfs_root)
		return;

	exynos8895_soc_debugfs_root =
		debugfs_create_dir("exynos8895_soc", NULL);
	if (IS_ERR_OR_NULL(exynos8895_soc_debugfs_root)) {
		exynos8895_soc_debugfs_root = NULL;
		return;
	}

	debugfs_create_file("fvmap", 0444, exynos8895_soc_debugfs_root,
			    NULL, &exynos8895_soc_fvmap_fops);
	debugfs_create_file("live", 0444, exynos8895_soc_debugfs_root,
			    NULL, &exynos8895_soc_live_fops);
	debugfs_create_file("profile", 0444, exynos8895_soc_debugfs_root,
			    NULL, &exynos8895_soc_profile_fops);
	debugfs_create_file("lut", 0444, exynos8895_soc_debugfs_root,
			    NULL, &exynos8895_soc_lut_fops);
	debugfs_create_file("verify", 0444, exynos8895_soc_debugfs_root,
			    NULL, &exynos8895_soc_verify_fops);
	debugfs_create_file("control", 0600, exynos8895_soc_debugfs_root,
			    NULL, &exynos8895_soc_control_fops);

	pr_info("Exynos8895 SoC probe: debugfs ready at /sys/kernel/debug/exynos8895_soc\n");
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
#if defined(CONFIG_SOC_EXYNOS8895)
	int g3d_ret;
#endif

	map_base = kzalloc(FVMAP_SIZE, GFP_KERNEL);
	if (!map_base)
		return -ENOMEM;

	fvmap_base = map_base;
	sram_fvmap_base = sram_base;
#if defined(CONFIG_SOC_EXYNOS8895)
	exynos8895_soc_debugfs_init();
#endif
	pr_info("%s:fvmap initialize %pK\n", __func__, sram_base);

#if defined(CONFIG_SOC_EXYNOS8895)
	/*
	 * Grow G3D in the live ACPM map first using the V12 dynamic allocator.
	 * If allocation/identity validation fails, keep Samsung's map intact.
	 */
	g3d_ret = exynos8895_g3d_hardcoded_apply();
	if (g3d_ret)
		pr_err("G3D expand: keeping stock FVMap (%d)\n", g3d_ret);
#endif

	fvmap_copy_from_sram(map_base, sram_base);
#if defined(CONFIG_SOC_EXYNOS8895)
	if (!g3d_ret)
		exynos8895_g3d_hardcoded_sync_cal();
#endif

	if (IS_ENABLED(CONFIG_VDD_AUTO_CAL))
		exynos_acpm_vdd_auto_calibration(1);

	return 0;
}
