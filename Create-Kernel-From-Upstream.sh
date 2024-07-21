#!/bin/bash
#This script will compile a kernel and create the deb packages
#Copy it in the a folder and run it in a sh terminal by drag and drop

echo  " "
echo  " ##################################################################"
echo  " #                   Kernel Download & Compile  Script            #"
echo  " #            Developed   by sergio melas 2021-24                 #"
echo  " #                                                                #"
echo  " #                Emai: sergiomelas@gmail.com                     #"
echo  " #                   Released unde GPV V2.0                       #"
echo  " #                                                                #"
echo  " ##################################################################"
echo  " "


#Admin login
echo  "Login as administrator to install"
sudo ls >/dev/null
echo  ""

#Install libs
sudo apt-get install build-essential libncurses-dev bison flex libssl-dev libelf-dev dwarves debhelper
sudo apt-get install wget

#Change to local directory
echo  ""
VAR=$0
DIR="$(dirname "${VAR}")"
cd  "${DIR}"

rm -r ./linux-latest

mkdir ./linux-latest

cd ./linux-latest



#download latest kernel from github
wget https://github.com/torvalds/linux/archive/refs/heads/master.zip

unzip './master.zip'
cd linux-master


#Configure kernel
cp -v /boot/config-$(uname -r) .config
make olddefconfig

#Cofigure modules
#Eample Lenovo 14'' AMD my system
#scripts/config --set-val CONFIG_THINKPAD_ACPI         y
#scripts/config --set-val CONFIG_LENOVO_YMC            y
#scripts/config --set-val CONFIG_SENSORS_LENOVO_EC     y
#scripts/config --set-val CONFIG_LENOVO_SE10_WDT       y
#scripts/config --set-val CONFIG_LENOVO_WMI_CAMERA     y


#Compile kernel
make -j$(nproc) bindeb-pkg

#Clean up
cd ../
rm -r ./linux-master
rm ./master.zip
rm *.buildinfo
rm *.changes
# Comment following line to not remove debug immage
rm linux-image*-dbg_*amd64.deb

#To install automatically uncomment next line
#sudo dpkg -i linux-*.deb

#Uncomment to remove old modules of deleted kernels
#sudo rm -r   $(dpkg -S /lib/modules/* 2>&1 | grep "no path found matching pattern" | awk '{ print $NF }' | tr "\n" " ")


