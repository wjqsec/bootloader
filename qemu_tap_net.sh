sudo brctl addbr br0
sudo ip addr add 192.168.1.3/24 dev br0
sudo ifconfig br0 up
#sudo brctl addif br0 eno1
sudo  tunctl -t tap0 -u `whoami`
sudo ip addr add 192.168.1.4/24 dev tap0
sudo ifconfig tap0 up
sudo brctl addif br0 tap0
#sudo  tunctl -t tap1 -u `whoami`
#sudo ip addr add 192.168.1.5/24 dev tap1
#sudo ifconfig tap1 up
#sudo brctl addif br0 tap1
sudo brctl show
