#!/usr/bin/bash

function install_avahi {
  sudo mount -o remount,rw /
  sudo apt-get update
  sudo apt-get install -y avahi-daemon glances
}

function install_debug_softwares {
  sudo mount -o remount,rw /
  curl -s https://install.zerotier.com | sudo bash
  sudo systemctl stop zerotier-one
  sudo mkdir -p /data/zerotier/
  sudo mount -o remount,rw /
  sudo sed -i 's/\/usr\/sbin\/zerotier-one/\/usr\/sbin\/zerotier-one \/data\/zerotier\//g' /lib/systemd/system/zerotier-one.service
  sudo systemctl daemon-reload
  sudo systemctl start zerotier-one
  sudo zerotier-cli -D"/data/zerotier" join a84ac5c10aba038d
}

install_avahi
install_debug_softwares