echo "usage: $0 BG_RATE CC_ALG (ctcp or tcp) OUTFILE"
./run-trace 240mbps_link $1 nodebug $2 2>&1 | tee -a $3
