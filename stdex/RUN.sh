rm -f *.out
for i in *.toml; do
   echo -n $i
   ../toml_cat $i >& $i.out
   if [ -f $i.res ]; then
      if $(diff $i.out $i.res >& /dev/null); then
        echo " [OK]"
      else
	echo " [FAILED]"
      fi
   else
      echo " [?????]"
   fi
 
done
