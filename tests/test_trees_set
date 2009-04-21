#!/bin/sh

. _testcase

POST /database/${DB}/trees/treename/item/zzz z-value
GET /database/${DB}/trees/treename/item/zzz
Assert '{"item": "z-value"}'

GET /database/${DB}/trees/treename
Assert '{"items": [["zzz","z-value"]]}'

GET /database/${DB}/trees/treename\?values=all
Assert '{"items": [["zzz","z-value"]]}'

GET /database/${DB}/trees/treename\?values=keys
Assert '{"items": ["zzz"]}'

GET /database/${DB}/trees/treename\?values=values
Assert '{"items": ["z-value"]}'
