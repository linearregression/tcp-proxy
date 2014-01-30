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

#ifndef TCP_NEWVEGAS_H
#define TCP_NEWVEGAS_H

#include "tcp-socket-base.h"
#include "vegas-list.h"

namespace ns3 {

/**
* Description:
*/
class TcpNewVegas : public TcpSocketBase
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * Create an unbound tcp socket.
   */
  TcpNewVegas (void);

  /**
   * \brief Copy constructor
   * \param sock the object to copy
   */
  TcpNewVegas (const TcpNewVegas& sock);

  virtual ~TcpNewVegas (void);

  // From TcpSocketBase
  virtual int Connect (const Address &address); // Initializes m_cWnd
  virtual int Listen (void); // Initializes m_cWnd

protected:
  virtual uint32_t SendDataPacket (SequenceNumber32 seq, uint32_t maxSize, bool withAck);

  virtual uint32_t Window (void); // Return the max possible number of unacked bytes
  virtual Ptr<TcpSocketBase> Fork (void); // Call CopyObject<TcpNewVegas> to clone me
  virtual void NewAck (const SequenceNumber32& seq); // Inc cwnd and call NewAck() of parent
  virtual void DupAck (const TcpHeader& t, uint32_t count);  // Fast retransmit
  virtual void Retransmit (void); // Retransmit timeout

  // Implementing ns3::TcpSocket -- Attribute get/set
  virtual void     SetSegSize (uint32_t size);
  virtual void     SetSSThresh (uint32_t threshold);
  virtual uint32_t GetSSThresh (void) const;
  virtual void     SetInitialCwnd (uint32_t cwnd);
  virtual uint32_t GetInitialCwnd (void) const;
private:
  /**
   * \brief Set the congestion window when connection starts
   */
  void InitializeCwnd (void);
  void SlowStart (void);
  void CongestionAvoidance (void);
  void EstimateDiff (const SequenceNumber32& seq);

protected:
  TracedValue<uint32_t>  m_cWnd;         //!< Congestion window
  uint32_t               m_ssThresh;     //!< Slow Start Threshold
  uint32_t               m_initialCWnd;  //!< Initial cWnd value
  bool                   m_inFastRec;    //!< currently in fast recovery

  int64_t                m_baseRTT;      //< Base RTT
  uint32_t               m_alpha;
  uint32_t               m_beta;  
  uint32_t               m_gamma;
  double                 m_diff;         //< Expected - Actual sending rate.
  bool                   m_slowStart;    //< True if slowstart. False if congestion avidance.
  bool                   m_slowStartBool;

  VegasList              m_info;
  uint32_t               m_checkRetransmit;

};

} // namespace ns3

#endif /* TCP_NEWVEGAS_H */