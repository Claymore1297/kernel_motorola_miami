// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2009-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2017-2019, Linaro Ltd.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/soc/qcom/smem.h>
#include <linux/string.h>
#include <linux/sys_soc.h>
#include <linux/types.h>
#include <soc/qcom/socinfo.h>

/*
 * SoC version type with major number in the upper 16 bits and minor
 * number in the lower 16 bits.
 */
#define SOCINFO_MAJOR(ver) (((ver) >> 16) & 0xffff)
#define SOCINFO_MINOR(ver) ((ver) & 0xffff)
#define SOCINFO_VERSION(maj, min)  ((((maj) & 0xffff) << 16)|((min) & 0xffff))

#define SMEM_SOCINFO_BUILD_ID_LENGTH           32
#define SMEM_SOCINFO_CHIP_ID_LENGTH			   32

/*
 * SMEM item id, used to acquire handles to respective
 * SMEM region.
 */
#define SMEM_HW_SW_BUILD_ID            137
#define SMEM_IMAGE_VERSION_TABLE	469

static uint32_t socinfo_format;

enum {
	HW_PLATFORM_UNKNOWN = 0,
	HW_PLATFORM_SURF    = 1,
	HW_PLATFORM_FFA     = 2,
	HW_PLATFORM_FLUID   = 3,
	HW_PLATFORM_SVLTE_FFA	= 4,
	HW_PLATFORM_SVLTE_SURF	= 5,
	HW_PLATFORM_MTP_MDM = 7,
	HW_PLATFORM_MTP  = 8,
	HW_PLATFORM_LIQUID  = 9,
	/* Dragonboard platform id is assigned as 10 in CDT */
	HW_PLATFORM_DRAGON	= 10,
	HW_PLATFORM_QRD	= 11,
	HW_PLATFORM_HRD	= 13,
	HW_PLATFORM_DTV	= 14,
	HW_PLATFORM_RCM	= 21,
	HW_PLATFORM_STP = 23,
	HW_PLATFORM_SBC = 24,
	HW_PLATFORM_ADP = 25,
	HW_PLATFORM_HDK = 31,
	HW_PLATFORM_ATP = 33,
	HW_PLATFORM_IDP = 34,
	HW_PLATFORM_INVALID
};

static const char * const hw_platform[] = {
	[HW_PLATFORM_UNKNOWN] = "Unknown",
	[HW_PLATFORM_SURF] = "Surf",
	[HW_PLATFORM_FFA] = "FFA",
	[HW_PLATFORM_FLUID] = "Fluid",
	[HW_PLATFORM_SVLTE_FFA] = "SVLTE_FFA",
	[HW_PLATFORM_SVLTE_SURF] = "SLVTE_SURF",
	[HW_PLATFORM_MTP_MDM] = "MDM_MTP_NO_DISPLAY",
	[HW_PLATFORM_MTP] = "MTP",
	[HW_PLATFORM_RCM] = "RCM",
	[HW_PLATFORM_LIQUID] = "Liquid",
	[HW_PLATFORM_DRAGON] = "Dragon",
	[HW_PLATFORM_QRD] = "QRD",
	[HW_PLATFORM_HRD] = "HRD",
	[HW_PLATFORM_DTV] = "DTV",
	[HW_PLATFORM_STP] = "STP",
	[HW_PLATFORM_SBC] = "SBC",
	[HW_PLATFORM_ADP] = "ADP",
	[HW_PLATFORM_HDK] = "HDK",
	[HW_PLATFORM_ATP] = "ATP",
	[HW_PLATFORM_IDP] = "IDP",
};

enum {
	PLATFORM_SUBTYPE_QRD = 0x0,
	PLATFORM_SUBTYPE_SKUAA = 0x1,
	PLATFORM_SUBTYPE_SKUF = 0x2,
	PLATFORM_SUBTYPE_SKUAB = 0x3,
	PLATFORM_SUBTYPE_SKUG = 0x5,
	PLATFORM_SUBTYPE_QRD_INVALID,
};

static const char * const qrd_hw_platform_subtype[] = {
	[PLATFORM_SUBTYPE_QRD] = "QRD",
	[PLATFORM_SUBTYPE_SKUAA] = "SKUAA",
	[PLATFORM_SUBTYPE_SKUF] = "SKUF",
	[PLATFORM_SUBTYPE_SKUAB] = "SKUAB",
	[PLATFORM_SUBTYPE_SKUG] = "SKUG",
	[PLATFORM_SUBTYPE_QRD_INVALID] = "INVALID",
};

static const char * const adp_hw_platform_subtype[] = {
	[0] = "ADP",
};

enum {
	PLATFORM_SUBTYPE_UNKNOWN = 0x0,
	PLATFORM_SUBTYPE_CHARM = 0x1,
	PLATFORM_SUBTYPE_STRANGE = 0x2,
	PLATFORM_SUBTYPE_STRANGE_2A = 0x3,
	PLATFORM_SUBTYPE_INVALID,
};

static const char * const hw_platform_subtype[] = {
	[PLATFORM_SUBTYPE_UNKNOWN] = "Unknown",
	[PLATFORM_SUBTYPE_CHARM] = "charm",
	[PLATFORM_SUBTYPE_STRANGE] = "strange",
	[PLATFORM_SUBTYPE_STRANGE_2A] = "strange_2a",
	[PLATFORM_SUBTYPE_INVALID] = "Invalid",
};

enum {
	/* External SKU */
	SKU_UNKNOWN = 0x0,
	SKU_AA = 0x1,
	SKU_AB = 0x2,
	SKU_AC = 0x3,
	SKU_AD = 0x4,
	SKU_AE = 0x5,
	SKU_AF = 0x6,
	SKU_EXT_RESERVE,

	/* Internal SKU */
	SKU_Y0 = 0xf1,
	SKU_Y1 = 0xf2,
	SKU_Y2 = 0xf3,
	SKU_Y3 = 0xf4,
	SKU_Y4 = 0xf5,
	SKU_Y5 = 0xf6,
	SKU_Y6 = 0xf7,
	SKU_Y7 = 0xf8,
	SKU_INT_RESERVE,
};

static const char * const hw_platform_esku[] = {
	[SKU_UNKNOWN] = "Unknown",
	[SKU_AA] = "AA",
	[SKU_AB] = "AB",
	[SKU_AC] = "AC",
	[SKU_AD] = "AD",
	[SKU_AE] = "AE",
	[SKU_AF] = "AF",
};

#define SKU_INT_MASK 0x0f
static const char * const hw_platform_isku[] = {
	[SKU_Y0 & SKU_INT_MASK] = "Y0",
	[SKU_Y1 & SKU_INT_MASK] = "Y1",
	[SKU_Y2 & SKU_INT_MASK] = "Y2",
	[SKU_Y3 & SKU_INT_MASK] = "Y3",
	[SKU_Y4 & SKU_INT_MASK] = "Y4",
	[SKU_Y5 & SKU_INT_MASK] = "Y5",
	[SKU_Y6 & SKU_INT_MASK] = "Y6",
	[SKU_Y7 & SKU_INT_MASK] = "Y7",
};

/* Socinfo SMEM item structure */
static struct socinfo {
	__le32 fmt;
	__le32 id;
	__le32 ver;
	char build_id[SMEM_SOCINFO_BUILD_ID_LENGTH];
	/* Version 2 */
	__le32 raw_id;
	__le32 raw_ver;
	/* Version 3 */
	__le32 hw_plat;
	/* Version 4 */
	__le32 plat_ver;
	/* Version 5 */
	__le32 accessory_chip;
	/* Version 6 */
	__le32 hw_plat_subtype;
	/* Version 7 */
	__le32 pmic_model;
	__le32 pmic_die_rev;
	/* Version 8 */
	__le32 pmic_model_1;
	__le32 pmic_die_rev_1;
	__le32 pmic_model_2;
	__le32 pmic_die_rev_2;
	/* Version 9 */
	__le32 foundry_id;
	/* Version 10 */
	__le32 serial_num;
	/* Version 11 */
	__le32 num_pmics;
	__le32 pmic_array_offset;
	/* Version 12 */
	__le32 chip_family;
	__le32 raw_device_family;
	__le32 raw_device_num;
	/* Version 13 */
	__le32 nproduct_id;
	char chip_name[SMEM_SOCINFO_CHIP_ID_LENGTH];
	/* Version 14 */
	__le32 num_clusters;
	__le32 ncluster_array_offset;
	__le32 num_defective_parts;
	__le32 ndefective_parts_array_offset;
	/* Version 15 */
	__le32  nmodem_supported;
	/* Version 16 */
	__le32  esku;
	__le32  nproduct_code;
	__le32  npartnamemap_offset;
	__le32  nnum_partname_mapping;
} *socinfo;

#define BUILD_ID_LENGTH 32
#define CHIP_ID_LENGTH 32
#define SMEM_IMAGE_VERSION_BLOCKS_COUNT 32
#define SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE 128
#define SMEM_IMAGE_VERSION_SIZE 4096
#define SMEM_IMAGE_VERSION_NAME_SIZE 75
#define SMEM_IMAGE_VERSION_VARIANT_SIZE 20
#define SMEM_IMAGE_VERSION_VARIANT_OFFSET 75
#define SMEM_IMAGE_VERSION_OEM_SIZE 33
#define SMEM_IMAGE_VERSION_OEM_OFFSET 95
#define SMEM_IMAGE_VERSION_PARTITION_APPS 10

int softsku_idx;
module_param_named(softsku_idx, softsku_idx, int, 0644);

/* Version 10 */
uint32_t socinfo_get_serial_number(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 10) ?
			le32_to_cpu(socinfo->serial_num) : 0)
		: 0;
}
EXPORT_SYMBOL(socinfo_get_serial_number);

struct qcom_socinfo {
	struct soc_device *soc_dev;
	struct soc_device_attribute attr;
	uint32_t current_image;
	struct rw_semaphore current_image_rwsem;
};

struct soc_id {
	unsigned int id;
	const char *name;
};

static const struct soc_id soc_id[] = {
	{ 87, "MSM8960" },
	{ 109, "APQ8064" },
	{ 122, "MSM8660A" },
	{ 123, "MSM8260A" },
	{ 124, "APQ8060A" },
	{ 126, "MSM8974" },
	{ 130, "MPQ8064" },
	{ 138, "MSM8960AB" },
	{ 139, "APQ8060AB" },
	{ 140, "MSM8260AB" },
	{ 141, "MSM8660AB" },
	{ 178, "APQ8084" },
	{ 184, "APQ8074" },
	{ 185, "MSM8274" },
	{ 186, "MSM8674" },
	{ 194, "MSM8974PRO" },
	{ 206, "MSM8916" },
	{ 208, "APQ8074-AA" },
	{ 209, "APQ8074-AB" },
	{ 210, "APQ8074PRO" },
	{ 211, "MSM8274-AA" },
	{ 212, "MSM8274-AB" },
	{ 213, "MSM8274PRO" },
	{ 214, "MSM8674-AA" },
	{ 215, "MSM8674-AB" },
	{ 216, "MSM8674PRO" },
	{ 217, "MSM8974-AA" },
	{ 218, "MSM8974-AB" },
	{ 246, "MSM8996" },
	{ 247, "APQ8016" },
	{ 248, "MSM8216" },
	{ 249, "MSM8116" },
	{ 250, "MSM8616" },
	{ 291, "APQ8096" },
	{ 305, "MSM8996SG" },
	{ 310, "MSM8996AU" },
	{ 311, "APQ8096AU" },
	{ 312, "APQ8096SG" },
	{ 356, "KONA" },
	{ 362, "SA8155" },
	{ 367, "SA8155P" },
	{ 377, "SA6155P" },
	{ 384, "SA6155"},
	{ 405, "SA8195P" },
	{ 415, "LAHAINA" },
	{ 439, "LAHAINAP" },
	{ 449, "SC_DIREWOLF"},
	{ 456, "LAHAINA-ATP" },
	{ 460, "SA_DIREWOLF_IVI"},
	{ 461, "SA_DIREWOLF_ADAS"},
	{ 501, "SM8325" },
	{ 502, "SM8325P" },
	{ 450, "SHIMA" },
	{ 454, "HOLI" },
	{ 507, "BLAIR" },
	{ 578, "BLAIR-LITE" },
	{ 565, "BLAIRP" },
	{ 628, "BLAIRP-XR" },
	{ 647, "BLAIRP-LTE" },
	{ 486, "MONACO" },
	{ 458, "SDXLEMUR" },
	{ 483, "SDXLEMUR-SD"},
	{ 509, "SDXLEMUR-LITE"},
	{ 475, "YUPIK" },
	{ 484, "SDXNIGHTJAR" },
	{ 441, "SCUBA" },
	{ 497, "YUPIK-IOT" },
	{ 498, "YUPIKP-IOT" },
	{ 499, "YUPIKP" },
	{ 515, "YUPIK-LTE" },
	{ 575, "KATMAI" },
	{ 576, "KATMAIP" },
};

static struct attribute *msm_custom_socinfo_attrs[35];

static umode_t soc_info_attribute(struct kobject *kobj,
						   struct attribute *attr,
						   int index)
{
	return attr->mode;
}

static const struct attribute_group custom_soc_attr_group = {
	.attrs = msm_custom_socinfo_attrs,
	.is_visible = soc_info_attribute,
};

static const char *socinfo_machine(unsigned int id)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(soc_id); idx++) {
		if (soc_id[idx].id == id)
			return soc_id[idx].name;
	}

	return NULL;
}

uint32_t socinfo_get_id(void)
{
	return (socinfo) ? le32_to_cpu(socinfo->id) : 0;
}
EXPORT_SYMBOL(socinfo_get_id);

const char *socinfo_get_id_string(void)
{
	uint32_t id = socinfo_get_id();

	return socinfo_machine(id);
}
EXPORT_SYMBOL(socinfo_get_id_string);

static int qcom_socinfo_probe(struct platform_device *pdev)
{
	struct qcom_socinfo *qs;
	struct socinfo *info;
	size_t item_size;

	info = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_HW_SW_BUILD_ID,
			      &item_size);
	if (IS_ERR(info)) {
		dev_err(&pdev->dev, "Couldn't find socinfo\n");
		return PTR_ERR(info);
	}

	socinfo_format = le32_to_cpu(info->fmt);
	socinfo = info;

	qs = devm_kzalloc(&pdev->dev, sizeof(*qs), GFP_KERNEL);
	if (!qs)
		return -ENOMEM;

	qs->attr.machine = socinfo_machine(le32_to_cpu(info->id));
	qs->attr.family = "Snapdragon";
	qs->attr.soc_id = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%u",
					 le32_to_cpu(info->id));
	qs->attr.revision = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%u.%u",
					   SOCINFO_MAJOR(le32_to_cpu(info->ver)),
					   SOCINFO_MINOR(le32_to_cpu(info->ver)));
	if (!qs->attr.soc_id || !qs->attr.revision)
		return -ENOMEM;

	if (offsetof(struct socinfo, serial_num) <= item_size) {
		qs->attr.serial_number = devm_kasprintf(&pdev->dev, GFP_KERNEL,
							"%u",
							le32_to_cpu(info->serial_num));
		if (!qs->attr.serial_number)
			return -ENOMEM;
	}

	qs->soc_dev = soc_device_register(&qs->attr);
	if (IS_ERR(qs->soc_dev))
		return PTR_ERR(qs->soc_dev);

	/* Feed the soc specific unique data into entropy pool */
	add_device_randomness(info, item_size);

	platform_set_drvdata(pdev, qs);

	return 0;
}

static int qcom_socinfo_remove(struct platform_device *pdev)
{
	struct qcom_socinfo *qs = platform_get_drvdata(pdev);

	soc_device_unregister(qs->soc_dev);

	return 0;
}

static struct platform_driver qcom_socinfo_driver = {
	.probe = qcom_socinfo_probe,
	.remove = qcom_socinfo_remove,
	.driver  = {
		.name = "qcom-socinfo",
	},
};

module_platform_driver(qcom_socinfo_driver);

MODULE_DESCRIPTION("Qualcomm SoCinfo driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:qcom-socinfo");
