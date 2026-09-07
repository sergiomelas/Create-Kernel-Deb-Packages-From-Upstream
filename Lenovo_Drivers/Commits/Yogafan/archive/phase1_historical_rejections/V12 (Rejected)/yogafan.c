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
#define MIN_TAU_MS		10      /* Minimum value accepted for the time constant (ms) */
#define MIN_THRESHOLD_RPM	10      /* Minimum value accepted for the RPM  threshold (rpm)  */

/* RPM Sanitation Constants */
#define RPM_FLOOR_LIMIT		50	/* Snap filtered value to 0 if raw is 0 */

static uint tau_s = 1;
module_param(tau_s, uint, 0644);
MODULE_PARM_DESC(tau_s, "Filter time constant in SECONDS");

static uint slew_time_s = 4;
module_param(slew_time_s, uint, 0644);
MODULE_PARM_DESC(slew_time_s, "Seconds to reach Max RPM from zero");

static uint stop_threshold_rpm = 50;
module_param(stop_threshold_rpm, uint, 0644);

struct yogafan_config {
	int multiplier;     /* Used if n_max == 0 */
	int fan_count;      /* 1 or 2 */
	int n_max;          /* Discrete steps (0 = Continuous) */
	int r_max;          /* Max physical RPM for estimation */
	const char *paths[2];
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

/* --- CONTINUOUS PROFILES (Nmax = 0) --- */

static const struct yogafan_config yoga_continuous_8bit_cfg = {
	.multiplier = 100, .fan_count = 1, .n_max = 0,
	.r_max = 5500,	/* Verified 14cACN peak */
	.paths = { "\\_SB.PCI0.LPC0.EC0.FANS", "\\_SB.PCI0.LPC0.EC0.FAN0" }
};

static const struct yogafan_config legion_continuous_16bit_cfg = {
	.multiplier = 1, .fan_count = 2, .n_max = 0,
	.r_max = 6500,	/* Standard Legion/LOQ peak */
	.paths = { "\\_SB.PCI0.LPC0.EC0.FANS", "\\_SB.PCI0.LPC0.EC0.FA2S" }
};

/* --- DISCRETE ESTIMATION PROFILES (NMAX > 0) --- */

static const struct yogafan_config yoga_710_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 59, .r_max = 4500,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

static const struct yogafan_config yoga_510_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 41, .r_max = 4500,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

static const struct yogafan_config ideapad_500s_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 44, .r_max = 5500,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

static const struct yogafan_config yoga3_14_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 80, .r_max = 5000,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", "\\_SB.PCI0.LPC0.EC0.FANS" }
};

static const struct yogafan_config yoga2_13_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 8, .r_max = 4200,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

static const struct yogafan_config yoga13_discrete_cfg = {
	.multiplier = 0, .fan_count = 2, .n_max = 255, .r_max = 5000,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN1", "\\_SB.PCI0.LPC0.EC0.FAN2" }
};

static const struct yogafan_config legacy_u_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 768, .r_max = 5000,
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", NULL }
};

static const struct yogafan_config thinkpad_discrete_cfg = {
	.multiplier = 0, .fan_count = 1, .n_max = 7,
	.r_max = 5500, /* Matching table peak for T540p/TP13 */
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", "\\_SB.PCI0.LPC0.EC0.FANS" }
};

static const struct yogafan_config thinkpad_l_cfg = {
	.multiplier = 100, .fan_count = 1, .n_max = 100,
	.r_max = 5500, /* Matching table peak for L390 */
	.paths = { "\\_SB.PCI0.LPC0.EC0.FAN0", "\\_SB.PCI0.LPC0.EC0.FAN1" }
};

static const struct yogafan_config legion_high_perf_cfg = {
	.multiplier = 1, .fan_count = 2, .n_max = 0,
	.r_max = 8000, /* Peak for Legion 7i / Yoga Pro 9 */
	.paths = { "\\_SB.PCI0.LPC0.EC0.FANS", "\\_SB.PCI0.LPC0.EC0.FA2S" }
};

/* --- Custom Sysfs Interface for Companion App --- */
static ssize_t show_custom_rpm(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct yoga_fan_data *data = dev_get_drvdata(dev);
	int nr = sensor_attr->index;

	/* Using index to differentiate between raw (0,1) and max (2,3) */
	if (strstr(attr->attr.name, "_max"))
		return sysfs_emit(buf, "%u\n", data->device_max_rpm);

	return sysfs_emit(buf, "%ld\n", data->raw_val[nr]);
}

static SENSOR_DEVICE_ATTR(fan1_raw, 0444, show_custom_rpm, NULL, 0);
static SENSOR_DEVICE_ATTR(fan1_max, 0444, show_custom_rpm, NULL, 0);
static SENSOR_DEVICE_ATTR(fan2_raw, 0444, show_custom_rpm, NULL, 1);
static SENSOR_DEVICE_ATTR(fan2_max, 0444, show_custom_rpm, NULL, 1);

static struct attribute *yogafan_attrs[] = {
	&sensor_dev_attr_fan1_raw.dev_attr.attr,
	&sensor_dev_attr_fan1_max.dev_attr.attr,
	&sensor_dev_attr_fan2_raw.dev_attr.attr,
	&sensor_dev_attr_fan2_max.dev_attr.attr,
	NULL
};

static const struct attribute_group yogafan_group = { .attrs = yogafan_attrs };
static const struct attribute_group *yogafan_groups[] = { &yogafan_group, NULL };

static void apply_rllag_filter(struct yoga_fan_data *data, int idx, long raw_rpm)
{
	ktime_t now = ktime_get_boottime();
	s64 dt_ms = ktime_to_ms(ktime_sub(now, data->last_sample[idx]));
	long delta, step, limit, alpha;
	s64 temp_num;

	if (raw_rpm > (long)data->device_max_rpm)
		raw_rpm = (long)data->device_max_rpm;

	data->raw_val[idx] = raw_rpm;

	if (raw_rpm < (long)(stop_threshold_rpm < MIN_THRESHOLD_RPM
		? MIN_THRESHOLD_RPM : stop_threshold_rpm)) {
		data->filtered_val[idx] = 0;
		data->last_sample[idx] = now;
		return;
	}

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

	temp_num = dt_ms << 12;
	alpha = (long)div64_s64(temp_num, (s64)(data->internal_tau_ms + dt_ms));
	step = (delta * alpha) >> 12;

	if (step == 0 && delta != 0)
		step = (delta > 0) ? 1 : -1;

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

	if (type != hwmon_fan || attr != hwmon_fan_input)
		return -EOPNOTSUPP;

	status = acpi_evaluate_integer(data->active_handles[channel], NULL, NULL, &raw_acpi);
	if (ACPI_FAILURE(status))
		return -EIO;

	if (cfg->n_max > 0) {
		rpm_raw = (long)div64_s64((s64)cfg->r_max * raw_acpi, cfg->n_max);
	} else {
		rpm_raw = (long)raw_acpi * cfg->multiplier;
	}

	apply_rllag_filter(data, channel, rpm_raw);

	if (attr == hwmon_fan_input) {
		*val = data->filtered_val[channel];
		return 0;
	}
	if (attr == hwmon_fan_max) {
		*val = (long)data->device_max_rpm;
		return 0;
	}

	return -EOPNOTSUPP;
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

static const struct hwmon_channel_info *yoga_fan_info[] = {
	HWMON_CHANNEL_INFO(fan,
			   HWMON_F_INPUT | HWMON_F_MAX, HWMON_F_INPUT | HWMON_F_MAX,
			   HWMON_F_INPUT | HWMON_F_MAX, HWMON_F_INPUT | HWMON_F_MAX,
			   HWMON_F_INPUT | HWMON_F_MAX, HWMON_F_INPUT | HWMON_F_MAX,
			   HWMON_F_INPUT | HWMON_F_MAX, HWMON_F_INPUT | HWMON_F_MAX),
	NULL
};

static const struct hwmon_chip_info yoga_fan_chip_info = {
	.ops = &yoga_fan_hwmon_ops,
	.info = yoga_fan_info,
};

static const struct dmi_system_id yogafan_quirks[] = {
	{
		.ident = "Lenovo Yoga 710",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Yoga 710") },
		.driver_data = (void *)&yoga_710_discrete_cfg,
	},
	{
		.ident = "Lenovo Yoga 510",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Yoga 510") },
		.driver_data = (void *)&yoga_510_discrete_cfg,
	},
	{
		.ident = "Lenovo Ideapad 510s",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Ideapad 510s") },
		.driver_data = (void *)&yoga_510_discrete_cfg,
	},
	{
		.ident = "Lenovo Ideapad 500S",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Ideapad 500S") },
		.driver_data = (void *)&ideapad_500s_discrete_cfg,
	},
	{
		.ident = "Lenovo U31-70",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "U31-70") },
		.driver_data = (void *)&ideapad_500s_discrete_cfg,
	},
	{
		.ident = "Lenovo Yoga 3 14",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "80JH") },
		.driver_data = (void *)&yoga3_14_discrete_cfg,
	},
	{
		.ident = "Lenovo Yoga 2 13",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "20344") },
		.driver_data = (void *)&yoga2_13_discrete_cfg,
	},
	{
		.ident = "Lenovo Yoga 13",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "20191") },
		.driver_data = (void *)&yoga13_discrete_cfg,
	},
	{
		.ident = "Lenovo U330p/U430p",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Lenovo u330p") },
		.driver_data = (void *)&legacy_u_discrete_cfg,
	},
	{
		.ident = "ThinkPad 13",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "ThinkPad 13") },
		.driver_data = (void *)&thinkpad_discrete_cfg,
	},
	{
		.ident = "ThinkPad Helix",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "3698") },
		.driver_data = (void *)&thinkpad_discrete_cfg,
	},
	{
		.ident = "ThinkPad X-Series",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "ThinkPad X") },
		.driver_data = (void *)&thinkpad_discrete_cfg,
	},
	{
		.ident = "ThinkPad T-Series",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "ThinkPad T") },
		.driver_data = (void *)&thinkpad_discrete_cfg,
	},
	{
		.ident = "Lenovo V330",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "81AX") },
		.driver_data = (void *)&thinkpad_l_cfg,
	},
	{
		.ident = "Lenovo Yoga Pro",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Yoga Pro") },
		.driver_data = (void *)&legion_high_perf_cfg,
	},
	{
		.ident = "Lenovo Legion Pro",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Legion P") },
		.driver_data = (void *)&legion_high_perf_cfg,
	},
	{
		.ident = "Lenovo ThinkPad L",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "ThinkPad L") },
		.driver_data = (void *)&thinkpad_l_cfg,
	},
	{
		.ident = "Lenovo Legion",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "Legion") },
		.driver_data = (void *)&legion_continuous_16bit_cfg,
	},
	{
		.ident = "Lenovo LOQ",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "LOQ") },
		.driver_data = (void *)&legion_continuous_16bit_cfg,
	},
	{
		.ident = "Lenovo Yoga",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "Yoga") },
		.driver_data = (void *)&yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo IdeaPad",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "IdeaPad") },
		.driver_data = (void *)&yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo Xiaoxin",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "Xiaoxin") },
		.driver_data = (void *)&yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo GeekPro",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "GeekPro") },
		.driver_data = (void *)&legion_continuous_16bit_cfg,
	},
	{
		.ident = "Lenovo ThinkBook",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "ThinkBook") },
		.driver_data = (void *)&yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo Slim",
		.matches = { DMI_MATCH(DMI_PRODUCT_FAMILY, "Slim") },
		.driver_data = (void *)&yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo V-Series",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Lenovo V") },
		.driver_data = (void *)&yoga_continuous_8bit_cfg,
	},
	{
		.ident = "Lenovo Aura Edition",
		.matches = { DMI_MATCH(DMI_PRODUCT_NAME, "Aura") },
		.driver_data = (void *)&yoga_continuous_8bit_cfg,
	},
	{ }
};
MODULE_DEVICE_TABLE(dmi, yogafan_quirks);

static int yoga_fan_probe(struct platform_device *pdev)
{
	const struct dmi_system_id *dmi_id;
	const struct yogafan_config *cfg;
	struct yoga_fan_data *data;
	struct device *hwmon_dev;
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
	data->device_max_rpm = cfg->r_max ? cfg->r_max : 5000;
	data->internal_tau_ms = tau_s * 1000;
	if (data->device_max_rpm == 0)
		data->device_max_rpm = 5000;
	data->internal_max_slew_rpm_s = data->device_max_rpm / (slew_time_s ? slew_time_s : 1);

	for (i = 0; i < 2; i++) {
		if (!cfg->paths[i])
			continue;

		status = acpi_get_handle(NULL, (char *)cfg->paths[i],
					 &data->active_handles[data->fan_count]);

		if (ACPI_SUCCESS(status)) {
			data->fan_count++;
			if (data->fan_count >= cfg->fan_count)
				break;
		}
	}

	if (data->fan_count == 0)
		return -ENODEV;

	hwmon_dev = devm_hwmon_device_register_with_info(&pdev->dev, DRVNAME,
							 data, &yoga_fan_chip_info,
							 yogafan_groups);

	return PTR_ERR_OR_ZERO(hwmon_dev);
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
