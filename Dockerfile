################################################################################
#
# Build in the source
#
################################################################################

ARG BASE_IMAGE=elementaryrobotics/atom:cpp-rewrite
ARG PRODUCTION_IMAGE=debian:buster-slim
FROM $BASE_IMAGE as atom-source

ARG DEBIAN_FRONTEND=noninteractive

#
# Redis itself
#

# Build redis
ADD ./third-party/redis /atom/third-party/redis
RUN cd /atom/third-party/redis && make -j8 && make PREFIX=/usr/local install

#
# C client
#

# Build the C library
ADD ./languages/c /atom/languages/c
RUN cd /atom/languages/c \
 && make clean && make -j8 && make install

#
# C++ client
#

# Build and install the c++ library
ADD ./languages/cpp /atom/languages/cpp
RUN cd /atom/languages/cpp \
 && make clean && make -j8 && make install

#
# Python client
#

# Build and install the python library
# Add and install requirements first to use DLC
ADD ./languages/python/requirements.txt /atom/languages/python/requirements.txt
RUN pip3 install --no-cache-dir -r /atom/languages/python/requirements.txt
ADD ./lua-scripts /atom/lua-scripts
ADD ./languages/python /atom/languages/python
RUN cd /atom/languages/python \
 && python3 setup.py install

#
# Command-line utility
#

ADD ./utilities/atom-cli/requirements.txt /atom/utilities/atom-cli/requirements.txt
RUN pip3 install --no-cache-dir -r /atom/utilities/atom-cli/requirements.txt
ADD ./utilities/atom-cli /atom/utilities/atom-cli
RUN cp /atom/utilities/atom-cli/atom-cli.py /usr/local/bin/atom-cli \
 && chmod +x /usr/local/bin/atom-cli

#
# Finish up
#

# Change working directory back to atom location
WORKDIR /atom

################################################################################
#
# Production atom image. Strips out source. Only includes libraries, headers
#     and Python venv.
#
################################################################################

FROM $PRODUCTION_IMAGE as atom

# Install python
RUN apt-get update -y \
 && apt-get install -y --no-install-recommends apt-utils \
                                               python3-minimal \
                                               python3-pip \
                                               libatomic1


# Copy contents of python virtualenv and activate
COPY --from=atom-source /opt/venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Copy C builds
COPY --from=atom-source /usr/local/lib /usr/local/lib
COPY --from=atom-source /usr/local/include /usr/local/include
ENV LD_LIBRARY_PATH=/usr/local/lib:${LD_LIBRARY_PATH}

# Copy atom-cli
COPY --from=atom-source /usr/local/bin/atom-cli /usr/local/bin/atom-cli

# Copy redis-cli
COPY --from=atom-source /usr/local/bin/redis-cli /usr/local/bin/redis-cli

# Add .circleci for docs build
ADD ./.circleci /atom/.circleci

# Change working directory back to atom location
WORKDIR /atom

################################################################################
#
# Nucleus image. Copies out only binary of redis-server
#
################################################################################

FROM atom as nucleus

# Add in redis-server
COPY --from=atom-source /usr/local/bin/redis-server /usr/local/bin/redis-server

ADD ./launch_nucleus.sh /nucleus/launch.sh
ADD ./redis.conf /nucleus/redis.conf
WORKDIR /nucleus
RUN chmod +x launch.sh
CMD ["./launch.sh"]

################################################################################
#
# Test image for atom release. Based off of production, adds in test dependencies
#
################################################################################

FROM atom as test

ARG DEBIAN_FRONTEND=noninteractive

#
# Install test dependencies
#

# Install googletest

ADD ./gtest_libs /atom/gtest_libs
WORKDIR /atom/gtest_libs
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    cmake \
    build-essential \
    python3-pip \
    lcov \
    gdb \
 && tar -xf release-1.10.0.tar.gz \
 && mkdir -p build \
 && cd build \
 && cmake ../googletest-release-1.10.0 \
 &&  make -j8 \
 &&  make install \
 && cd .. \
 && rm -rf build googletest-release-1.10.0 release-1.10.0.tar.gz

WORKDIR /atom

# Install valgrind
RUN apt-get install -y --no-install-recommends valgrind

# Install pytest
RUN pip3 install --no-cache-dir pytest

# Copy source code
COPY ./languages/c/ /atom/languages/c
COPY ./languages/cpp/ /atom/languages/cpp
COPY ./languages/python/tests /atom/languages/python/tests
