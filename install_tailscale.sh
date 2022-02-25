#!/usr/bin/bash

function install_debug_softwares {

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

install_debug_softwares