FROM alpine/flake8:3.7.0

ADD ./requirements.txt .

# Need to install build tools and auto-formatters
RUN apk add git openssh build-base && \
    pip3 install --upgrade pip && \
    pip3 install -r requirements.txt

# Add in the .flake8 spec
ADD ./.flake8 /usr/local/lib/.flake8

# Add in the entry script
ADD ./run.sh /usr/local/bin/run.sh

# Make the code directory. It is expected that code is
#   loaded into here when beginning the checking process
RUN mkdir /code

#
# Default environment variables
#

# Don't do formatting automatically. Set to something
#   non-empty to perform formatting. Choose a single
#   available formatter for a default here. Black is the
#   only currently supported formater and therefor default
ENV DO_FORMAT=""
ENV FORMAT_BLACK="y"

# Do check automatically. Set to something empty
#   to turn off checking
ENV DO_CHECK="y"

# Don't hang automatically. This is useful for dev
#   purposes but shouldn't be turned on in prod
ENV DO_HANG=""

# Excludes
ENV FLAKE8_EXCLUDE=third-party,languages/python/third-party,languages/python/build
ENV BLACK_EXCLUDE=third-party|build
ENV ISORT_EXCLUDE="--skip third-party --skip build"

# Code to check
ENV CODE_DIR=/code

# Define the entrypoint -- need to override the default
ENTRYPOINT ["/usr/local/bin/run.sh"]
