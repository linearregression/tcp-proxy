#include "ns3/nstime.h"
#include "ns3/sequence-number.h"

#ifndef VEGAS_NODE_H
#define VEGAS_NODE_H

namespace ns3 {

class VegasNode
{
public:
  VegasNode (void);

  void SetSeqNumber (SequenceNumber32 seq);
  SequenceNumber32 GetSeqNumber (void);

  void SetSentTime (Time time);
  Time GetSentTime (void);

  void AddBytes (uint32_t bytes);
  uint32_t GetBytes (void);

private:
  SequenceNumber32 m_seqNumber;
  Time m_sentTime;

  uint32_t m_bytes;

  Time m_sumRTT;
  uint32_t m_nRTT;
};

} // namespace ns3

#endif