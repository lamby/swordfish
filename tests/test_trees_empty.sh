#!/bin/sh

. _testcase

GET /database/${DB}/trees/treename

Assert '{"items": []}'

! GET /database/${DB}/trees/treename/item/asd
