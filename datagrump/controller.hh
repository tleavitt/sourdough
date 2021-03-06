#ifndef CONTROLLER_HH
#define CONTROLLER_HH

#include <cstdint>
#include <math.h>

class Controller
{
private:
  bool debug_; /* Enables debugging output */
  bool use_ctcp_; /* use CTCP flag. */

  /* Add member variables here */
  bool slow_start = true;
  int cwnd = 1;
  int dwnd = 0;

  double cwnd_ = 1;
  double dwnd_ = 0;

  /* CTCP params: */
  double alpha = 1.0;
  double beta = 0.3; /* not used, currently. */
  float k = 0.1;
  int gamma = 30;
  double zeta = 0.02;

  /* RTT params */
  double rtt = 0;
  double base_rtt = INFINITY; 

  double rtt_smooth = 0.05; /* ewma smoothing factor. */

  uint64_t next_ack_expected_ = 1; /* next ack we're expecting to see. */

  double SLOWSTART_TIMEOUT = 125;
  uint64_t LOSS_TIMEOUT = 80;
  uint64_t loss_timestamp = 0;

public:
  /* Public interface for the congestion controller */
  /* You can change these if you prefer, but will need to change
     the call site as well (in sender.cc) */

  /* Default constructor */
  Controller( const bool debug, const bool use_ctcp );

  /* Get current window size, in datagrams */
  unsigned int window_size();

  void enter_slow_start();

  /* A datagram was sent */
  void datagram_was_sent( const uint64_t sequence_number,
			  const uint64_t send_timestamp,
			  const bool after_timeout );

  void update_rtt(const uint64_t timestamp_ack_received, 
                               const uint64_t send_timestamp_acked);

  void update_dwnd(double win, double diff, bool loss);

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
