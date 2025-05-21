#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

export GOBIN=$DIR
go install github.com/toml-lang/toml-test/cmd/toml-test@latest # install test suite