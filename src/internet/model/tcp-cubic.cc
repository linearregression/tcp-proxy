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

#include "tcp-cubic.h"
#include "ns3/log.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "ns3/node.h"

NS_LOG_COMPONENT_DEFINE ("TcpCubic");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TcpCubic)
  ;

TypeId
TcpCubic::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpCubic")
    .SetParent<TcpSocketBase> ()
    .AddConstructor<TcpCubic> ()
    .AddAttribute ("ReTxThreshold", "Threshold for fast retransmit",
                    UintegerValue (3),
                    MakeUintegerAccessor (&TcpCubic::m_retxThresh),
                    MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("LimitedTransmit", "Enable limited transmit",
                    BooleanValue (false),
                    MakeBooleanAccessor (&TcpCubic::m_limitedTx),
                    MakeBooleanChecker ())
    .AddAttribute ("TcpFriendliness",
                   "Set whether TCP-friendly region will be considered or not",
                    BooleanValue (true),
                    MakeBooleanAccessor (&TcpCubic::m_tcpFrndlyness),
                    MakeBooleanChecker ())
    .AddAttribute ("FastConvergence",
                   "Enable Fast Convergence when sharing bandwidth",
                    BooleanValue (true),
                    MakeBooleanAccessor (&TcpCubic::m_fastConv),
                    MakeBooleanChecker ())
    .AddAttribute ("Beta",
                   "Packet loss event multiplicative factor",
                    DoubleValue (0.2),
                    MakeDoubleAccessor (&TcpCubic::m_beta),
                    MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("C",
                   "CUBIC constant parameter",
                    DoubleValue (0.4),
                    MakeDoubleAccessor (&TcpCubic::m_c),
                    MakeDoubleChecker<double> (0.0))
    .AddTraceSource ("CongestionWindow",
                     "The TCP connection's congestion window",
                     MakeTraceSourceAccessor (&TcpCubic::m_cWnd))
  ;
  return tid;
}

TcpCubic::TcpCubic (void)
  : m_retxThresh (3), // mute valgrind, actual value set by the attribute system
    m_inFastRec (false),
    m_limitedTx (false), // mute valgrind, actual value set by the attribute system
    m_tcpFrndlyness (true),
    m_fastConv (true),
    m_beta (0.2),
    m_c (0.4)
{
  NS_LOG_FUNCTION (this);
  CubicReset ();
}

TcpCubic::TcpCubic (const TcpCubic& sock)
  : TcpSocketBase (sock),
    m_cWnd (sock.m_cWnd),
    m_ssThresh (sock.m_ssThresh),
    m_initialCWnd (sock.m_initialCWnd),
    m_retxThresh (sock.m_retxThresh),
    m_tcpFrndlyness (sock.m_tcpFrndlyness),
    m_fastConv (sock.m_fastConv),
    m_beta (sock.m_beta),
    m_c (sock.m_c)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Invoked the copy constructor");
  CubicReset ();
}

TcpCubic::~TcpCubic (void)
{
}

/** We initialize m_cWnd from this function, after attributes initialized */
int
TcpCubic::Listen (void)
{
  NS_LOG_FUNCTION (this);
  InitializeCwnd ();
  return TcpSocketBase::Listen ();
}

/** We initialize m_cWnd from this function, after attributes initialized */
int
TcpCubic::Connect (const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  InitializeCwnd ();
  return TcpSocketBase::Connect (address);
}

/** Limit the size of in-flight data by cwnd and receiver's rxwin */
uint32_t
TcpCubic::Window (void)
{
  NS_LOG_FUNCTION (this);
  return std::min (m_rWnd.Get (), m_cWnd.Get ());
}

Ptr<TcpSocketBase>
TcpCubic::Fork (void)
{
  return CopyObject<TcpCubic> (this);
}

/** New ACK (up to seqnum seq) received. Increase cwnd and call TcpSocketBase::NewAck() */
void
TcpCubic::NewAck (const SequenceNumber32& seq)
{
  NS_LOG_FUNCTION (this << seq);
  NS_LOG_LOGIC ("TcpCubic receieved ACK for seq " << seq <<
                " cwnd " << m_cWnd <<
                " ssthresh " << m_ssThresh);
  
  // Check for exit condition of fast recovery
  if (m_inFastRec)
    { // RFC2001, sec.4; RFC2581, sec.3.2
      // First new ACK after fast recovery: reset cwnd
	  m_epochStart = Time();
	  uint32_t cWnd = m_cWnd.Get () / m_segmentSize;
	  if (Simulator::Now () > m_wLastTime + 0.1 * m_k)
		{
		  if (cWnd < m_wLastMax && m_fastConv)
			{
			  m_wLastMax = cWnd * (2.0 - m_beta) / 2.0;
			}
		  else
			{
			  m_wLastMax = cWnd;
			}
		}
	  m_wLastTime = Simulator::Now ();
	  m_cWnd = m_cWnd.Get () * (1.0 - m_beta);
      m_ssThresh = m_cWnd.Get ();
      //m_cWnd = m_ssThresh;
      m_inFastRec = false;
      NS_LOG_INFO ("Reset cwnd to " << m_cWnd);
    }

  if (m_dMin.IsZero())
	{
	  m_dMin = m_lastRtt.Get();
	}
  else
	{
	  m_dMin = std::min(m_dMin, m_lastRtt.Get());
	}
  
  // Increase of cwnd based on current phase (slow start or congestion avoidance)
  if (m_cWnd <= m_ssThresh)
    { // Slow start mode, add one segSize to cWnd. Default m_ssThresh is 65535. (RFC2001, sec.1)
      m_cWnd += m_segmentSize;
      NS_LOG_INFO ("In SlowStart, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
    }
  else
	{ // Congestion avoidance mode
	  if (CubicUpdate () < m_cWndCnt)
		{
		  m_cWnd += m_segmentSize;
		  m_cWndCnt = 0u;
		  NS_LOG_INFO ("In CongAvoid, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
		}
	  else
		{
		  m_cWndCnt++;
		}
	}

  // Complete newAck processing
  TcpSocketBase::NewAck (seq);
}

/** Cut cwnd and enter fast recovery mode upon triple dupack */
void
TcpCubic::DupAck (const TcpHeader& t, uint32_t count)
{
  NS_LOG_FUNCTION (this << "t " << count);
  if (count == m_retxThresh && !m_inFastRec)
    { // triple duplicate ack triggers fast retransmit (RFC2581, sec.3.2)
      m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
      m_cWnd = m_ssThresh + 3 * m_segmentSize;
      m_inFastRec = true;
      NS_LOG_INFO ("Triple dupack. Reset cwnd to " << m_cWnd << ", ssthresh to " << m_ssThresh);
      DoRetransmit ();
    }
  else if (m_inFastRec)
    { // In fast recovery, inc cwnd for every additional dupack (RFC2581, sec.3.2)
      m_cWnd += m_segmentSize;
      NS_LOG_INFO ("Increased cwnd to " << m_cWnd);
      SendPendingData (m_connected);
    };
}

/** Retransmit timeout */
void
TcpCubic::Retransmit (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
  m_inFastRec = false;

  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT) return;
  // If all data are received (non-closing socket and nothing to send), just return
  if (m_state <= ESTABLISHED && m_txBuffer.HeadSequence () >= m_highTxMark) return;

  CubicReset ();

  // According to RFC2581 sec.3.1, upon RTO, ssthresh is set to half of flight
  // size and cwnd is set to 1*MSS, then the lost packet is retransmitted and
  // TCP back to slow start	  m_epochStart = Time();
	  uint32_t cWnd = m_cWnd.Get () / m_segmentSize;
	  if (Simulator::Now () > m_wLastTime + 2.0 * m_lastRtt.Get ())
		{
		  if (cWnd < m_wLastMax && m_fastConv)
			{
			  m_wLastMax = cWnd * (2.0 - m_beta) / 2.0;
			}
		  else
			{
			  m_wLastMax = cWnd;
			}
		}
  m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
  m_cWnd = m_segmentSize;
  m_nextTxSequence = m_txBuffer.HeadSequence (); // Restart from highest Ack
  NS_LOG_INFO ("RTO. Reset cwnd to " << m_cWnd <<
               ", ssthresh to " << m_ssThresh << ", restart from seqnum " << m_nextTxSequence);
  m_rtt->IncreaseMultiplier ();             // Double the next RTO
  DoRetransmit ();                          // Retransmit the packet
}

void
TcpCubic::SetSegSize (uint32_t size)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpCubic::SetSegSize() cannot change segment size after connection started.");
  m_segmentSize = size;
}

void
TcpCubic::SetSSThresh (uint32_t threshold)
{
  m_ssThresh = threshold;
}

uint32_t
TcpCubic::GetSSThresh (void) const
{
  return m_ssThresh;
}

void
TcpCubic::SetInitialCwnd (uint32_t cwnd)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpCubic::SetInitialCwnd() cannot change initial cwnd after connection started.");
  m_initialCWnd = cwnd;
}

uint32_t
TcpCubic::GetInitialCwnd (void) const
{
  return m_initialCWnd;
}

void 
TcpCubic::InitializeCwnd (void)
{
  /*
   * Initialize congestion window, default to 1 MSS (RFC2001, sec.1) and must
   * not be larger than 2 MSS (RFC2581, sec.3.1). Both m_initiaCWnd and
   * m_segmentSize are set by the attribute system in ns3::TcpSocket.
   */
  m_cWnd = m_initialCWnd * m_segmentSize;
}

double 
TcpCubic::CubicUpdate (void)
{
  /*
   * Qwerty
   */
  NS_LOG_FUNCTION (this);
  
  uint32_t cWnd = m_cWnd / m_segmentSize;
  
  m_ackCnt++;
  
  if (m_epochStart <= 0)
	{
	  m_epochStart = Simulator::Now ();
	  
	  NS_LOG_INFO ("Starting new epoch");
	  if (cWnd < m_wLastMax)
		{
		  m_k = pow((m_wLastMax - cWnd) / m_c, (1.0 / 3.0));
		  m_originPoint = m_wLastMax;
		}
	  else
		{
		  m_k = 0.0;
		  m_originPoint = cWnd;
		}
	  m_ackCnt = 1;
	  m_wTcp = cWnd;
	}
	
  double t = (Simulator::Now () + m_dMin - m_epochStart).GetSeconds ();
  double target = m_originPoint + m_c * pow(t - m_k, 3.0);
  
  double cnt;
	
  if (target > cWnd)
	{
	  cnt = cWnd / (target - cWnd);
	}
  else
	{
	  cnt = 100u * cWnd;
	}
  
  return m_tcpFrndlyness ? CubicTcpFriendliness (cnt) : cnt;
}

double 
TcpCubic::CubicTcpFriendliness (double cnt)
{
  /*
   * Qwerty
   */
  NS_LOG_FUNCTION (this << cnt);
  
  uint32_t cWnd = m_cWnd / m_segmentSize;
  m_wTcp += 3 * m_beta / (2 - m_beta) * m_ackCnt / cWnd;
  
  m_ackCnt = 0u;
  
  if (m_wTcp > cWnd)
	{
	  double max_cnt = cWnd / (m_wTcp - cWnd);
	  if (cnt > max_cnt)
		{
		  cnt = max_cnt;
		}
	}
  
  return cnt;
}

void 
TcpCubic::CubicReset (void)
{
  /*
   * Asdf
   */
  NS_LOG_FUNCTION (this);
  m_wLastMax = 0u;
  m_epochStart = Time();
  m_originPoint = 0u;
  m_dMin = Time();
  m_wTcp = 0u;
  m_k = 0.0;
  m_ackCnt = 0u;
  m_wLastTime = Time();
}

} // namespace ns3
