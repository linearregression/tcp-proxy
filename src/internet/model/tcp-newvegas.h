/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Author: Camila Faundez
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
  void BaseRTTChange (int64_t o , int64_t n);

protected:
  TracedValue<uint32_t>  m_cWnd;         //!< Congestion window
  uint32_t               m_ssThresh;     //!< Slow Start Threshold
  uint32_t               m_initialCWnd;  //!< Initial cWnd value
  bool                   m_inFastRec;    //!< currently in fast recovery

  TracedValue<int64_t>   m_baseRTT;      //!< Base RTT
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
