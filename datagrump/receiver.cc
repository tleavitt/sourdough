/* simple UDP receiver that acknowledges every datagram */

#include <cstdlib>
#include <iostream>

#include "socket.hh"
#include "contest_message.hh"

using namespace std;

#define PACKET_SIZE_BITS (1500 * 8)

double bps_to_mpbps(double bps) {
  return (bps / 1024) / 1024;
}

class ThroughputTracker
{
private:
  bool verbose_;
  /* hyperpatameters. */
  double alpha;
  uint64_t min_time_delta;

  /* tracker variables. */
  uint64_t bits_in_interval;

  uint64_t last_timestamp;
  uint64_t cur_timestamp;

public:

  double ewma_throughput_bps;
  void init( uint64_t timestamp, bool verbose, double alpha_, uint64_t min_time_delta_);
  double update(uint64_t bits_received, uint64_t timestamp);
  double get_throughput();
};

void ThroughputTracker::init( uint64_t timestamp, bool verbose = false,
                              double alpha_ = 0.5, uint64_t min_time_delta_ = 100)
{
  last_timestamp = timestamp;
  cur_timestamp = timestamp;
  bits_in_interval = 0;
  ewma_throughput_bps = 0;
  verbose_ = verbose;
  alpha = alpha_;
  min_time_delta = min_time_delta_;
}

double ThroughputTracker::update(uint64_t bits_received, uint64_t timestamp)
{
  cur_timestamp = timestamp;
  bits_in_interval += bits_received;
  if (cur_timestamp > last_timestamp + min_time_delta) {
    /* Update ewma. */
    double cur_throughput_bps = double(bits_in_interval) / ((cur_timestamp - last_timestamp) / 1000.0 );
    if (ewma_throughput_bps == 0) /* initialize estimate */
      ewma_throughput_bps = cur_throughput_bps;
    else
      ewma_throughput_bps = alpha * cur_throughput_bps + (1 - alpha) * ewma_throughput_bps;

    if (verbose_)
      cerr << "timestamp: " << timestamp << ", average throughput: " << bps_to_mpbps(ewma_throughput_bps) << " Mpbs" << endl;
    /* Reset tracker variables. */
    bits_in_interval = 0;
    last_timestamp = cur_timestamp;
  }

  return ewma_throughput_bps;
}


double ThroughputTracker::get_throughput()
{
  return ewma_throughput_bps;
}

void prepare_and_send_ack(ContestMessage & message, UDPSocket & socket,
        const UDPSocket::received_datagram & recd,
        uint64_t & sequence_number) 
{
    /* else,  assemble the acknowledgment */
  message.transform_into_ack( sequence_number++, recd.timestamp );

  /* timestamp the ack just before sending */
  message.set_send_timestamp();

  /* send the ack */
  socket.sendto( recd.source_address, message.to_string() );
}

int main( int argc, char *argv[] )
{
   /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[ 0 ] << " PORT" << endl;
    return EXIT_FAILURE;
  }

  /* create UDP socket for incoming datagrams */
  UDPSocket socket;

  /* turn on timestamps on receipt */
  socket.set_timestamps();

  /* "bind" the socket to the user-specified local port number */
  socket.bind( Address( "::0", argv[ 1 ] ) );

  cerr << "Listening on " << socket.local_address().to_string() << endl;

  uint64_t sequence_number = 0;

  ThroughputTracker tracker;

  /* Loop and acknowledge every incoming datagram back to its source */
  while ( true ) {

    const UDPSocket::received_datagram recd = socket.recv();
    ContestMessage message = recd.payload;

    if (message.payload[0] == 'c') {
      /* we got one of our packets, continue. */
      tracker.init(recd.timestamp, true);
      prepare_and_send_ack(message, socket, recd, sequence_number);
      break; 
    }
  }

  while ( true ) {
    const UDPSocket::received_datagram recd = socket.recv();
    ContestMessage message = recd.payload;

    if (message.payload[0] == 'b')
      continue; /* this is a background packet, ignore it.*/

    /* Advance timesteps. */
    tracker.update(PACKET_SIZE_BITS, recd.timestamp);
    prepare_and_send_ack(message, socket, recd, sequence_number);


  }

  return EXIT_SUCCESS;
}
