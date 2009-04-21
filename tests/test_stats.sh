#!/bin/sh

. _testcase

GET /stats

[ "${RET}" != "" ]
