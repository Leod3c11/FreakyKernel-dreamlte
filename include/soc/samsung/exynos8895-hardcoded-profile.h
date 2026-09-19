#ifndef __EXYNOS8895_HARDCODED_PROFILE_H__
#define __EXYNOS8895_HARDCODED_PROFILE_H__

/*
 * Exynos8895 Hardcoded SoC Profile
 *
 * SINGLE SOURCE OF TRUTH FOR CUSTOM DVFS DATA.
 *
 * G3D and MIF are the first physically-enabled domains because their live
 * behavior and tables were already validated on the target Galaxy S8.
 *
 * The remaining SoC domains are registered here from day one.  Their exact
 * device tables can be added to this file without creating another DVFS
 * architecture.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#define EXYNOS8895_HC_PROFILE_VERSION       1U

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

enum exynos8895_hc_owner {
    EXYNOS8895_HC_OWNER_ACPM = 0,
    EXYNOS8895_HC_OWNER_HARDCODED = 1,
};

struct exynos8895_hc_domain_desc {
    const char *name;
    const char *devfreq_name;
    unsigned int acpm_index;
    enum exynos8895_hc_owner owner;
};

static const struct exynos8895_hc_domain_desc
exynos8895_hc_domains[EXYNOS8895_HC_DOMAIN_COUNT] = {
    [EXYNOS8895_HC_MIF]    = { "MIF",    "dvfs_mif",    0, EXYNOS8895_HC_OWNER_HARDCODED },
    [EXYNOS8895_HC_INT]    = { "INT",    "dvfs_int",    1, EXYNOS8895_HC_OWNER_ACPM },
    [EXYNOS8895_HC_CPUCL0] = { "CPUCL0", "dvfs_cpucl0", 2, EXYNOS8895_HC_OWNER_ACPM },
    [EXYNOS8895_HC_CPUCL1] = { "CPUCL1", "dvfs_cpucl1", 3, EXYNOS8895_HC_OWNER_ACPM },
    [EXYNOS8895_HC_G3D]    = { "G3D",    "dvfs_g3d",    4, EXYNOS8895_HC_OWNER_HARDCODED },
    [EXYNOS8895_HC_INTCAM] = { "INTCAM", "dvfs_intcam", 5, EXYNOS8895_HC_OWNER_ACPM },
    [EXYNOS8895_HC_CAM]    = { "CAM",    "dvfs_cam",    6, EXYNOS8895_HC_OWNER_ACPM },
    [EXYNOS8895_HC_DISP]   = { "DISP",   "dvfs_disp",   7, EXYNOS8895_HC_OWNER_ACPM },
    [EXYNOS8895_HC_G3DM]   = { "G3DM",   "dvs_g3dm",    8, EXYNOS8895_HC_OWNER_ACPM },
    [EXYNOS8895_HC_CP]     = { "CP",     "dvs_cp",      9, EXYNOS8895_HC_OWNER_ACPM },
};

/* ------------------------------------------------------------------------- */
/* MIF                                                                       */
/* ------------------------------------------------------------------------- */

struct exynos8895_hc_simple_opp {
    unsigned int clock_khz;
    unsigned int voltage_uv;
};

/*
 * Exynos8895 dvfs_mif source table.
 * Voltage is OPP metadata here.  MIF clock/rail transition remains ACPM-owned
 * in v1; the hardcoded controller owns the frequency list and its limits.
 */
static const struct exynos8895_hc_simple_opp exynos8895_hc_mif_opps[] = {
    { 2093000, 918750 },
    { 2002000, 906250 },
    { 1794000, 893750 },
    { 1540000, 862500 },
    { 1352000, 837500 },
    { 1014000, 800000 },
    {  845000, 781250 },
    {  676000, 756250 },
    {  546000, 700000 },
    {  421000, 725000 },
    {  286000, 712500 },
    {  208000, 700000 },
};

#define EXYNOS8895_HC_MIF_OPP_COUNT \
    ((unsigned int)ARRAY_SIZE(exynos8895_hc_mif_opps))

#define EXYNOS8895_HC_MIF_INITIAL_KHZ  2093000U
#define EXYNOS8895_HC_MIF_DEFAULT_KHZ   208000U
#define EXYNOS8895_HC_MIF_SUSPEND_KHZ  1014000U
#define EXYNOS8895_HC_MIF_MIN_KHZ       208000U
#define EXYNOS8895_HC_MIF_MAX_KHZ      2093000U
#define EXYNOS8895_HC_MIF_REBOOT_KHZ    421000U
#define EXYNOS8895_HC_MIF_BOOT_KHZ     2093000U

/* ------------------------------------------------------------------------- */
/* G3D                                                                       */
/* ------------------------------------------------------------------------- */

#define EXYNOS8895_HC_G3D_OPP_COUNT 9U

/*
 * Keep the legacy struct tag so existing Mali/CAL/FVMap users consume this
 * central table without maintaining a second copy.
 */
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
exynos8895_g3d_opp_table[EXYNOS8895_HC_G3D_OPP_COUNT] = {
    /* clock   key      uV      M    P  S  min max stay    MIF      little big   INT */
    { 850000, 839000, 850000, 425, 13, 0, 44, 65, 1, 2093000,       0,   0, 400000 },
    { 800000, 764000, 825000, 400, 13, 0, 43, 65, 1, 2093000,       0,   0, 400000 },
    { 700000, 683000, 775000, 350, 13, 0, 39, 65, 1, 2093000,       0,   0, 400000 },
    { 572000, 572000, 681250, 176,  4, 1, 47, 65, 1, 2093000,       0,   0, 400000 },
    { 546000, 546000, 662500, 168,  4, 1, 40, 65, 1, 2002000,       0,   0, 400000 },
    { 455000, 455000, 650000, 140,  4, 1, 40, 65, 1, 2002000,       0,   0, 267000 },
    { 385000, 385000, 643750, 385, 13, 1, 42, 65, 1, 1794000,       0,   0, 267000 },
    { 338000, 338000, 637500, 104,  4, 1, 35, 65, 1, 1352000,       0,   0, 178000 },
    { 260000, 260000, 637500, 160,  4, 2, 35, 65, 1, 1352000,       0,   0, 107000 },
};

static inline int exynos8895_hc_get_devfreq_table(
        const char *name,
        const struct exynos8895_hc_simple_opp **table,
        unsigned int *count)
{
    if (!name || !table || !count)
        return -1;

    if (!strcmp(name, "dvfs_mif")) {
        *table = exynos8895_hc_mif_opps;
        *count = EXYNOS8895_HC_MIF_OPP_COUNT;
        return 0;
    }

    /*
     * INT/INTCAM/CAM/DISP intentionally fall through to ECT until their full
     * device-verified tables are added above.  The devfreq hook is already
     * generic: once a table is added here, no new driver architecture is
     * required.
     */
    return -1;
}

static inline const struct exynos8895_g3d_hardcoded_opp *
exynos8895_hc_find_g3d(unsigned long clock_khz)
{
    unsigned int i;

    for (i = 0; i < EXYNOS8895_HC_G3D_OPP_COUNT; i++)
        if (exynos8895_g3d_opp_table[i].clock_khz == clock_khz)
            return &exynos8895_g3d_opp_table[i];

    return NULL;
}

#endif /* __EXYNOS8895_HARDCODED_PROFILE_H__ */
