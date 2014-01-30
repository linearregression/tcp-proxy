#include "ns3/vegas-node.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("VegasNode");

namespace ns3 {

VegasNode::VegasNode (void)
 : m_bytes (0)
{
  NS_LOG_FUNCTION (this);
}

void
VegasNode::SetSeqNumber (SequenceNumber32 seq) {
  m_seqNumber = seq;
}

SequenceNumber32
VegasNode::GetSeqNumber (void) {
  return m_seqNumber;
}

void
VegasNode::SetSentTime (Time stime) {
  m_sentTime = stime;
}

Time
VegasNode::GetSentTime (void){
  return m_sentTime;
}

void
VegasNode::AddBytes (uint32_t bytes) {
  m_bytes += bytes;
  NS_LOG_LOGIC (bytes << " bytes added to node " << m_seqNumber << ", total bytes: " << m_bytes);
}

uint32_t
VegasNode::GetBytes (void){
  return m_bytes;
}

} // namespace ns3