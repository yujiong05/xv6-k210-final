make clean
dos2unix etc/policy_*.txt
rm -f fs.img
make fs
make run