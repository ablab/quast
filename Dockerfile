FROM ubuntu:15.04
MAINTAINER Alexey Gurevich "https://github.com/alexeigurevich"

# Setup a base system
RUN apt-get update && \
    apt-get install -y curl wget g++ make libboost-all-dev git \
        tar gzip bzip2 build-essential python2.7-dev python-pip  \
        python-virtualenv zlib1g-dev default-jre perl && \
    apt-get upgrade -y libstdc++6

# Perl packages
#RUN Rscript -e "install.packages('Time::HiRes')"

# Matplotlib dependencies
RUN apt-get update && apt-get install -y pkg-config libfreetype6-dev \
    libpng-dev python-matplotlib

# TargQC installation
COPY . quast
RUN pip install --upgrade setuptools pip && \
    cd quast && \
    python setup.py develop && \
    cd ..

