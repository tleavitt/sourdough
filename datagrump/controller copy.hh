#ifndef CONTROLLER_HH
#define CONTROLLER_HH

#include <cstdint>
#include <math.h>



/* Congestion controller interface */

class Controller
{
private:
  bool debug_; /* Enables debugging output */

  /* Add member variables here */
  bool slow_start = true;
  int cwnd = 1;
  int dwnd = 0;

  double cwnd_ = 1;
  double dwnd_ = 0;

  /* CTCP params: */
  double alpha = 10;
  double beta = 0.1; /* not used, currently. */
  int k = 10;
  int gamma = 30;
  double zeta = 0.01;

  /* RTT params */
  double rtt = 0;
  double base_rtt = INFINITY; 

  double rtt_smooth = 0.05; /* ewma smoothing factor. */

  uint64_t send_seqno = 0; /* last sent seqno. */
  uint64_t recv_seqno = 0; /* last received seqno. */

  double SLOWSTART_TIMEOUT = 125;

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

  void update_rtt(const uint64_t timestamp_ack_received, 
                               const uint64_t send_timestamp_acked);

  void update_dwnd(double win, double diff);

  /* An ack was received */
  void ack_received( const uint64_t sequence_number_acked,
		     const uint64_t send_timestamp_acked,
		     const uint64_t recv_timestamp_acked,
		     const uint64_t timestamp_ack_received );

  /* How long to wait (in milliseconds) if there are no acks
     before sending one more datagram */
  unsigned int timeout_ms();
};

#endif
