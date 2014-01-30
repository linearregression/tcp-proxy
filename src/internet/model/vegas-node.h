#include "ns3/nstime.h"
#include "ns3/sequence-number.h"

#ifndef VEGAS_NODE_H
#define VEGAS_NODE_H

namespace ns3 {

class VegasNode
{
public:
  VegasNode (void); // Empty node
  VegasNode (SequenceNumber32 seq); // Creates an info node

  void SetSeqNumber (SequenceNumber32 seq); // Set sequence number
  SequenceNumber32 GetSeqNumber (void); // Get sequence number

  void SetSentTime (Time time); // Set sent time
  Time GetSentTime (void); // Get sent time

  void AddBytes (uint32_t bytes); // Add bytes to the partial sum of sent bytes
  uint32_t GetBytes (void); // Get total bytes sent

private:
  SequenceNumber32 	m_seqNumber; // Sequence number of packet
  Time 			m_sentTime; // Time when the packet was sent
  uint32_t 		m_bytes; // Bytes sent in packet's RTT
};

} // namespace ns3

#endif