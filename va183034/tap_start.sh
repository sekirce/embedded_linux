sudo tunctl -u $(whoami) -t tap0
sudo ifconfig tap0 192.168.10.1
sudo route add -net 192.168.10.0 netmask 255.255.255.0 dev tap0
sudo sh -c "echo 1 > /proc/sys/net/ipv4/ip_forward"
