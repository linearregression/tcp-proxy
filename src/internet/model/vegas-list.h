#include <list>
#include "vegas-node.h"

#ifndef VEGAS_LIST_H
#define VEGAS_LIST_H

namespace ns3 {

class VegasList
{
public:
  void Add (SequenceNumber32 seq);
  void Discard (SequenceNumber32 seq);
  void DiscardUpTo (SequenceNumber32 seq);
  VegasNode GetFirstNode(SequenceNumber32 seq);
  VegasNode GetLastNode(SequenceNumber32 seq);
  Time GetSentTime (SequenceNumber32 seq);
  void AddBytes (uint32_t bytes);

private:
  std::list<VegasNode> m_data;
};

} // namespace ns3

#endif