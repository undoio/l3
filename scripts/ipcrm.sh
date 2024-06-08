#!/bin/bash
# ##############################################################################
# Release unused shared segments.
#
# History:
#	May-2024	Written; developing perf u-benchmarking scripts for L3-logging
# ##############################################################################

set -e -u -o pipefail

Me=$(basename "$0")

for shmid in $(ipcs -m | grep -w '0' | awk '{print $2}'); do

	echo "$Me: Releasing shmid $shmid"
	ipcrm -m ${shmid}
done

for msqid in $(ipcs -q | grep -v -i -w -E '1|Message|msqid' | awk '{print $2}'); do
    echo "${msqid}"
	ipcrm -q ${msqid}
done

ipcs -am
