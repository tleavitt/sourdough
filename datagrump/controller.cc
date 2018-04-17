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

  /* calculate the forecast adaptively */
  int forecast_ms = int(base_forecast_ms - sqrt(rate_var) * forecast_scaling);

  double bdp = cur_rate * rtt_mean;

  /* after 100 ms, the number of bytes drained from the queue will be 100 * R.
     If we set cwnd to be exactly this value, a packet that enters the queue now
     will exit the queue 100 ms later. */

  /* Adaptively adjust the forecast */
  // cwnd = (unsigned int)max(cautious_rate * forecast_ms, MIN_WINDOW_SIZE);
  cwnd = (unsigned int)max(cautious_rate * forecast_ms, MIN_WINDOW_SIZE);

  /* Bound cwnd to be less than the */
  if (double(cwnd) > bdp_mult * bdp) {
    if (debug_)
      cerr << "bounding cwnd: " << cwnd << " to " << bdp_mult * bdp << endl;
    cwnd = (unsigned int)max(bdp_mult * bdp, MIN_WINDOW_SIZE);
  }

  if (debug_) {
    cerr << "cur rate: " << cur_rate << endl
    << "rate mean: " << rate_mean << ", rate std: " << sqrt(rate_var) << endl
    << "cautious_rate: " << cautious_rate << ", forecast: " << forecast_ms << endl
    << "cwnd: " << cwnd << endl;
  }
} 

void Controller::do_rtt_update(const uint64_t timestamp_ack_received, 
                               const uint64_t send_timestamp_acked) {

  double cur_rtt = double(timestamp_ack_received - send_timestamp_acked);
  if (rtt_mean == 0)
    rtt_mean = cur_rtt;
  else
    rtt_mean = rtt_smooth * cur_rtt + (1 - rtt_smooth) * rtt_mean;
  if (debug_)
    cerr << "rtt: " << rtt_mean << endl; 
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
  do_rtt_update(timestamp_ack_received, send_timestamp_acked);

  if (state == 0)
  {
    /* slow start */
    cwnd++;
    if (rtt_mean > 125)
      state = 1; /* switch to maintenance. */
  }

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

  // if (state == 1 && cwnd == MIN_WINDOW_SIZE && rtt_mean < 100)
  //   state = 0; /* back to slow start */

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
