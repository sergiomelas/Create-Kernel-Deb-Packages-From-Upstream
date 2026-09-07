// SPDX-License-Identifier: GPL-2.0-only
/**
 * yoga_fan.c - Lenovo Yoga/Legion Fan Hardware Monitoring Driver
 *
 * Copyright (C) 2026 Sergio Melas <sergiomelas@gmail.com>
 *
 * This driver provides fan speed monitoring for modern Lenovo Yoga, Legion,
 * and IdeaPad laptops by interfacing with the Embedded Controller (EC)
 * via ACPI. It registers a platform device to ensure compatibility with
 * modern HWMON consumers like KDE Plasma 6.
 *
 * Supported Models:
 * - Lenovo Yoga 7 / 14c series (Ryzen/Intel)
 * - Lenovo Legion 5 / 7 / Pro series (Dual-fan support)
 * - Lenovo Yoga Slim 7 / Pro / Carbon / Nano
 * - Lenovo IdeaPad 5 / ThinkBook series
 *
 * Context: v4.3 - Fixed static HWMON channel definition for kernel 7.0 compatibility.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/hwmon.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/dmi.h>

#define DRVNAME "yogafan"
#define MAX_FANS 2

static const char * const fan_paths[] = {
	"\\_SB.PCI0.LPC0.EC0.FANS",  // Primary Fan (Yoga 14c)
	"\\_SB.PCI0.LPC0.EC0.FA2S",  // Secondary Fan (Legion)
	"\\_SB.PCI0.LPC0.EC0.FAN0",  // IdeaPad / Slim
	"\\_SB.PCI0.LPC.EC.FAN0",    // Legacy
	"\\_SB.PCI0.LPC0.EC.FAN0",   // Alternate
};

struct yoga_fan_data {
	const char *active_paths[MAX_FANS];
	int fan_count;
};

/* --- HWMON Logic --- */

static int yoga_fan_read(struct device *dev, enum hwmon_sensor_types type,
			 u32 attr, int channel, long *val)
{
	struct yoga_fan_data *data = dev_get_drvdata(dev);
	unsigned long long raw_val;
	acpi_status status;

	if (type != hwmon_fan || attr != hwmon_fan_input)
		return -EOPNOTSUPP;

	if (channel >= data->fan_count)
		return -EINVAL;

	status = acpi_evaluate_integer(NULL, (char *)data->active_paths[channel], NULL, &raw_val);
	if (ACPI_FAILURE(status))
		return -EIO;

	/* Scaling: 0-255 scale (x100) or Raw RPM */
	*val = (raw_val > 0 && raw_val <= 255) ? (raw_val * 100) : raw_val;
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

/* Static configuration for up to 2 fans (Standard for Yoga/Legion) */
static const struct hwmon_channel_info *yoga_fan_info[] = {
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT, HWMON_F_INPUT),
	NULL
};

static const struct hwmon_chip_info yoga_fan_chip_info = {
	.ops = &yoga_fan_hwmon_ops,
	.info = yoga_fan_info,
};

/* --- Platform Driver Core --- */

static int yoga_fan_probe(struct platform_device *pdev)
{
	struct yoga_fan_data *data;
	struct device *hwmon_dev;
	acpi_handle handle;
	int i;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* ACPI Discovery via Handle Existence */
	for (i = 0; i < ARRAY_SIZE(fan_paths); i++) {
		if (ACPI_SUCCESS(acpi_get_handle(NULL, (char *)fan_paths[i], &handle))) {
			data->active_paths[data->fan_count++] = fan_paths[i];
			pr_info("yogafan: Registered fan hardware at %s\n", fan_paths[i]);

			if (data->fan_count >= MAX_FANS)
				break;
		}
	}

	if (data->fan_count == 0)
		return -ENODEV;

	/* Register with the static chip_info */
	hwmon_dev = devm_hwmon_device_register_with_info(&pdev->dev, DRVNAME,
							 data, &yoga_fan_chip_info, NULL);

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static struct platform_driver yoga_fan_driver = {
	.driver = { .name = DRVNAME },
	.probe = yoga_fan_probe,
};

static struct platform_device *yoga_fan_device;

static const struct dmi_system_id yoga_dmi_table[] __initconst = {
	{ .ident = "Lenovo", .matches = { DMI_MATCH(DMI_SYS_VENDOR, "LENOVO") } },
	{ }
};
MODULE_DEVICE_TABLE(dmi, yoga_dmi_table);

static int __init yoga_fan_init(void)
{
	if (!dmi_check_system(yoga_dmi_table)) return -ENODEV;
	if (platform_driver_register(&yoga_fan_driver)) return -ENOSYS;

	yoga_fan_device = platform_device_register_simple(DRVNAME, 0, NULL, 0);
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
MODULE_DESCRIPTION("Universal Lenovo Fan Driver v4.3");
MODULE_LICENSE("GPL v2");
