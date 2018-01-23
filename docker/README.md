# README #

A Dockerfile to create decaf running environments.



### Share files with the docker image ###

We use docker volume to share files between docker images and the host system. For example, to create a volume named decaf, use the following command.

`docker volume create decaf`

The [guest images](https://github.com/sycurelab/DECAFImages "DECAFImages") or malicious samples cannbe stored in this volume, so the docker image can access it. Or the out put of DECAF plugin can be sotred in this volume, so that we can extract the analysis results.


### Build the docker image ###

`docker built -t decaf ./decaf/`

### How to run the docker image? ###


To run this docker image, 


`sudo docker run -it -e  DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v $HOME/.Xauthority:/home/db/.Xauthority --net=host --mount source=decaf,target=/app decaf`


### Use DECAF within Docker image ###

Within the terminal used to run the docker image, users can input commands of DECAF or DECAF plugins to control DECAF. For example, to load plugin of DECAF(suppose malscalpel is stored in volume decaf, which is named /app/ after mounted, 

`load_plugin /app/malscalpel.so`


