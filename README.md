# ZD1200 KERNEL BUILDING PROCEDURES

## PRE-REQUISITES:

Install the following packages in an ubuntu - 14.04.6 LTS machine

```bash
sudo apt-get update
sudo ln -s /usr/bin/env /bin/env
sudo apt-get -y install build-essential
sudo apt-get -y install g++ curl cvs bison gettext flex texinfo libtool patch automake gawk lzma module-init-tools libssl-dev m4 git 
sudo apt-get -y install pkg-config unzip
sudo apt-get remove libnss-mdns
sudo apt-get install -y libncurses5-dev
sudo apt-get install -y python-setuptools
```

## Building steps:

1)	Untar zd1200_task.tar.gz file using  
$ `tar -xvf zd1200_task.tar.gz`

2)	Change the directory to zd1200_task  
$ `cd zd1200_task`

3) Change the directory to linux-mips  
$ `cd linux/kernels/linux-mips`

4)	apply the patch by using git apply  
$ `git apply --reject --whitespace=fix  ../../../zd1200.patch`

5)	Change the directory to buildroot  
$ `cd ../../../buildroot`

6) Give the build command in the buildroot directory  
$ `sudo make PROFILE=zd1200 `

7) While kernel is getting compiled, it asks for some configurations, press Enter for all

8) It will be built successfully and ends with providing a version number
