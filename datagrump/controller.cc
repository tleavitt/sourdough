#include <cstdlib>
#include <iostream>
#include <assert.h>
#include <math.h>       

#include "controller.hh"
#include "timestamp.hh"

using namespace std;

#define MIN_WINDOW_SIZE (1.0)
#define TICK_SIZE (50)
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

  // if (after_timeout) {
  //    probably an outage... 
  //   cwnd = 1;

  //   state = 0;
  // }

  if ( debug_ ) {
    cerr << "At time " << send_timestamp
	 << " sent datagram " << sequence_number << " (timeout = " << after_timeout << ")\n";
  }

}

void Controller::do_ewma_probe(const uint64_t tick_time) {

  double cur_rate = double(recv_seqno - prev_tick_seqno) / double(tick_time - prev_tick_time);
  rate_mean = cur_rate;
  rate_var = sqrt(rate_mean);

  if (debug_) {
    cerr << "cur rate: " << cur_rate << endl
    << "rate mean: " << rate_mean << ", rate var: " << rate_var << endl
    << "cwnd: " << cwnd << endl;
  }
} 

void Controller::do_ewma_steady(const uint64_t tick_time) {

  double cur_rate = double(recv_seqno - prev_tick_seqno) / double(tick_time - prev_tick_time);

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
    << "rate mean: " << rate_mean << ", rate std: " << sqrt(rate_var) << endl
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

  if (state == 0)
  {
    /* slow start */
    cwnd++;
    if (timestamp_ack_received - send_timestamp_acked > 100)
      state = 1; /* switch to maintenance. */
  }

  // if (timestamp_ack_received - send_timestamp_acked < 100) {
  //   rate_mean += 0.001;
  //   cerr << "increased rate_mean" << endl;
  // }

  // if (timestamp_ack_received - send_timestamp_acked > 125) {
  //   rate_mean -= 0.001;
  //   cerr << "decreased rate_mean" << endl;
  // }

  if (recv_timestamp_acked > prev_tick_time + TICK_SIZE
      || sequence_number_acked == 0) /* timer tick, do update */
  {

    if (state == 0) /* probe the network capacity */
      do_ewma_probe(recv_timestamp_acked); 
    else /* maintain steady state */
      do_ewma_steady(recv_timestamp_acked); 

    prev_tick_time = recv_timestamp_acked;
    prev_tick_seqno = recv_seqno;
  }

  // if (cwnd == MIN_WINDOW_SIZE && state == 1 && timestamp_ack_received - send_timestamp_acked < 150)
  //   /* try starting up again. */
  //   state = 0;

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
