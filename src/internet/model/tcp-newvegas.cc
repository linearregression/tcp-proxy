/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Adrian Sai-wah Tam
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Adrian Sai-wah Tam <adrian.sw.tam@gmail.com>
 */

#define NS_LOG_APPEND_CONTEXT \
  if (m_node) { std::clog << Simulator::Now ().GetSeconds () << " [node " << m_node->GetId () << "] "; }

#include "tcp-newvegas.h"
#include "ns3/log.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "ns3/node.h"

NS_LOG_COMPONENT_DEFINE ("TcpNewVegas");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TcpNewVegas)
  ;

TypeId
TcpNewVegas::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpNewVegas")
    .SetParent<TcpSocketBase> ()
    .AddConstructor<TcpNewVegas> ()
    .AddTraceSource ("CongestionWindow",
                     "The TCP connection's congestion window",
                     MakeTraceSourceAccessor (&TcpNewVegas::m_cWnd))
  ;
  return tid;
}

TcpNewVegas::TcpNewVegas (void)
: m_initialCWnd (2),
  m_inFastRec (false),
  m_baseRTT (9999999999),
  m_alpha (2), 
  m_beta (4), 
  m_gamma (1), 
  m_slowStart(true), 
  m_slowStartBool (true),
  m_checkRetransmit(0)

{
  NS_LOG_FUNCTION (this);
}

TcpNewVegas::TcpNewVegas (const TcpNewVegas& sock)
  : TcpSocketBase (sock),
    m_cWnd (sock.m_cWnd),
    m_ssThresh (sock.m_ssThresh),
    m_initialCWnd (sock.m_initialCWnd),
    m_inFastRec (false)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Invoked the copy constructor");
}

TcpNewVegas::~TcpNewVegas (void)
{
}

/* We initialize m_cWnd from this function, after attributes initialized */
int
TcpNewVegas::Listen (void)
{
  NS_LOG_FUNCTION (this);
  InitializeCwnd ();
  return TcpSocketBase::Listen ();
}

/* We initialize m_cWnd from this function, after attributes initialized */
int
TcpNewVegas::Connect (const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  InitializeCwnd ();
  return TcpSocketBase::Connect (address);
}

/* Limit the size of in-flight data by cwnd and receiver's rxwin */
uint32_t
TcpNewVegas::Window (void)
{
  NS_LOG_FUNCTION (this);
  return std::min (m_rWnd.Get (), m_cWnd.Get ());
}

Ptr<TcpSocketBase>
TcpNewVegas::Fork (void)
{
  return CopyObject<TcpNewVegas> (this);
}

uint32_t
TcpNewVegas::SendDataPacket (SequenceNumber32 seq, uint32_t maxSize, bool withAck) {

  uint32_t size = TcpSocketBase::SendDataPacket(seq, maxSize, withAck);

  NS_LOG_LOGIC ("Seq: " << seq + size);

  m_info.Discard (seq + size);
  m_info.Add(seq + size); // Create new node and add it to the list
  m_info.AddBytes(size); // Update sent bytes
  return size;
}

void
TcpNewVegas::EstimateDiff (SequenceNumber32 const& seq)
{
  VegasNode node = m_info.GetFirstNode(seq);

  Time rtt = Simulator::Now() - node.GetSentTime();
  int64_t lastRTT = rtt.GetInteger ();

  //m_baseRTT = ( lastRTT < m_baseRTT ) ? lastRTT : m_baseRTT;

  uint32_t bytes = node.GetBytes();

  if (bytes <= m_segmentSize) {
    m_baseRTT =  lastRTT;
    NS_LOG_INFO ("Reset BaseRTT to: " << m_baseRTT);
  }

  else if (lastRTT < m_baseRTT ) {
    m_baseRTT = lastRTT;
    NS_LOG_INFO ("Updated BaseRTT to: " << m_baseRTT);
  }

  // m_diff = static_cast<double> (m_cWnd.Get()) / m_baseRTT - static_cast<double> (bytes) / lastRTT;
  m_diff = static_cast<double> (m_cWnd.Get()) / m_baseRTT - static_cast<double> (m_cWnd.Get()) / lastRTT;

  m_diff *= m_baseRTT;
  m_diff /= m_segmentSize;

  NS_LOG_INFO ("BaseRTT: " << m_baseRTT << " LastRTT: " << lastRTT << " Cwnd: " << m_cWnd << " Bytes: " << bytes << " Diff: " << m_diff);
}

void
TcpNewVegas::SlowStart(void)
{
  m_cWnd = (m_slowStartBool) ? m_cWnd*2 : m_cWnd;
  m_slowStartBool = !m_slowStartBool;
  NS_LOG_INFO ("In SlowStart, updated to cwnd " << m_cWnd);
}

void
TcpNewVegas::CongestionAvoidance(void)
{
  if (m_diff < m_alpha)
    {
      m_cWnd+= m_segmentSize;
      NS_LOG_INFO ("In CongestionAvoidance, increased cwnd to" << m_cWnd << ", segment size " << m_segmentSize);
    }

  else if (m_beta < m_diff && m_cWnd > 2*m_segmentSize)
    {
      m_cWnd-= m_segmentSize;
      NS_LOG_INFO ("In CongestionAvoidance, decreased cwnd to" << m_cWnd << ", segment size " << m_segmentSize);
    }
  else
    NS_LOG_INFO ("In CongestionAvoidance, mantained cwnd " << m_cWnd);
}

/* New ACK (up to seqnum seq) received. Increase cwnd and call TcpSocketBase::NewAck() */
void
TcpNewVegas::NewAck (const SequenceNumber32& seq)
{
  NS_LOG_FUNCTION (this << seq);
  NS_LOG_LOGIC ("TcpNewVegas receieved ACK for seq " << seq <<
                " cwnd " << m_cWnd );

  EstimateDiff (seq);

  // Check if difference between expected and actual rate is greater than gamma (1 router buffer)
  if (m_diff > m_gamma)
      m_slowStart = false; // Exit slow start

  if (m_slowStart)
    SlowStart();
  else
    CongestionAvoidance();

  // Complete newAck processing
  TcpSocketBase::NewAck (seq);

  // Check for exit condition of fast recovery
  if (m_inFastRec)
    { // First new ACK after fast recovery: update cwnd to 3/4*cwnd
      m_cWnd = m_cWnd * 3 / 4; 
      m_inFastRec = false;
      NS_LOG_INFO ("Reset cwnd to " << m_cWnd);
    }

  // Check for retransmit if first/second ACK after DupAck
  if (m_checkRetransmit){
    NS_LOG_LOGIC ("Check for retransmit (" << m_checkRetransmit << ")");
    VegasNode node = m_info.GetLastNode(seq);
    Time rtt = Simulator::Now() - node.GetSentTime();

    if (m_rto.Get() < rtt) //If RTT>RTO, retransmits
    {
      NS_LOG_LOGIC ("Retransmit");
      m_nextTxSequence = m_txBuffer.HeadSequence ();
      DoRetransmit ();
      m_checkRetransmit = 2;
    }
    else
      m_checkRetransmit--;
  }
}

// Fast recovery and fast retransmit
void
TcpNewVegas::DupAck (const TcpHeader& t, uint32_t count)
{
  NS_LOG_FUNCTION (this << "t " << count);
  if (!m_inFastRec)
    { // Check to fast retransmit
      VegasNode node = m_info.GetFirstNode(t.GetAckNumber());
      Time rtt = Simulator::Now() - node.GetSentTime();
      if (m_rto.Get() < rtt)
      {
        m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
        m_cWnd = m_ssThresh + 3 * m_segmentSize;
        m_inFastRec = true;
        m_checkRetransmit = 2; // Flag to check the next 2 ACKs
        NS_LOG_INFO ("Retransmit. Reset cwnd to " << m_cWnd << ", ssthresh to " << m_ssThresh);
        DoRetransmit ();
      }
    }
  else if (m_inFastRec)
    { // In fast recovery, inc cwnd for every additional dupack (RFC2581, sec.3.2)
      m_cWnd += m_segmentSize;
      NS_LOG_INFO ("In fast recovery, increased cwnd to " << m_cWnd);
      SendPendingData (m_connected);
    };
}

// Retransmit timeout
void TcpNewVegas::Retransmit (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
  m_inFastRec = false;

  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT) return;
  // If all data are received (non-closing socket and nothing to send), just return
  if (m_state <= ESTABLISHED && m_txBuffer.HeadSequence () >= m_highTxMark) return;

  // According to "TCP Vegas Revisited":
  m_cWnd = 2 * m_segmentSize; // CWnd is set to 2*MSS
  m_slowStart = true; // TCP back to slow start

  m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);

  m_nextTxSequence = m_txBuffer.HeadSequence (); // Restart from highest Ack
  NS_LOG_INFO ("RTO. Reset cwnd to " << m_cWnd <<
               ", ssthresh to " << m_ssThresh << ", restart from seqnum " << m_nextTxSequence);
  m_rtt->IncreaseMultiplier ();             // Double the next RTO
  DoRetransmit ();                          // Retransmit the packet
}

void
TcpNewVegas::SetSegSize (uint32_t size)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpNewVegas::SetSegSize() cannot change segment size after connection started.");
  m_segmentSize = size;
}

void
TcpNewVegas::SetSSThresh (uint32_t threshold)
{
  m_ssThresh = threshold;
}

uint32_t
TcpNewVegas::GetSSThresh (void) const
{
  return m_ssThresh;
}

void
TcpNewVegas::SetInitialCwnd (uint32_t cwnd)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpNewVegas::SetInitialCwnd() cannot change initial cwnd after connection started.");
  m_initialCWnd = cwnd;
}

uint32_t
TcpNewVegas::GetInitialCwnd (void) const
{
  return m_initialCWnd;
}

void 
TcpNewVegas::InitializeCwnd (void)
{
  /*
   * Description:
   */
  m_cWnd = m_initialCWnd * m_segmentSize;
}

} // namespace ns3
