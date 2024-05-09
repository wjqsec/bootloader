sudo brctl delif br0 tap0
sudo tunctl -d tap0
sudo brctl delif br0 eno1
sudo ifconfig br0 down
sudo brctl delbr br0
