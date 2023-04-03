DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

rm -f tests
ln -s ./goworkspace/pkg/mod/github.com/\!burnt\!sushi/toml-test@v0.1.0/tests
./toml-test  ../toml_json
