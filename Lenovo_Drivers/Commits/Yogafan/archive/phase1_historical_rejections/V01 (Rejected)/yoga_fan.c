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
 * - Lenovo Legion 5 / 7 / Pro series (Dual-fan)
 * - Lenovo Yoga Slim 7 / Pro / Carbon / Nano
 * - Lenovo IdeaPad 5 / ThinkBook series (Standard EC naming)
 *
 * Context: v3.0 - Implements Platform Bus registration for Plasma 6.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/hwmon.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>

#define DRVNAME "yogafan"



static const char * const fan_paths[] = {
	"\\_SB.PCI0.LPC0.EC0.FANS",        // Yoga 14c ACN (Your machine), Yoga 7 Gen 6/7 (Ryzen)
	"\\_SB.PCI0.LPC0.EC0.FA2S",        // Legion 5/7, Yoga Pro 7/9i (Dual-Fan models)
	"\\_SB.PCI0.LPC0.EC0.FAN0",        // IdeaPad 5, Yoga Slim 7 (Intel), Standard ThinkBooks
	"\\_SB.PCI0.LPC.EC.FAN0",          // Older Yogas (Pre-2020), Legacy EC naming
	"\\_SB.PCI0.LPC0.EC.FAN0",         // Yoga Slim 7 Pro, Carbon/Nano-style motherboards
};

static const char *active_path = NULL;
static struct platform_device *yoga_pdev;
static struct device *hwmon_dev;

static int yoga_fan_read_rpm(void)
{
	unsigned long long val;
	if (!active_path || ACPI_FAILURE(acpi_evaluate_integer(NULL, (char *)active_path, NULL, &val)))
		return -EIO;

	/* Convert EC 0-255 scale or raw RPM */
	return (val > 0 && val <= 255) ? (int)(val * 100) : (int)val;
}

static umode_t yoga_fan_is_visible(const void *data, enum hwmon_sensor_types type, u32 attr, int channel)
{
	if (type == hwmon_fan && (attr == hwmon_fan_input || attr == hwmon_fan_label))
		return 0444;
	return 0;
}

static int yoga_fan_read(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, long *val)
{
	if (type == hwmon_fan && attr == hwmon_fan_input) {
		int rpm = yoga_fan_read_rpm();
		if (rpm < 0) return rpm;
		*val = rpm;
		return 0;
	}
	return -EOPNOTSUPP;
}

static int yoga_fan_read_string(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, const char **str)
{
	if (type == hwmon_fan && attr == hwmon_fan_label) {
		*str = "System Fan";
		return 0;
	}
	return -EOPNOTSUPP;
}

static const struct hwmon_channel_info *yoga_fan_info[] = {
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT | HWMON_F_LABEL),
	NULL
};

static const struct hwmon_ops yoga_fan_hwmon_ops = {
	.is_visible = yoga_fan_is_visible,
	.read = yoga_fan_read,
	.read_string = yoga_fan_read_string,
};

static const struct hwmon_chip_info yoga_fan_chip_info = {
	.ops = &yoga_fan_hwmon_ops,
	.info = yoga_fan_info,
};

static int __init yoga_fan_init(void)
{
	unsigned long long val;
	int i, ret;

	/* 1. ACPI Discovery */
	for (i = 0; i < ARRAY_SIZE(fan_paths); i++) {
		if (ACPI_SUCCESS(acpi_evaluate_integer(NULL, (char *)fan_paths[i], NULL, &val))) {
			active_path = fan_paths[i];
			break;
		}
	}

	if (!active_path)
		return -ENODEV;

	/* 2. Platform Device Allocation (Forces /sys/devices/platform/ for KDE 6) */
	yoga_pdev = platform_device_alloc(DRVNAME, 0);
	if (!yoga_pdev)
		return -ENOMEM;

	ret = platform_device_add(yoga_pdev);
	if (ret) {
		platform_device_put(yoga_pdev);
		return ret;
	}

	/* 3. HWMON Registration */
	hwmon_dev = hwmon_device_register_with_info(&yoga_pdev->dev, DRVNAME, NULL, &yoga_fan_chip_info, NULL);

	if (IS_ERR(hwmon_dev)) {
		platform_device_unregister(yoga_pdev);
		return PTR_ERR(hwmon_dev);
	}

	pr_info("yogafan: Sergio Melas driver (Platform Mode) loaded. Path: %s\n", active_path);
	return 0;
}

static void __exit yoga_fan_exit(void)
{
	if (hwmon_dev)
		hwmon_device_unregister(hwmon_dev);
	if (yoga_pdev)
		platform_device_unregister(yoga_pdev);
	pr_info("yogafan: Sergio Melas driver unloaded.\n");
}

module_init(yoga_fan_init);
module_exit(yoga_fan_exit);

MODULE_AUTHOR("Sergio Melas <sergiomelas@gmail.com>");
MODULE_DESCRIPTION("Lenovo Yoga Platform-based Fan Driver");
MODULE_LICENSE("GPL v2");
