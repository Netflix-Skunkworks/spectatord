#!/usr/bin/env bash

# -- api /: help --
echo "-- / help"
curl -s http://localhost:1234 | jq .

# -- api /config/common_tags: help --
echo "-- /config/common_tags help"
curl -s http://localhost:1234/config/common_tags | jq .

# -- api /config: spectator config --
echo "-- /config spectator config"
curl -s http://localhost:1234/config | jq .

# -- api /config/common_tags: error handling validation --
echo "-- invalid json - expect 400"
curl -X POST -d '{' -w " %{http_code}\n" http://localhost:1234/config/common_tags
curl -X POST -d '[' -w " %{http_code}\n" http://localhost:1234/config/common_tags
curl -X POST -d '["nf.app": "foo", "nf.wat": "bar"]' -w " %{http_code}\n" http://localhost:1234/config/common_tags

echo "-- array not allowed - expect 400"
curl -X POST -d '["foo", "bar", "baz"]' -w " %{http_code}\n" http://localhost:1234/config/common_tags

echo "-- all invalid keys - expect 400"
curl -X POST -d '{"nf.app": "foo", "nf.stack": "bar"}' -w " %{http_code}\n" http://localhost:1234/config/common_tags

echo "-- some invalid keys - expect 400"
curl -X POST -d '{"nf.app": "foo", "mantisJobId": "bar"}' -w " %{http_code}\n" http://localhost:1234/config/common_tags
curl -X POST -d '{"mantisJobId": "foo", "nf.app": "yolo", "mantisJobName": "bar", "mantisUser": ""}' -w " %{http_code}\n" http://localhost:1234/config/common_tags

echo "-- all valid keys (mantisJobId=foo mantisJobName=bar mantisUser='') - expect 200"
curl -X POST -d '{"mantisJobId": "foo", "mantisJobName": "bar", "mantisUser": ""}' -w " %{http_code}\n" http://localhost:1234/config/common_tags

echo "-- valid keys, invalid values - expect 400"
curl -X POST -d '{"mantisJobId": "foo", "mantisJobName": "bar", "mantisUser": 1}' -w " %{http_code}\n" http://localhost:1234/config/common_tags

# -- api /config/common_tags: update validation --
echo "-- starting common tags"
curl -s http://localhost:1234/config |jq '.common_tags'

echo "-- set common tags"
curl -X POST -d '{"mantisJobId": "foo", "mantisJobName": "bar", "mantisUser": "baz"}' -w " %{http_code}\n" http://localhost:1234/config/common_tags
curl -s http://localhost:1234/config |jq '.common_tags'

echo "-- overwrite common tags"
curl -X POST -d '{"mantisJobId": "new-foo", "mantisJobName": "new-bar", "mantisUser": "new-baz"}' -w " %{http_code}\n" http://localhost:1234/config/common_tags
curl -s http://localhost:1234/config |jq '.common_tags'

echo "-- delete common tags"
curl -X POST -d '{"mantisJobId": ""}' -w " %{http_code}\n" http://localhost:1234/config/common_tags
curl -s http://localhost:1234/config |jq '.common_tags'

# -- api: /metrics output validation --
echo "-- list all registry metrics"
curl -s http://localhost:1234/metrics | jq .
