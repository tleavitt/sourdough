/* UDP sender for congestion-control contest */

#include <cstdlib>
#include <iostream>
#include <stdlib.h> 
#include <unistd.h>
#include <sys/types.h>
#include <string>
#include <thread>

#include "socket.hh"
#include "contest_message.hh"
#include "controller.hh"
#include "poller.hh"
#include "timestamp.hh"

using namespace std;
using namespace PollerShortNames;

#define PACKET_SIZE_BITS (1500 * 8)

/* simple sender class to handle the accounting */
class DatagrumpSender
{
private:
  UDPSocket socket_;
  Controller controller_; /* your class */

  useconds_t bg_sender_period_; /* number of microseconds to wait between
                                  background sender injecting a packet.*/
  uint64_t send_time;
  uint64_t toggle_time;
  bool should_send_bg_traffic_;  /* flag indicating if we should send cross traffic. */

  uint64_t sequence_number_; /* next outgoing sequence number */

  /* if network does not reorder or lose datagrams,
     this is the sequence number that the sender
     next expects will be acknowledged by the receiver */
  uint64_t next_ack_expected_;

  void send_datagram( const bool after_timeout );
  void inject_bg_packet();
  void got_ack( const uint64_t timestamp, const ContestMessage & msg );
  bool window_is_open();
  static void toggle_bg_traffig();

public:
  DatagrumpSender( const char * const host,
          const char * const port, useconds_t bg_sender_period,
          const bool debug, const bool use_ctcp);
  int loop();
};

int main( int argc, char *argv[] )
{
   /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  bool use_ctcp = true;
  bool debug = false;
  int bg_rate = 10; /* Mbps */

  if (argc >= 6 and argv[5][0] == 't') {
    cerr << "using tcp instead of ctcp" << endl;
    use_ctcp = false;
  }
  if ( argc >= 5 and argv[4][0] == 'd') {
    cerr << "setting debug" << endl;
    debug = true;
  } else if ( argc >= 4) {
    cerr << "setting bgrate" << endl;
    bg_rate = atoi( argv[3] );
  } else if ( argc >= 3 ) {
    /* do nothing */
  } else {
    cerr << "Usage: " << argv[ 0 ] << " HOST PORT [bgrate] [debug]" << endl;
    return EXIT_FAILURE;
  }
  useconds_t bg_sender_period;
  if (bg_rate > 0)
    bg_sender_period = (PACKET_SIZE_BITS) / bg_rate;
  else bg_sender_period = 0;

  /* create sender object to handle the accounting */
  /* all the interesting work is done by the Controller */
  cerr << "Startind sender with bg_rate: " << bg_rate << ", debug: " << debug 
       << ", use_ctcp: " << use_ctcp << endl;
  DatagrumpSender sender( argv[ 1 ], argv[ 2 ], bg_sender_period, debug, use_ctcp);
  return sender.loop();
}

DatagrumpSender::DatagrumpSender( const char * const host,
				  const char * const port, useconds_t bg_sender_period,
				  const bool debug, const bool use_ctcp)
  : socket_(),
    controller_( debug, use_ctcp),
    bg_sender_period_ ( bg_sender_period ),
    send_time (0),    
    toggle_time (0),
    should_send_bg_traffic_ (false),
    sequence_number_( 0 ),
    next_ack_expected_( 0 )
{
  /* turn on timestamps when socket receives a datagram */
  socket_.set_timestamps();

  /* connect socket to the remote host */
  /* (note: this doesn't send anything; it just tags the socket
     locally with the remote address */
  socket_.connect( Address( host, port ) );  

  cerr << "background send period: " << bg_sender_period_ << " us" << endl;
  cerr << "Sending to " << socket_.peer_address().to_string() << endl;
}

void DatagrumpSender::got_ack( const uint64_t timestamp,
			       const ContestMessage & ack )
{
  if ( not ack.is_ack() ) {
    throw runtime_error( "sender got something other than an ack from the receiver" );
  }

  /* Update sender's counter */
  next_ack_expected_ = max( next_ack_expected_,
			    ack.header.ack_sequence_number + 1 );

  /* Inform congestion controller */
  controller_.ack_received( ack.header.ack_sequence_number,
			    ack.header.ack_send_timestamp,
			    ack.header.ack_recv_timestamp,
			    timestamp );
}

void DatagrumpSender::send_datagram( const bool after_timeout )
{

  string dummy_payload = string( 1424, 'c' ); /* ctcp packet */

  ContestMessage cm( sequence_number_++, dummy_payload );
  cm.set_send_timestamp();
  socket_.send( cm.to_string() );

  controller_.datagram_was_sent( cm.header.sequence_number,
				 cm.header.send_timestamp,
				 after_timeout );
}

void DatagrumpSender::inject_bg_packet() 
{
  string dummy_payload = string( 1424, 'b' ); /* background packet */
  ContestMessage cm( 0, dummy_payload ); /* null sequence number */
  cm.set_send_timestamp();
  string cm_string =  cm.to_string();
  // cerr << "packet string length: " << cm_string.length() << endl; 
  socket_.send( cm_string );
}

bool DatagrumpSender::window_is_open()
{
  return sequence_number_ - next_ack_expected_ < controller_.window_size();
}

int DatagrumpSender::loop()
{
  /* read and write from the receiver using an event-driven "poller" */
  Poller poller;

  /* first rule: if the window is open, close it by
     sending more datagrams */
  poller.add_action(
    Action( socket_, Direction::Out, [&] () {
  	    /* Send if possible */
        if ( window_is_open() ) {
  	     send_datagram( false );
  	    }

  	    return ResultType::Continue;
      }, [&] () { return window_is_open(); } 
    ) 
  );

  /* second rule: if sender receives an ack,
     process it and inform the controller
     (by using the sender's got_ack method) */
  poller.add_action( 
    Action( socket_, Direction::In, [&] () {
      	const UDPSocket::received_datagram recd = socket_.recv();
      	const ContestMessage ack  = recd.payload;
      	got_ack( recd.timestamp, ack );
      	return ResultType::Continue;
      } )
  );

  /* third rule: inject cross-traffic at a constant rate. This busy waits, oh well. */
  if (bg_sender_period_ > 0)
    poller.add_action(
      Action( socket_, Direction::Out, [&] () {
          uint64_t cur_time = timestamp_us();
          if (cur_time > send_time && should_send_bg_traffic_) {
            inject_bg_packet();
            send_time = cur_time + bg_sender_period_;
          }
          if (cur_time > toggle_time) {
            should_send_bg_traffic_ = !should_send_bg_traffic_;
            toggle_time = cur_time + 10 * 1000 * 1000; /* ten seconds */
            cerr << "background traffic is: " << (should_send_bg_traffic_ ? "on" : "off") << endl;
          }
          return ResultType::Continue;
        } ) 
    );

  /* Run these four rules forever */
  while ( true ) {
    const auto ret = poller.poll( controller_.timeout_ms() );
    if ( ret.result == PollResult::Exit ) {
      return ret.exit_status;
    } else if ( ret.result == PollResult::Timeout ) {
      /* After a timeout, send one datagram to try to get things moving again */
      send_datagram( true );
    }
  }
}
