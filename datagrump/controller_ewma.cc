#include <cstdlib>
#include <iostream>
#include <assert.h>
#include <math.h>       

#include "controller.hh"
#include "timestamp.hh"

using namespace std;

#define MIN_WINDOW_SIZE (5.0)
#define TICK_SIZE (20)
#define PACKET_SIZE_BYTES (1424)

/* Default constructor */
Controller::Controller( const bool debug )
  : debug_( debug ),
    rate_mean(10),
    rate_var( 0 )
{}

/* Get current window size, in datagrams */
unsigned int Controller::window_size()
{
  if ( debug_ ) {
    cerr << "At time " << timestamp_ms()
	 << " window size is " << cwnd << endl;
  }

  return cwnd;
}

/* A datagram was sent */
void Controller::datagram_was_sent( const uint64_t sequence_number,
				    /* of the sent datagram */
				    const uint64_t send_timestamp,
                                    /* in milliseconds */
				    const bool after_timeout
				    /* datagram was sent because of a timeout */ )
{

  send_seqno = max(sequence_number, send_seqno);

  if ( debug_ ) {
    cerr << "At time " << send_timestamp
	 << " sent datagram " << sequence_number << " (timeout = " << after_timeout << ")\n";
  }

}


void Controller::do_ewma_update(const uint64_t tick_time) {

  double cur_rate = double(recv_seqno - prev_tick_seqno) / (tick_time - prev_tick_time);

  rate_mean = mean_smooth * cur_rate + (1 - mean_smooth) * rate_mean;

  /* calculate the rate variance. */
  double sqdev = (cur_rate - rate_mean) * (cur_rate - rate_mean);
  rate_var = var_smooth * sqdev + (1 - var_smooth) * rate_var;

  double cautious_rate = rate_mean - sqrt(rate_var) * confidence_mult;
  /* expected number of bytes to clear the network in the next forecast milliseconds. */

  /* after 100 ms, the number of bytes drained from the queue will be 100 * R.
     If we set cwnd to be exactly this value, a packet that enters the queue now
     will exit the queue 100 ms later. */
  cwnd = (unsigned int)max(cautious_rate * forecast_ms, MIN_WINDOW_SIZE);

  if (debug_) {
    cerr << "cur rate: " << cur_rate << endl
    << "rate mean: " << rate_mean << ", rate var: " << rate_var << endl
    << "cautious_rate: " << cautious_rate << endl
    << "cwnd: " << cwnd << endl;
  }
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

  recv_seqno = max(sequence_number_acked, recv_seqno);
  if (recv_timestamp_acked > prev_tick_time + TICK_SIZE) /* timer tick, do update */
  { 
    do_ewma_update(recv_timestamp_acked); 
    prev_tick_time = recv_timestamp_acked;
    prev_tick_seqno = recv_seqno;
  }

  if ( debug_ ) {
    cerr << "At time " << timestamp_ack_received
	 << " received ack for datagram " << sequence_number_acked
	 << " (send @ time " << send_timestamp_acked
	 << ", received @ time " << recv_timestamp_acked << " by receiver's clock)" << endl;
  }
}

/* How long to wait (in milliseconds) if there are no acks
   before sending one more datagram */
unsigned int Controller::timeout_ms()
{
  return 500; /* timeout of one second */
}
