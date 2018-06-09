#include <cstdlib>
#include <iostream>
#include <cassert>
#include <math.h>

#include "controller.hh"
#include "timestamp.hh"

using namespace std;

#define MIN_WINDOW_SIZE (5.0)
#define TICK_SIZE (20)
#define PACKET_SIZE_BYTES (1424)

/* Default constructor */
Controller::Controller( const bool debug, const bool use_ctcp )
  : debug_( debug ),
    use_ctcp_( use_ctcp ),
    cwnd(1),
    dwnd(0),
    cwnd_(1.),
    dwnd_(0.)
{
  cerr << "cwnd: " << cwnd << " dwnd: " << dwnd << endl;
  cerr << "cwnd_: " << cwnd_ << " dwnd_: " << dwnd_ << endl;
}

/* Get current window size, in datagrams */
unsigned int Controller::window_size()
{
  /* Default: fixed window size of 100 outstanding datagrams */
  if ( debug_ ) {
    cerr << "At time " << timestamp_ms()
	 << " window size is " << cwnd + dwnd << endl;
  }
  assert (cwnd + dwnd >= 0);
  return cwnd + dwnd;
}


void Controller::enter_slow_start() {
  /* revert to slow start */ 
  cwnd_ = cwnd = 1;
  dwnd_ = dwnd = 0;
  slow_start = true;
}

/* A datagram was sent */
void Controller::datagram_was_sent( const uint64_t sequence_number,
				    /* of the sent datagram */
				    const uint64_t send_timestamp,
                                    /* in milliseconds */
				    const bool after_timeout
				    /* datagram was sent because of a timeout */ )
{
  /* Default: take no action */
  if (after_timeout) {
    enter_slow_start();
  }

  if ( debug_ ) {
    cerr << "At time " << send_timestamp
	 << " sent datagram " << sequence_number << " (timeout = " << after_timeout << ")\n";
  }

}

void Controller::update_rtt(const uint64_t timestamp_ack_received, 
                               const uint64_t send_timestamp_acked) {

  double cur_rtt = double(timestamp_ack_received - send_timestamp_acked);
  rtt = rtt_smooth * cur_rtt + (1 - rtt_smooth) * rtt;
  if (base_rtt > cur_rtt)
    base_rtt = cur_rtt;
  if (debug_)
    cerr << "rtt: " << rtt << endl; 
}

void Controller::update_dwnd(double win, double diff, bool loss) {
  if (loss) {
    dwnd_ = win * (1 - beta) - float(cwnd) / 2; 
    if (dwnd_ < 0) dwnd_ = 0; 
  } else if (diff < gamma) {
    double update = alpha * pow(win, k) - 1;
    if (update < 0) update = 0;
    dwnd_ += update;
  } else {
    dwnd_ -= zeta * diff; 
    if (dwnd_ < 0) dwnd_ = 0;
  }
  assert(dwnd_ >= 0);

}


bool is_router_buffer_full(const uint64_t send_timestamp_acked, const uint64_t timestamp_ack_received)
{
  // return timestamp_ack_received > send_timestamp_acked + 330; //for 72 Mbps
  return timestamp_ack_received > send_timestamp_acked + 155; //for 360 Mbps
  // return timestamp_ack_received > send_timestamp_acked + 130; //for 360 Mbps
  /* heuristic: 
     assume a 1500 packet buffer, 12,000 bits per MTU packet, link rate of 72Mbps, rtprop of 80ms
     then the buffer will be full when the packet delay is:
     1500 pkt * 12000 b/pkt / (72 Mbps) + 80 ms = 330 ms. */
}

/* An ack was received */
void Controller::ack_received( const uint64_t sequence_number_acked,
			       /* what sequence number was acknowledged */
			       const uint64_t send_timestamp_acked,
			       /* when the acknowledged datagram was sent (sender's clock) */
			       const uint64_t recv_timestamp_acked,
			       /* when the acknowledged datagram was received (receiver's clock)*/
			       const uint64_t timestamp_ack_received )
                               /* when the ack was received (by sender) */
{

  bool stochastic_loss = next_ack_expected_ != sequence_number_acked;
  bool packet_loss = stochastic_loss || is_router_buffer_full(send_timestamp_acked, timestamp_ack_received);

  bool loss = false;
  if (packet_loss && timestamp_ack_received > loss_timestamp + LOSS_TIMEOUT) {
    loss = true;
    loss_timestamp = timestamp_ack_received;
  }

  next_ack_expected_ = max(next_ack_expected_, sequence_number_acked + 1);

  update_rtt(timestamp_ack_received, send_timestamp_acked);
  if (debug_ && loss)
    cerr << "loss!" << endl;

  if (slow_start) {
    if (loss) {
      cwnd = 1;
    } else {
      cwnd += 1;
      if (rtt > SLOWSTART_TIMEOUT)
        slow_start = false; 
    }
    cwnd_ = cwnd;
    dwnd_ = dwnd;
  } else {
    /* update cwnd according to normal tcp: */
    if (loss) {
      cwnd_ /= 2.0;
      if (cwnd_ <= 1.0)
        enter_slow_start();

    } else
      cwnd_ += 1.0/(cwnd_ + dwnd_);
    
    if (use_ctcp_) {
      double win = cwnd_ + dwnd_;
      double expected = win / base_rtt;
      double actual =  win / rtt;
      double diff = (expected - actual) * base_rtt;
      update_dwnd(win, diff, loss);
    }

    cwnd = int(cwnd_);
    dwnd = int(dwnd_);
  } 

  if ( debug_ ) {
    cerr << "At time " << timestamp_ack_received
	 << " received ack for datagram " << sequence_number_acked
	 << " (send @ time " << send_timestamp_acked
	 << ", received @ time " << recv_timestamp_acked << " by receiver's clock)"
   << "slow start: " << slow_start
	 << endl;
  }
}

/* How long to wait (in milliseconds) if there are no acks
   before sending one more datagram */
unsigned int Controller::timeout_ms()
{
  return 400; /* timeout of half a second */
}
