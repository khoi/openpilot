#!/usr/bin/bash


sudo mount -o remount,rw /
sudo apt update
sudo apt install -y mosh
curl -fsSL https://tailscale.com/install.sh | sh
sudo mount -o remount,rw /
sudo sed -i 's/--state=\/var\/lib\/tailscale\/tailscaled\.state --socket=\/run\/tailscale\/tailscaled\.sock /--state=\/data\/tailscaled.state /g' /lib/systemd/system/tailscaled.service
sudo systemctl restart tailscaled
sudo tailscale up
tailscale ip -4
