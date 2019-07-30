# README #

A Dockerfile to create decaf running environments.



### Share files with the docker image ###

We use docker volume to share files between docker images and the host system. For example, to create a volume named decaf, use the following command.

`docker volume create decafvolume`

The [guest images](https://github.com/sycurelab/DECAFImages "DECAFImages") or malicious samples can be stored in this volume, so the docker image can access it. Or the out put of DECAF plugin can be sotred in this volume, so that we can extract the analysis results.


### Build the docker image ###

`docker build -t decaf ./decaf/`

### How to run the docker image? ###


To run this docker image, 

1. Copy the guest image to docker volume *decafvolume*

2. Start the docker. 


`sudo docker run -it -e  DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v $HOME/.Xauthority:/home/db/.Xauthority --net=host --mount source=decafvolume,target=/app decaf`


### Control DECAF within Docker image ###

Within the terminal used to run the docker image, users can input commands of DECAF or DECAF plugins to control DECAF. For example, to load plugin of DECAF(suppose malscalpel is stored in volume decaf, which is named /app/ after mounted, 

`load_plugin /app/malscalpel.so`

### Share files between guest os and host system ##

We use samba to share files.  If you read Dockerfile carefully, you will find that we set */app/* as the share folder of samba. When we run this docker image, we mount the created docker volume *decafvolume as */app/*.  In windows xp guest OS, the files stored in volume *decafvolume* can be accesed *\\10.0.2.2\qemu*.  For example, to upload a file to windows xp guest os, take the following steps.



1. Copy shared file and [windows xp image](https://github.com/sycurelab/DECAFImages "DECAFImages") to docker volume *decafvolume*

2. Run the docker image and mount the volume to /app/ target.


`sudo docker run -it -e  DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v $HOME/.Xauthority:/home/db/.Xauthority --net=host --mount source=decafvolume,target=/app decaf`


3. After guest OS starts,  open menu `start-Run` and input the following command.

`\\10.0.2.2`

Then you can see the files stored in docker volume decafvolume.






