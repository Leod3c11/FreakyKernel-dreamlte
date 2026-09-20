#ifndef __EXYNOS8895_HARDCODED_PROFILE_H__
#define __EXYNOS8895_HARDCODED_PROFILE_H__

/*
 * Exynos8895 Hardcoded SoC Profile
 *
 * Single editable source of truth for the SoC DVFS topology captured from
 * the target Galaxy S8 FVMap SRAM.
 *
 * IMPORTANT:
 *   - table_min/table_max describe rows physically present in FVMap/ECT.
 *   - policy_min/policy_max describe the range currently allowed by this
 *     device's ASV/policy.
 *   - voltage_uv == 0 means firmware did not provide a usable voltage for
 *     that row. Such a row is NOT automatically exposed to Linux devfreq
 *     and is rejected by runtime raw-rate control.
 *   - transition_owner describes who currently performs the physical
 *     transition. Only G3D is hybrid today.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#define EXYNOS8895_HC_PROFILE_VERSION 8U

enum exynos8895_hc_domain_id {
	EXYNOS8895_HC_MIF = 0,
	EXYNOS8895_HC_INT,
	EXYNOS8895_HC_CPUCL0,
	EXYNOS8895_HC_CPUCL1,
	EXYNOS8895_HC_G3D,
	EXYNOS8895_HC_INTCAM,
	EXYNOS8895_HC_CAM,
	EXYNOS8895_HC_DISP,
	EXYNOS8895_HC_G3DM,
	EXYNOS8895_HC_CP,
	EXYNOS8895_HC_DOMAIN_COUNT,
};

enum exynos8895_hc_transition_owner {
	EXYNOS8895_HC_TRANS_ACPM = 0,
	EXYNOS8895_HC_TRANS_HYBRID,
	EXYNOS8895_HC_TRANS_DIRECT,
	EXYNOS8895_HC_TRANS_FIRMWARE_ONLY,
};

/*
 * Compatibility with the first central-profile generation.
 * "owner" means table/policy ownership; transition_owner is the physical
 * transition authority and is the field to use for new code.
 */
enum exynos8895_hc_owner {
	EXYNOS8895_HC_OWNER_ACPM = 0,
	EXYNOS8895_HC_OWNER_HARDCODED = 1,
};

struct exynos8895_hc_simple_opp {
	unsigned int clock_khz;
	unsigned int voltage_uv;
};

struct exynos8895_hc_member {
	unsigned short fvmap_offset;
	unsigned int cal_id;
};

struct exynos8895_hc_pll_pms {
	unsigned short m;
	unsigned short p;
	unsigned short s;
};

struct exynos8895_hc_pll {
	unsigned int addr;
	unsigned short fvmap_offset;
	unsigned short raw_levels;
	unsigned int level_count;
	const struct exynos8895_hc_pll_pms *pms;
};

struct exynos8895_hc_domain_desc {
	const char *name;
	const char *devfreq_name; /* compatibility / Linux front-end name */
	const char *regulator_name;
	unsigned int acpm_index;

	unsigned int policy_min_khz;
	unsigned int policy_max_khz;
	unsigned int table_min_khz;
	unsigned int table_max_khz;

	unsigned int level_count;
	const struct exynos8895_hc_simple_opp *levels;

	unsigned int member_count;
	const struct exynos8895_hc_member *members;

	unsigned int pll_count;
	const struct exynos8895_hc_pll *plls;

	unsigned int mux_count;
	unsigned int div_count;

	enum exynos8895_hc_transition_owner transition_owner;
	enum exynos8895_hc_owner owner;

	/* Keep CP firmware semantics untouched until modem ownership is proven. */
	bool override_cal;

	/* rate command stays inside policy range; rate_raw may bypass policy. */
	bool runtime_rate_allowed;
	bool runtime_raw_allowed;
};

/* ------------------------------------------------------------------------- */
/* G3D compatibility + logical hardcoded table                               */
/* ------------------------------------------------------------------------- */

#define EXYNOS8895_G3D_ACPM_INDEX        4U
#define EXYNOS8895_G3D_OPP_COUNT         9U
#define EXYNOS8895_HC_G3D_OPP_COUNT      EXYNOS8895_G3D_OPP_COUNT
#define EXYNOS8895_G3D_FVMAP_COUNT       9U
#define EXYNOS8895_G3D_TMU_COUNT         7U
#define EXYNOS8895_G3D_PLL_FIN_KHZ       26000U
#define EXYNOS8895_G3D_PLL_SFR_LO        0x0140U
#define EXYNOS8895_G3D_MIN_UV            450000U
#define EXYNOS8895_G3D_MAX_UV            850000U
#define EXYNOS8895_G3D_START_KHZ         260000U
#define EXYNOS8895_G3D_HIGHSPEED_KHZ     455000U

#define EXYNOS8895_G3D_PACK_PMS(_m, _p, _s) \
	((((unsigned int)(_m) & 0x3ffU) << 16) | \
	 (((unsigned int)(_p) & 0x3fU) << 8) | \
	 ((unsigned int)(_s) & 0x7U))

#define EXYNOS8895_G3D_PMS_MASK \
	EXYNOS8895_G3D_PACK_PMS(0x3ffU, 0x3fU, 0x7U)

struct exynos8895_g3d_hardcoded_opp {
	unsigned int clock_khz;
	unsigned int acpm_key_khz;
	unsigned int voltage_uv;
	unsigned int pll_m;
	unsigned int pll_p;
	unsigned int pll_s;
	unsigned int min_threshold;
	unsigned int max_threshold;
	unsigned int down_staycount;
	unsigned int mem_freq;
	unsigned int cpu_little_min_freq;
	unsigned int cpu_big_max_freq;
	unsigned int int_min_freq;
};

static const struct exynos8895_g3d_hardcoded_opp
exynos8895_g3d_opp_table[EXYNOS8895_G3D_OPP_COUNT] = {
	/* logical key     uV      M   P  S min max stay  MIF      little big INT */
	{ 850000, 839000, 850000, 425, 13, 0, 44, 65, 1, 2093000, 0, 0, 400000 },
	{ 800000, 764000, 825000, 400, 13, 0, 43, 65, 1, 2093000, 0, 0, 400000 },
	{ 700000, 683000, 775000, 350, 13, 0, 39, 65, 1, 2093000, 0, 0, 400000 },
	{ 572000, 572000, 681250, 176,  4, 1, 47, 65, 1, 2093000, 0, 0, 400000 },
	{ 546000, 546000, 662500, 168,  4, 1, 40, 65, 1, 2002000, 0, 0, 400000 },
	{ 455000, 455000, 650000, 140,  4, 1, 40, 65, 1, 2002000, 0, 0, 267000 },
	{ 385000, 385000, 643750, 385, 13, 1, 42, 65, 1, 1794000, 0, 0, 267000 },
	{ 338000, 338000, 637500, 104,  4, 1, 35, 65, 1, 1352000, 0, 0, 178000 },
	{ 260000, 260000, 637500, 160,  4, 2, 35, 65, 1, 1352000, 0, 0, 107000 },
};

/* Samsung ACPM slot keys captured from the target. */
static const unsigned int
exynos8895_g3d_stock_rate[EXYNOS8895_G3D_OPP_COUNT] = {
	839000, 764000, 683000, 572000, 546000,
	455000, 385000, 338000, 260000,
};

/* Exact PLL-derived aliases observed before the source override. */
static const unsigned int
exynos8895_g3d_stock_pll_rate[EXYNOS8895_G3D_OPP_COUNT] = {
	838000, 764000, 682000, 572000, 546000,
	455000, 384000, 338000, 260000,
};

static const unsigned int
exynos8895_g3d_stock_pms[EXYNOS8895_G3D_OPP_COUNT] = {
	EXYNOS8895_G3D_PACK_PMS(129, 4, 0),
	EXYNOS8895_G3D_PACK_PMS(147, 5, 0),
	EXYNOS8895_G3D_PACK_PMS(105, 4, 0),
	EXYNOS8895_G3D_PACK_PMS(176, 4, 1),
	EXYNOS8895_G3D_PACK_PMS(168, 4, 1),
	EXYNOS8895_G3D_PACK_PMS(140, 4, 1),
	EXYNOS8895_G3D_PACK_PMS(148, 5, 1),
	EXYNOS8895_G3D_PACK_PMS(104, 4, 1),
	EXYNOS8895_G3D_PACK_PMS(160, 4, 2),
};

static const unsigned int
exynos8895_g3d_tmu_khz[EXYNOS8895_G3D_TMU_COUNT] = {
	850000, 800000, 700000, 572000, 455000, 385000, 260000,
};

static const unsigned int
exynos8895_g3d_thermal_khz[EXYNOS8895_G3D_TMU_COUNT] = {
	850000, 800000, 700000, 572000, 455000, 385000, 260000,
};


struct exynos8895_hc_lut_desc {
	unsigned int width;
	unsigned int rows;
	const int *params;
};

static const int exynos8895_hc_lut_0[] = { 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 1, 0, 3, 0, 2, 0, 4, 0, 3, 0, 5, 0, 4, 0, 12, 6, 5, 0, 12, 7, 5, 0, 12, 8, 5, 0, 12, 9, 5, 0, 12, 10, 5, 0, 12, 11, 5, 0 };
static const int exynos8895_hc_lut_1[] = { 1, 2, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 2, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 3, 2, 0, 3, 2, 2, 3, 3, 0, 3, 4, 0, 0, 1, 0, 0, 0, 3, 1, 1, 0, 0, 1, 0, 1, 3, 3, 1, 1, 2, 1, 1, 1, 3, 1, 1, 0, 0, 1, 1, 1, 3, 3, 1, 1, 2, 1, 1, 1, 3, 1, 1, 0, 0, 1, 1, 2, 5, 5, 2, 2, 5, 2, 2, 1, 3, 1, 1, 0, 0, 1, 1, 4, 7, 7, 5, 5, 5, 5, 5 };
static const int exynos8895_hc_lut_2[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };
static const int exynos8895_hc_lut_3[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const int exynos8895_hc_lut_4[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
static const int exynos8895_hc_lut_5[] = { 1, 1, 0, 0, 3, 3, 0, 0, 2, 2, 1, 1, 0, 0, 15, 15 };
static const int exynos8895_hc_lut_6[] = { 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 15, 15, 0, 0, 15, 15, 0, 3, 0, 0, 3, 0, 0, 0, 1, 0, 0, 3, 0, 0, 0, 0, 15, 15, 0, 0, 15, 15, 0, 3, 0, 0, 3, 0, 0, 1, 1, 0, 0, 3, 0, 0, 0, 0, 15, 15, 0, 0, 15, 15, 0, 2, 2, 2, 3, 0, 0, 1, 1, 0, 0, 3, 1, 0, 0, 0, 15, 15, 0, 0, 15, 15, 0, 2, 2, 2, 3, 0, 0, 3, 3, 0, 0, 3, 1, 0, 0, 0, 15, 15, 0, 0, 15, 15, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 15, 15, 15, 15, 15, 15, 0, 0, 15, 15, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15 };
static const int exynos8895_hc_lut_7[] = { 1, 0, 0, 1, 0, 2, 0, 3, 0, 5 };
static const int exynos8895_hc_lut_8[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
static const int exynos8895_hc_lut_9[] = { 0, 0, 0 };

static const struct exynos8895_hc_lut_desc
exynos8895_hc_lut_descs[EXYNOS8895_HC_DOMAIN_COUNT] = {
	[0] = { .width = 4U, .rows = 12U, .params = exynos8895_hc_lut_0 },
	[1] = { .width = 16U, .rows = 7U, .params = exynos8895_hc_lut_1 },
	[2] = { .width = 1U, .rows = 18U, .params = exynos8895_hc_lut_2 },
	[3] = { .width = 1U, .rows = 12U, .params = exynos8895_hc_lut_3 },
	[4] = { .width = 1U, .rows = 9U, .params = exynos8895_hc_lut_4 },
	[5] = { .width = 4U, .rows = 4U, .params = exynos8895_hc_lut_5 },
	[6] = { .width = 22U, .rows = 7U, .params = exynos8895_hc_lut_6 },
	[7] = { .width = 2U, .rows = 5U, .params = exynos8895_hc_lut_7 },
	[8] = { .width = 1U, .rows = 9U, .params = exynos8895_hc_lut_8 },
	[9] = { .width = 1U, .rows = 3U, .params = exynos8895_hc_lut_9 },
};

static inline const struct exynos8895_hc_lut_desc *
exynos8895_hc_lut_desc(unsigned int idx)
{
	if (idx >= EXYNOS8895_HC_DOMAIN_COUNT)
		return NULL;
	return &exynos8895_hc_lut_descs[idx];
}


/* ------------------------------------------------------------------------- */
/* Live FVMap snapshot: rate / voltage tables                                */
/* ------------------------------------------------------------------------- */

static const struct exynos8895_hc_simple_opp exynos8895_hc_mif_levels[] = {
	/* Firmware reports 0 uV for the two ASV-disabled top rows. */
	{ 2093000,      0 },
	{ 2002000,      0 },
	{ 1794000, 762500 },
	{ 1540000, 712500 },
	{ 1352000, 675000 },
	{ 1014000, 637500 },
	{  845000, 606250 },
	{  676000, 593750 },
	{  546000, 587500 },
	{  421000, 581250 },
	{  286000, 581250 },
	{  208000, 581250 },
};

#define EXYNOS8895_HC_MIF_OPP_COUNT \
	((unsigned int)ARRAY_SIZE(exynos8895_hc_mif_levels))
#define exynos8895_hc_mif_opps exynos8895_hc_mif_levels

static const struct exynos8895_hc_simple_opp exynos8895_hc_int_levels[] = {
	{ 667000, 781250 }, { 533000, 756250 }, { 400000, 643750 },
	{ 333000, 612500 }, { 267000, 587500 }, { 178000, 575000 },
	{ 107000, 575000 },
};

static const struct exynos8895_hc_simple_opp exynos8895_hc_cpucl0_levels[] = {
	{ 2808000, 1400000 }, { 2704000, 1181250 }, { 2652000, 1150000 },
	{ 2574000, 1106250 }, { 2496000, 1056250 }, { 2314000,  981250 },
	{ 2158000,  925000 }, { 2002000,  881250 }, { 1937000,  862500 },
	{ 1807000,  825000 }, { 1703000,  793750 }, { 1469000,  743750 },
	{ 1261000,  712500 }, { 1170000,  693750 }, { 1066000,  675000 },
	{  962000,  662500 }, {  858000,  650000 }, {  741000,  631250 },
};

static const struct exynos8895_hc_simple_opp exynos8895_hc_cpucl1_levels[] = {
	{ 2002000, 1300000 }, { 1898000, 1200000 }, { 1794000, 1156250 },
	{ 1690000, 1081250 }, { 1456000,  950000 }, { 1248000,  843750 },
	{ 1053000,  787500 }, {  949000,  756250 }, {  832000,  718750 },
	{  715000,  687500 }, {  598000,  650000 }, {  455000,  625000 },
};

static const struct exynos8895_hc_simple_opp exynos8895_hc_intcam_levels[] = {
	{ 690000, 812500 }, { 680000, 687500 },
	{ 670000, 637500 }, { 640000, 575000 },
};

static const struct exynos8895_hc_simple_opp exynos8895_hc_cam_levels[] = {
	{ 690000, 793750 }, { 680000, 793750 }, { 670000, 756250 },
	{ 660000, 693750 }, { 650000, 693750 }, { 640000, 575000 },
	{ 630000, 575000 },
};

static const struct exynos8895_hc_simple_opp exynos8895_hc_disp_levels[] = {
	{ 630000, 750000 }, { 533000, 712500 }, { 356000, 631250 },
	{ 214000, 587500 }, { 134000, 575000 },
};

static const struct exynos8895_hc_simple_opp exynos8895_hc_g3dm_levels[] = {
	{ 839000,      0 }, { 764000,      0 }, { 683000,      0 },
	{ 572000, 681250 }, { 546000, 668750 }, { 455000, 656250 },
	{ 385000, 650000 }, { 338000, 643750 }, { 260000, 643750 },
};

static const struct exynos8895_hc_simple_opp exynos8895_hc_cp_levels[] = {
	{ 1500000, 850000 }, { 1066000, 731250 }, { 800000, 731250 },
};

/* ------------------------------------------------------------------------- */
/* Live FVMap snapshot: members                                              */
/* ------------------------------------------------------------------------- */

static const struct exynos8895_hc_member exynos8895_hc_mif_members[] = {
	{ 0x00f8, 0x300000a }, { 0x0134, 0x3000001 },
	{ 0x1034, 0x4000022 }, { 0x1834, 0x5000035 },
};

static const struct exynos8895_hc_member exynos8895_hc_int_members[] = {
	{ 0x1008, 0x4000007 }, { 0x1058, 0x4000030 },
	{ 0x106c, 0x4000046 }, { 0x107c, 0x400001c },
	{ 0x1080, 0x400002a }, { 0x1088, 0x400003e },
	{ 0x1098, 0x4000008 }, { 0x100c, 0x400001b },
	{ 0x1808, 0x5000017 }, { 0x1854, 0x5000015 },
	{ 0x1868, 0x5000023 }, { 0x1878, 0x500001e },
	{ 0x187c, 0x500003c }, { 0x1888, 0x500004e },
	{ 0x1898, 0x500001d }, { 0x180c, 0x5000031 },
};

static const struct exynos8895_hc_member exynos8895_hc_cpucl0_members[] = {
	{ 0x02c8, 0x3000006 },
};

static const struct exynos8895_hc_member exynos8895_hc_cpucl1_members[] = {
	{ 0x03c0, 0x3000007 },
};

static const struct exynos8895_hc_member exynos8895_hc_g3d_members[] = {
	{ 0x0468, 0x3000009 },
};

static const struct exynos8895_hc_member exynos8895_hc_intcam_members[] = {
	{ 0x1090, 0x4000027 }, { 0x108c, 0x4000028 },
	{ 0x1890, 0x5000039 }, { 0x188c, 0x500003a },
};

static const struct exynos8895_hc_member exynos8895_hc_cam_members[] = {
	{ 0x1014, 0x4000024 }, { 0x1018, 0x4000025 },
	{ 0x101c, 0x4000026 }, { 0x1020, 0x4000039 },
	{ 0x1044, 0x400003d }, { 0x1048, 0x4000043 },
	{ 0x1054, 0x400001f }, { 0x1094, 0x4000035 },
	{ 0x10f8, 0x4000041 }, { 0x10fc, 0x4000042 },
	{ 0x1100, 0x400000b }, { 0x1814, 0x5000036 },
	{ 0x1818, 0x5000037 }, { 0x181c, 0x5000038 },
	{ 0x1820, 0x5000048 }, { 0x1844, 0x500004d },
	{ 0x1848, 0x5000052 }, { 0x1850, 0x5000025 },
	{ 0x1894, 0x5000045 }, { 0x1900, 0x5000051 },
	{ 0x1904, 0x5000050 }, { 0x1908, 0x5000024 },
};

static const struct exynos8895_hc_member exynos8895_hc_disp_members[] = {
	{ 0x104c, 0x4000006 }, { 0x184c, 0x5000016 },
};

static const struct exynos8895_hc_member exynos8895_hc_g3dm_members[] = {
	{ 0x0120, 0x3000009 },
};

static const struct exynos8895_hc_member exynos8895_hc_cp_members[] = {
	{ 0xffff, 0xffff },
};

/* ------------------------------------------------------------------------- */
/* Live FVMap snapshot: PMS                                                  */
/* ------------------------------------------------------------------------- */

static const struct exynos8895_hc_pll_pms exynos8895_hc_mif_pll0_pms[] = {
	{483,3,0},{462,3,0},{414,3,0},{592,5,0},{312,3,0},{468,3,1},
	{390,3,1},{312,3,1},{504,3,2},{518,4,2},{264,3,2},{384,3,3},
};
static const struct exynos8895_hc_pll_pms exynos8895_hc_mif_pll1_pms[] = {
	{287,4,0},{287,4,0},{287,4,0},{287,4,0},{287,4,0},{234,3,0},
	{195,3,0},{156,3,0},{252,3,1},{259,4,1},{264,3,2},{192,3,2},
};
static const struct exynos8895_hc_pll exynos8895_hc_mif_plls[] = {
	{ 0xa6000100, 0x00f8, 13, 12, exynos8895_hc_mif_pll0_pms },
	{ 0xa5a80140, 0x0134, 12, 12, exynos8895_hc_mif_pll1_pms },
};

static const struct exynos8895_hc_pll_pms exynos8895_hc_cpucl0_pms[] = {
	{324,3,0},{312,3,0},{306,3,0},{297,3,0},{288,3,0},{267,3,0},
	{498,3,1},{462,3,1},{447,3,1},{417,3,1},{393,3,1},{339,3,1},
	{291,3,1},{270,3,1},{492,3,2},{444,3,2},{396,3,2},{342,3,2},
};
static const struct exynos8895_hc_pll exynos8895_hc_cpucl0_plls[] = {
	{ 0xa6800120, 0x02c8, 18, 18, exynos8895_hc_cpucl0_pms },
};

static const struct exynos8895_hc_pll_pms exynos8895_hc_cpucl1_pms[] = {
	{231,3,0},{219,3,0},{207,3,0},{195,3,0},{168,3,0},{144,3,0},
	{243,3,1},{219,3,1},{192,3,1},{165,3,1},{276,3,2},{210,3,2},
};
static const struct exynos8895_hc_pll exynos8895_hc_cpucl1_plls[] = {
	{ 0xa6900120, 0x03c0, 12, 12, exynos8895_hc_cpucl1_pms },
};

static const struct exynos8895_hc_pll_pms exynos8895_hc_g3d_pms[] = {
	{425,13,0},{400,13,0},{350,13,0},{176,4,1},{168,4,1},
	{140,4,1},{385,13,1},{104,4,1},{160,4,2},
};
static const struct exynos8895_hc_pll exynos8895_hc_g3d_plls[] = {
	{ 0xa3800140, 0x0468, 9, 9, exynos8895_hc_g3d_pms },
};

/* ------------------------------------------------------------------------- */
/* Domain policy                                                             */
/* ------------------------------------------------------------------------- */

/*
 * policy limits are the current device's live ASV/policy limits.
 * Editing policy_max/policy_min is the explicit source-level unlock point.
 */
static const struct exynos8895_hc_domain_desc
exynos8895_hc_domains[EXYNOS8895_HC_DOMAIN_COUNT] = {
	[EXYNOS8895_HC_MIF] = {
		"dvfs_mif", "dvfs_mif", "vdd_mif", 0,
		286000, 1794000, 208000, 2093000,
		ARRAY_SIZE(exynos8895_hc_mif_levels), exynos8895_hc_mif_levels,
		ARRAY_SIZE(exynos8895_hc_mif_members), exynos8895_hc_mif_members,
		ARRAY_SIZE(exynos8895_hc_mif_plls), exynos8895_hc_mif_plls,
		1, 1, EXYNOS8895_HC_TRANS_ACPM, EXYNOS8895_HC_OWNER_HARDCODED, true, true, true,
	},
	[EXYNOS8895_HC_INT] = {
		"dvfs_int", "dvfs_int", "vdd_int", 1,
		107000, 667000, 107000, 667000,
		ARRAY_SIZE(exynos8895_hc_int_levels), exynos8895_hc_int_levels,
		ARRAY_SIZE(exynos8895_hc_int_members), exynos8895_hc_int_members,
		0, NULL, 8, 8, EXYNOS8895_HC_TRANS_ACPM, EXYNOS8895_HC_OWNER_ACPM, true, true, true,
	},
	[EXYNOS8895_HC_CPUCL0] = {
		"dvfs_cpucl0", NULL, "vdd_cpucl0", 2,
		741000, 2704000, 741000, 2808000,
		ARRAY_SIZE(exynos8895_hc_cpucl0_levels), exynos8895_hc_cpucl0_levels,
		ARRAY_SIZE(exynos8895_hc_cpucl0_members), exynos8895_hc_cpucl0_members,
		ARRAY_SIZE(exynos8895_hc_cpucl0_plls), exynos8895_hc_cpucl0_plls,
		0, 0, EXYNOS8895_HC_TRANS_ACPM, EXYNOS8895_HC_OWNER_ACPM, true, true, true,
	},
	[EXYNOS8895_HC_CPUCL1] = {
		"dvfs_cpucl1", NULL, "vdd_cpucl1", 3,
		455000, 1898000, 455000, 2002000,
		ARRAY_SIZE(exynos8895_hc_cpucl1_levels), exynos8895_hc_cpucl1_levels,
		ARRAY_SIZE(exynos8895_hc_cpucl1_members), exynos8895_hc_cpucl1_members,
		ARRAY_SIZE(exynos8895_hc_cpucl1_plls), exynos8895_hc_cpucl1_plls,
		0, 0, EXYNOS8895_HC_TRANS_ACPM, EXYNOS8895_HC_OWNER_ACPM, true, true, true,
	},
	[EXYNOS8895_HC_G3D] = {
		"dvfs_g3d", NULL, "vdd_g3d", 4,
		260000, 850000, 260000, 850000,
		EXYNOS8895_G3D_OPP_COUNT, NULL,
		ARRAY_SIZE(exynos8895_hc_g3d_members), exynos8895_hc_g3d_members,
		ARRAY_SIZE(exynos8895_hc_g3d_plls), exynos8895_hc_g3d_plls,
		0, 0, EXYNOS8895_HC_TRANS_HYBRID, EXYNOS8895_HC_OWNER_HARDCODED, true, true, true,
	},
	[EXYNOS8895_HC_INTCAM] = {
		"dvfs_intcam", "dvfs_intcam", NULL, 5,
		640000, 690000, 640000, 690000,
		ARRAY_SIZE(exynos8895_hc_intcam_levels), exynos8895_hc_intcam_levels,
		ARRAY_SIZE(exynos8895_hc_intcam_members), exynos8895_hc_intcam_members,
		0, NULL, 2, 2, EXYNOS8895_HC_TRANS_ACPM, EXYNOS8895_HC_OWNER_ACPM, true, true, true,
	},
	[EXYNOS8895_HC_CAM] = {
		"dvfs_cam", "dvfs_cam", "vdd_cam", 6,
		630000, 690000, 630000, 690000,
		ARRAY_SIZE(exynos8895_hc_cam_levels), exynos8895_hc_cam_levels,
		ARRAY_SIZE(exynos8895_hc_cam_members), exynos8895_hc_cam_members,
		0, NULL, 11, 11, EXYNOS8895_HC_TRANS_ACPM, EXYNOS8895_HC_OWNER_ACPM, true, true, true,
	},
	[EXYNOS8895_HC_DISP] = {
		"dvfs_disp", "dvfs_disp", NULL, 7,
		214000, 533000, 134000, 630000,
		ARRAY_SIZE(exynos8895_hc_disp_levels), exynos8895_hc_disp_levels,
		ARRAY_SIZE(exynos8895_hc_disp_members), exynos8895_hc_disp_members,
		0, NULL, 1, 1, EXYNOS8895_HC_TRANS_ACPM, EXYNOS8895_HC_OWNER_ACPM, true, true, true,
	},
	[EXYNOS8895_HC_G3DM] = {
		"dvs_g3dm", NULL, NULL, 8,
		260000, 546000, 260000, 839000,
		ARRAY_SIZE(exynos8895_hc_g3dm_levels), exynos8895_hc_g3dm_levels,
		ARRAY_SIZE(exynos8895_hc_g3dm_members), exynos8895_hc_g3dm_members,
		0, NULL, 0, 0, EXYNOS8895_HC_TRANS_ACPM, EXYNOS8895_HC_OWNER_ACPM, true, true, true,
	},
	[EXYNOS8895_HC_CP] = {
		"dvs_cp", NULL, "vdd_cp", 9,
		800000, 1500000, 800000, 1500000,
		ARRAY_SIZE(exynos8895_hc_cp_levels), exynos8895_hc_cp_levels,
		ARRAY_SIZE(exynos8895_hc_cp_members), exynos8895_hc_cp_members,
		0, NULL, 0, 0, EXYNOS8895_HC_TRANS_FIRMWARE_ONLY, EXYNOS8895_HC_OWNER_ACPM, false, false, false,
	},
};

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static inline const struct exynos8895_hc_domain_desc *
exynos8895_hc_domain(unsigned int idx)
{
	if (idx >= EXYNOS8895_HC_DOMAIN_COUNT)
		return NULL;
	return &exynos8895_hc_domains[idx];
}


static inline bool exynos8895_hc_override_cal(unsigned int idx)
{
	const struct exynos8895_hc_domain_desc *d = exynos8895_hc_domain(idx);

	return d && d->override_cal;
}

static inline const struct exynos8895_g3d_hardcoded_opp *
exynos8895_hc_find_g3d(unsigned long clock_khz)
{
	unsigned int i;

	for (i = 0; i < EXYNOS8895_G3D_OPP_COUNT; i++)
		if (exynos8895_g3d_opp_table[i].clock_khz == clock_khz)
			return &exynos8895_g3d_opp_table[i];

	return NULL;
}

static inline unsigned int
exynos8895_hc_level_rate(unsigned int idx, unsigned int level)
{
	const struct exynos8895_hc_domain_desc *d = exynos8895_hc_domain(idx);

	if (!d || level >= d->level_count)
		return 0;

	if (idx == EXYNOS8895_HC_G3D)
		return exynos8895_g3d_opp_table[level].clock_khz;

	return d->levels[level].clock_khz;
}

static inline unsigned int
exynos8895_hc_level_voltage(unsigned int idx, unsigned int level)
{
	const struct exynos8895_hc_domain_desc *d = exynos8895_hc_domain(idx);

	if (!d || level >= d->level_count)
		return 0;

	if (idx == EXYNOS8895_HC_G3D)
		return exynos8895_g3d_opp_table[level].voltage_uv;

	return d->levels[level].voltage_uv;
}

static inline int exynos8895_hc_fill_rate_table(unsigned int idx,
						unsigned long *table)
{
	const struct exynos8895_hc_domain_desc *d = exynos8895_hc_domain(idx);
	unsigned int i;

	if (!d || !table)
		return 0;

	for (i = 0; i < d->level_count; i++)
		table[i] = exynos8895_hc_level_rate(idx, i);

	return d->level_count;
}

static inline int exynos8895_hc_find_level(unsigned int idx,
					  unsigned long rate)
{
	const struct exynos8895_hc_domain_desc *d = exynos8895_hc_domain(idx);
	unsigned int i;

	if (!d)
		return -1;

	for (i = 0; i < d->level_count; i++)
		if (exynos8895_hc_level_rate(idx, i) == rate)
			return i;

	return -1;
}

static inline bool exynos8895_hc_rate_allowed(unsigned int idx,
					      unsigned long rate,
					      bool raw)
{
	const struct exynos8895_hc_domain_desc *d = exynos8895_hc_domain(idx);
	int level;

	if (!d)
		return false;

	/*
	 * v6 safety lock: raw policy bypass remains disabled until
	 * direct-transition validation passes for the target domain.
	 */
	if (raw)
		return false;
	else {
		if (!d->runtime_rate_allowed)
			return false;
		if (rate < d->policy_min_khz || rate > d->policy_max_khz)
			return false;
	}

	level = exynos8895_hc_find_level(idx, rate);
	if (level < 0)
		return false;

	/* Never send a raw rate whose target row has no known voltage. */
	if (!exynos8895_hc_level_voltage(idx, level))
		return false;

	return true;
}

/*
 * Devfreq gets a contiguous table beginning at the first row whose voltage
 * is known. This deliberately omits MIF 2093/2002 while their live FVMap
 * voltage is zero. Editing those voltage fields to validated non-zero values
 * automatically exposes them on the next build.
 */
static inline int exynos8895_hc_get_devfreq_table(
		const char *name,
		const struct exynos8895_hc_simple_opp **table,
		unsigned int *count)
{
	const struct exynos8895_hc_domain_desc *d = NULL;
	unsigned int i, first = 0;

	if (!name || !table || !count)
		return -1;

	for (i = 0; i < EXYNOS8895_HC_DOMAIN_COUNT; i++) {
		if (!strcmp(name, exynos8895_hc_domains[i].name)) {
			d = &exynos8895_hc_domains[i];
			break;
		}
	}

	if (!d || i == EXYNOS8895_HC_G3D || !d->levels)
		return -1;

	/* Only Linux devfreq domains are accepted here. */
	if (i != EXYNOS8895_HC_MIF && i != EXYNOS8895_HC_INT &&
	    i != EXYNOS8895_HC_INTCAM && i != EXYNOS8895_HC_CAM &&
	    i != EXYNOS8895_HC_DISP)
		return -1;

	while (first < d->level_count && !d->levels[first].voltage_uv)
		first++;

	if (first == d->level_count)
		return -1;

	*table = &d->levels[first];
	*count = d->level_count - first;
	return 0;
}

/* DTS-equivalent defaults for the devfreq front-end. */
#define EXYNOS8895_HC_MIF_INITIAL_KHZ  1794000U
#define EXYNOS8895_HC_MIF_DEFAULT_KHZ   286000U
#define EXYNOS8895_HC_MIF_SUSPEND_KHZ  1014000U
#define EXYNOS8895_HC_MIF_MIN_KHZ       286000U
#define EXYNOS8895_HC_MIF_MAX_KHZ      1794000U
#define EXYNOS8895_HC_MIF_REBOOT_KHZ    421000U
#define EXYNOS8895_HC_MIF_BOOT_KHZ     1794000U

#endif /* __EXYNOS8895_HARDCODED_PROFILE_H__ */
