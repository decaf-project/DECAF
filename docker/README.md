# README #

A Dockerfile to create decaf running environments.



### Share files with the docker image###

We use docker volume to share files between docker images and the host system. For example, to create a volume named decaf, use the following command.

`docker volume create my-vol`

### How do I get set up? ###


To run this docker image, 


sudo docker run -it -e  DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v $HOME/.Xauthority:/home/db/.Xauthority --net=host --mount source=decaf,target=/app decaf


### Contribution guidelines ###

* Writing tests
* Code review
* Other guidelines

### Who do I talk to? ###

* Repo owner or admin
* Other community or team contact
