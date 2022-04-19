#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
echo "PARAMS: $@" > sver_params.log
echo "EXEC: ${SCRIPT_DIR}/build/sver" >> sver_params.log
tee sver_in.log | ${SCRIPT_DIR}/build/sver $@ | tee sver_out.log
#tee sver_in.log | svlangserver $@ | tee sver_out.log
