#!/bin/sh

. _testcase

GET /database/${DB}/stats

test "${RET}" != ""
