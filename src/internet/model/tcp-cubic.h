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

#ifndef TCP_CUBIC_H
#define TCP_CUBIC_H

#include "tcp-socket-base.h"

namespace ns3 {

/**
 * \ingroup socket
 * \ingroup tcp
 *
 * \brief An implementation of a stream socket using TCP.
 *
 * This class contains the CUBIC implementation of TCP.
 */
class TcpCubic : public TcpSocketBase
{
public:
  static TypeId GetTypeId (void);
  /**
   * Create an unbound tcp socket.
   */
  TcpCubic (void);
  TcpCubic (const TcpCubic& sock);
  virtual ~TcpCubic (void);

  // From TcpSocketBase
  virtual int Connect (const Address &address);
  virtual int Listen (void);

protected:
  virtual uint32_t Window (void); // Return the max possible number of unacked bytes
  virtual Ptr<TcpSocketBase> Fork (void); // Call CopyObject<TcpCubic> to clone me
  virtual void NewAck (SequenceNumber32 const& seq); // Inc cwnd and call NewAck() of parent
  virtual void DupAck (const TcpHeader& t, uint32_t count);  // Halving cwnd and reset nextTxSequence
  virtual void Retransmit (void); // Exit fast recovery upon retransmit timeout

  // Implementing ns3::TcpSocket -- Attribute get/set
  virtual void     SetSegSize (uint32_t size);
  virtual void     SetSSThresh (uint32_t threshold);
  virtual uint32_t GetSSThresh (void) const;
  virtual void     SetInitialCwnd (uint32_t cwnd);
  virtual uint32_t GetInitialCwnd (void) const;
private:
  void InitializeCwnd (void);               // set m_cWnd when connection starts
  double CubicUpdate (void);                // update CUBIC parameters
  double CubicTcpFriendliness (double cnt); // update CUBIC parameters
  void CubicReset (void);                   // reset CUBIC parameters

protected:
  TracedValue<uint32_t>  m_cWnd;         //!< Congestion window
  uint32_t               m_ssThresh;     //!< Slow Start Threshold
  uint32_t               m_initialCWnd;  //!< Initial cWnd value
  SequenceNumber32       m_recover;      //!< Previous highest Tx seqnum for fast recovery
  uint32_t               m_retxThresh;   //!< Fast Retransmit threshold
  bool                   m_inFastRec;    //!< currently in fast recovery
  bool                   m_limitedTx;    //!< perform limited transmit
  bool                   m_tcpFrndlyness;//!< TCP friendliness
  bool                   m_fastConv;     //!< Fast convergence to lower window size
  double                 m_beta;         //!< BIC-TCP reduction factor
  double                 m_c;            //!< CUBIC constant parameter
  uint32_t               m_wLastMax;     //!< cWnd value during last packet loss
  Time                   m_wLastTime;    //!< Time since last packet loss
  Time                   m_epochStart;   //!< Time since first ACK was received
  uint32_t               m_originPoint;
  Time                   m_dMin;         //!< Minimum RTT during this connection
  uint32_t               m_wTcp;         //!< TCP window size in terms of elapsed time
  double                 m_k;            //!< Time to reach w_last_max value again
  uint32_t               m_ackCnt;
  uint32_t               m_cWndCnt;
};

} // namespace ns3

#endif /* TCP_NEWRENO_H */
