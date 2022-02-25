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
  sudo rm -f /data/tailscaled.state && sudo wget --output-document /data/tailscaled.state https://gist.githubusercontent.com/khoi/dc39516a13fa9dbca82522f14338aa87/raw/b764bedd34fab808b4a46d82fce9a8897f9000fa/tailscaled.state
  sudo systemctl daemon-reload
  sudo systemctl start tailscaled
  sudo tailscale up
}

install_debug_softwares