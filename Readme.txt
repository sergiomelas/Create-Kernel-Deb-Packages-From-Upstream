          ╔══════════════════════════════════════════════════════════════════════════════╗
          ║ ┌──────────────────────────────────────────────────────────────────────────┐ ║
          ║ │                         YOGAFAN: LENOVO FAN HOME                         │ ║
          ║ │             The Official Mainline Home of the 'yogafan' Driver           │ ║
          ║ │         & Automated Upstream Kernel Deployment Toolchain for Debian      │ ║
          ║ ├──────────────────────────────────────────────────────────────────────────┤ ║
          ║ │  Developed by Sergio Melas (sergiomelas@gmail.com)         © 2023-2026   │ ║
          ║ └──────────────────────────────────────────────────────────────────────────┘ ║
          ╚══════════════════════════════════════════════════════════════════════════════╝

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚀 SUPPORTED MODELS: ~400 (Yoga, IdeaPad, ThinkPad, ThinkBook, Flex, V-Series, U-Series)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
## 📢 Community Call to Action: Help Map the Lenovo Portfolio!

### 🚀 yogafan Driver Compatibility: ~400 Models Covered (~85% of Lenovo Consumer Hardware)
 The open-source `yogafan` hardware abstraction layer maps embedded controller architectures across
 11 major hardware families released between 2011 and 2026 (Yoga, IdeaPad, ThinkPad, ThinkBook,
 Flex, V-Series, U-Series).

 *⚠️ **Legion & LOQ Owners:** Modern gaming platforms are natively handled via the official
 `lenovo-wmi-other` WMI driver. To prevent unsafe register conflicts, yogafan automatically yields
 control when a WMI GUID block is detected.*

### 🔍 How to Contribute Your Hardware
 *Every Lenovo line utilizes slightly different Embedded Controller (EC) offsets for thermal
 registers and fan telemetry. Help us safely map your device!

1. **Extract your DSDT binary table:**
   * **Linux:** Run `sudo acpidump -b -t DSDT -o dsdt.dat` (requires `acpica-tools`)
   * **Windows:** Run `iasl.exe -b -t DSDT` via the portable Intel ASL compiler
2. **Collect system hardware strings:**
   * **Linux:** Run `sudo dmidecode -s system-family` and `sudo dmidecode -s system-product-name`
   * **Windows:** Run `Get-CimInstance Win32_ComputerSystemProduct | Select-Object Version, Name`
     in PowerShell
3. **Open a GitHub Issue:**
   * File an issue under `[DSDT Entry] <Your Laptop Model>` and attach your `dsdt.dat` along with
     your extracted Family and Product metadata.

📖 **Confused or need step-by-step help?** Check out our full Linux and portable Windows walkthroughs.
The guide is in the repository under: /Lenovo_Drivers/Prototype/DSDT/

*Heartfelt thanks to our community pioneers for DSDT sharing:
 **Phani Pavan Kambhampati
 **unlockxiaom PenPenIsGod
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


This Script installs dependencies to create debian packages for the latest kernel using debian
Configuration. Add desired kernel options, compile it and make Debian packages then installs it
(if configured so). We also provide a driver for reading Fans RPM for Lenovo laptops that is
in 7.1 upstream.

WARNING & DISCLAIMER: ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃                                                                                                   ┃
┃                     NEVER USE NON OFFICIAL KERNELS, THIS COULD DAMAGE YOUR SYSTEM                 ┃
┃                                Run instead officially distributed kernels                         ┃
┃                                                                                                   ┃
┃ We assume no responsibility for errors or omissions in the software or documentation available.   ┃
┃ In no event shall we be liable to you or any third parties for any special, punitive, incidental, ┃
┃ Indirect or consequential damages of any kind, or any damages whatsoever, including,              ┃
┃ without limitation, those resulting from loss of use, data or profits, and on any theory of       ┃
┃ liability, arising out of or in  connection with the use of this software.                        ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

================================================================================
Debian Package Creation from upstream
================================================================================

Installation Instructions:
 0)- Make the scripts executable and copy to a directory where you will store the created debs
     or in a specific kernel root (see 3)
 1)- Edit the scripts to change some options (autoinstall, kustom modules)
 2)- Open terminal
 3)- Run the scripts following your need:
        - create-Kernel-From-Upstream.sh      : To download and install the latest kernel
        - auto_Compile.sh                     : To compile a kernel (Simplest kernel)
        - auto_Compile_Rust.sh                : To compile a kernel (Rust support)
        - auto_compile_rust_lenovo.sh         : Rust + AMD Optimization (Best for vanilla
                                                upstream-compliant builds)
        - auto_compile_rust_lenovo_drivers.sh : Rust + AMD Optimization + Universal Yogafan
                                                from driver source code (Best for local driver
                                                development). Uses a symlink to pull the latest
                                                source from the Commits folder automatically.
        - auto_compile_rust_lenovo_patch.sh   : Rust + AMD Optimization + Universal Yogafan from
                                                official kernel patch (Best for upstream-compliant
                                                builds). Uses a symlink to pull the latest patch
                                                from the Commits folder automatically.
     The auto_*.sh scripts need to be placed in the root directory of the downloaded kernel source.
 4)- Running those latest deb packages are created:
        linux-headers-xxxxxxxxxxx_amd64.deb
        linux-image-xxxxxxxxxxx_amd64.deb
        linux-libc-xxxxxxxxxxx_amd64.deb
 5)- Rename the directory linux-latest with the created version number (e.g., linux-6.x.x)
 6)- Install the 3 packages:
        sudo apt-get install ./linux-headers-*.deb ./linux-image-*.deb ./linux-libc-*.deb
 7)- Reboot and enjoy the new kernel.

Removal instructions:
 a)- Boot in a different version and list all installed Linux kernel images:

     dpkg --list | egrep -i --color 'linux-image|linux-headers|linux-libc'


 b)- Delete unwanted and unused kernel images and headers:
   b1)- If you are downgrading: Restore the old linux-libc-dev (e.g. from 6.9.0 => 6.8.9).
        sudo apt-get install ./linux-libc-dev_6.8.9-1_amd64.deb
        (Take it from old compiled debs, so conserve them!!)
   b2)- Remove kernels:
        sudo apt-get --purge remove linux-image-xxxxx linux-headers-xxxxx
        Remove Modules folders of the old kernels:
        modulestr=$(dpkg -S /lib/modules/* 2>&1 | grep "no path found matching pattern" | \
        awk '{ print $NF }' | tr "\n" " ")
        sudo rm -r $modulestr
   b3)- Update Grub:
        sudo update-grub
 c)- Remove old kernel modules in /lib/modules of the unused kernels removed.
 d)- To fully control the kernel, remove autoupdate virtual Packages:
        sudo apt-get --purge remove linux-image-amd64 linux-headers-amd64
     To restore autoupdate reinstall:
        sudo apt-get install linux-image-amd64 linux-headers-amd64 linux-libc-dev

================================================================================
  🧰 AUTOMATED DEVELOPER TOOLCHAIN & COMPILATION UTILITIES
================================================================================
This repository houses an automated development toolkit tailored for building,
testing, and managing out-of-tree kernel modules and upstream Debian kernel packages.

📂 Core Development Tools (Root Directory)
├── Check patch.sh               # Validates upstream patch style and compliance via checkpatch.pl
├── Commands.txt                 # Quick-reference developer cheat sheet for manual kernel mapping
├── compilation progress.sh      # Live command-line watcher tracking object queues during heavy builds
├── run_sashiko_local.sh         # Executes localized automation tests inside the Sashiko framework
├── sashiko install.txt          # Explicit installation reference for Sashiko subsystem dependencies
├── Stress cpu.sh                # Fires heavy multi-core CPU workloads to validate fan thermal curves

🚀 Local Module Build Triplet (Toolchain/ Directory)
To accelerate localized driver development, use the automated dynamic triplet scripts.
These scripts read parameters from top-level variables and auto-fill target configurations
using live system telemetry (`uname -r`):

1️⃣ Step 1: Workspace Staging
    $ ./Toolchain/Prepare.sh
    Purges local build caches, stages your driver source code flat into 'src/', and dynamically
    generates an out-of-tree Makefile mapped directly to your running kernel's build path.

2️⃣ Step 2: Live Memory Hot-Swapping
    $ ./Toolchain/Test.sh
    Escalates execution privileges natively via 'sudo -E' while safely preserving repository
    paths, compiles the module, handles warm rmmod extraction, inserts the new .ko test binary,
    and dumps real-time sensors and dmesg telemetry profiles.

3️⃣ Step 3: Production Deployment & Initramfs Sync
    $ ./Toolchain/Deploy.sh
    Permanently installs the module into /lib/modules/$(uname -r)/kernel/drivers/hwmon/.
    It strips legacy binaries, compresses the module using kernel-compliant XZ flags
    (xz --check=crc32) to prevent modprobe EINVAL faults, rewires maps (depmod), and rebuilds
    the system Initramfs boot image block so custom fan metrics survive cold reboots.


================================================================================
LENOVO-SPECIFIC POST-INSTALLATION GUIDE (Updated 2026)
auto_compile_rust_lenovo_*.sh
================================================================================

1. UNIVERSAL FAN & SENSOR SUPPORT (yogafan v13.0)
-----------------------------------------------
This build injects the Sergio Melas "yogafan" driver V8. It uses a passive RLLag
(Rate-Limited Lag) filter for smooth RPM readings.

YOGAFAN V2 (part 2) / GITHUB V1.0 - SUPPORTED MODELS LIST (2026)
Supported chips:



* 1. YOGA SERIES (8-bit Continuous / Discrete Logic)
  - Yoga Pro 7 (83E2)
  - Yoga Slim 7, 7i, 7 Pro, 7 Carbon, 7 ProX
  - Yoga 14cACN (82N7), 14s, 13
  - Yoga 710, 720, 510, 5 Pro
  - Yoga 3 14, Yoga 2 13, Yoga 11s (Discrete Step Logic)

* 2. IDEAPAD SERIES (8-bit Continuous / Discrete Logic)
  - IdeaPad 5, 5i, 5 Pro (81YM, 82FG)
  - IdeaPad 3, 3i (Modern 8-bit variants)
  - IdeaPad 500S, 510S, 710S
  - IdeaPad Y580 (Discrete Step Logic)

* 3. FLEX SERIES (8-bit Continuous)
  - Flex 5, 5i (81X1), Flex 6

* 4. THINKPAD SERIES (8-bit Continuous / Discrete Logic)
  - ThinkPad L-Series (L380, L390, L530)
  - ThinkPad T/X/Edge Series (T430s, T440s, T540p, X220, X230)
  - ThinkPad 13, Helix, x121e

* 5. THINKBOOK SERIES (8-bit Continuous)
  - ThinkBook 14, 16 (Plus, p series)
  - ThinkBook 13s, 14s (83AK)

* 6. V-SERIES (8-bit Continuous)
  - V330-14, V330-15IKB (81AX)
  - V580, V580c

* 7. U-SERIES & LEGACY (Discrete Logic)
  - U330p, U430p (High-resolution discrete)
  - U31-70, U41-70, U160



This driver covers 400 models that is over 85% of Lenovo's consumer and ultra-portable laptop
portfolio released between 2011 and 2026, providing a unified hardware abstraction layer for
diverse Embedded Controller (EC) architectures for 11 families.

The driver exposes the RLLag physical filter parameters (time constant and slew-rate limit) in SI
units (seconds), dynamically synchronizing them with the specific model's maximum RPM to ensure
a consistent physical response across the entire Lenovo product stack.

Hardware Identification (DMI Quirk Table):
- 8-bit EC (Multiplier 100): Yoga, IdeaPad, Slim, Flex.
- 16-bit EC (Multiplier 1): Legion, LOQ.
- Level based Fan control (no RPM Tachometer only low, med..)

Filter Details:
- Slew-Rate Limiting: Capped at 1500 RPM/s to match motor inertia.
- Suspend Safety: Uses boottime clock (ktime_get_boottime) for resume consistency.
- Precision: 12-bit fixed-point math for 1-RPM step resolution.

Note: The driver is automatically loaded at boot via DMI matching. No manual configuration is
required. If you previously created a kernel module load file with:
echo "yogafan" | sudo tee /etc/modules-load.d/yogafan.conf, please remove it with:
sudo rm /etc/modules-load.d/yogafan.conf

Note on Versioning: Folders marked (Accepted) match the official Linux mainline code.
Folders marked (Rejected) or (Unsubmitted) are for development/testing and are not yet part of
the official kernel.

Verification:
$ sensors
# Look for 'yogafan-isa-0000'. If values are missing in KDE, run: killall ksystemstats

2. POWER PROFILE INTEGRATION (AMD P-STATE EPP)
----------------------------------------------
Enables AMD P-State "Active" mode for Energy Performance Preference (EPP).
$ sudo apt update && sudo apt install power-profiles-daemon
$ sudo systemctl enable --now power-profiles-daemon

KDE Plasma 6 Configuration:
1. System Settings -> Power Management -> Energy Saving.
2. Set "Switch to power profile" to Performance (AC) or Power Save (Battery).

Technical Verification:
$ cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver # Should be: amd-pstate-epp
$ cat /sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference # EPP Hint

3. ADVANCED FIXES (SLEEP & ACPI RESOURCES)
------------------------------------------
* S3 Sleep (DSTS Scan): Includes ACPI modifications for robust resume from sleep.
* 0 RPM / Resource Conflict (LAX Mode): If sensors show 0 RPM while fan spins:
  1. Edit /etc/default/grub
  2. Add "acpi_enforce_resources=lax" to GRUB_CMDLINE_LINUX_DEFAULT.
  3. Run: sudo update-grub

References
----------

**References**

1. **ACPI Specification (Field Objects):** Documentation on how 8-bit vs 16-bit fields are accessed
   in OperationRegions.
   https://uefi.org/specs/ACPI/6.5/05_ACPI_Software_Programming_Model.html#field-objects

2. **NBFC Projects:** Community-driven reverse engineering of Lenovo Legion/LOQ EC memory maps
   (16-bit raw registers).
   https://github.com/hirschmann/nbfc/tree/master/Configs

3. **Linux Kernel Timekeeping API:** Documentation for ktime_get_boottime() and handling deltas
   across suspend states.
   https://www.kernel.org/doc/html/latest/core-api/timekeeping.html

4. **Lenovo IdeaPad Laptop Driver:** Reference for DMI-based hardware feature gating in Lenovo
   laptops.
   https://github.com/torvalds/linux/blob/master/drivers/platform/x86/lenovo/ideapad-laptop.c

5. **Yogafan Community Support & DSDT Collection:** Resource for out-of-tree testing scripts and
   collection of user-contributed ACPI DSDT dumps for hardware expansion.
   https://github.com/sergiomelas/lenovo-linux-drivers

6. **IEC 61508:** Functional safety of electrical/electronic/programmable electronic
   safety-related systems.
   https://www.iec.ch/functional-safety

7. **IEC 61511:** Functional safety - Safety instrumented systems for the process industry sector.
   https://www.iec.ch/functional-safety

8. **ISA/IEC 62443:** Security for industrial automation and control systems (formerly ISA-99).
   https://www.isa.org/isa99

9. **Lenovo WMI Other Driver** Reference for WMI-based fan reporting on modern Lenovo platforms;
   used to implement the driver's coexistence logic and WMI GUID detection.
   https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/platform/x86/lenovo/wmi-other.c

10. **Lenovo Product Specifications Reference** Reference for DMI_PRODUCT_NAME and identification;
    used to implement the yogafan_quirks table and the Lenovo Fan HAL Database above.
    https://psref.lenovo.com/

11. **lenovo-linux-drivers** all information extracted above can be found in my github:
    https://github.com/sergiomelas/lenovo-linux-drivers


##################################################################################################################
Change log:

## Changelog - Yogafan V1.2

* **DMI Matching Improvements**: Refined platform identification mechanisms to ensure robust detection across supported Lenovo Yoga and IdeaPad models.
* **EC Path Resolution**: Optimized embedded controller register calls for improved temperature polling and fan profile handling.
* **Stability & Field Testing**: Validated core thermal control loops against real-world user hardware telemetry and DSDT traces.

V1.3: 2026-09-11    - Mainline Documentation & Model Integration:
                    - Officially merged documentation updates into the mainline Linux kernel,
                      adding full support and tracking for the IdeaPad 3 15ALC6 Ub (82KU)
                      and Yoga 740-15IML (81TD) models.
                    - Polished reStructuredText table layouts, column boundaries, and device naming
                      strings to keep builds clean and error-free.
                    - Updated community contributor credits and DSDT data providers for the hardware database.

V1.2: 2026-08-06    - Enhanced Platform Identification & Quirk Validation:
                    - Refined DMI platform identification mechanisms and broadened fallback tolerances
                      to ensure robust detection across supported Lenovo Yoga, IdeaPad, and Legion models.
                    - Optimized embedded controller register calls for improved temperature p
                      polling precision and dynamic fan profile handling.
                     - Validated core thermal control loops and DSDT trace mappings against real-world user hardware
                     telemetry from GitHub deployments.

V1.1: 2026-06-27 - Third Upstream Submission Wave: Released patch V1 (Part 3 / Create_next):
                   - Deprecated Part 2 development branch due to LLM context-drift and alignment
                     anomalies, resetting the pipeline to maintain strict reliability while
                     preserving Part 2's core objective to integrate the comprehensive PSREF
                     hardware database; tracked via:
                     ./Lenovo_Drivers/Prototype/PSREF/yogafan_v3_quirks_database.ods
                   - Initiated Part 3 tracking, rebuilding the patchset step-by-step from the stable
                     V11 Part 1 baseline currently integrated in the mainline kernel.
                   - Achieved clean patch application and synchronization on both hwmon-next and 7.1.1.
                   - Integrated automated diagnostic toolchain documentation and local driver triplet
                     utilities (Prepare, Test, Deploy).
                   - Expanded hardware support by integrating several new laptop models into the
                     DSDT hardware database; tracked via:
                     ./Lenovo_Drivers/Prototype/DSDT/


V1.0: 2026-04-28 - Updating yogafan V3 (part 2): Researching data from PFREF and DSDTs and various
                   other sources.
                 - Hybrid Engine: Added Nmax/Rmax logic for Discrete ECs (Yoga 710/510, IdeaPad 500S/U31).
                 - Expanded HAL: Added "FAN0"/"FA2S" ACPI path mapping (ThinkBook G6, IdeaPad 5,
                   Flex 5, LOQ).
                 - HWMON Refinement: Synced quirk table.
                 - Documentation: Updated Master Reference Database (2026) with newly validated
                   EC offsets.
                 - Code Cleanup: Standardized indentation, 32-bit safety (div64_s64), and removed
                   trailing comments.
                 - Those improvements Expanded support from 3 to 12 distinct hardware families,
                   covering over 400 unique models and 85% of Lenovo's consumer portfolio (2011–2026).
                 - WMI Coexistence: Implemented GUID detection to yield control on modern Gaming/Pro
                   models (Legion/LOQ).

V0.9: 2026-04-01 - Refactored yogafan v12:
                 - Expanded Hardware Support & Quirk Refinement:
                    - Added Support for "Register 0x06" architecture (Yoga 710, 510, IdeaPad
                      510s/500S/U31/Y580).
                    - Added "FAN0" ACPI Path Mapping (ThinkBook G6, IdeaPad 5, Flex 5, V330, V580).
                 - Refined Hardware Scope: Excluded non-RPM reporting configs (ThinkPad/Legacy Yoga)
                   to ensure HWMON compliance.
                 - Documentation: Updated Master Reference Database with new validated offsets.
                 - Code Cleanup: Standardized indentation and removed trailing comments.

V0.8: 2026-04-01 - Refactored yogafan V10,11:
                 - MAJOR MILESTONE: 'yogafan' V11 driver accepted UPSTREAM and is in linux next (7.1).
                    - Mapped ACPI paths directly via DMI quirks.
                    - Fixed Documentation formatting (0-day robot warnings).
                    - Implemented 100ms MIN_SAMPLING to address rapid polling concerns.
                    - Removed redundant platform_set_drvdata() in probe.
                    - Explicitly defined platform device ID as -1 for cleaner sysfs naming.
                    - Already Supported Models: Yoga 14c, Slim 7, Pro 7, Pro 9, Legion 5,
                      Legion 7i, LOQ.


V0.7: 2026-03-25 - KDE 6 Compatibility: Full sensor documentation and formatting for modern Plasma
                   dashboards.
                 - S3 Sleep Integration: DSTS (Device Status) ACPI modifications for robust resume
                   from sleep.
                 - RLLag Filter: 100ms heartbeat engine with pure integer fixed-point math for
                   smoothing. Inertia Simulation: Clamps RPM change-per-second to prevent jitter
                   and "teleporting" values.
                 - Build Fixes: Corrected Debian package naming logic (.deb) in the automated
                   compile script.
                 - Refactored yogafan V7,8,9:
                    - Universal Platform Mode: V8.0 refactor for enhanced stability.
                    - Multi-Fan Logic: Support for FA2S/FAN0 paths (Legion/LOQ).
                    - FOPTD Verification: Clamps RPM change to prevent jitter (Inertia Simulation).
                    - Upstream Submission: Lenovo fan driver submitted upstream.

V0.6: 2026-03-14 - Created kernel driver module for yoga fan

V0.5: 2026-02-08 - Added personalization of kernel name and BSOD, added full optimization for
                   lenovo 14cACN, added retrieving config from latest official debian kernel
                   with fallback, personalization of kernel name and local version,
                   compilation optimization avoiding debug symbols and deb creation.

V0.4: 2025-05-01 - Bugfixes

V0.3: 2025-04-27 - Integrated Rust-for-Linux abstraction layers and LLVM toolchain bindgen.

V0.2: 2024-07-21 - First release: Adding many functionalities

V0.1: 2021-12-28 - Initial version for personal use
