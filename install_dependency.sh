# sudo apt install autoconf
# sudo apt install nasm
# sudo apt install flex
# sudo apt install bison
# sudo apt install clang
# sudo apt install -y gnu-efi
git clone https://github.com/Gehim12/kAFL.git
echo -n "Intel PT support: "; if $(grep -q "intel_pt" /proc/cpuinfo); then echo "✅"; else echo "❌"; fi
sudo apt-get install -y python3-dev python3-venv git build-essential
cd kAFL
make deploy

# vim ./kAFL/kafl/fuzzer/kafl_fuzzer/common/config/default_settings.yaml
# qemu_base: -enable-kvm -machine kAFL64-v1 -cpu kAFL64-Hypervisor-v1,+vmx -no-reboot  -display none -netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no -device e1000,netdev=mynet0,mac=52:55:00:d1:55:01
# #qemu_append: nokaslr oops=panic nopti mitigations=off console=ttyS0