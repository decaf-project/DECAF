FROM ubuntu:xenial-20210416

RUN apt-get update 
RUN apt-get install libsdl1.2-dev -y
RUN apt-get install zlib1g-dev -y
RUN apt-get install libglib2.0-dev -y
RUN apt-get install libbfd-dev -y
RUN apt-get install build-essential -y
RUN apt-get install binutils -y
RUN apt-get install qemu -y
RUN apt-get install libboost-dev -y
RUN apt-get install git -y
RUN apt-get install libtool -y
RUN apt-get install autoconf -y
RUN apt-get install sudo -y
RUN apt-get install xorg-dev -y



WORKDIR /decafroot
RUN git clone https://github.com/sycurelab/DECAF.git

RUN pwd & ls
#ADD . /decafroot

#configure sleuthkit

WORKDIR /decafroot/DECAF/decaf/shared/sleuthkit
RUN rm ./config/ltmain.sh
RUN ln -s /usr/share/libtool/build-aux/ltmain.sh ./config/ltmain.sh
RUN autoconf
RUN ./configure
RUN make
WORKDIR /decafroot/DECAF/decaf
RUN ./configure --disable-tcg-taint --target-list=i386-softmmu

RUN make

RUN export uid=1000 gid=1000 
RUN mkdir -p /home/db/ 
RUN echo "db:x:${uid}:${gid}:db,,,:/home/db:/bin/ bash" >> /etc/passwd
RUN echo "db:x:${uid}:" >> /etc/group
RUN echo "db ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/db
RUN chmod 0440 /etc/sudoers.d/db
RUN chown ${uid}:${gid} -R /home/db

USER db
ENV HOME /home/db


RUN apt-get update 
#setup samba to share file between guest os and host for qemu
RUN apt-get install samba -y
WORKDIR /etc/samba/
RUN echo "[QEMU]" >> /etc/samba/smb.conf
RUN echo "	path=/app/" >>/etc/samba/smb.conf
RUN echo "	browseable = yes" >> /etc/samba/smb.conf
RUN echo "	guest ok = yes" >> /etc/samba/smb.conf
RUN echo "	writable = yes" >> /etc/samba/smb.conf
RUN echo "	create mask = 777" >> /etc/samba/smb.conf
RUN cat /etc/samba/smb.conf
#RUN /etc/init.d/samba start

WORKDIR /decafroot/
RUN apt install wget
RUN wget https://github.com/google/protobuf/releases/download/v3.5.0/protobuf-all-3.5.0.tar.gz 
RUN tar -xvf protobuf-all-3.5.0.tar.gz
WORKDIR /decafroot/protobuf-3.5.0
RUN ./autogen.sh
RUN ./configure --prefix=/usr/
RUN make
RUN make install




WORKDIR /decafroot/DECAF/decaf/i386-softmmu/
#CMD ["/bin/bash"]
CMD export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/ &/etc/init.d/samba start & ./qemu-system-i386 -monitor stdio -m 256 -net user,smb=/app/ -netdev user,id=mynet -device rtl8139,netdev=mynet  /app/winxpsp3_ie6.1.img -s
#CMD /etc/init.d/samba status