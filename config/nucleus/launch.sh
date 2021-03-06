#!/bin/bash

SUPERVISOR_DIR=/etc/supervisor
SUPERVISOR_INCLUDE_DIR=include

# If we're using metrics, launch with supervisor
if [[ ${ATOM_USE_METRICS} == "TRUE" ]]; then

    # If we don't want to launch the metrics monitor process
    #   we can move the corresponding supervisord conf file
    #   so it gets ignored. This way we can still find it later
    #   in the container if we want it
    if [[ ${NUCLEUS_METRICS_MONITOR} != "TRUE" ]]; then
        echo "Metrics ${ATOM_USE_METRICS}, Monitor ${NUCLEUS_METRICS_MONITOR} -- not running monitor element"
        mv ${SUPERVISOR_DIR}/${SUPERVISOR_INCLUDE_DIR}/metrics_monitor.conf \
            ${SUPERVISOR_DIR}/${SUPERVISOR_INCLUDE_DIR}/metrics_monitor.conf.ignore
    fi

    # Launch supervisor
    /usr/bin/supervisord -c ${SUPERVISOR_DIR}/supervisord.conf

# Otherwise, launch directly, i.e. original benavior
else

    /usr/local/bin/redis-server /etc/redis/redis-atom.conf

fi
