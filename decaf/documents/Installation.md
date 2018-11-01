# Installation Guide

​    This part is a quick start guide for setting up and running DECAF, the binary analysis platform based on [QEMU](http://wiki.qemu.org/Main_Page). It assumes that you have some familiarity with Linux. The instructions are based on the release of DECAF shown in the [github](https://github.com/sycurelab/DECAF), running a Ubuntu or Debian. We intermix instructions with explanations about utilities to give an overview of how things work.



## Linux

#### Step 0

DECAF is based on QEMU. It's useful to have a vanilla QEMU for testing and image development. Also, you need to install some stuff to compile qemu/decaf.

```
sudo apt-get build-dep qemu
```



For debian like users (etc. Ubuntu or Debian), you can follow this step to build DECAF at your workspace.

#### Step 1

Install dependencies.

```shell
sudo apt-get install -y libsdl1.2-dev zlib1g-dev libglib2.0-dev libbfd-dev build-essential binutils qemu libboost-dev git lib tool autoconf xorg-dev
```

#### Step 2

Clone the source code from github and set the environment.

```shell
git clone https://github.com/sycurelab/DECAF
cd DECAF/decaf
export DECAF_PATH=`pwd`
```



#### Step 3

Configure the sleuthkit library.

```shell
cd ${DECAF_PATH}/decaf/shared/sleuthkit
rm ./config/ltmain.sh
ln -s /usr/share/libtool/build-aux/ltmain.sh ./config/ltmain.sh
autoconf
./configure
make
```

#### Step 4

Build decaf and choose which features enabled by uncomment  `--enable-tcg-taint`, `--enable-tcg-ir-log` as `./configure` parameters. You can also add `--prefix` parameters to indicate which path DECAF will be  installed.

```shell
cd ${DECAF_PATH}
./configure # parameters
make #-j8 <- you can uncomment this parameter to speed up the complier
```

DECAF has three basic settings about TCG tainting, VMI, TCG IR logging. You can enable/disable at the configuration step. By default, VMI is enabled and TCG taiting and TCG IR logging is disabled.

Here are the parameters you can use to enable/disabled them:

| Feature       | Flags               | Default  |
| ------------- | ------------------- | -------- |
| TCG tainting  | --enable-tcg-taint  | Disabled |
| TCG IR logger | --enable-tcg-ir-log | Disabled |
| VMI           | --disable-vmi       | Enabled  |

#### Step 5

Build plugins

```shell
cd ${DEVAF_PATH}/plugins/[PLUGINS_NAME]
./configure
make
```



## Docker

If you are other system user, you can use docker to create decaf running environment.

### Share files with the docker image

We use docker volume to share files between docker images and the host system. For example, to create a volume named decaf, use the following command.

```
docker volume create decafvolume
```

The [guest images](https://github.com/sycurelab/DECAFImages) or malicious samples can be stored in this volume, so the docker image can access it. Or the out put of DECAF plugin can be sotred in this volume, so that we can extract the analysis results.

### Build the docker image

```shell
docker build -t decaf /path/to/Dockerfile # You can use . if current directory have decaf dockerfile
```

### How to run the  docker image?

To run this  docker image,

1. Copy the guest image to docker volume decafvolume
2. Start the docker.

```shell
sudo docker run -it -e DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v $HOME/.Xauthority:/home/db/.Xauthority  --net=host --mount source=decafvolume,target=/app --name=decaf decaf
```

### Control DECAF within Docker image

Within  the terminal used to run the docker image, users can input commmands of DECAF or DECAF plugins to control DECAF. For example, to load plugin of DECAF(suppose malscalpel is stored in volume decaf, which is named `/app/`  after  mounted)

load_plugin /app/malscalpel.so

### Share files between guest os and host system

We use samba to  share files. If you  read Dockerfile carefully, you will find taht we set `/app/` as the share folder of samba. When we run this docker image, we mount the created docker volume `decafvolume`  as `/app/`. In windows xp  guest OS, the files stored in volume decafvolume can be accessed `10.0.2.2|qemu`. For example, to upload a  file to windows xp guest os, take the following steps.

1. Copy shared file and windows xp image to docker volume `decafvolume`.
2. Run the docker image and mount the volume to `/app/` target.

```shell
sudo docker run -it -e DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v $HOME/.Xauthority:/home/db/.Xauthority --net=host --mount source=decafvolume,target=/app decaf
```

1. After  guest OS starts, open menu  `[start]->[Run]` and input the following  command to open samba folder.

```
\\10.0.2.2
```

Then you can see the files  stored in docker volume decafvolume.

### Interactive with Docker Image

Once docker container created, you can interactive with running  docker container, by typing following command:

```shell
docker exec -it decaf /bin/bash  # decaf is the name of container you sepecified when running it by argument --name=decaf, if you forget it, you can run docker ps -a to find it out.
```

After you get the shell of container, you can compile your new plugins just like the **Step 5** in section **Linux**

Meanwhile, you can exchange file from or into containter with `docker cp`.

