#!/bin/bash -e

apt-get update
apt-get -y upgrade
apt-get -y install git devscripts

#getting/building/installing
cd ~

if [ -d yuma123-git ] ; then
 cd yuma123-git
else
 git clone https://git.code.sf.net/p/yuma123/git yuma123-git
fi

cd yuma123-git

sudo mk-build-deps  -i -t 'apt-get -y'                                 
git clean -f

autoreconf -i -f
./configure --prefix=/usr
make
sudo make install

cd netconf/python
sudo mk-build-deps  -i -t 'apt-get -y'
autoreconf -i -f
./configure --prefix=/usr
make
sudo make install

#testing

ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa
cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
ssh-keyscan -t rsa -H localhost >> ~/.ssh/known_hosts

echo "PermitRootLogin yes" >> /etc/ssh/sshd_config
echo "Port 22" >> /etc/ssh/sshd_config
echo "Port 830" >> /etc/ssh/sshd_config
echo "Port 1830" >> /etc/ssh/sshd_config
echo "Port 2830" >> /etc/ssh/sshd_config
echo "Port 3830" >> /etc/ssh/sshd_config
echo 'Subsystem netconf "/usr/sbin/netconf-subsystem --ncxserver-sockname=830@/tmp/ncxserver.sock --ncxserver-sockname=1830@/tmp/ncxserver.1830.sock"' >> /etc/ssh/sshd_config

cd ~/yuma123-git/netconf/test/netconfd
apt-get -y install python-ncclient valgrind

autoreconf -i -f
./configure --prefix=/usr
make
make install
make check || true

cd ~/yuma123-git/netconf/test/yangcli
apt-get -y install expect

autoreconf -i -f
./configure --prefix=/usr
make
make install
make check || true

cd ~/yuma123_${ver}/netconf/test/yangdump

autoreconf -i -f
./configure --prefix=/usr
make
make install
make check || true

echo "Success!"