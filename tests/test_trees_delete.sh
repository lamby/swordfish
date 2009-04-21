#!/bin/sh

. _testcase

! GET /database/${DB}/trees/treename/item/aaa

POST /database/${DB}/trees/treename/item/aaa "value"

GET /database/${DB}/trees/treename
Assert '{"items": [["aaa","value"]]}'

POST /database/${DB}/trees/treename/item/aaa ""

! GET /database/${DB}/trees/treename/item/aaa

GET /database/${DB}/trees/treename
Assert '{"items": []}'
