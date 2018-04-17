#ifndef CONTROLLER_HH
#define CONTROLLER_HH

#include <cstdint>

/* Congestion controller interface */

class Controller
{
private:
  bool debug_; /* Enables debugging output */
  int state = 0; /* 0: slow start, 1: steady state */

  /* Add member variables here */
  double rate_mean = 0; /* ewma of link rate. */
  double rate_var = 0; /* ewma of rate variance. */
  double rtt_mean = 0; /* ewma of round trip time. */

  double rtt_smooth = 0.05;
  double bdp_mult = 2;

  double mean_smooth = 0.3; /* smoothing parameter for rate mean */
  double var_smooth = 0.5; /* smoothing parameter for rate var */

  double confidence_mult = 0.75; /* use mean - confidence_mult * var as the rate estimate. */

  unsigned int cwnd = 1; /* congestion window. */ 
  int base_forecast_ms = 100; /* forecast time. */
  int forecast_scaling = 0; /* controls how much we adapt the forecast to network conditions. */

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

  void do_rtt_update(const uint64_t timestamp_ack_received, 
                               const uint64_t send_timestamp_acked);
  void do_ewma_probe(const uint64_t tick_time);
  void do_ewma_steady(const uint64_t tick_time);

};

#endif
