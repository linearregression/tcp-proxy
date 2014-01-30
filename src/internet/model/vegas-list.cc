#include "vegas-list.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE ("VegasList");

namespace ns3 {

void
VegasList::Add(SequenceNumber32 seq) {
  NS_LOG_LOGIC ("Seq " << seq);
  //std::cout << "Add " << seq << std::endl;
  VegasNode node;
  node.SetSeqNumber(seq);
  node.SetSentTime(Simulator::Now());

  m_data.push_back (node);
}

void
VegasList::Discard(SequenceNumber32 seq) {
  NS_LOG_LOGIC ("Seq " << seq);

  std::list<VegasNode>::iterator i = m_data.begin ();

  while (i != m_data.end() )
    if ( (*i).GetSeqNumber() == seq)
      i = m_data.erase (i);
    else
      i++;
}

void
VegasList::DiscardUpTo(SequenceNumber32 seq) {
  NS_LOG_LOGIC ("Seq " << seq);

  std::list<VegasNode>::iterator i = m_data.begin ();

  while (i != m_data.end() )
    if ( (*i).GetSeqNumber() <= seq)
      i = m_data.erase (i);
    else
      i++;
}

VegasNode
VegasList::GetFirstNode(SequenceNumber32 seq) {
  NS_LOG_FUNCTION (this);
  std::list<VegasNode>::iterator i = m_data.begin ();

  while (i != m_data.end() )
    if ( (*i).GetSeqNumber() == seq)
      break;
    else
      i++;
  return (*i);
}

VegasNode
VegasList::GetLastNode(SequenceNumber32 seq) {
  NS_LOG_FUNCTION (this);
  std::list<VegasNode>::iterator i = m_data.begin ();
  VegasNode node;

  while (i != m_data.end() ) {
    if ( (*i).GetSeqNumber() == seq)
      node = (*i);
    i++;
  }
  return node;
}

Time
VegasList::GetSentTime (SequenceNumber32 seq){
  NS_LOG_FUNCTION (this);
  std::list<VegasNode>::iterator i = m_data.begin ();

  while (i != m_data.end() )
    if ( (*i).GetSeqNumber() == seq)
      return (*i).GetSentTime();
    else
      i++;
  return Seconds(0);
}

void
VegasList::AddBytes (uint32_t bytes) {
  NS_LOG_LOGIC (bytes << "bytes added");
  std::list<VegasNode>::iterator i = m_data.begin ();

  while (i != m_data.end() ) {
    (*i).AddBytes(bytes);
    i++;
  }
}

} // namespace ns3