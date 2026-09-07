// SPDX-License-Identifier: GPL-2.0-only
/**
 * yoga_fan.c - Lenovo Yoga/Legion Fan Hardware Monitoring Driver
 *
 * Provides fan speed monitoring for Lenovo Yoga, Legion, and IdeaPad
 * laptops by interfacing with the Embedded Controller (EC) via ACPI.
 *
 * The driver implements a passive discrete-time first-order lag filter
 * with slew-rate limiting (RLLag). This addresses low-resolution
 * tachometer sampling in the EC by smoothing RPM readings based on
 * the time delta (dt) between userspace requests, ensuring physical
 * consistency without background task overhead or race conditions.
 * The filter implements multirate filtering with autoreset in case
 * of large sampling time.
 *
 * Copyright (C) 2021-2026 Sergio Melas <sergiomelas@gmail.com>
 */
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include <linux/hwmon-sysfs.h>

/* Driver Configuration Constants */
#define DRVNAME			"yogafan"
#define MAX_FANS		8

/* Filter Configuration Constants */
#define TAU_MS			1000	/* Time constant for the first-order lag (ms) */
#define MAX_SLEW_RPM_S		1500	/* Maximum allowed change in RPM per second */
#define MAX_SAMPLING		5000	/* Maximum allowed Ts for reset (ms) */
#define MIN_SAMPLING		100	/* Minimum interval between filter updates (ms) */

/* RPM Sanitation Constants */
#define RPM_FLOOR_LIMIT		50	/* Snap filtered value to 0 if raw is 0 */
#define MIN_THRESHOLD_RPM	10	/* Minimum safety floor for per-model stop thresholds */

struct yogafan_config {
	int multiplier;			/* Used if n_max == 0 */
	int fan_count;			/* 1 or 2 */
	int n_max;			/* Discrete steps (0 = Continuous) */
	int r_max;			/* Max physical RPM for estimation */
	unsigned int tau_ms;		/* To store the smoothing speed    */
	unsigned int slew_time_s;	/* To store the acceleration limit */
	unsigned int stop_threshold;	/* To store the RPM floor */
	const char *paths[2];		/* Paths */
};

struct yoga_fan_data {
	acpi_handle active_handles[MAX_FANS];
	long filtered_val[MAX_FANS];
	long raw_val[MAX_FANS];
	ktime_t last_sample[MAX_FANS];
	const struct yogafan_config *config;
	int fan_count;
	/* Per-device physics constants */
	unsigned int internal_tau_ms;
	unsigned int internal_max_slew_rpm_s;
	unsigned int device_max_rpm;
};

/* Specific configurations mapped via DMI */
//* --- CONTINUOUS PROFILES (Nmax = 0) --- */

/* Standard 8-bit Yoga/IdeaPad (Covers 82N7, Slim 7, etc.) */
static struct yogafan_config yoga_continuous_8bit_cfg = {
	.multiplier = 100, .fan_count = 1, .n_max = 0,
	.r_max = 5500,	/* Verified 14cACN peak */
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FANS", "\\_SB.PCI0.LPC0.EC0.FAN0" }
};

/* Legion / LOQ Gaming (2 Fans, Raw RPM 16-bit) */
static struct yogafan_config legion_continuous_16bit_cfg = {
	.multiplier = 1, .fan_count = 2, .n_max = 0,
	.r_max = 6500,	/* Standard Legion/LOQ peak */
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FANS", "\\_SB.PCI0.LPC0.EC0.FA2S" }
};

/* --- DISCRETE ESTIMATION PROFILES (NMAX > 0) --- */

/* Yoga 710/720 (N=59) */
static struct yogafan_config yoga_710_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 59, .r_max = 4500,
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

/* Yoga 510 / Ideapad 510s (N=41) */
static struct yogafan_config yoga_510_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 41, .r_max = 4500,
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

/* Ideapad 500S / U31-70 (N=44) */
static struct yogafan_config ideapad_500s_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 44, .r_max = 5500,
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

/* Yoga 3 14 / Yoga 11s (N=80) */
static struct yogafan_config yoga3_14_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 80, .r_max = 5000,
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", "\\_SB.PCI0.LPC0.EC0.FANS" }
};

/* Yoga 2 13 (N=8) */
static struct yogafan_config yoga2_13_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 8, .r_max = 4200,
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

/* Yoga 13 (N=255) - Dual Fan */
static struct yogafan_config yoga13_discrete_cfg = {
	.multiplier = 0, .fan_count = 2, .n_max = 255, .r_max = 5000,
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN1", "\\_SB.PCI0.LPC0.EC0.FAN2" }
};

/* Legacy U330p/U430p (N=768) */
static struct yogafan_config legacy_u_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 768, .r_max = 5000,
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

/* ThinkPad 13 / Helix / T-Series (Strict Discrete) */
static struct yogafan_config thinkpad_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 7,
	.r_max = 5500, /* Matching table peak for T540p/TP13 */
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", "\\_SB.PCI0.LPC0.EC0.FANS" }
};

/* ThinkPad L-Series / V580 (Continuous 8-bit) */
static struct yogafan_config thinkpad_l_cfg = {
	.multiplier = 100, .fan_count = 1, .n_max = 100,
	.r_max = 5500, /* Matching table peak for L390 */
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", "\\_SB.PCI0.LPC0.EC0.FAN1" }
};

/* High Performance (Strict Continuous) */
static struct yogafan_config legion_high_perf_cfg = {
	.multiplier = 1, .fan_count = 2, .n_max = 0,
	.r_max = 8000, /* Peak for Legion 7i / Yoga Pro 9 */
	.tau_ms = 1000, .slew_time_s = 4, .stop_threshold = 50,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FANS", "\\_SB.PCI0.LPC0.EC0.FA2S" }
};

static void apply_rllag_filter(struct yoga_fan_data *data, int idx, long raw_rpm)
{
	ktime_t now = ktime_get_boottime();
	s64 dt_ms = ktime_to_ms(ktime_sub(now, data->last_sample[idx]));
	long delta, step, limit, alpha;
	s64 temp_num;

	/* 1. PHYSICAL CLAMP & TELEMETRY: Use per-device device_max_rpm */
	if (raw_rpm > (long)data->device_max_rpm)
		raw_rpm = (long)data->device_max_rpm;

	data->raw_val[idx] = raw_rpm;

	/* 2. Threshold logic */
	if (raw_rpm < (long)(data->config->stop_threshold < MIN_THRESHOLD_RPM
		? MIN_THRESHOLD_RPM : data->config->stop_threshold)) {
		data->filtered_val[idx] = 0;
		data->last_sample[idx] = now;
		return;
	}

	/* 3. Auto-reset logic */
	if (data->last_sample[idx] == 0 || dt_ms > MAX_SAMPLING) {
		data->filtered_val[idx] = raw_rpm;
		data->last_sample[idx] = now;
		return;
	}

	if (dt_ms < MIN_SAMPLING)
		return;

	delta = raw_rpm - data->filtered_val[idx];
	if (delta == 0) {
		data->last_sample[idx] = now;
		return;
	}

	/* 4.  PHYSICS: Use per-device internal_tau_ms */
	temp_num = dt_ms << 12;
	alpha = (long)div64_s64(temp_num, (s64)(data->config->tau_ms + dt_ms));
	step = (delta * alpha) >> 12;

	if (step == 0 && delta != 0)
		step = (delta > 0) ? 1 : -1;

	/* 5.  SLEW: Use per-device internal_max_slew_rpm_s */
	limit = ((long)data->internal_max_slew_rpm_s * (long)dt_ms) / 1000;
	if (limit < 1)
		limit = 1;

	if (step > limit)
		step = limit;
	else if (step < -limit)
		step = -limit;

	data->filtered_val[idx] += step;
	data->last_sample[idx] = now;
}

static int yoga_fan_read(struct device *dev, enum hwmon_sensor_types type,
			 u32 attr, int channel, long *val)
{
	struct yoga_fan_data *data = dev_get_drvdata(dev);
	const struct yogafan_config *cfg = data->config;
	unsigned long long raw_acpi;
	long rpm_raw;
	acpi_status status;

	if (type != hwmon_fan)
		return -EOPNOTSUPP;

	/* 1. Handle static MAX attribute immediately without filtering */
	if (attr == hwmon_fan_max) {
		*val = (long)data->device_max_rpm;
		return 0;
	}

	if (attr != hwmon_fan_input)
		return -EOPNOTSUPP;

	/* 2. Get hardware data only for INPUT requests */
	status = acpi_evaluate_integer(data->active_handles[channel], NULL, NULL, &raw_acpi);
	if (ACPI_FAILURE(status))
		return -EIO;

	/* 3. Calculate raw RPM based on architecture */
	if (cfg->n_max > 0)
		rpm_raw = (long)div64_s64((s64)cfg->r_max * raw_acpi, cfg->n_max);
	else
		rpm_raw = (long)raw_acpi * cfg->multiplier;

	/* 4. Apply filter only for real speed readings */
	apply_rllag_filter(data, channel, rpm_raw);

	*val = data->filtered_val[channel];
	return 0;
}

static umode_t yoga_fan_is_visible(const void *data, enum hwmon_sensor_types type,
				   u32 attr, int channel)
{
	const struct yoga_fan_data *fan_data = data;

	if (type == hwmon_fan && channel < fan_data->fan_count)
		return 0444;

	return 0;
}

static const struct hwmon_ops yoga_fan_hwmon_ops = {
	.is_visible = yoga_fan_is_visible,
	.read = yoga_fan_read,
};

static const struct dmi_system_id yogafan_quirks[] = {
	/* --- DISCRETE OVERRIDES (Specific matches MUST come first) --- */
	{
		.ident = "Lenovo Yoga 710",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Yoga 710") },
		.driver_data = &yoga_710_discrete_cfg,
	},
	{
		.ident = "Lenovo Yoga 510",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Yoga 510") },
		.driver_data = &yoga_510_discrete_cfg,
	},
	{
		.ident = "Lenovo Ideapad 510s",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Ideapad 510s") },
		.driver_data = &yoga_510_discrete_cfg,
	},
	{
		.ident = "Lenovo Ideapad 500S",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Ideapad 500S") },
		.driver_data = &ideapad_500s_discrete_cfg,
	},
	{
		.ident = "Lenovo U31-70",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "U31-70") },
		.driver_data = &ideapad_500s_discrete_cfg,
	},
	{
		.ident = "Lenovo Yoga 3 14",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "80JH") },
		.driver_data = &yoga3_14_discrete_cfg,
	},
	{
		.ident = "Lenovo Yoga 2 13",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "20344") },
		.driver_data = &yoga2_13_discrete_cfg,
	},
	{
		.ident = "Lenovo Yoga 13",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "20191") },
		.driver_data = &yoga13_discrete_cfg,
	},
	{
		.ident = "Lenovo U330p/U430p",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Lenovo u330p") },
		.driver_data = &legacy_u_discrete_cfg,
	},
	{
		.ident = "ThinkPad 13",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "ThinkPad 13") },
		.driver_data = &thinkpad_discrete_cfg,
	},
	{
		.ident = "ThinkPad Helix",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "3698") },
		.driver_data = &thinkpad_discrete_cfg,
	},
	{
		.ident = "ThinkPad X-Series",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "ThinkPad X") },
		.driver_data = &thinkpad_discrete_cfg,
	},
	{
		.ident = "ThinkPad T-Series",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "ThinkPad T") },
		.driver_data = &thinkpad_discrete_cfg,
	},
	{
		.ident = "Lenovo V330",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "81AX") },
		.driver_data = &thinkpad_l_cfg,
	},

	/* --- SPECIAL PROFILES (Must precede general fallbacks) --- */
	{
		.ident = "Lenovo Yoga Pro",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Yoga Pro") },
		.driver_data = &legion_high_perf_cfg,
	},
	{
		.ident = "Lenovo Legion Pro",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Legion P") },
		.driver_data = &legion_high_perf_cfg,
	},
	{
		.ident = "Lenovo ThinkPad L",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "ThinkPad L") },
		.driver_data = &thinkpad_l_cfg,
	},

	/* --- CONTINUOUS FALLBACKS (Family matches last) --- */
	{
		.ident = "Lenovo Legion",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "Legion") },
		.driver_data = &legion_continuous_16bit_cfg,
	},
	{
		.ident = "Lenovo LOQ",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "LOQ") },
		.driver_data = &legion_continuous_16bit_cfg,
	},
	{
		.ident = "Lenovo Yoga",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "Yoga") },
		.driver_data = &yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo IdeaPad",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "IdeaPad") },
		.driver_data = &yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo Xiaoxin",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "Xiaoxin") },
		.driver_data = &yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo GeekPro",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "GeekPro") },
		.driver_data = &legion_continuous_16bit_cfg,
	},
	{
		.ident = "Lenovo ThinkBook",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "ThinkBook") },
		.driver_data = &yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo Slim",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "Slim") },
		.driver_data = &yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo V-Series",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Lenovo V") },
		.driver_data = &yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo Aura Edition",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Aura") },
		.driver_data = &yoga_continuous_8bit_cfg,
	},
	{ }
};

MODULE_DEVICE_TABLE(dmi, yogafan_quirks);

static int yoga_fan_probe(struct platform_device *pdev)
{
	const struct dmi_system_id *dmi_id;
	const struct yogafan_config *cfg;
	struct yoga_fan_data *data;
	struct hwmon_chip_info *chip_info;
	struct hwmon_channel_info *info;
	u32 *fan_config;
	acpi_status status;
	int i;

	dmi_id = dmi_first_match(yogafan_quirks);
	if (!dmi_id)
		return -ENODEV;

	cfg = dmi_id->driver_data;
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->config = cfg;
	data->device_max_rpm = cfg->r_max ?: 5000;
	data->internal_tau_ms = cfg->tau_ms;
	data->internal_max_slew_rpm_s = data->device_max_rpm / (cfg->slew_time_s ?: 1);

	/* 1. Discover handles and set the REAL fan_count */
	for (i = 0; i < 2 && cfg->paths[i]; i++) {
		acpi_handle handle;

		status = acpi_get_handle(NULL, cfg->paths[i], &handle);
		if (ACPI_SUCCESS(status)) {
			data->active_handles[data->fan_count] = handle;
			data->fan_count++;
		}
	}

	if (data->fan_count == 0)
		return -ENODEV;

	/* 2. Dynamically build the HWMON channel info  */
	fan_config = devm_kcalloc(&pdev->dev, data->fan_count + 1, sizeof(u32), GFP_KERNEL);
	if (!fan_config)
		return -ENOMEM;

	for (i = 0; i < data->fan_count; i++)
		fan_config[i] = HWMON_F_INPUT | HWMON_F_MAX;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->type = hwmon_fan;
	info->config = fan_config;

/* 3. Wrap it in chip_info */
	chip_info = devm_kzalloc(&pdev->dev, sizeof(*chip_info), GFP_KERNEL);
	if (!chip_info)
		return -ENOMEM;

	chip_info->ops = &yoga_fan_hwmon_ops;

	/* Create AND ALLOCATE the temporary pointer array */
	const struct hwmon_channel_info **chip_info_array;

	chip_info_array = devm_kcalloc(&pdev->dev, 2, sizeof(*chip_info_array), GFP_KERNEL);
	if (!chip_info_array)
		return -ENOMEM;

	chip_info_array[0] = info;
	chip_info_array[1] = NULL; /* Null terminated */

	chip_info->info = chip_info_array;

	/* 4. Register with the accurate hardware description and return the result */
	return PTR_ERR_OR_ZERO(devm_hwmon_device_register_with_info(&pdev->dev,
				DRVNAME, data, chip_info, NULL));
}

static struct platform_driver yoga_fan_driver = {
	.driver = { .name = DRVNAME },
	.probe = yoga_fan_probe,
};

static struct platform_device *yoga_fan_device;

static int __init yoga_fan_init(void)
{
	int ret;

	if (!dmi_check_system(yogafan_quirks))
		return -ENODEV;

	ret = platform_driver_register(&yoga_fan_driver);
	if (ret)
		return ret;

	yoga_fan_device = platform_device_register_simple(DRVNAME, -1, NULL, 0);
	if (IS_ERR(yoga_fan_device)) {
		platform_driver_unregister(&yoga_fan_driver);
		return PTR_ERR(yoga_fan_device);
	}
	return 0;
}

static void __exit yoga_fan_exit(void)
{
	platform_device_unregister(yoga_fan_device);
	platform_driver_unregister(&yoga_fan_driver);
}

module_init(yoga_fan_init);
module_exit(yoga_fan_exit);

MODULE_AUTHOR("Sergio Melas <sergiomelas@gmail.com>");
MODULE_DESCRIPTION("Lenovo Yoga/Legion Fan Monitor Driver");
MODULE_LICENSE("GPL");
