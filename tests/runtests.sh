#!/bin/sh

set -eu

TESTS=0
PASSED=0
FAILED=0

FAILED_TESTS=""

for TEST in test_*
do
	TESTS=$(($TESTS+1))

	if sh ${TEST} runner
	then
		PASSED=$(($PASSED+1))
	else
		FAILED=$(($FAILED+1))

		FAILED_TESTS="${FAILED_TESTS} ${TEST}"
	fi
done

echo
echo "===================================================================="
echo " ${TESTS} test(s) run, ${PASSED} passed, ${FAILED} failed."

if [ "${FAILED}" = 0 ]
then
	exit 0
fi

echo
echo "The following tests failed:"

for TEST in ${FAILED_TESTS}
do
	printf ' * %s\n' "${TEST}"
done

exit 1
