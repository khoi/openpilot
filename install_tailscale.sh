#!/usr/bin/bash

function install_tailscale {

  sudo mount -o remount,rw /
  sudo apt update
  sudo mount -o remount,rw /
  eval "$(curl -fsSL https://tailscale.com/install.sh)"
  sudo mount -o remount,rw /
  sudo systemctl stop tailscaled
  
  sudo mount -o remount,rw /
  sudo sed -i 's/--state=\/var\/lib\/tailscale\/tailscaled\.state --socket=\/run\/tailscale\/tailscaled\.sock /--state=\/data\/tailscaled.state /g' /lib/systemd/system/tailscaled.service
  sudo systemctl daemon-reload
  sudo systemctl start tailscaled
  sudo tailscale up
}

function persist_syslog {
    sudo mkdir -p /data/syslog
    sudo chmod 0777 -R /data/syslog
    sudo mount -o remount,rw /
    sudo sed -i 's/\/var\/log/\/data\/syslog/g' /etc/rsyslog.d/50-default.conf
}

install_tailscale
persist_syslog