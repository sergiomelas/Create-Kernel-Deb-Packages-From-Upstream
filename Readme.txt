
                    ##################################################################
                    ##################################################################
                    ###               Kernel Download & Compile  Script            ###
                    ###           Create-Kernel-From-Upstream.sh Readme            ###
                    ### Developed by sergio melas (sergiomelas@gmail.com) 2023-24  ###
                    ##################################################################
                    ##################################################################

This Script install dependencies to create debian packages for the latest kernel using debian Configuration.
Add desired kernel options
Compile it and make Debian pakages then installs it (if configured so)


WARNING & DISCLAIMER: ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃                                                                                                  ┃
┃                   NEVER USE NON OFFICIAL KERNELS,  THIS COULD DAMAGE YOUR SYSTEM                 ┃
┃                              Run instead officially distributed kernels                          ┃
┃                                                                                                  ┃
┃ We assume no responsibility for errors or omissions in the software or documentation available.  ┃
┃ In no event shall we be liable to you or any third parties for any special, punitive, incidental,┃
┃ Indirect or consequential damages of any kind, or any damages whatsoever, including,             ┃
┃ without limitation, those resulting from loss of use, data or profits, and on any theory of      ┃
┃ liability, arising out of or in  connection with the use of this software.                       ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

Installation Instructions:
 0)- Make the scrit executable and copy to a directory where you will store the created debs 
 1)- Edit the script to change some options (autoinstall, kustom modules)
 2)- Open terminal
 3)- Run the install.sh script (Just drag and drop file the rest is automatic), give root acess whes the sudo prompt appear
 4)- a directory is created and after kernel compilation will contain the latest and greates deb packages of the linux Kernel: 
       linux-headers-xxxxxxxxxxx_amd64.deb
       linux-image-xxxxxxxxxxx_amd64.deb
       linux-libc-xxxxxxxxxxx_amd64.deb
 5)- rename the directory linux-latest with the creted version number linux-xxxxxxxxxxx (for future use see b1)
 6)- Install the 3 pakages 
        sudo apt-get install linux-headers-xxxxxxxxxxx_amd64.deb linux-image-xxxxxxxxxxx_amd64.deb  linux-libc-xxxxxxxxxxx_amd64.deb
     or the packages are auto installed if you activated the option in the script
 7)- Reboot and enjoy the new kernel

Removal instructions:


To remove an old kernel or the newest one in case of problems
 a)- Boot in a different version and list all installed Linux kernel images typeing the following dpkg command:

   dpkg --list | egrep -i --color 'linux-image|linux-headers|linux-libc'

 b) – Delete unwanted and unused kernel images and header
   b1)- If you are downgrading do the following or go directily to b2)
        We need to restore the old  linux-libc-dev to the kernel will became the newest one
        for example from kernel 6.9.0 => 6.8.9 then

         sudo apt-get install linux-libc-dev_6.8.9-1_amd64.deb
         (take it from old compiled debs, so conserve them!!)

   b2)- Remove the unwanted kernels:

         sudo apt-get --purge remove linux-image-xxxxx linux-headers-xxxxx linux-image-yyyyy linux-headers-yyyyy

     Remove Modules folders of the old kernels

         modulestr=$(dpkg -S /lib/modules/* 2>&1 | grep "no path found matching pattern" | awk '{ print $NF }' | tr "\n" " ")
         sudo rm -r   $modulestr

     Or is done automatically if the script is configured so

   b3)- Update Grub:

         sudo update-grub

 c)- Check if old  kernel modules in /lib/modules of the unused kernels has been removed

 !!Inportant never remove current kernel (use "uname -r" to find it) otherwise you could brick your system

##################################################################################################################
Change log:

V0.1: 2023-12-28
  -Initial version for personal use
V0.2: 2024-07-21
  -First release: Adding many functionality
