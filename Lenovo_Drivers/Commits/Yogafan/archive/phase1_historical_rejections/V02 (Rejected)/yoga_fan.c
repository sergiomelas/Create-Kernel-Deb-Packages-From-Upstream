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
 * Context: v4.0 - Full Platform Bus and Multi-Fan support.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/hwmon.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/dmi.h>

#define DRVNAME "yogafan"
#define MAX_FANS 4

static const char * const fan_paths[] = {
	"\\_SB.PCI0.LPC0.EC0.FANS",
	"\\_SB.PCI0.LPC0.EC0.FA2S",
	"\\_SB.PCI0.LPC0.EC0.FAN0",
	"\\_SB.PCI0.LPC.EC.FAN0",
	"\\_SB.PCI0.LPC0.EC.FAN0",
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

	status = acpi_evaluate_integer(NULL, (char *)data->active_paths[channel], NULL, &raw_val);
	if (ACPI_FAILURE(status))
		return -EIO;

	/* * Scaling: If EC returns 0-255, it's a percentage scale (x100 RPM).
	 * If > 255, it's likely already a raw RPM value.
	 */
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

/* --- Platform Driver Core --- */

static int yoga_fan_probe(struct platform_device *pdev)
{
	struct yoga_fan_data *data;
	struct device *hwmon_dev;
	struct hwmon_channel_info **info;
	struct hwmon_chip_info *chip;
	unsigned long long dummy;
	int i;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* Find all available fans */
	for (i = 0; i < ARRAY_SIZE(fan_paths); i++) {
		if (ACPI_SUCCESS(acpi_evaluate_integer(NULL, (char *)fan_paths[i], NULL, &dummy))) {
			data->active_paths[data->fan_count++] = fan_paths[i];
			if (data->fan_count >= MAX_FANS)
				break;
		}
	}

	if (data->fan_count == 0)
		return -ENODEV;

	/* Dynamically create HWMON info based on fan count */
	info = devm_kcalloc(&pdev->dev, 2, sizeof(*info), GFP_KERNEL);
	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!info || !chip)
		return -ENOMEM;

	// This creates 'n' fan channels
	info[0] = devm_hwmon_gen_config(&pdev->dev, hwmon_fan, hwmon_fan_input, data->fan_count);
	info[1] = NULL;

	chip->ops = &yoga_fan_hwmon_ops;
	chip->info = (const struct hwmon_channel_info **)info;

	hwmon_dev = devm_hwmon_device_register_with_info(&pdev->dev, DRVNAME, data, chip, NULL);
	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static struct platform_driver yoga_fan_driver = {
	.driver = {
		.name = DRVNAME,
	},
	.probe = yoga_fan_probe,
};

static struct platform_device *yoga_fan_device;

/* --- Module Init / Exit --- */

static const struct dmi_system_id yoga_dmi_table[] __initconst = {
	{
		.ident = "Lenovo Yoga/Legion/IdeaPad",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		},
	},
	{ }
};
MODULE_DEVICE_TABLE(dmi, yoga_dmi_table);

static int __init yoga_fan_init(void)
{
	int ret;

	if (!dmi_check_system(yoga_dmi_table))
		return -ENODEV;

	ret = platform_driver_register(&yoga_fan_driver);
	if (ret)
		return ret;

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
MODULE_DESCRIPTION("Universal Lenovo Yoga/Legion Fan Driver");
MODULE_LICENSE("GPL v2");
