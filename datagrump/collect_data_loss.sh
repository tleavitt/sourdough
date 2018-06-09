echo "usage: $0 LOSS_RATE CC_ALG (ctcp or tcp) OUTFILE"
./run-trace-loss 240mbps_link $1 nodebug $2 2>&1 | tee -a $3
