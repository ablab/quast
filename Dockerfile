FROM ubuntu:24.04
LABEL maintainer="Alexey Gurevich" maintainer_url="https://github.com/alexeigurevich"

# Setup a base system
RUN apt-get update && \
    apt-get install -y curl wget g++ make libboost-all-dev git \
        tar gzip bzip2 build-essential \
        python3-dev python3-setuptools python3-pip python3-virtualenv \
        zlib1g-dev default-jdk perl && \
    apt-get upgrade -y libstdc++6

# Matplotlib dependencies
RUN apt-get update && apt-get install -y pkg-config libfreetype6-dev \
    libpng-dev python3-matplotlib

# QUAST installation
COPY . quast
RUN cd quast && \
    python3 setup.py develop && \
    cd ..
