#systemC introduction

git clone git@github.com:eriknordhamn/syscintro.git

##Installs

###systemC
Download systemC and unpack to, e.g, ~/Development/3rd_party/systemc.
install essentials:
sudo apt install -y build-essential cmake git

cd into directory
mkdir build
cd build
cmake ..
make -j$(nproc)
sudo make install

edit .bashrc and include:
export SYSTEMC_HOME=/opt/systemc
export LD_LIBRARY_PATH=$SYSTEMC_HOME/lib:$LD_LIBRARY_PATH

##CCI
git clone https://github.com/accellera-official/cci.git
git submodule update --init --recursive
mkdir build
cd build
cmake ..
make check
sudo make install


