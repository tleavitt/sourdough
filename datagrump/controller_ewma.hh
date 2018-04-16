#ifndef CONTROLLER_HH
#define CONTROLLER_HH

#include <cstdint>

/* Congestion controller interface */

class Controller
{
private:
  bool debug_; /* Enables debugging output */

  /* Add member variables here */
  double rate_mean; /* ewma link rate .*/
  double rate_var; /* ewma of rate variance. */

  double mean_smooth = 0.01; /* smoothing parameter for rate mean */
  double var_smooth = 0.01; /* smoothing parameter for rate var */

  double confidence_mult = 1.0; /* use mean - confidence_mult * var as the rate estimate. */

  unsigned int cwnd = 15; /* initial estimate for cwnd. */ 
  int forecast_ms = 500; /* number of steps ahead to forecast. */

  uint64_t prev_tick_time = 0; /* last recv time that we did an update. */
  uint64_t prev_tick_seqno = 0; /* receiver seqno at last tick. */

  uint64_t send_seqno = 0; /* last sent seqno. */
  uint64_t recv_seqno = 0; /* last received seqno. */

public:
  /* Public interface for the congestion controller */
  /* You can change these if you prefer, but will need to change
     the call site as well (in sender.cc) */

  /* Default constructor */
  Controller( const bool debug );

  /* Get current window size, in datagrams */
  unsigned int window_size();

  /* A datagram was sent */
  void datagram_was_sent( const uint64_t sequence_number,
			  const uint64_t send_timestamp,
			  const bool after_timeout );

  /* An ack was received */
  void ack_received( const uint64_t sequence_number_acked,
		     const uint64_t send_timestamp_acked,
		     const uint64_t recv_timestamp_acked,
		     const uint64_t timestamp_ack_received );

  /* How long to wait (in milliseconds) if there are no acks
     before sending one more datagram */
  unsigned int timeout_ms();

  void do_ewma_update(const uint64_t tick_time);

};

#endif
