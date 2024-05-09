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

