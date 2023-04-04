if ! (which jq >& /dev/null); then 
    echo "ERROR: please install the 'jq' utility"
    exit 1
fi

#
#  POSITIVE tests
#
for i in toml-spec-tests/values/*.toml; do
    fname="$i"
    ext="${fname##*.}"
    fname="${fname%.*}"
    echo -n $fname ' '
    res='[OK]'
    if (../toml_json $fname.toml >& $fname.json.out); then 
        jq -S . $fname.json.out > t.json
	mv t.json $fname.json.out
	if [ -f $fname.json ]; then
	    if ! (diff $fname.json $fname.json.out >& /dev/null); then 
	        res='[FAILED]'
	    else
		rm -f $fname.json.out
	    fi
	else
	    res='[??]'
	fi
    fi
    echo ... $res
done


#
#  NEGATIVE tests
#
for i in toml-spec-tests/errors/*.toml; do 
    echo -n $i ' '
    res='[OK]'
    if (../toml_json $i >& $i.json.out); then 
	res='[FAILED]'
    fi
    echo ... $res
done
