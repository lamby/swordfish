#!/bin/sh

. test_trees_set.sh

# Add another item

POST /database/${DB}/trees/treename/item/aaa zzzz-value
GET /database/${DB}/trees/treename/item/aaa
Assert '{"item": "zzzz-value"}'

GET /database/${DB}/trees/treename
Assert '{"items": [["aaa","zzzz-value"],["zzz","z-value"]]}'

GET /database/${DB}/trees/treename\?values=all
Assert '{"items": [["aaa","zzzz-value"],["zzz","z-value"]]}'

GET /database/${DB}/trees/treename\?values=keys
Assert '{"items": ["aaa","zzz"]}'

GET /database/${DB}/trees/treename\?values=values
Assert '{"items": ["zzzz-value","z-value"]}'
