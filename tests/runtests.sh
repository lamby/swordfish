#!/bin/sh

set -eu

TESTS=0
PASSED=0
FAILED=0

for TEST in ./test_*.sh
do
	TESTS=$(($TESTS+1))

	if sh ${TEST} runner
	then
		PASSED=$(($PASSED+1))
	else
		FAILED=$(($FAILED+1))
	fi
done

echo
echo "===================================================================="
echo " ${TESTS} test(s) run, ${PASSED} passed, ${FAILED} failed."

if [ "${FAILED}" != 0 ]
then
	exit 1
fi
